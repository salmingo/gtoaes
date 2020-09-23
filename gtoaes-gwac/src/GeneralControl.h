/*
 * @file GeneralControl.h 软件总控入口声明文件
 * @author 卢晓猛
 * @date 2017-1-19
 * @version 0.3
 *
 * @date 2017-05-06
 * @version 0.4
 * @li 构造函数中传入io_service对象, 当软件运行遇到致命性错误时可以主动结束
 */

#ifndef GENERALCONTROL_H_
#define GENERALCONTROL_H_

#include <vector>
#include <boost/asio.hpp>
#include "msgque_base.h"
#include "tcp_asio.h"
#include "NTPClient.h"
#include "ObservationSystem.h"
#include "parameter.h"
#include "asciiproto.h"
#include "mountproto.h"

class GeneralControl : public msgque_base {
public:
	GeneralControl(boost::asio::io_service& ios_main);
	virtual ~GeneralControl();

public:
	/* 公共成员函数 */
	/*!
	 * @brief 启动总控服务
	 * @return
	 * 服务启动结果
	 */
	bool Start();
	/*!
	 * @brief 停止总控服务
	 */
	void Stop();

private:
	/* 私有成员函数 */
	/*!
	 * @brief 启动所有网络服务
	 * @return
	 * 网络服务启动结果
	 */
	bool StartServers();
	/*!
	 * @brief 单个网络服务启动结果
	 * @param server 网络服务访问入口
	 * @param port   网络服务端口
	 * @return
	 * 服务启动结果
	 */
	bool StartServer(tcpsptr *server, int port);
	/*!
	 * @brief 查找与gid和uid匹配的观测系统
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @return
	 * 匹配的观测系统访问接口
	 * @note
	 * 若观测系统不存在, 则先创建该系统
	 */
	obssptr find_os(const std::string& gid, const std::string& uid);

private:
	/* 回调函数 */
	/*!
	 * @brief 处理收到的网络连接请求
	 * @param client 为远程连接请求分配的本地资源, 数据类型为shared_ptr<tcp_client>
	 * @param param  处理远程连接请求的服务器
	 */
	void NetworkAccept(const tcpcptr& client, const long param);
	/*!
	 * @brief 处理来自控制台客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveClient(const long client, const long ec);
	/*!
	 * @brief 处理来自数据库客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveDatabase(const long client, const long ec);
	/*!
	 * @brief 处理来自转台客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveMount(const long client, const long ec);
	/*!
	 * @brief 处理来自相机客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveCamera(const long client, const long ec);
	/*!
	 * @brief 处理来自转台附属设备客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveMountAnnex(const long client, const long ec);

private:
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_messages();
	/* 消息响应函数 */
	/*!
	 * @brief 处理消息MSG_RECEIVE_CLIENT, 即来自客户端的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveClient  (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_RECEIVE_DATABASE, 即来自数据库的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveDatabase(long param1, long param2);
	/*!
	 * @brief 处理消息MSG_RECEIVE_MOUNT, 即来自转台的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveMount   (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_RECEIVE_CAMERA, 即来自相机的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveCamera  (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_RECEIVE_MOUNTANNEX, 即来自转台附属设备的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveMountAnnex  (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_CLOSE_CLIENT, 即与客户端之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseClient    (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_CLOSE_DATABASE, 即与数据库之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseDatabase  (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_CLOSE_MOUNT, 即与转台之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseMount     (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_CLOSE_CAMERA, 即与相机之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseCamera    (long param1, long param2);
	/*!
	 * @brief 处理消息MSG_CLOSE_MOUNTANNEX, 即与转台附属设备之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseMountAnnex (long param1, long param2);
	/*!
	 * @brief 解析ASCII编码通信协议
	 * @param client 网络连接资源
	 * @param type   远程主机类型. enum peer_type. 1: 控制台; 2: 数据库; 3: 转台; 4: 相机
	 */
	void ResolveAscProtocol(tcp_client* client, int type);
	/*!
	 * @brief 处理控制台编码协议
	 * @param proto_type 协议类型
	 * @param proto_body 协议主体
	 */
	void ProcessClientProtocol(std::string& proto_type, apbase& proto_body);
	/*!
	 * @brief 处理相机编码协议
	 * @param client 网络连接资源
	 * @param proto_type 协议类型
	 * @param proto_body 协议主体
	 */
	void ProcessCameraProtocol(tcp_client* client, std::string& proto_type, apbase& proto_body);
	/*!
	 * @brief 解析转台相关通信协议
	 * @param client 网络连接资源
	 */
	void ResolveMountProtocol(tcp_client* client);
	/*!
	 * @brief 处理转台编码协议
	 * @param client 网络连接资源
	 * @param proto_type 协议类型
	 * @param proto_body 协议主体
	 */
	void ProcessMountProtocol(tcp_client* client, std::string& proto_type, mpbase& proto_body);

private:
	/*!
	 * @brief 线程: 监测网络连接活动
	 */
	void ThreadNetwork();

private:
	enum {// 消息类型
		MSG_RECEIVE_CLIENT = MSG_USER,	//< 消息: 收到来自控制台客户端的网络信息
		MSG_RECEIVE_DATABASE,			//< 消息: 收到来自数据库客户端的网络信息
		MSG_RECEIVE_MOUNT,				//< 消息: 收到来自转台的网络信息
		MSG_RECEIVE_CAMERA,				//< 消息: 收到来自相机的网络信息
		MSG_RECEIVE_MOUNTANNEX,			//< 消息: 收到来自转台附属设备的网络信息
		MSG_CLOSE_CLIENT,				//< 消息: 控制台客户端已断开
		MSG_CLOSE_DATABASE,				//< 消息: 数据库客户端已断开
		MSG_CLOSE_MOUNT,				//< 消息: 转台客户端已断开
		MSG_CLOSE_CAMERA,				//< 消息: 相机客户端已断开
		MSG_CLOSE_MOUNTANNEX,			//< 消息: 转台附属设备客户端已断开
		MSG_END		// 最后一条消息, 占位, 不使用
	};

