/*
 * @file GeneralControl.cpp 定义文件, 封装总控服务
 * @version 0.3
 * @date 2017-10-02
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include "GeneralControl.h"
#include "globaldef.h"
#include "GLog.h"

using namespace boost;
using namespace boost::posix_time;

GeneralControl::GeneralControl() {
	param_ = boost::make_shared<param_config>();
	param_->LoadFile(gConfigPath);
	bufrcv_.reset(new char[TCP_PACK_SIZE]);
	ascproto_ = make_ascproto();
	mntproto_ = make_mount();
}

GeneralControl::~GeneralControl() {
}

bool GeneralControl::StartService() {
	// 启动消息队列和网络服务
	register_message();
	std::string name = "msgque_";
	name += DAEMON_NAME;
	if (!Start(name.c_str())) return false;
	if (!create_all_server()) return false;
	// 启动NTP时钟
	if (param_->enableNTP) {
		ntp_ = make_ntp(param_->hostNTP.c_str(), 123, param_->maxDiffNTP);
		ntp_->EnableAutoSynch(true);
	}
	// 其它初始化
	if (param_->enableDB && !param_->urlDB.empty()) {
		db_.reset(new DataTransfer(param_->urlDB.c_str()));
		thrd_status_.reset(new boost::thread(boost::bind(&GeneralControl::thread_status, this)));
	}
	thrd_monitor_obss_.reset(new boost::thread(boost::bind(&GeneralControl::thread_monitor_obss, this)));
	thrd_monitor_plan_.reset(new boost::thread(boost::bind(&GeneralControl::thread_monitor_plan, this)));

	return true;
}

void GeneralControl::StopService() {
	interrupt_thread(thrd_monitor_plan_);
	interrupt_thread(thrd_monitor_obss_);
	interrupt_thread(thrd_status_);
	exit_ignore_plan();
	exit_close_obss(obss_gwac_);
	exit_close_obss(obss_normal_);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 网络服务与消息 -----------------*/
int GeneralControl::create_server(TcpSPtr *server, const uint16_t port) {
	const TCPServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	*server = maketcp_server();
	(*server)->RegisterAccespt(slot);
	return (*server)->CreateServer(port);
}

bool GeneralControl::create_all_server() {
	int ec;

	if ((ec = create_server(&tcps_client_, param_->portClient))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for client on port<%d>. ErrorCode<%d>",
				param_->portClient, ec);
		return false;
	}
	if ((ec = create_server(&tcps_tele_, param_->portTele))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for telescope on port<%d>. ErrorCode<%d>",
				param_->portTele, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_, param_->portMount))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for mount on port<%d>. ErrorCode<%d>",
				param_->portMount, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_, param_->portCamera))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for camera on port<%d>. ErrorCode<%d>",
				param_->portCamera, ec);
		return false;
	}
	if ((ec = create_server(&tcps_mount_annex_, param_->portMountAnnex))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for mount-annex on port<%d>. ErrorCode<%d>",
				param_->portMountAnnex, ec);
		return false;
	}
	if ((ec = create_server(&tcps_camera_annex_, param_->portCameraAnnex))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for camera-annex on port<%d>. ErrorCode<%d>",
				param_->portCameraAnnex, ec);
		return false;
	}

	return true;
}

void GeneralControl::network_accept(const TcpCPtr& client, const long server) {
	TCPServer* ptr = (TCPServer*) server;

	/* 不使用消息队列, 需要互斥 */
	if (ptr == tcps_client_.get()) {// 客户端
		mutex_lock lck(mtx_tcpc_client_);
		tcpc_client_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_client, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_tele_.get()) {// 通用望远镜
		mutex_lock lck(mtx_tcpc_tele_);
		tcpc_tele_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_telescope, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_mount_.get()) {// GWAC转台
		mutex_lock lck(mtx_tcpc_mount_);
		tcpc_mount_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_mount, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_camera_.get()) {// 相机
		mutex_lock lck(mtx_tcpc_camera_);
		tcpc_camera_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_camera, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_mount_annex_.get()) {// GWAC镜盖+调焦
		mutex_lock lck(mtx_tcpc_mount_annex_);
		tcpc_mount_annex_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_mount_annex, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_camera_annex_.get()) {// GWAC温控
		mutex_lock lck(mtx_tcpc_camera_annex_);
		tcpc_camera_annex_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_camera_annex, this, _1, _2);
		client->RegisterRead(slot);
	}
}

void GeneralControl::receive_client(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CLIENT : MSG_RECEIVE_CLIENT, client);
}

void GeneralControl::receive_telescope(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_TELESCOPE : MSG_RECEIVE_TELESCOPE, client);
}

void GeneralControl::receive_mount(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT : MSG_RECEIVE_MOUNT, client);
}

void GeneralControl::receive_camera(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA : MSG_RECEIVE_CAMERA, client);
}

void GeneralControl::receive_mount_annex(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT_ANNEX : MSG_RECEIVE_MOUNT_ANNEX, client);
}

void GeneralControl::receive_camera_annex(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA_ANNEX : MSG_RECEIVE_CAMERA_ANNEX, client);
}

void GeneralControl::resolve_protocol_ascii(TCPClient* client, PEER_TYPE peer) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			string ip = client->GetSocket().remote_endpoint().address().to_string();
			_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii",
					"too long message from IP<%s>. peer type is %s", ip.c_str(),
					peer == PEER_CLIENT ? "CLIENT"
							: (peer == PEER_TELESCOPE ? "TELESCOPE"
									: (peer == PEER_CAMERA ? "CAMERA" : "CAMERA-ANNEX")));
			client->Close();
		}
		else {// 读取协议内容并解析执行
			client->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;

			proto = ascproto_->Resolve(bufrcv_.get());
			// 检查: 协议有效性及设备标志基本有效性
			if (!proto.use_count()
					|| (!proto->uid.empty() && proto->gid.empty())
					|| (!proto->cid.empty() && (proto->gid.empty() || proto->uid.empty()))) {
				_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii",
						"illegal protocol. received: %s", bufrcv_.get());
				client->Close();
			}
			else if (peer == PEER_CLIENT)       process_protocol_client       (proto, client);
			else if (peer == PEER_TELESCOPE)    process_protocol_telescope    (proto, client);
			else if (peer == PEER_CAMERA)       process_protocol_camera       (proto, client);
			else if (peer == PEER_CAMERA_ANNEX) process_protocol_camera_annex (proto, client);
		}
	}
}

