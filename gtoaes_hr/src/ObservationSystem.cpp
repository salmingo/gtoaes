/**
 * @class ObservationSystem 观测系统集成控制接口
 * @version 0.1
 * @date 2019-10-22
 */
#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include "ObservationSystem.h"
#include "GLog.h"
#include "ADefine.h"
#include "AstroDeviceDef.h"

using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::placeholders;

//////////////////////////////////////////////////////////////////////////////
ObsSysPtr make_obss(const string& gid, const string& uid) {
	return boost::make_shared<ObservationSystem>(gid, uid);
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 公共接口 -------------------*/
//////////////////////////////////////////////////////////////////////////////
ObservationSystem::ObservationSystem(const string& gid, const string& uid) {
	gid_ = gid;
	uid_ = uid;
	keep_alive_ = 60;
	minEle_     = 10.0;
}

ObservationSystem::~ObservationSystem() {

}

bool ObservationSystem::Start() {
	string name = "msgque_" + gid_ + uid_;
	register_message();
	if (!MessageQueue::Start(name.c_str())) return false;
	nfObss_   = boost::make_shared<OBSSInfo>();
	ascproto_ = boost::make_shared<AsciiProtocol>();
	annproto_ = boost::make_shared<AnnexProtocol>(gid_, uid_);
	netrcv_.reset(new char[TCP_PACK_SIZE]);
	thrd_cycle_.reset(new thread(boost::bind(&ObservationSystem::thread_cycle, this)));
	_gLog.Write("Observation System [%s:%s] goes running", gid_.c_str(), uid_.c_str());
	return true;
}

void ObservationSystem::Stop() {
	MessageQueue::Stop();
	interrupt_thread(thrd_acqplan_);
	interrupt_thread(thrd_cycle_);
	_gLog.Write("Observation System [%s:%s] had stopped", gid_.c_str(), uid_.c_str());
}

int ObservationSystem::IsMatched(const string &gid, const string &uid) {
	if (uid == uid_ && gid == gid_) return 1;
	if (uid == "" && (gid == gid_ || gid == "")) return 2;
	return 0;
}

void ObservationSystem::SetGeosite(const string& name, const double lon, const double lat,
		const double alt, const int timezone) {
	_gLog.Write("OBSS[%s:%s] located at [%s, %.4f, %.4f, %.1f]", gid_.c_str(), uid_.c_str(),
			name.c_str(), lon, lat, alt);
	obsite_ = boost::make_shared<ascii_proto_obsite>();
	obsite_->sitename = name;
	obsite_->lon      = lon;
	obsite_->lat      = lat;
	obsite_->alt      = alt;
	obsite_->timezone = timezone;

	ats_.SetSite(lon, lat, alt, timezone);
}

void ObservationSystem::SetElevationLimit(double value) {
	minEle_ = value;
}

void ObservationSystem::RegisterAcquirePlan(const AcquirePlanSlot& slot) {
	cb_acqplan_.connect(slot);
}

void ObservationSystem::SetDBUrl(const char *dburl) {
	dbt_ = boost::make_shared<DBCurl>(dburl);
}

bool ObservationSystem::CoupleMount(TcpCPtr ptr) {
	if (tcpc_mount_.use_count() && tcpc_mount_ != ptr) {
		_gLog.Write(LOG_WARN, "ObservationSystem::CoupleMount()",
				"OBSS[%s:%s] had correlated mount", gid_.c_str(), uid_.c_str());

		return false;
	}
	else if (tcpc_mount_ == ptr) return true;
	else {
		_gLog.Write("Mount[%s:%s] was on-line", gid_.c_str(), uid_.c_str());
		const TCPClient::CBSlot &slot = boost::bind(&ObservationSystem::receive_mount, this, _1, _2);
		ptr->RegisterRead(slot);
		tcpc_mount_ = ptr;
		nfMount_  = boost::make_shared<MountInfo>();
		switch_state();

		return true;
	}
}

bool ObservationSystem::CoupleCamera(TcpCPtr ptr, const string& cid) {
	if (tcpc_camera_.use_count() && tcpc_camera_ != ptr) {
		_gLog.Write(LOG_WARN, "ObservationSystem::CoupleCamera()",
				"OBSS[%s:%s] had correlated camera", gid_.c_str(), uid_.c_str());
		return false;
	}
	else if (tcpc_camera_ == ptr) return true;
	else {
		_gLog.Write("Camera[%s:%s:%s] was on-line", gid_.c_str(), uid_.c_str(), cid.c_str());
		const TCPClient::CBSlot &slot = boost::bind(&ObservationSystem::receive_camera, this, _1, _2);
		ptr->RegisterRead(slot);
		tcpc_camera_ = ptr;
		if (!nfCamera_.unique()) {
			nfCamera_ = boost::make_shared<CameraInfo>();
			cid_ = cid;
		}
		// 测站位置=>相机
		int n;
		const char *s = ascproto_->CompactObsSite(obsite_, n);
		tcpc_camera_->Write(s, n);
		switch_state();

		return true;
	}
}

int ObservationSystem::CoupleFocus(TcpCPtr ptr, const string& cid) {
	if (!tcpc_camera_.unique()) {
		cid_ = cid;
		nfCamera_ = boost::make_shared<CameraInfo>();
	}
	if (!tcpc_focus_.use_count() && cid == cid_) {
		_gLog.Write("focuser[%s:%s:%s] was on-line", gid_.c_str(), uid_.c_str(), cid.c_str());
		tcpc_focus_ = ptr;
	}
	if (tcpc_focus_ == ptr) return 1;
	else {
		_gLog.Write(LOG_WARN, "ObservationSystem::CoupleFocus()",
				"focuser delivered unmatched cid[%s] for camera[%s:%s:%s]",
				cid.c_str(), gid_.c_str(), uid_.c_str(), cid_.c_str());
		return 0;
	}
}

void ObservationSystem::DecoupleFocus() {
	_gLog.Write("focuser[%s:%s:%s] was off-line", gid_.c_str(), uid_.c_str(), cid_.c_str());
	tcpc_focus_.reset();
}

void ObservationSystem::NotifyAsciiProtocol(apbase proto) {
	mutex_lock lck(mtx_apque_);
	apque_.push_back(proto);
	PostMessage(MSG_NEW_PROTOCOL);
}

void ObservationSystem::NotifyFocus(const string &cid, int position) {
#ifdef NDEBUG
	_gLog.Write("focus[%s:%s:%s] position was %d", gid_.c_str(), uid_.c_str(), cid.c_str(), position);
#endif
	if (tcpc_camera_.unique()) {// 焦点位置=>相机
		int n;
		const char *s = ascproto_->CompactFocus(position, n);
		tcpc_camera_->Write(s, n);
	}
}

void ObservationSystem::GetID(string &gid, string &uid) {
	gid = gid_;
	uid = uid_;
}

bool ObservationSystem::IsAlive() {
	if (tcpc_mount_.unique() || tcpc_camera_.unique() || tcpc_focus_.use_count())
		return true;
	ptime::time_duration_type tdt = second_clock::universal_time() - tmclosed_;
	return (tdt.total_seconds() < keep_alive_);
}
//////////////////////////////////////////////////////////////////////////////
/*------------------- 公共接口 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/*------------------- 网络通信 -------------------*/
//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::receive_mount(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_MOUNT : MSG_RECEIVE_MOUNT, ptr);
}

