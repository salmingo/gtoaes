/**
 * @file GeneralControl.h 声明文件, 封装总控服务
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include "globaldef.h"
#include "GLog.h"
#include "GeneralControl.h"

using namespace boost;
using namespace boost::posix_time;

GeneralControl::GeneralControl() {

}

GeneralControl::~GeneralControl() {

}

//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::Start() {
	param_.Load(gConfigPath);
	bufrcv_.reset(new char[TCP_PACK_SIZE]);
	ascproto_ = make_ascproto();
	register_message();

	string name = "msgque_";
	name += DAEMON_NAME;
	if (!MessageQueue::Start(name.c_str())) return false;
	if (!create_all_server()) return false;
	if (param_.ntpEnable) {
		ntp_ = make_ntp(param_.ntpHost.c_str(), 123, 1000);
		_gLog->SetNTP(ntp_);
		_gLogPlan->SetNTP(ntp_);
		thrd_clocksync_.reset(new boost::thread(boost::bind(&GeneralControl::thread_clocksync, this)));
	}
	if (param_.dbEnable) {

	}

	return true;
}

void GeneralControl::Stop() {
	MessageQueue::Stop();
	interrupt_thread(thrd_clocksync_);
}
//////////////////////////////////////////////////////////////////////////////
/*----------------- 网络服务 -----------------*/
int GeneralControl::create_server(TcpSPtr *server, const uint16_t port) {
	if (port == 0) return true;

	const TCPServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	*server = maketcp_server();
	(*server)->RegisterAccespt(slot);
	return (*server)->CreateServer(port);
}

bool GeneralControl::create_all_server() {
	int ec;

	if ((ec = create_server(&tcps_client_, param_.portClient))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for client on port<%d>. ErrorCode<%d>",
				param_.portClient, ec);
		return false;
	}
	if ((ec = create_server(&tcps_tele_, param_.portTele))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for telescope on port<%d>. ErrorCode<%d>",
				param_.portTele, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_, param_.portMount))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for mount on port<%d>. ErrorCode<%d>",
				param_.portMount, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_, param_.portCamera))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for camera on port<%d>. ErrorCode<%d>",
				param_.portCamera, ec);
		return false;
	}
	if ((ec = create_server(&tcps_tele_annex_, param_.portTeleAnnex))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for telescope-annex on port<%d>. ErrorCode<%d>",
				param_.portTeleAnnex, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_annex_, param_.portMountAnnex))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for mount-annex on port<%d>. ErrorCode<%d>",
				param_.portMountAnnex, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_annex_, param_.portCameraAnnex))) {
		_gLog->Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for camera-annex on port<%d>. ErrorCode<%d>",
				param_.portCameraAnnex, ec);
		return false;
	}

	return true;
}

void GeneralControl::network_accept(const TcpCPtr& client, const long server) {
	TCPServer* ptr = (TCPServer*) server;
	const TCPClient::CBSlot& slot;

	/* 不使用消息队列, 需要互斥 */
	if (ptr == tcps_client_.get()) {// 客户端
		mutex_lock lck(mtx_tcpc_client_);
		tcpc_client_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_client, this, _1, _2);
	}
	else if (ptr == tcps_tele_.get()) {// 通用望远镜
		mutex_lock lck(mtx_tcpc_tele_);
		tcpc_tele_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_telescope, this, _1, _2);
	}
	else if (ptr == tcps_mount_.get()) {// GWAC望远镜
		mutex_lock lck(mtx_tcpc_mount_);
		tcpc_mount_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_mount, this, _1, _2);
	}
	else if (ptr == tcps_camera_.get()) {// 相机
		mutex_lock lck(mtx_tcpc_camera_);
		tcpc_camera_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_camera, this, _1, _2);
	}
	else if (ptr == tcps_tele_annex_.get()) {// 通用镜盖+调焦+天窗
		mutex_lock lck(mtx_tcpc_tele_annex_);
		tcpc_tele_annex_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_telescope_annex, this, _1, _2);
	}
	else if (ptr == tcps_mount_annex_.get()) {// GWAC镜盖+调焦+天窗
		mutex_lock lck(mtx_tcpc_mount_annex_);
		tcpc_mount_annex_.push_back(client);
		slot = boost::bind(&GeneralControl::receive_telescope_annex, this, _1, _2);
	}
	else if (ptr == tcps_camera_annex_.get()) {// GWAC温控
		mutex_lock lck(mtx_tcpc_camera_annex_);
		tcpc_camera_annex_.push_back(client);
		client->UseBuffer();
		slot = boost::bind(&GeneralControl::receive_camera_annex, this, _1, _2);
	}
	client->UseBuffer();
	client->RegisterRead(slot);
}

