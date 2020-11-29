/**
 * @file ObservationSystem.cpp
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#include <stdlib.h>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include "globaldef.h"
#include "ObservationSystem.h"
#include "GLog.h"

using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::placeholders;

ObservationSystem::ObservationSystem(const string& gid, const string& uid)
		: param_(NULL) {
	gid_ = gid;
	uid_ = uid;
	robotic_  = false;
	mode_run_ = OBSS_ERROR;
	altLimit_ = 0.0;
	odt_ = -1;
}

ObservationSystem::~ObservationSystem() {

}

bool ObservationSystem::Start() {
	// 启动消息机制
	string name = DAEMON_NAME;
	name += gid_ + uid_;
	if (!MessageQueue::Start(name.c_str())) {
		_gLog.Write(LOG_FAULT, "fail to create message queue for OBSS[%s:%s]",
				gid_.c_str(), uid_.c_str());
		return false;
	}
	// 网络通信
	bufTcp_.reset(new char[TCP_PACK_SIZE]);
	kvProto_    = KvProtocol::Create();
	nonkvProto_ = NonkvProtocol::Create();
	// 观测计划
	obsPlans_   = ObservationPlan::Create();

	_gLog.Write("OBSS[%s:%s] starts running", gid_.c_str(), uid_.c_str());
	return true;
}

void ObservationSystem::Stop() {
	MessageQueue::Stop();
	interrupt_thread(thrd_acqPlan_);
	interrupt_thread(thrd_calPlan_);

	_gLog.Write("OBSS[%s:%s] stopped", gid_.c_str(), uid_.c_str());
}

void ObservationSystem::SetParameter(const OBSSParam* param) {
	_gLog.Write("OBSS[%s:%s] locates at: %.4f, %.4f, altitude: %.1f, timezone: %d. AltLimit = %.1f",
			gid_.c_str(), uid_.c_str(),
			param->siteLon, param->siteLat, param->siteAlt, param->timeZone,
			param->altLimit);
	ats_.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timeZone);
	altLimit_ = param->altLimit * D2R;
	robotic_  = param->robotic;
	param_    = param;

	if (param->autoBias || param->autoDark || param->autoFlat)
		thrd_calPlan_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_calbration_plan, this)));
}

void ObservationSystem::SetDBPtr(DBCurlPtr ptr) {
	dbPtr_ = ptr;
}

void ObservationSystem::RegisterAcquirePlan(const AcqPlanCBSlot& slot) {
	if (!acqPlan_.empty()) acqPlan_.disconnect_all_slots();
	acqPlan_.connect(slot);
}

bool ObservationSystem::IsSafePoint(ObsPlanItemPtr plan, const ptime& now) {
	bool safe(true);
	if ((plan->coorsys == TypeCoorSys::COORSYS_EQUA
			|| plan->coorsys == TypeCoorSys::COORSYS_ALTAZ)) {// 位置
		double lon = plan->lon * D2R;
		double lat = plan->lat * D2R;
		if (plan->coorsys == TypeCoorSys::COORSYS_ALTAZ) safe = lat >= altLimit_;
		else {
			MtxLck lck(mtx_ats_);
			double azi, alt;

			ats_.SetMJD(now.date().modjulian_day() + now.time_of_day().total_seconds() / DAYSEC);
			ats_.Eq2Horizon(ats_.LocalMeanSiderealTime() - lon, lat, azi, alt);
			safe = lat >= altLimit_;
		}
	}
	return safe;
}

int ObservationSystem::IsActive() {
	int n = net_camera_.size();
	if (net_mount_.IsOpen()) ++n;

	return n;
}

int ObservationSystem::IsMatched(const string& gid, const string& uid) {
	if (gid_ == gid && uid_ == uid) return 1;
	if (gid.empty() || (gid_ == gid && uid.empty())) return 2;
	return 0;
}

void ObservationSystem::GetIDs(string& gid, string& uid) {
	gid = gid_;
	uid = uid_;
}

int ObservationSystem::GetPriority() {
	int prio(0);

	if (mode_run_ != OBSS_AUTO) prio = INT_MAX;
	else if (plan_wait_.use_count()) // 按照策略, 等待区计划优先级不低于当前计划优先级
		prio = plan_wait_->priority;
	else if (plan_now_.use_count()) {// 当前计划的优先级
		int T  = plan_now_->period;
		int dt = (second_clock::universal_time() - plan_now_->tmbegin).total_seconds();
		if (dt >= int(T * 0.7)) prio = 4 * plan_now_->priority;
		else prio = plan_now_->priority * T / (T - dt);
	}
	return prio;
}

void ObservationSystem::CoupleClient(const TcpCPtr client) {
	MtxLck lck(mtx_client_);
	tcpc_client_.push_back(client);
}

int ObservationSystem::CoupleMount(const TcpCPtr client, kvbase base) {
	// 尝试关联转台和观测系统
	if (!net_mount_.IsOpen()) {
		net_mount_.client = client;
		net_mount_.kvtype = true;

		if (!param_->p2hMount) {// P2P模式, 由OBSS接管网络信息接收/解析
			const TcpClient::CBSlot& slot = boost::bind(&ObservationSystem::receive_from_peer, this, _1, _2, PEER_MOUNT);
			client->RegisterRead(slot);
		}
		PostMessage(MSG_MOUNT_LINKED, 1);
	}
	else if (net_mount_() != client) {
		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] has related mount",
				gid_.c_str(), uid_.c_str());
		return MODE_ERROR;
	}
	if (iequals(base->type, KVTYPE_MOUNT)) {// 处理通信协议
		int old_state(net_mount_.state);
		net_mount_ = from_kvbase<kv_proto_mount>(base);
		if (old_state != net_mount_.state) PostMessage(MSG_MOUNT_CHANGED, old_state);
	}

	// 返回关联结果
	return param_->p2hMount ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMount(const TcpCPtr client, nonkvbase base) {
	// 尝试关联转台和观测系统
	if (!net_mount_.IsOpen()) {
		net_mount_.client = client;
		net_mount_.kvtype = false;
		PostMessage(MSG_MOUNT_LINKED, 1);
	}
	else if (net_mount_() != client) {
		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] has related mount",
				gid_.c_str(), uid_.c_str());
		return MODE_ERROR;
	}
	if (iequals(base->type, NONKVTYPE_MOUNT)) {
		MtxLck lck(mtx_ats_);
		// 补充地平坐标
		nonkvmount proto = from_nonkvbase<nonkv_proto_mount>(base);
		ptime now = microsec_clock::universal_time();
		int old_state(net_mount_.state);
		double azi, alt;

		ats_.SetMJD(now.date().modjulian_day() + now.time_of_day().total_microseconds() * 1E-6 / DAYSEC);
		ats_.Eq2Horizon(ats_.LocalMeanSiderealTime() - proto->ra * D2R, proto->dec * D2R, azi, alt);
		proto->azi = azi * R2D;
		proto->alt = alt * R2D;
		net_mount_ = proto;
		if (old_state != net_mount_.state) PostMessage(MSG_MOUNT_CHANGED, old_state);
	}
	return param_->p2hMount ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleCamera(const TcpCPtr client, kvbase base) {
	string cid = base->cid;
	NetCamPtr cam = find_camera(cid);
	if (!cam->IsOpen()) {
		_gLog.Write("Camera[%s:%s:%s] is on-line", gid_.c_str(), uid_.c_str(), cid.c_str());
		cam->client = client;

		if (!param_->p2hCamera) {
			const TcpClient::CBSlot& slot = boost::bind(&ObservationSystem::receive_from_peer, this, _1, _2, PEER_CAMERA);
			client->RegisterRead(slot);
		}
		PostMessage(MSG_CAMERA_LINKED, 1);
		if (dbPtr_.use_count()) {//...通知数据库

		}
	}
	else if ((*cam)() != client) {
		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] has releated camera[%s]",
				gid_.c_str(), uid_.c_str(), cid.c_str());
		return MODE_ERROR;
	}

	if (iequals(base->type, KVTYPE_CAMERA)) {
		int old_state(cam->state);
		*cam = from_kvbase<kv_proto_camera>(base);
		if (old_state != cam->state && plan_now_.use_count()) PostMessage(MSG_CAMERA_CHANGED);
	}
	return param_->p2hCamera ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMountAnnex(const TcpCPtr client, kvbase base) {
	return param_->p2hMountAnnex ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMountAnnex(const TcpCPtr client, nonkvbase base) {
	return param_->p2hMountAnnex ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleCameraAnnex(const TcpCPtr client, kvbase base) {
	return param_->p2hCameraAnnex ? MODE_P2H : MODE_P2P;
}

void ObservationSystem::DecoupleClient(const TcpCPtr client) {
	MtxLck lck(mtx_client_);
	TcpCVec::iterator it, itend = tcpc_client_.end();
	for (it = tcpc_client_.begin(); it != itend && (*it) != client; ++it);
	if (it != itend) tcpc_client_.erase(it);
}

void ObservationSystem::DecoupleMount(const TcpCPtr client) {
	if (net_mount_() == client) PostMessage(MSG_MOUNT_LINKED, 0);
}

void ObservationSystem::DecoupleCamera(const TcpCPtr client) {
	MtxLck lck(mtx_camera_);
	NetCamVec::iterator it, itend = net_camera_.end();
	for (it = net_camera_.begin(); it != itend && client != (**it)(); ++it);
	if (it != itend) {
		_gLog.Write("Camera[%s:%s:%s] is off-line", gid_.c_str(), uid_.c_str(),
				(*it)->cid.c_str());
		net_camera_.erase(it);
		PostMessage(MSG_CAMERA_LINKED, 0);

		if (dbPtr_.use_count()) {//...通知数据库

		}
	}
}

void ObservationSystem::DecoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::NotifyKVClient(kvbase base) {
	MtxLck lck(mtx_queKv_);
	queKv_.push_back(base);
	PostMessage(MSG_RECEIVE_KV);
}

void ObservationSystem::NotifyPlan(ObsPlanItemPtr plan) {
	if (plan_wait_.use_count()) {// 等待区计划退回计划队列
		plan_wait_->state = StateObservationPlan::OBSPLAN_CATALOGED;
		plan_wait_.reset();
	}
	if (plan_now_.use_count()) {// 中止当前计划
		plan->state = StateObservationPlan::OBSPLAN_WAITING;
		plan_wait_ = plan;
		abort_plan();
	}
	else {// 立即执行计划
		plan->state = StateObservationPlan::OBSPLAN_RUNNING;
		plan_now_ = plan;
		process_plan();
	}
}

void ObservationSystem::AbortPlan(ObsPlanItemPtr plan) {
	if (plan == plan_wait_) plan_wait_.reset();
	else abort_plan();
}

void ObservationSystem::NotifyODT(int type) {
	// 改变可观测时间类型
	if (type != odt_) {
		odt_ = type;
		PostMessage(MSG_SWITCH_OBSFLOW);
	}
}

void ObservationSystem::NotifySlitState(int state) {
	//...
	PostMessage(MSG_SWITCH_OBSFLOW);
}

//////////////////////////////////////////////////////////////////////////////
/*----------------- 消息响应 -----------------*/
void ObservationSystem::register_messages() {
	const CBSlot& slot1 = boost::bind(&ObservationSystem::on_tcp_receive,     this, _1, _2);
	const CBSlot& slot2 = boost::bind(&ObservationSystem::on_receive_kv,      this, _1, _2);
	const CBSlot& slot3 = boost::bind(&ObservationSystem::on_receive_nonkv,   this, _1, _2);
	const CBSlot& slot4 = boost::bind(&ObservationSystem::on_mount_linked,    this, _1, _2);
	const CBSlot& slot5 = boost::bind(&ObservationSystem::on_mount_changed,   this, _1, _2);
	const CBSlot& slot6 = boost::bind(&ObservationSystem::on_camera_linked,   this, _1, _2);
	const CBSlot& slot7 = boost::bind(&ObservationSystem::on_camera_changed,  this, _1, _2);
	const CBSlot& slot8 = boost::bind(&ObservationSystem::on_switch_obsflow,  this, _1, _2);

	RegisterMessage(MSG_TCP_RECEIVE,     slot1);
	RegisterMessage(MSG_RECEIVE_KV,      slot2);
	RegisterMessage(MSG_RECEIVE_NONKV,   slot3);
	RegisterMessage(MSG_MOUNT_LINKED,    slot4);
	RegisterMessage(MSG_MOUNT_CHANGED,   slot5);
	RegisterMessage(MSG_CAMERA_LINKED,   slot6);
	RegisterMessage(MSG_CAMERA_CHANGED,  slot7);
	RegisterMessage(MSG_SWITCH_OBSFLOW,  slot8);
}