void ObservationSystem::receive_camera(const long ptr, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA : MSG_RECEIVE_CAMERA, ptr);
}

void ObservationSystem::thread_cycle() {
	boost::chrono::seconds period(5);
	string utcMount, utcCamera;

	while(1) {
		this_thread::sleep_for(period);

		if (dbt_.unique()) {// 向数据库发送更新信息
			mutex_lock lck(mtx_database_);
			string tzt = to_iso_string(second_clock::local_time());

			dbt_->UpdateMountLinked(gid_, uid_, nfMount_.unique());
			if (nfMount_.unique()) {
				dbt_->UpdateMountState(gid_, uid_, tzt, nfMount_->state, nfMount_->errcode,
						nfMount_->ra, nfMount_->dec, nfMount_->azi, nfMount_->alt);
			}

			dbt_->UpdateCameraLinked(gid_, uid_, cid_, nfCamera_.unique());
			if (nfCamera_.unique()) {
				dbt_->UpdateCameraState(gid_, uid_, cid_, tzt, nfCamera_->state, nfCamera_->errcode, nfCamera_->coolget);
			}
		}
/*
		if (nfMount_.unique() && nfMount_->utc.size()) {// 检查转台时标
			if (utcMount != nfMount_->utc) utcMount = nfMount_->utc;
			else tcpc_mount_->Close();
		}

		if (nfCamera_.unique() && nfCamera_->utc.size()) {// 检查相机时标
			if (utcCamera != nfCamera_->utc) utcCamera = nfCamera_->utc;
			else tcpc_camera_->Close();
		}
*/
	}
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 网络通信 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/*------------------- 消息机制 -------------------*/
//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::register_message() {
	const CBSlot& slot11 = boost::bind(&ObservationSystem::on_receive_mount,   this, _1, _2);
	const CBSlot& slot12 = boost::bind(&ObservationSystem::on_receive_camera,  this, _1, _2);
	const CBSlot& slot13 = boost::bind(&ObservationSystem::on_new_protocol,    this, _1, _2);
	const CBSlot& slot14 = boost::bind(&ObservationSystem::on_new_plan,        this, _1, _2);
	const CBSlot& slot15 = boost::bind(&ObservationSystem::on_flat_reslew,     this, _1, _2);

	const CBSlot& slot21 = boost::bind(&ObservationSystem::on_close_mount,     this, _1, _2);
	const CBSlot& slot22 = boost::bind(&ObservationSystem::on_close_camera,    this, _1, _2);

	RegisterMessage(MSG_RECEIVE_MOUNT,   slot11);
	RegisterMessage(MSG_RECEIVE_CAMERA,  slot12);
	RegisterMessage(MSG_NEW_PROTOCOL,    slot13);
	RegisterMessage(MSG_NEW_PLAN,        slot14);
	RegisterMessage(MSG_FLAT_RESLEW,     slot15);

	RegisterMessage(MSG_CLOSE_MOUNT,     slot21);
	RegisterMessage(MSG_CLOSE_CAMERA,    slot22);
}

