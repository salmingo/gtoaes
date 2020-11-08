/**
 * @file GeneralControl.h 声明文件, 封装总控服务
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include "globaldef.h"
#include "GLog.h"
#include "GeneralControl.h"

using namespace boost;
using namespace boost::posix_time;
using namespace boost::placeholders;

GeneralControl::GeneralControl() {
}

GeneralControl::~GeneralControl() {

}

//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::Start() {
	param_.Load(gConfigPath);
	bufrcv_.reset(new char[TCP_PACK_SIZE]);
	ascproto_ = AsciiProtocol::Create();
	obs_plans_ = ObservationPlan::Create();

	if (!create_all_server()) return false;
	if (param_.ntpEnable) {
		ntp_ = NTPClient::Create(param_.ntpHost.c_str(), 123, param_.ntpDiffMax);
		thrd_clocksync_.reset(new boost::thread(boost::bind(&GeneralControl::thread_clocksync, this)));
	}
	if (param_.dbEnable) {

	}

	return true;
}

void GeneralControl::Stop() {
	interrupt_thread(thrd_netevent_);
	interrupt_thread(thrd_clocksync_);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 网络服务 -----------------*/
int GeneralControl::create_server(TcpSPtr *server, const uint16_t port) {
	if (port == 0) return true;

	const TcpServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	*server = TcpServer::Create();
	(*server)->RegisterAccept(slot);
	return (*server)->CreateServer(port);
}

bool GeneralControl::create_all_server() {
	int ec;

	if ((ec = create_server(&tcps_client_, param_.portClient))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_, param_.portMount))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_, param_.portCamera))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_annex_, param_.portMountAnnex))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_annex_, param_.portCameraAnnex))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}

	thrd_netevent_.reset(new boost::thread(boost::bind(&GeneralControl::thread_network_event, this)));
	return true;
}

void GeneralControl::network_accept(const TcpCPtr client, const TcpSPtr server) {
	/* 不使用消息队列, 需要互斥 */
	if (server == tcps_client_) {// 客户端
		MtxLck lck(mtx_tcpc_client_);
		const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_client, this, _1, _2);
		client->RegisterRead(slot);
		tcpc_client_.push_back(client);
	}
	else if (server == tcps_mount_) {// 转台
		MtxLck lck(mtx_tcpc_mount_);
		const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_mount, this, _1, _2);
		client->RegisterRead(slot);
		tcpc_mount_.push_back(client);
	}
	else if (server == tcps_camera_) {// 相机
		MtxLck lck(mtx_tcpc_camera_);
		const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_camera, this, _1, _2);
		client->RegisterRead(slot);
		tcpc_camera_.push_back(client);
	}
	else if (server == tcps_mount_annex_) {// 转台附属
		MtxLck lck(mtx_tcpc_mount_annex_);
		const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_mount_annex, this, _1, _2);
		client->RegisterRead(slot);
		tcpc_mount_annex_.push_back(client);
	}
	else if (server == tcps_camera_annex_) {// 相机附属
		MtxLck lck(mtx_tcpc_camera_annex_);
		const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_camera_annex, this, _1, _2);
		client->RegisterRead(slot);
		tcpc_camera_annex_.push_back(client);
	}
}

void GeneralControl::receive_client(const TcpCPtr client, const int ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = network_event::Create(PEER_CLIENT, client, ec);
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_mount(const TcpCPtr client, const int ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = network_event::Create(PEER_MOUNT, client, ec);
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_camera(const TcpCPtr client, const int ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = network_event::Create(PEER_CAMERA, client, ec);
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_mount_annex(const TcpCPtr client, const int ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = network_event::Create(PEER_MOUNT_ANNEX, client, ec);
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_camera_annex(const TcpCPtr client, const int ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = network_event::Create(PEER_CAMERA_ANNEX, client, ec);
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息机制 -----------------*/
void GeneralControl::on_receive_client(const TcpCPtr client) {
	resolve_protocol_ascii(client, PEER_CLIENT);
}

void GeneralControl::on_receive_mount(const TcpCPtr client) {
	//...区别处理keywor=value格式和紧凑格式
	resolve_protocol_ascii(client, PEER_MOUNT);
}

void GeneralControl::on_receive_camera(const TcpCPtr client) {
	resolve_protocol_ascii(client, PEER_CAMERA);
}

void GeneralControl::on_receive_mount_annex(const TcpCPtr client) {
	//...区别处理keywor=value格式和紧凑格式
	resolve_protocol_ascii(client, PEER_MOUNT_ANNEX);
}

void GeneralControl::on_receive_camera_annex(const TcpCPtr client) {
	resolve_protocol_ascii(client, PEER_CAMERA_ANNEX);
}

void GeneralControl::on_close_client(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_client_);
	TcpCVec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && client != *it; ++it);
	if (it != tcpc_client_.end()) tcpc_client_.erase(it);
}

void GeneralControl::on_close_mount(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_mount_);
	TcpCVec::iterator it;

	for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && client != *it; ++it);
	if (it != tcpc_mount_.end()) tcpc_mount_.erase(it);
}

void GeneralControl::on_close_camera(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_camera_);
	TcpCVec::iterator it;

	for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && client != *it; ++it);
	if (it != tcpc_camera_.end()) tcpc_camera_.erase(it);
}

