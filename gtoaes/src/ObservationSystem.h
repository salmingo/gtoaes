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
#include <boost/enable_shared_from_this.hpp>
#include "AsioTCP.h"
#include "ATimeSpace.h"
#include "KvProtocol.h"
#include "NonkvProtocol.h"
#include "ObservationPlanBase.h"
#include "Parameter.h"
#include "ATimeSpace.h"
#include "DatabaseCurl.h"

class ObservationSystem : public boost::enable_shared_from_this<ObservationSystem> {
public:
	ObservationSystem(const string& gid, const string& uid);
	virtual ~ObservationSystem();

public:
	/* 数据类型 */
	using Pointer = boost::shared_ptr<ObservationSystem>;
	using ThreadPtr = boost::shared_ptr<boost::thread>;
	using MtxLck = boost::unique_lock<boost::mutex>;
	using KvProtoQue = std::deque<kvbase>;
	using NonkvProtoQue = std::deque<nonkvbase>;

	/*!
	 * @brief 回调函数, 尝试从队列里获取可用的观测计划
	 */
	using AcquirePlanFunc = boost::signals2::signal<ObsPlanItemPtr (const Pointer)>;
	using CBSlot = AcquirePlanFunc::slot_type;

	enum {///< 观测系统工作模式
		OBSS_ERROR,		///< 错误
		OBSS_MANUAL,	///< 手动
		OBSS_AUTO		///< 自动
	};

protected:
	struct NetworkMount {
		TcpCPtr client;		///< 网络连接
		int type;			///< 通信协议类型
		int state;			///< 工作状态
		int errcode;		///< 错误代码
		double ra, dec;		///< 赤经/赤纬, 角度
		double azi, alt;	///< 方位/高度, 角度
		int coorsys;		///< 目标坐标系
		double objra, objdec;	///< 目标赤经/赤纬, 角度
		double objazi, objalt;	///< 目标方位/高度, 角度

	public:
		/*!
		 * @brief 重定义操作符(), 引用网络连接
		 * @return
		 * 网络连接
		 */
		TcpCPtr operator()() {
			return client;
		}
	};

	struct NetworkCamera {
		TcpCPtr client;		///< 网络连接
		string  cid;		///< 相机编号
		int		state;		///< 工作状态
		int		errcode;	///< 错误代码
		int		coolget;	///< 探测器温度, 量纲: 摄氏度
		string	filter;		///< 滤光片

	public:
		/*!
		 * @brief 重定义操作符(), 引用网络连接
		 * @return
		 * 网络连接
		 */
		TcpCPtr operator()() {
			return client;
		}
	};
	using NetCamVec = std::vector<NetworkCamera>;

protected:
	/* 成员变量 */
	string gid_;	///< 组标志
	string uid_;	///< 单元标志
	/*!
	 * @brief 自动化模式
	 * - true: 自动化模式
	 *   当转台、相机等设备状态良好时, 系统切换到自动观测模式
	 * - false: 非自动化模式
	 *   当转台、相机等设备状态良好时, 系统切换到手动响应模式
	 */
	bool robotic_;
	int mode_;		///< 系统工作模式

	double altLimit_;	///< 高度限位, 弧度
	const OBSSParam* param_;	///< 观测系统工作参数
	AstroUtil::ATimeSpace ats_;	///< 时空转换接口

	/* 观测计划 */
	ObsPlanItemPtr plan_now_;	///< 观测计划: 正在执行
	ObsPlanItemPtr plan_wait_;	///< 观测计划: 等待执行
	AcquirePlanFunc acqPlan_;	///< 回调函数, 尝试获取观测计划
	ThreadPtr thrd_acqPlan_;	///< 线程: 尝试获取观测计划
	boost::condition_variable cv_acqPlan_;	///< 条件变量: 获取观测计划

	/* 网络资源 */
	NetworkMount tcpc_mount_;	///< 网络连接: 转台

	/* 被投递的通信协议 */
	KvProtoQue queKv_;			///< 被投递的键值对协议队列
	boost::mutex mtx_queKv_;	///< 互斥锁: 键值对协议队列

	NonkvProtoQue queNonkv_;	///< 被投递的非键值对协议队列
	boost::mutex mtx_queNonkv_;	///< 互斥锁: 非键值对协议队列

	/* 数据库 */
	DBCurlPtr dbPtr_;	///< 数据库访问接口

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
	 * @brief 设置观测系统工作参数
	 * @param param  参数指针
	 */
	void SetParameter(const OBSSParam* param);
	/*!
	 * @brief 设置数据库访问接口
	 */
	void SetDBPtr(DBCurlPtr ptr);
	/*!
	 * @brief 注册回调函数: 请求新的观测计划
	 * @param slot  插槽函数
	 */
	void RegisterAcquirePlan(const CBSlot& slot);
	/*!
	 * @brief 查看观测系统唯一性标志组合
	 * @param gid  组标志
	 * @param uid  单元标志
	 */
	void GetIDs(string& gid, string& uid);
	/*!
	 * @brief 检查观测计划是否在可指向安全范围内
	 * @param plan  观测计划
	 * @param now   当前时间
	 * @return
	 * 是否在安全范围内
	 */
	bool IsSafePoint(ObsPlanItemPtr plan, const ptime& now);
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
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMount(const TcpCPtr client);
	/*!
	 * @brief 关联观测系统与相机
	 * @param client  网络连接
	 * @param cid     相机标志
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleCamera(const TcpCPtr client, const string& cid);
	/*!
	 * @brief 关联观测系统与转台附属设备
	 * @param client  网络连接
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMountAnnex(const TcpCPtr client);
	/*!
	 * @brief 关联观测系统与相机附属设备
	 * @param client  网络连接
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleCameraAnnex(const TcpCPtr client, const string& cid);
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
	 * @brief 解除观测系统与相机的关联
	 * @param client  网络连接
	 */
	void DecoupleCamera(const TcpCPtr client);
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
	 * @brief 投递来自客户端的键值对协议
	 * @param proto  通信协议
	 */
	void NotifyNonkvProtocol(nonkvbase proto);
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
	/*!
	 * @brief 处理转台信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_mount(const TcpCPtr client, const int ec);

	//////////////////////////////////////////////////////////////////////////////
	/* 观测计划 */
	/*!
	 * @brief 尝试执行当前观测计划
	 */
	void process_plan();
	/*!
	 * @brief 尝试中止当前观测计划
	 */
	void abort_plan();

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 多线程 */
	/*!
	 * @brief 线程: 尝试获取新的计划
	 * @note
	 * - 当前计划完成后, 尝试获取新计划
	 * -
	 */
	void thread_acquire_plan();
};
using ObsSysPtr = ObservationSystem::Pointer;

#endif /* SRC_OBSERVATIONSYSTEM_H_ */