void ObservationSystem::on_receive_mount(const long addr, const long) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	bool isMount;
	apbase proto;

	while (tcpc_mount_->IsOpen() && (pos = tcpc_mount_->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_mount()",
					"too long message from mount[%s:%s]", gid_.c_str(), uid_.c_str());
			tcpc_mount_->Close();
		}
		else {// 读取协议内容并解析执行
			tcpc_mount_->Read(netrcv_.get(), toread);
			netrcv_[pos] = 0;
			proto = ascproto_->Resolve(netrcv_.get());

			if (proto.unique() && ((isMount = iequals(proto->type, APTYPE_MOUNT)) || iequals(proto->type, APTYPE_REG))) {
				if (isMount) process_info_mount(static_pointer_cast<ascii_proto_mount>(proto));
			}
			else {
				_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_mount()",
						"illegal protocol. received: %s", netrcv_.get());
				tcpc_mount_->Close();
			}
		}
	}
}

void ObservationSystem::process_info_mount(apmount proto) {
	int now(proto->state), prev(nfMount_->state);
	static int repark = 0;
	// 更新望远镜指向坐标
	*nfMount_ = proto;

	if (nfMount_->alt <= minEle_) {
		if (now != MOUNT_PARKING && ++repark == 1) {
			_gLog.Write(LOG_WARN, NULL, "orientation[azi=%.4f, alt=%.4f] of mount[%s:%s] was out of safe limit",
					nfMount_->azi, nfMount_->alt, gid_.c_str(), uid_.c_str());
			process_park();
		}
		if (repark > 10) repark = 0;
	}
	else if (now != prev) {
#ifdef NDEBUG
		if (now >= MOUNT_ERROR && now <= MOUNT_TRACKING)
			_gLog.Write("Mount[%s:%s] was %s", gid_.c_str(), uid_.c_str(), MOUNT_STATE_STR[now]);
		else
			_gLog.Write(LOG_WARN, "ObservationSystem::process_info_mount()", "undefined mount state[=%d]", now);
#endif
		if (nfMount_->slewing && prev == MOUNT_SLEWING && (now == MOUNT_TRACKING || now == MOUNT_FREEZE)) {
			_gLog.Write("mount[%s:%s] arrived orientation of [ra=%.4f, dec=%.4f] and [azi=%.4f, alt=%.4f]",
					gid_.c_str(), uid_.c_str(), nfMount_->ra, nfMount_->dec, nfMount_->azi, nfMount_->alt);
			nfMount_->slewing = false;
			if (plan_now_.use_count()) {
				if (nfMount_->HasArrived()) command_expose(nfObss_->exposing ? EXPOSE_RESUME : EXPOSE_START);
				else {
					_gLog.Write(LOG_FAULT, NULL, "mount[%s:%s] arrived wrong target orientation",
							gid_.c_str(), uid_.c_str());
					interrupt_plan();
				}
			}
		}
	}
}

