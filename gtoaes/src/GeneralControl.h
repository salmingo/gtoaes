/**
 * @file GeneralControl.h 声明文件, 封装总控服务
 * @version 0.3
 * @date 2017-10-02
 * - 维护网络服务器
 * - 维护网络连接
 * - 维护观测系统
 * - 更新时钟
 * @version 1.0
 * @date 2020-07-05
 * - 观测系统建立后生命周期同主程序. 每日按照活跃度降序排序
 */

#ifndef GENERALCONTROL_H_
#define GENERALCONTROL_H_

#include <vector>
#include <deque>
#include "MessageQueue.h"
#include "Parameter.h"
#include "NTPClient.h"
#include "AsioUDP.h"
#include "ObservationSystem.h"
#include "DatabaseCurl.h"
#include "DomeSlit.h"
#include "KvProtocol.h"
#include "NonkvProtocol.h"
#include "ObservationPlan.h"
#include "TcpReceived.h"

//////////////////////////////////////////////////////////////////////////////
class GeneralControl : public MessageQueue {
/* 构造函数 */
public:
	GeneralControl();
	virtual ~GeneralControl();

/* 数据结构 */
protected:
	/*!
	 * @struct EnvInfo
	 * @brief 环境信息
	 */
	struct EnvInfo {
		using Pointer = boost::shared_ptr<EnvInfo>;

		const OBSSParam* param;	///< 观测系统参数
		string gid;	///< 组标志
		/* 气象 */
		bool safe;	///< 安全判定: 气象条件
		int rain;	///< 雨量标志
		int orient;	///< 风向
		int speed;	///< 风速
		int cloud;	///< 云量
		/* 时间 */
		int odt;	///< 观测时间类型

	public:
		EnvInfo(const string& gid) {
			param = NULL;
			this->gid = gid;
			safe   = false;
			rain   = -1;
			orient = speed = -1;
			cloud  = -1;
			odt    = -1;
		}

		static Pointer Create(const string& gid) {
			return Pointer(new EnvInfo(gid));
		}
	};
	using NfEnvPtr = EnvInfo::Pointer;
	using NfEnvVec = std::vector<NfEnvPtr>;
	using NfEnvQue = std::deque<NfEnvPtr>;

/* 成员变量 */
protected:
	//////////////////////////////////////////////////////////////////////////////
	enum {
		MSG_TCP_RECEIVE = MSG_USER,///< 收到TCP消息
		MSG_ENV_CHANGED	///< 气象信息改变的响应
	};

	//////////////////////////////////////////////////////////////////////////////
	Parameter param_;
	NTPPtr ntp_;

	/* 网络资源 */
	TcpSPtr	tcpS_client_;		///< 网络服务: 客户端
	TcpSPtr tcpS_mount_;		///< 网络服务: 转台
	TcpSPtr tcpS_camera_;		///< 网络服务: 相机
	TcpSPtr tcpS_mountAnnex_;	///< 网络服务: 转台附属
	TcpSPtr tcpS_cameraAnnex_;	///< 网络服务: 相机附属

	UdpPtr  udpS_env_;			///< 网络服务: 气象环境, UDP
	boost::shared_array<char> bufUdp_;	///< 网络信息存储区: 消息队列中调用

	TcpCVec tcpC_buff_;			///< 网络连接
	boost::mutex mtx_tcpC_buff_;///< 互斥锁: 网络连接
	ThreadPtr thrd_tcpClean_;	///< 线程: 释放已关闭的网络连接

	TcpRcvQue que_tcpRcv_;		///< 网络事件队列
	boost::mutex mtx_tcpRcv_;	///< 互斥锁: 网络事件

	boost::shared_array<char> bufTcp_;	///< 网络信息存储区: 消息队列中调用
	KvProtoPtr kvProto_;		///< 键值对格式协议访问接口
	NonkvProtoPtr nonkvProto_;	///< 非键值对格式协议访问接口

