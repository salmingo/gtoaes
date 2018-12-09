/*
 * @file ObservationSystem.cpp 定义文件, 建立观测系统基类
 * @version 0.2
 * @date 2017-10-06
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <stdlib.h>
#include "ObservationSystem.h"
#include "GLog.h"
#include "ADefine.h"

using namespace boost;
using namespace boost::posix_time;
using AstroUtil::ATimeSpace;

ObservationSystemCamera::ObservationSystemCamera(const string& id) {
	cid     = id;
	enabled = true;
	fwhm    = 0.0;
}

ObservationSystem::ObservationSystem(const string& gid, const string& uid) {
	gid_      = gid;
	uid_      = uid;
	timezone_ = 8;
	minEle_   = 20.0 * D2R;
	tslew_    = AS2D * 10;
	tguide_   = AS2D;
	odtype_   = OD_DAY;
	tmLast_   = second_clock::universal_time();
	data_     = boost::make_shared<ObservationSystemData>(gid, uid);
	ats_      = boost::make_shared<ATimeSpace>();
	ascproto_ = make_ascproto();
	bufrcv_.reset(new char[TCP_PACK_SIZE]);
}

ObservationSystem::~ObservationSystem() {
	StopService();
}

bool ObservationSystem::StartService() {
	if (data_->running) return true;

	string name = "msgque_" + gid_ + uid_;
	register_message();
	if (!Start(name.c_str())) return false;
	_gLog.Write("Observation System [%s:%s] goes running", gid_.c_str(), uid_.c_str());
	thrd_time_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_time, this)));
	data_->running = true;

	return data_->running;
}

void ObservationSystem::StopService() {
	if (data_->running) {
		interrupt_thread(thrd_client_);
		interrupt_thread(thrd_idle_);
		interrupt_thread(thrd_time_);
		data_->running = false;
		_gLog.Write("Observation System [%s:%s] stop running", gid_.c_str(), uid_.c_str());
	}
}

int ObservationSystem::IsMatched(const string& gid, const string& uid) {
	if (gid == gid_ && uid == uid_) return 1;
	if (uid.empty() && (gid == gid_ || gid.empty())) return 0;
	return -1;
}

void ObservationSystem::SetGeosite(const string& name, const double lgt, const double lat,
		const double alt, const int timezone) {
	_gLog.Write("Observation System [%s:%s] located at [%s, %.4f, %.4f, %.1f]",
			gid_.c_str(), uid_.c_str(), name.c_str(), lgt, lat, alt);
	ats_->SetSite(lgt, lat, alt, timezone);
	timezone_ = timezone;
}

void ObservationSystem::SetElevationLimit(double value) {
	_gLog.Write("Elevation limit of Observation System [%s:%s] is %.1f[degree]",
			gid_.c_str(), uid_.c_str(), value);
	minEle_ = value * D2R;
}

void ObservationSystem::GetID(string& gid, string& uid) {
	gid = gid_;
	uid = uid_;
}

OBSS_STATUS ObservationSystem::GetState() {
	return data_->state;
}

NFTelePtr ObservationSystem::GetTelescope() {
	return nftele_;
}

ObssCamVec& ObservationSystem::GetCameras() {
	return cameras_;
}

const char* ObservationSystem::GetPlan(int& sn) {
	if (plan_now_.use_count()) {
		sn = plan_now_->plan->plan_sn;
		return plan_now_->op_time.c_str();
	}

	sn = -1;
	return NULL;
}

bool ObservationSystem::HasAnyConnection() {
	// 存在任一网络连接
	if (tcpc_telescope_.use_count() || cameras_.size()) return true;
	// 最后一条网络连接断开时间不超过60秒
	time_duration dt = second_clock::universal_time() - tmLast_;
	return (dt.total_seconds() < 60);
}

bool ObservationSystem::CoupleClient(TcpCPtr client) {
	mutex_lock lck(mtx_client_);
	// 检查是否已关联: 避免重复关联
	TcpCVec::iterator it;
	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && (*it) != client; ++it);
	if (it == tcpc_client_.end()) {
		tcpc_client_.push_back(client);
		if (!thrd_client_.unique()) {// 启动客户端线程
			thrd_client_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_client, this)));
		}
		return true;
	}
	return false;
}

bool ObservationSystem::DecoupleClient(TcpCPtr client) {
	mutex_lock lck(mtx_client_);
	TcpCVec::iterator it;
	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && (*it) != client; ++it);
	if (it != tcpc_client_.end()) {
		(*it).reset();
		tcpc_client_.erase(it);
		if (!tcpc_client_.size()) interrupt_thread(thrd_client_); // 停用客户端线程
		return true;
	}
	return false;
}

bool ObservationSystem::CoupleTelescope(TcpCPtr client) {
	if (!tcpc_telescope_.use_count()) {
		_gLog.Write("telescope [%s:%s] is on-line", gid_.c_str(), uid_.c_str());
		tcpc_telescope_ = client;
		nftele_ =  boost::make_shared<NFTele>();

		return true;
	}
	return false;
}

/*
 * DecoupleTelescope()适用于GWAC系统
 */
void ObservationSystem::DecoupleTelescope(TcpCPtr client) {
	if (tcpc_telescope_ == client) on_close_telescope();
}