void ObservationSystem::on_tcp_receive(const long par1, const long par2) {
	TcpRcvPtr rcvd;
	{// 取队首
		MtxLck lck(mtx_tcpRcv_);
		rcvd = que_tcpRcv_.front();
		que_tcpRcv_.pop_front();
	}

	TcpCPtr client = rcvd->client;
	int peer = rcvd->peer;
	if (rcvd->hadRcvd) {
		const char term[] = "\n";	// 信息结束符: 换行
		int lenTerm = strlen(term);	// 结束符长度
		int pos;
		while (client->IsOpen() && (pos = client->Lookup(term, lenTerm)) >= 0) {
			client->Read(bufTcp_.get(), pos + lenTerm);
			bufTcp_[pos] = 0;
			if      (peer == PEER_MOUNT)        resolve_kv_mount       (client);
			else if (peer == PEER_CAMERA)       resolve_kv_camera      (client);
			else if (peer == PEER_MOUNT_ANNEX)  resolve_kv_mount_annex (client);
			else if (peer == PEER_CAMERA_ANNEX) resolve_kv_camera_annex(client);
		}
	}
	else {
		client->Close();
		if      (peer == PEER_MOUNT)        PostMessage(MSG_MOUNT_LINKED, 0);
		else if (peer == PEER_CAMERA)       DecoupleCamera(client);
		else if (peer == PEER_MOUNT_ANNEX)  DecoupleMountAnnex(client);
		else if (peer == PEER_CAMERA_ANNEX) DecoupleCameraAnnex(client);
	}
}