void GeneralControl::resolve_protocol_mount(TCPClient* client, PEER_TYPE peer) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	mpbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			string ip = client->GetSocket().remote_endpoint().address().to_string();
			_gLog.Write(LOG_FAULT, "GeneralControl::resolve_protocol_mount",
					"too long message from IP<%s>. peer type is %s", ip.c_str(),
					peer == PEER_MOUNT ? "MOUNT" : "MOUNT-ANNEX");
			client->Close();
		}
		else {
			/* 读取协议内容 */
			client->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;

			proto = mntproto_->Resolve(bufrcv_.get());
			if (!proto.use_count()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::resolve_protocol_mount",
						"illegal protocol. received: %s", bufrcv_.get());
				client->Close();
			}
			else if (peer == PEER_MOUNT)       process_protocol_mount       (proto, client);
			else if (peer == PEER_MOUNT_ANNEX) process_protocol_mount_annex (proto, client);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 处理收到的网络信息 -----------------*/
/*
 * 通信协议分为两类:
 * - 需要在GeneralControl中做特殊处理
 * - 依据group_id:unit_id投递给观测系统
 */
void GeneralControl::process_protocol_client(apbase proto, TCPClient* client) {
	string type = proto->type;

	if      (iequals(type, APTYPE_APPPLAN)) process_protocol_append_plan (from_apbase<ascii_proto_append_plan> (proto));
	else if (iequals(type, APTYPE_APPGWAC)) process_protocol_append_gwac (from_apbase<ascii_proto_append_plan> (proto));
	else if (iequals(type, APTYPE_CHKPLAN)) process_protocol_check_plan  (from_apbase<ascii_proto_check_plan>  (proto), client);
	else if (iequals(type, APTYPE_ABTPLAN)) process_protocol_abort_plan (from_apbase<ascii_proto_abort_plan> (proto));
	else if (iequals(type, APTYPE_REG))     process_protocol_register    (from_apbase<ascii_proto_reg>         (proto), client);
	else if (iequals(type, APTYPE_UNREG))   process_protocol_unregister  (from_apbase<ascii_proto_unreg>       (proto), client);
	else if (iequals(type, APTYPE_RELOAD))  process_protocol_reload();
	else if (iequals(type, APTYPE_REBOOT))  process_protocol_reboot();
	else {// 可直接投递协议
		string gid = proto->gid;
		string uid = proto->uid;

		if (obss_gwac_.size()) {// 尝试向GWAC类型观测系统投递协议
			mutex_lock lck(mtx_obss_gwac_);
			for (ObsSysVec::iterator it = obss_gwac_.begin(); it != obss_gwac_.end(); ++it) {
				if ((*it)->IsMatched(gid, uid) >= 0) (*it)->NotifyProtocol(proto);
			}
		}

		if (obss_normal_.size()) {// 尝试向通用型观测系统中投递协议
			mutex_lock lck(mtx_obss_normal_);
			for (ObsSysVec::iterator it = obss_normal_.begin(); it != obss_normal_.end(); ++it) {
				if ((*it)->IsMatched(gid, uid) >= 0) (*it)->NotifyProtocol(proto);
			}
		}
	}
}

/*
 * @note
 * 通用望远镜通信协议. 对于通用望远镜, 在GeneralControl中仅且接受register协议, 从而建立观测系统与网络连接的关联性
 */
void GeneralControl::process_protocol_telescope(apbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;

	if (!iequals(proto->type, APTYPE_REG)) {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_telescope",
				"received <%s> but just accept <%s> from <%s:%s>",
				proto->type.c_str(), APTYPE_REG, gid.c_str(), uid.c_str());
		client->Close();
	}
	else {// 关联观测系统与望远镜
		ObsSysPtr obss = find_obss(gid, uid, OST_NORMAL);
		if (obss.use_count()) {
			mutex_lock lck(mtx_tcpc_tele_);
			TcpCVec::iterator it;
			TcpCPtr ptr;

			for (it = tcpc_tele_.begin(); it != tcpc_tele_.end() && (*it).get() != client; ++it);
			ptr = *it;
			if (!obss->CoupleTelescope(ptr)) client->Close();
			else tcpc_tele_.erase(it);
		}
		else client->Close();
	}
}

/*
 * void process_protocol_mount() 处理GWAC转台通信协议
 */
void GeneralControl::process_protocol_mount(mpbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;

	ObsSysGWACPtr obss = from_obss<ObservationSystemGWAC>(find_obss(gid, uid, OST_GWAC));
	if (obss.use_count()) {
		string type = proto->type;
		TcpCPtr cliptr;
		{// 在缓冲区中查找对应的指针
			mutex_lock lck(mtx_tcpc_mount_);
			TcpCVec::iterator it;
			for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && client != (*it).get(); ++it);
			cliptr = *it;
		}
		// GWAC转台系统中, 多个转台复用一条网络连接, 因此需要由GeneralControl管理网络连接资源
		obss->CoupleTelescope(cliptr);
		// 依据协议类型处理通信协议
		if      (iequals(type, MPTYPE_POSITION)) obss->NotifyPosition (from_mpbase<mntproto_position> (proto));
		else if (iequals(type, MPTYPE_UTC))      obss->NotifyUtc      (from_mpbase<mntproto_utc>      (proto));
		else if (iequals(type, MPTYPE_READY))    obss->NotifyReady    (from_mpbase<mntproto_ready>    (proto));
		else if (iequals(type, MPTYPE_STATE))    obss->NotifyState    (from_mpbase<mntproto_state>    (proto));
	}
	else client->Close();
}

