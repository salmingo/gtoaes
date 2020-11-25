/**
 * @file GeneralControl.h 声明文件, 封装总控服务
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
#include "globaldef.h"
#include "GLog.h"
#include "GeneralControl.h"
#include "ADefine.h"
#include "ATimeSpace.h"

using namespace boost;
using namespace boost::posix_time;
using namespace boost::placeholders;
using namespace AstroUtil;

GeneralControl::GeneralControl() {
}

GeneralControl::~GeneralControl() {
}

//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::Start() {
	// 启动消息机制
	string name = DAEMON_NAME;
	if (!MessageQueue::Start(name.c_str())) return false;
	// 加载参数
	param_.Load(gConfigPath);
	// 启动网络服务
	if (!create_all_server()) return false;
	bufUdp_.reset(new char[UDP_PACK_SIZE]);
	bufTcp_.reset(new char[TCP_PACK_SIZE]);
	kvProto_    = KvProtocol::Create();
	nonkvProto_ = NonkvProtocol::Create();
	// 其它设备初始化
	if (param_.ntpEnable) {
		ntp_ = NTPClient::Create(param_.ntpHost.c_str(), 123, param_.ntpDiffMax);
		ntp_->EnableAutoSynch();
	}
	if (param_.dbEnable) dbPtr_ = DatabaseCurl::Create(param_.dbUrl);
	// 观测计划
	obsPlans_  = ObservationPlan::Create();
	// 启动线程
	thrd_odt_.reset(new boost::thread(boost::bind(&GeneralControl::thread_odt, this)));
	thrd_noon_.reset(new boost::thread(boost::bind(&GeneralControl::thread_noon, this)));
	thrd_tcpClean_.reset(new boost::thread(boost::bind(&GeneralControl::thread_clean_tcp, this)));

	return true;
}

void GeneralControl::Stop() {
	MessageQueue::Stop();
	close_all_server();
	interrupt_thread(thrd_tcpClean_);
	interrupt_thread(thrd_noon_);
	interrupt_thread(thrd_odt_);
	for (OBSSVec::iterator it = obss_.begin(); it != obss_.end(); ++it)
		(*it)->Stop();
	for (TcpCVec::iterator it = tcpC_buff_.begin(); it != tcpC_buff_.end(); ++it) {
		if ((*it)->IsOpen()) (*it)->Close();
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息响应 -----------------*/
void GeneralControl::register_messages() {
	const CBSlot& slot1 = boost::bind(&GeneralControl::on_tcp_receive, this, _1, _2);
	const CBSlot& slot2 = boost::bind(&GeneralControl::on_env_changed, this, _1, _2);

	RegisterMessage(MSG_TCP_RECEIVE, slot1);
	RegisterMessage(MSG_ENV_CHANGED, slot2);
}

void GeneralControl::on_tcp_receive(const long par1, const long par2) {
	TcpRcvPtr rcvd;
	{// 取队首
		MtxLck lck(mtx_tcpRcv_);
		rcvd = que_tcpRcv_.front();
		que_tcpRcv_.pop_front();
	}

	TcpCPtr client = rcvd->client;
	if (rcvd->hadRcvd) {
		const char term[] = "\n";	// 信息结束符: 换行
		int lenTerm = strlen(term);	// 结束符长度
		int pos;
		while (client->IsOpen() && (pos = client->Lookup(term, lenTerm)) >= 0) {
			client->Read(bufTcp_.get(), pos + lenTerm);
			bufTcp_[pos] = 0;
			resolve_from_peer(client, rcvd->peer);
		}
	}
	else {
		client->Close();
		close_socket(client, rcvd->peer);
	}
}