void ObservationSystem::on_receive_kv(const long par1, const long par2) {
	// 处理由GeneralControl投递的KV类型协议
	// 提取队列的头信息
	kvbase base;
	{
		MtxLck lck(mtx_queKv_);
		base = queKv_.front();
		queKv_.pop_front();
	}
	// 分类处理
	string type = base->type;
	char ch = type[0];

	if (ch == 'a' || ch == 'A') {
		if      (iequals(type, KVTYPE_ABTSLEW))  process_abort_slew();
		else if (iequals(type, KVTYPE_ABTIMG))   process_abort_image(base->cid);
	}
	else if (ch == 'f' || ch == 'F') {
		if      (iequals(type, KVTYPE_FWHM))     process_fwhm(base->cid, from_kvbase<kv_proto_fwhm>(base)->value);
		else if (iequals(type, KVTYPE_FOCUS))    process_focus(base->cid, from_kvbase<kv_proto_focus>(base)->position);
		else if (iequals(type, KVTYPE_FINDHOME)) process_findhome();
	}
	else if (ch == 's' || ch == 'S') {
		if      (iequals(type, KVTYPE_START))    process_start();
		else if (iequals(type, KVTYPE_STOP))     process_stop();
		else if (iequals(type, KVTYPE_SLEWTO))   process_slewto(from_kvbase<kv_proto_slewto>(base));
	}
	else if (iequals(type, KVTYPE_GUIDE))        process_guide(from_kvbase<kv_proto_guide>(base));
	else if (iequals(type, KVTYPE_HOMESYNC))     process_homesync(from_kvbase<kv_proto_home_sync>(base));
	else if (iequals(type, KVTYPE_MCOVER))       process_mcover(base->cid, from_kvbase<kv_proto_mcover>(base)->command);
	else if (iequals(type, KVTYPE_PARK))         process_park();
	else if (iequals(type, KVTYPE_TAKIMG))       process_take_image(from_kvbase<kv_proto_take_image>(base));
	else if (iequals(type, KVTYPE_DISABLE))      process_disable(base->cid);
	else if (iequals(type, KVTYPE_ENABLE))       process_enable(base->cid);
}

