/*
 * @file GeneralControl.cpp 软件总控入口定义文件
 * @author 卢晓猛
 * @date 2017-1-19
 * @version 0.3
 *
 * @date 2017-05-06
 * @version 0.4
 * @date 2017-05-07
 * @li 缺: 对pair_id的特殊处理
 */
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include "GeneralControl.h"
#include "GLog.h"
#include "globaldef.h"

using namespace std;
using namespace boost::posix_time;
using namespace boost::placeholders;

GeneralControl::GeneralControl(boost::asio::io_service& ios_main) {// 构造函数
	iosmain_ = &ios_main;
}

GeneralControl::~GeneralControl() {// 析构函数
}

bool GeneralControl::Start() {// 尝试启动总控服务
	param_ = boost::make_shared<param_config>();
	param_->LoadFile(gConfigPath);
	bufrcv_.reset(new char[1500]);
	ascproto_ = boost::make_shared<ascii_proto>();
	mntproto_ = boost::make_shared<mount_proto>();
	register_messages();
	string name = "msgque_";
	name += DAEMON_NAME;
	if (!start(name.c_str())) return false;
	if (!StartServers()) return false;
	if (param_->enableNTP) {
		ntp_ = boost::make_shared<NTPClient>(param_->hostNTP.c_str(), 123, param_->maxDiffNTP);
		ntp_->EnableAutoSynch(true);
	}
	thrd_net_.reset(new boost::thread(boost::bind(&GeneralControl::ThreadNetwork, this)));

	return true;
}

void GeneralControl::Stop() {// 终止总控服务
	if (thrd_net_.unique()) {
		thrd_net_->interrupt();
		thrd_net_->join();
		thrd_net_.reset();
	}
	stop();
}

bool GeneralControl::StartServers() {
	if (!StartServer(&tcps_client_, param_->portClient)) {
		gLog.Write("StartServers", LOG_FAULT, "Failed to start server for client on <%d>", param_->portClient);
		return false;
	}
	if (!StartServer(&tcps_db_, param_->portDB)) {
		gLog.Write("StartServers", LOG_FAULT, "Failed to start server for database on <%d>", param_->portDB);
		return false;
	}
	if (!StartServer(&tcps_mount_, param_->portMount)) {
		gLog.Write("StartServers", LOG_FAULT, "Failed to start server for mount on <%d>", param_->portMount);
		return false;
	}
	if (!StartServer(&tcps_camera_, param_->portCamera)) {
		gLog.Write("StartServers", LOG_FAULT, "Failed to start server for camera on <%d>", param_->portCamera);
		return false;
	}
	if (!StartServer(&tcps_mountannex_, param_->portMountAnnex)) {
		gLog.Write("StartServers", LOG_FAULT, "Failed to start server for mount-annex on <%d>", param_->portMountAnnex);
		return false;
	}

	return true;
}

bool GeneralControl::StartServer(tcpsptr *server, int port) {
	const tcps_cbtype& slot = boost::bind(&GeneralControl::NetworkAccept, this, _1, _2);
	*server = boost::make_shared<tcp_server>();
	(*server)->register_accept(slot);
	return (*server)->start(port);
}