void ObservationSystem::on_receive_camera(const long addr, const long) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;
	TCPClient *tcpc = (TCPClient*) addr;

	while (tcpc->IsOpen() && (pos = tcpc->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_camera()",
					"too long message from camera[%s:%s]", gid_.c_str(), uid_.c_str(), cid_.c_str());
			tcpc->Close();
		}
		else {// 读取协议内容并解析执行
			tcpc->Read(netrcv_.get(), toread);
			netrcv_[pos] = 0;
			proto = ascproto_->Resolve(netrcv_.get());

			if (proto.unique() && iequals(proto->type, APTYPE_CAMERA))
				process_info_camera(static_pointer_cast<ascii_proto_camera>(proto), tcpc);
			else {
				_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_camera()",
						"illegal protocol. received: %s", netrcv_.get());
				tcpc->Close();
			}
		}
	}
}

void ObservationSystem::process_info_camera(apcamera proto, TCPClient* client) {
	int prev = nfCamera_->state;
	int now  = proto->state;
	*nfCamera_ = *proto;

	// 依据状态处理曝光流程
	if (!plan_now_.use_count()) {// 无观测计划
		if (now > CAMCTL_IDLE && prev <= CAMCTL_IDLE) command_expose(EXPOSE_STOP);
	}
	else {// 有观测计划
		if (now > CAMCTL_IDLE && prev <= CAMCTL_IDLE) {// 进入曝光状态
			nfObss_->enter_exposing();
		}
		else if (now <= CAMCTL_IDLE && prev > CAMCTL_IDLE) {// 离开曝光状态
			if (nfObss_->leave_expoing()) {// 所有相机完成曝光流程
				_gLog.Write("Plan[%s] was over", plan_now_->plan_sn.c_str());
				if (dbt_.unique()) {
					mutex_lock lck(mtx_database_);

					string tzt = to_iso_string(second_clock::local_time());
					dbt_->UploadObsplanState(plan_now_->plan_sn, "over", tzt);
				}
				plan_now_->state = OBSPLAN_OVER;
				plan_now_.reset();
				if (nfObss_->state == OBSS_AUTO) cv_acqplan_.notify_one();
			}
		}
		else if (prev != now){// 平场特殊处理
			if (   (now == CAMCTL_WAIT_SYNC && nfObss_->enter_waitsync())
				|| (now == CAMCTL_WAIT_FLAT && nfObss_->enter_waitflat())) {
				PostMessage(MSG_FLAT_RESLEW);
			}
			else if (prev == CAMCTL_WAIT_SYNC) nfObss_->leave_waitsync();
			else if (prev == CAMCTL_WAIT_FLAT) nfObss_->leave_waitflat();
		}
	}
}

void ObservationSystem::on_close_mount(const long addr, const long) {
	_gLog.Write("mount[%s:%s] was off-line", gid_.c_str(), uid_.c_str());
	tcpc_mount_.reset();
	nfMount_.reset();
	tmclosed_ = second_clock::universal_time();
	switch_state();
}

void ObservationSystem::on_close_camera(const long addr, const long) {
	_gLog.Write("camera[%s:%s:%s] was off-line", gid_.c_str(), uid_.c_str(), cid_.c_str());
	nfObss_->leave_expoing();	// 强制离开曝光状态
	nfObss_->leave_waitflat();
	nfObss_->leave_waitsync();
	tcpc_camera_.reset();
	nfCamera_.reset();
	tmclosed_ = second_clock::universal_time();
	switch_state();
}

