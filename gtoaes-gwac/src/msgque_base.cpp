/*
 * @file msgque_base.cpp 基于message_queue封装消息队列
 * @date 2017-01-18
 * @version 0.2
 * @author Xiaomeng Lu
 * @note
 * 基于message_queue重新封装实现消息队列
 */

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include "msgque_base.h"
#include "GLog.h"

msgque_base::msgque_base() {
	slots_.reset(new mqb_cbfunc[1024]);
}

msgque_base::~msgque_base() {
	stop();
}

void msgque_base::post_message(const long _msg, const long _p1, const long _p2) {
	if (queue_.unique()) {
		msg_unit one(_msg, _p1, _p2);
		queue_->send(&one, sizeof(msg_unit), 1);
	}
}

void msgque_base::send_message(const long _msg, const long _p1, const long _p2) {
	if (queue_.unique()) {
		msg_unit one(_msg, _p1, _p2);
		queue_->send(&one, sizeof(msg_unit), 10);
	}
}

void msgque_base::register_message(const long _msg, const mqb_cbtype& _slot) {
	long pos = _msg - MSG_USER;
	if (pos >= 0 && pos < 1024) slots_[pos].connect(_slot);
}

bool msgque_base::start(const char* name) {
	if (thread_.unique()) return true;

	try {
		name_ = name;
		msgque::remove(name);
		queue_.reset(new msgque(boost::interprocess::create_only, name, 1024, sizeof(msg_unit)));
		thread_.reset(new boost::thread(boost::bind(&msgque_base::thread_body, this)));

		return true;
	}
	catch(boost::interprocess::interprocess_exception& ex) {
		gLog.Write("msgque_base::start", LOG_FAULT, "%s", ex.what());
		return false;
	}
}

/*!
 * @brief 销毁消息队列
 */
void msgque_base::stop() {
	if (thread_.unique()) {
		send_message(MSG_QUIT);
		thread_->join();
		thread_.reset();
	}
	if (queue_.unique()) {
		msgque::remove(name_.c_str());
		queue_.reset();
	}
}

void msgque_base::thread_body() {
	msg_unit msg;
	msgque::size_type recvd_size;
	msgque::size_type msg_size = sizeof(msg_unit);
	unsigned int priority;
	long pos;

	do {
		queue_->receive((void*) &msg, msg_size, recvd_size, priority);
		if (msg.id >= MSG_USER) {
			pos = msg.id - MSG_USER;
			if (pos >= 0 && pos < 1024) (slots_[pos])(msg.param1, msg.param2);
		}
	}while(msg.id != MSG_QUIT);
}
