/*
 * @file ObservationSystemNormal.cpp 定义GWAC观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#include "ObservationSystemNormal.h"
#include "GLog.h"
#include "ADefine.h"
#include "parameter.h"

using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;

ObsSysNormalPtr make_obss_normal(const string& gid, const string& uid) {
	return boost::make_shared<ObservationSystemNormal>(gid, uid);
}

ObservationSystemNormal::ObservationSystemNormal(const string& gid, const string& uid)
	: ObservationSystem(gid, uid) {
	tslew_    = AS2D * 30;	// 30角秒
	tguide_   = 1.39E-4;	// 0.5角秒
	ostype_   = OBSST_NORMAL;
}

ObservationSystemNormal::~ObservationSystemNormal() {
}

bool ObservationSystemNormal::CoupleTelescope(TcpCPtr ptr) {
	if (!ObservationSystem::CoupleTelescope(ptr)) return false;
	// 重定向读出函数
	const TCPClient::CBSlot& slot = boost::bind(&ObservationSystemNormal::receive_telescope, this, _1, _2);
	ptr->RegisterRead(slot);

	return true;
}

int ObservationSystemNormal::relative_priority(int x) {
	int r1(x), r2(x);
	if (plan_now_.use_count())  r1 = x - plan_now_->plan->priority;
	if (plan_wait_.use_count()) r2 = x - plan_wait_->plan->priority;
	return (r1 < r2 ? r1 : r2);
}

/*
 * bool change_planstate() 改变观测计划工作状态
 * - GWAC系统中断计划后, 状态变更为结束, 即不再执行该计划
 */
bool ObservationSystemNormal::change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS old_state, OBSPLAN_STATUS new_state) {
	if (new_state == OBSPLAN_INT && old_state == OBSPLAN_WAIT) {
		plan->state = OBSPLAN_CAT;
		return true;
	}

	return false;
}

/*
 * bool resolve_obsplan() 解析曝光参数并发送给相机
 * - 全新执行计划: 解析观测计划, 提取曝光参数发送给相机
 * - 中断后再次执行计划: 将中断断点记录的曝光参数发送给相机
 */
void ObservationSystemNormal::resolve_obsplan() {
	int n;
	const char *s;

	if (plan_now_->ptBreak.size()) {// 从断点中读取曝光参数
		ObsPlanBreakPtVec &param = plan_now_->ptBreak;
		ObsPlanBreakPtVec::iterator it;
		ObssCamPtr camptr;

		for (it = param.begin(); it != param.end(); ++it) {
			camptr = find_camera((*it)->cid);
			s = ascproto_->CompactObject(*it, n);
			camptr->tcptr->Write(s, n);
		}
	}
	else {// 从观测计划中读取曝光参数
		mutex_lock lck(mtx_camera_);

		apappplan plan = plan_now_->plan;
		vector<string> &filter = plan->filter;
		int nfilter(filter.size()), ifilter(0);
		ObssCamVec::iterator it;

		for (it = cameras_.begin(); it != cameras_.end(); ++it) {
			if (!(*it)->enabled) continue;
			apobject object = boost::make_shared<ascii_proto_object>(*plan); // 为各个相机分别生成参数
			object->cid    = (*it)->cid;
			if (nfilter) object->filter = filter[ifilter];
			if (nfilter && ++ifilter >= nfilter) ifilter = 0;

			// 构建并发送格式化字符串
			s = ascproto_->CompactObject(object, n);
			(*it)->tcptr->Write(s, n);
			// 存储断点
			plan_now_->ptBreak.push_back(object);
		}
	}
}

void ObservationSystemNormal::receive_telescope(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_TELESCOPE : MSG_RECEIVE_TELESCOPE);
}

bool ObservationSystemNormal::process_guide(double &dra, double &ddec) {
	int n;
	const char *s = ascproto_->CompactGuide(dra, ddec, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_fwhm(apfwhm proto) {
	if (!ObservationSystem::process_fwhm(proto) || !tcpc_telescope_.use_count()) return false;
	int n;
	const char *s = ascproto_->CompactFWHM(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_focus(apfocus proto) {
	if (!ObservationSystem::process_focus(proto) || !tcpc_telescope_.use_count()) return false;
	int n;
	const char *s = ascproto_->CompactFocus(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_abortslew() {
	if (!ObservationSystem::process_abortslew()) return false;
	int n;
	const char *s = ascproto_->CompactAbortSlew(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_park() {
	if (!ObservationSystem::process_park()) return false;
	int n;
	const char *s = ascproto_->CompactPark(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_slewto(double ra, double dec, double epoch) {
	if (!tcpc_telescope_.use_count()) return false;
	int n;
	const char *s = ascproto_->CompactSlewto(ra, dec, epoch, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_mcover(apmcover proto) {
	if (!ObservationSystem::process_mcover(proto) || !tcpc_telescope_.use_count()) return false;

	mutex_lock lck(mtx_camera_);
	int n, cmd = proto->value;
	string cid = proto->cid;
	bool empty = cid.empty();
	const char *s;
	for (ObssCamVec::iterator it = cameras_.begin(); it != cameras_.end(); ++it) {
		if (empty || iequals(cid, (*it)->cid)) {
			if ((*it)->enabled) {
				if (empty) proto->cid = (*it)->cid;
				s = ascproto_->CompactMirrorCover(proto, n);
				tcpc_telescope_->Write(s, n);
			}
			if (!empty) break;
		}
	}
	return true;
}

bool ObservationSystemNormal::process_findhome() {
	if (!ObservationSystem::process_findhome()) return false;
	int n;
	const char *s = ascproto_->CompactFindHome(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_homesync(aphomesync proto) {
	if (!ObservationSystem::process_homesync(proto)) return false;
	int n;
	const char *s = ascproto_->CompactHomeSync(proto, n);
	return tcpc_telescope_->Write(s, n);
}