void GeneralControl::on_close_mount_annex(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_mount_annex_);
	TcpCVec::iterator it;

	for (it = tcpc_mount_annex_.begin(); it != tcpc_mount_annex_.end() && client != *it; ++it);
	if (it != tcpc_mount_annex_.end()) tcpc_mount_annex_.erase(it);
}

void GeneralControl::on_close_camera_annex(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_camera_annex_);
	TcpCVec::iterator it;

	for (it = tcpc_camera_annex_.begin(); it != tcpc_camera_annex_.end() && client != *it; ++it);
	if (it != tcpc_camera_annex_.end()) tcpc_camera_annex_.erase(it);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 解析/执行通信协议 -----------------*/
void GeneralControl::resolve_protocol_ascii(const TcpCPtr client, int peer) {
	const char term[] = "\n";	// 换行符作为信息结束标记
	const char prefix[] = "g#";	// 其它格式的引导符
	int len = strlen(term);		// 结束符长度
	int pos, toread;
	apbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii()",
					"too long message from %s", peer == PEER_CLIENT ? "CLIENT"
							: (peer == PEER_MOUNT ? "MOUNT" : "CAMERA"));
			client->Close();
		}
		else {
			client->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;
			if (strstr(bufrcv_.get(), prefix)) {

			}
			else {

			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 观测计划 -----------------*/

//////////////////////////////////////////////////////////////////////////////
/*----------------- 多线程 -----------------*/
void GeneralControl::interrupt_thread(ThreadPtr& thrd) {
	if (thrd.unique()) {
		thrd->interrupt();
		thrd->join();
		thrd.reset();
	}
}

/* 处理网络事件 */
void GeneralControl::thread_network_event() {
	boost::mutex mtx;
	MtxLck lck(mtx);

	while (1) {
		cv_netev_.wait(lck);

		while (queNetEv_.size()) {
			NetEvPtr netEvPtr;
			{// 减少队列互斥锁时间
				MtxLck lck_net(mtx_netev_);
				netEvPtr = queNetEv_.front();
				queNetEv_.pop_front();
			}
			switch(netEvPtr->peer) {
			case PEER_MOUNT:
				if (netEvPtr->type) {
					on_close_mount(netEvPtr->tcpc);
				}
				else if (netEvPtr->tcpc->IsOpen()) {
					on_receive_mount(netEvPtr->tcpc);
				}
				break;
			case PEER_CAMERA:
				if (netEvPtr->type) {
					on_close_camera(netEvPtr->tcpc);
				}
				else if (netEvPtr->tcpc->IsOpen()) {
					on_receive_camera(netEvPtr->tcpc);
				}
				break;
			case PEER_MOUNT_ANNEX:
				if (netEvPtr->type) {
					on_close_mount_annex(netEvPtr->tcpc);
				}
				else if (netEvPtr->tcpc->IsOpen()) {
					on_receive_mount_annex(netEvPtr->tcpc);
				}
				break;
			case PEER_CAMERA_ANNEX:
				if (netEvPtr->type) {
					on_close_camera_annex(netEvPtr->tcpc);
				}
				else if (netEvPtr->tcpc->IsOpen()) {
					on_receive_camera_annex(netEvPtr->tcpc);
				}
				break;
			default:
				if (netEvPtr->type) {
					on_close_client(netEvPtr->tcpc);
				}
				else if (netEvPtr->tcpc->IsOpen()) {
					on_receive_client(netEvPtr->tcpc);
				}
				break;
			}
		}
	}
}

// 时钟同步
void GeneralControl::thread_clocksync() {
	boost::chrono::hours period(1);
	this_thread::sleep_for(chrono::seconds(10));

	while (1) {
		ntp_->SynchClock();
		this_thread::sleep_for(period);
	}
}