bool ObservationSystem::CoupleCamera(TcpCPtr client, const string& cid) {
	ObssCamPtr camptr = find_camera(cid);
	if (!camptr.use_count()) {
		_gLog.Write("camera [%s:%s:%s] is on-line", gid_.c_str(), uid_.c_str(), cid.c_str());
		ObssCamPtr one = boost::make_shared<ObssCamera>(cid);
		const TCPClient::CBSlot& slot = boost::bind(&ObservationSystem::receive_camera, this, _1, _2);
		client->RegisterRead(slot);
		one->tcptr = client;
		one->info  = boost::make_shared<ascii_proto_camera>();
		// 相机资源入库, 并检查系统工作状态
		mutex_lock lck(mtx_camera_);
		cameras_.push_back(one);
		switch_state();
		return true;
	}

	return false;
}

void ObservationSystem::RegisterPlanFinished(const PlanFinishedSlot& slot) {
	cb_plan_finished_.connect(slot);
}

void ObservationSystem::RegisterAcquireNewPlan(const AcquireNewPlanSlot& slot) {
	cb_acqnewplan_.connect(slot);
}

//////////////////////////////////////////////////////////////////////////////
/* 响应GeneralControl传递消息 */
/*
 * void NotifyProtocol() 处理由GC转发的、适用于该观测系统的指令
 * - 缓存指令, 并触发消息
 * - NotifyProtocol在GC的消息队列中调用
 */
void ObservationSystem::NotifyProtocol(apbase body) {
	mutex_lock lck(mtx_queap_);
	que_ap_.push_back(body);

	PostMessage(MSG_NEW_PROTOCOL);
}

void ObservationSystem::NotifyPlan(ObsPlanPtr plan) {
	/* 修改等待区中计划, 退还给计划队列 */
	change_planstate(plan_wait_, OBSPLAN_INT);
	/* 新的计划进入等待区 */
	plan_wait_ = plan; // 计划先进入等待区
	change_planstate(plan_wait_, OBSPLAN_WAIT); // 计划状态变更为等待
	PostMessage(MSG_NEW_PLAN);
}

/*
 * 计算观测计划的相对(当前计划/等待计划)优先级
 * - 相对优先级由继承类计算
 * - 检查系统状态
 * - 检查相对优先级
 * - 检查时间有效性
 * - 检查图像类型与时间匹配性和位置安全性
 */
int ObservationSystem::PlanRelativePriority(apappplan plan, ptime& now) {
	// 检查系统可用性
	if (data_->state != OBSS_RUN) return -1;
	// 计算并评估相对优先级
	int r = relative_priority(plan->priority);
	if (r <= 0) return -2;
	// 检查时间有效性
	if ((from_iso_extended_string(plan->begin_time) - now).total_seconds() > 120) return -3;
	// 检查计划可执行性
	string abbr;
	IMAGE_TYPE imgtyp = check_imgtype(plan->imgtype, abbr);
	if (   (imgtyp >= IMGTYPE_FLAT   && !tcpc_telescope_.use_count())	// 望远镜不在线
		|| (imgtyp == IMGTYPE_FLAT   && odtype_ != OD_FLAT)				// 非天光平场时间
		|| (imgtyp == IMGTYPE_OBJECT && odtype_ != OD_NIGHT)			// 非夜间观测时间
		|| (imgtyp >= IMGTYPE_OBJECT && !safe_position(plan->ra, plan->dec))	// 非安全位置
		)
		return -4;

	return r;
}

//////////////////////////////////////////////////////////////////////////////
ObssCamPtr ObservationSystem::find_camera(TCPClient *ptr) {
	mutex_lock lck(mtx_camera_);
	ObssCamVec::iterator it;
	for (it = cameras_.begin(); it != cameras_.end() && (**it) != ptr; ++it);
	return (*it);
}

ObssCamPtr ObservationSystem::find_camera(const string &cid) {
	mutex_lock lck(mtx_camera_);
	ObssCamVec::iterator it;
	for (it = cameras_.begin(); it != cameras_.end() && (**it) != cid; ++it);
	return (*it);
}

/*
 * 为平场生成天顶附近随机位置
 * - 天顶距阈值: (5, 10]度. 阈值>5度: 规避地平式天顶盲区
 * - 方位角阈值: 上午[180, 360), 下午[0, 180)
 */
void ObservationSystem::flat_position(double &ra, double &dec, double &epoch) {
	mutex_lock lck(mtx_ats_);
	ptime now = second_clock::universal_time();
	double fd = now.time_of_day().total_seconds() / DAYSEC;
	double azi0 = (fd * 24 + timezone_) < 12.0 ? 180.0 : 0.0;
	double azi, alt;
	// 生成天顶坐标
	ats_->SetMJD(now.date().modjulian_day() + fd);
	azi = drand48() * 180.0 + azi0;
	alt = 80 + drand48() * 5.0;
	ats_->Horizon2Eq(azi * D2R, alt * D2R, ra, dec);
	ra   = ats_->LocalMeanSiderealTime() - ra;
	// 转换为J2000系
	ats_->EqReTransfer(ra, dec, ra, dec);
	ra   *= R2D;
	dec  *= R2D;
	epoch = 2000.0;
}

bool ObservationSystem::safe_position(double ra, double dec) {
	mutex_lock lck(mtx_ats_);
	ptime now = second_clock::universal_time();
	double azi, alt;

	ats_->SetMJD(now.date().modjulian_day() + now.time_of_day().total_seconds() / DAYSEC);
	ats_->Eq2Horizon(ats_->LocalMeanSiderealTime() - ra * D2R, dec * D2R, azi, alt);
	return (alt > minEle_);
}

void ObservationSystem::civil_twilight(double mjd, double& dawn, double& dusk) {
	mutex_lock lck(mtx_ats_);
	ats_->SetMJD(mjd);
	ats_->TwilightTime(dawn, dusk, 1);
}

/*
 * 中止观测计划
 * - 中止曝光过程: 包括曝光前等待
 * - 设置计划工作状态
 * state选项: OBSPLAN_INT, OBSPLAN_ABANDON, OBSPLAN_DELETE
 */