void ObservationSystem::on_new_protocol(const long, const long) {
	apbase proto;
	string type;
	{// 从队列中提取队头的通信协议
		mutex_lock lck(mtx_apque_);
		proto = apque_.front();
		apque_.pop_front();
		type = proto->type;
	}
	// 分类处理通信协议
	if      (iequals(type, APTYPE_FWHM))     process_fwhm    (from_apbase<ascii_proto_fwhm>     (proto));
	else if (iequals(type, APTYPE_PARK))     process_park    ();
	else if (iequals(type, APTYPE_STOP))     process_autobs  (false);
	else if (iequals(type, APTYPE_ABTPLAN))  interrupt_plan  ();
	else if (iequals(type, APTYPE_HOMESYNC)) process_homesync(from_apbase<ascii_proto_home_sync>(proto));
	else if (iequals(type, APTYPE_PRESLEW))  process_preslew ();
	else if (!nfObss_->automode) {
		if      (iequals(type, APTYPE_START))    process_autobs     ();
		else if (iequals(type, APTYPE_ABTSLEW))  process_abortslew  ();
		else if (iequals(type, APTYPE_ABTIMG))   process_abortimage ();
		else if (iequals(type, APTYPE_SLEWTO))   process_slewto     (from_apbase<ascii_proto_slewto>      (proto));
		else if (iequals(type, APTYPE_TRACK))    process_track      (from_apbase<ascii_proto_track>       (proto));
		else if (iequals(type, APTYPE_TAKIMG))   process_takeimage  (from_apbase<ascii_proto_take_image>  (proto));
	}
	else {
		_gLog.Write(LOG_WARN, NULL, "illegal protocol[%s] for OBSS[%s:%s] was rejected",
				type.c_str(), gid_.c_str(), uid_.c_str());
	}
}

void ObservationSystem::process_autobs(bool mode) {
	if (mode != nfObss_->automode) {
		nfObss_->automode = mode;
		switch_state();
	}
}

void ObservationSystem::process_homesync(aphomesync proto) {
	if (nfMount_.unique() && nfMount_->state == MOUNT_TRACKING) {
		_gLog.Write(LOG_WARN, NULL, "mount[%s:%s] was synchronized to [%.4f, %.4f]",
				gid_.c_str(), uid_.c_str(), proto->ra, proto->dec);
		int n;
		const char *s = ascproto_->CompactHomeSync(proto, n);
		tcpc_mount_->Write(s, n);
	}
	else {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s] was rejected for mount was off-line or not in track mode",
				APTYPE_HOMESYNC, gid_.c_str(), uid_.c_str());
	}
}

void ObservationSystem::process_slewto(apslewto proto) {
	if (tcpc_mount_.unique())
		process_slewto(proto->coorsys, proto->coor1, proto->coor2);
	else {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s] was rejected for mount was off-line",
				APTYPE_SLEWTO, gid_.c_str(), uid_.c_str());
	}
}

void ObservationSystem::process_slewto(int coorsys, double coor1, double coor2) {
	_gLog.Write("mount[%s:%s] will slew to [%.4f, %.4f](%s)", gid_.c_str(), uid_.c_str(),
			coor1, coor2, coorsys == 1 ? "Horizontal" : "Equatorial");
	nfMount_->SetObject(coorsys, coor1, coor2);

	int n;
	const char *s = ascproto_->CompactSlewto(coorsys, coor1, coor2, n);
	tcpc_mount_->Write(s, n);
}

void ObservationSystem::process_track(aptrack proto) {
	if (tcpc_mount_.unique()) process_track(proto->objid, proto->line1, proto->line2);
	else {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s] was rejected for mount was off-line",
				APTYPE_TRACK, gid_.c_str(), uid_.c_str());
	}
}