void GeneralControl::on_env_changed(const long par1, const long par2) {
	NfEnvPtr nfEnv;
	{// 取队首事件
		MtxLck lck_net(mtx_nfEnv_);
		nfEnv = que_nfEnv_.front();
		que_nfEnv_.pop_front();
	}

	/* 响应变化: 关闭天窗 */
	string gid = nfEnv->gid;
	if (!(nfEnv->param || (nfEnv->param = param_.GetParamOBSS(gid))))
		_gLog.Write(LOG_FAULT, "undefined Environment[%s]", gid.c_str());
	else {// 安全性判定
		const OBSSParam* param = nfEnv->param;
		bool safe = !((param->useRainfall && nfEnv->rain)	// 降水
				|| (param->useWindSpeed && nfEnv->speed > param->maxWindSpeed)  // 大风
				|| (param->useCloudCamera && nfEnv->cloud > param->maxCloudPerent)); // 多云
		if (safe != nfEnv->safe) {
			_gLog.Write("Environment[%s] shows %s", gid.c_str(), safe ? "safe" : "!!! DANGEROUS !!!");
			nfEnv->safe = safe;
			if (param->useDomeSlit) {
				if (!safe)
					command_slit(gid, "", CommandSlit::SLITC_CLOSE);
				else if (param->robotic && nfEnv->odt > TypeObservationDuration::ODT_DAYTIME)
					command_slit(gid, "", CommandSlit::SLITC_OPEN);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 网络服务 -----------------*/
bool GeneralControl::create_server(TcpSPtr *server, const uint16_t port) {
	const TcpServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	*server = TcpServer::Create();
	(*server)->RegisterAccept(slot);
	return (*server)->CreateServer(port);
}

bool GeneralControl::create_all_server() {
	/* 启动TCP服务 */
	if (!(     create_server(&tcpS_client_,      param_.portClient)
			&& create_server(&tcpS_mount_,       param_.portMount)
			&& create_server(&tcpS_camera_,      param_.portCamera)
			&& create_server(&tcpS_mountAnnex_,  param_.portMountAnnex)
			&& create_server(&tcpS_cameraAnnex_, param_.portCameraAnnex))) {
		_gLog.Write(LOG_FAULT, "failed to create network server");
		return false;
	}
	/* 启动UDP服务 */
	udpS_env_ = UdpSession::Create();
	if (udpS_env_->Open(param_.portEnv)) {
		const UdpSession::CBSlot& slot = boost::bind(&GeneralControl::receive_from_env, this, _1, _2);
		udpS_env_->RegisterRead(slot);
	}
	return true;
}

void GeneralControl::close_server(TcpSPtr& server) {
	if (server.unique()) server.reset();
}

void GeneralControl::close_all_server() {
	close_server(tcpS_client_);
	close_server(tcpS_mount_);
	close_server(tcpS_camera_);
	close_server(tcpS_mountAnnex_);
	close_server(tcpS_cameraAnnex_);
	if (udpS_env_.unique()) udpS_env_->Close();
}

void GeneralControl::network_accept(const TcpCPtr client, const TcpSPtr server) {
	int peer(PEER_CLIENT);
	if      (server == tcpS_mount_)       peer = PEER_MOUNT;
	else if (server == tcpS_camera_)      peer = PEER_CAMERA;
	else if (server == tcpS_mountAnnex_)  peer = PEER_MOUNT_ANNEX;
	else if (server == tcpS_cameraAnnex_) peer = PEER_CAMERA_ANNEX;

	MtxLck lck(mtx_tcpC_buff_);
	const TcpClient::CBSlot& slot = boost::bind(&GeneralControl::receive_from_peer, this, _1, _2, peer);
	client->RegisterRead(slot);
	tcpC_buff_.push_back(client);
}

void GeneralControl::receive_from_peer(const TcpCPtr client, const error_code& ec, int peer) {
	MtxLck lck(mtx_tcpRcv_);
	TcpRcvPtr rcvd = TcpReceived::Create(client, peer, !ec);
	que_tcpRcv_.push_back(rcvd);
	PostMessage(MSG_TCP_RECEIVE);
}

void GeneralControl::receive_from_env(const UdpPtr client, const error_code& ec) {
	int n;

	if (client->Read(bufUdp_.get(), n) && bufUdp_[n - 1] == '\n') {
		bufUdp_[n - 1] = 0;
		/* 解析气象信息 */
		kvbase base = kvProto_->ResolveEnv(bufUdp_.get());
		string gid  = base->gid;
		NfEnvPtr nfEnv = find_info_env(gid);

		if (nfEnv.use_count()) {
			string type = base->type;
			bool changed(false);

			if (iequals(type, KVTYPE_RAINFALL)) {
				kvrain proto = from_kvbase<kv_proto_rainfall>(base);
				if (nfEnv->rain != proto->value) {
					nfEnv->rain = proto->value;
					changed = true;
				}
			}
			else if (iequals(type, KVTYPE_WIND)) {
				kvwind proto = from_kvbase<kv_proto_wind>(base);
				nfEnv->orient = proto->orient;
				if (nfEnv->speed != proto->speed) {
					nfEnv->speed = proto->speed;
					changed = true;
				}
			}
			else if (iequals(type, KVTYPE_CLOUD)) {
				kvcloud proto = from_kvbase<kv_proto_cloud>(base);
				if (nfEnv->cloud != proto->value) {
					nfEnv->cloud = proto->value;
					changed = true;
				}
			}

			if (changed) {
				MtxLck lck(mtx_nfEnv_);
				que_nfEnv_.push_back(nfEnv);
				PostMessage(MSG_ENV_CHANGED);
			}
		}
	}
}

void GeneralControl::erase_coupled_tcp(const TcpCPtr client) {
	MtxLck lck(mtx_tcpC_buff_);
	TcpCVec::iterator it, itend = tcpC_buff_.end();
	for (it = tcpC_buff_.begin(); it != itend && (*it) != client; ++it);
	if (it != itend) tcpC_buff_.erase(it);
}

void GeneralControl::close_socket(const TcpCPtr client, int peer) {
	// 解除与观测系统的耦合
	MtxLck lck1(mtx_obss_);
	for (OBSSVec::iterator it = obss_.begin(); client.use_count() > 2 && it != obss_.end(); ++it) {
		if      (peer == PEER_CLIENT)      (*it)->DecoupleClient     (client);
		else if (peer == PEER_MOUNT)       (*it)->DecoupleMount      (client);
		else if (peer == PEER_CAMERA)      (*it)->DecoupleCamera     (client);
		else if (peer == PEER_MOUNT_ANNEX) (*it)->DecoupleMountAnnex (client);
		else                               (*it)->DecoupleCameraAnnex(client);
	}
	// 解除与多模天窗的耦合
	if (peer == PEER_MOUNT_ANNEX && client.use_count() > 2) {
		MtxLck lck2(mtx_slit_);
		SlitMulVec::iterator it, itend = slit_.end();
		for (it = slit_.begin(); it != itend && (*it)->client != client; ++it);
		if (it != itend) slit_.erase(it);
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 解析/执行通信协议 -----------------*/
void GeneralControl::resolve_from_peer(const TcpCPtr client, int peer) {
	const char prefix[] = "g#";	// 非键值对格式的引导符
	int lenPre  = strlen(prefix);	// 引导符长度

	if (strstr(bufTcp_.get(), prefix)) {// 非键值对协议
		nonkvbase base = nonkvProto_->Resove(bufTcp_.get() + lenPre);
		if (base.unique()) {
			if      (peer == PEER_MOUNT)       process_nonkv_mount      (client, base);
			else if (peer == PEER_MOUNT_ANNEX) process_nonkv_mount_annex(client, base);
		}
		else {
			_gLog.Write(LOG_FAULT, "unknown protocol from %s: [%s]",
					peer == PEER_MOUNT ? "mount" : "mount-annex",
					bufTcp_.get());
			client->Close();
		}
	}
	else {// 键值对协议
		if      (peer == PEER_CLIENT)       resolve_kv_client      (client);
		else if (peer == PEER_MOUNT)        resolve_kv_mount       (client);
		else if (peer == PEER_CAMERA)       resolve_kv_camera      (client);
		else if (peer == PEER_MOUNT_ANNEX)  resolve_kv_mount_annex (client);
		else if (peer == PEER_CAMERA_ANNEX) resolve_kv_camera_annex(client);
	}
}

void GeneralControl::resolve_kv_client(const TcpCPtr client) {
	/*
	 * 客户端接收到的信息, 分为两种处理方式:
	 * 1. 本地处理
	 * 2. 投递给观测系统
	 */
	kvbase base = kvProto_->ResolveClient(bufTcp_.get());
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from client: [%s]", bufTcp_.get());
		client->Close();
	}
	else {
		string type = base->type;
		string gid  = base->gid;
		string uid  = base->uid;

		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 新的观测计划 !!!!!!<*/
		if (iequals(type, KVTYPE_APPPLAN) || iequals(type, KVTYPE_IMPPLAN)) {
			ObsPlanItemPtr plan;
			bool opNow = (type[0] == 'a' || type[0] == 'A') ? false : true;
			plan = opNow ? from_kvbase<kv_proto_implement_plan>(base)->plan
					: from_kvbase<kv_proto_append_plan>(base)->plan;
			if (plan->CompleteCheck()) {
				obsPlans_->AddPlan(plan);	// 计划加入队列
				if (opNow) try_implement_plan(plan);
			}
			else {
				_gLog.Write(LOG_FAULT, "plan[%s] couldn't pass validity check", plan->plan_sn.c_str());
			}
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 中止观测计划 !!!!!!<*/
		else if (iequals(type, KVTYPE_ABTPLAN)) {
			try_abort_plan(from_kvbase<kv_proto_abort_plan>(base)->plan_sn);
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 检查观测计划 !!!!!!<*/
		else if (iequals(type, KVTYPE_CHKPLAN)) {// 查看观测计划状态
			string plan_sn = from_kvbase<kv_proto_check_plan>(base)->plan_sn;
			ObsPlanItemPtr plan = obsPlans_->Find(plan_sn);
			kvplan proto = boost::make_shared<kv_proto_plan>();
			const char* s;
			int n;

			proto->plan_sn = plan_sn;
			if (plan.use_count()) proto->state = plan->state;
			else proto->state = StateObservationPlan::OBSPLAN_ERROR;
			s = kvProto_->CompactPlan(proto, n);
			client->Write(s, n);
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 关联观测系统 !!!!!!<*/
		else if (iequals(type, KVTYPE_REG)) {
			MtxLck lck(mtx_obss_);
			int matched(0);
			for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid, uid))) (*it)->CoupleClient(client);
			}
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 解除与观测系统的关联 !!!!!!<*/
		else if (iequals(type, KVTYPE_UNREG)) {
			MtxLck lck(mtx_obss_);
			int matched(0);
			for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid, uid))) (*it)->DecoupleClient(client);
			}
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 开关天窗 !!!!!!<*/
		else if (iequals(type, KVTYPE_SLIT)) {
			command_slit(gid, uid, from_kvbase<kv_proto_slit>(base)->command);
		}
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 投递到观测系统 !!!!!!<*/
		else {
			MtxLck lck(mtx_obss_);
			int matched(0);
			for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid, uid))) (*it)->NotifyKVClient(base);
			}
		}
	}
} // process_kv_client(kvbase proto, const TcpCPtr client)