bool ObservationSystem::interrupt_plan(OBSPLAN_STATUS state) {
	bool immediate = !data_->exposing;

	if (change_planstate(plan_now_, state)) {
		command_expose("", EXPOSE_STOP);
		if (immediate) {
			// plan_sn == INT_MAX为手动曝光模式
			if (!((*plan_now_) == INT_MAX)) cb_plan_finished_(plan_now_);
			plan_now_.reset();
		}
	}

	return immediate;
}

bool ObservationSystem::change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS state) {
	if (plan.use_count() && plan->state != state) {
		// 更改计划状态
		if (change_planstate(plan, plan->state, state)) state = plan->state;
		else plan->state = state;
		_gLog.Write("plan[%d] on [%s:%s] was %s", plan->plan->plan_sn,
				gid_.c_str(), uid_.c_str(), OBSPLAN_STATUS_STR[plan->state]);
		if (plan->plan->plan_sn != INT_MAX) {// 处理变更后状态
			if      (state == OBSPLAN_RUN)  plan->CoupleOBSS(gid_, uid_);
			else if (state != OBSPLAN_WAIT) cb_plan_finished_(plan);
		}

		return true;
	}
	return false;
}

/*
 * void write_to_camera() 发送格式化字符串给指定相机或所有相机
 */
void ObservationSystem::write_to_camera(const char *s, const int n, const char *cid) {
	mutex_lock lck(mtx_camera_);
	bool empty = !cid || iequals(cid, "");
	for (ObssCamVec::iterator it = cameras_.begin(); it != cameras_.end(); ++it) {
		if (empty || iequals((*it)->cid, cid)) {
			if ((*it)->enabled) (*it)->tcptr->Write(s, n);
			if (!empty) break;
		}
	}
}

void ObservationSystem::command_expose(const string &cid, EXPOSE_COMMAND cmd) {
	int n;
	const char *s = ascproto_->CompactExpose(cmd, n);
	write_to_camera(s, n, cid.c_str());
}

void ObservationSystem::switch_state() {
	OBSS_STATUS state;

	if (cameras_.size() && data_->enabled) state = data_->automode ? OBSS_RUN : OBSS_STOP;
	else state = OBSS_ERROR;
	if (data_->state != state) {
		data_->state = state;

		if (state == OBSS_RUN)
			thrd_idle_.reset(new thread(boost::bind(&ObservationSystem::thread_idle, this)));
		else {
			interrupt_thread(thrd_idle_);
			interrupt_plan(OBSPLAN_INT);
		}
	}
}

