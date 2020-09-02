/**
 * @class GeneralControl 总控服务器
 * @version 0.1
 * @date 2019-10-22
 */
#include <algorithm>
#include <cstdio>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include "globaldef.h"
#include "GeneralControl.h"
#include "GLog.h"
#include "ADefine.h"
#include "AstroDeviceDef.h"

using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::filesystem;
using namespace boost::placeholders;

// 观测系统区别:
// 1. 海南仅有一个圆顶; 怀柔有多圆顶, 且每个圆顶内有多个望远镜
// 2. 海南不需要全自动, 由人工操作打开天窗; 怀柔需要全自动
// 3. 怀柔需要自动校正零点
#define OBSS_LOC	1	// 观测系统位置. 1: 海南; 2: 怀柔

GeneralControl::GeneralControl() {
	param_.LoadFile(gConfigPath);
}

GeneralControl::~GeneralControl() {
}

bool GeneralControl::Start() {
	string name = string("msgque_") + DAEMON_NAME;
	register_message();
	if (!MessageQueue::Start(name.c_str())) return false;
	param_.LoadFile(gConfigPath);
	if (!create_server()) return false;
	if (param_.dbEnable) dbt_ = boost::make_shared<DBCurl>(param_.dbUrl);
	if (param_.ntpEnable) {
		ntp_ = make_ntp(param_.ntpHost.c_str(), 123, param_.ntpMaxDiff);
		ntp_->EnableAutoSynch(true);
	}
	thrd_obss_.reset(new boost::thread(boost::bind(&GeneralControl::thread_obss, this)));
	thrd_obsplan_.reset(new boost::thread(boost::bind(&GeneralControl::thread_obsplan, this)));

	return true;
}

void GeneralControl::Stop() {
	MessageQueue::Stop();
	interrupt_thread(thrd_odt_);
	interrupt_thread(thrd_obss_);
	interrupt_thread(thrd_obsplan_);
	stop_obss();
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 网络通信 -------------------*/
//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::create_server() {
	ascproto_ = boost::make_shared<AsciiProtocol>();
	annproto_ = boost::make_shared<AnnexProtocol>();
	netrcv_.reset(new char[TCP_PACK_SIZE]);

	return (create_server(&tcps_client_,  param_.portClient)
			&& create_server(&tcps_mount_,  param_.portMount)
			&& create_server(&tcps_camera_, param_.portCamera)
			&& create_server(&tcps_focus_,  param_.portFocus)
			&& create_server(&tcps_annex_,  param_.portAnnex));
}

bool GeneralControl::create_server(TcpSPtr *server, uint16_t port) {
	const TCPServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	int rslt;
	*server = maketcp_server();
	(*server)->RegisterAccespt(slot);
	rslt = (*server)->CreateServer(port);
	if (rslt) {
		_gLog.Write(LOG_FAULT, NULL, "failed to create network server on port<%u>", port);
	}
	return !rslt;
}

void GeneralControl::network_accept(const TcpCPtr &client, const long ptr) {
	TCPServer* server = (TCPServer*) ptr;

	if (server == tcps_client_.get()) {// 用户
		mutex_lock lck(mtx_tcp_client_);
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_client, this, _1, _2);
		client->RegisterRead(slot);
		client->UseBuffer();
		tcpc_client_.push_back(client);
	}
	else if (server == tcps_mount_.get()) {// 转台
		mutex_lock lck(mtx_tcp_mount_);
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_mount, this, _1, _2);
		client->RegisterRead(slot);
		client->UseBuffer();
		tcpc_mount_.push_back(client);
	}
	else if (server == tcps_camera_.get()) {// 相机
		mutex_lock lck(mtx_tcp_camera_);
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_camera, this, _1, _2);
		client->RegisterRead(slot);
		client->UseBuffer();
		tcpc_camera_.push_back(client);
	}
	else if (server == tcps_focus_.get()) {// 调焦
		if (tcpc_focus_.use_count()) {
			_gLog.Write(LOG_WARN, NULL, "multi connections for focus was rejected");
			client->Close();
		}
		else {
			mutex_lock lck1(mtx_tcp_focus_);
			const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_focus, this, _1, _2);
			client->RegisterRead(slot);
			client->UseBuffer();
			tcpc_focus_ = client;
		}
	}
	else if (server == tcps_annex_.get()) {// 附属设备
		if (tcpc_annex_.unique()) {
			_gLog.Write(LOG_WARN, NULL, "multi connections for annexed device was rejected");
			client->Close();
		}
		else {
			const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_annex, this, _1, _2);
			client->RegisterRead(slot);
			client->UseBuffer();
			tcpc_annex_ = client;

			if (dbt_.unique()) dbt_->UpdateDomeLinked("001", true);
		}
	}
}