void GeneralControl::resolve_kv_mount(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveMount(bufTcp_.get());
	bool success(false);
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from mount: [%s]", bufTcp_.get());
	}
	else {
		// kv协议的转台连接耦合到观测系统
		string gid = base->gid;
		string uid = base->uid;
		if (gid.empty() || uid.empty()) {
			_gLog.Write(LOG_FAULT, "requires non-empty mount IDs[%s:%s]",
					gid.c_str(), uid.c_str());
		}
		else {
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleMount(client, base))) {// P2P或P2H模式
					if (mode == MODE_P2P) erase_coupled_tcp(client);
					success = true;
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple mount with OBSS[%s:%s]", gid.c_str(), uid.c_str());
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for mount", gid.c_str(), uid.c_str());
			}
		}
	}
	if (!success) client->Close();
}

void GeneralControl::resolve_kv_camera(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveCamera(bufTcp_.get());
	bool success(false);
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from camera: [%s]", bufTcp_.get());
	}
	else {
		// kv协议的相机连接耦合到观测系统
		string gid  = base->gid;
		string uid  = base->uid;
		string cid  = base->cid;
		if (gid.empty() || uid.empty() || cid.empty()) {
			_gLog.Write(LOG_FAULT, "requires non-empty camera IDs[%s:%s:%s]",
					gid.c_str(), uid.c_str(), cid.c_str());
		}
		else {
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleCamera(client, base))) {// P2P或P2H模式
					if (mode == MODE_P2P) erase_coupled_tcp(client);
					success = true;
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple camera[%s] with OBSS[%s:%s]",
							cid.c_str(), gid.c_str(), uid.c_str());
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for camera[%s]",
						gid.c_str(), uid.c_str(), cid.c_str());
			}
		}
	}
	if (!success) client->Close();
}