bool ObservationSystem::slew_arrived() {
	double era  = fabs((nftele_->ra - (nftele_->ora + nftele_->dra)) * cos(nftele_->dec * D2R));
	double edec = fabs(nftele_->dec - (nftele_->odec + nftele_->ddec));
	if (era > 180.0) era = 360.0 - era;

	if (era > tslew_ || edec > tslew_) {
		_gLog.Write(LOG_WARN, NULL, "pointing error [%.4f, %.4f] of [%s:%s] exceeds limit",
				era, edec, gid_.c_str(), uid_.c_str());
		interrupt_plan(OBSPLAN_INT);
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
/*
 * 线程:
 * void thread_idle():   空闲时执行操作
 * void thread_client(): 定时向客户端发送系统工作状态
 * void thread_time():   计算并监测当前是否天光平场时间
 */
void ObservationSystem::thread_idle() {
	boost::chrono::minutes period(1); // 周期1分钟

	while(1) {
		boost::this_thread::sleep_for(period);

		if (!plan_now_.use_count()) cb_acqnewplan_(gid_, uid_);
	}
}

void ObservationSystem::thread_client() {
	boost::chrono::seconds period(10); // 周期10秒

	while(1) {
		boost::this_thread::sleep_for(period);

		const char *s1, *s2, *s3 = NULL;
		int n1, n2, n3;
		/* 构建观测系统信息 */
		apobss obss = boost::make_shared<ascii_proto_obss>();
		obss->set_id(gid_, uid_);
		obss->set_timetag();
		obss->state = data_->state;
		if (plan_now_.use_count()) {
			obss->op_sn = plan_now_->plan->plan_sn;
			obss->op_time = plan_now_->op_time;
		}
		obss->mount = nftele_->state;
		for (ObssCamVec::iterator it = cameras_.begin(); it != cameras_.end(); ++it) {
			ascii_proto_obss::camera_state stat;
			stat.cid = (*it)->cid;
			stat.state = (*it)->info->state;
			obss->camera.push_back(stat);
		}
		s1 = ascproto_->CompactObss(obss, n1);

		/* 构建望远镜信息 */
		aptele tele = boost::make_shared<ascii_proto_telescope>();
		tele->set_id(gid_, uid_);
		tele->utc     = nftele_->utc;
		tele->state   = nftele_->state;
		tele->errcode = nftele_->errcode;
		tele->ra      = nftele_->ra;
		tele->dec     = nftele_->dec;
		tele->azi     = nftele_->azi;
		tele->ele     = nftele_->ele;
		s2 = ascproto_->CompactTelescope(tele, n2);

		/* 构建观测计划信息 */
		if (plan_now_.use_count()) {
			applan plan = boost::make_shared<ascii_proto_plan>();
			plan->set_id(gid_, uid_);
			obss->set_timetag();
			plan->plan_sn = plan_now_->plan->plan_sn;
			plan->state   = plan_now_->state;
			s3 = ascproto_->CompactPlan(plan, n3);
		}

		{
			mutex_lock lck(mtx_client_);
			for (TcpCVec::iterator it = tcpc_client_.begin(); it != tcpc_client_.end(); ++it) {
				(*it)->Write(s1, n1);
				(*it)->Write(s2, n2);
				if (s3) (*it)->Write(s3, n3);
			}
		}
	}
}

/*
 * @note
 * 天光平场时间约定:
 * 民用晨昏时±0.5小时
 */
void ObservationSystem::thread_time() {
	double dawn, dusk, mjd0(0.0), mjd;
	int wait, fd, t11, t12, t21, t22;
	bool am;

	while(1) {
		ptime now = second_clock::local_time();
		mjd = now.date().modjulian_day();
		if (mjd0 != mjd) {// 日期改变时计算晨昏时, 并设置时间阈值
			civil_twilight(mjd, dawn, dusk);
			mjd0 = mjd;
			t11 = (int) (dawn * 3600 - 1800);
			t12 = t11 + 3600;
			t21 = (int) (dusk * 3600 - 1800);
			t22 = t21 + 3600;
		}
		fd = now.time_of_day().total_seconds();
		am = fd < 43200;
		if ((t11 <= fd && fd <= t12) || (t21 <= fd && fd <= t22)) odtype_ = OD_FLAT;
		else if ((t11 > fd) || (t22 < fd)) odtype_ = OD_NIGHT;
		else {// 白天时停止需要天光的曝光
			odtype_ = OD_DAY;
			if (plan_now_.use_count() && plan_now_->imgtype >= IMGTYPE_FLAT) interrupt_plan();
		}

		// 计算线程挂起时间
		if      (odtype_ == OD_FLAT)  wait = am ? (t12 - fd) : (t22 - fd);
		else if (odtype_ == OD_NIGHT) wait = am ? t11 - fd : 86400 - fd;
		else wait = t21 - fd;
		if (wait <= 0) wait = 1; // 规避特殊情况
		// 挂起线程
		boost::this_thread::sleep_for(boost::chrono::seconds(wait));
	}
}

///////////////////////////////////////////////////////////////////////////////
/* 消息响应函数 */
void ObservationSystem::receive_camera(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CAMERA : MSG_RECEIVE_CAMERA, client);
}

void ObservationSystem::register_message() {
	const CBSlot& slot01 = boost::bind(&ObservationSystem::on_receive_telescope,  this, _1, _2);
	const CBSlot& slot02 = boost::bind(&ObservationSystem::on_receive_camera,     this, _1, _2);
	const CBSlot& slot03 = boost::bind(&ObservationSystem::on_close_telescope,    this, _1, _2);
	const CBSlot& slot04 = boost::bind(&ObservationSystem::on_close_camera,       this, _1, _2);
	const CBSlot& slot05 = boost::bind(&ObservationSystem::on_telescope_track,    this, _1, _2);
	const CBSlot& slot06 = boost::bind(&ObservationSystem::on_out_safelimit,      this, _1, _2);
	const CBSlot& slot07 = boost::bind(&ObservationSystem::on_new_protocol,       this, _1, _2);
	const CBSlot& slot08 = boost::bind(&ObservationSystem::on_new_plan,           this, _1, _2);
	const CBSlot& slot09 = boost::bind(&ObservationSystem::on_flat_reslew,        this, _1, _2);

	RegisterMessage(MSG_RECEIVE_TELESCOPE,  slot01);
	RegisterMessage(MSG_RECEIVE_CAMERA,     slot02);
	RegisterMessage(MSG_CLOSE_TELESCOPE,    slot03);
	RegisterMessage(MSG_CLOSE_CAMERA,       slot04);
	RegisterMessage(MSG_TELESCOPE_TRACK,    slot05);
	RegisterMessage(MSG_OUT_SAFELIMIT,      slot06);
	RegisterMessage(MSG_NEW_PROTOCOL,       slot07);
	RegisterMessage(MSG_NEW_PLAN,           slot08);
	RegisterMessage(MSG_FLAT_RESLEW,        slot09);
}

void ObservationSystem::on_receive_telescope(const long, const long) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;

	while (tcpc_telescope_->IsOpen() && (pos = tcpc_telescope_->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			string ip = tcpc_telescope_->GetSocket().remote_endpoint().address().to_string();
			_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_telescope",
					"too long message from telescope [%s:%s]", gid_.c_str(), uid_.c_str());
			tcpc_telescope_->Close();
		}
		else {// 读取协议内容并解析执行
			tcpc_telescope_->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;

			proto = ascproto_->Resolve(bufrcv_.get());
			// 检查: 协议有效性及设备标志基本有效性
			if (!proto.use_count()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::on_receive_telescope",
						"illegal protocol. received: %s", bufrcv_.get());
				tcpc_telescope_->Close();
			}
			else {
				string type = proto->type;
				if      (iequals(type, APTYPE_TELE))   process_info_telescope (from_apbase<ascii_proto_telescope> (proto));
				else if (iequals(type, APTYPE_FOCUS))  process_info_focus     (from_apbase<ascii_proto_focus>     (proto));
				else if (iequals(type, APTYPE_MCOVER)) process_info_mcover    (from_apbase<ascii_proto_mcover>    (proto));
			}
		}
	}
}

void ObservationSystem::on_receive_camera(const long client, const long) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;
	ObssCamPtr camptr = find_camera((TCPClient*) client);
	TcpCPtr cliptr = camptr->tcptr;

	while (cliptr->IsOpen() && (pos = cliptr->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			_gLog.Write(LOG_FAULT, "ObservationSystem::on_receive_camera",
					"too long message from camera [%s:%s:%s]",
					gid_.c_str(), uid_.c_str(), camptr->cid.c_str());
			cliptr->Close();
		}
		else {// 读取协议内容并解析执行
			cliptr->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;

			proto = ascproto_->Resolve(bufrcv_.get());
			if (!proto.use_count()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::on_receive_camera",
						"illegal protocol. received: %s", bufrcv_.get());
				cliptr->Close();
			}
			else process_info_camera(camptr, from_apbase<ascii_proto_camera>(proto));
		}
	}
}

