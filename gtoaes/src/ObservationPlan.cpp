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
	itnow_ = itend_ = plans_.end();
	thrd_cycle_.reset(new boost::thread(boost::bind(&ObservationPlan::thread_cycle, this)));
}

ObservationPlan::~ObservationPlan() {
	thrd_cycle_->interrupt();
	for (ObsPlanVec::iterator it = plans_.begin(); it != plans_.end(); ++it) {
		if ((*it)->state < StateObservationPlan::OBSPLAN_OVER) {
			_gLog.Write("plan[%s] : %s", (*it)->plan_sn.c_str(),
					StateObservationPlan::ToString(StateObservationPlan::OBSPLAN_ABANDONED));
		}
	}
	thrd_cycle_->join();
}

void ObservationPlan::AddPlan(ObsPlanItemPtr plan) {
	MtxLck lck(mtx_);
	ObsPlanVec::iterator it;
	for (it = plans_.begin(); it != plans_.end() && (*it)->priority > plan->priority; ++it);
	plans_.insert(it, plan);
	itend_ = plans_.end();
}

bool ObservationPlan::Find(const string& gid, const string& uid) {
	MtxLck lck(mtx_);
	gid_obss_ = gid;
	uid_obss_ = uid;
	for (itnow_ = plans_.begin(); itnow_ != itend_ && !(*itnow_)->IsMatched(gid, uid); ++itnow_);
	return itnow_ != itend_;
}

bool ObservationPlan::GetNext(ObsPlanItemPtr& plan) {
	MtxLck lck(mtx_);
	plan.reset();
	if (itnow_ != itend_) {
		plan = *itnow_;
		while (++itnow_ != itend_ && !(*itnow_)->IsMatched(gid_obss_, uid_obss_));
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
	boost::chrono::minutes period(5);

	while (1) {
		boost::this_thread::sleep_for(period);

		if (itnow_ == itend_) {// 没有搜索观测计划
			MtxLck lck(mtx_);
			ptime now = second_clock::universal_time();
			ObsPlanVec::iterator it;
			for (it = plans_.begin(); it != plans_.end();) {
				// 改变计划状态
				if (((*it)->tmend - now).total_seconds() < (*it)->period				// 时间限制
						&& (*it)->state <= StateObservationPlan::OBSPLAN_INTERRUPTED)	// 状态限制
					(*it)->state = StateObservationPlan::OBSPLAN_ABANDONED;
				// 清理已结束的计划
				if ((*it)->state >= StateObservationPlan::OBSPLAN_OVER) {
					_gLog.Write("plan[%s] : %s", (*it)->plan_sn.c_str(), StateObservationPlan::ToString((*it)->state));
					//...上传到数据库
					it = plans_.erase(it);
				}
				else ++it;
			}

			itnow_ = itend_ = it;
		}
	}
}