void ObservationSystem::on_receive_nonkv(const long par1, const long par2) {

}

void ObservationSystem::on_mount_linked(const long linked, const long par2) {
	int mode;
	if (linked) {
		_gLog.Write("Mount[%s:%s] is on-line", gid_.c_str(), uid_.c_str());
		mode = net_camera_.empty() ? OBSS_MANUAL : (robotic_ ? OBSS_AUTO : OBSS_MANUAL);
	}
	else {
		_gLog.Write("Mount[%s:%s] is off-line", gid_.c_str(), uid_.c_str());
		net_mount_.Reset();
		mode = net_camera_.empty() ? OBSS_ERROR : OBSS_MANUAL;
	}
	switch_runmode(mode);

	if (dbPtr_.use_count()) {//...通知数据库

	}
}

void ObservationSystem::on_mount_changed(const long old_state, const long par2) {
	_gLog.Write("mount<%s:%s> goes into <%s>", gid_.c_str(), uid_.c_str(),
			StateMount::ToString(net_mount_.state));

	/* 转台工作状态变化时, 开始曝光的判定条件:
	 * - 在执行观测计划
	 * - 状态变更为TRACKING
	 * - 指向偏差小于阈值
	 * 分支:
	 * - 对于GWAC系统, 由于指向偏差较大, 为减少重做模板次数, 指向到位后仅启动导星相机开始曝光
	 * 错误处理:
	 * - 指向偏差过大, 中止观测计划
	 */
	if (plan_now_.use_count() && net_mount_.state == StateMount::MOUNT_TRACKING) {
		double ea(0.0);
		if (plan_now_->iimgtype <= TypeImage::IMGTYP_FLAT	// 定标: 不检查指向偏差
				|| (ea = net_mount_.ArriveError() * 60.0) <= param_->tArrive) {// 非定标: 检查指向偏差
			/* 通知相机开始曝光 */
			// 通用: 通知所有相机开始曝光
			process_expose(CommandExpose::EXP_START);
			// GWAC: 通知导星相机开始曝光
//			process_expose(CommandExpose::EXP_START, true);
		}
		else {// 打印错误并中止计划
			_gLog.Write(LOG_WARN, "PE<%.1f>[arcmin] of Mount[%s:%s] is beyond the threshold",
					ea, gid_.c_str(), uid_.c_str());
			abort_plan();
		}
	}
}

