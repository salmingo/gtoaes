/*
 * @file ObservationSystemGWAC.cpp 定义GWAC观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#include "ObservationSystemGWAC.h"
#include "GLog.h"
#include "ADefine.h"

using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;

ObsSysGWACPtr make_obss_gwac(const string& gid, const string& uid) {
	return boost::make_shared<ObservationSystemGWAC>(gid, uid);
}

ObservationSystemGWAC::ObservationSystemGWAC(const string& gid, const string& uid)
	: ObservationSystem(gid, uid) {
	mntproto_ = make_mount(gid, uid);
}

ObservationSystemGWAC::~ObservationSystemGWAC() {
}

void ObservationSystemGWAC::NotifyReady(mpready proto) {

}

void ObservationSystemGWAC::NotifyState(mpstate proto) {

}

void ObservationSystemGWAC::NotifyUtc(mputc proto) {
	static int count(999);
	nftele_->utc = proto->utc;
	if (++count == 1000) {// 检查望远镜时钟是否正确. 每1000周期检查一次
		count = 0;
		ptime now = microsec_clock::universal_time();
		try {
			/*
			 * 计算转台控制机时钟漂移
			 * - ss > 0: 时钟过慢
			 * - ss < 0: 时钟过快
			 */
			ptime utc = from_iso_extended_string(proto->utc);
			ptime::time_duration_type::tick_type ss = (now - utc).total_milliseconds();
			if (ss > 5000 || ss < -5000) {
				_gLog.Write(LOG_WARN, NULL, "telescope [%s:%s] time drifts %.3f seconds",
						gid_.c_str(), uid_.c_str(), ss * 0.001);
			}
		}
		catch(...) {
			_gLog.Write(LOG_WARN, NULL, "wrong time [%s] from telescope [%s:%s]", proto->utc.c_str(),
					gid_.c_str(), uid_.c_str());
		}
	}
}

void ObservationSystemGWAC::NotifyPosition(mpposition proto) {
	double ora(nftele_->ra), odc(nftele_->dc), oaz(nftele_->az), oel(nftele_->el);
	double ra(proto->ra), dc(proto->dc), azi, ele;
	bool safe = safe_position(ra, dc, azi, ele);
	TELESCOPE_STATE state = nftele_->state;
	bool slewing = state == TELESCOPE_SLEWING;
	bool parking = state == TELESCOPE_PARKING;

	// 更新转台指向坐标
	nftele_->ra = ra, nftele_->dc = dc;
	nftele_->az = azi * D2R, nftele_->el = ele * D2R;

	if (!safe) {// 超出限位
		if (state != TELESCOPE_PARKING) {
			_gLog.Write(LOG_WARN, NULL, "telescope [%s:%s] position [%.4f, %.4f] is out of safe limit",
					gid_.c_str(), uid_.c_str(), ra, dc);
			nftele_->state = TELESCOPE_PARKING;
			PostMessage(MSG_OUT_SAFELIMIT);
		}
	}
	else if(slewing || parking) {// 是否由动至静
		double e1 = slewing ? fabs(ora - ra) : fabs(oaz - azi);
		double e2 = slewing ? fabs(odc - dc) : fabs(oel - ele);
		double t(0.003); // 到位阈值: 0.003度==10.8角秒

		if (e1 > 180.0) e1 = 360.0 - e1;
		if (e1 < t && e2 < t) {// 指向到位, 但此时不确定指向是否正确
			if (nftele_->StableArrive()) {
				_gLog.Write("telescope [%s:%s] arrived at [%.4f, %.4f]",
						gid_.c_str(), uid_.c_str(), ra, dc);
				nftele_->state = slewing ? TELESCOPE_TRACKING : TELESCOPE_PARKED;
				if (nftele_->state == TELESCOPE_TRACKING) PostMessage(MSG_TELESCOPE_TRACKING);
			}
		}
		else nftele_->UnstableArrive();
	}
}

void ObservationSystemGWAC::NotifyCooler(apcooler proto) {
	tmLast_ = second_clock::universal_time();
}

bool ObservationSystemGWAC::safe_position(double ra, double dec, double &azi, double &ele) {
	mutex_lock lck(mtx_ats_);
	ptime now = microsec_clock::universal_time();

	ats_->SetMJD(now.date().modjulian_day() + now.time_of_day().total_milliseconds() * 0.001 / DAYSEC);
	ats_->Eq2Horizon(ats_->LocalMeanSiderealTime() - ra * D2R, dec * D2R, azi, ele);
	return (ele > minEle_);
}