	/* 观测计划 */
	ObsPlanPtr obsPlans_;		///< 观测计划集合
	boost::mutex mtx_obsPlans_;	///< 互斥锁: 观测计划集合

	/* 观测系统 */
	using OBSSVec = std::vector<ObsSysPtr>;	///< 观测系统集合
	OBSSVec obss_;		///< 观测系统集合
	boost::mutex mtx_obss_;	///< 互斥锁: 观测系统

	/* 天窗 */
	SlitMulVec slit_;		///< 复用的天窗
	boost::mutex mtx_slit_;	///< 互斥锁：天窗

	/* 环境信息 */
	NfEnvVec nfEnv_;	///< 环境信息: 在线信息集合
	NfEnvQue que_nfEnv_;	///< 环境信息队列: 信息改变
	boost::mutex mtx_nfEnv_;	///< 互斥锁: 环境信息

	/* 数据库 */
	DBCurlPtr dbPtr_;	///< 数据库访问接口

	/* 观测时间类型 */
	ThreadPtr thrd_odt_;	///< 线程: 计算观测系统所处的观测时间类型
	ThreadPtr thrd_noon_;	///< 线程: 每天中午清理无效的资源

	//////////////////////////////////////////////////////////////////////////////
/* 接口 */
public:
	/*!
	 * @brief 启动服务
	 * @return
	 * 服务启动结果
	 */
	bool Start();
	/*!
	 * @brief 停止服务
	 */
	void Stop();

/* 功能 */
protected:
	/*----------------- 消息响应 -----------------*/
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_messages();
	/*!
	 * @brief 消息: 收到TCP信息
	 * @param par1  参数1, 保留
	 * @param par2  参数2, 保留
	 */
	void on_tcp_receive(const long par1, const long par2);
	/*!
	 * @brief 响应气象信息改变
	 * @param par1  保留
	 * @param par2  保留
	 */
	void on_env_changed(const long par1, const long par2);
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确
	 * @param peer   远程主机类型
	 */
	void receive_from_peer(const TcpCPtr client, const error_code& ec, int peer);

protected:
	/*----------------- 网络服务 -----------------*/
	/*!
	 * @brief 创建网络服务
	 * @param server 网络服务
	 * @param port   监听端口
	 * @return
	 * 创建结果
	 */
	bool create_server(TcpSPtr *server, const uint16_t port);
	/*!
	 * @brief 依据配置文件, 创建所有网络服务
	 * @return
	 * 创建结果
	 */
	bool create_all_server();
	/*!
	 * @brief 关闭网络服务
	 * @param server  网络服务
	 */
	void close_server(TcpSPtr& server);
	/*!
	 * @brief 关闭所有网络服务
	 */
	void close_all_server();
	/*!
	 * @brief 处理网络连接请求
	 * @param client 为连接请求分配额网络资源
	 * @param server 服务器标识
	 */
	void network_accept(const TcpCPtr client, const TcpSPtr server);
	/*!
	 * @brief 处理环境监测信息
	 */
	void receive_from_env(const UdpPtr client, const error_code& ec);
	/*!
	 * @brief 从网络资源存储区里移除指定连接, 该连接已关联观测系统
	 * @param client  网络连接
	 */
	void erase_coupled_tcp(const TcpCPtr client);
	/*!
	 * @brief 关闭套接口
	 * @param client  网络连接
	 * @param peer    远程主机类型
	 */
	void close_socket(const TcpCPtr client, int peer);

protected:
	/*----------------- 解析/执行通信协议 -----------------*/
	/*!
	 * @brief 解析与用户/数据库、通用望远镜、相机、制冷(GWAC)、真空(GWAC)相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	void resolve_from_peer(const TcpCPtr client, int peer);
	/*!
	 * @brief  解析处理客户端的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_client(const TcpCPtr client);
	/*!
	 * @brief  解析处理转台的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_mount(const TcpCPtr client);
	/*!
	 * @brief  解析处理相机的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_camera(const TcpCPtr client);
	/*!
	 * @brief  解析处理转台附属设备的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_mount_annex (const TcpCPtr client);
	/*!
	 * @brief  解析处理相机附属设备的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_camera_annex(const TcpCPtr client);

	/*!
	 * @fn process_nonkv_mount
	 * @brief   处理转台的非键值对协议
	 * @fn process_nonkv_mount_annex
	 * @brief   处理转台附属设备的非键值对协议
	 *
	 * @param proto   通信协议
	 * @param client  网络连接
	 */
	void process_nonkv_mount(const TcpCPtr client, nonkvbase base);
	void process_nonkv_mount_annex(const TcpCPtr client, nonkvbase base);

protected:
	/*----------------- 观测计划 -----------------*/
	/*!
	 * @brief 检查计划时间是否有效
	 * @param plan  观测计划
	 * @param now   当前UTC时间
	 * @return
	 * 时间有效性
	 */
	bool is_valid_plantime(const ObsPlanItemPtr plan, const ptime& now);
	/*!
	 * @brief 回调函数, 为通用系统申请新的观测计划
	 * @param obss  观测系统指针
	 * @return
	 * 获取到的观测计划
	 */
	ObsPlanItemPtr acquire_new_plan(const ObsSysPtr obss);
	/*!
	 * @brief 尝试立即执行计划
	 * @param plan  观测计划
	 */
	void try_implement_plan(ObsPlanItemPtr plan);
	/*!
	 * @brief 尝试中止观测计划
	 * @param plan_sn  计划编号
	 */
	void try_abort_plan(const string& plan_sn);

protected:
	/*----------------- 观测系统 -----------------*/
	/*!
	 * @brief 查找与(gid, uid)对应的观测系统. 若系统不存在, 则创建系统并启动工作流程
	 * @param gid  组标志
	 * @param uid  单元标志
	 * @return
	 * 观测系统实例指针
	 */
	ObsSysPtr find_obss(const string& gid, const string& uid);
	/*!
	 * @brief 查找与gid对应的多模天窗. 若数据结构不存在, 则创建
	 * @param gid     组标志
	 * @param client  网络连接
	 * @param kvtype  键值对类型
	 * @return
	 * 多模天窗接口
	 */
	SlitMulPtr find_slit(const string& gid, const TcpCPtr client, bool kvtype = true);
	/*!
	 * @brief 天窗控制指令
	 * @param gid    组标志
	 * @param uid    单元标志
	 * @param cmd    控制指令
	 * @note
	 * - 由OBSS发送控制指令
	 * - 由天窗控制程序, 判断执行策略
	 */
	void command_slit(const string& gid, const string& uid, int cmd);
	/*!
	 * @brief 控制多模天窗
	 * @param slit  多模天窗
	 * @param cmd   控制指令
	 */
	void command_slit(const SlitMulPtr slit, int cmd);

protected:
	/*----------------- 环境信息 -----------------*/
	/*!
	 * @brief 查找与gid对应的环境信息记录. 若记录不存在, 则创建记录
	 * @param gid        组标志
	 * @return
	 * 记录实例指针
	 */
	NfEnvPtr find_info_env(const string& gid);
	/*!
	 * @brief 创建ODT对象
	 * @param param  观测系统参数
	 * @return
	 * 记录实例指针
	 */
	NfEnvPtr find_info_env(const OBSSParam* param);

protected:
	/*----------------- 多线程 -----------------*/
	/*!
	 * @brief 集中清理已断开的网络连接
	 */
	void thread_clean_tcp();
	/*!
	 * @brief 计算观测时间类型
	 * @note
	 * - odt: Observation Duration Type
	 * - 计算各组标志系统对应的odt
	 * - odt执行不同类型的观测计划
	 */
	void thread_odt();
	/*!
	 * @brief 线程: 中午清理无效资源
	 */
	void thread_noon();
};

#endif /* SRC_GENERALCONTROL_H_ */