void ObservationSystem::on_camera_linked(const long linked, const long par2) {
	int mode; // 系统工作状态
	if (linked) {
		mode = net_mount_.IsOpen() ? (robotic_ ? OBSS_AUTO : OBSS_MANUAL) : OBSS_MANUAL;
	}
	else {
		mode = net_camera_.size() ? mode_run_ : (net_mount_.IsOpen() ? OBSS_MANUAL : OBSS_ERROR);
	}
	switch_runmode(mode);
}

void ObservationSystem::on_camera_changed(const long par1, const long par2) {
	int nTot(net_camera_.size()), nIdle(0), nFlat(0);
	{// 统计相机工作状态
		MtxLck lck(mtx_camera_);
		for (NetCamVec::iterator it = net_camera_.begin(); it != net_camera_.end(); ++it) {
			if      ((*it)->state == StateCameraControl::CAMCTL_IDLE)         ++nIdle;
			else if ((*it)->state == StateCameraControl::CAMCTL_WAITING_FLAT) ++nFlat;
		}
	}
	if (nIdle == nTot) {// 所有相机曝光结束; 观测计划结束; 申请新的计划或执行等待区计划
		if (plan_now_->state == StateObservationPlan::OBSPLAN_RUNNING)
			plan_now_->state = StateObservationPlan::OBSPLAN_OVER;
		_gLog.Write("plan<%s> on [%s:%s] is %s", plan_now_->plan_sn.c_str(), gid_.c_str(), uid_.c_str(),
				StateObservationPlan::ToString(plan_now_->state));

		plan_now_.reset();
		cv_acqPlan_.notify_one();
	}
	else if ((nFlat + nIdle) == nTot) {// 平场, 重新指向
		flat_reslew();
	}
}