obssptr GeneralControl::find_os(const string& gid, const string& uid) {
	mutex_lock lock(mutex_obss_);
	obssptr obss;
	obssvec::iterator it;

	for (it = obss_.begin(); it != obss_.end() && !(*it)->is_matched(gid, uid); ++it);
	if (it == obss_.end()) {
		obss = make_obss(gid, uid);
		if (obss.unique()) {
			// 设置地理位置
			double lgt, lat, alt;
			string name;
			param_->GetGeosite(gid, name, lgt, lat, alt);
			obss->set_geosite(name, lgt, lat, alt);
			obss_.push_back(obss);
		}
	}
	else obss = *it;

	return obss;
}
///////////////////////////////////////////////////////////////////////////////
/*---------------------------------- 回调函数 ----------------------------------*/
// 处理收到的网络连接请求
void GeneralControl::NetworkAccept(const tcpcptr& client, const long param) {
	const tcp_server *server = (const tcp_server*) param;
	if (server == tcps_client_.get()) {// 收到客户端网络连接请求
		mutex_lock lock(mutex_client_);
		const tcpc_cbtype &slot = boost::bind(&GeneralControl::ReceiveClient, this, _1, _2);
		tcpc_client_.push_back(client);
		client->register_receive(slot);
	}
	else if (server == tcps_db_.get()) {// 收到数据库网络连接请求
		mutex_lock lock(mutex_db_);
		const tcpc_cbtype &slot = boost::bind(&GeneralControl::ReceiveDatabase, this, _1, _2);
		tcpc_db_.push_back(client);
		client->register_receive(slot);
	}
	else if (server == tcps_mount_.get()) {// 收到转台网络连接请求
		mutex_lock lock(mutex_mount_);
		const tcpc_cbtype &slot = boost::bind(&GeneralControl::ReceiveMount, this, _1, _2);
		tcpc_mount_.push_back(client);
		client->register_receive(slot);
	}
	else if (server == tcps_camera_.get()) {// 收到相机网络连接请求
		mutex_lock lock(mutex_camera_);
		const tcpc_cbtype &slot = boost::bind(&GeneralControl::ReceiveCamera, this, _1, _2);
		tcpc_camera_.push_back(client);
		client->register_receive(slot);
	}
	else if (server == tcps_mountannex_.get()) {// 收到转台附属设备网络连接请求
		mutex_lock lock(mutex_mountannex_);
		const tcpc_cbtype &slot = boost::bind(&GeneralControl::ReceiveMountAnnex, this, _1, _2);
		tcpc_mountannex_.push_back(client);
		client->register_receive(slot);
	}
}

// 处理来自客户端的网络信息
void GeneralControl::ReceiveClient(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_CLIENT : MSG_CLOSE_CLIENT, client);
}

// 处理来自数据库的网络信息
void GeneralControl::ReceiveDatabase(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_DATABASE : MSG_CLOSE_DATABASE, client);
}

// 处理来自转台的网络信息
void GeneralControl::ReceiveMount(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_MOUNT : MSG_CLOSE_MOUNT, client);
}

// 处理来自相机的网络信息
void GeneralControl::ReceiveCamera(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_CAMERA : MSG_CLOSE_CAMERA, client);
}

void GeneralControl::ReceiveMountAnnex(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_MOUNTANNEX : MSG_CLOSE_MOUNTANNEX, client);
}

///////////////////////////////////////////////////////////////////////////////
/*-------------------------------- 消息响应函数 --------------------------------*/
// 注册消息响应函数
void GeneralControl::register_messages() {
	const mqb_cbtype &slot1 = boost::bind(&GeneralControl::OnReceiveClient,     this, _1, _2);
	const mqb_cbtype &slot2 = boost::bind(&GeneralControl::OnReceiveDatabase,   this, _1, _2);
	const mqb_cbtype &slot3 = boost::bind(&GeneralControl::OnReceiveMount,      this, _1, _2);
	const mqb_cbtype &slot4 = boost::bind(&GeneralControl::OnReceiveCamera,     this, _1, _2);
	const mqb_cbtype &slot5 = boost::bind(&GeneralControl::OnReceiveMountAnnex, this, _1, _2);

	const mqb_cbtype &slot6  = boost::bind(&GeneralControl::OnCloseClient,     this, _1, _2);
	const mqb_cbtype &slot7  = boost::bind(&GeneralControl::OnCloseDatabase,   this, _1, _2);
	const mqb_cbtype &slot8  = boost::bind(&GeneralControl::OnCloseMount,      this, _1, _2);
	const mqb_cbtype &slot9  = boost::bind(&GeneralControl::OnCloseCamera,     this, _1, _2);
	const mqb_cbtype &slot10 = boost::bind(&GeneralControl::OnCloseMountAnnex, this, _1, _2);

	register_message(MSG_RECEIVE_CLIENT,      slot1);
	register_message(MSG_RECEIVE_DATABASE,    slot2);
	register_message(MSG_RECEIVE_MOUNT,       slot3);
	register_message(MSG_RECEIVE_CAMERA,      slot4);
	register_message(MSG_RECEIVE_MOUNTANNEX,  slot5);

	register_message(MSG_CLOSE_CLIENT,      slot6);
	register_message(MSG_CLOSE_DATABASE,    slot7);
	register_message(MSG_CLOSE_MOUNT,       slot8);
	register_message(MSG_CLOSE_CAMERA,      slot9);
	register_message(MSG_CLOSE_MOUNTANNEX,  slot10);
}