/*
 * @note
 * 相机通信协议. 对于相机, 在GeneralControl中仅且接受register协议, 从而建立观测系统与网络连接的关联性
 */
void GeneralControl::process_protocol_camera(apbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;
	string cid = proto->cid;

	if (!iequals(proto->type, APTYPE_REG) || cid.empty()) {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_camera",
				"received <%s> but just accept <%s> from <%s:%s:%s>", proto->type.c_str(),
				APTYPE_REG, gid.c_str(), uid.c_str(), cid.c_str());
		client->Close();
	}
	else {// 关联观测系统与望远镜
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss.use_count()) {
			mutex_lock lck(mtx_tcpc_camera_);
			TcpCVec::iterator it;
			TcpCPtr ptr;

			for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && (*it).get() != client; ++it);
			ptr = *it;
			if (!obss->CoupleCamera(ptr, cid)) client->Close();
			else tcpc_camera_.erase(it);
		}
		else client->Close();
	}
}

/*
 * void process_protocol_mount_annex() 处理GWAC调焦+镜盖通信协议
 */
void GeneralControl::process_protocol_mount_annex(mpbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;

	ObsSysGWACPtr obss = from_obss<ObservationSystemGWAC>(find_obss(gid, uid, OST_GWAC));
	if (obss.use_count()) {
		string type = proto->type;
		TcpCPtr cliptr;
		{// 在缓冲区中查找对应的指针
			mutex_lock lck(mtx_tcpc_mount_annex_);
			TcpCVec::iterator it;
			for (it = tcpc_mount_annex_.begin(); it != tcpc_mount_annex_.end() && client != (*it).get(); ++it);
			cliptr = *it;
		}
		// GWAC转台系统中, 多个转台复用一条网络连接, 因此需要由GeneralControl管理网络连接资源
		obss->CoupleMountAnnex(cliptr);
		// 依据协议类型处理通信协议
		if      (iequals(type, MPTYPE_FOCUS))  obss->NotifyFocus  (from_mpbase<mntproto_focus> (proto));
		else if (iequals(type, MPTYPE_MCOVER)) obss->NotifyMCover (from_mpbase<mntproto_mcover>(proto));
	}
	else client->Close();
}

/*
 * @note
 * 适用于GWAC定制相机, 处理单独温控单元测量的温度数据.
 * 后随两条操作:
 * @li 将温控数据发送给数据库
 * @li 将温控数据传递给对应观测系统, 由观测系统发送给相机控制机, 然后写入FITS头
 */
void GeneralControl::process_protocol_camera_annex(apbase proto, TCPClient* client) {
	apcooler cooler = from_apbase<ascii_proto_cooler>(proto);
	string gid = cooler->gid;
	string uid = cooler->uid;
	string cid = cooler->cid;

	if (!cid.empty()) {
		// 向观测系统传递信息
		ObsSysGWACPtr obss = from_obss<ObservationSystemGWAC>(find_obss(gid, uid, OST_GWAC));
		if (obss.use_count()) obss->NotifyCooler(cooler);
		else client->Close();
	}
	else client->Close();
}

/*
 * 通用望远镜观测计划.
 * - 区分imgtype类型处理:
 *  @li bias, dark, flat, focus: 分配给匹配gid:uid的所有在线系统,
 *      为各系统复制观测计划
 *  @li object: 存储观测计划; 若系统在线则判断计划是否可执行
 */
void GeneralControl::process_protocol_append_plan(apappplan plan) {
	string imgtype = plan->imgtype;
	string gid = plan->gid;
	string uid = plan->uid;
	bool empty = gid.empty() || uid.empty();

	correct_time(plan);		// 修正计划起止时间
	write_plan_log(plan);	// 日志中记录接收到的观测计划

	if (iequals(imgtype, "bias") || iequals(imgtype, "dark")
				|| iequals(imgtype, "flat") || iequals(imgtype, "focus")) {
		/*
		 * - 复制观测计划
		 * - 分配给匹配的在线系统
		 * - 由计划调度策略触发执行, 即并非立即执行
		 */
		if (!empty) catalogue_new_plan(plan);
		else if (obss_normal_.size()) {
			mutex_lock lck(mtx_obss_normal_);
			int x(0);	// 系统匹配结果
			for (ObsSysVec::iterator it = obss_normal_.begin(); it != obss_normal_.end() && x != 1; ++it) {
				if ((x = (*it)->IsMatched(gid, uid)) >= 0) {
					(*it)->GetID(plan->gid, plan->uid);
					catalogue_new_plan(ascproto_->CopyAppendPlan(plan));
				}
			}
		}
	}
	else if (iequals(imgtype, "object")) {
		/*
		 * - object图像类型计划, 最多仅允许被一个系统执行
		 * - 查找观测系统, 可以立即执行计划
		 */
		// 存储观测计划
		ExObsPlanPtr explan = catalogue_new_plan(plan);
		// 查找观测系统
		if (obss_normal_.size()) {
			mutex_lock lck(mtx_obss_normal_);
			ptime now = second_clock::universal_time();
			int max_priority(0), x(0), relative;
			ObsSysPtr obss;
			for (ObsSysVec::iterator it = obss_normal_.begin(); it != obss_normal_.end() && x != 1; ++it) {
				if ((x = (*it)->IsMatched(gid, uid)) >= 0
						&& (relative = (*it)->PlanRelativePriority(plan, now)) > max_priority) {
					max_priority = relative;
					obss = *it;
				}
			}
			// 通知系统准备执行计划
			if (obss.use_count()) {
				explan->AssignObservationSystem(obss);
				obss->NotifyPlan(to_obsplan(explan));
			}
		}
	}
}

/*
 * GWAC专用工作计划
 * - 区分imgtype类型处理:
 *  @li bias, dark, flat, focus: 分配给匹配gid:uid的所有在线系统,
 *      为各系统复制观测计划
 *  @li object: 不接受通配符, 区分pair_id处理
 * - 区分pair_id处理:
 *  @li <   0: 不需要配对观测
 *  @li >=0 0: 需要配对观测
 */
