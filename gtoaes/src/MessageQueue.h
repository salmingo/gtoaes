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
		MSG_USER		///< 用户自定义消息起始编号
	};

	//////////////////////////////////////////////////////////////////////////////
	/* 消息队列 */
	const long szFunc_;	///< 自定义回调函数数组长度
	MQPtr mqptr_;		///< 消息队列
	CBArray funcs_;		///< 回调函数数组
	std::string errmsg_;///< 错误原因

	/* 多线程 */
	ThreadPtr thrd_msg_;		///< 消息响应线程

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
	 * @brief 注册消息响应函数
	 */
	virtual void register_messages() = 0;

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
};

#endif /* SRC_MESSAGEQUEUE_H_ */