void ObservationSystem::process_track(const string &objid, const string &line1, const string &line2) {
	_gLog.Write("mount[%s:%s] will start tracking for [%s]", gid_.c_str(), uid_.c_str(), objid.c_str());
	/*!
	 * @date 2019-11-02
	 * - 增加坐标系COORSYS_GUIDE, 用来区分声明引导跟踪模式.
	 * - 在COORSYS_GUIDE下, 采用转台当前指向赤道坐标作为初始位置, 与转台到位位置进行相同性判定. 若相同则认定指向失败
	 */
	nfMount_->SetObject(COORSYS_GUIDE, nfMount_->ra, nfMount_->dec);

	int n;
	const char *s = ascproto_->CompactTrack(objid, line1, line2, n);
	tcpc_mount_->Write(s, n);
}

void ObservationSystem::process_abortslew() {
	if (tcpc_mount_.unique()) {
		_gLog.Write("mount[%s:%s] will abort slew", gid_.c_str(), uid_.c_str());
		int n;
		const char *s = ascproto_->CompactAbortSlew(n);
		tcpc_mount_->Write(s, n);
	}
	else {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s] was rejected for mount was off-line",
				APTYPE_ABTSLEW, gid_.c_str(), uid_.c_str());
	}
}

void ObservationSystem::process_park() {
	if (!nfMount_.unique()) {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s] was rejected for mount was off-line",
				APTYPE_PARK, gid_.c_str(), uid_.c_str());
	}
	else if (nfMount_->state != MOUNT_PARKING && nfMount_->state != MOUNT_PARKED) {
		_gLog.Write("mount[%s:%s] will park", gid_.c_str(), uid_.c_str());
		interrupt_plan();

		int n;
		const char *s = ascproto_->CompactPark(n);
		tcpc_mount_->Write(s, n);
	}
#ifdef NDEBUG
	else {
		_gLog.Write("OBSS[%s:%s] was %s", gid_.c_str(), uid_.c_str(),
				nfMount_->state == MOUNT_PARKING ? "Parking" : "Parked");
	}
#endif
}

void ObservationSystem::process_preslew() {
	if (nfMount_.unique()) {
		_gLog.Write("mount[%s:%s] will do pre-slew", gid_.c_str(), uid_.c_str());
		if (nfMount_->state == MOUNT_PARKED) {// 第一次预指向
			process_slewto(1, 150.0, 30.0);
		}
		else {// 第二次预指向
			process_park();
		}
	}
}

void ObservationSystem::process_takeimage(aptakeimg proto) {
	if (plan_now_.unique()) {
		_gLog.Write(LOG_WARN, NULL, "take_image[%s:%s:%s] was rejected for plan was running",
				gid_.c_str(), uid_.c_str(), proto->cid.c_str());
	}
	else if (!tcpc_camera_.unique()) {
		_gLog.Write(LOG_WARN, NULL, "take_image[%s:%s:%s] was rejected for camera was off-line",
				gid_.c_str(), uid_.c_str(), proto->cid.c_str());
	}
	else if (!(proto->cid.empty() || proto->cid == cid_)) {
		_gLog.Write(LOG_WARN, NULL, "take_image[%s:%s:%s] was rejected for cid doesn't match",
				gid_.c_str(), uid_.c_str(), proto->cid.c_str());
	}
	else {
		string sabbr;
		int imgtyp = check_imgtype(proto->imgtype, sabbr);
		if (imgtyp == IMGTYPE_ERROR || (imgtyp == IMGTYPE_FLAT
				&& (!nfMount_.unique() || nfMount_->state == MOUNT_ERROR))) {
			_gLog.Write(LOG_WARN, NULL, "%s[%s:%s:%s] was rejected for wrong image-type[%s] or mount was off-line",
					APTYPE_TAKIMG, gid_.c_str(), uid_.c_str(), proto->cid.c_str(), proto->imgtype.c_str());
		}
		else {
			_gLog.Write("take_image[%s:%s:%s]. image type is %s",
					gid_.c_str(), uid_.c_str(), cid_.c_str(), proto->imgtype.c_str());
			ptime now = second_clock::universal_time();
			plan_now_ = boost::make_shared<ObservationPlan>();
			plan_now_->plan_type = PLANTYPE_MANUAL;
			plan_now_->plan_sn   = "manual";
			plan_now_->objname   = proto->objname;
			plan_now_->imgtype   = proto->imgtype;
			plan_now_->sabbr     = sabbr;
			plan_now_->iimgtyp   = imgtyp;
			plan_now_->expdur    = proto->expdur;
			plan_now_->frmcnt    = proto->frmcnt;
			plan_now_->state     = OBSPLAN_RUN;
			plan_now_->btime     = now;
			plan_now_->etime     = now + minutes(20);
			// 执行观测计划
			notify_obsplan();
			if (imgtyp == IMGTYPE_FLAT) PostMessage(MSG_FLAT_RESLEW);
			else {
				if (imgtyp > IMGTYPE_FLAT && nfMount_.unique()) notify_orientation();
				command_expose(EXPOSE_START);
			}
		}
	}
}