// 处理来自客户端的网络信息
void GeneralControl::OnReceiveClient  (long param1, long param2) {
	ResolveAscProtocol((tcp_client*) param1, PEER_CLIENT);
}

// 处理来自数据库的网络信息
void GeneralControl::OnReceiveDatabase(long param1, long param2) {
	ResolveAscProtocol((tcp_client*) param1, PEER_DATABASE);
}

// 处理来自转台的网络信息
void GeneralControl::OnReceiveMount   (long param1, long param2) {
	ResolveMountProtocol((tcp_client*) param1);
}

// 处理来自相机的网络信息
void GeneralControl::OnReceiveCamera  (long param1, long param2) {
	ResolveAscProtocol((tcp_client*) param1, PEER_CAMERA);
}

void GeneralControl::OnReceiveMountAnnex  (long param1, long param2) {
	ResolveMountProtocol((tcp_client*) param1);
}

// 处理客户端的网络断开
void GeneralControl::OnCloseClient    (long param1, long param2) {
	mutex_lock lock(mutex_client_);
	tcp_client *client = (tcp_client *) param1;
	tcpcvec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && client != (*it).get(); ++it);
	if (it != tcpc_client_.end()) tcpc_client_.erase(it);
}

// 处理数据库的网络断开
void GeneralControl::OnCloseDatabase  (long param1, long param2) {
	mutex_lock lock(mutex_db_);
	tcp_client *client = (tcp_client *) param1;
	tcpcvec::iterator it;

	for (it = tcpc_db_.begin(); it != tcpc_db_.end() && client != (*it).get(); ++it);
	if (it != tcpc_db_.end()) tcpc_db_.erase(it);
}

// 处理转台的网络断开
void GeneralControl::OnCloseMount     (long param1, long param2) {
	mutex_lock lock(mutex_mount_);
	tcp_client *client = (tcp_client *) param1;
	tcpcvec::iterator it;

	for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && client != (*it).get(); ++it);
	if (it != tcpc_mount_.end()) {
		// 取消与观测系统的关联关系
		mutex_lock lck2(mutex_obss_);
		for (obssvec::iterator j = obss_.begin(); j != obss_.end(); ++j) (*j)->DecoupleMount(*it);
		tcpc_mount_.erase(it);
	}
}

// 处理相机的网络断开
void GeneralControl::OnCloseCamera    (long param1, long param2) {
	mutex_lock lock(mutex_camera_);
	tcp_client *client = (tcp_client *) param1;
	tcpcvec::iterator it;

	for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && client != (*it).get(); ++it);
	if (it != tcpc_camera_.end()) tcpc_camera_.erase(it);
	else {// 已关联观测系统. 在关联观测系统响应函数前网络已断开. Mar 26, 2017
		mutex_lock lck2(mutex_obss_);
		for (obssvec::iterator j = obss_.begin(); j != obss_.end() && !(*j)->DecoupleCamera(param1); ++j);
	}
}

// 处理转台附属设备的网络断开
void GeneralControl::OnCloseMountAnnex     (long param1, long param2) {
	mutex_lock lock(mutex_mountannex_);
	tcp_client *client = (tcp_client *) param1;
	tcpcvec::iterator it;

	for (it = tcpc_mountannex_.begin(); it != tcpc_mountannex_.end() && client != (*it).get(); ++it);
	if (it != tcpc_mountannex_.end()) {
		// 取消与观测系统的关联关系
		mutex_lock lck2(mutex_obss_);
		for (obssvec::iterator j = obss_.begin(); j != obss_.end(); ++j) (*j)->DecoupleMountAnnex(*it);
		tcpc_mountannex_.erase(it);
	}
}