void ObservationSystem::on_switch_obsflow(const long par1, const long par2) {
	if (odt_ > TypeObservationDuration::ODT_DAYTIME // 时间: 非白天
			&& mode_run_ == OBSS_AUTO // 模式: 自动
			) { // 天窗: 没有或已打开
		if (!thrd_acqPlan_.unique()) {
			_gLog.Write("OBSS[%s:%s] starts observation", gid_.c_str(), uid_.c_str());
			thrd_acqPlan_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_acquire_plan, this)));
		}
	}
	else if (thrd_acqPlan_.unique()) {
		_gLog.Write("OBSS[%s:%s] stops observation", gid_.c_str(), uid_.c_str());
		interrupt_thread(thrd_acqPlan_);
	}
}

/*----------------- 消息响应 -----------------*/
//////////////////////////////////////////////////////////////////////////////
/* 网络通信 */
void ObservationSystem::receive_from_peer(const TcpCPtr client, const error_code& ec, int peer) {
	MtxLck lck(mtx_tcpRcv_);
	TcpRcvPtr rcvd = TcpReceived::Create(client, peer, !ec);
	que_tcpRcv_.push_back(rcvd);
	PostMessage(MSG_TCP_RECEIVE);
}

void ObservationSystem::resolve_kv_mount(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveMount(bufTcp_.get());
	if (iequals(base->type, KVTYPE_MOUNT)) {// 转台状态
		int old_state(net_mount_.state);
		net_mount_ = from_kvbase<kv_proto_mount>(base);
		if (old_state != net_mount_.state)
			PostMessage(MSG_MOUNT_CHANGED, old_state);
	}
}

void ObservationSystem::resolve_kv_camera(const TcpCPtr client) {
	kvbase base = kvProto_->ResolveCamera(bufTcp_.get());
	if (iequals(base->type, KVTYPE_CAMERA)) {// 相机状态
		NetCamPtr cam = find_camera(client);
		int old_state(cam->state);
		*cam = from_kvbase<kv_proto_camera>(base);
		if (old_state != cam->state && plan_now_.use_count())
			PostMessage(MSG_CAMERA_CHANGED);
	}
}

void ObservationSystem::resolve_kv_mount_annex (const TcpCPtr client) {

}

void ObservationSystem::resolve_kv_camera_annex(const TcpCPtr client) {

}

//////////////////////////////////////////////////////////////////////////////
/* 处理由上层程序投递到观测系统的键值对协议 */
void ObservationSystem::process_start() {

}