void ObservationSystem::process_abortimage() {
	if (tcpc_camera_.unique()) {
		_gLog.Write("camera[%s:%s:%s] will abort image", gid_.c_str(), uid_.c_str(), cid_.c_str());
		command_expose(EXPOSE_STOP);
	}
	else {
		_gLog.Write(LOG_WARN, NULL, "%s[%s:%s:%s] was rejected for camera was off-line",
				APTYPE_ABTIMG, gid_.c_str(), uid_.c_str(), cid_.c_str());
	}
}

void ObservationSystem::process_fwhm(apfwhm proto) {
#if NDEBUG
	_gLog.Write("fwhm[%s:%s:%s] was %.2f", gid_.c_str(), uid_.c_str(), cid_.c_str(), proto->value);
#endif
	if (tcpc_focus_.use_count()) {
		int n;
		const char *s = annproto_->CompactFwhm(cid_, proto->value, n);
		tcpc_focus_->Write(s, n);
	}
}

void ObservationSystem::on_new_plan(const long, const long) {
	_gLog.Write("OBSS[%s:%s] starts running plan[%s] ", gid_.c_str(), uid_.c_str(), plan_now_->plan_sn.c_str());
	// 改变计划状态
	plan_now_->state = OBSPLAN_RUN;
	// 将计划发送给相机
	notify_obsplan();
	// 控制转台指向
	if (plan_now_->plan_type == PLANTYPE_TRACK) {
		process_track(plan_now_->objname, plan_now_->line1, plan_now_->line2);
	}
	else if (plan_now_->plan_type == PLANTYPE_POINT) {
		process_slewto(plan_now_->coorsys, plan_now_->coor1, plan_now_->coor2);
	}
	else if (plan_now_->plan_type == PLANTYPE_MANUAL) {
		if (plan_now_->iimgtyp == IMGTYPE_FLAT) PostMessage(MSG_FLAT_RESLEW);
		else {
			if (plan_now_->iimgtyp > IMGTYPE_DARK) notify_orientation();
			command_expose(EXPOSE_START);
		}
	}
	// 将计划发送给数据库
	if (dbt_.unique()) {
		mutex_lock lck(mtx_database_);
		string tzt = to_iso_string(second_clock::local_time());
		dbt_->UploadObsplanState(plan_now_->plan_sn, "begin", tzt);
	}
}

void ObservationSystem::on_flat_reslew(const long, const long) {
	bool reslew(true);
	// 平场采集失败时相机返回WAIT_SYNC. 所有相机都失败时, 每4分钟一次重新指向
	if (nfObss_->waitsync && nfObss_->waitsync == nfObss_->exposing) {
		reslew = (second_clock::universal_time() - nfObss_->lastflat).total_seconds() > 240;
	}

	if (!reslew) command_expose(EXPOSE_RESUME);
	else {// 重新指向
		double azi, alt;
		flat_orientation(azi, alt);
		notify_orientation();
		process_slewto(1, azi, alt);
	}
}

void ObservationSystem::flat_orientation(double &azi, double &alt) {
	ptime now = second_clock::local_time();
	double azi0 = now.time_of_day().hours() > 12 ? 100.0 : 180.0;	// 北零点
	nfObss_->lastflat = now;
	// 生成天顶坐标
	azi = drand48() * 90.0 + azi0;
	alt = 80 + drand48() * 5.0;
}

void ObservationSystem::command_expose(int command) {
	int n;
	const char *s = ascproto_->CompactExpose(command, n);
	tcpc_camera_->Write(s, n);
}

