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
	Stop();
}

bool MessageQueue::Start(const char *name) {
	if (thrd_msg_.unique()) return true;

	try {
		// 启动消息队列
		MQ::remove(name);
		mqptr_.reset(new MQ(create_only, name, szFunc_, sizeof(Message)));

		// 为TCP客户端接收解析构建工作环境
		bufTcp_.reset(new char[TCP_PACK_SIZE]);
		kvProto_    = KvProtocol::Create();
		nonkvProto_ = NonkvProtocol::Create();
		thrd_msg_.reset(new boost::thread(boost::bind(&MessageQueue::thread_message, this)));
		thrd_tcpClean_.reset(new boost::thread(boost::bind(&MessageQueue::thread_clean_tcp, this)));
		// 观测计划
		obsPlans_  = ObservationPlan::Create();

		return true;
	}
	catch(interprocess_exception &ex) {
		errmsg_ = ex.what();
		return false;
	}
}

void MessageQueue::Stop() {
	interrupt_thread(thrd_tcpClean_);
	if (thrd_msg_.unique()) {
		SendMessage(MSG_QUIT);
		thrd_msg_->join();
		thrd_msg_.reset();
	}
	for (TcpCVec::iterator it = tcpC_buff_.begin(); it != tcpC_buff_.end(); ++it) {
		if ((*it)->IsOpen()) (*it)->Close();
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

void MessageQueue::on_tcp_receive(const long par1, const long par2) {
	TcpRcvPtr rcvd;
	{// 取队首
		MtxLck lck(mtx_tcpRcv_);
		rcvd = que_tcpRcv_.front();
		que_tcpRcv_.pop_front();
	}

	TcpCPtr client = rcvd->client;
	const char term[] = "\n";	// 信息结束符: 换行
	int lenTerm = strlen(term);		// 结束符长度
	int pos;
	while (client->IsOpen() && (pos = client->Lookup(term, lenTerm)) >= 0) {
		client->Read(bufTcp_.get(), pos + lenTerm);
		bufTcp_[pos] = 0;
		resolve_from_peer(rcvd->client, rcvd->peer);
	}
}

void MessageQueue::receive_from_peer(const TcpCPtr client, const error_code& ec, int peer) {
	if (!ec) {
		MtxLck lck(mtx_tcpRcv_);
		TcpRcvPtr rcvd = TcpReceived::Create(client, peer);
		que_tcpRcv_.push_back(rcvd);
		PostMessage(MSG_TCP_RECEIVE);
	}
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
		if ((pos = msg.id - MSG_USER) >= 0) {
			if (pos < szFunc_) (funcs_[pos])(msg.par1, msg.par2);
		}
		else if (msg.id == MSG_TCP_RECEIVE)
			on_tcp_receive();
	} while(msg.id != MSG_QUIT);
}

void MessageQueue::thread_clean_tcp() {
	boost::chrono::seconds period(30);

	while (1) {
		boost::this_thread::sleep_for(period);

		MtxLck lck(mtx_tcpC_buff_);
		for (TcpCVec::iterator it = tcpC_buff_.begin(); it != tcpC_buff_.end(); ) {
			if ((*it)->IsOpen()) ++it;
			else it = tcpC_buff_.erase(it);
		}
	}
}
