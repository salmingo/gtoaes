/**
 * @file GeneralControl.h 声明文件, 封装总控服务
 * @version 0.3
 * @date 2017-10-02
 * - 维护网络服务器
 * - 维护网络连接
 * - 维护观测系统
 * - 更新时钟
 */

#ifndef GENERALCONTROL_H_
#define GENERALCONTROL_H_

#include <boost/container/stable_vector.hpp>
#include <boost/container/deque.hpp>
#include "../Parameter.h"
#include "MessageQueue.h"
#include "NTPClient.h"
#include "tcpasio.h"
#include "AsciiProtocol.h"
#include "MountProtocol.h"
#include "ObservationSystemGWAC.h"
#include "ObservationSystemNormal.h"
#include "ObservationPlan.h"
#include "DataTransfer.h"

/*!
 * @struct ExObservationPlan 扩展观测计划, 关联: 观测计划-观测系统
 */
struct ExObservationPlan : public ObservationPlan {
	ObsSysPtr obss;

public:
	ExObservationPlan(apappplan ap) : ObservationPlan(ap) {
	}

	/*!
	 * @brief 为观测计划分配观测系统
	 * @param x 观测系统指针
	 */
	void AssignObservationSystem(ObsSysPtr x) {
		obss = x;
	}
};
typedef boost::shared_ptr<ExObservationPlan> ExObsPlanPtr;
typedef boost::container::stable_vector<ExObsPlanPtr> ExObsPlanVec;

class GeneralControl: public MessageQueue {
public:
	GeneralControl();
	virtual ~GeneralControl();

protected:
	//< 数据类型
	typedef boost::container::stable_vector<TcpCPtr> TcpCVec;		//< 网络连接存储区
	typedef boost::container::stable_vector<ObsSysPtr> ObsSysVec;	//< 观测系统存储区
	typedef boost::container::stable_vector<apappplan> PairPlanVec;	//< 配对观测计划存储区

protected:
	//< 数据类型
	/*!
	 * @note
	 * 2017年
	 * - 每台相机使用一条网络连接
	 * - 每台转台使用一条网络连接(Normal)
	 * - 多个转台使用一条网络连接(GWAC)
	 * - 多个调焦(+镜盖)使用一条网络连接(GWAC)
	 * - 多个温控使用一条网络连接(GWAC-GY)
	 * - 多个真空器使用一条网络连接(GWAC-GY)
	 */
	enum MSG_GC {// 总控服务消息
		MSG_RECEIVE_CLIENT = MSG_USER,	//< 收到客户端信息
		MSG_RECEIVE_TELESCOPE,		//< 收到通用望远镜信息
		MSG_RECEIVE_MOUNT,			//< 收到GWAC转台信息
		MSG_RECEIVE_CAMERA,			//< 收到相机信息
		MSG_RECEIVE_MOUNT_ANNEX,	//< 收到镜盖+调焦信息
		MSG_RECEIVE_CAMERA_ANNEX,	//< 收到温控+真空信息(GWAC-GY相机: 2017年)
		MSG_CLOSE_CLIENT,		//< 客户端断开网络连接
		MSG_CLOSE_TELESCOPE,	//< 通用望远镜断开网络连接
		MSG_CLOSE_MOUNT,		//< 转台断开网络连接
		MSG_CLOSE_CAMERA,		//< 相机断开网络连接
		MSG_CLOSE_MOUNT_ANNEX,	// < 镜盖+调焦断开网络连接
		MSG_CLOSE_CAMERA_ANNEX,	//< 温控断开网络连接
		MSG_ACQUIRE_PLAN,		//< 申请观测计划
		MSG_LAST	//< 占位, 不使用
	};

	enum PEER_TYPE {// 对应主机类型
		PEER_CLIENT,		//< 客户端
		PEER_TELESCOPE,		//< 通用望远镜
		PEER_MOUNT,			//< GWAC转台
		PEER_CAMERA,		//< 相机
		PEER_MOUNT_ANNEX,	//< 镜盖+调焦
		PEER_CAMERA_ANNEX,	//< 温控+真空(GWAC-GY)
		PEER_LAST		//< 占位, 不使用
	};