void GeneralControl::process_protocol_append_gwac(apappplan plan) {
	string imgtype = plan->imgtype;
	string gid     = plan->gid;
	string uid     = plan->uid;
	bool empty = gid.empty() || uid.empty();

	correct_time(plan);	// 修正计划起止时间
	write_plan_log(plan);	// 日志中记录接收到的观测计划

	if (iequals(imgtype, "bias") || iequals(imgtype, "dark")
				|| iequals(imgtype, "flat") || iequals(imgtype, "focus")) {
		/*
		 * - 复制观测计划
		 * - 分配给匹配的在线系统
		 * - 由计划调度策略触发执行, 即并非立即执行
		 */
		if (!empty) catalogue_new_plan(plan);
		else if (obss_gwac_.size()) {
			mutex_lock lck(mtx_obss_gwac_);
			int x(0);	// 系统匹配结果
			for (ObsSysVec::iterator it = obss_gwac_.begin(); it != obss_gwac_.end() && x != 1; ++it) {
				if ((x = (*it)->IsMatched(gid, uid)) >= 0) {
					(*it)->GetID(plan->gid, plan->uid);
					catalogue_new_plan(ascproto_->CopyAppendPlan(plan));
				}
			}
		}
	}
	else if (!empty && iequals(imgtype, "object")) {
		/* GWAC系统不接受object图像类型时, gid:uid为通配符 */
		int pair_id = plan->pair_id;
		/* 非配对计划 */
		if (pair_id < 0) {
			mutex_lock lck(mtx_obss_gwac_);
			ptime now = second_clock::universal_time();
			ObsSysVec::iterator it;
			for (it = obss_gwac_.begin(); it != obss_gwac_.end() && (*it)->IsMatched(gid, uid) != 1; ++it);
			if (it != obss_gwac_.end() && (*it)->PlanRelativePriority(plan, now) > 0) {
				/* 计划满足执行条件 */
				ExObsPlanPtr explan = catalogue_new_plan(plan);
				explan->AssignObservationSystem(*it);
				(*it)->NotifyPlan(to_obsplan(explan));
			}
			else {
				/* 抛弃该计划, GWAC抛弃object图像类型计划的条件:
				 * - 系统不在线
				 * - 不满足系统可用状态、时间、位置保护、优先级等条件
				 */
				_gLog.Write(LOG_WARN, NULL,
						"plan<%d> for obss<%s:%s> was thrown for unsatisfied executable conditions",
						plan->plan_sn, gid.c_str(), uid.c_str());
			}
		}
		/* 配对计划 */
		else {
			PairPlanVec::iterator it;
			for (it = plan_pair_.begin(); it != plan_pair_.end() && (*it)->pair_id != pair_id; ++it);
			if (it == plan_pair_.end()) {// 新的配对需求
				plan_pair_.push_back(plan);
			}
			else {// 存在两条件计划具有相同pair_id: it指向第一条计划, plan指向第二条计划
				ObsSysPtr obss1, obss2;
				apappplan plan1 = *it;
				apappplan plan2 = plan;
				string gid1 = plan1->gid;
				string uid1 = plan1->uid;
				bool fail(true);

				plan_pair_.erase(it);
				/* 定位与计划对应的观测系统 */
				if (!(gid == gid1 && uid == uid1) && obss_gwac_.size()) {
					mutex_lock lck(mtx_obss_gwac_);
					ptime now = second_clock::universal_time();
					ObsSysVec::iterator it1;
					for (it1 = obss_gwac_.begin(); it1 != obss_gwac_.end() && (*it1)->IsMatched(gid, uid) != 1; ++it1);
					if (it1 != obss_gwac_.end() && (*it1)->PlanRelativePriority(plan2, now) > 0) {
						obss2 = *it1;	// 对应第二条计划
						// 对应第一条计划
						for (it1 = obss_gwac_.begin(); it1 != obss_gwac_.end() && (*it1)->IsMatched(gid1, uid1) != 1; ++it1);
						if (it1 != obss_gwac_.end() && (*it1)->PlanRelativePriority(plan1, now) > 0) {
							obss1 = *it1;
							fail  = false;
						}
					}
				}
				/* 立即投递执行观测计划 */
				if (!fail) {
					string tmbegin = to_iso_extended_string(second_clock::universal_time() + minutes(2));
					plan1->begin_time = tmbegin;
					plan2->begin_time = tmbegin;

					ExObsPlanPtr explan1 = catalogue_new_plan(plan1);
					ExObsPlanPtr explan2 = catalogue_new_plan(plan2);

					explan1->AssignObservationSystem(obss1);
					explan2->AssignObservationSystem(obss2);
					obss1->NotifyPlan(to_obsplan(explan1));
					obss2->NotifyPlan(to_obsplan(explan2));
				}
				else {
					_gLog.Write(LOG_WARN, NULL,
							"pair<%d> plans<%d and %d> were both thrown for unsatisfied executable conditions",
							pair_id, plan1->plan_sn, plan2->plan_sn);
				}
			}
		}
	}
	else {
		_gLog.Write(LOG_WARN, NULL, "plan<%d> was thrown for wrong parameters", plan->plan_sn);
	}
}

void GeneralControl::process_protocol_check_plan(apchkplan proto, TCPClient* client) {
	applan plan = boost::make_shared<ascii_proto_plan>();
	int plan_sn(proto->plan_sn);
	bool found(false);
	const char *output;
	int n;

	plan->plan_sn = plan_sn;
	if (plans_.size()) {
		mutex_lock lck(mtx_plans_);
		int state;

		/* 遍历观测计划, 向客户端发送指定计划的工作状态 */
		for (ExObsPlanVec::iterator it = plans_.begin(); it != plans_.end() && client->IsOpen(); ++it) {
			if (plan_sn < 0 || (*it)->plan->plan_sn == plan_sn) {
				if (!found) found = true;
				// 构建并发送计划状态
				if (plan_sn < 0) *plan = *((*it)->plan);
				state = plan->state = (*it)->state;
				output = ascproto_->CompactPlan(plan, n);
				client->Write(output, n);
			}
		}
	}
	if (!found && client->IsOpen()) {
		output = ascproto_->CompactPlan(plan, n);
		client->Write(output, n);
	}
}

