/*!
 * @file MessageQueue.h 声明文件, 基于boost::interprocess::ipc::message_queue封装消息队列
 * @version 0.2
 * @date 2017-10-02
 * - 优化消息队列实现方式
 * @date 2020-10-01
 * - 优化
 * - 面向gtoaes, 将GeneralControl和ObservationSystem的共同特征迁移至此处
 */

#ifndef SRC_MESSAGEQUEUE_H_
#define SRC_MESSAGEQUEUE_H_

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/thread/thread.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/signals2.hpp>
#include <string>
#include "AsioTCP.h"
#include "KvProtocol.h"
#include "NonkvProtocol.h"
#include "ObservationPlan.h"

/*!
 * @struct TcpReceived
 * @brief 网络事件
 */
struct TcpReceived {
	using Pointer = boost::shared_ptr<TcpReceived>;

	TcpCPtr client;	///< 网络连接
	int peer;	///< 主机类型

public:
	TcpReceived(TcpCPtr _client, int _peer) {
		client = _client;
		peer   = _peer;
	}

	static Pointer Create(TcpCPtr _client, int _peer) {
		return Pointer(new TcpReceived(_client, _peer));
	}
};
using TcpRcvPtr = TcpReceived::Pointer;
using TcpRcvQue = std::deque<TcpRcvPtr>;
using TcpCVec = std::vector<TcpCPtr> ; ///< 网络连接存储区

enum {// 对应主机类型
	PEER_CLIENT,		///< 客户端
	PEER_MOUNT,			///< GWAC望远镜
	PEER_CAMERA,		///< 相机
	PEER_MOUNT_ANNEX,	///< 镜盖+调焦+天窗(GWAC)
	PEER_CAMERA_ANNEX,	///< 温控+真空(GWAC-GY)
	PEER_LAST		///< 占位, 不使用
};

class MessageQueue {
protected:
	/* 数据类型 */
	struct Message {
		long id;			// 消息编号
		long par1, par2;	// 参数

	public:
		Message() {
			id = par1 = par2 = 0;
		}

		Message(long _id, long _par1 = 0, long _par2 = 0) {
			id   = _id;
			par1 = _par1;
			par2 = _par2;
		}
	};

	//////////////////////////////////////////////////////////////////////////////
	using CallbackFunc = boost::signals2::signal<void (const long, const long)>;	///< 消息回调函数
	using CBSlot = CallbackFunc::slot_type;	///< 回调函数插槽
	using CBArray = boost::shared_array<CallbackFunc>;	///< 回调函数数组
	using MQ = boost::interprocess::message_queue;	///< boost消息队列
	using MQPtr = boost::shared_ptr<MQ>;	///< boost消息队列指针
	using MtxLck = boost::unique_lock<boost::mutex>;	///< 信号灯互斥锁
	using ThreadPtr = boost::shared_ptr<boost::thread>;	///< boost线程指针

protected:
	/* 成员变量 */
	//////////////////////////////////////////////////////////////////////////////
	enum {
		MSG_QUIT = 0,	///< 结束消息队列
		MSG_TCP_RECEIVE,///< 收到TCP消息
		MSG_USER		///< 用户自定义消息起始编号
	};

	//////////////////////////////////////////////////////////////////////////////
	/* 消息队列 */
	const long szFunc_;	///< 自定义回调函数数组长度
	MQPtr mqptr_;		///< 消息队列
	CBArray funcs_;		///< 回调函数数组
	std::string errmsg_;///< 错误原因
	/* 网络通信 */
	TcpCVec tcpC_buff_;			///< 网络连接
	boost::mutex mtx_tcpC_buff_;///< 互斥锁: 网络连接

	TcpRcvQue que_tcpRcv_;		///< 网络事件队列
	boost::mutex mtx_tcpRcv_;	///< 互斥锁: 网络事件

	boost::shared_array<char> bufTcp_;	///< 网络信息存储区: 消息队列中调用
	KvProtoPtr kvProto_;		///< 键值对格式协议访问接口
	NonkvProtoPtr nonkvProto_;	///< 非键值对格式协议访问接口

	/* 多线程 */
	ThreadPtr thrd_msg_;		///< 消息响应线程
	ThreadPtr thrd_tcpClean_;	///< 线程: 释放已关闭的网络连接

	/* 观测计划 */
	ObsPlanPtr obsPlans_;		///< 观测计划集合
	boost::mutex mtx_obsPlans_;	///< 互斥锁: 观测计划集合

public:
	MessageQueue();
	virtual ~MessageQueue();
	/*!
	 * @brief 创建消息队列并启动监测/响应服务
	 * @param name 消息队列名称
	 * @return
	 * 操作结果. false代表失败
	 */
	bool Start(const char *name);
	/*!
	 * @brief 停止消息队列监测/响应服务, 并销毁消息队列
	 */
	virtual void Stop();
	/*!
	 * @brief 注册消息及其响应函数
	 * @param id   消息代码
	 * @param slot 回调函数插槽
	 * @return
	 * 消息注册结果. 若失败返回false
	 */
	bool RegisterMessage(const long id, const CBSlot& slot);
	/*!
	 * @brief 投递低优先级消息
	 * @param id   消息代码
	 * @param par1 参数1
	 * @param par2 参数2
	 */
	void PostMessage(const long id, const long par1 = 0, const long par2 = 0);
	/*!
	 * @brief 投递高优先级消息
	 * @param id   消息代码
	 * @param par1 参数1
	 * @param par2 参数2
	 */
	void SendMessage(const long id, const long par1 = 0, const long par2 = 0);
	/*!
	 * @brief 查看错误提示
	 * @return
	 * 错误提示
	 */
	const char *GetError();

protected:
	/* 消息响应函数 */
	/*!
	 * @brief 消息: 收到TCP信息
	 * @param par1  参数1, 保留
	 * @param par2  参数2, 保留
	 */
	void on_tcp_receive(const long par1 = 0, const long par2 = 0);

protected:
	/* TCP接收回调函数 */
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 * @param peer   远程主机类型
	 */
	void receive_from_peer(const TcpCPtr client, const error_code& ec, int peer);
	/*!
	 * @brief 解析与用户/数据库、通用望远镜、相机、制冷(GWAC)、真空(GWAC)相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	virtual void resolve_from_peer(const TcpCPtr client, int peer) = 0;

protected:
	/*!
	 * @brief 中止线程
	 * @param thrd 线程指针
	 */
	void interrupt_thread(ThreadPtr& thrd);
	/*!
	 * @brief 线程, 监测/响应消息
	 */
	void thread_message();
	/*!
	 * @brief 集中清理已断开的网络连接
	 */
	void thread_clean_tcp();
};

#endif /* SRC_MESSAGEQUEUE_H_ */