void GeneralControl::resolve_kv_mount_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveMountAnnex(bufTcp_.get());
	bool success(false);
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from mount-annex: [%s]", bufTcp_.get());
	}
	else {
		// kv协议的转台连接耦合到观测系统
		string gid = base->gid;
		string uid = base->uid;
		if (gid.empty() || (uid.empty() && !iequals(base->type, KVTYPE_SLIT))) {
			_gLog.Write(LOG_FAULT, "illegal protocol from mount-annex: [%s]", bufTcp_.get());
		}
		else if (uid.empty()) {
			int state = from_kvbase<kv_proto_slit>(base)->state;
			SlitMulPtr slit = find_slit(gid, client, true);
			if (slit.use_count()) {
				if (state != slit->state) {// 通知相关观测系统天窗状态
					_gLog.Write("Slit[%s] is %s", StateSlit::ToString(state));
					slit->state = state;

					MtxLck lck(mtx_obss_);
					OBSSVec::iterator itend = obss_.end();
					for (OBSSVec::iterator it = obss_.begin(); it != itend; ++it) {
						if ((*it)->IsMatched(gid, uid)) (*it)->NotifySlitState(state);
					}
				}
				success = true;
			}
			else {
				_gLog.Write(LOG_FAULT, "no settings found for slit[%s]", gid.c_str());
			}
		}
		else {// 当(gid, uid)都不为空时, 投递到观测系统
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleMountAnnex(client, base))) {// P2P或P2H模式
					if (mode == MODE_P2P) erase_coupled_tcp(client);
					success = true;
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple mount-annex with OBSS[%s:%s]", gid.c_str(), uid.c_str());
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for mount-annex", gid.c_str(), uid.c_str());
			}
		}
	}
	if (!success) client->Close();
}