/*
 * 删除观测计划
 * - 修改计划工作状态为"删除", 由其它线程完成删除操作
 * - 若计划正在执行, 则通知观测系统中止观测流程. 当计划中止后, 观测系统逻辑修改计划状态
 */
void GeneralControl::process_protocol_abort_plan(apabtplan proto) {
	int plan_sn(proto->plan_sn);
	int state;

	if (plans_.size()) {
		mutex_lock lck(mtx_plans_);
		for (ExObsPlanVec::iterator it = plans_.begin(); it != plans_.end(); ++it) {
			if ((state = (*it)->state) < OBSPLAN_OVER // 未完成观测流程. 已完成计划不需要删除
					&& (plan_sn < 0 || plan_sn == (*it)->plan->plan_sn)) {
				if (state <= OBSPLAN_INT) (*it)->state = OBSPLAN_DELETE;
				else (*it)->obss->NotifyProtocol(to_apbase(proto));
			}
		}
	}
}

/*
 * 注册关联客户端与观测系统
 */
void GeneralControl::process_protocol_register(apreg proto, TCPClient* client) {
	TcpCPtr cliptr;
	string gid = proto->gid;
	string uid = proto->uid;

	{// 定位于client对应的TcpCPtr型指针
		mutex_lock lck(mtx_tcpc_client_);
		TcpCVec::iterator it;
		for (it = tcpc_client_.begin(); it != tcpc_client_.end() && (*it).get() != client; ++it);
		cliptr = *it;
	}

	if (obss_gwac_.size()) {// 尝试在GWAC类型观测系统中注册客户端
		mutex_lock lck(mtx_obss_gwac_);
		ObsSysVec::iterator it;
		for (it = obss_gwac_.begin(); it != obss_gwac_.end(); ++it) {
			if ((*it)->IsMatched(gid, uid) >= 0) (*it)->CoupleClient(cliptr);
		}
	}

	if (obss_normal_.size()) {// 尝试在通用型观测系统中注册客户端
		mutex_lock lck(mtx_obss_normal_);
		ObsSysVec::iterator it;
		for (it = obss_normal_.begin(); it != obss_normal_.end(); ++it) {
			if ((*it)->IsMatched(gid, uid) >= 0) (*it)->CoupleClient(cliptr);
		}
	}
}

void GeneralControl::process_protocol_unregister(apunreg proto, TCPClient* client) {
	TcpCPtr cliptr;
	string gid = proto->gid;
	string uid = proto->uid;
	int x;

	{// 定位于client对应的TcpCPtr型指针
		mutex_lock lck(mtx_tcpc_client_);
		TcpCVec::iterator it;
		for (it = tcpc_client_.begin(); it != tcpc_client_.end() && (*it).get() != client; ++it);
		cliptr = *it;
	}

	if (obss_gwac_.size()) {// 尝试在GWAC类型观测系统中注销客户端
		mutex_lock lck(mtx_obss_gwac_);
		ObsSysVec::iterator it;
		for (it = obss_gwac_.begin(); it != obss_gwac_.end(); ++it) {
			if ((x = (*it)->IsMatched(gid, uid)) >= 0) (*it)->DecoupleClient(cliptr);
			if (x == 1) break;
		}
	}

	if (obss_normal_.size()) {// 尝试在通用型观测系统中注销客户端
		mutex_lock lck(mtx_obss_normal_);
		ObsSysVec::iterator it;
		for (it = obss_normal_.begin(); it != obss_normal_.end(); ++it) {
			if ((x = (*it)->IsMatched(gid, uid)) >= 0) (*it)->DecoupleClient(cliptr);
			if (x == 1) break;
		}
	}
}

// 重新加载参数
void GeneralControl::process_protocol_reload() {
	// 忽略, gtoaes暂不实现该功能
}

// 重新启动软件
void GeneralControl::process_protocol_reboot() {
	// 忽略, gtoaes不需要实现该功能
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息响应函数 -----------------*/
void GeneralControl::register_message() {
	const CBSlot& slot11 = boost::bind(&GeneralControl::on_receive_client,       this, _1, _2);
	const CBSlot& slot12 = boost::bind(&GeneralControl::on_receive_telescope,    this, _1, _2);
	const CBSlot& slot13 = boost::bind(&GeneralControl::on_receive_mount,        this, _1, _2);
	const CBSlot& slot14 = boost::bind(&GeneralControl::on_receive_camera,       this, _1, _2);
	const CBSlot& slot15 = boost::bind(&GeneralControl::on_receive_mount_annex,  this, _1, _2);
	const CBSlot& slot16 = boost::bind(&GeneralControl::on_receive_camera_annex, this, _1, _2);

	const CBSlot& slot21 = boost::bind(&GeneralControl::on_close_client,       this, _1, _2);
	const CBSlot& slot22 = boost::bind(&GeneralControl::on_close_telescope,    this, _1, _2);
	const CBSlot& slot23 = boost::bind(&GeneralControl::on_close_mount,        this, _1, _2);
	const CBSlot& slot24 = boost::bind(&GeneralControl::on_close_camera,       this, _1, _2);
	const CBSlot& slot25 = boost::bind(&GeneralControl::on_close_mount_annex,  this, _1, _2);
	const CBSlot& slot26 = boost::bind(&GeneralControl::on_close_camera_annex, this, _1, _2);

	const CBSlot& slot31 = boost::bind(&GeneralControl::on_acquire_plan,   this, _1, _2);

	RegisterMessage(MSG_RECEIVE_CLIENT,        slot11);
	RegisterMessage(MSG_RECEIVE_TELESCOPE,     slot12);
	RegisterMessage(MSG_RECEIVE_MOUNT,         slot13);
	RegisterMessage(MSG_RECEIVE_CAMERA,        slot14);
	RegisterMessage(MSG_RECEIVE_MOUNT_ANNEX,   slot15);
	RegisterMessage(MSG_RECEIVE_CAMERA_ANNEX,  slot16);

	RegisterMessage(MSG_CLOSE_CLIENT,       slot21);
	RegisterMessage(MSG_CLOSE_TELESCOPE,    slot22);
	RegisterMessage(MSG_CLOSE_MOUNT,        slot23);
	RegisterMessage(MSG_CLOSE_CAMERA,       slot24);
	RegisterMessage(MSG_CLOSE_MOUNT_ANNEX,  slot25);
	RegisterMessage(MSG_CLOSE_CAMERA_ANNEX, slot26);

	RegisterMessage(MSG_ACQUIRE_PLAN,       slot31);
}

void GeneralControl::on_receive_client(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CLIENT);
}