void GeneralControl::receive_client(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CLIENT : MSG_RECEIVE_CLIENT, ptr);
}

void GeneralControl::receive_mount(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT : MSG_RECEIVE_MOUNT, ptr);
}

void GeneralControl::receive_camera(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA : MSG_RECEIVE_CAMERA, ptr);
}

void GeneralControl::receive_focus(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_FOCUS : MSG_RECEIVE_FOCUS, ptr);
}

void GeneralControl::receive_annex(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_ANNEX : MSG_RECEIVE_ANNEX, ptr);
}

void GeneralControl::resolve_protocol_ascii(TCPClient* client, int peer) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii()",
					"too long message from %s", peer == PEER_CLIENT ? "CLIENT"
							: (peer == PEER_MOUNT ? "MOUNT" : "CAMERA"));
			client->Close();
		}
		else {// 读取协议内容并解析执行
			client->Read(netrcv_.get(), toread);
			netrcv_[pos] = 0;
			proto = ascproto_->Resolve(netrcv_.get());
			if (!proto.unique()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii()",
						"illegal protocol [%s]", netrcv_.get());
				client->Close();
			}
			else if (peer == PEER_CLIENT) process_protocol_client(proto, client);
			else if (peer == PEER_MOUNT)  process_protocol_mount (proto, client);
			else if (peer == PEER_CAMERA) process_protocol_camera(proto, client);
		}
	}
}

void GeneralControl::resolve_protocol_annex(TCPClient* client, int peer) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	annpbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			string ip = client->GetSocket().remote_endpoint().address().to_string();
			_gLog.Write(LOG_FAULT, "GeneralControl::resolve_protocol_annex()",
					"too long message annex");
			client->Close();
		}
		else {// 读取协议内容并解析执行
			client->Read(netrcv_.get(), toread);
			netrcv_[pos] = 0;
			proto = annproto_->Resolve(netrcv_.get());
			if (!proto.unique()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::resolve_protocol_annex()",
						"illegal protocol [%s]", netrcv_.get());
				client->Close();
			}
			else if (peer == PEER_FOCUS) process_protocol_focus(proto);
			else if (peer == PEER_ANNEX) process_protocol_annex(proto);
		}
	}
}

void GeneralControl::process_protocol_client(apbase proto, TCPClient* client) {
	string type = proto->type;
	string gid = proto->gid;
	string uid = proto->uid;

	if (iequals(type, APTYPE_START))         autobs(gid, uid);
	else if (iequals(type, APTYPE_STOP))     autobs(gid, uid, false);
	else if (iequals(type, APTYPE_LOADPLAN)) cv_loadplan_.notify_one();
	else if (iequals(type, APTYPE_SLIT)) {
		apslit slit = static_pointer_cast<ascii_proto_slit>(proto);
//		if (bool(slit->enable) != nfEnv_.slitEnable && (slit->enable == 0 || slit->enable == 1)) {
//			nfEnv_.slitEnable = slit->enable;
//			_gLog.Write("slit was %s", nfEnv_.slitEnable ? "enabled" : "disabled");
//		}
//		if (nfEnv_.mode == OC_STOP && nfEnv_.slitEnable) command_slit(slit->command);
		command_slit(slit->command);
	}
	else if (obss_.size()) {// 尝试向观测系统投递协议
		mutex_lock lck(mtx_obss_);
		string gid  = proto->gid;
		string uid  = proto->uid;
		for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
			if ((*it)->IsMatched(gid, uid)) (*it)->NotifyAsciiProtocol(proto);
		}
	}
}

void GeneralControl::autobs(const string &gid, const string &uid, bool start) {
	if (gid.empty() && uid.empty()) nfEnv_.mode = start ? OC_START : OC_STOP;
	/* 通知观测改变工作模式 */
	mutex_lock lck(mtx_obss_);
	apbase proto = start ? to_apbase(boost::make_shared<ascii_proto_start>())
			: to_apbase(boost::make_shared<ascii_proto_stop>());
	for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
		if ((*it)->IsMatched(gid, uid)) (*it)->NotifyAsciiProtocol(proto);
	}
	/* 当条件满足时, 关闭天窗 */
