/**
 * @file ObservationSystem.h
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#ifndef SRC_OBSERVATIONSYSTEM_H_
#define SRC_OBSERVATIONSYSTEM_H_

#include "AsciiProtocol.h"
#include "AsioTCP.h"
#include "ATimeSpace.h"
#include "ObservationPlanBase.h"
#include "Parameter.h"

class ObservationSystem {
public:
	ObservationSystem();
	virtual ~ObservationSystem();

public:
	/* 数据类型 */
	using Pointer = boost::shared_ptr<ObservationSystem>;
	using ThreadPtr = boost::shared_ptr<boost::thread>;
	using MtxLck = boost::unique_lock<boost::mutex>;

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

public:
	/* 接口 */
	/*!
	 * @brief 创建实例指针
	 * @return
	 * boost::shared_ptr<>型指针
	 */
	static Pointer Create() {
		return Pointer(new ObservationSystem);
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
	 * 匹配结果
	 */
	bool IsMatched(const string& gid, const string& uid);
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

	/* 功能 */
protected:
	/* 网络通信 */

protected:
	/* 多线程 */
	/*!
	 * @brief 中止线程
	 * @param thrd 线程指针
	 */
	void interrupt_thread(ThreadPtr& thrd);
};
using ObsSPtr = ObservationSystem::Pointer;

#endif /* SRC_OBSERVATIONSYSTEM_H_ */