	struct AcquirePlan {// 申请观测计划参数
		string type;	//< 观测系统类型, 两类: gwac; normal
		string gid;		//< 组标志
		string uid;		//< 单元标志

	public:
		AcquirePlan() {
		}

		AcquirePlan(const string& _t, const string& _g, const string& _u) {
			type = _t;
			gid  = _g;
			uid  = _u;
		}
	};
	typedef boost::shared_ptr<AcquirePlan> AcquirePlanPtr;
	typedef boost::container::deque<AcquirePlanPtr> AcquirePlanQue;

protected:
	// 成员变量
	boost::mutex mtx_tcpc_client_;			//< 互斥锁: 客户端
	boost::mutex mtx_tcpc_tele_;			//< 互斥锁: 通用望远镜
	boost::mutex mtx_tcpc_mount_;			//< 互斥锁: GWAC转台
	boost::mutex mtx_tcpc_camera_;			//< 互斥锁: 相机
	boost::mutex mtx_tcpc_mount_annex_;		//< 互斥锁: 镜盖+调焦(GWAC)
	boost::mutex mtx_tcpc_camera_annex_;	//< 互斥锁: 温控+真空(GWAC-GY)
	boost::mutex mtx_obss_gwac_;			//< 互斥锁, GWAC观测系统
	boost::mutex mtx_obss_normal_;			//< 互斥锁, 通用观测系统
	boost::mutex mtx_db_;					//< 数据库互斥锁
	boost::mutex mtx_plans_;				//< 互斥锁: 观测计划
	boost::mutex mtx_acqplan_;				//< 互斥锁: 观测计划申请参数

//////////////////////////////////////////////////////////////////////
	boost::shared_ptr<param_config> param_;	//< 配置参数
	NTPPtr ntp_;		//< NTP时钟同步接口

	/*---------------- GWAC观测计划 ----------------*/
	PairPlanVec     plan_pair_;			//< 配对观测计划
	ExObsPlanVec    plans_;				//< 观测计划集合
	AcquirePlanQue  que_acqplan_;		//< 观测计划申请参数队列
	threadptr       thrd_monitor_plan_;	//< 线程: 检查观测计划有效性

	/*---------------- 观测系统 ----------------*/
	ObsSysVec     obss_gwac_;			//< GWAC观测系统集合
	ObsSysVec     obss_normal_;			//< 通用观测系统集合
	threadptr     thrd_monitor_obss_;	//< 线程: 监测观测系统有效性

	/*----------------- 网络资源 -----------------*/
	boost::shared_array<char> bufrcv_;	//< 网络信息存储区: 消息队列中调用
	AscProtoPtr ascproto_;	//< 通用协议解析接口
	MountPtr mntproto_;		//< GWAC转台协议解析接口

	TcpCVec tcpc_client_;		//< 网络连接: 客户端
	TcpCVec tcpc_tele_;			//< 网络连接: 通用望远镜
	TcpCVec tcpc_mount_;		//< 网络连接: GWAC转台
	TcpCVec tcpc_camera_;		//< 网络连接: 相机
	TcpCVec tcpc_mount_annex_;	//< 网络连接: 镜盖+调焦(GWAC)
	TcpCVec tcpc_camera_annex_;	//< 网络连接: 温控+真空(GWAC-GY)

	TcpSPtr tcps_client_;		//< 网络服务: 客户端
	TcpSPtr tcps_tele_;			//< 网络服务: 通用望远镜
	TcpSPtr tcps_mount_;		//< 网络服务: 转台
	TcpSPtr tcps_camera_;		//< 网络服务: 相机
	TcpSPtr tcps_mount_annex_;	//< 网络服务: 镜盖+调焦(GWAC)
	TcpSPtr tcps_camera_annex_;	//< 网络服务: 温控+真空(GWAC-GY)

