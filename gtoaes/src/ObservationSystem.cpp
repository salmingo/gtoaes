/**
 * @file ObservationSystem.cpp
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#include <boost/date_time/posix_time/posix_time.hpp>
#include "ADefine.h"
#include "ObservationSystem.h"
#include "GLog.h"

using namespace AstroUtil;
using namespace boost::posix_time;

ObservationSystem::ObservationSystem(const string& gid, const string& uid)
		: param_(NULL) {
	gid_ = gid;
	uid_ = uid;
	robotic_  = true;
	mode_     = OBSS_ERROR;
	altLimit_ = 0.0;
}

ObservationSystem::~ObservationSystem() {

}

void ObservationSystem::SetParameter(const OBSSParam* param) {
	_gLog.Write("OBSS[%s:%s] locates at: %.4f, %.4f, altitude: %.1f, timezone: %d. AltLimit = %.1f",
			gid_.c_str(), uid_.c_str(),
			param->siteLon, param->siteLat, param->siteAlt, param->timeZone,
			param->altLimit);
	ats_.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timeZone);
	altLimit_ = param->altLimit * D2R;
	param_ = param;
}

void ObservationSystem::SetDBPtr(DBCurlPtr ptr) {
	dbPtr_ = ptr;
}

void ObservationSystem::RegisterAcquirePlan(const CBSlot& slot) {
	if (!acqPlan_.empty()) acqPlan_.disconnect_all_slots();
	acqPlan_.connect(slot);
}

void ObservationSystem::GetIDs(string& gid, string& uid) {
	gid = gid_;
	uid = uid_;
}

bool ObservationSystem::IsSafePoint(ObsPlanItemPtr plan, const ptime& now) {
	bool safe(true);
	if ((plan->coorsys == TypeCoorSys::COORSYS_EQUA
			|| plan->coorsys == TypeCoorSys::COORSYS_ALTAZ)) {// 位置
		double lon = plan->lon * D2R;
		double lat = plan->lat * D2R;
		if (plan->coorsys == TypeCoorSys::COORSYS_ALTAZ) safe = lat >= altLimit_;
		else {
			ptime::date_type today = now.date();
			double lmst, azi, alt;

			ats_.SetUTC(today.year(), today.month(), today.day(),
					now.time_of_day().total_seconds() / DAYSEC);
			lmst = ats_.LocalMeanSiderealTime();
			ats_.Eq2Horizon(lmst - lon, lat, azi, alt);
			safe = lat >= altLimit_;
		}
	}
	return safe;
}

bool ObservationSystem::Start() {
	//...

	_gLog.Write("OBSS[%s:%s] starts running", gid_.c_str(), uid_.c_str());
	return true;
}

void ObservationSystem::Stop() {
	//...
//	interrupt_thread(thrd_acqPlan_);
	_gLog.Write("OBSS[%s:%s] stopped", gid_.c_str(), uid_.c_str());
}

int ObservationSystem::ActiveCount() {
	return 0;
}

int ObservationSystem::IsMatched(const string& gid, const string& uid) {
	if (gid_ == gid && uid_ == uid) return 1;
	if (gid.empty() || (gid_ == gid && uid.empty())) return 2;
	return 0;
}

int ObservationSystem::GetPriority() {
	int prio(0);

	if (mode_ == OBSS_ERROR) prio = INT_MAX;
	else if (plan_wait_.use_count()) // 按照策略, 等待区计划优先级不低于当前计划优先级
		prio = plan_wait_->priority;
	else if (plan_now_.use_count()) {// 当前计划的优先级
		int T  = plan_now_->period;
		int dt = (second_clock::universal_time() - plan_now_->tmbegin).total_seconds();
		if (dt >= int(T * 0.7)) prio = 4 * plan_now_->priority;
		else prio = plan_now_->priority * T / (T - dt);
	}
	return prio;
}

void ObservationSystem::CoupleClient(const TcpCPtr client) {

}

int ObservationSystem::CoupleMount(const TcpCPtr client) {
	if (!tcpc_mount_.client.use_count()) {
		_gLog.Write("Mount[%s:%s] was on-line", gid_.c_str(), uid_.c_str());
		tcpc_mount_.client = client;

		if (!param_->p2hMount) {// P2P模式, 由OBSS接管网络信息接收/解析

		}
	}
	else if (tcpc_mount_() != client) {
		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] had related mount",
				gid_.c_str(), uid_.c_str());
		return 0;
	}
	return param_->p2hMount ? 2 : 1;
}

int ObservationSystem::CoupleCamera(const TcpCPtr client, const string& cid) {
	return param_->p2hCamera ? 2 : 1;
}

int ObservationSystem::CoupleMountAnnex(const TcpCPtr client) {
	return param_->p2hMountAnnex ? 2 : 1;
}

int ObservationSystem::CoupleCameraAnnex(const TcpCPtr client, const string& cid) {
	return param_->p2hCameraAnnex ? 2 : 1;
}

void ObservationSystem::DecoupleClient(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMount(const TcpCPtr client) {

}

void ObservationSystem::DecoupleCamera(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::NotifyKVProtocol(kvbase proto) {
	MtxLck lck(mtx_queKv_);
	queKv_.push_back(proto);
}

void ObservationSystem::NotifyNonkvProtocol(nonkvbase proto) {

}

void ObservationSystem::NotifyPlan(ObsPlanItemPtr plan) {
	if (plan_wait_.use_count()) {// 等待区计划退回计划队列
		plan_wait_->state = StateObservationPlan::OBSPLAN_CATALOGED;
		plan_wait_.reset();
	}
	if (plan_now_.use_count()) {// 中止当前计划
		plan->state = StateObservationPlan::OBSPLAN_WAITING;
		plan_wait_ = plan;
		abort_plan();
	}
	else {// 立即执行计划
		plan->state = StateObservationPlan::OBSPLAN_RUNNING;
		plan_now_ = plan;
		process_plan();
	}
}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::process_kv_client(kvbase proto) {

}

//////////////////////////////////////////////////////////////////////////////
/* 观测计划 */
void ObservationSystem::process_plan() {
	if (plan_now_->gid.empty()) plan_now_->gid   = gid_;
	if (plan_now_->uid.empty()) plan_now_->uid   = uid_;
	plan_now_->state = StateObservationPlan::OBSPLAN_RUNNING;
	//...
}

void ObservationSystem::abort_plan() {
	//...
}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::thread_acquire_plan() {
	boost::chrono::minutes period(2);
	boost::mutex mtx;
	MtxLck lck(mtx);

	while(1) {
		cv_acqPlan_.wait_for(lck, period);

		if (!(plan_now_.use_count() || plan_wait_.use_count())) {
			plan_now_ = *acqPlan_(shared_from_this());
			if (plan_now_.use_count()) {
				//...开始执行计划
			}
		}
	}
}