void GeneralControl::ResolveAscProtocol(tcp_client* client, int type) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	string proto_type;
	apbase proto_body;

	while (client->is_open() && (pos = client->lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_BUFF_SIZE) {/* 有效性判定. 原因: 遗漏换行符作为协议结束标记; 高概率性丢包 */
			gLog.Write("ResolveASCIIProtocol", LOG_FAULT, "%d: protocol length is over than threshold", type);
			client->close();
		}
		else {/* 读取协议内容 */
			client->read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;
//			if (type == PEER_CLIENT || type == PEER_DATABASE) gLog.Write("Received<%s>", bufrcv_.get());
			proto_body = ascproto_->resolve(bufrcv_.get(), proto_type);
			if (proto_body.use_count()) {
				if      (type == PEER_CLIENT || type == PEER_DATABASE) ProcessClientProtocol(proto_type, proto_body);
				else if (type == PEER_CAMERA) ProcessCameraProtocol(client, proto_type, proto_body);
			}
		}
	}
}

// 处理控制台客户端通信协议
void GeneralControl::ProcessClientProtocol(string& proto_type, apbase& proto_body) {
	using namespace boost;
	/* 提取group_id和unit_id */
	string gid = proto_body->group_id;
	string uid  = proto_body->unit_id;

	mutex_lock lck(mutex_obss_);
	for (obssvec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
		if ((*it)->is_matched(gid, uid)) {/* 处理协议 */
			if (iequals(proto_type, "append_gwac")) {// append_gwac需要特殊处理
				boost::shared_ptr<ascproto_append_gwac> proto = static_pointer_cast<ascproto_append_gwac>(proto_body);
				if (proto->pair_id <= 0) (*it)->notify_append_gwac(proto_body);
				else {// ... 当需要多观测系统配对工作时, 需要特殊处理

				}
			}
			else if (iequals(proto_type, "focus"))       (*it)->notify_focus(proto_body);
			else if (iequals(proto_type, "fwhm"))        (*it)->notify_fwhm(proto_body);
			else if (iequals(proto_type, "guide"))       (*it)->notify_guide(proto_body);
			else if (iequals(proto_type, "take_image"))  (*it)->notify_take_image(proto_body);
			else if (iequals(proto_type, "abort_slew"))  (*it)->notify_abort_slew();
			else if (iequals(proto_type, "slewto"))      (*it)->notify_slewto(proto_body);
			else if (iequals(proto_type, "abort_image")) (*it)->notify_abort_image(proto_body);
			else if (iequals(proto_type, "home_sync"))   (*it)->notify_home_sync(proto_body);
			else if (iequals(proto_type, "start_gwac"))  (*it)->notify_start_gwac();
			else if (iequals(proto_type, "stop_gwac"))   (*it)->notify_stop_gwac();
			else if (iequals(proto_type, "find_home"))   (*it)->notify_find_home();
			else if (iequals(proto_type, "park"))        (*it)->notify_park();
			else if (iequals(proto_type, "mcover"))      (*it)->notify_mcover(proto_body);
			else if (iequals(proto_type, "enable")) {

			}
			else if (iequals(proto_type, "disable")) {

			}
		}
	}
}

