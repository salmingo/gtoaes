/*
 * @file ObservationSystem.cpp 封装管理观测系统
 * @author 卢晓猛
 * @date 2017-1-29
 * @version 0.3
 *
 * @date 2017-05-07
 * @version 0.4
 * - 添加对转台信息处理. 注意事项: 与转台之间的单元和相机标志采用的是全局标志
 */
#include <boost/bind/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/format.hpp>
#include <math.h>
#include <stdlib.h>
#include "ObservationSystem.h"
#include "ADefine.h"
#include "GLog.h"

using std::string;
using namespace AstroUtil;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::placeholders;

obssptr make_obss(const string& gid, const string& uid) {
	gLog.Write("Try to create Observation System <%s:%s>", gid.c_str(), uid.c_str());
	obssptr obss = boost::make_shared<ObservationSystem>(gid, uid);
	if (!obss->Start()) obss.reset();
	return obss;
}

ObservationSystem::ObservationSystem(const string& gid, const string& uid) {
	tmflag_  = 0;
	groupid_ = gid;
	unitid_  = uid;
	errMountTime_ = 0;
	errSafe_ = 0;
	seqGuide_ = 0;
	seqPosErr_ = 0;
}

ObservationSystem::~ObservationSystem() {
	if (thrd_waitplan_.unique()) {
		thrd_waitplan_->interrupt();
		thrd_waitplan_->join();
		thrd_waitplan_.reset();
	}
	gLog.Write("Observation System <%s:%s> is destroyed", groupid_.c_str(), unitid_.c_str());
}

bool ObservationSystem::Start() {
	bool rslt(false);

	if (groupid_.empty() || unitid_.empty()) {
		gLog.Write(NULL, LOG_FAULT, "Failed to create Observation System <%s:%s>",
				groupid_.c_str(), unitid_.c_str());
	}
	else {
		string name = "msgque_" + groupid_ + unitid_;
		if (!start(name.c_str())) {
			gLog.Write(NULL, LOG_FAULT, "Failed to create message queue for <%s:%s>", groupid_.c_str(), unitid_.c_str());
		}
		else {
			gLog.Write("Observation System <%s:%s> goes running", groupid_.c_str(), unitid_.c_str());
			rslt = true;
			tmflag_ = second_clock::universal_time().time_of_day().total_seconds();
			ats_ = boost::make_shared<ATimeSpace>();
			bufrcv_.reset(new char[2048]);
			ascproto_ = boost::make_shared<ascii_proto>();
			mntproto_ = boost::make_shared<mount_proto>();
			obsplan_  = boost::make_shared<ascproto_object_info>();
			waitplan_ = boost::make_shared<ascproto_object_info>();
			obsplan_->reset();
			waitplan_->reset();
			register_messages();
		}
	}
	return rslt;
}

// 查看时间戳
int ObservationSystem::get_timeflag() {
	return tmflag_;
}

// 检测系统标志是否在策略上一致
bool ObservationSystem::is_matched(const string& gid, const string& uid) {
	using namespace boost;
	return ((gid.empty() && uid.empty())							// 全为空时操作对所有观测系统有效
			|| (uid.empty() && iequals(gid, groupid_))				// 单元标志为空时, 组标志必须相同
			|| (iequals(gid, groupid_) && iequals(uid, unitid_))	// 全不为空时, 二者必须相同
			);														// 不接受单元标志相同, 而组标志相同
}

// 设置系统所在位置地理信息
void ObservationSystem::set_geosite(const string& name, double lgt, double lat, double alt) {
	gLog.Write("<%s:%s> located at <%s, %.5f, %.5f, %.1f>",
			groupid_.c_str(), unitid_.c_str(),
			name.c_str(), lgt, lat, alt);
	ats_->SetSite(lgt, lat, alt);
}

// 关联观测系统与转台网络资源
void ObservationSystem::CoupleMount(tcpcptr& client) {
	mutex_lock lck(mutex_mount_);
	if (client != tcp_mount_) {
		gLog.Write("Mount<%s:%s> is on-line", groupid_.c_str(), unitid_.c_str());
		tcp_mount_ = client;
		mntnf_ = boost::make_shared<mount_info>();
		errMountTime_ = 0;
		errSafe_ = 0;
		seqPosErr_ = 0;
	}
}

// 解联观测系统与转台网络资源
void ObservationSystem::DecoupleMount(tcpcptr& client) {
	mutex_lock lck(mutex_mount_);
	if (client == tcp_mount_) {
		gLog.Write("Mount<%s:%s> is off-line", groupid_.c_str(), unitid_.c_str());
		tcp_mount_.reset();
		mntnf_.reset();
	}
}

// 关联观测系统与相机网络资源
bool ObservationSystem::CoupleCamera(tcpcptr client, const string gid, const string uid, const string cid) {
	mutex_lock lck(mutex_camera_);

	bool rslt(true);
	camvec::iterator it;
	for (it = tcp_camera_.begin(); (it != tcp_camera_.end()) && !iequals(cid, (*it)->id); ++it);
	if (it != tcp_camera_.end()) {
		gLog.Write(NULL, LOG_FAULT, "<%s:%s> had related camera <%s>",
				gid.c_str(), uid.c_str(), cid.c_str());
		rslt = false;
	}
	else {
		gLog.Write("Camera<%s:%s:%s> is on-line", gid.c_str(), uid.c_str(), cid.c_str());

		// 更新回调函数
		const tcpc_cbtype& slot = boost::bind(&ObservationSystem::ReceiveCamera, this, _1, _2);
		client->register_receive(slot);
		// 建立关联关系
		camptr newcam = boost::make_shared<one_camera>(client, cid);
		tcp_camera_.push_back(newcam);
		// 通知相机已成功注册
		ascproto_camera_info proto;
		int n;
		proto.reset();
		proto.group_id = gid;
		proto.unit_id  = uid;
		proto.camera_id= cid;
		const char* compacted = ascproto_->compact_camera_info(&proto, n);
		client->write(compacted, n);
	}

	return rslt;
}

// 解联观测系统与相机网络资源
bool ObservationSystem::DecoupleCamera(const long param) {
	mutex_lock lck(mutex_camera_);
	tcp_client *client = (tcp_client*) param;
	camvec::iterator it;
	for (it = tcp_camera_.begin(); (it != tcp_camera_.end()) && (client != (*it)->tcpcli.get()); ++it);
	if (it != tcp_camera_.end()) {
		post_message(MSG_CLOSE_CAMERA, param);
		return true;
	}
	return false;
}

// 关联观测系统与转台附属设备网络资源
void ObservationSystem::CoupleMountAnnex(tcpcptr& client) {
	mutex_lock lck(mutex_mountannex_);
	if (client != tcp_mountannex_) {
		gLog.Write("Mount-Annex<%s:%s> is on-line", groupid_.c_str(), unitid_.c_str());
		tcp_mountannex_ = client;
	}
}

// 解联观测系统与转台附属设备网络资源
void ObservationSystem::DecoupleMountAnnex(tcpcptr& client) {
	mutex_lock lck(mutex_mountannex_);
	if (client == tcp_mountannex_) {
		gLog.Write("Mount-Annex<%s:%s> is off-line", groupid_.c_str(), unitid_.c_str());
		tcp_mountannex_.reset();
	}
}

///////////////////////////////////////////////////////////////////////////////
/*-------------------------------- 响应网络指令 --------------------------------*/
/* 来自客户端或数据库的网络消息 */
void ObservationSystem::append_protocol(const char* type, apbase proto) {
	mutex_lock lck(mutex_proto_);
	protoptr one = boost::make_shared<one_ascproto>();
	one->type = type;
	one->body = proto;
	protovec_.push_back(one);
	post_message(MSG_NEW_PROTOCOL);
}

void ObservationSystem::notify_take_image(apbase proto) {
	append_protocol("take_image", proto);
}

void ObservationSystem::notify_abort_slew() {
	apbase proto;
	append_protocol("abort_slew", proto);
}

void ObservationSystem::notify_append_gwac(apbase proto) {
	append_protocol("append_gwac", proto);
}

void ObservationSystem::notify_slewto(apbase proto) {
	append_protocol("slewto", proto);
}

