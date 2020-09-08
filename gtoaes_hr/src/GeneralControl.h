/**
 * @class GeneralControl 总控服务器
 * @version 0.1
 * @date 2019-10-22
 */

#ifndef GENERALCONTROL_H_
#define GENERALCONTROL_H_

#include <vector>
#include "parameter.h"
#include "MessageQueue.h"
#include "tcpasio.h"
#include "AsciiProtocol.h"
#include "AnnexProtocol.h"
#include "ObservationSystem.h"
#include "ObservationPlan.h"
#include "DBCurl.h"
#include "NTPClient.h"
#include "ATimeSpace.h"

using AstroUtil::ATimeSpace;

class GeneralControl: public MessageQueue {
public:
	GeneralControl();
	virtual ~GeneralControl();

protected:
	/* 声明数据结构 */
	enum {// 终端类型
		PEER_CLIENT,
		PEER_MOUNT,
		PEER_CAMERA,
		PEER_FOCUS,
		PEER_ANNEX
	};

	enum {// 消息ID
		MSG_RECEIVE_CLIENT = MSG_USER,
		MSG_RECEIVE_MOUNT,
		MSG_RECEIVE_CAMERA,
		MSG_RECEIVE_ANNEX,
		MSG_CLOSE_CLIENT,
		MSG_CLOSE_MOUNT,
		MSG_CLOSE_CAMERA,
		MSG_CLOSE_ANNEX
	};

	struct EnvInfo {// 工作环境信息
		string gid;			//< 组标志
		TcpCPtr tcpclient;	//< 网络连接
		int mode;		//< 系统工作模式
		bool slitEnable;//< 启用天窗控制
		int slitState;	//< 天窗状态
		int rain;		//< 降水
		int odt;		//< 时段标志

	public:
		EnvInfo() {
			mode       = -1;
			slitEnable = true;
			slitState  = -1;
			rain       = -1;
			odt        = -1;
		}

		EnvInfo* Get(const string &_gid) {
			return gid == _gid ? this : NULL;
		}

		EnvInfo* Get(TcpCPtr client) {
			return (tcpclient == client) ? this : NULL;
		}

		EnvInfo* Get(TCPClient* client) {
			return (tcpclient.get() == client) ? this : NULL;
		}
	};

	typedef std::vector<TcpCPtr> TcpCVec;
	typedef std::vector<ObsSysPtr> ObsSysVec;
	typedef std::vector<EnvInfo> EnvInfoVec;

protected:
	/* 成员变量 */
	param_config param_;	//< 配置参数
	EnvInfoVec nfEnv_;		//< 工作环境
	NTPPtr ntp_;			//< NTP时钟同步接口

