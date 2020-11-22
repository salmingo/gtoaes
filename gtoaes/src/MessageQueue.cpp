/*!
 * @file MessageQueue.h 声明文件, 基于boost::interprocess::ipc::message_queue封装消息队列
 * @version 0.2
 * @date 2017-10-02
 * - 优化消息队列实现方式
 * @date 2020-10-01
 * - 优化
 */

#include <boost/bind/bind.hpp>
#include "MessageQueue.h"
#include "GLog.h"

using namespace boost::placeholders;
using namespace boost::interprocess;

MessageQueue::MessageQueue()
	: szFunc_ (128) {
	funcs_.reset(new CallbackFunc[szFunc_]);
}

MessageQueue::~MessageQueue() {
}

bool MessageQueue::Start(const char *name) {
	if (thrd_msg_.unique()) return true;

	try {
		// 启动消息队列
		MQ::remove(name);
		mqptr_.reset(new MQ(create_only, name, 1024, sizeof(Message)));
		register_messages();
		thrd_msg_.reset(new boost::thread(boost::bind(&MessageQueue::thread_message, this)));

		return true;
	}
	catch(interprocess_exception &ex) {
		errmsg_ = ex.what();
		return false;
	}
}

void MessageQueue::Stop() {
	if (thrd_msg_.unique()) {
		SendMessage(MSG_QUIT);
		thrd_msg_->join();
		thrd_msg_.reset();
	}
}

bool MessageQueue::RegisterMessage(const long id, const CBSlot& slot) {
	long pos(id - MSG_USER);
	bool rslt = pos >= 0 && pos < szFunc_;
	if (rslt) funcs_[pos].connect(slot);
	return rslt;
}

void MessageQueue::PostMessage(const long id, const long par1, const long par2) {
	if (mqptr_.unique()) {
		Message msg(id, par1, par2);
		mqptr_->send(&msg, sizeof(Message), 1);
	}
}

void MessageQueue::SendMessage(const long id, const long par1, const long par2) {
	if (mqptr_.unique()) {
		Message msg(id, par1, par2);
		mqptr_->send(&msg, sizeof(Message), 10);
	}
}

const char *MessageQueue::GetError() {
	return errmsg_.c_str();
}

void MessageQueue::interrupt_thread(ThreadPtr& thrd) {
	if (thrd.unique()) {
		thrd->interrupt();
		thrd->join();
		thrd.reset();
	}
}

void MessageQueue::thread_message() {
	Message msg;
	MQ::size_type szrcv;
	MQ::size_type szmsg = sizeof(Message);
	uint32_t priority;
	long pos;

	do {
		mqptr_->receive(&msg, szmsg, szrcv, priority);
		if ((pos = msg.id - MSG_USER) >= 0 && pos < szFunc_)
			(funcs_[pos])(msg.par1, msg.par2);
	} while(msg.id != MSG_QUIT);
}
