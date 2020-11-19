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
	string name = DAEMON_NAME;
	if (!MessageQueue::Start(name.c_str())) return false;

	param_.Load(gConfigPath);
	bufUdp_.reset(new char[UDP_PACK_SIZE]);

	if (!create_all_server()) return false;
	if (param_.ntpEnable) {
		ntp_ = NTPClient::Create(param_.ntpHost.c_str(), 123, param_.ntpDiffMax);
		ntp_->EnableAutoSynch();
	}
	if (param_.dbEnable) dbPtr_ = DatabaseCurl::Create(param_.dbUrl);

	return true;
}

void GeneralControl::Stop() {
	MessageQueue::Stop();
	interrupt_thread(thrd_odt_);
	interrupt_thread(thrd_nfEnvChanged_);

	for (OBSSVec::iterator it = obss_.begin(); it != obss_.end(); ++it)
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
	if (       (ec = create_server(&tcpS_client_,      param_.portClient))
			|| (ec = create_server(&tcpS_mount_,       param_.portMount))
			|| (ec = create_server(&tcpS_camera_,      param_.portCamera))
			|| (ec = create_server(&tcpS_mountAnnex_,  param_.portMountAnnex))
			|| (ec = create_server(&tcpS_cameraAnnex_, param_.portCameraAnnex))) {
		_gLog.Write(LOG_FAULT, "File[%s], Line[%d]. ErrorCode<%d>", __FILE__, __LINE__, ec);
		return false;
	}
	/* 启动UDP服务 */
	udpS_env_ = UdpSession::Create();
	if (udpS_env_->Open(param_.portEnv)) {
		const UdpSession::CBSlot& slot = boost::bind(&GeneralControl::receive_from_env, this, _1, _2);
		udpS_env_->RegisterRead(slot);
	}

	thrd_odt_.reset(new boost::thread(boost::bind(&GeneralControl::thread_odt, this)));
	thrd_nfEnvChanged_.reset(new boost::thread(boost::bind(&GeneralControl::thread_nfenv_changed, this)));
	return true;
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

void GeneralControl::receive_from_env(const UdpPtr client, const error_code& ec) {
	int n;

	if (client->Read(bufUdp_.get(), n) && bufUdp_[n - 1] == '\n') {
		bufUdp_[n - 1] = 0;
		/* 解析气象信息 */
		kvbase base = kvProto_->ResolveEnv(bufUdp_.get());
		string type = base->type;
		string gid  = base->gid;
		NfEnvPtr nfEnv = find_info_env(gid);
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
			cv_nfEnvChanged_.notify_one();
		}
	}
}

void GeneralControl::erase_coupled_tcp(const TcpCPtr client) {
	MtxLck lck(mtx_tcpC_buff_);
	TcpCVec::iterator it, end = tcpC_buff_.end();
	for (it = tcpC_buff_.begin(); it != end && (*it) != client; ++it);
	if (it != end) tcpC_buff_.erase(it);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 解析/执行通信协议 -----------------*/
void GeneralControl::resolve_from_peer(const TcpCPtr client, int peer) {
	const char prefix[] = "g#";	// 非键值对格式的引导符
	int lenPre  = strlen(prefix);	// 引导符长度

	if (strstr(bufTcp_.get(), prefix)) {// 非键值对协议
		nonkvbase base = nonkvProto_->Resove(bufTcp_.get() + lenPre);
		if (base.unique()) {
			if      (peer == PEER_MOUNT)       process_nonkv_mount      (base, client);
			else if (peer == PEER_MOUNT_ANNEX) process_nonkv_mount_annex(base, client);
		}
		else {
			_gLog.Write(LOG_FAULT, "unknown protocol from %s: %s",
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
		_gLog.Write(LOG_FAULT, "unknown protocol from client: %s", bufTcp_.get());
		client->Close();
	}
	else {
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
				if (opNow) try_implement_plan(plan);
			}
			else {
				_gLog.Write(LOG_FAULT, "plan[%s] couldn't pass validity check", plan->plan_sn.c_str());
			}
		}
		else if (iequals(type, KVTYPE_ABTPLAN)) {
			/*>!!!!!! 中止观测计划 !!!!!!<*/
			try_abort_plan(from_kvbase<kv_proto_abort_plan>(base)->plan_sn);
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
			if (plan.use_count()) proto_rsp->state = plan->state;
			else proto_rsp->state = StateObservationPlan::OBSPLAN_ERROR;
			s = kvProto_->CompactPlan(proto_rsp, n);
			client->Write(s, n);
		} // if (iequals(type, KVTYPE_CHKPLAN))
		/////////////////////////////////////////////////////////////////////////
		/*>!!!!!! 投递到观测系统 !!!!!!<*/
		else {
			MtxLck lck(mtx_obss_);
			int matched(0);
			for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
				if ((matched = (*it)->IsMatched(gid, uid))) (*it)->NotifyKVProtocol(base);
			}
		}
	}
} // void GeneralControl::process_kv_client(kvbase proto, const TcpCPtr client)