void GeneralControl::on_receive_telescope(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_TELESCOPE);
}

void GeneralControl::on_receive_camera(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CAMERA);
}

void GeneralControl::on_receive_mount(const long client, const long ec) {
	resolve_protocol_mount((TCPClient*) client, PEER_MOUNT);
}

void GeneralControl::on_receive_mount_annex(const long client, const long ec) {
	resolve_protocol_mount((TCPClient*) client, PEER_MOUNT_ANNEX);
}

void GeneralControl::on_receive_camera_annex(const long client, const long ec) {
	resolve_protocol_ascii((TCPClient*) client, PEER_CAMERA_ANNEX);
}

void GeneralControl::on_close_client(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_client_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_client_.end()) {
		// 遍历GWAC观测系统
		if ((*it).use_count() > 1) {
			mutex_lock lck(mtx_obss_gwac_);
			for (ObsSysVec::iterator it1 = obss_gwac_.begin(); it1 != obss_gwac_.end(); ++it1) {
				(*it1)->DecoupleClient(*it);
			}
		}
		// 遍历通用望远镜系统
		if ((*it).use_count() > 1) {
			mutex_lock lck(mtx_obss_normal_);
			for (ObsSysVec::iterator it1 = obss_normal_.begin(); it1 != obss_normal_.end(); ++it1) {
				(*it1)->DecoupleClient(*it);
			}
		}
		// 释放资源
		tcpc_client_.erase(it);
	}
}

void GeneralControl::on_close_telescope(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_tele_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_tele_.begin(); it != tcpc_tele_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_tele_.end()) tcpc_tele_.erase(it);
}

void GeneralControl::on_close_mount(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_mount_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_mount_.end()) {
		if ((*it).use_count() > 1) {
			// 遍历观测系统, 解联关系
			mutex_lock lck(mtx_obss_gwac_);
			ObsSysVec::iterator it1;
			for (it1 = obss_gwac_.begin(); it1 != obss_gwac_.end(); ++it1) {
				(*it1)->DecoupleTelescope(*it);
			}
		}
		// 释放资源
		tcpc_mount_.erase(it);
	}
}

void GeneralControl::on_close_camera(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_camera_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_camera_.end()) tcpc_camera_.erase(it);
}

void GeneralControl::on_close_mount_annex(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_mount_annex_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_mount_annex_.begin(); it != tcpc_mount_annex_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_mount_annex_.end()) {
		if ((*it).use_count() > 1) {
			// 遍历观测系统, 解联关系
			mutex_lock lck(mtx_obss_gwac_);
			ObsSysVec::iterator it1;
			for (it1 = obss_gwac_.begin(); it1 != obss_gwac_.end(); ++it1) {
				from_obss<ObservationSystemGWAC>(*it1)->DecoupleMountAnnex(*it);
			}
		}
		// 释放资源
		tcpc_mount_annex_.erase(it);
	}
}

void GeneralControl::on_close_camera_annex(const long client, const long ec) {
	mutex_lock lck(mtx_tcpc_camera_annex_);
	TCPClient* ptr = (TCPClient*) client;
	TcpCVec::iterator it;

	for (it = tcpc_camera_annex_.begin(); it != tcpc_camera_annex_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_camera_annex_.end()) tcpc_camera_annex_.erase(it);
}

/*
 * 遍历观测计划, 查找匹配的计划并发送给观测系统
 */
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

	ObsSysPtr obss = find_obss(gid, uid, acq->type == "gwac" ? OST_GWAC : OST_NORMAL);
	/*
	 * 查找gid:uid强匹配的可执行观测计划
	 * - GWAC观测计划要求gid:uid不为空, 因此当IsMatched()>=0时, 其匹配值实际为1
	 * - 观测计划已按照优先级排序, 因此第一条匹配计划即为所需计划
	 */
	if (obss.use_count()) {
		mutex_lock lck(mtx_plans_);
		ptime now = second_clock::universal_time();
		ObsPlanPtr plan;
		for (ExObsPlanVec::iterator it = plans_.begin(); it != plans_.end(); ++it) {
			if ((*it)->IdleMatched(gid, uid) >= 0 && obss->PlanRelativePriority((*it)->plan, now) > 0) {
				(*it)->AssignObservationSystem(obss);
				plan = to_obsplan(*it);
				break;
			}
		}
		if (plan.use_count()) obss->NotifyPlan(plan);
	}
}

//////////////////////////////////////////////////////////////////////////////
/**
 * -1 检查观测系统<gid, uid>是否存在
 * -2 若不存在, 则尝试穿建并初始化
 */