	enum peer_type {// 远程主机类型
		PEER_CLIENT,		// 控制台客户端
		PEER_DATABASE,		// 数据库
		PEER_MOUNT,			// 转台
		PEER_CAMERA,		// 相机
		PEER_MOUNT_ANNEX,	// 转台附属设备
		PEER_LAST		// 占位符, 不使用
	};

	struct disabled_device {// 禁用设备
		std::string gid;	//< 组标志
		std::string uid;	//< 单元标志
		std::string cid;	//< 相机标志
	};
	typedef boost::shared_ptr<disabled_device> disdevptr;

	/* 声明数据类型 */
	typedef std::vector<tcpcptr> tcpcvec;			//< boost::shared_ptr<tcp_client>缓存区
	typedef std::vector<obssptr> obssvec;			//< obssptr缓存区
	typedef std::vector<disdevptr> disdevvec;		//< 禁用设备列表
	typedef boost::unique_lock<boost::mutex> mutex_lock; //< 基于boost::mutex的互斥锁
	typedef boost::shared_ptr<boost::thread> threadptr;	//< 线程指针

	/* 成员变量 */
	boost::asio::io_service *iosmain_;	//< 主程序io_service服务
	boost::shared_ptr<param_config> param_;//< 配置参数
	boost::shared_ptr<NTPClient> ntp_;	//< NTP网络时钟同步服务
/*-------------------- 观测系统 --------------------*/
	obssvec obss_;				//< 观测系统缓冲区
	boost::mutex mutex_obss_;	//< 观测系统缓冲区互斥锁

	disdevvec disdev_;			//< 禁用设备列表
	boost::mutex mutex_disdev_;	//< 禁用设备互斥锁
/*-------------------- 网络资源 --------------------*/
	tcpsptr tcps_client_;		//< 面向客户端的网络服务器
	tcpsptr tcps_db_;			//< 面向数据库的网络服务器
	tcpsptr tcps_mount_;		//< 面向转台的网络服务器
	tcpsptr tcps_camera_;		//< 面向相机的网络服务器
	tcpsptr tcps_mountannex_;	//< 面向转台附属设备的网络服务器

	tcpcvec tcpc_client_;		//< 面向客户端的网络连接缓冲区
	tcpcvec tcpc_db_;			//< 面向数据库的网络连接缓冲区
	tcpcvec tcpc_mount_;		//< 面向转台的网络连接缓冲区
	tcpcvec tcpc_camera_;		//< 面向相机的网络连接缓冲区
	tcpcvec tcpc_mountannex_;	//< 面向转台附属设备的网络连接缓冲区

	boost::mutex mutex_client_;			//< 面向客户端网络连接缓冲区的互斥锁
	boost::mutex mutex_db_;				//< 面向数据库网络连接缓冲区的互斥锁
	boost::mutex mutex_mount_;			//< 面向转台网络连接缓冲区的互斥锁
	boost::mutex mutex_camera_;			//< 面向相机网络连接缓冲区的互斥锁
	boost::mutex mutex_mountannex_;		//< 面向转台附属设备网络连接缓冲区的互斥锁

	boost::shared_array<char> bufrcv_;	//< 网络信息接收缓存区. 网络事件经消息队列串行化, 故可使用单一缓冲区
	ascptr ascproto_;					//< ASCII编码网络信息解码编码接口
	mntptr mntproto_;					//< 转台编码网络信息编码解码接口
/*-------------------- 线程 --------------------*/
	threadptr thrd_net_;	//< 网络客户端监测线程. 当网络连接长时间无活动时, 触发销毁操作
};

#endif /* GENERALCONTROL_H_ */