void GeneralControl::resolve_kv_mount(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveMount(bufTcp_.get());
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from mount: %s", bufTcp_.get());
		client->Close();
	}
	else {
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
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleMount(client))) {// P2P或P2H模式
					obss->NotifyKVProtocol(base);
					if (mode == 1) erase_coupled_tcp(client);
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple mount with OBSS[%s:%s]", gid.c_str(), uid.c_str());
					client->Close();
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "not found OBSS[%s:%s]", gid.c_str(), uid.c_str());
				client->Close();
			}
		}
	}
}

void GeneralControl::resolve_kv_camera(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveCamera(bufTcp_.get());
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from camera: %s", bufTcp_.get());
		client->Close();
	}
	else {
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

			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleCamera(client, cid))) {// P2P或P2H模式
					obss->NotifyKVProtocol(base);
					if (mode == 1) erase_coupled_tcp(client);
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple camera[%s] with OBSS[%s:%s]",
							cid.c_str(), gid.c_str(), uid.c_str());
					client->Close();
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "not found OBSS[%s:%s]", gid.c_str(), uid.c_str());
				client->Close();
			}
		}
	}
}

void GeneralControl::resolve_kv_mount_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveMount(bufTcp_.get());
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from mount-annex: %s", bufTcp_.get());
		client->Close();
	}
	else {
		// kv协议的转台连接耦合到观测系统
		string gid = base->gid;
		string uid = base->uid;
		if (gid.empty() || uid.empty()) {
			_gLog.Write(LOG_FAULT, "requires non-empty mount-annex IDs[%s:%s]",
					gid.c_str(), uid.c_str());
			client->Close();
		}
		else {
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleMountAnnex(client))) {// P2P或P2H模式
					obss->NotifyKVProtocol(base);
					if (mode == 1) erase_coupled_tcp(client);
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple mount-annex with OBSS[%s:%s]", gid.c_str(), uid.c_str());
					client->Close();
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "not found OBSS[%s:%s]", gid.c_str(), uid.c_str());
				client->Close();
			}
		}
	}
}

void GeneralControl::resolve_kv_camera_annex(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveCamera(bufTcp_.get());
	if (!base.unique()) {
		_gLog.Write(LOG_FAULT, "unknown protocol from camera-annex: %s", bufTcp_.get());
		client->Close();
	}
	else {
		// kv协议的相机连接耦合到观测系统
		string gid  = base->gid;
		string uid  = base->uid;
		string cid  = base->cid;
		if (gid.empty() || uid.empty() || cid.empty()) {
			_gLog.Write(LOG_FAULT, "requires non-empty camera-annex IDs[%s:%s:%s]",
					gid.c_str(), uid.c_str(), cid.c_str());
			client->Close();
		}
		else {
			ObsSysPtr obss = find_obss(gid, uid);
			if (obss.use_count()) {
				int mode;
				if ((mode = obss->CoupleCameraAnnex(client, cid))) {// P2P或P2H模式
					obss->NotifyKVProtocol(base);
					if (mode == 1) erase_coupled_tcp(client);
				}
				else {
					_gLog.Write(LOG_FAULT, "failed to couple camera-annex[%s] with OBSS[%s:%s]",
							cid.c_str(), gid.c_str(), uid.c_str());
					client->Close();
				}
			}
			else {
				_gLog.Write(LOG_FAULT, "not found OBSS[%s:%s]", gid.c_str(), uid.c_str());
				client->Close();
			}
		}
	}
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
	OBSSVec::iterator it, end = obss_.end();
	for (it = obss_.begin(); it != end && !(*it)->IsMatched(gid, uid); ++it);
	if (it != end) obss = *it;
	else {
		/* 新建观测系统 */
		const OBSSParam* param = param_.GetParamOBSS(gid);
		if (param) {
			_gLog.Write("try to create ObservationSystem[%s:%s]",
					gid.c_str(), uid.c_str());

			const ObservationSystem::CBSlot& slot = boost::bind(&GeneralControl::acquire_new_plan, this, _1);
			obss = ObservationSystem::Create(gid, uid);
			obss->RegisterAcquirePlan(slot);
			obss->SetParameter(param);
			obss->SetDBPtr(dbPtr_);
			if (obss->Start()) obss_.push_back(obss);
			create_info_env(param);	// 检查并创建新的环境信息
		}
		else {
			_gLog.Write(LOG_FAULT, "not found any setting for ObservationSystem[%s:%s]",
					gid.c_str(), uid.c_str());
		}
	}
	return obss;
}

