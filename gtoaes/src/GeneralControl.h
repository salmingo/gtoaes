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

//////////////////////////////////////////////////////////////////////////////
class GeneralControl : public MessageQueue {
/* 构造函数 */
public:
	GeneralControl();
	virtual ~GeneralControl();

/* 成员变量 */
protected:
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

	/* 观测系统 */
	using OBSSVec = std::vector<ObsSysPtr>;	///< 观测系统集合
	OBSSVec obss_;		///< 观测系统集合
	boost::mutex mtx_obss_;	///< 互斥锁: 观测系统

	/* 环境信息 */
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

	NfEnvVec nfEnv_;			///< 环境信息: 在线信息集合
	NfEnvQue que_nfEnv_;		///< 环境信息队列: 信息改变
	boost::mutex mtx_nfEnv_;	///< 互斥锁: 环境信息
	boost::condition_variable cv_nfEnvChanged_;	///< 条件触发: 信息改变
	ThreadPtr thrd_nfEnvChanged_;	///< 线程: 环境信息变化

	/* 数据库 */
	DBCurlPtr dbPtr_;	///< 数据库访问接口

	/* 观测时间类型 */
	ThreadPtr thrd_odt_;///< 线程: 计算观测系统所处的观测时间类型

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
	 * @brief 处理环境监测信息
	 */
	void receive_from_env(const UdpPtr client, const error_code& ec);
	/*!
	 * @brief 从网络资源存储区里移除指定连接, 该连接已关联观测系统
	 * @param client  网络连接
	 */
	void erase_coupled_tcp(const TcpCPtr client);

protected:
	/*----------------- 解析/执行通信协议 -----------------*/
	/*!
	 * @brief 解析与用户/数据库、通用望远镜、相机、制冷(GWAC)、真空(GWAC)相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	void resolve_from_peer(const TcpCPtr client, int peer);

	/*!
	 * @fn resolve_kv_client
	 * @brief  解析处理客户端的键值对协议
	 * @fn resolve_kv_mount
	 * @brief  解析处理转台的键值对协议
	 * @fn resolve_kv_camera
	 * @brief  解析处理相机的键值对协议
	 * @fn resolve_kv_mount_annex
	 * @brief  解析处理转台附属设备的键值对协议
	 * @fn resolve_kv_camera_annex
	 * @brief  解析处理相机附属设备的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_client      (const TcpCPtr client);
	void resolve_kv_mount       (const TcpCPtr client);
	void resolve_kv_camera      (const TcpCPtr client);
	void resolve_kv_mount_annex (const TcpCPtr client);
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
	 * @brief 天窗控制指令
	 * @param gid    组标志
	 * @param uid    单元标志
	 * @param cmd    控制指令
	 * @note
	 * - 由OBSS发送控制指令
	 * - 由天窗控制程序, 判断执行策略
	 */
	void command_slit(const string& gid, const string& uid, int cmd);

protected:
	/*----------------- 环境信息 -----------------*/
	/*!
	 * @brief 查找与gid对应的环境信息记录. 若记录不存在, 则创建记录
	 * @param gid  组标志
	 * @return
	 * 记录实例指针
	 */
	NfEnvPtr find_info_env(const string& gid);
	/*!
	 * @brief 创建ODT对象
	 * @param param  观测系统参数
	 */
	void create_info_env(const OBSSParam* param);

protected:
	/*----------------- 多线程 -----------------*/
	/*!
	 * @brief 处理变化的环境信息
	 */
	void thread_nfenv_changed();
	/*!
	 * @brief 计算观测时间类型
	 * @note
	 * - odt: Observation Duration Type
	 * - 计算各组标志系统对应的odt
	 * - odt执行不同类型的观测计划
	 */
	void thread_odt();
};

#endif /* SRC_GENERALCONTROL_H_ */