void ObservationSystem::process_stop() {

}

void ObservationSystem::process_enable(const string& cid) {

}

void ObservationSystem::process_disable(const string& cid) {

}

void ObservationSystem::process_findhome() {

}

void ObservationSystem::process_slewto(int type, double lon, double lat, double epoch) {
	//...内部调用, 进入判据不同
	kvslewto proto = boost::make_shared<kv_proto_slewto>();
	proto->coorsys = type;
	proto->lon     = lon;
	proto->lat     = lat;
	proto->epoch   = epoch;
	process_slewto(proto);
}

void ObservationSystem::process_slewto(kvslewto proto) {
	if (net_mount_.IsOpen()) {//...判据
		int n;
		const char* to_write;
		net_mount_.BeginSlew(proto->coorsys, proto->lon, proto->lat);
		to_write = kvProto_->CompactSlewto(proto, n);
		net_mount_()->Write(to_write, n);
	}
}

void ObservationSystem::process_abort_slew() {

}

void ObservationSystem::process_guide(kvguide proto) {

}

void ObservationSystem::process_homesync(kvhomesync proto) {

}

void ObservationSystem::process_park() {

}

void ObservationSystem::process_take_image(kvtakeimg proto) {

}

void ObservationSystem::process_abort_image(const string& cid) {

}

void ObservationSystem::process_fwhm(const string& cid, const double fwhm) {

}

void ObservationSystem::process_focus(const string& cid, const int pos) {

}

void ObservationSystem::process_mcover(const string& cid, int cmd) {

}
/* 处理由上层程序投递到观测系统的键值对协议 */
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::switch_runmode(int mode) {
	if (mode != mode_run_) {
		_gLog.Write("OBSS[%s:%s] enters %s mode", gid_.c_str(), uid_.c_str(),
				mode == OBSS_ERROR ? "ERROR"
						: (mode == OBSS_MANUAL ? "MANUAL" : "AUTO"));
		mode_run_ = mode;
		PostMessage(MSG_SWITCH_OBSFLOW);
	}
}

ObservationSystem::NetCamPtr ObservationSystem::find_camera(const string& cid) {
	NetCamPtr cam;
	NetCamVec::iterator it, end = net_camera_.end();
	for (it = net_camera_.begin(); it != net_camera_.end() && (*it)->cid != cid; ++it);
	if (it != end) cam = (*it);
	else {
		NetCamPtr cam = NetworkCamera::Create();
		cam->cid = cid;
		net_camera_.push_back(cam);
	}
	return cam;
}

ObservationSystem::NetCamPtr ObservationSystem::find_camera(const TcpCPtr client) {
	NetCamVec::iterator it, end = net_camera_.end();
	for (it = net_camera_.begin(); it != end && (**it)() != client; ++it);
	return (*it);
}

//////////////////////////////////////////////////////////////////////////////
/* 观测计划 */
void ObservationSystem::process_plan() {
	if (plan_now_->gid.empty()) plan_now_->gid = gid_;
	if (plan_now_->uid.empty()) plan_now_->uid = uid_;
	plan_now_->state = StateObservationPlan::OBSPLAN_RUNNING;
	//...
}

void ObservationSystem::abort_plan() {
	//...
}

ObsPlanItemPtr ObservationSystem::generate_plan_common(const ptime& utcNow) {
	ObsPlanItemPtr plan = ObservationPlanItem::Create();
	plan->gid = gid_;
	plan->uid = uid_;
	plan->plan_time = to_iso_extended_string(utcNow);
	plan->plan_type = "Calibration";
	plan->obstype   = "Cal";
	plan->observer  = "auto";
	plan->expdur    = param_->autoExpdur;
	plan->frmcnt    = param_->autoFrmCnt;
	plan->tmbegin   = utcNow;
	plan->tmend     = utcNow + hours(23);
	plan->priority  = INT_MAX;	// 定标计划不可打断

	return plan;
}