void GeneralControl::resolve_kv_camera_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveCameraAnnex(bufTcp_.get());
	bool success(false);
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from camera-annex: [%s]", bufTcp_.get());
	}
	else {
		// kv协议的相机连接耦合到观测系统
		string gid  = base->gid;
		string uid  = base->uid;
		string cid  = base->cid;
		if (gid.empty() || uid.empty() || cid.empty()) {
			_gLog.Write(LOG_FAULT, "illegal protocol from camera-annex: [%s]", bufTcp_.get());
		}
		else {
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleCameraAnnex(client, base))) {// P2P或P2H模式
					if (mode == MODE_P2P) erase_coupled_tcp(client);
					success = true;
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple camera-annex[%s] with OBSS[%s:%s]",
							cid.c_str(), gid.c_str(), uid.c_str());
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for camera-annex[%s]",
						gid.c_str(), uid.c_str(), cid.c_str());
			}
		}
	}
	if (!success) client->Close();
}

void GeneralControl::process_nonkv_mount(const TcpCPtr client, nonkvbase base) {
	string gid = base->gid;
	string uid = base->uid;
	bool success(false);

	if (gid.empty() || uid.empty()) {
		_gLog.Write(LOG_FAULT, "requires non-empty mount IDs[%s:%s]",
				gid.c_str(), uid.c_str());
	}
	else {
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss.use_count()) {
			int mode;
			if ((mode = obss->CoupleMount(client, base))) {// P2P或P2H模式
				success = true;
			}
			else {
				_gLog.Write(LOG_FAULT, "failed to couple mount with OBSS[%s:%s]", gid.c_str(), uid.c_str());
			}
		}
		else {
			_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for mount", gid.c_str(), uid.c_str());
		}
	}
	if (!success) client->Close();
}