void GeneralControl::command_slit(const string& gid, const string& uid, int cmd) {
	MtxLck lck(mtx_obss_);
	int matched(0);
	kvslit proto = boost::make_shared<kv_proto_slit>();
	kvbase base;
	proto->command = cmd;
	base = to_kvbase(proto);
	for (OBSSVec::iterator it = obss_.begin(); it != obss_.end() && matched != 1; ++it) {
		if ((matched = (*it)->IsMatched(gid, uid))) (*it)->NotifyKVProtocol(base);
	}
}

/*----------------- 环境信息 -----------------*/
GeneralControl::NfEnvPtr GeneralControl::find_info_env(const string& gid) {
	MtxLck lck(mtx_nfEnv_);
	NfEnvPtr env;
	NfEnvVec::iterator it, end = nfEnv_.end();
	for (it = nfEnv_.begin(); it != end && (*it)->gid != gid; ++it);
	if (it != end) env = *it;
	else {
		/* 新建环境气象 */
		_gLog.Write("creating Environment Information[%s]", gid.c_str());
		env = EnvInfo::Create(gid);
		nfEnv_.push_back(env);
	}

	return env;
}

void GeneralControl::create_info_env(const OBSSParam* param) {
	MtxLck lck(mtx_nfEnv_);
	string gid = param->gid;
	NfEnvVec::iterator it, end = nfEnv_.end();
	for (it = nfEnv_.begin(); it != end && (*it)->gid != gid; ++it);
	if (it == end) {
		NfEnvPtr env = EnvInfo::Create(gid);
		env->param = param;
		nfEnv_.push_back(env);
	}
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 多线程 -----------------*/
/* 处理网络事件 */
void GeneralControl::thread_nfenv_changed() {
	boost::mutex mtx;
	MtxLck lck(mtx);
	NfEnvPtr nfEnv;
	string gid;

	while (1) {
		cv_nfEnvChanged_.wait(lck);

		while (que_nfEnv_.size()) {
			{// 取队首事件
				MtxLck lck_net(mtx_tcpRcv_);
				nfEnv = que_nfEnv_.front();
				que_nfEnv_.pop_front();
			}

			/* 响应变化: 关闭天窗 */
			gid = nfEnv->gid;
			if (!(nfEnv->param || (nfEnv->param = param_.GetParamOBSS(gid))))
				_gLog.Write(LOG_FAULT, "undefined Environment[%s]", gid.c_str());
			else {// 安全性判定
				const OBSSParam* param = nfEnv->param;
				bool safe = !((param->useRainfall && nfEnv->rain)	// 降水
						|| (param->useWindSpeed && nfEnv->speed > param->maxWindSpeed)  // 大风
						|| (param->useCloudCamera && nfEnv->cloud > param->maxCloudPerent)); // 多云
				if (safe != nfEnv->safe) {
					_gLog.Write("Environment[%s] shows %s", safe ? "safe" : "!!! DANGER !!!");
					nfEnv->safe = safe;
					if (!safe && param->useDomeSlit) command_slit(gid, "", CommandSlit::SLITC_CLOSE);
				}
			}
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
			ats.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timeZone);
			lmst = ats.LocalMeanSiderealTime(mjd, param->siteLon * D2R);
			ats.Eq2Horizon(lmst - ra, dec, azi, alt);
			alt *= R2D;
			if (alt > param->altDay)        odt = TypeObservationDuration::ODT_DAYTIME;
			else if (alt < param->altNight) odt = TypeObservationDuration::ODT_NIGHT;
			else                            odt = TypeObservationDuration::ODT_FLAT;

			if (odt != (*it)->odt) {
				_gLog.Write("OBSS[%s:all] enter %s duration", param->gid.c_str(),
						TypeObservationDuration::ToString(odt));
				(*it)->odt = odt;
				if (param->useDomeSlit) {
					if (odt == TypeObservationDuration::ODT_DAYTIME) // 白天: 关闭天窗
						command_slit(param->gid, "", CommandSlit::SLITC_CLOSE);
					else if ((*it)->safe) // 当气象条件满足时, 打开天窗
						command_slit(param->gid, "", CommandSlit::SLITC_OPEN);
				}
			}
		}
	}
}