void ObservationSystem::notify_guide(apbase proto) {
	append_protocol("guide", proto);
}

void ObservationSystem::notify_focus(apbase proto) {
	append_protocol("focus", proto);
}

void ObservationSystem::notify_fwhm(apbase proto) {
	append_protocol("fwhm", proto);
}

void ObservationSystem::notify_abort_image(apbase proto) {
	append_protocol("abort_image", proto);
}

void ObservationSystem::notify_home_sync(apbase proto) {
	append_protocol("home_sync", proto);
}

void ObservationSystem::notify_start_gwac() {
	apbase proto;
	append_protocol("start_gwac", proto);
}

void ObservationSystem::notify_stop_gwac() {
	apbase proto;
	append_protocol("stop_gwac", proto);
}

void ObservationSystem::notify_find_home() {
	apbase proto;
	append_protocol("find_home", proto);
}

void ObservationSystem::notify_park() {
	apbase proto;
	append_protocol("park", proto);
}

void ObservationSystem::notify_mcover(apbase proto) {
	append_protocol("mcover", proto);
}

/* 来自转台的工作状态 */
// 转台工作状态
void ObservationSystem::notify_mount_state(int state) {
	tmflag_ = tcp_mount_->get_timeflag();
	mutex_lock lck(mutex_mount_);

	if (state != mntnf_->state) {/* 更新转台工作状态 */
		mntnf_->state = state;
		if (state <= MOUNT_FIRST || state >= MOUNT_LAST) {
			gLog.Write(NULL, LOG_WARN, "undefined mount state <%d> on <%s:%s>", state,
					groupid_.c_str(), unitid_.c_str());
		}
		else {
			gLog.Write("mount<%s:%s> goes into <%s>",
					groupid_.c_str(), unitid_.c_str(), mount_state_desc[state].c_str());

			int item(0);
			if (state == MOUNT_TRACKING) {
				if      (systate_.guiding) item = 2;
				else if (systate_.slewing) item = 1;
				systate_.enter_tracking();
			}
			else if (state == MOUNT_PARKED && systate_.parking) {
				item = 3;
				systate_.parking = 0;
			}
			if (item && item < 3) {// 进入跟踪状态后, 判断否要做后续处理
				gLog.Write("<%s:%s> arrives at <%.4f, %.4f>[degree]", groupid_.c_str(), unitid_.c_str(),
						mntnf_->ra00, mntnf_->dc00);
				post_message(MSG_MOUNT_CHANGED, item);
			}
		}
	}
}

// 转台时标
void ObservationSystem::notify_mount_utc(boost::shared_ptr<mntproto_utc> proto) {
	mutex_lock lck(mutex_mount_);

	ptime now = second_clock::universal_time();
	tmflag_ = now.time_of_day().total_seconds();
	mntnf_->utc = proto->utc;
	try {
		/*
		 * ss < 0: 转台时钟较服务器慢; ss > 0: 比服务器时钟快
		 */
		ptime utc = from_iso_extended_string(proto->utc);
		ptime::time_duration_type dt = utc - now;
		ptime::time_duration_type::tick_type ss = dt.total_milliseconds();
		if ((ss >= 50 || ss <= -50) && (++errMountTime_ % 6000 == 1)) {
			gLog.Write(NULL, LOG_WARN, "mount-clock<%s:%s> drifts for %.3f seconds",
					groupid_.c_str(), unitid_.c_str(), ss * 0.001);
		}
	}
	catch(...) {
		if (++errMountTime_ % 6000 == 1) {
			gLog.Write(NULL, LOG_WARN, "mount<%s:%s> tell wrong time <%s> ",
					groupid_.c_str(), unitid_.c_str(), mntnf_->utc.c_str());
		}
	}
}

// 转台位置
void ObservationSystem::notify_mount_position(boost::shared_ptr<mntproto_position> proto) {
	tmflag_ = tcp_mount_->get_timeflag();
	/* 记录转台位置 */
	mutex_lock lck(mutex_mount_);
	double era = fabs(proto->ra  - mntnf_->ra00);
	double edc = fabs(proto->dec - mntnf_->dc00);
	double azi, alt;
	bool safe = SafePosition(proto->ra, proto->dec, NULL);

	mntnf_->ra00 = proto->ra;
	mntnf_->dc00 = proto->dec;

	if (!(safe || systate_.parking)) {
		gLog.Write("position<%.4f, %.4f> of <%s:%s> is out of safe range",
				proto->ra, proto->dec, groupid_.c_str(), unitid_.c_str());
		if (++errSafe_ >= 2) post_message(MSG_OUT_LIMIT);
	}
	else if (safe) {
		errSafe_ = 0;
		bool moving = systate_.slewing || systate_.guiding || systate_.parking;
		/*
		 * 检查是否由"动"至"静"
		 * - 转台反馈错误状态
		 * - 三种运动模式
		 */
		if (mntnf_->state < MOUNT_ERROR && moving) {
			double t(0.008); // 指向或导星到位阈值: 28.8角秒

			if (era > 180) era = 360.0 - era;
			if (fabs(edc) < t && fabs(era) < t) {// 位置变化小于阈值
				int item(0);

				if      (systate_.stable_slew())  item = 1;
				else if (systate_.stable_guide()) item = 2;
				else if (systate_.stable_park())  item = 3;

				if (item == 3) gLog.Write("mount<%s:%s> goes into parked", groupid_.c_str(), unitid_.c_str());
				else if (item) {// 进入跟踪状态后, 判读是否要做后续处理
					gLog.Write("<%s:%s> arrives at <%.4f, %.4f>[degree]",
							groupid_.c_str(), unitid_.c_str(), mntnf_->ra00, mntnf_->dc00);
					post_message(MSG_MOUNT_CHANGED, item);
				}
			}
		}
		/* 检查是否需要由"静"至"动": 主要解决初次指向偏差较大触发零点同步后, 转台位置偏差
		 * - 观测计划编号小于INT_MAX, 即非手动触发曝光
		 * - 设定指向目标
		 * - 转台反馈状态正确且处于跟踪状态
		 * - 转台反馈状态错误且不处于三种运动状态
		 */
/*		else if (obsplan_->op_sn >= 0 && obsplan_->op_sn < INT_MAX && valid_ra(mntnf_->ora00) && valid_dc(mntnf_->odc00)
				&& (mntnf_->state == MOUNT_TRACKING || (mntnf_->state < MOUNT_ERROR && !moving))) {
			double tslew(0.5); // 2017-06-27. 30角分
			era = mntnf_->ora00 - mntnf_->dra - mntnf_->ra00;
			edc = mntnf_->dc00 - mntnf_->odc00 - mntnf_->ddc;
			if (era > 180.0) era -= 360.0;
			else if (era < -180.0) era += 360.0;
			if (fabs(edc) >= tslew || fabs(era * cos(mntnf_->odc00 * D2R)) >= tslew) {// 需要重新指向
				if (++seqPosErr_ == 3) {
					gLog.Write("PE<%.4f, %.4f>[degree] of <%s:%s> requires re-slew to <%.4f, %.4f>[degree]%s",
						era, edc, groupid_.c_str(), unitid_.c_str(), mntnf_->ora00, mntnf_->odc00,
						systate_.lighting ? ". pause exposure furthermore as light required" : "");

					int n;
					if (systate_.lighting) {
						// 暂停曝光
						const char *compacted = ascproto_->compact_expose(EXPOSE_PAUSE, n);
						WriteToCamera(compacted, n, "");
					}
					seqPosErr_ = 0;
					systate_.begin_slew(); // 设立监测点
					// 重新指向
					const char *compacted = mntproto_->compact_slew(groupid_, unitid_,
						mntnf_->ora00 - mntnf_->dra, mntnf_->odc00 + mntnf_->ddc, n);
					tcp_mount_->write(compacted, n);
				}
			}
			else if (seqPosErr_) seqPosErr_ = 0;
		}
*/
	}
}