void GeneralControl::process_nonkv_mount_annex(const TcpCPtr client, nonkvbase base) {
	string type = base->type;
	string gid = base->gid;
	string uid = base->uid;
	bool success(false);

	if (gid.empty()) {
		_gLog.Write(LOG_FAULT, "illegal protocol from mount-annex: [%s]", bufTcp_.get());
	}
	else if (uid.empty()) {
		if (iequals(type, NONKVTYPE_SLIT)) {
			int state = from_nonkvbase<nonkv_proto_slit>(base)->state;
			SlitMulPtr slit = find_slit(gid, client, false);
			if (slit.use_count()) {
				if (state != slit->state) {// 通知相关观测系统天窗状态
					_gLog.Write("Slit[%s] is %s", StateSlit::ToString(state));
					slit->state = state;

					MtxLck lck(mtx_obss_);
					OBSSVec::iterator itend = obss_.end();
					for (OBSSVec::iterator it = obss_.begin(); it != itend; ++it) {
						if ((*it)->IsMatched(gid, uid)) (*it)->NotifySlitState(state);
					}
				}
				success = true;
			}
			else {
				_gLog.Write(LOG_FAULT, "no settings found for slit[%s]", gid.c_str());
			}
		}
		else if (iequals(type, NONKVTYPE_RAIN)) {
			NfEnvPtr nfEnv = find_info_env(gid);
			if (nfEnv.use_count()) {
				int rain = from_nonkvbase<nonkv_proto_rain>(base)->state;
				if (nfEnv->rain != rain) {
					nfEnv->rain = rain;

					MtxLck lck(mtx_nfEnv_);
					que_nfEnv_.push_back(nfEnv);
					PostMessage(MSG_ENV_CHANGED);
				}
				success = true;
			}
			else {
				_gLog.Write(LOG_FAULT, "no settings found for rainfall[%s]", gid.c_str());
			}
		}
	}
	else {// 投递到观测系统
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss.use_count()) {
			int mode;
			if ((mode = obss->CoupleMountAnnex(client, base))) {// P2P或P2H模式
				success = true;
			}
			else {
				_gLog.Write(LOG_FAULT, "failed to couple mount-annex with OBSS[%s:%s]", gid.c_str(), uid.c_str());
			}
		}
		else {
			_gLog.Write(LOG_FAULT, "no OBSS[%s:%s] found for mount-annex", gid.c_str(), uid.c_str());
		}
	}
	if (!success) client->Close();
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 观测计划 -----------------*/
bool GeneralControl::is_valid_plantime(const ObsPlanItemPtr plan, const ptime& now) {
	int tmLeadMax(300);	// 时间提前量, 秒

	return ((plan->tmbegin - now).total_seconds() <= tmLeadMax
			&& (plan->tmend - now).total_seconds() >= plan->period);
}

ObsPlanItemPtr GeneralControl::acquire_new_plan(const ObsSysPtr obss) {
	// 尝试为观测系统分配计划
	MtxLck lck(mtx_obsPlans_);
	ptime now = second_clock::universal_time();
	ObsPlanItemPtr plan;
	string gid, uid;
	bool found(false);

	obss->GetIDs(gid, uid);
	if (obsPlans_->Find(gid, uid)) {
		while (!found && obsPlans_->GetNext(plan)) {
			found = is_valid_plantime(plan, now) && obss->IsSafePoint(plan, now);
		}

		if (!found) plan.reset();
	}
	return plan;
}

void GeneralControl::try_implement_plan(ObsPlanItemPtr plan) {
	// kv_proto_implement_plan指向的观测计划需要立即执行, 选择合适的观测系统.
	MtxLck lck(mtx_obss_);
	string gid = plan->gid;
	string uid = plan->uid;
	ptime now = second_clock::universal_time();
	ObsSysPtr obss;
	int matched(0), prio_min(INT_MAX), prio_plan(plan->priority), prio;

	/* 有效性检查
	 * - (gid, uid)匹配. 1: 强匹配; 2: 弱匹配
	 * - 计划开始时间在时间提前量最大范围内
	 * - 结束时间足够完成计划
	 * - 观测系统优先级最低
	 * - 坐标在安全范围内
	 **/
	if (is_valid_plantime(plan, now)) {
		for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1 && prio_min > 0; ++it) {
			if ((matched = (*it)->IsMatched(gid, uid))
					&& (prio = (*it)->GetPriority()) < prio_plan
					&& prio < prio_min
					&& (*it)->IsSafePoint(plan, now)) {
				prio_min = prio;
				obss = *it;
			}
		}
	}
	if (obss.use_count()) // 尝试投递计划
		obss->NotifyPlan(plan);
	else
		_gLog.Write(LOG_WARN, "plan[%s] will delay implementation", plan->plan_sn.c_str());
}

