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
#include "Parameter.h"
#include "NTPClient.h"
#include "AsioTCP.h"
#include "AsioUDP.h"
#include "KvProtocol.h"
#include "NonkvProtocol.h"
#include "ObservationPlan.h"
#include "ObservationSystem.h"
#include "DatabaseCurl.h"

//////////////////////////////////////////////////////////////////////////////
class GeneralControl {
/* 构造函数 */
public:
	GeneralControl();
	virtual ~GeneralControl();

//< 数据类型
protected:
	//////////////////////////////////////////////////////////////////////////////
	typedef std::vector<TcpCPtr> TcpCVec;		//< 网络连接存储区
	using MtxLck = boost::unique_lock<boost::mutex>;	//< 信号灯互斥锁
	using ThreadPtr = boost::shared_ptr<boost::thread>;	//< boost线程指针
	using ObsSysVec = std::vector<ObsSysPtr>;

	enum {// 对应主机类型
		PEER_CLIENT,			//< 客户端
		PEER_MOUNT,				//< GWAC望远镜
		PEER_CAMERA,			//< 相机
		PEER_MOUNT_ANNEX,		//< 镜盖+调焦+天窗(GWAC)
		PEER_CAMERA_ANNEX,		//< 温控+真空(GWAC-GY)
		PEER_LAST		//< 占位, 不使用
	};

	/*!
	 * @struct network_event
	 * @brief 网络事件
	 */
	struct NetworkEvent {
		using Pointer = boost::shared_ptr<NetworkEvent>;

		TcpCPtr client;	///< 网络连接
		int peer;	///< 主机类型
		int type;	///< 事件类型. 0: 接收信息; 其它: 关闭

	public:
		NetworkEvent(TcpCPtr _client, int _peer, int _type) {
			client = _client;
			peer   = _peer;
			type   = _type;
		}

		static Pointer Create(TcpCPtr _client, int _peer, int _type) {
			return Pointer(new NetworkEvent(_client, _peer, _type));
		}
	};
	using NetEvPtr = NetworkEvent::Pointer;
	using NetEvQue = std::deque<NetEvPtr>;

	/*!
	 * @struct EnvInfo
	 * @brief 环境信息
	 */
	struct EnvInfo {
		using Pointer = boost::shared_ptr<EnvInfo>;

		string gid;	///< 组标志
		int rain;	///< 雨量标志
		int orient;	///< 风向
		int speed;	///< 风速
		int cloud;	///< 云量

	public:
		static Pointer Create() {
			return Pointer(new EnvInfo);
		}
	};
	using NfEnvPtr = EnvInfo::Pointer;
	using NfEnvVec = std::vector<NfEnvPtr>;

	//////////////////////////////////////////////////////////////////////////////

/* 成员变量 */
protected:
	//////////////////////////////////////////////////////////////////////////////
	Parameter param_;
	NTPPtr ntp_;

	/* 网络资源 */
	TcpSPtr	tcps_client_;		///< 网络服务: 客户端
	TcpSPtr tcps_mount_;		///< 网络服务: 转台
	TcpSPtr tcps_camera_;		///< 网络服务: 相机
	TcpSPtr tcps_mount_annex_;	///< 网络服务: 转台附属
	TcpSPtr tcps_camera_annex_;	///< 网络服务: 相机附属

	UdpPtr  udps_env_;			///< 网络服务: 气象环境, UDP

	TcpCVec tcpc_client_;
	TcpCVec tcpc_mount_;
	TcpCVec tcpc_camera_;
	TcpCVec tcpc_mount_annex_;
	TcpCVec tcpc_camera_annex_;

	boost::mutex mtx_tcpc_client_;			///< 互斥锁: 客户端
	boost::mutex mtx_tcpc_mount_;			///< 互斥锁: GWAC望远镜
	boost::mutex mtx_tcpc_camera_;			///< 互斥锁: 相机
	boost::mutex mtx_tcpc_mount_annex_;		///< 互斥锁: 转台附属
	boost::mutex mtx_tcpc_camera_annex_;	///< 互斥锁: 相机附属

	NetEvQue queNetEv_;			///< 网络事件队列
	boost::mutex mtx_netev_;	///< 互斥锁: 网络事件
	boost::condition_variable cv_netev_;	///< 条件触发: 网络事件

	boost::shared_array<char> buftcp_;	///< 网络信息存储区: 消息队列中调用
	boost::shared_array<char> bufudp_;	///< 网络信息存储区: 消息队列中调用
	KvProtoPtr kvProto_;		///< 键值对格式协议访问接口
	NonkvProtoPtr nonkvProto_;	///< 非键值对格式协议访问接口

	/* 观测计划 */
	ObsPlanPtr obsPlans_;		///< 观测计划集合
	boost::mutex mtx_obsPlans_;	///< 互斥锁: 观测计划集合

	/* 观测系统 */
	ObsSysVec obss_;		///< 观测系统集合
	boost::mutex mtx_obss_;	///< 互斥锁: 观测系统

	/* 环境信息 */
	NfEnvVec nfEnv_;	///< 环境

	/* 数据库 */
	DBCurlPtr dbPtr_;	///< 数据库访问接口