void GeneralControl::receive_client(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CLIENT : MSG_RECEIVE_CLIENT, client);
}

void GeneralControl::receive_telescope(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_TELE : MSG_RECEIVE_TELE, client);
}

void GeneralControl::receive_mount(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT : MSG_RECEIVE_MOUNT, client);
}

void GeneralControl::receive_camera(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA : MSG_RECEIVE_CAMERA, client);
}

void GeneralControl::receive_telescope_annex(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_TELE_ANNEX : MSG_RECEIVE_TELE_ANNEX, client);
}

void GeneralControl::receive_mount_annex(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT_ANNEX : MSG_RECEIVE_MOUNT_ANNEX, client);
}

void GeneralControl::receive_camera_annex(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA_ANNEX : MSG_RECEIVE_CAMERA_ANNEX, client);
}

void GeneralControl::resolve_protocol_ascii(TCPClient* client, int peer) {

}

void GeneralControl::resolve_protocol_gwac(TCPClient* client, int peer) {

}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息机制 -----------------*/
void GeneralControl::register_message() {
	const CBSlot& slot11 = boost::bind(&GeneralControl::on_receive_client,          this, _1, _2);
	const CBSlot& slot12 = boost::bind(&GeneralControl::on_receive_telescope,       this, _1, _2);
	const CBSlot& slot13 = boost::bind(&GeneralControl::on_receive_mount,           this, _1, _2);
	const CBSlot& slot14 = boost::bind(&GeneralControl::on_receive_camera,          this, _1, _2);
	const CBSlot& slot15 = boost::bind(&GeneralControl::on_receive_telescope_annex, this, _1, _2);
	const CBSlot& slot16 = boost::bind(&GeneralControl::on_receive_mount_annex,     this, _1, _2);
	const CBSlot& slot17 = boost::bind(&GeneralControl::on_receive_camera_annex,    this, _1, _2);

	const CBSlot& slot21 = boost::bind(&GeneralControl::on_close_client,          this, _1, _2);
	const CBSlot& slot22 = boost::bind(&GeneralControl::on_close_telescope,       this, _1, _2);
	const CBSlot& slot23 = boost::bind(&GeneralControl::on_close_mount,           this, _1, _2);
	const CBSlot& slot24 = boost::bind(&GeneralControl::on_close_camera,          this, _1, _2);
	const CBSlot& slot25 = boost::bind(&GeneralControl::on_close_telescope_annex, this, _1, _2);
	const CBSlot& slot26 = boost::bind(&GeneralControl::on_close_mount_annex,     this, _1, _2);
	const CBSlot& slot27 = boost::bind(&GeneralControl::on_close_camera_annex,    this, _1, _2);

	const CBSlot& slot31 = boost::bind(&GeneralControl::on_acquire_plan,   this, _1, _2);

	RegisterMessage(MSG_RECEIVE_CLIENT,       slot11);
	RegisterMessage(MSG_RECEIVE_TELE,         slot12);
	RegisterMessage(MSG_RECEIVE_MOUNT,        slot13);
	RegisterMessage(MSG_RECEIVE_CAMERA,       slot14);
	RegisterMessage(MSG_RECEIVE_TELE_ANNEX,   slot15);
	RegisterMessage(MSG_RECEIVE_MOUNT_ANNEX,  slot16);
	RegisterMessage(MSG_RECEIVE_CAMERA_ANNEX, slot17);

	RegisterMessage(MSG_CLOSE_CLIENT,       slot21);
	RegisterMessage(MSG_CLOSE_TELE,         slot22);
	RegisterMessage(MSG_CLOSE_TELE,         slot23);
	RegisterMessage(MSG_CLOSE_CAMERA,       slot24);
	RegisterMessage(MSG_CLOSE_TELE_ANNEX,   slot25);
	RegisterMessage(MSG_CLOSE_MOUNT_ANNEX,  slot26);
	RegisterMessage(MSG_CLOSE_CAMERA_ANNEX, slot27);

	RegisterMessage(MSG_ACQUIRE_PLAN,       slot31);}