void GeneralControl::try_abort_plan(const string& plan_sn) {
	// 尝试中止计划
	ObsPlanItemPtr plan = obsPlans_->Find(plan_sn);

	if (plan.use_count()) {// 找到计划
		_gLog.Write("abort plan[%s], current state is %s",
				StateObservationPlan::ToString(plan->state));
		// 变更状态
		if (plan->state == StateObservationPlan::OBSPLAN_CATALOGED || plan->state == StateObservationPlan::OBSPLAN_INTERRUPTED) {
			plan->state = StateObservationPlan::OBSPLAN_DELETED;
		}
		// 中止计划
		else if (plan->state == StateObservationPlan::OBSPLAN_RUNNING || plan->state == StateObservationPlan::OBSPLAN_WAITING) {
			ObsSysPtr obss = find_obss(plan->gid, plan->uid);
			obss->AbortPlan(plan);
		}
	}
	else
		_gLog.Write(LOG_WARN, "plan[%s] was not found", plan_sn.c_str());
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 观测系统 -----------------*/
ObsSysPtr GeneralControl::find_obss(const string& gid, const string& uid) {
	MtxLck lck(mtx_obss_);
	ObsSysPtr obss;
	OBSSVec::iterator it, end = obss_.end();
	for (it = obss_.begin(); it != end && !(*it)->IsMatched(gid, uid); ++it);
	if (it != end) obss = *it;
	else {
		_gLog.Write("try to create ObservationSystem[%s:%s]",
				gid.c_str(), uid.c_str());
		/* 新建观测系统 */
		const OBSSParam* param = param_.GetParamOBSS(gid);
		if (param) {
			obss = ObservationSystem::Create(gid, uid);
			if (obss->Start()) {
				const ObservationSystem::AcqPlanCBSlot& slot = boost::bind(&GeneralControl::acquire_new_plan, this, _1);
				obss->RegisterAcquirePlan(slot);
				obss->SetParameter(param);
				obss->SetDBPtr(dbPtr_);
				obss_.push_back(obss);

				NfEnvPtr nfEnv = find_info_env(param);	// 检查并创建新的环境信息
				obss->NotifyODT(nfEnv->odt);
			}
			else obss.reset();
		}
		else {
			_gLog.Write(LOG_FAULT, "not found any setting for ObservationSystem[%s:%s]",
					gid.c_str(), uid.c_str());
		}
	}
	return obss;
}

SlitMulPtr GeneralControl::find_slit(const string& gid, const TcpCPtr client, bool kvtype) {
	MtxLck lck(mtx_slit_);
	SlitMulPtr slit;
	SlitMulVec::iterator it, itend = slit_.end();
	for (slit_.begin(); it != itend && !(*it)->IsMatched(gid); ++it);
	if (it != itend) slit = *it;
	else if (param_.GetParamOBSS(gid)) {
		slit = SlitMultiplex::Create(gid);
		slit->client = client;
		slit->kvtype = kvtype;
		slit_.push_back(slit);
	}

	return slit;
}

void GeneralControl::command_slit(const string& gid, const string& uid, int cmd) {
	if (CommandSlit::IsValid(cmd)) {
		{// 通知复用天窗
			int matched(0);
			MtxLck lck(mtx_slit_);
			SlitMulVec::iterator itend = slit_.end();
			for (SlitMulVec::iterator it = slit_.begin(); it != itend && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid))) command_slit(*it, cmd);
			}
		}

		{// 通知观测系统
			int matched(0);
			MtxLck lck(mtx_obss_);
			kvslit proto = boost::make_shared<kv_proto_slit>();
			kvbase base;
			proto->command = cmd;
			base = to_kvbase(proto);
			for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid, uid))) (*it)->NotifyKVClient(base);
			}
		}
	}
}

void GeneralControl::command_slit(const SlitMulPtr slit, int cmd) {
	int n;
	const char* s;
	if (slit->kvtype)
		s = kvProto_->CompactSlit(slit->gid, "", cmd, n);
	else
		s = nonkvProto_->CompactSlit(slit->gid, "", cmd, n);
	slit->client->Write(s, n);
}

/*----------------- 环境信息 -----------------*/
GeneralControl::NfEnvPtr GeneralControl::find_info_env(const string& gid) {
	NfEnvPtr env;
	const OBSSParam* param = param_.GetParamOBSS(gid);
	if (param) env = find_info_env(param);
	return env;
}