ObsSysPtr GeneralControl::find_obss(const string& gid, const string& uid, int ostype) {
	ObsSysPtr obss;
	ObssTraitPtr trait;

	/* 检查参数有效性 */
	if (gid.empty() || uid.empty() || 0 == (trait = param_->GetObservationSystemTrait(gid)).use_count()) {
		_gLog.Write(LOG_FAULT, "GeneralControl::find_obss",
				"invalid ObservationSystem IDs<%s:%s>", gid.c_str(), uid.c_str());
	}
	/* 定位/创建观测系统 */
	else if (ostype == 0 || trait->ostype == ostype) {
		if (trait->ostype == OST_GWAC) {
			mutex_lock lck(mtx_obss_gwac_);
			ObsSysVec::iterator it;
			for (it = obss_gwac_.begin(); it != obss_gwac_.end() && (*it)->IsMatched(gid, uid) != 1; ++it);
			if (it != obss_gwac_.end()) obss = *it; // obss计数>1, 退出if后计数不变
			else {
				ObsSysGWACPtr obss_gwac = make_obss_gwac(gid, uid);
				obss = to_obss(obss_gwac); // obss计数==2, 退出if后计数==1
			}
		}
		else if (trait->ostype == OST_NORMAL) {
			mutex_lock lck(mtx_obss_normal_);
			ObsSysVec::iterator it;
			for (it = obss_normal_.begin(); it != obss_normal_.end() && (*it)->IsMatched(gid, uid) != 1; ++it);
			if (it != obss_normal_.end()) obss = *it; // obss计数>1, 退出if后计数不变
			else {
				ObsSysNormalPtr obss_normal = make_obss_normal(gid, uid);
				obss = to_obss(obss_normal); // obss计数==2, 退出if后计数==1
			}
		}
	}
	else {
		_gLog.Write(LOG_FAULT, NULL, "illegal ObservationSystemType<%d> for group_id<%s>",
				trait->ostype, gid.c_str());
	}

	/* 启动观测系统服务 */
	if (obss.unique()) {// 新创建观测系统
		if (!obss->StartService()) {// 观测系统服务启动失败
			obss.reset();
		}
		else {// 设置环境参量
			MountLimitPtr limit = param_->GetMountSafeLimit(gid, uid);
			obss->SetGeosite(trait->sitename, trait->lgt, trait->lat, trait->alt, trait->timezone);
			obss->SetElevationLimit(limit.use_count() ? limit->value : 20.0);

			if (trait->ostype == OST_GWAC) {
				mutex_lock lck(mtx_obss_gwac_);
				obss_gwac_.push_back(obss);
				// 注册回调函数: 申请新的观测计划
				const ObservationSystem::AcquireNewPlanSlot& slot = boost::bind(&GeneralControl::acquire_new_gwac, this, _1, _2);
				obss->RegisterAcquireNewPlan(slot);
			}
			else {// if (trait.ostype == OST_NORMAL)
				mutex_lock lck(mtx_obss_normal_);
				obss_normal_.push_back(obss);
				// 注册回调函数: 申请新的观测计划
				const ObservationSystem::AcquireNewPlanSlot& slot = boost::bind(&GeneralControl::acquire_new_plan, this, _1, _2);
				obss->RegisterAcquireNewPlan(slot);
			}
			// 注册回调函数: 观测计划工作状态发生变更
			const ObservationSystem::PlanFinishedSlot& slot = boost::bind(&GeneralControl::plan_finished, this, _1);
			obss->RegisterPlanFinished(slot);
		}
	}

	return obss;
}

/*
 * 定时检查观测系统的有效性
 */
void GeneralControl::thread_monitor_obss() {
	boost::chrono::minutes period(1);	// 定时周期: 1分钟

	while(1) {
		boost::this_thread::sleep_for(period);
		/* 检查/清理无效观测系统 */
		if (obss_gwac_.size()) {// GWAC观测系统
			mutex_lock lck(mtx_obss_gwac_);
			for (ObsSysVec::iterator it = obss_gwac_.begin(); it != obss_gwac_.end();) {
				if (!(*it)->HasAnyConnection()) it = obss_gwac_.erase(it);
				else ++it;
			}
		}
		if (obss_normal_.size()) {// 通用望远镜观测系统
			mutex_lock lck(mtx_obss_normal_);
			for (ObsSysVec::iterator it = obss_normal_.begin(); it != obss_normal_.end();) {
				if (!(*it)->HasAnyConnection()) it = obss_normal_.erase(it);
				else ++it;
			}
		}
	}
}

/*
 * 检查观测计划有效性
 * - 定时周期: 5分钟
 * - 有效性特征: (结束时间 - 当前时间) > 5分钟
 */
void GeneralControl::thread_monitor_plan() {
	boost::chrono::minutes period(5);	// 定时周期: 5分钟
	long minute5 = 5 * 60;

	while(1) {
		boost::this_thread::sleep_for(period);
		// 检查/清理常规观测计划
		if (plans_.size()) {
			mutex_lock lck(mtx_plans_);
			ptime now = second_clock::universal_time();
			time_duration dt;
			int x;

			for (ExObsPlanVec::iterator it = plans_.begin(); it != plans_.end();) {
				x  = (*it)->state;
				dt = from_iso_extended_string((*it)->plan->end_time) - now;
				if ((x >= OBSPLAN_OVER ) ||	// 已完成或已删除计划
					(dt.total_seconds() <= minute5 && x <= OBSPLAN_INT)) {// 计划当前未执行且结束时间到达
					write_plan_log(*it);
					it = plans_.erase(it);
				}
				else ++it;
			}
		}
	}
}

/*
 * 向数据库定时发送观测系统和观测计划工作状态
 */
void GeneralControl::database_upload(ObsSysVec& obss) {
	string strnow = to_iso_extended_string(second_clock::universal_time());
	string gid, uid;
	ObsPlanPtr plan;
	NFTelePtr tele;
	apcam cam;
	int plan_sn;
	const char* op_time;
	char strstat[512];

	for (ObsSysVec::iterator it = obss.begin(); it != obss.end(); ++it) {
		(*it)->GetID(gid, uid);

		// 转台
		tele = (*it)->GetTelescope();
		if (tele.use_count()) {
			db_->uploadMountStatus(gid.c_str(), uid.c_str(), tele->utc.c_str(),
					tele->state, tele->errcode, tele->ra, tele->dc, tele->ora, tele->odc, strstat);
		}
		// 相机
		ObssCamVec &cameras = (*it)->GetCameras();
		for (ObssCamVec::iterator it1 = cameras.begin(); it1 != cameras.end(); ++it1) {
			cam = (*it1)->info;
			if (cam.use_count()) {
				db_->uploadCameraStatus(gid.c_str(), uid.c_str(), cam->cid.c_str(), cam->utc.c_str(), cam->mcstate,
						cam->focus, cam->coolget, cam->filter.c_str(), cam->state, cam->errcode, cam->imgtype.c_str(),
						cam->objname.c_str(), cam->frmno, cam->filename.c_str(), strstat);
			}
		}
		// 系统
		op_time = (*it)->GetPlan(plan_sn);
		db_->uploadObsCtlSysStatus(gid.c_str(), uid.c_str(), strnow.c_str(), (*it)->GetState(),
				plan_sn, !op_time ? "" : op_time, strstat);
	}
}

