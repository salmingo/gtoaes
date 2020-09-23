/*
 * @file msgque_base.h 基于message_queue封装消息队列
 * @date 2017-01-18
 * @version 0.2
 * @author Xiaomeng Lu
 * @note
 * 基于message_queue重新封装实现消息队列
 */

#ifndef MSGQUE_BASE_H_
#define MSGQUE_BASE_H_

#include <string>
#include <boost/signals2.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

// 声明msgque_base回调函数
typedef boost::signals2::signal<void (long, long)> mqb_cbfunc;
// 声明msgque_base回调函数插槽
typedef mqb_cbfunc::slot_type mqb_cbtype;

class msgque_base {
public:
	/* 声明数据类型 */
	enum msg_enum {// 定义消息
		MSG_QUIT,		//< 退出消息队列
		MSG_USER = 128	//< 用户消息起始地址
	};

private:
	/* 声明数据类型 */
	struct msg_unit {// 消息体单元
		long id, param1, param2;

	public:
		msg_unit() {
			id = param1 = param2 = 0;
		}

		/*!
		 * @brief 构造函数
		 * @param _id  消息标识
		 * @param _p1  参数1
		 * @param _p2  参数2
		 */
		msg_unit(const long _id, const long _p1 = 0, const long _p2 = 0) {
			id     = _id;
			param1 = _p1;
			param2 = _p2;
		}
	};

	typedef boost::interprocess::message_queue msgque;

	/* 声明成员变量 */
	std::string name_;							//< 消息队列名称
	boost::shared_ptr<msgque> queue_;			//< 消息队列
	boost::shared_ptr<boost::thread> thread_;	//< 线程
	boost::shared_array<mqb_cbfunc> slots_;		//< 消息回调函数

protected:
	/*!
	 * @brief 处理消息队列的线程主体
	 */
	void thread_body();

public:
	// 构造函数
	explicit msgque_base();
	// 析构函数
	virtual ~msgque_base();

public:
	/*!
	 * @brief 投递低优先级消息
	 * @param _msg  消息ID
	 * @param _p1   参数1
	 * @param _p2   参数2
	 */
	void post_message(const long _msg, const long _p1 = 0, const long _p2 = 0);
	/*!
	 * @brief 投递高优先级消息
	 * @param _msg  消息ID
	 * @param _p1   参数1
	 * @param _p2   参数2
	 */
	void send_message(const long _msg, const long _p1 = 0, const long _p2 = 0);
	/*!
	 * @brief 注册消息
	 * @param _msg  消息, 应不小于MSG_USER
	 * @param _slot 消息函数
	 */
	void register_message(const long _msg, const mqb_cbtype& _slot);

	/*!
	 * @brief 创建消息队列
	 * @param name 消息队列名称
	 * @return
	 */
	bool start(const char* name);
	/*!
	 * @brief 销毁消息队列
	 */
	void stop();
};

#endif /* MSGQUE_HPP_ */