//	if (!start && nfEnv_.slitState == SS_OPEN) command_slit(SC_CLOSE);
}

void GeneralControl::command_slit(int cmd) {
	if (tcpc_annex_.unique()) {
//		if ((cmd == SC_CLOSE && nfEnv_.slitState != SS_CLOSED && nfEnv_.slitState != SS_CLOSING)
//			|| (cmd == SC_OPEN && nfEnv_.slitState != SS_OPEN && nfEnv_.slitState != SS_OPENING)) {
			_gLog.Write("try to %s dome slit", cmd == SC_CLOSE ? "close" : "open");
			int n;
//			const char *s = annproto_->CompactSlit(cmd, n);
			const char *s = annproto_->CompactSlit("001", cmd, n);
			tcpc_annex_->Write(s, n);
//		}
	}
#ifdef NDEBUG
	else {
		_gLog.Write(LOG_WARN, NULL, "slit controller was off-line");
	}
#endif
}

void GeneralControl::process_protocol_mount(apbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;

	if (iequals(proto->type, APTYPE_REG) || iequals(proto->type, APTYPE_MOUNT)) {// 关联观测系统与望远镜
		ObsSysPtr obss = find_obss(gid, uid);
		if (!obss.use_count()) client->Close();
		else {
			mutex_lock lck(mtx_tcp_mount_);
			TcpCVec::iterator it;

			for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && (*it).get() != client; ++it);
			if (obss->CoupleMount(*it)) tcpc_mount_.erase(it);
			/*
			 * GeneralControl中可能连续收到多条REG信息
			 */
//			else client->Close();
		}
	}
	else {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_mount()",
				"protocol[%s gid=%s, uid=%s] was rejected", proto->type.c_str(),
				gid.c_str(), uid.c_str());
		client->Close();
	}
}

void GeneralControl::process_protocol_camera(apbase proto, TCPClient* client) {
	string gid = proto->gid;
	string uid = proto->uid;
	string cid = proto->cid;

	if (!iequals(proto->type, APTYPE_REG)) {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_camera()",
				"protocol[%s gid=%s, uid=%s] was rejected", proto->type.c_str(),
				gid.c_str(), uid.c_str());
		client->Close();
	}
	else {// 关联观测系统与望远镜
		ObsSysPtr obss = find_obss(gid, uid);
		if (!obss.use_count() || cid.empty()) client->Close();
		else {
			mutex_lock lck(mtx_tcp_camera_);
			TcpCVec::iterator it;

			for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && (*it).get() != client; ++it);
			if (obss->CoupleCamera(*it, cid)) tcpc_camera_.erase(it);
			else client->Close();
		}
	}
}

void GeneralControl::process_protocol_focus(annpbase proto) {
	if (!iequals(proto->type, APTYPE_FOCUS)) {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_focus()",
				"protocol[%s] was rejected", proto->type.c_str());
		tcpc_focus_->Close();
	}
	else {
		annpfocus focus = static_pointer_cast<annexproto_focus>(proto);
		string gid = focus->gid;
		string uid = focus->uid;
		string cid = focus->cid;
//		ObsSysPtr obss = find_obss(gid, uid, false);
		ObsSysPtr obss = find_obss(gid, uid);
		if (obss.use_count()) {
			int rslt = obss->CoupleFocus(tcpc_focus_, cid);
			if (rslt == 1)  obss->NotifyFocus(cid, focus->position);
			else if (!rslt) tcpc_focus_->Close();
		}
	}
}

void GeneralControl::process_protocol_annex(annpbase proto) {
	string type = proto->type;

	if (iequals(type, APTYPE_RAIN)) {
		annprain rain = static_pointer_cast<annexproto_rain>(proto);
		if (rain->value != nfEnv_.rain) {
			change_skystate(rain->value);

			if (dbt_.unique()) {
				string tzt = to_iso_string(second_clock::local_time());
				dbt_->UpdateRainfall("001", tzt, rain->value);
			}
		}
	}
	else if (iequals(type, APTYPE_SLIT)) {
		annpslit slit = static_pointer_cast<annexproto_slit>(proto);
		if (slit->state != nfEnv_.slitState) {
			change_slitstate(slit->state);

			if (dbt_.unique()) {
				string tzt = to_iso_string(second_clock::local_time());
				dbt_->UpdateDomeState("001", tzt, slit->state, 0);
			}
		}
	}
	else {
		_gLog.Write(LOG_FAULT, "GeneralControl::process_protocol_annex()",
				"protocol[%s] was rejected", proto->type.c_str());
		tcpc_annex_->Close();
	}
}

