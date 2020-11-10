/**
 * @file ObservationPlan.h
 * @brief 定义: 天文观测计划访问接口
 * @version 0.1
 * @date 2020-11-07
 * @author 卢晓猛
 */

#ifndef OBSERVATIONPLAN_H_
#define OBSERVATIONPLAN_H_

#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/thread.hpp>
#include "ObservationPlanBase.h"

/////////////////////////////////////////////////////////////////////////////
class ObservationPlan {
public:
	ObservationPlan();
	virtual ~ObservationPlan();

	/* 数据类型 */
protected:
	using ObsPlanVec = std::vector<ObsPlanItemPtr>;		///< 观测计划集合
	using MtxLck = boost::unique_lock<boost::mutex>;	///< 互斥锁
	using ThreadPtr = boost::shared_ptr<boost::thread>;	///< 线程

public:
	using Pointer = boost::shared_ptr<ObservationPlan>;

protected:
	/* 成员变量 */
	/*!
	 * @brief 观测计划集合
	 * @note
	 * - 集合按照优先级, 降序排序
	 */
	ObsPlanVec plans_;
	ObsPlanVec::iterator itnow_;	///< 当前对象指针
	ObsPlanVec::iterator itend_;	///< 集合结束指针
	boost::mutex mtx_;		///< 互斥锁: 观测计划
	ThreadPtr thrd_cycle_;	///< 定时检查计划的有效性, 无效计划移除队列

	string gid_obss_, uid_obss_;	///< 搜索观测计划时的观测系统编号

public:
	/*!
	 * @brief 创建实例并返回其指针
	 * @return
	 * 实例指针, boost::shared_ptr<>
	 */
	static Pointer Create() {
		return Pointer(new ObservationPlan);
	}
	/*!
	 * @brief 增加一条计划
	 * @param plan  计划指针
	 */
	void AddPlan(ObsPlanItemPtr plan);
	/*!
	 * @brief 搜索适用于观测系统的计划
	 * @param gid  观测系统组标志
	 * @param uid  观测系统单元标志
	 * @return
	 * 搜索成功标志
	 */
	bool Find(const string& gid, const string& uid);
	/*!
	 * @brief 获得下一个搜索结果
	 * @param plan  计划指针
	 * @return
	 * 获取成功标志
	 */
	bool GetNext(ObsPlanItemPtr& plan);
	/*!
	 * @brief 搜索plan_sn对应的观测计划
	 * @param plan_sn  计划编号
	 * @return
	 * 对应的观测计划
	 */
	ObsPlanItemPtr Find(const string& plan_sn);

protected:
	/*!
	 * @brief 线程
	 * @note
	 * - 定时检查计划的有效性性. 判据: 结束时间
	 * - 移除无效计划
	 * - 移除已完成和已中断计划
	 * - 记录日志
	 */
	void thread_cycle();
};
using ObsPlanPtr = ObservationPlan::Pointer;

#endif /* OBSERVATIONPLAN_H_ */