void ObservationSystem::on_close_telescope(const long, const long) {
	_gLog.Write(LOG_WARN, NULL, "telescope [%s:%s] is off-line", gid_.c_str(), uid_.c_str());
	tmLast_        = second_clock::universal_time();
	nftele_->state = TELESCOPE_ERROR;
	tcpc_telescope_.reset();
	// 处理观测计划
	if (plan_now_.use_count() && plan_now_->imgtype >= IMGTYPE_FLAT) interrupt_plan(OBSPLAN_INT);
}

void ObservationSystem::on_close_camera(const long client, const long ec) {
	mutex_lock lck(mtx_camera_);

	ObssCamVec::iterator it;
	TCPClient *tcptr = (TCPClient*) client;
	for (it = cameras_.begin(); it != cameras_.end() && (**it) != tcptr; ++it);
	if (it != cameras_.end()) {
		_gLog.Write(LOG_WARN, NULL, "camera [%s:%s:%s] is off-line",
				gid_.c_str(), uid_.c_str(), (*it)->cid.c_str());
		// 更新参数
		ObssCamPtr camptr = *it;
		tmLast_  = second_clock::universal_time();
		cameras_.erase(it);
		// 处理观测计划
		if (plan_now_.use_count()) {
			apcam info = camptr->info;
			bool leave_exposing(false), leave_waitflat(false);
			if (info->state > CAMCTL_IDLE) {
				leave_exposing = data_->leave_expoing();
				leave_waitflat = (info->state == CAMCTL_WAIT_FLAT && data_->leave_waitflat())
						|| (info->state == CAMCTL_WAIT_SYNC && data_->leave_waitsync());
			}
			if (leave_exposing) {
				change_planstate(plan_now_, OBSPLAN_OVER);
				plan_now_.reset();
			}
			else if (leave_waitflat) PostMessage(MSG_FLAT_RESLEW);
		}
		// 检查/更新系统工作状态
		switch_state();
	}
}

void ObservationSystem::on_telescope_track(const long, const long) {
	if (slew_arrived()) {// 指向到位
		command_expose(plan_now_->plan->cid, data_->exposing ? EXPOSE_RESUME : EXPOSE_START);
	}
}

void ObservationSystem::on_out_safelimit(const long, const long) {
	process_park();
}

void ObservationSystem::on_new_protocol(const long, const long) {
	apbase proto;
	string type;
	{// 从队列中提取队头的通信协议
		mutex_lock lck(mtx_queap_);
		proto = que_ap_.front();
		que_ap_.pop_front();
		type = proto->type;
	}
	// 分类处理通信协议
	if      (iequals(type, APTYPE_ABTSLEW))  process_abortslew  ();
	else if (iequals(type, APTYPE_ABTIMG))   process_abortimage (proto->cid);
	else if (iequals(type, APTYPE_PARK))     process_park();
	else if (iequals(type, APTYPE_ABTPLAN))  process_abortplan  (from_apbase<ascii_proto_abort_plan>  (proto));
	else if (iequals(type, APTYPE_DISABLE))  process_disable    (from_apbase<ascii_proto_disable>     (proto));
	else if (iequals(type, APTYPE_GUIDE))    process_guide      (from_apbase<ascii_proto_guide>       (proto));
	else if (iequals(type, APTYPE_FWHM))     process_fwhm       (from_apbase<ascii_proto_fwhm>        (proto));
	else if (iequals(type, APTYPE_FOCUS))    process_focus      (from_apbase<ascii_proto_focus>       (proto));
	else if (iequals(type, APTYPE_SLEWTO))   process_slewto     (from_apbase<ascii_proto_slewto>      (proto));
	else if (iequals(type, APTYPE_TAKIMG))   process_takeimage  (from_apbase<ascii_proto_take_image>  (proto));
	else if (iequals(type, APTYPE_START))    process_start      (from_apbase<ascii_proto_start>       (proto));
	else if (iequals(type, APTYPE_STOP))     process_stop       (from_apbase<ascii_proto_stop>        (proto));
	else if (iequals(type, APTYPE_MCOVER))   process_mcover     (from_apbase<ascii_proto_mcover>      (proto));
	else if (iequals(type, APTYPE_FINDHOME)) process_findhome   ();
	else if (iequals(type, APTYPE_HOMESYNC)) process_homesync   (from_apbase<ascii_proto_home_sync>   (proto));
	else if (iequals(type, APTYPE_ENABLE))   process_enable     (from_apbase<ascii_proto_enable>      (proto));
}

void ObservationSystem::on_flat_reslew(const long, const long) {
	bool reslew(true); // 重新指向标志
	// 平场采集失败时相机返回WAIT_SYNC. 所有相机都失败时, 每4分钟一次重新指向
	if (data_->waitsync && data_->waitsync == data_->exposing) {
		reslew = (second_clock::universal_time() - lastflat_).total_seconds() > 240;
	}

	if (!reslew) command_expose("", EXPOSE_RESUME);
	else {// 重新指向
		double ra, dec, epoch;
		int n;
		const char *s;

		lastflat_ = second_clock::universal_time();
		flat_position(ra, dec, epoch);
		// 通知相机位置
		s = ascproto_->CompactSlewto(ra, dec, epoch, n);
		write_to_camera(s, n);
		// 通知望远镜指向
		_gLog.Write("flat-field re-slew [%s:%s] to [%.4f, %.4f]",
				gid_.c_str(), uid_.c_str(), ra, dec);
		nftele_->SetObject(ra, dec);
		process_slewto(ra, dec, epoch);
	}
}