void GeneralControl::thread_status() {
	boost::chrono::seconds period(10);	// 定时周期: 10秒

	while(1) {
		boost::this_thread::sleep_for(period);

		/* 发送GWAC系统工作状态 */
		if (obss_gwac_.size()) {
			mutex_lock lck(mtx_obss_gwac_);
			database_upload(obss_gwac_);
		}

		/* 发送通用系统工作状态 */
		if (obss_normal_.size()) {
			mutex_lock lck(mtx_obss_normal_);
			database_upload(obss_normal_);
		}
	}
}

void GeneralControl::exit_ignore_plan() {
	for (ExObsPlanVec::iterator it = plans_.begin(); it != plans_.end(); ++it) {
		write_plan_log(*it);
	}
}

void GeneralControl::exit_close_obss(ObsSysVec &obss) {
	for (ObsSysVec::iterator it = obss.begin(); it != obss.end(); ++it) {
		(*it)->StopService();
		(*it).reset();
	}
}

/*
 * 构建观测计划并入库
 */
void GeneralControl::catalogue_new_plan(ExObsPlanPtr plan) {
	int x = plan->plan->priority;

	mutex_lock lck(mtx_plans_);
	ExObsPlanVec::iterator it;
	for (it = plans_.begin(); it != plans_.end() && (*it)->RelativePriority(x) >= 0; ++it);
	plans_.insert(it, plan);
}

ExObsPlanPtr GeneralControl::catalogue_new_plan(apappplan proto) {
	ExObsPlanPtr plan = boost::make_shared<ExObservationPlan>(proto);
	catalogue_new_plan(plan);

	return plan;
}

void GeneralControl::correct_time(apappplan proto) {
	/*
	 * 检查并改正计划起始/结束时间
	 * 起始时间:
	 * - 无效或已经过或三天后, 修改为当前时间
	 *
	 * 结束时间:
	 * - 无效或已经过或三天后, 修改为当前时间 + 72小时
	 */
	ptime now = second_clock::universal_time();
	string s = to_iso_extended_string(now);
	time_duration dt;
	long days3 = 86400 * 3;
	try {// 起始时间
		dt = from_iso_extended_string(proto->begin_time) - now;
		if (dt.total_seconds() < 0 || dt.total_seconds() >= days3) proto->begin_time = s;
	}
	catch(...) {
		proto->begin_time = s;
	}

	s = to_iso_extended_string(now + hours(72));
	try {// 结束时间
		dt = from_iso_extended_string(proto->end_time) - now;
		if (dt.total_seconds() < 0 || dt.total_seconds() > days3) proto->end_time = s;
	}
	catch(...) {
		proto->end_time = s;
	}
}

/*
 * 在日志文件中记录收到的观测计划
 */
void GeneralControl::write_plan_log(apappplan proto) {
	_gLog.Write("Plan: %6s<%6d> %8.4f %8.4f %6s %4d, IDs<%s:%s>",
			proto->type == "append_gwac" ? "GWAC" : "Normal",
			proto->plan_sn,
			valid_ra(proto->ra) ? proto->ra : 0.0,
			valid_dec(proto->dec) ? proto->dec : 0.0,
			proto->imgtype.c_str(), proto->priority,
			proto->gid.c_str(), proto->uid.c_str());
}

/*
 * 在观测计划日志中, 记录当观测计划被销毁时的执行状态
 */
void GeneralControl::write_plan_log(ExObsPlanPtr x) {
	apappplan plan = x->plan;
	OBSPLAN_STATUS s = x->state;

	_gLogPlan.Write(s == OBSPLAN_OVER ? LOGPLAN_OVER : (s == OBSPLAN_INT ? LOGPLAN_INTR : LOGPLAN_IGNR),
			"%6d %7.3f %7.3f %6s %4d, %s:%s",
			plan->plan_sn,
			valid_ra(plan->ra) ? plan->ra : 0.0,
			valid_dec(plan->dec) ? plan->dec : 0.0,
			plan->imgtype.c_str(), plan->priority,
			plan->gid.c_str(), plan->uid.c_str());
}

/*
 * 转换关系: 普通型与扩展型观测计划指针
 */
ObsPlanPtr GeneralControl::to_obsplan(ExObsPlanPtr ptr) {
	return boost::static_pointer_cast<ObservationPlan>(ptr);
}

ExObsPlanPtr GeneralControl::from_obsplan(ObsPlanPtr ptr) {
	return boost::static_pointer_cast<ExObservationPlan>(ptr);
}

/*
 * void planstate_changed() 观测计划工作状态发生改变
 * - 回调函数
 * - 当计划完成/中断/删除时, 解除与观测系统的关联
 */
void GeneralControl::plan_finished(ObsPlanPtr ptr) {
	if (ptr->state != OBSPLAN_WAIT && ptr->state != OBSPLAN_RUN) from_obsplan(ptr)->obss.reset();
}

void GeneralControl::acquire_new_gwac(const string& gid, const string& uid) {
	mutex_lock lck(mtx_acqplan_);
	AcquirePlanPtr acq = boost::make_shared<AcquirePlan>("gwac", gid, uid);
	que_acqplan_.push_back(acq);
	PostMessage(MSG_ACQUIRE_PLAN);
}

void GeneralControl::acquire_new_plan(const string& gid, const string& uid) {
	mutex_lock lck(mtx_acqplan_);
	AcquirePlanPtr acq = boost::make_shared<AcquirePlan>("normal", gid, uid);
	que_acqplan_.push_back(acq);
	PostMessage(MSG_ACQUIRE_PLAN);
}
