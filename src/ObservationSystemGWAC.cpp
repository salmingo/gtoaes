/*
 * @file ObservationSystemGWAC.cpp 定义GWAC观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#include "ObservationSystemGWAC.h"

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

}

void ObservationSystemGWAC::NotifyPosition(mpposition proto) {

}

void ObservationSystemGWAC::NotifyCooler(apcooler proto) {
	tmLast_ = second_clock::universal_time();
}

bool ObservationSystemGWAC::NotifyPlan(ObsPlanPtr plan) {
	if (!ObservationSystem::NotifyPlan(plan)) return false;

	return true;
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
bool ObservationSystemGWAC::change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS state) {
	if (!ObservationSystem::change_planstate(plan, state)) return false;
	if (plan->state == OBSPLAN_INT) plan->state = OBSPLAN_OVER;

	return true;
}

/*
 * bool resolve_obsplan() 解析并投递GWAC观测计划给相机
 * - GWAC系统中, 所有相机使用相同参数
 */
bool ObservationSystemGWAC::resolve_obsplan() {
	apappplan plan = plan_now_->plan;
	apobject proto = boost::make_shared<ascii_proto_object>();
	const char *s;
	int n;

	// 构建ascii_proto_object成员变量
	*proto = *plan;
	proto->expdur = plan->expdur[0];
	proto->frmcnt = plan->frmcnt[0];
	if (plan->delay.size()) proto->delay = plan->delay[0];

	// 构建并发送格式化字符串
	s = ascproto_->CompactObject(proto, n);
	write_to_camera(s, n);

	return true;
}

void ObservationSystemGWAC::on_new_plan(const long, const long) {

}

bool ObservationSystemGWAC::process_guide(apguide proto) {
	if (!ObservationSystem::process_guide(proto)) return false;
	int n;
	const char *s = mntproto_->CompactGuide(proto->ra, proto->dc, n);
	return tcpc_telescope_->Write(s, n);
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
		change_planstate(plan_wait_, OBSPLAN_DELETE);
		cb_planstate_changed_(plan_wait_);
		plan_wait_.reset();
	}
}