// 焦点位置
void ObservationSystem::notify_mount_focus(boost::shared_ptr<mntproto_focus> proto) {
	tmflag_ = tcp_mountannex_->get_timeflag();
	mutex_lock lck(mutex_camera_);
	string cid;
	camvec::iterator it;
	int focus, n;

	cid = proto->camera_id;
	focus = proto->position;
	// 定位相机
	for (it = tcp_camera_.begin(); it != tcp_camera_.end() && !iequals(cid, (*it)->id); ++it);
	if ((it != tcp_camera_.end()) && (focus != (*it)->info->focus)) {
		(*it)->info->focus = focus;
		gLog.Write("focus<%s:%s:%s> is <%d>", groupid_.c_str(), unitid_.c_str(), cid.c_str(), focus);
		// 通知相机焦点位置发生变化
		const char* compacted = ascproto_->compact_focus((const double) focus, n);
		(*it)->tcpcli->write(compacted, n);
	}
}

// 镜盖开关状态
void ObservationSystem::notify_mount_mcover(boost::shared_ptr<mntproto_mcover> proto) {
	tmflag_ = tcp_mountannex_->get_timeflag();

	mutex_lock lck(mutex_camera_);
	string cid;
	camvec::iterator it;
	int state;

	for (int i = 0; i < proto->n; ++i) {
		cid = proto->state[i].camera_id;
		state = proto->state[i].state;
		for (it = tcp_camera_.begin(); it != tcp_camera_.end() && !iequals(cid, (*it)->id); ++it);
		if ((it != tcp_camera_.end()) && (state != (*it)->info->mcover)) {
			(*it)->info->mcover = state;
			if (state > MC_FIRST && state < MC_LAST) {
				gLog.Write("mirror-cover<%s:%s:%s> is %s",
						groupid_.c_str(), unitid_.c_str(), cid.c_str(), mc_state_desc[state + 2].c_str());
			}
			else {
				gLog.Write(NULL, LOG_WARN, "undefined mirror-cover state<%d> for <%s:%s:%s>",
						state, groupid_.c_str(), unitid_.c_str(), cid.c_str());
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// 发送常规信息到指定相机
void ObservationSystem::WriteToCamera(const char* data, const int n, const std::string& cid) {
	mutex_lock lck(mutex_camera_);
	camvec::iterator it;
	bool any(cid.empty());

	for (it = tcp_camera_.begin(); it != tcp_camera_.end(); ++it) {
		if (any || iequals(cid, (*it)->id)) {
			(*it)->tcpcli->write(data, n);
			if (!any) break;
		}
	}
}

/*!
 * @brief 向相机发送控制指令
 * @param cmd   曝光指令
 * @param mode  模式
 *              0: 导星相机FFoV
 *              1: 拼接相机JFoV
 *              2: 所有相机
 */
void ObservationSystem::WriteToCamera(EXPOSE_COMMAND cmd, EXPMODE mode) {
	mutex_lock lck(mutex_camera_);
	camvec::iterator it;
	int n;
	const char *compacted = ascproto_->compact_expose(cmd, n);

	for (it = tcp_camera_.begin(); it != tcp_camera_.end(); ++it) {
		if ((mode == EXPMODE_ALL)	// 所有相机
			|| ((*it)->type == CAMERA_TYPE_FFOV && mode == EXPMODE_GUIDE)	// 导星相机且导星模式
			|| ((*it)->type == CAMERA_TYPE_JFOV && mode == EXPMODE_JOINT)) {	// 拼接相机且拼接模式
				(*it)->tcpcli->write(compacted, n);
				if (mode == EXPMODE_GUIDE) break;
		}
	}
}

// 发送目标信息到指定相机
void ObservationSystem::WriteObject(boost::shared_ptr<ascproto_object_info> nf, const std::string &cid) {
	mutex_lock lck(mutex_camera_);
	camvec::iterator it;
	bool any(cid.empty());
	int n;
	const char *data = ascproto_->compact_object_info(nf.get(), n);

	for (it = tcp_camera_.begin(); it != tcp_camera_.end(); ++it) {
		if (any || iequals(cid, (*it)->id)) {
			(*it)->info->imgtype = nf->iimgtype;
			(*it)->fwhm = 0.0;
			(*it)->tcpcli->write(data, n);
			if (!any) break;
		}
	}
}

int ObservationSystem::IsExposing(const std::string& cid) {
	if (cid.empty()) return AnyExposing();

	mutex_lock lck(mutex_camera_);
	camvec::iterator it;
	camnfptr nf;
	int state(-1);

	for (it = tcp_camera_.begin(); (it != tcp_camera_.end()) && !iequals(cid, (*it)->id); ++it);
	if (it != tcp_camera_.end()) {
		nf = (*it)->info;
		state = nf->state <= CAMCTL_IDLE ? 0 : ((nf->imgtype <= IMGTYPE_DARK) ? 1 : 2);
	}

	return state;
}

int ObservationSystem::AnyExposing() {
	return !tcp_camera_.size() ? -1 : (systate_.exposing ? (systate_.lighting  ? 2 : 1) : 0);
}

void ObservationSystem::RandomZenith(double &ra, double &dc) {
	mutex_lock lck(mutex_ats_);

	ptime now = second_clock::universal_time();
	ptime::date_type date = now.date();
	double fd = now.time_of_day().total_seconds() / DAYSEC;
	double ha;
	ats_->SetUTC(date.year(), date.month(), date.day(), fd);
	ats_->Horizon2Eq(0, 90 * D2R, ha, dc);
	srand(now.time_of_day().total_microseconds());
	ha += ((double(rand()) / RAND_MAX - 0.5) * 10 * D2R); // 加入5度幅度随机量, 尝试避免大尺度结构云层带来的影响
	dc += ((double(rand()) / RAND_MAX - 0.5) * 10 * D2R);
	ra = ats_->reduce(ats_->LocalMeanSiderealTime() - ha, A2PI) * R2D;
	dc *= R2D;
	systate_.validflat = false;
	systate_.lastflat = now;	
}

bool ObservationSystem::SafePosition(double ra0, double dc0, ptime *at) {
	mutex_lock lck(mutex_ats_);
	/* 近似算法:
	 * - 直接使用J2000坐标替代真位置, 检测位置安全性. 其偏差带来影响可忽略.
	 * - 近似算法换取快速处理速度
	 */
	double azi, alt;
	ptime now = at == NULL ? second_clock::universal_time() : *at;
	ptime::date_type ymd = now.date();
	double fd = now.time_of_day().total_seconds() / DAYSEC;
	ats_->SetUTC(ymd.year() & 0xFFFF, ymd.month() & 0xFF, ymd.day() & 0xFF, fd);
	ats_->Eq2Horizon(ats_->LocalMeanSiderealTime() - ra0 * D2R, dc0 * D2R, azi, alt);

	return (alt >= 20.0 * D2R);
}

int ObservationSystem::ValidFlat(ptime &start, ptime &stop) {
	mutex_lock lck(mutex_ats_);
	start = ptime(not_a_date_time);
	stop  = ptime(not_a_date_time);

	double sunrise, sunset;
	ptime utc = second_clock::universal_time();
	ptime::date_type date = utc.date();
	ats_->SetUTC(date.year(), date.month(), date.day(), 0.5);
	if (ats_->TwilightTime(sunrise, sunset, 1)) return 3; // 极昼或极夜, 不适合做平场?
	ptime ltm = second_clock::local_time();
	double hours = ltm.time_of_day().total_seconds() / 3600.0; // 当前本地时对应的小时数
	int am_pm = hours <= 12.0 ? 1 : 2;
	if ((am_pm == 1 && hours > (sunrise - 0.16)) // 晨光始: 时间过迟, 天光变亮
			|| (am_pm == 2 && hours > (sunset + 0.42))) // 昏影终: 时间过迟, 天光变暗
		return 0;

	return am_pm;
}

///////////////////////////////////////////////////////////////////////////////
void ObservationSystem::ReceiveCamera(const long client, const long ec) {
	post_message(ec == 0 ? MSG_RECEIVE_CAMERA : MSG_CLOSE_CAMERA, client);
}

void ObservationSystem::register_messages() {
	const mqb_cbtype& slot1 = boost::bind(&ObservationSystem::OnReceiveCamera, this, _1, _2);
	const mqb_cbtype& slot2 = boost::bind(&ObservationSystem::OnCloseCamera,   this, _1, _2);
	const mqb_cbtype& slot3 = boost::bind(&ObservationSystem::OnMountChanged,  this, _1, _2);
	const mqb_cbtype& slot4 = boost::bind(&ObservationSystem::OnNewPlan,       this, _1, _2);
	const mqb_cbtype& slot5 = boost::bind(&ObservationSystem::OnOutLimit,      this, _1, _2);
	const mqb_cbtype& slot6 = boost::bind(&ObservationSystem::OnNewProtocol,   this, _1, _2);
	const mqb_cbtype& slot7 = boost::bind(&ObservationSystem::OnFlatReslew,    this, _1, _2);

	register_message(MSG_RECEIVE_CAMERA, slot1);
	register_message(MSG_CLOSE_CAMERA,   slot2);
	register_message(MSG_MOUNT_CHANGED,  slot3);
	register_message(MSG_NEW_PLAN,       slot4);
	register_message(MSG_OUT_LIMIT,      slot5);
	register_message(MSG_NEW_PROTOCOL,   slot6);
	register_message(MSG_FLAT_RESLEW,    slot7);
}

void ObservationSystem::OnReceiveCamera(long param1, long param2) {
	mutex_lock lck(mutex_camera_);

	tcp_client* client = (tcp_client*) param1;
	tmflag_ = client->get_timeflag();

	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	string proto_type;
	apbase proto_body;
	/* 定位相机 */
	camnfptr camnf;
	camvec::iterator it;
	for (it = tcp_camera_.begin(); it != tcp_camera_.end() && client != (*it)->tcpcli.get(); ++it);
	camnf = (*it)->info;

	/* 处理接收到的网络信息 */
	while (client->is_open() && (pos = client->lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_BUFF_SIZE) {// 有效性判定
			gLog.Write(NULL, LOG_FAULT, "Camera<%s:%s:%s> receives over-long protocol",
					groupid_.c_str(), unitid_.c_str(), (*it)->id.c_str());
			client->close();
		}
		else {// 解析协议
			client->read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;
			proto_body = ascproto_->resolve(bufrcv_.get(), proto_type);
			if (iequals(proto_type, "camera_info")) ProcessCameraProtocol(camnf, boost::static_pointer_cast<ascproto_camera_info>(proto_body));
			else {
				gLog.Write(NULL, LOG_FAULT, "Camera<%s:%s:%s> receives undefined protocol",
						groupid_.c_str(), unitid_.c_str(), (*it)->id.c_str());
				client->close();
			}
		}
	}
}

void ObservationSystem::OnCloseCamera(long param1, long param2) {
	mutex_lock lck1(mutex_camera_);
	tcp_client* client = (tcp_client*) param1;
	camvec::iterator it;

	for (it = tcp_camera_.begin(); it != tcp_camera_.end() && client != (*it)->tcpcli.get(); ++it);
	if (it != tcp_camera_.end()) {
		gLog.Write("Camera<%s:%s:%s> is off-line", groupid_.c_str(), unitid_.c_str(), (*it)->id.c_str());
		// 重置观测计划
		if ((*it)->info->state > CAMCTL_IDLE // 脱机相机状态是'曝光'
				&& systate_.leave_expose((*it)->info->imgtype) // 所有相机都已'结束'曝光
				&& obsplan_->op_sn >= 0)  {// 在执行观测计划
			gLog.Write(NULL, LOG_WARN, "plan<%d> is out of service for cameras are off-line", obsplan_->op_sn);
			obsplan_->reset();
		}
		// 释放资源
		tcp_camera_.erase(it);
	}
}

/* OnMountChanged()触发条件:
 * - 使用本系统驱动转台运动
 * - 转台状态发生变化
 * 前置条件:
 * - 手动指向到位
 * - 按观测计划指向到位
 * - 同步零点触发重新指向
 * - 转台跳动触发重新指向
 * - 导星到位
 */
void ObservationSystem::OnMountChanged(long param1, long param2) {
	mutex_lock lck(mutex_mount_);

	double t(AS2D * 1200); // 指向偏差(轴系)阈值: 20角分(2017-06-27. 传统上, 应采用Nx测角分辨率, N≈3-5)
	double era = mntnf_->ora00 - mntnf_->dra - mntnf_->ra00;
	double edc = mntnf_->dc00 - mntnf_->odc00 - mntnf_->ddc;

	if (era > 180.0) era -= 360.0;
	else if (era < -180.0) era += 360.0;
	if (fabs(edc) > t || fabs(era * cos(mntnf_->odc00 * D2R)) > t) {// 超出阈值, 不能启动后续流程
		gLog.Write(NULL, LOG_WARN, "PE<%.1f, %.1f>[arcsec] of <%s:%s> are beyond the threshold",
				era * D2AS, edc * D2AS, groupid_.c_str(), unitid_.c_str());
		mntnf_->reset_object();
		if (obsplan_->op_sn >= 0) {
			if (systate_.exposing) {
				gLog.Write(NULL, LOG_WARN, "exposure<%s:%s> is interrupted for position error",
						groupid_.c_str(), unitid_.c_str());
				WriteToCamera(EXPOSE_STOP, EXPMODE_ALL);
			}
			else {
				gLog.Write(NULL, LOG_WARN, "plan<%d> on <%s:%s> is interrupted for position error",
						obsplan_->op_sn, groupid_.c_str(), unitid_.c_str());
			}
			if (!systate_.exposing || obsplan_->op_sn == INT_MAX) obsplan_->reset();
		}
	}
	else if (param1 == 1) {// 指向到位
		if (obsplan_->op_sn >= 0) {
			int n;
			gLog.Write("<%s:%s> %s exposure", groupid_.c_str(), unitid_.c_str(),
					systate_.exposing ? "resume" : "start");
			const char *compacted = ascproto_->compact_expose(EXPOSE_START, n);
			if (boost::iequals(obsplan_->obstype, "mon") || boost::iequals(obsplan_->obstype, "toa")) {
				// 通知导星相机开始曝光
				WriteToCamera(EXPOSE_START, EXPMODE_GUIDE);
			}
			else WriteToCamera(EXPOSE_START, EXPMODE_ALL);
		}
	}
	else {// 导星到位
		if (systate_.lighting) {
			gLog.Write("<%s:%s> resume suspended exposure", groupid_.c_str(), unitid_.c_str());
			// 导星功能仅在观测模式为"mon"或"toa"时有效
			WriteToCamera(EXPOSE_START, EXPMODE_GUIDE);
		}
	}
}

/* OnNewPlan()触发条件:
 * - append_gwac计划直接执行
 * - ThreadWaitPlan正常结束
 * - 完成观测计划
 * - 中断观测计划
 */
void ObservationSystem::OnNewPlan(long param1, long param2) {
	/* 完成计划后检测是否有新计划等待执行 */
	mutex_lock lck(mutex_mount_); // 保证在函数执行期间, 与转台网络连接状态不发生变更
	if (waitplan_->op_sn < 0 || thrd_waitplan_.unique() || !mntnf_.unique()) return;

	// 开始执行观测计划
	gLog.Write("plan<%d> goes running on <%s:%s>",
			waitplan_->op_sn, groupid_.c_str(), unitid_.c_str());
	// 等待计划赋予执行计划
	*obsplan_ = *waitplan_;
	waitplan_->reset();
	// 指向
	bool bpoint = obsplan_->iimgtype >= IMGTYPE_FLAT; // 是否需要定位
	double ra(obsplan_->ra), dc(obsplan_->dec);
	int n;
	if (obsplan_->iimgtype == IMGTYPE_FLAT) {
		RandomZenith(ra, dc);
		obsplan_->ra = ra;
		obsplan_->dec= dc;
	}
	else if (obsplan_->iimgtype > IMGTYPE_FLAT && mntnf_->state == MOUNT_TRACKING) {// 检查转台是否已指向目标位置
		double era(obsplan_->ra - mntnf_->ra00);
		double edc(obsplan_->dec - mntnf_->dc00);
		double t(0.03); // 重新指向阈值: 1.8角分
		if (era > 180) era -= 360.0;
		else if (era < -180) era += 360.0;
		bpoint = fabs(edc) > t || fabs(era * cos(obsplan_->dec * D2R)) > t;
	}

	seqGuide_ = 0;
	lastGuide_ = ptime(not_a_date_time);
	mntnf_->set_object(ra, dc);
	if (bpoint) {
		gLog.Write("<%s:%s> points to <%.4f, %.4f>[degree]", groupid_.c_str(), unitid_.c_str(), ra, dc);
		systate_.begin_slew();
		const char *compacted = mntproto_->compact_slew(groupid_, unitid_, ra, dc, n);
		tcp_mount_->write(compacted, n);
	}
	// 通知相机: 目标信息
	WriteObject(obsplan_, "");
	if (!bpoint) {// 不需要指向, 开始曝光
		gLog.Write("<%s:%s> start exposure", groupid_.c_str(), unitid_.c_str());

		const char *compacted = ascproto_->compact_expose(EXPOSE_START, n);
		WriteToCamera(compacted, n, "");
	}
}

void ObservationSystem::OnOutLimit(long param1, long param2) {
	gLog.Write(NULL, LOG_WARN, "<%s:%s> is out of safe range", groupid_.c_str(), unitid_.c_str());
	errSafe_ = 0;
	ProcessPark();
}

void ObservationSystem::OnNewProtocol(long param1, long param2) {
	protoptr one;
	{
		mutex_lock lck(mutex_proto_);
		one = protovec_.front();
		protovec_.pop_front();
	}

	if      (iequals(one->type, "take_image"))  ProcessTakeImage  (static_pointer_cast<ascproto_take_image>(one->body));
	else if (iequals(one->type, "abort_image")) ProcessAbortImage (static_pointer_cast<ascproto_abort_image>(one->body));
	else if (iequals(one->type, "abort_slew"))  ProcessAbortSlew  ();
	else if (iequals(one->type, "append_gwac")) ProcessAppendPlan (static_pointer_cast<ascproto_append_gwac>(one->body));
	else if (iequals(one->type, "find_home"))   ProcessFindHome   ();
	else if (iequals(one->type, "focus"))       ProcessFocus      (static_pointer_cast<ascproto_focus>(one->body));
	else if (iequals(one->type, "fwhm"))        ProcessFWHM       (static_pointer_cast<ascproto_fwhm>(one->body));
	else if (iequals(one->type, "guide"))       ProcessGuide      (static_pointer_cast<ascproto_guide>(one->body));
	else if (iequals(one->type, "home_sync"))   ProcessHomeSync   (static_pointer_cast<ascproto_home_sync>(one->body));
	else if (iequals(one->type, "mcover"))      ProcessMirrorCover(static_pointer_cast<ascproto_mcover>(one->body));
	else if (iequals(one->type, "park"))        ProcessPark       ();
	else if (iequals(one->type, "slewto"))      ProcessSlewto     (static_pointer_cast<ascproto_slewto>(one->body));
	else if (iequals(one->type, "start_gwac"))  ProcessStartGwac  ();
	else if (iequals(one->type, "stop_gwac"))   ProcessStopGwac   ();
}

/* OnFlatReslew()触发条件:
 * - 平场模式
 * - 所有处于平场模式相机都进入CAMCTL_WAIT_FLAT状态
 */
void ObservationSystem::OnFlatReslew(long param1, long param2) {
	mutex_lock lck(mutex_mount_);

	if (!mntnf_.unique()) {
		gLog.Write(NULL, LOG_WARN, "off-line mount trigger <abort_image> to interrupt flat acquisition on <%s:%s>",
				groupid_.c_str(), unitid_.c_str());
		int n;
		const char *compacted = ascproto_->compact_expose(EXPOSE_STOP, n);
		WriteToCamera(compacted, n, "");
	}
	else if (obsplan_->op_sn == INT_MAX && !(systate_.slewing || systate_.guiding || systate_.parking)) {
		bool reslew = systate_.validflat;
		if (!reslew) {// 未获得有效平场
			// 检查与最后一次平场指向的时间偏差
			// 若已经在该位置尝试20分钟, 则重新指向
			ptime now = second_clock::universal_time();
			reslew = (now - systate_.lastflat).total_seconds() > 1200;
		}

		if (reslew) {// 重新指向
			double ra, dc;
			int n;

			RandomZenith(ra, dc);
			// 通知相机: 转台位置
			obsplan_->ra = ra;
			obsplan_->dec= dc;
			WriteObject(obsplan_, "");
			// 指向
			gLog.Write("flat-slew <%s:%s> to <%.4f, %.4f>[degree]", groupid_.c_str(), unitid_.c_str(), ra, dc);
			seqGuide_ = 0;
			systate_.begin_slew();
			mntnf_->set_object(ra, dc);
			const char* compacted = mntproto_->compact_slew(groupid_.c_str(), unitid_.c_str(), ra, dc, n);
			tcp_mount_->write(compacted, n);
		}
		else {// 在该位置继续尝试曝光
			WriteToCamera(EXPOSE_START, EXPMODE_ALL);
		}
	}
}

void ObservationSystem::OnWaitOver(long param1, long param2) {
	thrd_waitplan_->join();
	thrd_waitplan_.reset();

	if (obsplan_->op_sn < 0) {// 当前无计划, 直接开始执行计划
		post_message(MSG_NEW_PLAN);
	}
	else {// 需要停止当前计划
		boost::shared_ptr<ascproto_abort_image> proto = boost::make_shared<ascproto_abort_image>();
		ProcessAbortImage(proto);
	}
}

void ObservationSystem::ProcessCameraProtocol(camnfptr nf, boost::shared_ptr<ascproto_camera_info> proto) {
	int prev(nf->state), now(proto->state);
	*nf = *proto;

	if (prev != now) {// 处理相机状态变化
		bool overexposure(false); // 曝光流程结束
		if      (now >= CAMCTL_EXPOSE && prev <= CAMCTL_IDLE) systate_.enter_expose(nf->imgtype);
		else if (now <= CAMCTL_IDLE && prev >= CAMCTL_EXPOSE) overexposure = systate_.leave_expose(nf->imgtype);

		if (obsplan_->iimgtype == IMGTYPE_FLAT && now == CAMCTL_COMPLETE)
			systate_.validflat |= nf->validexp;

		if (overexposure) {
			gLog.Write("<%s:%s> exposure sequence is over", groupid_.c_str(), unitid_.c_str());
			if (obsplan_->op_sn >= 0) {
				gLog.Write("plan<%d> on <%s:%s> is over", obsplan_->op_sn, groupid_.c_str(), unitid_.c_str());
				obsplan_->reset();
			}
			post_message(MSG_NEW_PLAN); // 相机曝光结束, 检查是否有新的观测计划
		}
		else if (systate_.flatting) {
			/* 处理平场
			 * - 当所有相机状态变更为CAMCTL_WAIT_FLAT时, 重新指向天顶
			 * - 当相机离开CAMCTL_WAIT_FLAT、且并非曝光流程结束时, 计数减1
			 * - 当相机完成曝光流程, 但其他相机需要继续曝光时
			 */
			if ((now == CAMCTL_IDLE && systate_.waitflat == systate_.flatting)
					|| (now == CAMCTL_WAIT_FLAT && systate_.enter_waitflat())) {
				post_message(MSG_FLAT_RESLEW);
			}
			else if (prev == CAMCTL_WAIT_FLAT) systate_.leave_waitflat();
		}
	}
}

/*
 * take_image仅处理手动曝光, 即不是由观测计划触发的曝光
 */
void ObservationSystem::ProcessTakeImage(boost::shared_ptr<ascproto_take_image> proto) {
	string cid = proto->camera_id;
	/* 指令禁止执行:
	 * C1: 有计划正在执行
	 * C2: 相机未联网, 或已在曝光
	 * C3: 禁止平场与其它图像类型模式共存(平场转动转台破坏其它类型图像)
	 * C4: 平场需要可用转台
	 */
	bool c1 = obsplan_->op_sn >= 0;
	int  c2 = IsExposing(cid);
	bool c3 = (proto->iimgtype == IMGTYPE_FLAT && systate_.lighting && !systate_.flatting) // 平场, 已有非平场感光需求
			|| (proto->iimgtype > IMGTYPE_FLAT && systate_.flatting);	// 非平场感光, 已有平场
	bool c4 = proto->iimgtype == IMGTYPE_FLAT && !mntnf_.unique();
	if (c1 || c2 || c3 || c4) {
		format fmt("%s%s%s%s");
		string prompt;

		fmt % (c1 ? "plan in operate; " : "")
				% (c2 < 0 ? "camera is off-line; " : (c2 > 0 ? "in exposure state; " : ""))
				% (c3 ? "flat cannot co-exist with other image type; " : "")
				% (c4 ? "flat requires mount being on-line" : "");
		prompt = fmt.str();
		trim_right_if(prompt, is_punct() || is_space());
		gLog.Write(NULL, LOG_WARN, "take_image<%s:%s> is rejected for [%s]",
				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
	}
	else {
		gLog.Write("take_image <name=%s, type=%s, expdur=%.3f, frmcnt=%d> on <%s:%s:%s>",
				proto->obj_id.c_str(), proto->imgtype.c_str(), proto->expdur, proto->frmcnt,
				groupid_.c_str(), unitid_.c_str(), cid.empty() ? "all" : cid.c_str());

		boost::shared_ptr<ascproto_object_info> nf = boost::make_shared<ascproto_object_info>();
		int n;

		*nf = *proto;
		obsplan_->iimgtype = proto->iimgtype;
		systate_.validflat = false;
		systate_.lastflat  = second_clock::universal_time();
		if (proto->iimgtype >= IMGTYPE_FLAT && mntnf_.unique()) {// 天光类型: 以转台当前指向为目标位置
			nf->ra = mntnf_->ra00;
			nf->dec= mntnf_->dc00;
			nf->epoch = 2000.0;
			mntnf_->actual2object();
		}
		if (proto->iimgtype == IMGTYPE_FLAT) {
			ptime start, stop;
			int am_pm = ValidFlat(start, stop);
			if (am_pm == 1) nf->expdur = 15.0;
			else if (am_pm == 2) nf->expdur = 2.0;
			else nf->expdur = 8.0;
		}
		if (!systate_.exposing) {
			obsplan_->op_sn = INT_MAX;
			obsplan_->priority = INT_MAX;
		}
		WriteObject(nf, cid);
		const char *compacted = ascproto_->compact_expose(EXPOSE_START, n);
		WriteToCamera(compacted, n, cid);
	}
}

void ObservationSystem::ProcessAbortImage(boost::shared_ptr<ascproto_abort_image> proto) {
	string cid = proto.unique() ? proto->camera_id : "";

	gLog.Write("abort_image<%s:%s:%s>", groupid_.c_str(), unitid_.c_str(),
			cid.empty() ? "all" : cid.c_str());
	if (mntnf_.unique()) mntnf_->reset_object();

	int n;
	const char *compacted = ascproto_->compact_expose(EXPOSE_STOP, n);
	WriteToCamera(compacted, n, cid);
}

void ObservationSystem::ProcessAbortSlew(int sn) {
	mutex_lock lck(mutex_mount_);
	if (!mntnf_.unique()) {
		gLog.Write(NULL, LOG_WARN, "abort_slew<%s:%s> is rejected for mount is off-line",
				groupid_.c_str(), unitid_.c_str());
	}
	else {
		if (sn < 0) gLog.Write("abort_slew<%s:%s>", groupid_.c_str(), unitid_.c_str());
		else gLog.Write("plan<%d> on <%s:%s> is interrupted", sn, groupid_.c_str(), unitid_.c_str());

		// 中止指向
		mntnf_->reset_object();
		int n;
		const char* compacted = mntproto_->compact_abort_slew(groupid_, unitid_, n);
		tcp_mount_->write(compacted, n);
		// 中止曝光
		compacted = ascproto_->compact_expose(EXPOSE_STOP, n);
		WriteToCamera(compacted, n, "");
	}
}

void ObservationSystem::ProcessAppendPlan(boost::shared_ptr<ascproto_append_gwac> proto) {
	/* 观测计划响应条件:
	 * C1: 转台在线
	 * C2: 有相机在线
	 * C3: 优先级更高
	 * C4: 非重复计划
	 */
	ptime btm(not_a_date_time), etm(not_a_date_time), now;
	ptime::time_duration_type td;
	bool success(true);
	bool c1 = !mntnf_.unique();
	bool c2 = !tcp_camera_.size();
	bool c3 = proto->priority < obsplan_->priority;
	bool c4 = proto->op_sn < 0;
	bool c5 = (proto->op_sn == obsplan_->op_sn) || (proto->op_sn == waitplan_->op_sn);

	if (c1 || c2 || c3 || c4 || c5) {// 不满足计划响应条件
		format fmt("%s%s%s%s%s");
		string prompt;
		fmt % (c1 ? "mount is off-line; " : "")
				% (c2 ? "none camera is on-line; " : "")
				% (c3 ? "priority is less than that in operation; " : "")
				% (c4 ? "illegal op_sn; " : "")
				% (c5 ? "repeat plan number" : "");
		prompt = fmt.str();
		trim_right_if(prompt, is_punct() || is_space());
		gLog.Write(NULL, LOG_WARN, "plan<%d> on <%s:%s> is rejected for [%s]",
				proto->op_sn, groupid_.c_str(), unitid_.c_str(), prompt.c_str());
		success = false;
	}
	else if (proto->iimgtype >= IMGTYPE_OBJECT) {// 其它匹配条件
		now = second_clock::universal_time();
		try { btm = from_iso_extended_string(proto->begin_time); }
		catch(...) {}
		try { etm = from_iso_extended_string(proto->end_time); }
		catch(...) {}

		if (!etm.is_special()) {
			double dly = proto->delay < 1.0 ? 3.0 : proto->delay;
			double dt = proto->frmcnt * (proto->expdur + dly) * 0.3; // 观测所需时间, 量纲: 秒
			td = etm - now;
			c1 = double(td.total_seconds()) < dt; // 观测时间不足
		}
		if (c1) {// 时间有效性
			gLog.Write(NULL, LOG_WARN, "plan<%d> on <%s:%s> is rejected for end-time <%s> is extremely early",
					proto->op_sn, groupid_.c_str(), unitid_.c_str(), proto->end_time.c_str());
			success = false;
		}
		else {// 位置有效性
			bool safe = SafePosition(proto->ra, proto->dec, btm.is_special() ? NULL : &btm);
			if (!safe) {
				gLog.Write(NULL, LOG_WARN, "plan<%d> on <%s:%s> is rejected for lower than 20 degrees at begin-time",
						proto->op_sn, groupid_.c_str(), unitid_.c_str());
				success = false;
			}
		}
	}
	else if (proto->iimgtype == IMGTYPE_FLAT) {
		ptime start, stop;
		int am_pm = ValidFlat(start, stop);
		if (!am_pm) success = false;
		else {
			if (am_pm == 1) proto->expdur = 15.0;
			else if (am_pm == 2) proto->expdur = 2.0;
			else proto->expdur = 8.0;

			if (!start.is_special()) {

			}
			if (!stop.is_special()) {

			}
		}
	}

	if (success) {
		if (thrd_waitplan_.unique()) thrd_waitplan_->interrupt();
		*waitplan_ = *proto;

		if (!btm.is_special() && (btm - now).total_seconds() > 250)
			thrd_waitplan_.reset(new boost::thread(boost::bind(&ObservationSystem::ThreadWaitPlan, this, td.total_seconds() - 220)));
		else {
			if (!systate_.exposing) post_message(MSG_NEW_PLAN);
			else {// 中止曝光, 并通过中止曝光重置观测计划
				mntnf_->reset_object();
				int n;
				const char *compacted = ascproto_->compact_expose(EXPOSE_STOP, n);
				WriteToCamera(compacted, n, "");
			}
		}
	}
}

void ObservationSystem::ProcessFindHome() {
	mutex_lock lck(mutex_mount_);
	/* 搜索零点启动条件:
	 * - 转台在线, 且处于静止或跟踪状态
	 */
	bool c1 = !mntnf_.unique();
	bool c2 = systate_.lighting;
	if (c1 || c2) {
		gLog.Write(NULL, LOG_WARN, "find_home<%s:%s> is rejected for %s",
				groupid_.c_str(), unitid_.c_str(), c1 ? "mount is off-line" : "in light required exposure");
	}
	else {
		gLog.Write("try to find home<%s:%s>", groupid_.c_str(), unitid_.c_str());
		int n;
		const char* compacted = mntproto_->compact_find_home(groupid_, unitid_, 1, 1, n);
		tcp_mount_->write(compacted, n);
	}
}

void ObservationSystem::ProcessFocus(boost::shared_ptr<ascproto_focus> proto) {
	mutex_lock lck(mutex_mountannex_);
	bool c1 = !tcp_mountannex_.use_count();
	bool c2 = proto->camera_id.empty();

	if (c1 || c2) {
		format fmt("%s%s");
		string prompt;
		fmt % (c1 ? "mount-annex is off-line; " : "") % (c2 ? "cam_id is empty" : "");
		prompt = fmt.str();
		trim_right_if(prompt, is_punct() || is_space());
		gLog.Write(NULL, LOG_WARN, "focus<%s:%s> is rejected for [%s]",
				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
	}
	else {
		string cid = proto->camera_id;
		gLog.Write("Focus<%s:%s:%s> moves to %d", groupid_.c_str(), unitid_.c_str(), cid.c_str(), proto->value);

		int n;
		const char* compacted = mntproto_->compact_focus(groupid_, unitid_, cid, proto->value, n);
		tcp_mountannex_->write(compacted, n);
	}
}

void ObservationSystem::ProcessFWHM(boost::shared_ptr<ascproto_fwhm> proto) {
	mutex_lock lck(mutex_mountannex_);
	bool c1 = !tcp_mountannex_.use_count();
	bool c2 = proto->camera_id.empty();

//	if (c1 || c2) {
//		format fmt("%s%s");
//		string prompt;
//		fmt % (c1 ? "mount-annex is off-line; " : "") % (c2 ? "cam_id is empty" : "");
//		prompt = fmt.str();
//		trim_right_if(prompt, is_punct() || is_space());
//		gLog.Write(NULL, LOG_WARN, "fwhm<%s:%s> is rejected for [%s]",
//				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
//	}
//	else {
	if (!c2) {
		mutex_lock lck(mutex_camera_);
		camvec::iterator it, itend = tcp_camera_.end();
		string cid = proto->camera_id;

		if (!c1) {
			int n;
			const char* compacted = mntproto_->compact_fwhm(groupid_, unitid_, cid, proto->value, n);
			tcp_mountannex_->write(compacted, n);
		}

		for (it = tcp_camera_.begin(); it != itend && !iequals(cid, (*it)->id); ++it);
		if (it != itend && fabs(proto->value - (*it)->fwhm) > 0.1) {
//			gLog.Write("FWHM<%s:%s:%s> is %.2f",
//				 groupid_.c_str(), unitid_.c_str(), cid.c_str(), proto->value);
			(*it)->fwhm = proto->value;
		}
	}
}

/* ProcessGuide()触发条件:
 * - 外界手动指令
 * - 数据处理软件指令
 */
void ObservationSystem::ProcessGuide(boost::shared_ptr<ascproto_guide> proto) {
	mutex_lock lck(mutex_mount_);
	if (!mntnf_.unique()) {
		gLog.Write(NULL, LOG_WARN, "guide<%s:%s> is rejected for mount is off-line",
				groupid_.c_str(), unitid_.c_str());
	}
	else if ((mntnf_->state >= MOUNT_ERROR && mntnf_->state != MOUNT_TRACKING)
		|| (mntnf_->state < MOUNT_ERROR && (systate_.slewing || systate_.parking || systate_.guiding))) {
		gLog.Write(NULL, LOG_WARN, "guide<%s:%s> is rejected for not being in tracking mode",
				groupid_.c_str(), unitid_.c_str());
	}
	else if (boost::iequals(obsplan_->obstype, "mon") || boost::iequals(obsplan_->obstype, "toa")) {
		/*
		 * - 导星坐标系为J2000. GWAC, May 01, 2017
		 * - 导星方向: May 17, 2017
		 *   赤经 -- 东: +; 西: -
		 *   赤纬 -- 北: +; 南: -
		 */
		double tsame(AS2D);	// 目标坐标与转台目标一致阈值: 1角秒
		double tproto(2.777);	// 协议阈值: 9997.2角秒
		double tguide(0.08);	// 导星阈值: 288角秒=4.8角分
		double ra(proto->ra), dc(proto->dec);	// 天文坐标
		double ora(proto->objra), odc(proto->objdec);	// 目标坐标
		double era(0.0), edc(0.0);	// 导星量
		bool bsky(true);	// 是否有天文坐标. 若有天文坐标, 可以执行同步零点
		bool legal(true);	// 导星量合法标记
		double dis(0.0);	// 大圆距离, 量纲: 角分

		/* 计算导星量并评估其合法性 */
		if (valid_ra(ora) && valid_dc(odc)) {// 目标位置有效
			era = fabs(ora - mntnf_->ora00);
			edc = fabs(odc - mntnf_->odc00);
			if (era > 180.0) era = 360.0 - era;
			if (true == (legal = edc <= tsame && (era * cos(odc * D2R)) <= tsame)) {// 目标坐标相同, 可以导星或同步零点
				edc = odc - dc; // (era, edc)调整为导星量
				if ((era = ra - ora) > 180.0) era -= 360.0;
				else if (era < -180.0) era += 360.0;
				dis = ats_->SphereAngle(ra * D2R, dc * D2R, ora * D2R, odc * D2R) * R2D;
			}
		}
		else {// 导星协议: 数据可容纳范围9999角秒, 一次导星量不超过2.5度=9000角秒
			bsky = false;
			era = ra;
			edc = dc;
		}

		/* 执行: 导星或同步零点 */
		format fmt1("offset: <%.1f, %.1f>[arcsec]");
		string prompt;

		fmt1 % (era * 3600.0) % (edc * 3600.0);
		prompt = fmt1.str();
		if (bsky) {
			format fmt2("object: <%.4f, %.4f>[degree]");
			format fmt3("sky: <%.4f, %.4f>[degree]");
			fmt2 % ora % odc;
			fmt3 % ra % dc;
			prompt += ", " + fmt2.str() + ", " + fmt3.str();
		}

		if (legal && dis > tguide) {
			 gLog.Write("%s guide<%s:%s>. %s", legal ? "legal" : "illegal",
				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
		}
		/* 判定是否可以开始有效曝光: 通过天文定位, 判定转台已到达目标位置 */
		if (legal
			&& dis <= tguide
			&& systate_.exposing < tcp_camera_.size()) {
			WriteToCamera(EXPOSE_START, EXPMODE_JOINT);
		}

		if (!legal || dis <= tguide) seqGuide_ = 0;
		else if (++seqGuide_ >= 2) {
			bool bguide(true);
			ptime now = second_clock::universal_time();

//			if (!bsky) {// 检查导星量是否越界
				if (era > tproto) era = tproto;
				else if (era < -tproto) era = -tproto;
				if (edc > tproto) edc = tproto;
				else if (edc < -tproto) edc = -tproto;
//			}
			if (lastGuide_.is_special() // 未执行过导星
					|| (bguide = (now - lastGuide_).total_seconds() >= 120)) {// 120秒内未执行过导星
				lastGuide_ = now;
			}

			if (bguide) {// 执行导星
				seqGuide_ = 0;
//				if (mntnf_->add_offset(era, edc) && bsky) {// 同步零点
/*
					gLog.Write("guide trigger home_sync<%s:%s>: <%.4f, %.4f>[degree]", groupid_.c_str(), unitid_.c_str(), ra, dc);

					boost::shared_ptr<ascproto_home_sync> proto = boost::make_shared<ascproto_home_sync>();
					proto->group_id = groupid_;
					proto->unit_id  = unitid_;
					proto->ra  = ra;
					proto->dec = dc;
					proto->epoch = 2000.0;	// 采用J2000
					notify_home_sync(static_pointer_cast<ascproto_base>(proto));
*/
//				}
//				else {
					mntnf_->add_offset(era, edc);
					int n;
					era *= 3600.0;
					edc *= 3600.0;
					gLog.Write("guide<%s:%s>: <%.1f, %.1f>[arcsec]%s", groupid_.c_str(), unitid_.c_str(), era, edc,
							systate_.lighting ? ". pause exposure furthmore as light required" : "");
					if (systate_.lighting) {// 暂停曝光
						WriteToCamera(EXPOSE_PAUSE, EXPMODE_ALL);
					}
					systate_.begin_guide();
					const char *compacted = mntproto_->compact_guide(groupid_, unitid_, int(era), int(edc), n);
					tcp_mount_->write(compacted, n);
//				}
			}
		}
	}
}

void ObservationSystem::ProcessHomeSync(boost::shared_ptr<ascproto_home_sync> proto) {
	mutex_lock lck(mutex_mount_);

	if (!mntnf_.unique()) {
		gLog.Write(NULL, LOG_WARN, "home_sync<%s:%s> is rejected for mount is off-line",
				groupid_.c_str(), unitid_.c_str());
	}
	else if ((mntnf_->state >= MOUNT_ERROR && mntnf_->state != MOUNT_TRACKING)
			|| (mntnf_->state < MOUNT_ERROR && (systate_.slewing || systate_.guiding || systate_.parking))) {
		gLog.Write(NULL, LOG_WARN, "home_sync<%s:%s> is rejected for not being in tracking mode",
				groupid_.c_str(), unitid_.c_str());
	}
	else {
		gLog.Write("home_sync<%s:%s>. mount: <%.4f, %.4f> <== sky: <%.4f, %.4f>",
				groupid_.c_str(), unitid_.c_str(), mntnf_->ra00, mntnf_->dc00, proto->ra, proto->dec);
		mntnf_->reset_offset(); // 同步零点时清除累计导星量
		int n;
		const char *compacted = mntproto_->compact_home_sync(groupid_, unitid_, proto->ra, proto->dec, n);
		tcp_mount_->write(compacted, n);
	}
}

void ObservationSystem::ProcessMirrorCover(boost::shared_ptr<ascproto_mcover> proto) {
	mutex_lock lck(mutex_mountannex_);
	int command = proto->command;
	bool c1 = !tcp_mountannex_.use_count();
	bool c2 = command < 0 || command > 1;
	bool c3 = command == 0 && proto->camera_id.empty() && systate_.lighting;
	if (c1 || c2 || c3) {
		format fmt("%s%s%s");
		string prompt;
		fmt % (c1 ? "mount-annex is off-line; " : "")
				% (c2 ? "illegal command; " : "")
				% (c3 ? "in light required exposure; " : "");
		prompt = fmt.str();
		trim_right_if(prompt, is_punct() || is_space());
		gLog.Write(NULL, LOG_WARN, "mcover<%s:%s> is rejected for [%s]",
				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
	}
	else {
		int n;
		string cid = proto->camera_id;
		gLog.Write("%s mirror-cover<%s:%s:%s>", command ? "open" : "close", groupid_.c_str(), unitid_.c_str(),
				cid.empty() ? "all" : cid.c_str());
		if (cid.empty()) {// 打开/关闭所有在线相机镜盖
			mutex_lock lck(mutex_camera_);
			camvec::iterator it;
			for (it = tcp_camera_.begin(); it != tcp_camera_.end(); ++it) {
				const char *compacted = mntproto_->compact_mirror_cover(groupid_, unitid_, (*it)->id, command, n);
				tcp_mountannex_->write(compacted, n);
			}
		}
		else {
			const char *compacted = mntproto_->compact_mirror_cover(groupid_, unitid_, cid, command, n);
			tcp_mountannex_->write(compacted, n);
		}
	}
}

/* ProcessPark()触发条件:
 * - 外界手动指令
 * - 转台指向超出保护区
 * 复位时并发行为:
 * - 转台复位至预定安全位置(一般为天顶)
 * - 中止相机曝光
 * - 重置观测计划
 */
void ObservationSystem::ProcessPark() {
	mutex_lock lck(mutex_mount_);
	if (!mntnf_.unique()) {
		gLog.Write(NULL, LOG_WARN, "park<%s:%s> is rejected for %s",
				groupid_.c_str(), unitid_.c_str(), "mount is off-line");
	}
	else if (!systate_.parking) {
		gLog.Write("parking <%s:%s>", groupid_.c_str(), unitid_.c_str());
		// 复位转台
		int n;
		const char *compacted = mntproto_->compact_park(groupid_, unitid_, n);
		systate_.begin_park();
		mntnf_->reset_object();
		tcp_mount_->write(compacted, n);
		if (systate_.exposing) {
			/* 中止曝光
			 * - 不检测相机实际工作状态, 直接显式中断.
			 *   避免由于延时等原因, 未及时收到相机的工作状态
			 */
			compacted = ascproto_->compact_expose(EXPOSE_STOP, n);
			WriteToCamera(compacted, n, "");
		}
	}
}

/* ProecessSlewto()触发条件:
 * - 外界手动指令
 * 指令响应条件:
 * - 转台可用
 * - 未执行观测计划
 */
void ObservationSystem::ProcessSlewto(boost::shared_ptr<ascproto_slewto> proto) {
	mutex_lock lck(mutex_mount_);
	bool c1 = !mntnf_.unique();		// 转台未联网
	bool c2 = obsplan_->op_sn >= 0;	// 已开始执行观测计划
	bool c3 = waitplan_->op_sn >= 0 && !thrd_waitplan_.unique();	// 准备执行观测计划
	if (c1 || c2 || c3) {
		format fmt("%s%s%s");
		string prompt;

		fmt % (c1 ? "mount is off-line; " : "")
				% (c2 ? "plan in operation; " : "")
				% (c3 ? "plan waiting to be executed" : "");
		prompt = fmt.str();
		trim_right_if(prompt, is_punct() || is_space());
		gLog.Write(NULL, LOG_WARN, "slewto<%s:%s> is rejected for [%s]",
				groupid_.c_str(), unitid_.c_str(), prompt.c_str());
	}
	else {// 检查位置安全性
		double ra(proto->ra), dc(proto->dec);
		bool safe = SafePosition(ra, dc, NULL);

		if (!safe) {
			gLog.Write("slewto<%s:%s> is rejected for lower than 20 degrees", groupid_.c_str(), unitid_.c_str());
		}
		else {// 通知转台指向
			gLog.Write("<%s:%s> points to <%.4f, %.4f>[degree]", groupid_.c_str(), unitid_.c_str(), ra, dc);
			int n;
			const char* compacted = mntproto_->compact_slew(groupid_.c_str(), unitid_.c_str(), ra, dc, n);
			seqGuide_ = 0;
			lastGuide_ = ptime(not_a_date_time);
			mntnf_->set_object(ra, dc);
			systate_.begin_slew();
			tcp_mount_->write(compacted, n);
		}
	}
}

void ObservationSystem::ProcessStartGwac() {
	if (!systate_.running) {
		gLog.Write("Observation System <%s:%s> enter AUTO mode", groupid_.c_str(), unitid_.c_str());
		systate_.running = true;

		// ... 执行观测前自检流程
	}
}

void ObservationSystem::ProcessStopGwac() {
	if (systate_.running) {
		gLog.Write(NULL, LOG_WARN, "Observation System <%s:%s> leave AUTO mode", groupid_.c_str(), unitid_.c_str());
		systate_.running = false;

		// ... 执行观测后恢复流程
	}
}

///////////////////////////////////////////////////////////////////////////////
void ObservationSystem::ThreadWaitPlan(int seconds) {// 在观测开始执行时触发线程
	gLog.Write("plan<%d> on <%s:%s> will wait for <%d> seconds",
			waitplan_->op_sn, groupid_.c_str(), unitid_.c_str(), seconds);
	boost::this_thread::sleep_for(boost::chrono::seconds(seconds));
	post_message(MSG_WAIT_OVER);
}
