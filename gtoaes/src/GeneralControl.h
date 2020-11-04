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
 */

#ifndef SRC_GENERALCONTROL_H_
#define SRC_GENERALCONTROL_H_

#include <vector>
#include <deque>
#include "MessageQueue.h"
#include "NTPClient.h"
#include "tcpasio.h"
#include "AsciiProtocol.h"
#include "Parameter.h"

class GeneralControl : public MessageQueue {
/* 构造函数 */
public:
	GeneralControl();
	virtual ~GeneralControl();

//< 数据类型
protected:
	typedef std::vector<TcpCPtr> TcpCVec;		//< 网络连接存储区

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
		MSG_RECEIVE_TELE,			//< 收到通用望远镜信息
		MSG_RECEIVE_MOUNT,			//< 收到通用望远镜信息
		MSG_RECEIVE_CAMERA,			//< 收到相机信息
		MSG_RECEIVE_TELE_ANNEX,		//< 收到镜盖+调焦信息
		MSG_RECEIVE_MOUNT_ANNEX,	//< 收到镜盖+调焦信息
		MSG_RECEIVE_CAMERA_ANNEX,	//< 收到温控+真空信息(GWAC-GY相机: 2017年)
		MSG_CLOSE_CLIENT,			//< 客户端断开网络连接
		MSG_CLOSE_TELE,				//< 通用望远镜断开网络连接
		MSG_CLOSE_MOUNT,			//< GWAC望远镜断开网络连接
		MSG_CLOSE_CAMERA,			//< 相机断开网络连接
		MSG_CLOSE_TELE_ANNEX,		// < 镜盖+调焦断开网络连接
		MSG_CLOSE_MOUNT_ANNEX,		// < 镜盖+调焦断开网络连接
		MSG_CLOSE_CAMERA_ANNEX,		//< 温控断开网络连接
		MSG_ACQUIRE_PLAN,			//< 申请观测计划
		MSG_LAST	//< 占位, 不使用
	};

	enum {// 对应主机类型
		PEER_CLIENT,			//< 客户端
		PEER_TELE,				//< 通用望远镜
		PEER_MOUNT,				//< GWAC望远镜
		PEER_CAMERA,			//< 相机
		PEER_TELE_ANNEX,		//< 镜盖+调焦+天窗(通用)
		PEER_MOUNT_ANNEX,		//< 镜盖+调焦+天窗(GWAC)
		PEER_CAMERA_ANNEX,		//< 温控+真空(GWAC-GY)
		PEER_LAST		//< 占位, 不使用
	};

	struct AcquirePlan {// 申请观测计划参数
		int type;	//< 观测系统类型, 两类: gwac; normal
		string gid;	//< 组标志
		string uid;	//< 单元标志

	public:
		AcquirePlan() {
			type = 0;
		}

		AcquirePlan(const int _t, const string& _g, const string& _u) {
			type = _t;
			gid  = _g;
			uid  = _u;
		}
	};
	typedef boost::shared_ptr<AcquirePlan> AcquirePlanPtr;
	typedef std::deque<AcquirePlanPtr> AcquirePlanQue;

/* 成员变量 */
protected:
	Parameter param_;
	NTPPtr ntp_;
	/* 网络资源 */
	TcpSPtr	tcps_client_;
	TcpSPtr tcps_tele_;
	TcpSPtr tcps_mount_;
	TcpSPtr tcps_camera_;
	TcpSPtr tcps_tele_annex_;	// 望远镜附属设备包括: 调焦, 镜盖, 天窗等
	TcpSPtr tcps_mount_annex_;	// 望远镜附属设备包括: 调焦, 镜盖, 天窗等
	TcpSPtr tcps_camera_annex_;

	TcpCVec tcpc_client_;
	TcpCVec tcpc_tele_;
	TcpCVec tcpc_mount_;
	TcpCVec tcpc_camera_;
	TcpCVec tcpc_tele_annex_;
	TcpCVec tcpc_mount_annex_;
	TcpCVec tcpc_camera_annex_;

	boost::mutex mtx_tcpc_client_;			//< 互斥锁: 客户端
	boost::mutex mtx_tcpc_tele_;			//< 互斥锁: 通用望远镜
	boost::mutex mtx_tcpc_mount_;			//< 互斥锁: GWAC望远镜
	boost::mutex mtx_tcpc_camera_;			//< 互斥锁: 相机
	boost::mutex mtx_tcpc_tele_annex_;		//< 互斥锁: 镜盖+调焦(通用)
	boost::mutex mtx_tcpc_mount_annex_;		//< 互斥锁: 镜盖+调焦(GWAC)
	boost::mutex mtx_tcpc_camera_annex_;	//< 互斥锁: 温控+真空(GWAC-GY)

	boost::shared_array<char> bufrcv_;	//< 网络信息存储区: 消息队列中调用
	AscProtoPtr ascproto_;

	/* 观测计划 */
	AcquirePlanQue que_acqplan_;	//< 观测计划申请参数队列
	boost::mutex mtx_acqplan_;		//< 互斥锁: 观测计划申请参数

	/* 观测系统 */

	/* 数据库 */

	/* 多线程 */
	threadptr thrd_clocksync_;

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
	 * @brief 处理GWAC望远镜信息
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
	void receive_telescope_annex(const long client, const long ec);
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
	 * - PEER_TELESCOPE_ANNEX
	 * - PEER_CAMERA_ANNEX
	 */
	void resolve_protocol_ascii(TCPClient* client, int peer);
	/*!
	 * @brief 解析与GWAC转台、GWAC镜盖+调焦相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 * @note
	 * 与转台无关远程主机类型包括:
	 * - PEER_MOUNT
	 * - PEER_MOUNT_ANNEX
	 */
	void resolve_protocol_gwac(TCPClient* client, int peer);

protected:
	/*----------------- 消息机制 -----------------*/
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
	 * @brief 响应消息MSG_RECEIVE_TELE
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
	 * @brief 响应消息MSG_RECEIVE_TELE_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_receive_telescope_annex(const long client, const long ec);
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
	 * @brief 响应消息MSG_CLOSE_TELE
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
	 * @brief 响应消息MSG_CLOSE_TELE_ANNEX
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_telescope_annex(const long client, const long ec);
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

protected:
	/*----------------- 观测计划 -----------------*/
	/*!
	 * @brief 回调函数, 为通用系统申请新的观测计划
	 */
	void acquire_new_plan(const int type, const string& gid, const string& uid);

protected:
	/*----------------- 多线程 -----------------*/
	/*!
	 * @brief 时钟同步
	 * @note
	 * - 每日正午检查时钟偏差, 当时钟偏差大于1秒时修正本机时钟
	 */
	void thread_clocksync();
};

#endif /* SRC_GENERALCONTROL_H_ */