void GeneralControl::on_receive_client(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CLIENT);
}

void GeneralControl::on_receive_telescope(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_TELE);
}

void GeneralControl::on_receive_mount(const long client, const long ec) {
	resolve_protocol_gwac((TCPClient*) client, PEER_MOUNT);
}

void GeneralControl::on_receive_camera(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CAMERA);
}

void GeneralControl::on_receive_telescope_annex(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_TELE_ANNEX);
}

void GeneralControl::on_receive_mount_annex(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_MOUNT_ANNEX);
}

void GeneralControl::on_receive_camera_annex(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CAMERA_ANNEX);
}

void GeneralControl::on_close_client(const long client, const long ec) {
	/*
	 * 在观测系统中检测网络连接的有效性. 当网络已经断开时释放资源
	 */
	mutex_lock lck(mtx_tcpc_client_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_client_.end()) tcpc_client_.erase(it);
}

void GeneralControl::on_close_telescope(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_tele_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_tele_.begin(); it != tcpc_tele_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_tele_.end()) tcpc_tele_.erase(it);
}

void GeneralControl::on_close_mount(const long client, const long ec) {
	/*
	 * 在观测系统中检测网络连接的有效性. 当网络已经断开时释放资源
	 */
	mutex_lock lck(mtx_tcpc_mount_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_mount_.end()) tcpc_mount_.erase(it);
}

void GeneralControl::on_close_camera(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_camera_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_camera_.end()) tcpc_camera_.erase(it);
}

void GeneralControl::on_close_telescope_annex(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_tele_annex_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_tele_annex_.begin(); it != tcpc_tele_annex_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_tele_annex_.end()) tcpc_tele_annex_.erase(it);
}

void GeneralControl::on_close_mount_annex(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_mount_annex_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_mount_annex_.begin(); it != tcpc_mount_annex_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_mount_annex_.end()) tcpc_mount_annex_.erase(it);
}

void GeneralControl::on_close_camera_annex(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_camera_annex_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_camera_annex_.begin(); it != tcpc_camera_annex_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_camera_annex_.end()) tcpc_camera_annex_.erase(it);
}

void GeneralControl::on_acquire_plan(const long, const long) {
	AcquirePlanPtr acq;
	string gid, uid;
	{// 提取参数
		mutex_lock lck(mtx_acqplan_);
		acq = que_acqplan_.front();
		que_acqplan_.pop_front();
		gid = acq->gid;
		uid = acq->uid;
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 观测计划 -----------------*/
void GeneralControl::acquire_new_plan(const int type, const string& gid, const string& uid) {
	mutex_lock lck(mtx_acqplan_);
	AcquirePlanPtr acq = boost::make_shared<AcquirePlan>(type, gid, uid);
	que_acqplan_.push_back(acq);
	PostMessage(MSG_ACQUIRE_PLAN);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 多线程 -----------------*/
// 时钟同步
void GeneralControl::thread_clocksync() {
	this_thread::sleep_for(chrono::seconds(10));

	while (1) {
		ntp_->SynchClock();

		ptime now(second_clock::local_time());
		ptime noon(now.date(), hours(12));
		long secs = (noon - now).total_seconds();
		this_thread::sleep_for(chrono::seconds(secs < 10 ? secs + 86400 : secs));
	}
}