// 处理相机客户端通信协议
void GeneralControl::ProcessCameraProtocol(tcp_client* client, string& proto_type, apbase& proto_body) {
	// 来自相机的信息只有一种, 即camera_info
	boost::shared_ptr<ascproto_camera_info> proto = boost::static_pointer_cast<ascproto_camera_info>(proto_body);
	string gid = proto->group_id;
	string uid = proto->unit_id;
	string cid = proto->camera_id;

	if (gid.empty() || uid.empty() || cid.empty()) {
		gLog.Write(NULL, LOG_FAULT, "Camera provides null Identification: <%s:%s:%s>", gid.c_str(), uid.c_str(), cid.c_str());
		client->close();
	}
	else {
		obssptr obss = find_os(gid, uid);
		// 处理观测系统定位结果
		if (!obss.use_count()) {
			gLog.Write(NULL, LOG_FAULT, "LOGICAL ERROR!!! None Observation System found for camera");
			client->close();
		}
		else {// 关联相机与观测系统
			// 定位相机网络资源
			mutex_lock lock(mutex_camera_);
			tcpcvec::iterator it;
			for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && (*it).get() != client; ++it);
			if (it == tcpc_camera_.end()) {
				gLog.Write(NULL, LOG_FAULT, "LOGICAL ERROR!!! tcp_client pointer associated with camera is wild");
				client->close();
			}
			else if (obss->CoupleCamera(*it, gid, uid, cid)) tcpc_camera_.erase(it);	// ObservationSystem接管网络连接
			else {
				gLog.Write(NULL, LOG_FAULT, "Failed to relate camera with Observation System");
				client->close();
			}
		}
	}
}

// 解析转台相关通信协议
void GeneralControl::ResolveMountProtocol(tcp_client* client) {
	if (!client->is_open()) return;

	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	mpbase proto_body;
	string proto_type;

	while (client->is_open() && (pos = client->lookup(term, len)) >= 0) {
		/* 有效性判定 */
		if ((toread = pos + len) > TCP_BUFF_SIZE) {// 原因: 遗漏换行符作为协议结束标记; 高概率性丢包
			gLog.Write("ResolveMountProtocol", LOG_FAULT, "protocol length from mount is over than threshold");
			client->close();
		}
		else {
			/* 读取协议内容 */
			client->read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;
			/* 解析协议 */
			proto_type = mntproto_->resolve(bufrcv_.get(), proto_body);
			if (!proto_type.empty()) ProcessMountProtocol(client, proto_type, proto_body);
			else {
				gLog.Write("ResolveMountProtocol", LOG_FAULT, "Protocol does not match with mount");
				client->close();
			}
		}
	}
}

