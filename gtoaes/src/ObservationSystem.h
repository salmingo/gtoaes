/**
 * @file ObservationSystem.h
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include <deque>
#include "AsioTCP.h"
#include "ATimeSpace.h"
#include "KvProtocol.h"
#include "ObservationPlanBase.h"
#include "Parameter.h"

class ObservationSystem {
public:
	ObservationSystem(const string& gid, const string& uid);
	virtual ~ObservationSystem();

public:
	/* 数据类型 */
	using Pointer = boost::shared_ptr<ObservationSystem>;
	using ThreadPtr = boost::shared_ptr<boost::thread>;
	using MtxLck = boost::unique_lock<boost::mutex>;
	using KvProtoQue = std::deque<kvbase>;

	/*!
	 * @brief 回调函数, 尝试从队列里获取可用的观测计划
	 */
	using AcquirePlanFunc = boost::signals2::signal<bool (const Pointer, ObsPlanItemPtr& plan)>;
	using CBSlot = AcquirePlanFunc::slot_type;

protected:

protected:
	/* 成员变量 */
	string gid_;	///< 组标志
	string uid_;	///< 单元标志

	/* 观测计划 */
	AcquirePlanFunc acqplan_;	///< 回调函数, 尝试获取观测计划

	/* 网络资源 */
	TcpCPtr tcpc_mount_;
	ObsPlanItemPtr plan_now_;	///< 观测计划: 正在执行
	ObsPlanItemPtr plan_wait_;	///< 观测计划: 等待执行

	/* 被投递的通信协议 */
	KvProtoQue queKv_;			///< 被投递的键值对协议队列
	boost::mutex mtx_queKv_;	///< 互斥锁: 键值对协议队列
	boost::condition_variable cv_queKv_;	///< 条件变量: 新的协议进入队列
	ThreadPtr thrd_queKv_;		///< 线程: 监测键值对协议队列

public:
	/* 接口 */
	/*!
	 * @brief 创建实例指针
	 * @return
	 * boost::shared_ptr<>型指针
	 */
	static Pointer Create(const string& gid, const string& uid) {
		return Pointer(new ObservationSystem(gid, uid));
	}
	/*!
	 * @brief 启动系统工作流程
	 * @return
	 * 启动结果
	 */
	bool Start();
	/*!
	 * @brief 停止系统工作流程
	 */
	void Stop();
	/*!
	 * @brief 检查系统活跃度
	 * @return
	 * 系统活跃度, 即前一日执行的计划条目数
	 */
	int ActiveCount();
	/*!
	 * @brief 检查是否与系统匹配
	 * @param gid  组标志
	 * @param uid  单元标志
	 * @return
	 * 匹配结果.
	 * - 0: 匹配失败
	 * - 1: 强匹配, gid和uid都相同
	 * - 2: 弱匹配, 符合相同原则
	 */
	int IsMatched(const string& gid, const string& uid);
	/*!
	 * @brief 获取观测系统正在执行计划的优先级
	 * @return
	 * 优先级
	 * @note
	 * - 系统空闲时, 优先级 == 0
	 * - 在执行计划时, \f$ prio = prio_{plan} * T / [T - (t - t_0)] \f$
	 * - 优先级限制: \f$ prio <= 4 * prio_{plan} \f$
	 */
	int GetPriority();
	/*!
	 * @brief 关联观测系统与客户端
	 * @param client  网络连接
	 */
	void CoupleClient(const TcpCPtr client);
	/*!
	 * @brief 关联观测系统与转台
	 * @param client  网络连接
	 */
	void CoupleMount(const TcpCPtr client, int type);
	/*!
	 * @brief 关联观测系统与相机
	 * @param client  网络连接
	 */
	void CoupleCamera(const TcpCPtr client, const string& cid);
	/*!
	 * @brief 关联观测系统与转台附属设备
	 * @param client  网络连接
	 */
	void CoupleMountAnnex(const TcpCPtr client);
	/*!
	 * @brief 关联观测系统与相机附属设备
	 * @param client  网络连接
	 */
	void CoupleCameraAnnex(const TcpCPtr client);
	/*!
	 * @brief 解除观测系统与客户端的关联
	 * @param client  网络连接
	 */
	void DecoupleClient(const TcpCPtr client);
	/*!
	 * @brief 解除观测系统与转台的关联
	 * @param client  网络连接
	 */
	void DecoupleMount(const TcpCPtr client);
	/*!
	 * @brief 解除观测系统与转台附属设备的关联
	 * @param client  网络连接
	 */
	void DecoupleMountAnnex(const TcpCPtr client);
	/*!
	 * @brief 解除观测系统与相机附属设备的关联
	 * @param client  网络连接
	 */
	void DecoupleCameraAnnex(const TcpCPtr client);
	/*!
	 * @brief 投递来自客户端的键值对协议
	 * @param proto  通信协议
	 */
	void NotifyKVProtocol(kvbase proto);
	/*!
	 * @brief 投递观测计划
	 * @param plan  观测计划
	 */
	void NotifyPlan(ObsPlanItemPtr plan);

	/* 功能 */
protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 网络通信 */
	/*!
	 * @brief 处理由上层程序投递到观测系统的键值对协议
	 * @param proto  通信协议
	 */
	void process_kv_client(kvbase proto);

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 多线程 */
	/*!
	 * @brief 中止线程
	 * @param thrd 线程指针
	 */
	void interrupt_thread(ThreadPtr& thrd);
	/*!
	 * @brief 线程: 监测并处理键值对队列中的通信协议
	 * @note
	 * - 通信协议由上层程序投递
	 * - 通信协议来自客户端
	 */
	void monitor_kv_queue();
	/*!
	 * @brief 线程: 定时检查网络连接的有效性
	 * @note
	 * - 周期: 1分钟
	 * - 判据:
	 */
	void monitor_network_connection();
};
using ObsSysPtr = ObservationSystem::Pointer;

#endif /* SRC_OBSERVATIONSYSTEM_H_ */
