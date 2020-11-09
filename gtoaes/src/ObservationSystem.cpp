/**
 * @file ObservationSystem.cpp
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#include "ObservationSystem.h"

ObservationSystem::ObservationSystem(const string& gid, const string& uid) {
	gid_ = gid;
	uid_ = uid;
}

ObservationSystem::~ObservationSystem() {

}

bool ObservationSystem::Start() {
	return false;
}

void ObservationSystem::Stop() {

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
	int prio(0), prio(0);

	if (plan_wait_.use_count())
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

void ObservationSystem::CoupleMount(const TcpCPtr client, int type) {

}

void ObservationSystem::CoupleCamera(const TcpCPtr client, const string& cid) {

}

void ObservationSystem::CoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::CoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleClient(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMount(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::NotifyKVProtocol(kvbase proto) {
	MtxLck lck(mtx_queKv_);
	queKv_.push_back(proto);
	cv_queKv_.notify_one();
}

void ObservationSystem::NotifyPlan(ObsPlanItemPtr plan) {
	// 有效性检查: 高度角...

	if (plan_wait_.use_count()) {// 等候区计划退回计划队列
		plan_wait_->state = StateObservationPlan::OBSPLAN_CATALOGED;
		plan_wait_.reset();
	}
	if (plan_now_.use_count()) {// 中止当前计划; 计划中断后执行等候区
		plan->state = StateObservationPlan::OBSPLAN_WAITING;
		plan_wait_ = plan;
		// 中断当前计划
	}
	else {// 立即执行计划
		if (plan->gid.empty()) plan->gid   = gid_;
		if (plan->uid.empty()) plan->uid   = uid_;
		plan->state = StateObservationPlan::OBSPLAN_RUNNING;
		plan_now_ = plan;
	}
}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::process_kv_client(kvbase proto) {

}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::interrupt_thread(ThreadPtr& thrd) {
	if (thrd.unique()) {
		thrd->interrupt();
		thrd->join();
		thrd.reset();
	}
}

void ObservationSystem::monitor_kv_queue() {
	boost::mutex mtx;
	MtxLck lck(mtx);
	kvbase proto;

	while (1) {
		cv_queKv_.wait(lck);

		while (queKv_.size()) {
			{// 取队首协议
				MtxLck lck1(mtx_queKv_);
				proto = queKv_.front();
				queKv_.pop_front();
			}
			process_kv_client(proto);
		}
	}
}
