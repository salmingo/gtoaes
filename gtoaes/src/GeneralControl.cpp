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
	param_.Load(gConfigPath);
	buftcp_.reset(new char[TCP_PACK_SIZE]);
	bufudp_.reset(new char[UDP_PACK_SIZE]);
	kvProto_    = KvProtocol::Create();
	nonkvProto_ = NonkvProtocol::Create();
	obsPlans_   = ObservationPlan::Create();

	if (!create_all_server()) return false;
	if (param_.ntpEnable) {
		ntp_ = NTPClient::Create(param_.ntpHost.c_str(), 123, param_.ntpDiffMax);
		ntp_->EnableAutoSynch();
	}
	if (param_.dbEnable)
		dbPtr_ = DatabaseCurl::Create(param_.dbUrl);

	return true;
}

void GeneralControl::Stop() {
	interrupt_thread(thrd_netevent_);
	for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it)
		(*it)->Stop();
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

	/* 启动TCP服务 */
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
	/* 启动UDP服务 */
	udps_env_ = UdpSession::Create();
	if (udps_env_->Open(param_.portEnv)) {
		const UdpSession::CBSlot& slot = boost::bind(&receive_environment, this);
	}

	thrd_netevent_.reset(new boost::thread(boost::bind(&GeneralControl::monitor_network_event, this)));
	return true;
}