	/* 网络连接 */
	boost::mutex mtx_tcp_client_;	//< 互斥锁: 用户
	boost::mutex mtx_tcp_mount_;	//< 互斥锁: 转台
	boost::mutex mtx_tcp_camera_;	//< 互斥锁: 相机
	boost::mutex mtx_tcp_annex_;	//< 互斥锁: 附属(雨量+天窗+调焦)
	TcpSPtr tcps_client_;	//< 服务器: 用户
	TcpSPtr tcps_mount_;	//< 服务器: 转台
	TcpSPtr tcps_camera_;	//< 服务器: 相机
	TcpSPtr tcps_annex_;	//< 服务器: 附属
	TcpCVec tcpc_client_;	//< 客户端: 用户
	TcpCVec tcpc_mount_;	//< 客户端: 转台
	TcpCVec tcpc_camera_;	//< 客户端: 相机
	AscProtoPtr ascproto_;	//< 通信协议: AsciiProtocol接口
	AnnProtoPtr annproto_;	//< 通信协议: AnnexProtocol接口
	boost::shared_array<char> netrcv_;	//< 存储区: 网络信息
	/* 观测系统 */
	boost::mutex mtx_obss_;	//< 互斥锁: 观测系统
	ObsSysVec obss_;		//< 观测系统集合
	threadptr thrd_obss_;	//< 线程: 观测系统有效性
	/* 数据库 */
	boost::shared_ptr<DBCurl> dbt_; //< 数据库访问接口
	/* 观测计划 */
	boost::mutex mtx_obsplan_;	//< 互斥锁: 观测计划
	boost::condition_variable cv_loadplan_;	//< 条件: 手动加载观测计划
	ObsPlanVec obsplan_;		//< 存储区: 观测计划
	ATimeSpace ats_;		//< 天文时空接口
	threadptr thrd_odt_;	//< 线程: 计算当前时间的对应的天文观测时段
	threadptr thrd_obsplan_;	//< 线程: 观测计划

public:
	/*!
	 * @brief 启动总控服务
	 */
	bool Start();
	/*!
	 * @brief 停止总控服务
	 */
	void Stop();

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 网络通信 */
	/*!
	 * @brief 创建所有网络服务
	 */
	bool create_server();
	/*!
	 * @brief 在端口上建立TCP服务
	 * @param server 服务器器地址
	 * @param port   服务端口
	 */
	bool create_server(TcpSPtr *server, uint16_t port);
	/*!
	 * @brief 处理网络连接请求
	 * @param client 为连接请求分配额网络资源
	 * @param ptr    服务器标识
	 */
	void network_accept(const TcpCPtr &client, const long ptr);
	/*!
	 * @brief 处理客户端信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_client(const long ptr, const long ec);
	/*!
	 * @brief 处理GWAC转台信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_mount(const long ptr, const long ec);
	/*!
	 * @brief 处理相机信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_camera(const long ptr, const long ec);
	/*!
	 * @brief 处理附属设备信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_annex(const long ptr, const long ec);
	/*!
	 * @brief 解析与用户/数据库、转台、相机相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	void resolve_protocol_ascii(TCPClient* client, int peer);
	/*!
	 * @brief 解析与天窗+雨量+调焦相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	void resolve_protocol_annex(TCPClient* client, int peer);
	/*!
	 * @brief 处理来自用户/数据库的网络信息
	 * @param proto  信息主体
	 * @param peer   远程主机类型
	 * @param client 网络资源
	 */
	void process_protocol_client(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自转台的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_mount(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自相机的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_camera(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自附属设备的网络信息
	 * @param proto 信息主体
	 */
	void process_protocol_annex(annpbase proto, TCPClient* client);
	/*!
	 * @brief 启动自动观测工作逻辑
	 */
	void autobs(const string &gid, const string &uid, bool start = true);
	/*!
	 * @brief 打开/关闭天窗
	 */
	void command_slit(int cmd);
	/*!
	 * @brief 改变天窗状态
	 */
	void change_slitstate(int state);
	/*!
	 * @brief 改变降水状态
	 */
	void change_skystate();
	/* 网络通信 */
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 消息机制 */
	/*!
	 * @brief 注册消息及响应函数
	 */
	void register_message();
	/*!
	 * @brief 消息ID == MSG_RECEIVE_CLIENT, 接收解析网络信息: 用户
	 */
	void on_receive_client(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_RECEIVE_MOUNT, 接收解析网络信息: 转台
	 */
	void on_receive_mount(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_RECEIVE_CAMERA, 接收解析网络信息: 相机
	 */
	void on_receive_camera(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_RECEIVE_FOCUS, 接收解析网络信息: 调焦
	 */
//	void on_receive_focus(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_RECEIVE_ANNEX, 接收解析网络信息: 附属
	 */
	void on_receive_annex(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_CLIENT, 断开连接: 用户
	 */
	void on_close_client(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_MOUNT, 断开连接: 转台
	 */
	void on_close_mount(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_CAMERA, 断开连接: 相机
	 */
	void on_close_camera(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_FOCUS, 断开连接: 调焦
	 */
//	void on_close_focus(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_ANNEX, 断开连接: 附属
	 */
	void on_close_annex(const long addr, const long);
	/* 消息机制 */
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 观测系统 */
	/*!
	 * @brief 查找并创建观测系统
	 * @return
	 * 观测系统地址
	 * @note
	 * 当观测系统不存在时, 创建该系统. 否则返回已创建的指针
	 */
	ObsSysPtr find_obss(const string &gid, const string &uid, bool create = true);
	/*!
	 * @brief 显示停止哪个所有观测系统
	 */
	void stop_obss();
	/*!
	 * @brief 线程: 监测观测系统有效性
	 */
	void thread_obss();
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 观测计划 */
	/*!
	 * @brief 生成用于定标(本底,暗场,平场)的观测计划
	 */
	void obsplan_calibration();

public:
	/*!
	 * @brief 扫描计划存储目录, 查找并解析观测计划
	 * @return
	 * 观测计划目录扫描及解析结果
	 */
	bool scan_obsplan();
	/*!
	 * @brief 解析一个观测计划文件
	 * @param filepath 文件路径
	 * @return
	 * 文件解析结果
	 */
	bool resolve_obsplan(const char *filepath);
	/*!
	 * @brief 按照观测计划起始观测时间增量排序
	 */
	void resort_obsplan();
	/*!
	 * @brief 回调函数, 申请新的观测计划
	 */
	ObsPlanPtr acquire_new_plan(const string& gid, const string& uid);
	/*!
	 * @brief 线程: 检查观测计划
	 */
	void thread_obsplan();
	/*!
	 * @brief 线程: 计算当前时间对应的天文观测时段
	 */
	void thread_odt();
	/* 观测计划 */
//////////////////////////////////////////////////////////////////////////////

protected:
	/* 数据库 */
};

#endif /* GENERALCONTROL_H_ */