/*
 * void on_new_plan() 处理新的观测计划
 * 触发时间:
 * - GeneralControl通过NotifyPlan通知可用的观测计划
 * - ObservationSystem在完成计划观测/中断流程后立即检查是否有可执行计划
 */
void ObservationSystem::on_new_plan(const long, const long) {
	if (plan_wait_.use_count() && interrupt_plan()) {
		/* 计划还在等待区 */
		plan_now_ = plan_wait_;
		plan_wait_.reset();
		change_planstate(plan_now_, OBSPLAN_RUN);
		/* 开始执行计划 */
		resolve_obsplan(); // 将曝光参数发给相机
		if      (plan_now_->imgtype <= IMGTYPE_DARK) command_expose(plan_now_->plan->cid, EXPOSE_START);
		else if (plan_now_->imgtype == IMGTYPE_FLAT) on_flat_reslew(0, 0);
		else {
			apappplan plan = plan_now_->plan;
			nftele_->SetObject(plan->ra, plan->dec);
			process_slewto(plan->ra, plan->dec, plan->epoch);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/* 处理由GeneralControl转发的控制协议 */
/*
 * bool process_findhome() 望远镜搜索零点
 */
bool ObservationSystem::process_findhome() {
	if (data_->enabled && tcpc_telescope_.use_count() && !plan_now_.use_count()) {
		_gLog.Write("telescope [%s:%s] try to find home", gid_.c_str(), uid_.c_str());
		return true;
	}
	return false;
}

/*
 * bool process_homesync() 改变望远镜零点位置
 * - 该指令可能触发暂停观测计划及望远镜重新指向
 */
bool ObservationSystem::process_homesync(aphomesync proto) {
	if (data_->enabled && tcpc_telescope_.use_count()) {
		_gLog.Write("telescope [%s:%s] try to sync home. Telescope: [%.4f, %.4f] <== Sky: [%.4f, %.4f]",
				gid_.c_str(), uid_.c_str(), nftele_->ra, nftele_->dec, proto->ra, proto->dec);
		nftele_->ResetOffset();
		return true;
	}
	return false;
}

/*
 * bool process_slewto() 指向目标位置
 * - 执行计划时拒绝响应
 * - 检查位置是否安全
 */
bool ObservationSystem::process_slewto(apslewto proto) {
	if (data_->enabled && tcpc_telescope_.use_count() && !plan_now_.use_count()) {
		double ra(proto->ra), dc(proto->dec);
		if (safe_position(ra, dc)) {
			_gLog.Write("telescope [%s:%s] will slew to [%.4f, %.4f][degree], [%.1f]",
					gid_.c_str(), uid_.c_str(), ra, dc, proto->epoch);
			lastguide_ = ptime(not_a_date_time);
			nftele_->SetObject(ra, dc);
			return process_slewto(ra, dc, proto->epoch);
		}
	}
	return false;
}

/*
 * bool process_abortslew 中止指向和跟踪过程
 * - 该指令将触发停止观测计划
 */
bool ObservationSystem::process_abortslew() {
	if (data_->enabled && tcpc_telescope_.use_count()) {
		_gLog.Write("telescope [%s:%s] try to abort slew", gid_.c_str(), uid_.c_str());
		interrupt_plan();
		return ((nftele_->state == TELESCOPE_SLEWING || nftele_->state == TELESCOPE_PARKING));
	}
	return false;
}

/*
 * bool process_park() 复位
 */
bool ObservationSystem::process_park() {
	if (data_->enabled && tcpc_telescope_.use_count()
			&& nftele_->state != TELESCOPE_PARKING && nftele_->state != TELESCOPE_PARKED) {
		_gLog.Write("parking telescope [%s:%s]", gid_.c_str(), uid_.c_str());
		interrupt_plan();
		return true;
	}
	return false;
}

void ObservationSystem::process_guide(apguide proto) {
	if (data_->enabled && nftele_->state == TELESCOPE_TRACKING) {
		double ora(proto->objra), odec(proto->objdec); // 通信协议中的目标位置
		double tsame(AS2D); // 坐标相同阈值: 1角秒
		double era, edc;
		/* 计算导星量 */
		if (valid_ra(ora) && valid_dec(odec)) {// 由目标位置和天文定位位置计算导星量
			if ((era = fabs(ora - nftele_->ora)) > 180.0) era = 360.0 - era;
			edc = fabs(odec - nftele_->odec);
			if (era > tsame || edc > tsame) return; // 目标位置不同, 禁止导星
			// 计算导星量
			era = ora - proto->ra;
			edc = odec - proto->dec;
		}
		else {
			era = proto->ra;
			edc = proto->dec;
		}

		if ((fabs(era) > tguide_ || fabs(edc) > tguide_) && process_guide(era, edc)) {
			nftele_->Guide(era, edc);
			lastguide_ = second_clock::universal_time();
			_gLog.Write("telescope [%s:%s] do guiding for [%.1f, %.1f][arcsec]",
					gid_.c_str(), uid_.c_str(), era * 3600.0, edc * 3600.0);
		}
	}
}

/*
 * bool process_mcover() 开关镜盖
 */
bool ObservationSystem::process_mcover(apmcover proto) {
	if (data_->enabled) {
		MIRRORCOVER_COMMAND cmd = MIRRORCOVER_COMMAND(proto->value);
		if (!(cmd == MCC_CLOSE && plan_now_.use_count() && plan_now_->imgtype >= IMGTYPE_FLAT)) {
			_gLog.Write("%s mirror-cover for camera [%s:%s:%s]", cmd == MCC_OPEN ? "open" : "close",
					gid_.c_str(), uid_.c_str(), proto->cid.empty() ? "All" : proto->cid.c_str());
			return true;;
		}
	}
	return false;
}

/*
 * bool process_focus() 通知望远镜改变焦点位置
 */
bool ObservationSystem::process_focus(apfocus proto) {
	if (data_->enabled) {
		string cid = proto->cid;
		ObssCamPtr camptr = find_camera(cid);
		if (camptr.use_count() && camptr->info->focus != proto->value) {
			_gLog.Write("focus [%s:%s:%s] position will goto %d",
					gid_.c_str(), uid_.c_str(), cid.c_str(), proto->value);
			return true;
		}
	}
	return false;
}

/*
 * bool process_fwhm() 通过数据处理统计得到的FWHM, 通知望远镜调焦
 */
bool ObservationSystem::process_fwhm(apfwhm proto) {
	string cid = proto->cid;
	ObssCamPtr camptr = find_camera(cid);

	if (camptr.use_count() && fabs(camptr->fwhm - proto->value) > 0.1) {
		_gLog.Write("FWHM [%s:%s:%s] was %.2f", gid_.c_str(), uid_.c_str(), cid.c_str(), proto->value);
		camptr->fwhm = proto->value;
		return true;
	}
	return false;
}

/*
 * void process_takeimage() 手动曝光
 * - 系统状态 == OBSS_STOP, 手动模式
 */
void ObservationSystem::process_takeimage(aptakeimg proto) {
	if (data_->state != OBSS_STOP || data_->exposing || plan_now_.use_count()) return;
	ObsPlanPtr plan = boost::make_shared<ObservationPlan>(proto);
	IMAGE_TYPE imgtyp = plan->imgtype;
	if (imgtyp == IMGTYPE_ERROR || (imgtyp >= IMGTYPE_FLAT && nftele_->state == TELESCOPE_ERROR)) return;

	// 将指令构建为观测计划
	_gLog.Write("take image on [%s:%s]. image type is %s",
			gid_.c_str(), uid_.c_str(), plan->plan->imgtype.c_str());
	plan_now_ = plan;
	plan_now_->plan->gid = gid_;
	plan_now_->plan->uid = uid_;
	plan_now_->plan->cid = proto->cid;
	if (imgtyp >= IMGTYPE_OBJECT) {
		plan_now_->plan->ra = nftele_->ra;
		plan_now_->plan->dec= nftele_->dec;
		nftele_->Actual2Object();
	}
	// 执行观测计划
	resolve_obsplan();
	if (imgtyp == IMGTYPE_FLAT) PostMessage(MSG_FLAT_RESLEW);
	else command_expose(proto->cid, EXPOSE_START);
}

/*
 * void process_abortimage() 中止曝光
 * - 联动中止观测计划
 */
void ObservationSystem::process_abortimage(const string &cid) {
	command_expose(cid, EXPOSE_STOP);
}

/*
 * void process_abortplan() 删除观测计划
 * - 删除待执行计划
 * - 删除在执行计划
 */
void ObservationSystem::process_abortplan(apabtplan proto) {
	int plan_sn = proto->plan_sn;
	if (plan_wait_.use_count() && (plan_sn < 0 || (*plan_wait_) == plan_sn)) {
		change_planstate(plan_wait_, OBSPLAN_DELETE);
		plan_wait_.reset();
	}
	if (plan_now_.use_count() && (plan_sn == -1 || (*plan_now_) == plan_sn)) {
		interrupt_plan(OBSPLAN_DELETE);
	}
}

/*
 * void process_start() 启动自动化天文观测流程
 * - 响应处理观测计划
 */
void ObservationSystem::process_start(apstart proto) {
	if (!data_->automode) {
		_gLog.Write("Observation System [%s:%s] enter AUTO mode",
				gid_.c_str(), uid_.c_str());
		data_->automode = true;
		switch_state();
	}
}

/*
 * void process_stop() 停止自动化天文观测流程
 * - 响应处理手动观测需求
 */
void ObservationSystem::process_stop(apstop proto) {
	if (data_->automode) {
		_gLog.Write("Observation System [%s:%s] leave AUTO mode", gid_.c_str(), uid_.c_str());
		data_->automode = false;
		switch_state();
	}
}

/*
 * void process_enable() 启用指定相机或观测系统
 */
void ObservationSystem::process_enable(apenable proto) {
	string cid = proto->cid;
	bool empty = cid.empty();

	if (empty) {
		if (!data_->enabled) {
			_gLog.Write("Observation System [%s:%s] is enabled", gid_.c_str(), uid_.c_str());
			data_->enabled = true;
			switch_state();
		}
	}
	else {
		ObssCamPtr camptr = find_camera(cid);
		if (camptr.use_count() && !camptr->enabled) {
			_gLog.Write("camera [%s:%s:%s] is enabled", gid_.c_str(), uid_.c_str(), cid.c_str());
			camptr->enabled = true;
		}
	}
}

/*
 * void process_disable() 禁用指定相机或观测系统
 * - 禁用系统联动触发停止观测计划
 * - 禁用相机联动触发停止曝光
 */
void ObservationSystem::process_disable(apdisable proto) {
	string cid = proto->cid;
	bool empty = cid.empty();

	if (empty) {
		if (data_->enabled) {
			_gLog.Write("Observation System [%s:%s] is disabled", gid_.c_str(), uid_.c_str());
			data_->enabled = false;
			switch_state();
		}
	}
	else {
		ObssCamPtr camptr = find_camera(cid);
		if (camptr.use_count() && camptr->enabled) {
			_gLog.Write("Camera [%s:%s:%s] is disabled", gid_.c_str(), uid_.c_str(), cid.c_str());
			if (camptr->info->state > CAMCTL_IDLE) command_expose(cid, EXPOSE_STOP);
			camptr->enabled = false;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/*
 * void process_info_telescope() 处理通用望远镜工作状态
 * - 超出限位触发复位
 */
void ObservationSystem::process_info_telescope(aptele proto) {
	double ra(proto->ra), dc(proto->dec);
	TELESCOPE_STATE prev = nftele_->state;
	TELESCOPE_STATE now  = TELESCOPE_STATE(proto->state);
	// 更新望远镜指向坐标
	*nftele_ = *proto;

	if (proto->ele * D2R <= minEle_) {
		if (now != TELESCOPE_PARKING) {
			_gLog.Write(LOG_WARN, NULL, "telescope [%s:%s] position [%.4f, %.4f] was out of safe limit",
					gid_.c_str(), uid_.c_str(), ra, dc);
			PostMessage(MSG_OUT_SAFELIMIT);
		}
	}
	else if (now == TELESCOPE_TRACKING) {
		if (nftele_->StableArrive()) {
			_gLog.Write("telescope [%s:%s] arrived at [%.4f, %.4f][degree]", gid_.c_str(), uid_.c_str(), ra, dc);
			if (plan_now_.use_count()) PostMessage(MSG_TELESCOPE_TRACK);
		}
	}
	else if (now != prev && now == TELESCOPE_PARKED) {
		_gLog.Write("telescope [%s:%s] was parked", gid_.c_str(), uid_.c_str());
	}
}

/*
 * void process_info_focus() 处理焦点位置
 * - 将焦点位置发送给相机控制
 * - 焦点位置写入FITS头
 */
void ObservationSystem::process_info_focus(apfocus proto) {
	string cid = proto->cid;
	ObssCamPtr camptr = find_camera(cid);
	if (camptr.use_count() && proto->state == FOCUS_FREEZE && proto->value != camptr->info->focus) {
		_gLog.Write("focus [%s:%s:%s] position arrived at [%d]", gid_.c_str(), uid_.c_str(),
				cid.c_str(), proto->value);

		int n;
		const char *s = ascproto_->CompactFocus(proto, n);
		camptr->tcptr->Write(s, n);
	}
}

/*
 * void process_info_mcover() 处理镜盖状态信息
 * - 暂不关联图像类型判断是否产生影响
 */
void ObservationSystem::process_info_mcover(apmcover proto) {
	string cid = proto->cid;
	ObssCamPtr camptr = find_camera(cid);
	if (camptr.use_count() && proto->value != camptr->info->mcstate) {
		_gLog.Write("mirror-cover [%s:%s:%s] was %s", gid_.c_str(), uid_.c_str(),
				cid.c_str(), MIRRORCOVER_STATE_STR[proto->value]);

		int n;
		const char *s = ascproto_->CompactMirrorCover(proto, n);
		camptr->tcptr->Write(s, n);
	}
}

/*
 * void process_info_camera() 处理单台相机的工作状态信息
 * - 联动检查所有相机工作状态是否一致, 以驱动观测计划工作流程
 * - 一些异常处理: gtoaes崩溃后丢失观测计划; 相机控制端手动控制曝光
 */
void ObservationSystem::process_info_camera(ObssCamPtr camptr, apcam proto) {
	CAMCTL_STATUS prev = CAMCTL_STATUS(camptr->info->state);
	CAMCTL_STATUS now  = CAMCTL_STATUS(proto->state);
	camptr->info = proto;

	/*
	 * plan_now_判真原因:
	 * - 相机控制软件端人为控制
	 * - 服务器异常退出后重新启动
	 */
	bool valid_plan = plan_now_.use_count();
	if (now > CAMCTL_IDLE && prev <= CAMCTL_IDLE) {// 进入曝光态
		data_->enter_exposing();
	}
	else if (now <= CAMCTL_IDLE & prev > CAMCTL_IDLE) {// 离开曝光态
		/* 记录断点 */
		if (valid_plan && plan_now_->state == OBSPLAN_INT && plan_now_->plan->plan_sn != INT_MAX) {
			apobject ptbreak = plan_now_->GetBreakPoint(camptr->cid);
			if (ptbreak.use_count()) {
				ptbreak->ifilter = proto->ifilter;
				ptbreak->frmno   = proto->frmno;
				ptbreak->loopno  = proto->loopno;
			}
		}
		/* 处理观测计划 */
		if (data_->leave_expoing() && valid_plan) {
			// 观测计划正常结束
			if (plan_now_->state == OBSPLAN_RUN) change_planstate(plan_now_, OBSPLAN_OVER);
			plan_now_.reset();
			if (data_->state == OBSS_RUN) PostMessage(MSG_NEW_PLAN);
		}
	}
	else if (prev != now) {// 处理平场
		if (now == CAMCTL_WAIT_SYNC) {
			if (data_->enter_waitsync() && valid_plan) PostMessage(MSG_FLAT_RESLEW);
		}
		else if (now == CAMCTL_WAIT_FLAT) {
			if (data_->enter_waitflat() && valid_plan) PostMessage(MSG_FLAT_RESLEW);
		}
		else if (prev == CAMCTL_WAIT_SYNC) data_->leave_waitsync();
		else if (prev == CAMCTL_WAIT_FLAT) data_->leave_waitflat();
	}
}