int ObservationSystemGWAC::relative_priority(int x) {
	int r1(x), r2(x);
	if (plan_now_.use_count())  r1 = x - plan_now_->plan->priority;
	if (plan_wait_.use_count()) r2 = x - plan_wait_->plan->priority;
	return (r1 < r2 ? r1 + 1 : r2 + 1);
}

/*
 * bool change_planstate() 改变观测计划工作状态
 * - GWAC系统中断计划后, 状态变更为结束, 即不再执行该计划
 */
bool ObservationSystemGWAC::change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS old_state, OBSPLAN_STATUS new_state) {
	if (new_state == OBSPLAN_INT) {
		plan->state = old_state == OBSPLAN_WAIT ? OBSPLAN_ABANDON : OBSPLAN_OVER;
		return true;
	}

	return false;
}

/*
 * void resolve_obsplan() 解析并投递GWAC观测计划给相机
 * - GWAC系统中, 所有相机使用相同参数
 */
void ObservationSystemGWAC::resolve_obsplan() {
	apappplan plan = plan_now_->plan;
	apobject proto = boost::make_shared<ascii_proto_object>(*plan);
	const char *s;
	int n;

	// 构建ascii_proto_object成员变量
	proto->expdur = plan->expdur[0];
	proto->frmcnt = plan->frmcnt[0];
	if (plan->delay.size()) proto->delay = plan->delay[0];

	// 构建并发送格式化字符串
	s = ascproto_->CompactObject(proto, n);
	write_to_camera(s, n);
}

bool ObservationSystemGWAC::target_arrived() {
	return false;
}

void ObservationSystemGWAC::process_guide(apguide proto) {
	/* 导星条件1: 观测计划类型为mon == monitor */
	if (!(plan_now_.use_count() && iequals(plan_now_->plan->obstype, "mon")))
		return;
	/* 导星条件2: 最后一次导星距离现在现在时间已超过2分钟 */
	ptime now = second_clock::universal_time();
	if ((now - lastguide_).total_seconds() < 120)
		return;

	double tguide(0.08);// 导星阈值. 0.08度=4.8角分==288角秒
	double tmax(2.7);	// 最大导星量. 2.7度=9720角秒
	double dra, ddc;	// 导星量
	/*  */
	if (valid_ra(proto->objra) && valid_dec(proto->objdc)) {

	}
	else {
		dra = proto->ra;
		ddc = proto->dc;
	}

//	int n;
//	const char *s = mntproto_->CompactGuide(proto->ra, proto->dc, n);
//	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemGWAC::process_fwhm(apfwhm proto) {
	// 由数据库完成
	return true;
}

bool ObservationSystemGWAC::process_focus(apfocus proto) {
	// 由数据库完成
	return true;
}

bool ObservationSystemGWAC::process_abortslew() {
	if (!ObservationSystem::process_abortslew()) return false;
	// 向下位机发送控制信息
	int n;
	const char *s = mntproto_->CompactAbortslew(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemGWAC::process_park() {
	if (!ObservationSystem::process_park()) return false;
	int n;
	const char *s = mntproto_->CompactPark(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemGWAC::process_slewto(double ra, double dec, double epoch) {
	if (!tcpc_telescope_.use_count()) return false;
	int n;
	const char *s = mntproto_->CompactSlew(ra, dec, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemGWAC::process_mcover(apmcover proto) {
	// 由数据库实现
	return true;
}

bool ObservationSystemGWAC::process_findhome() {
	if (!ObservationSystem::process_findhome()) return false;
	int n;
	const char *s = mntproto_->CompactFindhome(true, true, n);
	return tcpc_telescope_->Write(s, n);
}

/*
 * bool process_homesync(): 改变转台零点位置
 * - 下位机不响应该协议
 */
bool ObservationSystemGWAC::process_homesync(aphomesync proto) {
	if (!ObservationSystem::process_homesync(proto)) return true;
	int n;
	const char* s = mntproto_->CompactHomesync(proto->ra, proto->dc, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemGWAC::process_focusync() {
	// 由数据库实现
	return true;
}

void ObservationSystemGWAC::process_abortplan(int plan_sn) {
	if (plan_wait_.use_count() && (plan_sn == -1 || (*plan_wait_) == plan_sn)) {
		ObservationSystem::change_planstate(plan_wait_, OBSPLAN_DELETE);
		cb_plan_finished_(plan_wait_);
		plan_wait_.reset();
	}
}