void GeneralControl::change_slitstate(int state) {
	if (state < SS_ERROR || state > SS_FREEZE) {
		_gLog.Write(LOG_WARN, "GeneralControl::change_slitstate()", "undefined slit state[=%d]", state);
		tcpc_annex_->Close();
	}
	else {
		_gLog.Write("SLIT was %s", SLIT_STATE_STR[state]);
		nfEnv_.slitState = state;
		/* 当天窗状态发生变化时, 望远镜复位, 触发中止观测计划 */
		if (state != SS_OPEN && state != SS_CLOSED) {
			mutex_lock lck(mtx_obss_);
			apbase proto = to_apbase(boost::make_shared<ascii_proto_park>());
			for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
				(*it)->NotifyAsciiProtocol(proto);
			}
		}
	}
}

void GeneralControl::change_skystate(int rain) {
	_gLog.Write("Sky was %s", rain == RAIN_CLEAR ? "Clear" :
			(rain == RAIN_RAINY ? "Rainy" : "Unknown"));
	nfEnv_.rain = rain;
	/*
	 * - 当发生降水时, 关闭天窗
	 * - 当停止降水时, 记录时标
	 */
//	if (rain == RAIN_RAINY && nfEnv_.slitState == SS_OPEN) command_slit(SC_CLOSE);
//	else if (rain == RAIN_CLEAR) nfEnv_.tmclear = second_clock::universal_time();
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 网络通信 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/*------------------- 消息机制 -------------------*/
//////////////////////////////////////////////////////////////////////////////
void GeneralControl::register_message() {
	const CBSlot& slot11 = boost::bind(&GeneralControl::on_receive_client,  this, _1, _2);
	const CBSlot& slot12 = boost::bind(&GeneralControl::on_receive_mount,   this, _1, _2);
	const CBSlot& slot13 = boost::bind(&GeneralControl::on_receive_camera,  this, _1, _2);
	const CBSlot& slot14 = boost::bind(&GeneralControl::on_receive_focus,   this, _1, _2);
	const CBSlot& slot15 = boost::bind(&GeneralControl::on_receive_annex,   this, _1, _2);

	const CBSlot& slot21 = boost::bind(&GeneralControl::on_close_client,    this, _1, _2);
	const CBSlot& slot22 = boost::bind(&GeneralControl::on_close_mount,     this, _1, _2);
	const CBSlot& slot23 = boost::bind(&GeneralControl::on_close_camera,    this, _1, _2);
	const CBSlot& slot24 = boost::bind(&GeneralControl::on_close_focus,     this, _1, _2);
	const CBSlot& slot25 = boost::bind(&GeneralControl::on_close_annex,     this, _1, _2);

	RegisterMessage(MSG_RECEIVE_CLIENT,  slot11);
	RegisterMessage(MSG_RECEIVE_MOUNT,   slot12);
	RegisterMessage(MSG_RECEIVE_CAMERA,  slot13);
	RegisterMessage(MSG_RECEIVE_FOCUS,   slot14);
	RegisterMessage(MSG_RECEIVE_ANNEX,   slot15);

	RegisterMessage(MSG_CLOSE_CLIENT,    slot21);
	RegisterMessage(MSG_CLOSE_MOUNT,     slot22);
	RegisterMessage(MSG_CLOSE_CAMERA,    slot23);
	RegisterMessage(MSG_CLOSE_FOCUS,     slot24);
	RegisterMessage(MSG_CLOSE_ANNEX,     slot25);
}

void GeneralControl::on_receive_client(const long addr, const long) {
	resolve_protocol_ascii((TCPClient*) addr, PEER_CLIENT);
}

void GeneralControl::on_receive_mount(const long addr, const long) {
	resolve_protocol_ascii((TCPClient*) addr, PEER_MOUNT);
}

void GeneralControl::on_receive_camera(const long addr, const long) {
	resolve_protocol_ascii((TCPClient*) addr, PEER_CAMERA);
}

void GeneralControl::on_receive_focus(const long addr, const long) {
	resolve_protocol_annex((TCPClient*) addr, PEER_FOCUS);
}

void GeneralControl::on_receive_annex(const long addr, const long) {
	resolve_protocol_annex((TCPClient*) addr, PEER_ANNEX);
}

void GeneralControl::on_close_client(const long addr, const long) {
	mutex_lock lck(mtx_tcp_client_);
	TCPClient* ptr = (TCPClient*) addr;
	TcpCVec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_client_.end()) tcpc_client_.erase(it);
}