	/* 多线程 */
	ThreadPtr thrd_netevent_;

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
	/*----------------- 网络服务 -----------------*/
	/*!
	 * @brief 创建网络服务
	 * @param server 网络服务
	 * @param port   监听端口
	 * @return
	 * 创建结果
	 * 0 -- 成功
	 * 其它 -- 错误代码
	 */
	int create_server(TcpSPtr *server, const uint16_t port);
	/*!
	 * @brief 依据配置文件, 创建所有网络服务
	 * @return
	 * 创建结果
	 */
	bool create_all_server();
	/*!
	 * @brief 处理网络连接请求
	 * @param client 为连接请求分配额网络资源
	 * @param server 服务器标识
	 */
	void network_accept(const TcpCPtr client, const TcpSPtr server);
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_client(const TcpCPtr client, const error_code& ec);
	/*!
	 * @brief 处理转台信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_mount(const TcpCPtr client, const error_code& ec);
	/*!
	 * @brief 处理相机信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_camera(const TcpCPtr client, const error_code& ec);
	/*!
	 * @brief 处理镜盖+调焦信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_mount_annex(const TcpCPtr client, const error_code& ec);
	/*!
	 * @brief 处理温控+真空信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_camera_annex(const TcpCPtr client, const error_code& ec);
	/*!
	 * @brief 处理环境监测信息
	 */
	void receive_environment(const UdpPtr client, const error_code& ec);

protected:
	/*----------------- 消息机制 -----------------*/
	/*!
	 * @brief 从网络资源存储区里移除指定连接
	 * @param buff    存储区
	 * @param client  网络连接
	 */
	void erase_tcpclient(TcpCVec& buff, const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CLIENT
	 * @param client 网络连接
	 */
	void on_receive_client(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_RECEIVE_MOUNT
	 * @param client 网络连接
	 */
	void on_receive_mount(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CAMERA
	 * @param client 网络连接
	 */
	void on_receive_camera(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_RECEIVE_MOUNT_ANNEX
	 * @param client 网络连接
	 */
	void on_receive_mount_annex(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CAMERA_ANNEX
	 * @param client 网络连接
	 */
	void on_receive_camera_annex(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_CLOSE_CLIENT
	 * @param client 网络连接
	 */
	void on_close_client(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_CLOSE_MOUNT
	 * @param client 网络连接
	 */
	void on_close_mount(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_CLOSE_CAMERA
	 * @param client 网络连接
	 */
	void on_close_camera(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_CLOSE_MOUNT_ANNEX
	 * @param client 网络连接
	 */
	void on_close_mount_annex(const TcpCPtr client);
	/*!
	 * @brief 响应消息MSG_CLOSE_CAMERA_ANNEX
	 * @param client 网络连接
	 */
	void on_close_camera_annex(const TcpCPtr client);

protected:
	/*----------------- 解析/执行通信协议 -----------------*/
	/*!
	 * @brief 解析与用户/数据库、通用望远镜、相机、制冷(GWAC)、真空(GWAC)相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 * @note
	 * 与转台无关远程主机类型包括:
	 * - PEER_CLIENT
	 * - PEER_CAMERA
	 * - PEER_CAMERA_ANNEX
	 */
	void resolve_protocol(const TcpCPtr client, int peer);

	/*!
	 * @fn process_kv_client
	 * @brief  处理客户端的键值对协议
	 * @fn process_kv_mount
	 * @brief  处理转台的键值对协议
	 * @fn process_kv_camera
	 * @brief  处理相机的键值对协议
	 * @fn process_kv_mount_annex
	 * @brief  处理转台附属设备的键值对协议
	 * @fn process_kv_clientprocess_kv_camera_annex
	 * @brief  处理相机附属设备的键值对协议
	 *
	 * @param base    通信协议
	 * @param client  网络连接
	 */
	void process_kv_client      (kvbase base, const TcpCPtr client);
	void process_kv_mount       (kvbase base, const TcpCPtr client);
	void process_kv_camera      (kvbase base, const TcpCPtr client);
	void process_kv_mount_annex (kvbase base, const TcpCPtr client);
	void process_kv_camera_annex(kvbase base, const TcpCPtr client);

	/*!
	 * @fn process_nonkv_mount
	 * @brief   处理转台的非键值对协议
	 * @fn process_nonkv_mount_annex
	 * @brief   处理转台附属设备的非键值对协议
	 *
	 * @param proto   通信协议
	 * @param client  网络连接
	 */
	void process_nonkv_mount(nonkvbase base, const TcpCPtr client);
	void process_nonkv_mount_annex(nonkvbase base, const TcpCPtr client);

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
	 * @param 观测系统指针
	 * @return
	 * 获取到的观测计划
	 */
	ObsPlanItemPtr acquire_new_plan(const ObsSysPtr obss);
	/*!
	 * @brief 尝试立即执行计划
	 * @param plan  观测计划
	 */
	void tryto_implement_plan(ObsPlanItemPtr plan);
	/*!
	 * @brief 尝试中止观测计划
	 * @param plan_sn  计划编号
	 */
	void tryto_abort_plan(const string& plan_sn);

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

protected:
	/*----------------- 环境信息 -----------------*/
	/*!
	 * @brief 查找与gid对应的环境信息记录. 若记录不存在, 则创建记录
	 * @param gid  组标志
	 * @return
	 * 记录实例指针
	 */
	NfEnvPtr find_info_env(const string& gid);

protected:
	/*----------------- 多线程 -----------------*/
	/*!
	 * @brief 中止线程
	 * @param thrd 线程指针
	 */
	void interrupt_thread(ThreadPtr& thrd);
	/*!
	 * @brief 处理网络事件
	 * @note
	 * - 接收信息: 解析并投递执行
	 * - 关闭: 释放资源
	 */
	void monitor_network_event();
};

#endif /* SRC_GENERALCONTROL_H_ */
