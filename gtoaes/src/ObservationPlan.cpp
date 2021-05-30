/**
 * @file ObservationPlan.cpp
 * @brief 定义: 天文观测计划访问接口
 * @version 0.1
 * @date 2020-11-07
 * @author 卢晓猛
 */

#include "ObservationPlan.h"
#include "GLog.h"
//#include "DBxxxx.h"

ObservationPlan::ObservationPlan() {
	thrd_cycle_.reset(new boost::thread(boost::bind(&ObservationPlan::thread_cycle, this)));
}

ObservationPlan::~ObservationPlan() {
	thrd_cycle_->interrupt();
	const char* pstrAbandon = StateObservationPlan::ToString(StateObservationPlan::OBSPLAN_ABANDONED);
	for (ObsPlanVec::iterator it = plans_.begin(); it != plans_.end(); ++it) {
		if ((*it)->state < StateObservationPlan::OBSPLAN_OVER) {
			_gLog.Write("plan[%s] : %s", (*it)->plan_sn.c_str(), pstrAbandon);
		}
	}
	thrd_cycle_->join();
}

void ObservationPlan::AddPlan(ObsPlanItemPtr plan) {
	if (plan->state == StateObservationPlan::OBSPLAN_CATALOGED) {
		MtxLck lck(mtx_);
		ObsPlanVec::iterator it;
		for (it = plans_.begin(); it != plans_.end() && (*it)->priority >= plan->priority; ++it);
		plans_.insert(it, plan);
	}
}

bool ObservationPlan::Find(const string& gid, const string& uid) {
	MtxLck lck(mtx_);
	gid_obss_ = gid;
	uid_obss_ = uid;
	itend_    = plans_.end();
	for (itnow_ = plans_.begin();
			itnow_ != itend_
					&& (*itnow_)->state <= StateObservationPlan::OBSPLAN_INTERRUPTED
					&& !(*itnow_)->IsMatched(gid, uid);
			++itnow_);
	return itnow_ != itend_;
}

bool ObservationPlan::GetNext(ObsPlanItemPtr& plan) {
	MtxLck lck(mtx_);
	plan.reset();
	if (itnow_ != itend_) {
		plan = *itnow_;
		while (++itnow_ != itend_
				&& (*itnow_)->state <= StateObservationPlan::OBSPLAN_INTERRUPTED
				&& !(*itnow_)->IsMatched(gid_obss_, uid_obss_));
	}
	return plan.use_count();
}

ObsPlanItemPtr ObservationPlan::Find(const string& plan_sn) {
	MtxLck lck(mtx_);
	ObsPlanItemPtr plan;
	ObsPlanVec::iterator it;
	for (it = plans_.begin(); it != plans_.end() && plan_sn != (*it)->plan_sn; ++it);
	if (it != plans_.end()) plan = *it;
	return plan;
}

void ObservationPlan::thread_cycle() {
	while (1) {
		// 中午检查清理观测计划
		ptime now    = second_clock::local_time();
		ptime next12 = ptime(now.date(), hours(14));
		int secs = (next12 - now).total_seconds();
		if (secs < 10) secs += 86400;
		boost::this_thread::sleep_for(boost::chrono::seconds(secs));
		// 清理无效(已执行、已抛弃)计划
		MtxLck lck(mtx_);
		now = second_clock::universal_time();
		for (ObsPlanVec::iterator it = plans_.begin(); it != plans_.end();) {
			// 改变计划状态
			if (((*it)->tmend - now).total_seconds() < (*it)->period				// 时间限制
					&& (*it)->state <= StateObservationPlan::OBSPLAN_INTERRUPTED)	// 状态限制
				(*it)->state = StateObservationPlan::OBSPLAN_ABANDONED;
			// 清理已结束的计划
			if ((*it)->state >= StateObservationPlan::OBSPLAN_OVER) {
				//...上传到数据库
				_gLog.Write("plan[%s] : %s", (*it)->plan_sn.c_str(), StateObservationPlan::ToString((*it)->state));
				it = plans_.erase(it);
			}
			else ++it;
		}
	}
}