	/*---------------- 数据库 ----------------*/
	boost::shared_ptr<DataTransfer> db_;//< 数据库访问接口
	threadptr    thrd_status_;			//< 定时向数据库传送观测系统和观测计划工作状态

public:
	// 接口
	/*!
	 * @brief 启动总控服务
	 * @return
	 * 服务启动结果
	 */
	bool StartService();
	/*!
	 * @brief 结束总控服务
	 */
	void StopService();

protected:
	/*!
	 * @brief 查找制定的观测系统. 若系统不存在则创建该系统
	 * @param gid    组标志
	 * @param uid    单元标志
	 * @param ostype 观测系统类型
	 * @return
	 * 观测系统访问指针
	 */
	ObsSysPtr find_obss(const string& gid, const string& uid, int ostype = 0);
	/*!
	 * @brief 线程: 监测观测系统有效性
	 * @note
	 * 定时周期: 60秒
	 */
	void thread_monitor_obss();
	/*!
	 * @brief 线程: 检查通用观测计划有效性
	 * @note
	 * 定时周期: 1800秒=0.5小时
	 */
	void thread_monitor_plan();
	/*!
	 * @brief 线程: 向数据库发送观测系统和观测计划的工作状态
	 * @note
	 * 定时周期: 10秒
	 */
	void thread_status();
	/*!
	 * @brief 向数据库上传观测系统工作状态
	 * @param obss 观测系统集合
	 */
	void database_upload(ObsSysVec& obss);
	/*!
	 * @brief 退出程序时, 记录被抛弃的观测计划
	 */
	void exit_ignore_plan();
	/*!
	 * @brief 退出程序时, 显式关闭观测系统
	 * @param obss 观测系统集合
	 */
	void exit_close_obss(ObsSysVec &obss);
	/*!
	 * @brief 为通信协议ascii_proto_append_plan创建观测计划, 并入库管理
	 * @param plan 适用于GWAC系统的通信协议
	 * @return
	 * 适用于GWAC系统的观测计划
	 */
	ExObsPlanPtr catalogue_new_plan(apappplan proto);
	void catalogue_new_plan(ExObsPlanPtr plan);
	/*!
	 * @brief 修正通信协议中ascii_proto_append_plan中无效的起止时间
	 * @param proto 与观测计划对应的通信协议
	 */
	void correct_time(apappplan proto);
	/*!
	 * @brief 记录与观测计划对应的通信协议
	 * @param proto 通信协议, 对应观测计划
	 */
	void write_plan_log(apappplan proto);
	/*!
	 * @brief 记录观测计划日志
	 * @param x 观测计划指针
	 */
	void write_plan_log(ExObsPlanPtr x);
	/*!
	 * @brief 将扩展型观测计划指针转换为普通观测计划
	 * @param ptr 扩展计划指针
	 * @return
	 * 普通观测计划指针
	 */
	ObsPlanPtr to_obsplan(ExObsPlanPtr ptr);
	/*!
	 * @brief 将由普通观测计划指代的扩展型指针转换为原型
	 * @param ptr 普通指针指代的扩展型指针
	 * @return
	 * 扩展观测计划指针
	 */
	ExObsPlanPtr from_obsplan(ObsPlanPtr ptr);
	/*!
	 * @brief 回调函数, 当观测计划终止时触发
	 * @param ptr 观测计划指针
	 */
	void plan_finished(ObsPlanPtr ptr);
	/*!
	 * @brief 回调函数, 为GWAC系统申请新的观测计划
	 * @param addr ObservationSystem对象地址
	 * @note
	 * GWAC系统仅接受强匹配结果, 即gid:uid必须严格相同
	 */
	void acquire_new_gwac(const string& gid, const string& uid);
	/*!
	 * @brief 回调函数, 为通用系统申请新的观测计划
	 * @param addr ObservationSystem对象地址
	 */
	void acquire_new_plan(const string& gid, const string& uid);

protected:
	// 功能
	/*----------------- 网络服务与消息 -----------------*/
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
	void network_accept(const TcpCPtr& client, const long server);
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_client(const long client, const long ec);
	/*!
	 * @brief 处理通用望远镜信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_telescope(const long client, const long ec);
	/*!
	 * @brief 处理GWAC转台信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_mount(const long client, const long ec);
	/*!
	 * @brief 处理相机信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_camera(const long client, const long ec);
	/*!
	 * @brief 处理镜盖+调焦信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_mount_annex(const long client, const long ec);
	/*!
	 * @brief 处理温控+真空信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_camera_annex(const long client, const long ec);
	/*!
	 * @brief 解析与用户/数据库、通用望远镜、相机、制冷(GWAC)、真空(GWAC)相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 * @note
	 * 与转台无关远程主机类型包括:
	 * - PEER_CLIENT
	 * - PEER_TELESCOPE
	 * - PEER_CAMERA
	 * - PEER_CAMERA_ANNEX
	 */
	void resolve_protocol_ascii(TCPClient* client, PEER_TYPE peer);
	/*!
	 * @brief 解析与GWAC转台、GWAC镜盖+调焦相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 * @note
	 * 与转台无关远程主机类型包括:
	 * - PEER_MOUNT
	 * - PEER_MOUNT_ANNEX
	 */
	void resolve_protocol_mount(TCPClient* client, PEER_TYPE peer);

protected:
	// 功能
	/*----------------- 处理收到的网络信息 -----------------*/
	/*!
	 * @brief 处理来自用户/数据库的网络信息
	 * @param proto  信息主体
	 * @param peer   远程主机类型
	 * @param client 网络资源
	 */
	void process_protocol_client(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自通用望远镜的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_telescope(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自GWAC转台的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_mount(mpbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自相机的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_camera(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自GWAC镜盖+调焦的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_mount_annex(mpbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自GWAC制冷+真空的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_camera_annex(apbase proto, TCPClient* client);

protected:
	// 功能
	/*----------------- 处理收到的单条网络信息 -----------------*/
	/*!
	 * @brief 处理通信协议append_plan, 转换为通用观测计划并分配给观测系统
	 * @param plan 对应观测计划的通信协议
	 */
	void process_protocol_append_plan(apappplan plan);
	/*!
	 * @brief 处理通信协议append_gwac, 转换为GWAC专用观测计划并分配给观测系统
	 * @param plan 对应观测计划的通信协议
	 */
	void process_protocol_append_gwac(apappplan plan);
	/*!
	 * @brief 检查观测计划工作状态, 并将当前状态发送给客户端
	 * @param proto  通信协议
	 * @param client 发送通信协议的客户端网络连接
	 */
	void process_protocol_check_plan(apchkplan proto, TCPClient* client);
	/*!
	 * @brief 删除观测计划, 若计划正在执行则中止观测流程
	 * @param proto  通信协议
	 */
	void process_protocol_abort_plan(apabtplan proto);
	/*!
	 * @brief 注册关联客户端与观测系统
	 * @param proto  通信协议
	 * @param client 发送通信协议的客户端网络连接
	 */
	void process_protocol_register(apreg proto, TCPClient* client);
	/*!
	 * @brief 注销客户端与观测系统的关联
	 * @param proto  通信协议
	 * @param client 发送通信协议的客户端网络连接
	 */
	void process_protocol_unregister(apunreg proto, TCPClient* client);
	/*!
	 * @brief 重新加载配置参数, 并依据变更参数执行操作
	 */
	void process_protocol_reload();
	/*!
	 * @brief 重新启动软件服务
	 */
	void process_protocol_reboot();

protected:
	// 功能
	/*----------------- 消息响应函数 -----------------*/
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_message();
	/*!
	 * @brief 响应消息MSG_RECEIVE_CLIENT
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_client(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_RECEIVE_TELESCOPE
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_telescope(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_RECEIVE_MOUNT
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_mount(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CAMERA
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_camera(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_RECEIVE_MOUNT_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_mount_annex(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CAMERA_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_camera_annex(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_CLIENT
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_client(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_TELESCOPE
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_telescope(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_MOUNT
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_mount(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_CAMERA
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_camera(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_MOUNT_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_mount_annex(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_CLOSE_CAMERA_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_camera_annex(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_ACQUIRE_PLAN
	 */
	void on_acquire_plan(const long, const long);
};

#endif /* GENERALCONTROL_H_ */