void GeneralControl::on_close_mount(const long addr, const long) {
	mutex_lock lck(mtx_tcp_mount_);
	TCPClient* ptr = (TCPClient*) addr;
	TcpCVec::iterator it;

	for (it = tcpc_mount_.begin(); it != tcpc_mount_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_mount_.end()) tcpc_mount_.erase(it);
}

void GeneralControl::on_close_camera(const long addr, const long) {
	mutex_lock lck(mtx_tcp_camera_);
	TCPClient* ptr = (TCPClient*) addr;
	TcpCVec::iterator it;

	for (it = tcpc_camera_.begin(); it != tcpc_camera_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_camera_.end()) tcpc_camera_.erase(it);
}

void GeneralControl::on_close_focus(const long addr, const long) {
	if (tcpc_focus_.use_count() > 1) {
		mutex_lock lck(mtx_obss_);
		for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
			(*it)->DecoupleFocus();
		}
	}
	_gLog.Write("focus connection was broken");
	tcpc_focus_.reset();
}

void GeneralControl::on_close_annex(const long addr, const long) {
	_gLog.Write("connection for annexed devices was broken");
	tcpc_annex_.reset();

	if (dbt_.unique()) dbt_->UpdateDomeLinked("001", false);
}
//////////////////////////////////////////////////////////////////////////////
/*------------------- 消息机制 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测系统 -------------------*/
//////////////////////////////////////////////////////////////////////////////
ObsSysPtr GeneralControl::find_obss(const string &gid, const string &uid, bool create) {
	ObsSysPtr obss;
	ObssTraitPtr trait = param_.GetObservationSystemTrait(gid);

	if (gid.empty() || uid.empty()) {
		_gLog.Write(LOG_FAULT, "find_obss()", "illegal ID[%s:%s]", gid.c_str(), uid.c_str());
	}
	else if (!trait.use_count()) {
		_gLog.Write(LOG_FAULT, "find_obss()", "group[%s] was not set in configuration file", gid.c_str());
	}
	else {
		mutex_lock lck(mtx_obss_);
		ObsSysVec::iterator it;
		for (it = obss_.begin(); it != obss_.end() && !(*it)->IsMatched(gid, uid); ++it);
		if (it != obss_.end()) obss = *it;
		else if (create) obss = make_obss(gid, uid);
	}

	if (obss.unique()) {
		if (!obss->Start()) {
			obss->Stop();
			obss.reset();
		}
		else {
			mutex_lock lck(mtx_obss_);
			obss_.push_back(obss);

			obss->SetGeosite(trait->sitename, trait->lon, trait->lat, trait->alt, trait->timezone);
			obss->SetElevationLimit(param_.GetMountSafeLimit(gid, uid));
			const ObservationSystem::AcquirePlanSlot& slot =
					boost::bind(&GeneralControl::acquire_new_plan, this, _1, _2);
			obss->RegisterAcquirePlan(slot);
			if (param_.dbEnable) obss->SetDBUrl(param_.dbUrl.c_str());
			/* 启动天文时段计算 */
			if (!thrd_odt_.unique()) {
				ats_.SetSite(trait->lon, trait->lat, trait->alt, trait->timezone);
				thrd_odt_.reset(new boost::thread(boost::bind(&GeneralControl::thread_odt, this)));
			}
		}
	}

	return obss;
}

void GeneralControl::stop_obss() {
	for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it)
		(*it)->Stop();
}