// 处理转台通信协议
void GeneralControl::ProcessMountProtocol(tcp_client* client, string& proto_type, mpbase& proto_body) {
	/*!
	 * @brief 投递消息到指定观测系统
	 */
	string gid = proto_body->group_id;
	string uid  = proto_body->unit_id;
	obssptr obss;

	/*!
	 * ready和state一次发送所有转台状态, 需要创建\维护unit_id
	 * unit_id编码约定:
	 * - 从1开始
	 * - 以3字节字符串表示
	 */
	if (boost::iequals(proto_type, "ready")) {// 转台是否完成准备
/*
		boost::shared_ptr<mntproto_ready> proto = boost::static_pointer_cast<mntproto_ready>(proto_body);
		boost::format fmt;
		for (int i = 0; i < proto->n; ++i) {
			if (proto->ready[i] >= 0) {
				fmt = boost::format("%03d") % (i + 1);
				uid = fmt.str();
				obss = find_os(gid, uid);
				if (obss.use_count()) {
					if (proto->ready[i] == 1) obss->CoupleMount(cliptr);
					else                      obss->DecoupleMount(cliptr);
				}
			}
		}
*/
	}
	else if (boost::iequals(proto_type, "state")) {// 转台实时工作状态
/*
		boost::shared_ptr<mntproto_status> proto = boost::static_pointer_cast<mntproto_status>(proto_body);
		boost::format fmt;
		for (int i = 0; i < proto->n; ++i) {
			if (proto->state[i] >= 0) {
				fmt = boost::format("%03d") % (i + 1);
				uid = fmt.str();
				obss = find_os(gid, uid);
				if (obss.use_count()) {
					obss->CoupleMount(cliptr);
					obss->notify_mount_state(proto->state[i]);
				}
			}
		}
*/
	}
	else {
		obss = find_os(gid, uid);
		if (!obss.use_count()) {
			gLog.Write(NULL, LOG_FAULT, "LOGICAL ERROR!!! None Observation System found for mount");
			client->close();
		}
		else {// 投递消息到指定观测系统
			bool b1, b2;
			if ((b1 = boost::iequals(proto_type, "utc")) || (b2 = boost::iequals(proto_type, "position"))) {
				/*!
				 * @brief 按照转台控制软件结构设计, 同一group_id的所有转台采用唯一转台控制软件
				 */
				tcpcptr cliptr;
				if (tcpc_mount_.size()) {
					mutex_lock lck(mutex_mount_);
					tcpcvec::iterator it;
					for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && (*it).get() != client; ++it);
					cliptr = *it;
				}

				obss->CoupleMount(cliptr);
				if (b1) obss->notify_mount_utc(boost::static_pointer_cast<mntproto_utc>(proto_body));
				else    obss->notify_mount_position(boost::static_pointer_cast<mntproto_position>(proto_body));
			}
			else if ((b1 = boost::iequals(proto_type, "focus")) || (b2 = boost::iequals(proto_type, "mcover"))) {
				tcpcptr cliptr;
				if (tcpc_mountannex_.size()) {
					mutex_lock lck(mutex_mountannex_);
					tcpcvec::iterator it;
					for (it = tcpc_mountannex_.begin(); it != tcpc_mountannex_.end() && (*it).get() != client; ++it);
					cliptr = *it;
				}

				obss->CoupleMountAnnex(cliptr);
				if (b1) obss->notify_mount_focus(boost::static_pointer_cast<mntproto_focus>(proto_body));
				else    obss->notify_mount_mcover(boost::static_pointer_cast<mntproto_mcover>(proto_body));
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/*-------------------------------- 线程函数 --------------------------------*/
void GeneralControl::ThreadNetwork() {
	boost::chrono::seconds period(10);
	int count0 = period.count(), count1, t;
	tcpcvec::iterator it;

	while(1) {
		boost::this_thread::sleep_for(period);
		t = second_clock::universal_time().time_of_day().total_seconds();

		if (tcpc_client_.size()) {// 检测客户端网络连接
			mutex_lock lock(mutex_client_);
			for (it = tcpc_client_.begin(); it != tcpc_client_.end(); ++it) {
				if ((count1 = t - (*it)->get_timeflag()) < 0) count1 += 86400;
				if (count1 > count0) (*it)->close();
			}
		}

		if (tcpc_db_.size()) {// 检测数据库网络连接
			mutex_lock lock(mutex_db_);
			for (it = tcpc_db_.begin(); it != tcpc_db_.end(); ++it) {
				if ((count1 = t - (*it)->get_timeflag()) < 0) count1 += 86400;
				if (count1 > count0) (*it)->close();
			}
		}

		if (tcpc_mount_.size()) {// 检测转台网络连接
			mutex_lock lock(mutex_mount_);
			for (it = tcpc_mount_.begin(); it != tcpc_mount_.end(); ++it) {
				if ((count1 = t - (*it)->get_timeflag()) < 0) count1 += 86400;
				if (count1 > count0) (*it)->close();
			}
		}

		if (tcpc_camera_.size()) {// 检测相机网络连接
			mutex_lock lock(mutex_camera_);
			for (it = tcpc_camera_.begin(); it != tcpc_camera_.end(); ++it) {
				if ((count1 = t - (*it)->get_timeflag()) < 0) count1 += 86400;
				if (count1 > count0) (*it)->close();
			}
		}

		if (tcpc_mountannex_.size()) {// 检测转台附属设备网络连接
			mutex_lock lock(mutex_mountannex_);
			for (it = tcpc_mountannex_.begin(); it != tcpc_mountannex_.end(); ++it) {
				if ((count1 = t - (*it)->get_timeflag()) < 0) count1 += 86400;
				if (count1 > count0) (*it)->close();
			}
		}
	}
}
