/**
 * @file ObservationSystem.cpp
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "globaldef.h"
#include "ADefine.h"
#include "ObservationSystem.h"
#include "GLog.h"

using namespace AstroUtil;
using namespace boost::posix_time;
using namespace boost::placeholders;

ObservationSystem::ObservationSystem(const string& gid, const string& uid)
		: param_(NULL) {
	gid_ = gid;
	uid_ = uid;
	robotic_  = true;
	mode_     = OBSS_ERROR;
	altLimit_ = 0.0;
}

ObservationSystem::~ObservationSystem() {

}

void ObservationSystem::SetParameter(const OBSSParam* param) {
	_gLog.Write("OBSS[%s:%s] locates at: %.4f, %.4f, altitude: %.1f, timezone: %d. AltLimit = %.1f",
			gid_.c_str(), uid_.c_str(),
			param->siteLon, param->siteLat, param->siteAlt, param->timeZone,
			param->altLimit);
	ats_.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timeZone);
	altLimit_ = param->altLimit * D2R;
	param_ = param;
}

void ObservationSystem::SetDBPtr(DBCurlPtr ptr) {
	dbPtr_ = ptr;
}

void ObservationSystem::RegisterAcquirePlan(const AcqPlanCBSlot& slot) {
	if (!acqPlan_.empty()) acqPlan_.disconnect_all_slots();
	acqPlan_.connect(slot);
}

void ObservationSystem::GetIDs(string& gid, string& uid) {
	gid = gid_;
	uid = uid_;
}

bool ObservationSystem::IsSafePoint(ObsPlanItemPtr plan, const ptime& now) {
	bool safe(true);
	if ((plan->coorsys == TypeCoorSys::COORSYS_EQUA
			|| plan->coorsys == TypeCoorSys::COORSYS_ALTAZ)) {// 位置
		double lon = plan->lon * D2R;
		double lat = plan->lat * D2R;
		if (plan->coorsys == TypeCoorSys::COORSYS_ALTAZ) safe = lat >= altLimit_;
		else {
			ptime::date_type today = now.date();
			double lmst, azi, alt;

			ats_.SetUTC(today.year(), today.month(), today.day(),
					now.time_of_day().total_seconds() / DAYSEC);
			lmst = ats_.LocalMeanSiderealTime();
			ats_.Eq2Horizon(lmst - lon, lat, azi, alt);
			safe = lat >= altLimit_;
		}
	}
	return safe;
}

bool ObservationSystem::Start() {
	string name = DAEMON_NAME;
	name += gid_ + uid_;
	if (!MessageQueue::Start(name.c_str())) {
		_gLog.Write(LOG_FAULT, "fail to create message queue for OBSS[%s:%s]",
				gid_.c_str(), uid_.c_str());
		return false;
	}
	//...
	bufTcp_.reset(new char[TCP_PACK_SIZE]);

	_gLog.Write("OBSS[%s:%s] starts running", gid_.c_str(), uid_.c_str());
	return true;
}

void ObservationSystem::Stop() {
	//...
//	interrupt_thread(thrd_acqPlan_);
	_gLog.Write("OBSS[%s:%s] stopped", gid_.c_str(), uid_.c_str());
}

int ObservationSystem::IsActive() {
	return 0;
}

int ObservationSystem::IsMatched(const string& gid, const string& uid) {
	if (gid_ == gid && uid_ == uid) return 1;
	if (gid.empty() || (gid_ == gid && uid.empty())) return 2;
	return 0;
}

int ObservationSystem::GetPriority() {
	int prio(0);

	if (mode_ == OBSS_ERROR) prio = INT_MAX;
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

int ObservationSystem::CoupleMount(const TcpCPtr client, kvbase proto) {
//	if (!net_mount_.IsOpen()) {
//		_gLog.Write("Mount[%s:%s] is on-line", gid_.c_str(), uid_.c_str());
//		net_mount_.client = client;
//		net_mount_.kvtype = kvtype;
//
//		if (!param_->p2hMount) {// P2P模式, 由OBSS接管网络信息接收/解析
//			const TcpClient::CBSlot& slot = boost::bind(&ObservationSystem::receive_from_peer, this, _1, _2, PEER_MOUNT);
//			client->RegisterRead(slot);
//		}
//	}
//	else if (net_mount_() != client) {
//		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] has related mount",
//				gid_.c_str(), uid_.c_str());
//		return MODE_ERROR;
//	}
	return param_->p2hMount ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMount(const TcpCPtr client, nonkvbase proto) {

	return param_->p2hMount ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleCamera(const TcpCPtr client, kvbase proto) {
//	NetCamPtr cam = find_camera(cid);
//	if (!cam->IsOpen()) {
//		_gLog.Write("Camera[%s:%s:%s] is on-line", gid_.c_str(), uid_.c_str(), cid.c_str());
//		cam->client = client;
//
//		if (!param_->p2hCamera) {
//			const TcpClient::CBSlot& slot = boost::bind(&ObservationSystem::receive_from_peer, this, _1, _2, PEER_CAMERA);
//			client->RegisterRead(slot);
//		}
//	}
//	else if ((*cam)() != client) {
//		_gLog.Write(LOG_FAULT, "OBSS[%s:%s] has releated camera[%s]",
//				gid_.c_str(), uid_.c_str(), cid.c_str());
//		return MODE_ERROR;
//	}
	return param_->p2hCamera ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMountAnnex(const TcpCPtr client, kvbase proto) {
	return param_->p2hMountAnnex ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleMountAnnex(const TcpCPtr client, nonkvbase proto) {
	return param_->p2hMountAnnex ? MODE_P2H : MODE_P2P;
}

int ObservationSystem::CoupleCameraAnnex(const TcpCPtr client, const string& cid) {
	return param_->p2hCameraAnnex ? MODE_P2H : MODE_P2P;
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

void ObservationSystem::NotifySlitState(int state) {

}

void ObservationSystem::NotifyKVClient(kvbase proto) {

}

void ObservationSystem::NotifyKVClient(const TcpCPtr client, kvbase proto) {

}

void ObservationSystem::AbortPlan(ObsPlanItemPtr plan) {
	if (plan == plan_wait_) plan_wait_.reset();
	else abort_plan();
}

void ObservationSystem::register_messages() {
	const CBSlot& slot1 = boost::bind(&ObservationSystem::on_tcp_receive, this, _1, _2);

	RegisterMessage(MSG_TCP_RECEIVE, slot1);
}

void ObservationSystem::on_tcp_receive(const long par1, const long par2) {
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

void ObservationSystem::receive_from_peer(const TcpCPtr client, const error_code& ec, int peer) {
	MtxLck lck(mtx_tcpRcv_);
	TcpRcvPtr rcvd = TcpReceived::Create(client, peer, !ec);
	que_tcpRcv_.push_back(rcvd);
	PostMessage(MSG_TCP_RECEIVE);
}

//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::resolve_from_peer(const TcpCPtr client, int peer) {

}

void ObservationSystem::close_socket(const TcpCPtr client, int peer) {
	if (peer == PEER_MOUNT) {
		_gLog.Write("Mount[%s:%s] is off-line", gid_.c_str(), uid_.c_str());
		net_mount_.Reset();
		//...关联
	}
	else if (peer == PEER_CAMERA) {

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

void ObservationSystem::process_kv_client(kvbase proto) {

}

//////////////////////////////////////////////////////////////////////////////
/* 观测计划 */
void ObservationSystem::process_plan() {
	if (plan_now_->gid.empty()) plan_now_->gid   = gid_;
	if (plan_now_->uid.empty()) plan_now_->uid   = uid_;
	plan_now_->state = StateObservationPlan::OBSPLAN_RUNNING;
	//...
}

void ObservationSystem::abort_plan() {
	//...
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