void GeneralControl::thread_obss() {
	boost::chrono::minutes period(1);

	while(1) {
		boost::this_thread::sleep_for(period);
		mutex_lock lck(mtx_obss_);
		for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end();) {
			if ((*it)->IsAlive()) ++it;
			else {
				(*it)->Stop();
				it = obss_.erase(it);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测系统 -------------------*/
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测计划 -------------------*/
//////////////////////////////////////////////////////////////////////////////
void GeneralControl::obsplan_calibration() {
	mutex_lock lck(mtx_obss_);
	string gid, uid;
	ptime btime = second_clock::universal_time();
	ptime etime = btime + hours(20);
	ptime::time_duration_type tdt = btime.time_of_day();
	string plan_sn0;
	format fmt("%02d%02d");
	int id(0);

	fmt % tdt.hours() % tdt.minutes();
	plan_sn0 = to_iso_string(btime.date()) + fmt.str();

	for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
		(*it)->GetID(gid, uid);
		/* 定标计划: 平场 */
		ObsPlanPtr flat = boost::make_shared<ObservationPlan>();
		flat->gid       = gid;
		flat->uid       = uid;
		flat->plan_type = PLANTYPE_MANUAL;
		flat->plan_sn   = plan_sn0 + to_string(IMGTYPE_FLAT * 10000 + ++id);
		flat->btime     = btime;
		flat->etime     = etime;
		flat->imgtype   = "flat";
		flat->expdur    = 5.0;
		flat->frmcnt    = 20;
		flat->update();
		/* 加入计划队列 */
		obsplan_.push_back(flat);
	}
}

bool GeneralControl::scan_obsplan() {
	try {
		mutex_lock lck(mtx_obsplan_);
		obsplan_.clear();
		// 依据当前日期构建文件存储目录
		path pathroot = param_.planPath;
		ptime::date_type today = second_clock::universal_time().date();
		string tdstr = gregorian::to_iso_string(today);
		path subpath = pathroot.append(tdstr);

		// 查找目录内文件, 并逐一解析
		for (directory_iterator x = directory_iterator(subpath); x != directory_iterator(); ++x) {
			resolve_obsplan(x->path().c_str());
		}
		if (obsplan_.size()) {
			_gLog.Write("%d plans were load", obsplan_.size());
			obsplan_calibration();
			resort_obsplan();
		}
	}
	catch (boost::filesystem::filesystem_error &ex) {
#ifdef NDEBUG
		_gLog.Write(LOG_FAULT, "GeneralControl::scan_obsplan()", "%s", ex.what());
#endif
	}
	return obsplan_.size();
}

bool GeneralControl::resolve_obsplan(const char *filepath) {
	const int sizeline = 300;
	char line[sizeline], ch, *token;
	char seps[] = " ";
	int n;
	string sline, mode, btime, etime, bymd, bhms, eymd, ehms;
	FILE *fp = fopen(filepath, "r");

	if (!fp) {
		_gLog.Write(LOG_FAULT, NULL, "failed to open Plan File [%s]", filepath);
		return false;
	}

	while (!feof(fp)) {
		if (NULL == fgets(line, sizeline, fp)) continue;
		n = strlen(line);
		if ((ch = line[n - 1]) == '\n' || ch == '\r') --n;
		if ((ch = line[n - 2]) == '\r') --n;
		line[n] = 0;
		sline = line;

		// 解析文本, 构建观测计划
		ObsPlanPtr plan = boost::make_shared<ObservationPlan>();

		token = strtok(line, seps); plan->gid = token;
		token = strtok(NULL, seps); plan->uid = token;
		token = strtok(NULL, seps); plan->plan_sn = token;
		token = strtok(NULL, seps); mode = token;
		token = strtok(NULL, seps); bymd = token;
		token = strtok(NULL, seps); bhms = token;
		token = strtok(NULL, seps); eymd = token;
		token = strtok(NULL, seps); ehms = token;
		btime= bymd + "T" + bhms;		// 格式: CCYYMMDDThhmmss
		etime= eymd + "T" + ehms;
		plan->imgtype = "object";
		plan->btime   = from_iso_string(btime);
		plan->etime   = from_iso_string(etime);

		if (boost::iequals(mode, "MODE1")) {
			string::size_type pos1, pos2;

			token = strtok(NULL, seps); plan->objname = token;
			token = strtok(NULL, seps); plan->expdur  = atof(token);
			pos1  = sline.find(" 1 ");
			pos2  = sline.find(" 2 ");
			plan->line1     = sline.substr(pos1 + 1, pos2 - pos1 - 1);
			plan->line2     = sline.substr(pos2 + 1);
			plan->plan_type = PLANTYPE_TRACK;
		}
		else {// MODE2 || MODE3
			token = strtok(NULL, seps); plan->coorsys = atoi(token);
			token = strtok(NULL, seps); plan->coor1   = atof(token);
			token = strtok(NULL, seps); plan->coor2   = atof(token);
			if (iequals(mode, "MODE2")) {
				token = strtok(NULL, seps);
				plan->expdur  = atof(token);
			}
			else {// MODE3
				int frmcnt;
				double expdur;
				token = strtok(NULL, seps); plan->expdur = atof(token);
				token = strtok(NULL, seps); frmcnt = atoi(token);
				token = strtok(NULL, seps); expdur = atof(token);
				token = strtok(NULL, seps); frmcnt = atoi(token);
			}
			plan->objname   = "point";
			plan->plan_type = PLANTYPE_POINT;
		}
		plan->update();
		obsplan_.push_back(plan);
	}
	fclose(fp);

	return true;
}

void GeneralControl::resort_obsplan() {
	std::sort(obsplan_.begin(), obsplan_.end(), [](ObsPlanPtr x1, ObsPlanPtr x2) {
		return x1->btime < x2->btime;
	});
}

ObsPlanPtr GeneralControl::acquire_new_plan(const string& gid, const string& uid) {
	ObsPlanPtr plan;
	int odt(nfEnv_.odt), state(nfEnv_.slitState);
	if (!(nfEnv_.slitEnable										// 禁用天窗
			&& ((state != SS_OPEN  && state != SS_CLOSED)		// 天窗状态不是全开或全关
				|| (odt == OD_DAY  && state != SS_CLOSED)		// 白天 : 天窗没有完全关闭
				|| (odt >= OD_FLAT && state != SS_OPEN)))) {	// 夜间 : 天窗没有完全打开
		int iimgtyp, td1, td2;
		string gid1, uid1;
		ptime now = second_clock::universal_time();

		mutex_lock lck(mtx_obsplan_);
		for (ObsPlanVec::iterator it = obsplan_.begin(); !plan.use_count() && it != obsplan_.end(); ++it) {
			iimgtyp = (*it)->iimgtyp;
			gid1 = (*it)->gid;
			uid1 = (*it)->uid;
			if (((gid1 == gid && uid1 == uid) || (uid1.empty() && (gid1.empty() || gid1 == gid)))	// 标志一致
				&& !((odt == OD_DAY && iimgtyp > IMGTYPE_DARK)			// 白天: 图像类型需要天光
					|| (odt == OD_FLAT && iimgtyp != IMGTYPE_FLAT)		// 平场: 非平场类型
					|| (odt == OD_NIGHT && iimgtyp < IMGTYPE_OBJECT))	// 夜间: 非夜间类型
				&& (*it)->state == OBSPLAN_CAT) { // 计划类型: 入库
				td1 = ((*it)->btime - now).total_seconds();
				td2 = ((*it)->etime - now).total_seconds();
				if (iimgtyp <= IMGTYPE_DARK || (td1 <= 60 && td2 >= 10)) plan = *it;
			}
		}
		if (plan.use_count()) plan->state = OBSPLAN_WAIT;
	}
	return plan;
}

void GeneralControl::thread_obsplan() {
	boost::chrono::minutes period(10);

	while (1) {
		boost::this_thread::sleep_for(period);

		mutex_lock lck(mtx_obsplan_);
		ptime now = second_clock::universal_time();
		string tzt = to_iso_string(now + hours(8));
		int tdt;
		for (ObsPlanVec::iterator it = obsplan_.begin(); it != obsplan_.end();) {
			if ((*it)->state >= OBSPLAN_OVER	//< 状态: 完成或抛弃
				|| (tdt = ((*it)->etime - now).total_seconds()) < 20) {	//< 距离结束时间不足20秒
				if ((*it)->state == OBSPLAN_CAT) {
					_gLog.Write(LOG_WARN, NULL, "Ignored plan[type: %s] <%s>",
							(*it)->plan_type == PLANTYPE_TRACK ? "track"
							: ((*it)->plan_type == PLANTYPE_POINT ? "point" : "manual"),
							(*it)->plan_sn.c_str());

					if (dbt_.unique()) dbt_->UploadObsplanState((*it)->plan_sn, "abandon", tzt);
				}
				it = obsplan_.erase(it);
			}
			else ++it;
		}
	}
}

void GeneralControl::thread_odt() {
	boost::chrono::minutes period(2);
	boost::mutex mtx;
	mutex_lock lck(mtx);
	int dayold(-1), daynew, odt, preslew(0);
	double ra, dec, azi, alt, altold(0.0), lmst;
	double odtDay(param_.odtDay), odtNight(param_.odtNight);

	boost::this_thread::sleep_for(boost::chrono::seconds(10));
	while(1) {
		ptime now = second_clock::universal_time();
		ptime::date_type day = now.date();
		ptime::time_duration_type tmday = now.time_of_day();
		/* 检查观测计划 */
		daynew = day.day();

		if (dayold != daynew && scan_obsplan()) {
			dayold  = daynew;
			preslew = 0;

			/* 向数据库上传所有观测计划 */
			if (dbt_.unique()) {
				string btime, etime;
				for (ObsPlanVec::iterator it = obsplan_.begin(); it != obsplan_.end(); ++it) {
					btime = to_iso_string((*it)->btime + hours(8));
					etime = to_iso_string((*it)->etime + hours(8));
					dbt_->UploadObsPlan((*it)->plan_sn, (*it)->plan_type, btime, etime);
				}
			}

			/**
			 * @date 2019-11-24
			 * 第一次: 驱动转台微量运动, 消除长时间静止造成的阻尼
			 **/
			mutex_lock lck(mtx_obss_);
			apbase proto = to_apbase(boost::make_shared<ascii_proto_preslew>());
			for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
				(*it)->NotifyAsciiProtocol(proto);
			}
		}
		/* 计算观测时段类型 */
		ats_.SetUTC(day.year(), day.month(), daynew, (tmday.hours() + tmday.minutes() / 60.0) / 24.0);
		ats_.SunPosition(ra, dec);
		lmst = ats_.LocalMeanSiderealTime();
		ats_.Eq2Horizon(lmst - ra, dec, azi, alt);
		alt *= R2D;
		odt = alt > odtDay ? OD_DAY : (alt < odtNight ? OD_NIGHT : OD_FLAT);
		if (odt == OD_DAY && alt < altold && alt < (odtDay + 1) && ++preslew == 1) {
			/**
			 * @date 2019-11-24
			 * 第二次: 驱动转台微量运动, 消除长时间静止造成的阻尼
			 **/
			mutex_lock lck(mtx_obss_);
			apbase proto = to_apbase(boost::make_shared<ascii_proto_preslew>());
			for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
				(*it)->NotifyAsciiProtocol(proto);
			}
		}
		altold = alt;
		if (nfEnv_.odt != odt) {
			_gLog.Write("Enter %s duration", odt == OD_NIGHT ? "Night"
					: (odt == OD_FLAT ? "Flat field" : "Daylight"));
			if (odt == OD_DAY && nfEnv_.odt != -1) {
				mutex_lock lck(mtx_obss_);
				apbase proto = to_apbase(boost::make_shared<ascii_proto_park>());
				for (ObsSysVec::iterator it = obss_.begin(); it != obss_.end(); ++it) {
					(*it)->NotifyAsciiProtocol(proto);
				}
			}
			nfEnv_.odt = odt;

			if (nfEnv_.slitEnable && odt == OD_DAY && nfEnv_.slitState == SS_OPEN)
				command_slit(SC_CLOSE);
		}
		/*
		 * @date 2019-10-25
		 * HN
		 * 由于环境监测状态不完备（风速、云量、雷电等），工作逻辑变更为：
		 * - 手动打开天窗
		 * - 监测雨量，自动关闭天窗
		 * @date 2019-11-03
		 * - 取消天窗控制功能
		 * @date 2019-11-18
		 * - 夜间进入白天, 当天窗状态判定为全开时, 尝试3次关闭天窗
		 */
//		if (nfEnv_.slitEnable) {
//			cmd = -1;
//			if (odt == OD_DAY && nfEnv_.slitState == SS_OPEN && ++delay > 1 && delay < 5) cmd = SC_CLOSE;
//			else if (odt > OD_DAY && !nfEnv_.rain && nfEnv_.slitState == SS_CLOSED && nfEnv_.mode == OC_START && obsplan_.size()) cmd = SC_OPEN;
//			if (cmd != -1) command_slit(cmd);
//		}

		/**
		 * @date 2019-11-02
		 * - 重新加载观测计划: 调试过程中需要手动生成观测计划
		 */
		if (cv_status::no_timeout == cv_loadplan_.wait_for(lck, period)) dayold = -1;
	}
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测计划 -------------------*/
//////////////////////////////////////////////////////////////////////////////