GeneralControl::NfEnvPtr GeneralControl::find_info_env(const OBSSParam* param) {
	MtxLck lck(mtx_nfEnv_);
	string gid = param->gid;
	NfEnvPtr env;
	NfEnvVec::iterator it, itend = nfEnv_.end();
	for (it = nfEnv_.begin(); it != itend && (*it)->gid != gid; ++it);
	if (it != itend) env = *it;
	else {
		_gLog.Write("creating Environment Information[%s]", gid.c_str());
		env = EnvInfo::Create(gid);
		env->param = param;
		nfEnv_.push_back(env);
	}
	return env;
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 多线程 -----------------*/
/* 处理网络事件 */
void GeneralControl::thread_clean_tcp() {
	boost::chrono::minutes period(1);

	while (1) {
		boost::this_thread::sleep_for(period);
		// 清理已关闭的网络连接
		MtxLck lck1(mtx_tcpC_buff_);
		for (TcpCVec::iterator it = tcpC_buff_.begin(); it != tcpC_buff_.end(); ) {
			if ((*it)->IsOpen()) ++it;
			else it = tcpC_buff_.erase(it);
		}
		// 清理已关闭的多模天窗
		MtxLck lck2(mtx_slit_);
		for (SlitMulVec::iterator it = slit_.begin(); it != slit_.end(); ) {
			if ((*it)->IsOpen()) ++it;
			else it = slit_.erase(it);
		}
	}
}

void GeneralControl::thread_odt() {
	boost::chrono::minutes period(2);
	ATimeSpace ats;
	const OBSSParam* param;
	ptime now;
	ptime::date_type today;
	double fd, mjd, lmst;
	double ra, dec;		// 太阳赤道坐标
	double azi, alt;	// 太阳地平坐标
	int odt;
	string gid;
	string uid = "";

	while (1) {
		boost::this_thread::sleep_for(period);

		/* 更新各系统的观测时间类型标志 */
		MtxLck lck(mtx_nfEnv_);
		now = second_clock::universal_time();
		today = now.date();
		fd = now.time_of_day().total_seconds() / DAYSEC;
		ats.SetUTC(today.year(), today.month(), today.day(), fd);
		mjd = ats.ModifiedJulianDay();
		ats.SunPosition(ra, dec);

		for (NfEnvVec::iterator it = nfEnv_.begin(); it != nfEnv_.end(); ++it) {
			param = (*it)->param;
			gid = param->gid;
			/* 依据太阳高度角判定可观测时间类型 */
			ats.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timeZone);
			lmst = ats.LocalMeanSiderealTime(mjd, param->siteLon * D2R);
			ats.Eq2Horizon(lmst - ra, dec, azi, alt);
			alt *= R2D;
			if (alt > param->altDay)        odt = TypeObservationDuration::ODT_DAYTIME;
			else if (alt < param->altNight) odt = TypeObservationDuration::ODT_NIGHT;
			else                            odt = TypeObservationDuration::ODT_FLAT;
			/* 依据约束条件控制天窗开关 */
			if (odt != (*it)->odt) {
				_gLog.Write("OBSS[%s:all] enter %s duration", param->gid.c_str(),
						TypeObservationDuration::ToString(odt));
				(*it)->odt = odt;
				if (param->useDomeSlit) {
					if (odt == TypeObservationDuration::ODT_DAYTIME) // 白天: 关闭天窗
						command_slit(param->gid, "", CommandSlit::SLITC_CLOSE);
					else if (param->robotic && (*it)->safe) // 当气象条件满足时, 打开天窗
						command_slit(param->gid, "", CommandSlit::SLITC_OPEN);
				}

				// 通知观测系统时间改变
				MtxLck lck1(mtx_obss_);
				for (OBSSVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
					if ((*it)->IsMatched(gid, uid)) (*it)->NotifyODT(odt);
				}
			}
		}
	}
}

void GeneralControl::thread_noon() {
	int t;

	while (1) {
		ptime now = second_clock::local_time();
		ptime noon(now.date(), hours(12));
		if ((t = (noon - now).total_seconds()) < 10) t += 86400;
		boost::this_thread::sleep_for(boost::chrono::seconds(t));

		MtxLck lck(mtx_obss_);
		for (OBSSVec::iterator it = obss_.begin(); it != obss_.end(); ) {
			if ((*it)->IsActive()) ++it;
			else it = obss_.erase(it);
		}
	}
}