void ObservationSystem::notify_orientation() {
	apmount proto = boost::make_shared<ascii_proto_mount>();
	proto->gid = gid_;
	proto->uid = uid_;
	proto->state   = nfMount_->state;
	proto->errcode = nfMount_->errcode;
	proto->ra = nfMount_->ra;
	proto->dec = nfMount_->dec;
	proto->azi = nfMount_->azi;
	proto->alt = nfMount_->alt;

	int n;
	const char *s;
	s = ascproto_->CompactMount(proto, n);
	tcpc_camera_->Write(s, n);
}

void ObservationSystem::notify_obsplan() {
	int n;
	const char *s;
	apobject proto = boost::make_shared<ascii_proto_object>(); // 为各个相机分别生成参数

	proto->plan_type = plan_now_->plan_type;
	proto->plan_sn   = plan_now_->plan_sn;
	proto->objname   = plan_now_->objname;
	proto->btime     = to_iso_string(plan_now_->btime);
	proto->etime     = to_iso_string(plan_now_->etime);
	proto->imgtype   = plan_now_->imgtype;
	proto->sabbr     = plan_now_->sabbr;
	proto->iimgtyp   = plan_now_->iimgtyp;
	proto->expdur    = plan_now_->expdur;
	proto->frmcnt    = plan_now_->frmcnt;
	s = ascproto_->CompactObject(proto, n);
	tcpc_camera_->Write(s, n);
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 消息机制 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测计划与自动观测逻辑 -------------------*/
//////////////////////////////////////////////////////////////////////////////
void ObservationSystem::switch_state() {
	int state;

	if (nfObss_->automode && nfMount_.unique() && nfCamera_.unique()) state = OBSS_AUTO;
	else if (nfMount_.unique() || nfCamera_.unique()) state = OBSS_MANUAL;
	else state = OBSS_ERROR;
	if (state != nfObss_->state) {
		_gLog.Write("OBSS[%s:%s] switches into %s mode", gid_.c_str(), uid_.c_str(),
				state == OBSS_AUTO ? "AUTO" : (state == OBSS_MANUAL ? "MANUAL" : "ERROR"));
		if (state == OBSS_AUTO) thrd_acqplan_.reset(new thread(boost::bind(&ObservationSystem::thread_acqplan, this)));
		else if (nfObss_->state == OBSS_AUTO) {
			interrupt_thread(thrd_acqplan_);
			process_park();
		}
		nfObss_->state = state;
	}
}

void ObservationSystem::interrupt_plan() {
	if (plan_now_.use_count()) {
		_gLog.Write("Plan[%s] was interrupted", plan_now_->plan_sn.c_str());
		if (nfObss_->exposing) command_expose(EXPOSE_STOP);
		else {
			if (dbt_.unique()) {
				mutex_lock lck(mtx_database_);

				string tzt = to_iso_string(second_clock::local_time());
				dbt_->UploadObsplanState(plan_now_->plan_sn, "over", tzt);
			}
			plan_now_->state = OBSPLAN_OVER;
			plan_now_.reset();
		}
	}
}

void ObservationSystem::thread_acqplan() {
	mutex mtx;
	mutex_lock lck(mtx);
	boost::chrono::seconds period(30);

	/*
	 * 申请观测计划条件:
	 * - 定时
	 * - 触发: 当完成在执行计划后
	 * - 转台状态为: TRACK, FREEZE, PARKED之一
	 */
	while (1) {
		cv_acqplan_.wait_for(lck, period);

		if (!plan_now_.use_count() && nfMount_->IsStable()) {
			ObsPlanPtr plan = *cb_acqplan_(gid_, uid_);
			if (plan.use_count()) {
				plan_now_ = plan;
				PostMessage(MSG_NEW_PLAN);
			}
		}
		else if (plan_now_.use_count()) {// 检查当前计划有效性
			ptime now = second_clock::universal_time();
			ptime::time_duration_type tdt = now - plan_now_->etime;
			if (tdt.total_seconds() > int(plan_now_->expdur + 10)) interrupt_plan();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*------------------- 观测计划与自动观测逻辑 -------------------*/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/* 数据库 */
//////////////////////////////////////////////////////////////////////////////