void GeneralControl::network_accept(const TcpCPtr client, const TcpSPtr server) {
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

void GeneralControl::receive_client(const TcpCPtr client, const error_code& ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = NetworkEvent::Create(client, PEER_MOUNT, ec.value());
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_mount(const TcpCPtr client, const error_code& ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = NetworkEvent::Create(client, PEER_MOUNT, ec.value());
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_camera(const TcpCPtr client, const error_code& ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = NetworkEvent::Create(client, PEER_MOUNT, ec.value());
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_mount_annex(const TcpCPtr client, const error_code& ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = NetworkEvent::Create(client, PEER_MOUNT, ec.value());
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_camera_annex(const TcpCPtr client, const error_code& ec) {
	MtxLck lck(mtx_netev_);
	NetEvPtr netev = NetworkEvent::Create(client, PEER_MOUNT, ec.value());
	queNetEv_.push_back(netev);
	cv_netev_.notify_one();
}

void GeneralControl::receive_environment(const UdpPtr client, const error_code& ec) {
	int n;

	if (client->Read(bufudp_.get(), n)) {
		if (bufudp_[n - 1] == '\n') {
			bufudp_[n - 1] = 0;
			kvbase base = kvProto_->ResolveEnv(bufudp_.get());
			string type = base->type;
			string gid  = base->gid;
			NfEnvPtr nfEnv = find_info_env(gid);

			if (iequals(type, KVTYPE_RAINFALL)) {
				kvrain proto = from_kvbase<kv_proto_rainfall>(base);
				if (nfEnv->rain != proto->value) {
					nfEnv->rain = proto->value;
					//...
				}
			}
			else if (iequals(type, KVTYPE_WIND)) {
				kvwind proto = from_kvbase<kv_proto_wind>(base);
				nfEnv->orient = proto->orient;
				if (nfEnv->speed != proto->speed) {
					nfEnv->speed = proto->speed;
					//...
				}
			}
			else if (iequals(type, KVTYPE_CLOUD)) {
				kvcloud proto = from_kvbase<kv_proto_cloud>(base);
				if (nfEnv->cloud != proto->value) {
					nfEnv->cloud = proto->value;
					//...
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息机制 -----------------*/
void GeneralControl::erase_tcpclient(TcpCVec& buff, const TcpCPtr client) {
	TcpCVec::iterator it;
	for (it = buff.begin(); it != buff.end() && (*it) != client; ++it);
	if (it != buff.end()) buff.erase(it);
}

void GeneralControl::on_receive_client(const TcpCPtr client) {
	resolve_protocol(client, PEER_CLIENT);
}

void GeneralControl::on_receive_mount(const TcpCPtr client) {
	resolve_protocol(client, PEER_MOUNT);
}

void GeneralControl::on_receive_camera(const TcpCPtr client) {
	resolve_protocol(client, PEER_CAMERA);
}

void GeneralControl::on_receive_mount_annex(const TcpCPtr client) {
	resolve_protocol(client, PEER_MOUNT_ANNEX);
}

void GeneralControl::on_receive_camera_annex(const TcpCPtr client) {
	resolve_protocol(client, PEER_CAMERA_ANNEX);
}

void GeneralControl::on_close_client(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_client_);
	erase_tcpclient(tcpc_client_, client);
}

void GeneralControl::on_close_mount(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_mount_);
	erase_tcpclient(tcpc_mount_, client);
}

void GeneralControl::on_close_camera(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_camera_);
	erase_tcpclient(tcpc_camera_, client);
}

void GeneralControl::on_close_mount_annex(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_mount_annex_);
	erase_tcpclient(tcpc_mount_annex_, client);
}

void GeneralControl::on_close_camera_annex(const TcpCPtr client) {
	MtxLck lck(mtx_tcpc_camera_annex_);
	erase_tcpclient(tcpc_camera_annex_, client);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 解析/执行通信协议 -----------------*/
void GeneralControl::resolve_protocol(const TcpCPtr client, int peer) {
	const char term[] = "\n";	// 信息结束符: 换行
	const char prefix[] = "g#";	// 非键值对格式的引导符
	int lenTerm = strlen(term);		// 结束符长度
	int lenPre  = strlen(prefix);	// 引导符长度
	int pos;
	bool success(true);

	while (client->IsOpen() && (pos = client->Lookup(term, lenTerm)) >= 0) {
		client->Read(buftcp_.get(), pos + lenTerm);
		buftcp_[pos] = 0;
		if (strstr(buftcp_.get(), prefix)) {// 非键值对协议
			nonkvbase base = nonkvProto_->Resove(buftcp_.get() + lenPre);
			if ((success = base.unique())) {
				if      (peer == PEER_MOUNT)       process_nonkv_mount      (base, client);
				else if (peer == PEER_MOUNT_ANNEX) process_nonkv_mount_annex(base, client);
			}
		}
		else {// 键值对协议
			if      (peer == PEER_CLIENT)       success = resolve_kv_client      (client);
			else if (peer == PEER_MOUNT)        success = resolve_kv_mount       (client);
			else if (peer == PEER_CAMERA)       success = resolve_kv_camera      (client);
			else if (peer == PEER_MOUNT_ANNEX)  success = resolve_kv_mount_annex (client);
			else if (peer == PEER_CAMERA_ANNEX) success = resolve_kv_camera_annex(client);
		}

		if (!success) {
			_gLog.Write(LOG_FAULT, "File[%s], Line[%s], Peer Type[%s]. protocol resolver failed",
					__FILE__, __LINE__,
					peer == PEER_CLIENT ? "Client" :
						(peer == PEER_MOUNT ? "Mount" :
							(peer == PEER_CAMERA ? "Camera" :
								(peer == PEER_MOUNT_ANNEX ? "Mount-Annex" : "Camera-Annex"))));
			client->Close();
		}
	}
}

bool GeneralControl::resolve_kv_client(const TcpCPtr client) {
	/*
	 * 客户端接收到的信息, 分为两种处理方式:
	 * 1. 本地处理
	 * 2. 投递给观测系统
	 */
	kvbase base = kvProto_->ResolveClient(buftcp_.get());
	if (!base.unique())
		return false;

	string type = base->type;
	string gid  = base->gid;
	string uid  = base->uid;

	if (iequals(type, KVTYPE_APPPLAN) || iequals(type, KVTYPE_IMPPLAN)) {
		/*>!!!!!! 新的观测计划 !!!!!!<*/
		ObsPlanItemPtr plan;
		bool opNow = (type[0] == 'a' || type[0] == 'A') ? 0 : 1;
		plan = opNow ? from_kvbase<kv_proto_implement_plan>(base)->plan
				: from_kvbase<kv_proto_append_plan>(base)->plan;
		if (plan->CompleteCheck()) {
			obsPlans_->AddPlan(plan);	// 计划加入队列

			if (opNow)
				tryto_implement_plan(plan);
		}
		else {
			_gLog.Write(LOG_FAULT, "plan[%s] couldn't pass validity check", plan->plan_sn.c_str());
		}
	}
	else if (iequals(type, KVTYPE_ABTPLAN)) {
		/*>!!!!!! 中止观测计划 !!!!!!<*/
		tryto_abort_plan(from_kvbase<kv_proto_abort_plan>(base)->plan_sn);
	}
	/////////////////////////////////////////////////////////////////////////
	/*>!!!!!! 检查观测计划 !!!!!!<*/
	else if (iequals(type, KVTYPE_CHKPLAN)) {// 查看观测计划状态
		string plan_sn = from_kvbase<kv_proto_check_plan>(base)->plan_sn;
		ObsPlanItemPtr plan = obsPlans_->Find(plan_sn);
		kvplan proto_rsp = boost::make_shared<kv_proto_plan>();
		const char* s;
		int n;

		proto_rsp->plan_sn = plan_sn;
		if (plan.use_count())
			proto_rsp->state = plan->state;
		else
			proto_rsp->state = StateObservationPlan::OBSPLAN_ERROR;
		s = kvProto_->CompactPlan(proto_rsp, n);
		client->Write(s, n);
	} // if (iequals(type, KVTYPE_CHKPLAN))
	/////////////////////////////////////////////////////////////////////////
	/*>!!!!!! 开关天窗 !!!!!!<*/
	else if (iequals(type, KVTYPE_SLIT)) {
		const OBSSParam* param = param_.GetParamOBSS(gid);
		if (param) {

		}
		else {
			_gLog.Write(LOG_FAULT, "not found any setting for ObservationSsytem[%s:%s]",
					gid.c_str());
		}
	}
	/////////////////////////////////////////////////////////////////////////
	/*>!!!!!! 投递到观测系统 !!!!!!<*/
	else {
		MtxLck lck(mtx_obss_);
		int matched(0);
		for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
			if ((matched = (*it)->IsMatched(gid, uid)))
				(*it)->NotifyKVProtocol(base);
		}
	}

	return true;
} // void GeneralControl::process_kv_client(kvbase proto, const TcpCPtr client)

bool GeneralControl::resolve_kv_mount(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveClient(buftcp_.get());
	if (!base.unique())
		return false;

	// kv协议的转台连接耦合到观测系统
	string gid = base->gid;
	string uid = base->uid;
	if (gid.empty() || uid.empty()) {
		_gLog.Write(LOG_FAULT, "requires non-empty mount IDs[%s:%s]",
				gid.c_str(), uid.c_str());
		client->Close();
	}
	else {
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss->CoupleMount(client, TypeNetworkDevice::NETDEV_KV)) {
			MtxLck lck(mtx_tcpc_mount_);
			erase_tcpclient(tcpc_mount_, client);
		}
	}

	return true;
}

bool GeneralControl::resolve_kv_camera(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveClient(buftcp_.get());
	if (!base.unique())
		return false;

	// kv协议的相机连接耦合到观测系统
	string gid  = base->gid;
	string uid  = base->uid;
	string cid  = base->cid;
	if (gid.empty() || uid.empty() || cid.empty()) {
		_gLog.Write(LOG_FAULT, "requires non-empty camera IDs[%s:%s:%s]",
				gid.c_str(), uid.c_str(), cid.c_str());
		client->Close();
	}
	else {
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss->CoupleCamera(client, cid)) {
			MtxLck lck(mtx_tcpc_camera_);
			erase_tcpclient(tcpc_camera_, client);
		}
	}
	return true;
}

bool GeneralControl::resolve_kv_mount_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveClient(buftcp_.get());
	if (!base.unique())
		return false;

	return true;
}

bool GeneralControl::resolve_kv_camera_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveClient(buftcp_.get());
	if (!base.unique())
		return false;

	return true;
}

void GeneralControl::process_nonkv_mount(nonkvbase base, const TcpCPtr client){

}

void GeneralControl::process_nonkv_mount_annex(nonkvbase base, const TcpCPtr client){

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
			found = is_valid_plantime(plan, now)
					&& obss->IsSafePoint(plan, now);
		}
	}
	return plan;
}