void ObservationSystem::generate_plan_bias(const ptime& utcNow) {
	ObsPlanItemPtr plan = generate_plan_common(utcNow);
	plan->plan_sn = to_iso_string(utcNow.date()) + "_bias";
	if (plan->CompleteCheck()) obsPlans_->AddPlan(plan);
}

void ObservationSystem::generate_plan_dark(const ptime& utcNow) {
	ObsPlanItemPtr plan = generate_plan_common(utcNow);
	plan->plan_sn = to_iso_string(utcNow.date()) + "_dark";
	if (plan->CompleteCheck()) obsPlans_->AddPlan(plan);
}

void ObservationSystem::generate_plan_flat(const ptime& utcNow) {
	ObsPlanItemPtr plan = generate_plan_common(utcNow);
	plan->plan_sn = to_iso_string(utcNow.date()) + "_flat";
	plan->filters.push_back("All");
	if (plan->CompleteCheck()) obsPlans_->AddPlan(plan);
}

void ObservationSystem::flat_reslew(bool first) {
	/* 生成随机位置 */
	ptime utc = second_clock::universal_time();
	int hour = utc.time_of_day().hours() + param_->timeZone;
	double azi0, azi, alt;
	double mjd, lmst, ha, ra, dec;

	if (first) srand(utc.time_of_day().total_seconds());
	mjd = utc.date().modjulian_day() + utc.time_of_day().total_seconds() / DAYSEC;
	lmst = ats_.LocalMeanSiderealTime(mjd, param_->siteLon * D2R);
	// 方位角(南零点): (0, 90] 上午; [270, 360) 下午
	if (hour < 0) hour += 24;
	else if (hour >= 24) hour -= 24;
	azi0 = hour < 12 ? 0.0 : 270.0;
	// 生成方位和高度
	alt = rand() * 5.0 / RAND_MAX + 80.0;
	azi = rand() * 90.0 / RAND_MAX + azi0;
	ats_.Horizon2Eq(azi * D2R, alt * D2R, ha, dec);
	ra  = cycmod(lmst - ha, A2PI) * R2D;
	dec*= R2D;
	// 执行指向操作
	process_slewto(TypeCoorSys::COORSYS_EQUA, ra, dec);
}

void ObservationSystem::process_expose(int cmd, bool just_guide) {
	MtxLck lck(mtx_camera_);
	int n;
	const char* to_write = kvProto_->CompactExpose(cmd, n);
	bool matched(false);
	for (NetCamVec::iterator it = net_camera_.begin(); it != net_camera_.end() && !matched; ++it) {
		if (!just_guide || (matched = (std::stoi((*it)->cid) % 5) == 0))
			(*it)->client->Write(to_write, n);
	}
}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::thread_acquire_plan() {
	boost::chrono::minutes period(2);
	boost::mutex mtx;
	MtxLck lck(mtx);

	while(1) {
		cv_acqPlan_.wait_for(lck, period);

		if (!(plan_now_.use_count() || plan_wait_.use_count())) {
			plan_now_ = *acqPlan_(shared_from_this());
			if (plan_now_.use_count()) {
				//...开始执行计划
			}
		}
	}
}

void ObservationSystem::thread_calbration_plan() {
	int t;
	ptime now, next;

	boost::this_thread::sleep_for(boost::chrono::seconds(10));
	while (1) {
		now = second_clock::universal_time();
		if (param_->autoBias) generate_plan_bias(now);
		if (param_->autoDark) generate_plan_dark(now);
		if (param_->autoFlat) generate_plan_flat(now);

		now = now + hours(param_->timeZone);
		next = ptime(now.date() + ptime::date_duration_type(1), hours(12));
		t = (next - now).total_seconds();
		boost::this_thread::sleep_for(boost::chrono::seconds(t));
	}
}