void GeneralControl::tryto_implement_plan(ObsPlanItemPtr plan) {
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
		for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1 && prio_min > 0; ++it) {
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

void GeneralControl::tryto_abort_plan(const string& plan_sn) {
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
			obss->NotifyKVProtocol(to_kvbase(boost::make_shared<kv_proto_abort_plan>()));
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
	ObsSysVec::iterator it;
	for (it = obss_.begin(); it != obss_.end() && !(*it)->IsMatched(gid, uid); ++it);
	if (it != obss_.end())
		obss = *it;
	else {
		/* 新建观测系统 */
		_gLog.Write("try to create ObservationSystem[%s:%s]",
				gid.c_str(), uid.c_str());
		obss = ObservationSystem::Create(gid, uid);
		const OBSSParam* param = param_.GetParamOBSS(gid);
		if (param) {
			const ObservationSystem::CBSlot& slot = boost::bind(&GeneralControl::acquire_new_plan, this, _1);
			obss->RegisterAcquirePlan(slot);
			obss->SetParameter(param);
			obss->SetDBPtr(dbPtr_);
			if (obss->Start())
				obss_.push_back(obss);
		}
		else {
			_gLog.Write(LOG_FAULT, "not found any setting for ObservationSystem[%s:%s]",
					gid.c_str(), uid.c_str());
		}
	}
	return obss;
}

//////////////////////////////////////////////////////////////////////////////
GeneralControl::NfEnvPtr GeneralControl::find_info_env(const string& gid) {
	GeneralControl envPtr;

	return envPtr;
}

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
void GeneralControl::monitor_network_event() {
	boost::mutex mtx;
	MtxLck lck(mtx);
	NetEvPtr netEvPtr;

	while (1) {
		cv_netev_.wait(lck);

		while (queNetEv_.size()) {
			{// 取队首网络事件
				MtxLck lck_net(mtx_netev_);
				netEvPtr = queNetEv_.front();
				queNetEv_.pop_front();
			}
			switch(netEvPtr->peer) {
			case PEER_MOUNT:
				if (netEvPtr->type)
					on_close_mount(netEvPtr->client);
				else if (netEvPtr->client->IsOpen())
					on_receive_mount(netEvPtr->client);
				break;
			case PEER_CAMERA:
				if (netEvPtr->type)
					on_close_camera(netEvPtr->client);
				else if (netEvPtr->client->IsOpen())
					on_receive_camera(netEvPtr->client);
				break;
			case PEER_MOUNT_ANNEX:
				if (netEvPtr->type)
					on_close_mount_annex(netEvPtr->client);
				else if (netEvPtr->client->IsOpen())
					on_receive_mount_annex(netEvPtr->client);
				break;
			case PEER_CAMERA_ANNEX:
				if (netEvPtr->type)
					on_close_camera_annex(netEvPtr->client);
				else if (netEvPtr->client->IsOpen())
					on_receive_camera_annex(netEvPtr->client);
				break;
			default:
				if (netEvPtr->type)
					on_close_client(netEvPtr->client);
				else if (netEvPtr->client->IsOpen())
					on_receive_client(netEvPtr->client);
				break;
			}
		}
	}
}
