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
	cid = id;
	enabled = true;
}

ObservationSystem::ObservationSystem(const string& gid, const string& uid) {
	gid_      = gid;
	uid_      = uid;
	timezone_ = 8;
	minEle_   = 20.0 * D2R;
	odtype_   = OD_DAY;
	tmLast_   = second_clock::universal_time();
	lastguide_= tmLast_;
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
		_gLog.Write("Observation System [%s:%s] stop running", gid_.c_str(), uid_.c_str());
		interrupt_thread(thrd_client_);
		interrupt_thread(thrd_idle_);
		interrupt_thread(thrd_time_);
		Stop();
		data_->running = false;
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
	_gLog.Write("Elevation limit of Observation System [%s:%s] is %.1f",
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
		switch_state();
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
	if (change_planstate(plan_wait_, OBSPLAN_INT)) {
		_gLog.Write("plan[%d] on [%s:%s] is %s", plan_wait_->plan->plan_sn,
				gid_.c_str(), uid_.c_str(), OBSPLAN_STATUS_STR[plan_wait_->state]);
		cb_plan_finished_(plan_wait_);
	}
	/* 新的计划进入等待区 */
	plan_wait_ = plan; // 计划先进入等待区
	change_planstate(plan_wait_, OBSPLAN_WAIT); // 计划状态变更为等待
	PostMessage(MSG_NEW_PLAN);
}

/*
 * 计算观测计划的相对(当前计划/等待计划)优先级
 * - 相对优先级由继承类计算
 * - 检查系统状态
 * - 检查是否手动曝光: take_image
 * - 检查相对优先级
 * - 检查时间有效性
 * - 检查图像类型与时间匹配性和位置安全性
 */
int ObservationSystem::PlanRelativePriority(apappplan plan, ptime& now) {
	// 检查系统可用性
	if (!(data_->enabled && data_->automode && cameras_.size())) return -1;
	// 是否手动曝光
	if (!plan_now_.use_count() && data_->exposing) return -2;
	// 计算并评估相对优先级
	int r = relative_priority(plan->priority);
	if (r <= 0) return -3;
	// 检查时间有效性
	if ((from_iso_extended_string(plan->begin_time) - now).total_seconds() > 120) return -4;
	// 检查计划可执行性
	string abbr;
	IMAGE_TYPE imgtyp = check_imgtype(plan->imgtype, abbr);
	if ((imgtyp >= IMGTYPE_FLAT && !tcpc_telescope_.use_count())				// 望远镜不在线
		|| (imgtyp == IMGTYPE_FLAT && odtype_ != OD_FLAT)						// 非天光平场时间
		|| (imgtyp == IMGTYPE_OBJECT && odtype_ != OD_NIGHT)					// 非夜间观测时间
		|| (imgtyp >= IMGTYPE_OBJECT && !safe_position(plan->ra, plan->dec))	// 非安全位置
		)
		return -5;

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
 * - 天顶距阈值: [5, 10]度. 阈值>5度: 规避地平式天顶盲区
 * - 方位角阈值: 上午[180, 360), 下午[0, 180)
 */
void ObservationSystem::flat_position(double &ra, double &dec, double &epoch) {
	mutex_lock lck(mtx_ats_);
	ptime now = second_clock::universal_time();
	double fd = now.time_of_day().total_seconds() / DAYSEC;
	double azi0 = (fd * 24 + timezone_) < 12.0 ? 180.0 : 0.0;
	double azi, alt;

	ats_->SetMJD(now.date().modjulian_day() + fd);
	epoch = ats_->Epoch();
	azi = arc4random_uniform(180) + azi0;
	alt = 80 + arc4random_uniform(6);
	ats_->Horizon2Eq(azi * D2R, alt * D2R, ra, dec);
	ra   = reduce((ats_->LocalMeanSiderealTime() - ra) * R2D, 360.0);
	dec *= R2D;
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
 */
bool ObservationSystem::interrupt_plan(OBSPLAN_STATUS state) {
	bool immediate = !data_->exposing;

	if (change_planstate(plan_now_, state)) {
		_gLog.Write("plan[%d] on [%s:%s] is %s", plan_now_->plan->plan_sn,
				gid_.c_str(), uid_.c_str(), OBSPLAN_STATUS_STR[plan_now_->state]);

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
	OBSPLAN_STATUS old = plan->state;
	if (plan.use_count() && old != state) {
		if (!change_planstate(plan, old, state)) plan->state = state;
		if (state == OBSPLAN_RUN) plan->CoupleOBSS(gid_, uid_);
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
			(*it)->tcptr->Write(s, n);
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
	if (data_->state == OBSS_ERROR && tcpc_telescope_.use_count() && cameras_.size()) {
		data_->state = data_->automode ? OBSS_RUN : OBSS_STOP;
		if (data_->state == OBSS_RUN && !thrd_idle_.unique()) {
			thrd_idle_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_idle, this)));
		}
	}
	else if (data_->state == OBSS_RUN && !(tcpc_telescope_.use_count() && cameras_.size())) {
		if (plan_now_.use_count()) interrupt_plan(OBSPLAN_ABANDON);
		data_->state = OBSS_ERROR;
	}
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

		if (!(data_->enabled && data_->automode && data_->state == OBSS_ERROR && plan_now_.use_count()))
			continue;
		cb_acqnewplan_(gid_, uid_);
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
		tele->utc   = nftele_->utc;
		tele->state = nftele_->state;
		tele->ec    = nftele_->errcode;
		tele->ra    = nftele_->ra;
		tele->dc    = nftele_->dc;
		tele->azi   = nftele_->az;
		tele->ele   = nftele_->el;
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
		else odtype_ = OD_DAY;
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
	const CBSlot& slot05 = boost::bind(&ObservationSystem::on_telescope_tracking, this, _1, _2);
	const CBSlot& slot06 = boost::bind(&ObservationSystem::on_out_safelimit,      this, _1, _2);
	const CBSlot& slot07 = boost::bind(&ObservationSystem::on_new_protocol,       this, _1, _2);
	const CBSlot& slot08 = boost::bind(&ObservationSystem::on_new_plan,           this, _1, _2);
	const CBSlot& slot09 = boost::bind(&ObservationSystem::on_flat_reslew,        this, _1, _2);

	RegisterMessage(MSG_RECEIVE_TELESCOPE,  slot01);
	RegisterMessage(MSG_RECEIVE_CAMERA,     slot02);
	RegisterMessage(MSG_CLOSE_TELESCOPE,    slot03);
	RegisterMessage(MSG_CLOSE_CAMERA,       slot04);
	RegisterMessage(MSG_TELESCOPE_TRACKING, slot05);
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
				if (iequals(type, APTYPE_TELE))        process_info_telescope (from_apbase<ascii_proto_telescope> (proto));
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
			// 检查: 协议有效性及设备标志基本有效性
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
	tmLast_ = second_clock::universal_time();
	data_->state   = OBSS_ERROR;
	nftele_->state = TELESCOPE_ERROR;
	tcpc_telescope_.reset();
	if (data_->lighting) interrupt_plan();
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
		if ((*it)->info->state > CAMCTL_IDLE) {
			data_->leave_expoing(plan_now_->imgtype);
			if ((*it)->info->state == CAMCTL_WAIT_FLAT) data_->leave_waitflat();
		}
		tmLast_  = second_clock::universal_time();
		cameras_.erase(it);
		switch_state();
	}
}

void ObservationSystem::on_telescope_tracking(const long, const long) {
	/* 检查指向是否到位 */

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
	if      (iequals(type, APTYPE_ABTSLEW))  process_abortslew();
	else if (iequals(type, APTYPE_ABTIMG))   process_abortimage(proto->cid.c_str());
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
	else if (iequals(type, APTYPE_FOCSYNC))  process_focusync   ();
	else if (iequals(type, APTYPE_ENABLE))   process_enable     (from_apbase<ascii_proto_enable>      (proto));
}

void ObservationSystem::on_flat_reslew(const long, const long) {
	double ra, dec, epoch;
	int n;
	const char *s;

	flat_position(ra, dec, epoch);
	s = ascproto_->CompactSlewto(ra, dec, epoch, n);
	/*
	 * - 设置控制参数
	 * - 指向
	 * - 通知相机指向位置
	 */
	nftele_->SetObject(ra, dec);
	process_slewto(ra, dec, epoch);
	write_to_camera(s, n);
}

///////////////////////////////////////////////////////////////////////////////
/* 处理由GeneralControl转发的控制协议 */
/*!
 * @brief 望远镜搜索零点
 * @note
 * 在观测计划执行过程中可能拒绝该指令
 */
bool ObservationSystem::process_findhome() {
	return (tcpc_telescope_.use_count() && !plan_now_.use_count());
}

/*!
 * @brief 改变望远镜零点位置
 * @note
 * 该指令将触发暂停观测计划及望远镜重新指向
 */
bool ObservationSystem::process_homesync(aphomesync proto) {
	if (tcpc_telescope_.use_count()) {
		nftele_->ResetOffset();
		return true;
	}
	return false;
}

/*!
 * bool process_slewto() 指向目标位置
 * - 执行计划时拒绝响应
 * - 检查位置是否安全
 */
bool ObservationSystem::process_slewto(apslewto proto) {
	if (plan_now_.use_count() || !safe_position(proto->ra, proto->dc))
		return false;
	nftele_->BeginMove();
	return process_slewto(proto->ra, proto->dc, proto->epoch);
}

/*!
 * @brief 中止指向和跟踪过程
 * @note
 * 该指令将触发停止观测计划
 */
bool ObservationSystem::process_abortslew() {
	interrupt_plan();
	return (nftele_->state == TELESCOPE_SLEWING);
}

/*!
 * @brief 复位
 * @note
 * 该指令将触发停止观测计划
 */
bool ObservationSystem::process_park() {
	interrupt_plan();
	nftele_->BeginMove();
	return (nftele_->state != TELESCOPE_PARKING && nftele_->state != TELESCOPE_PARKED);
}

/*!
 * @brief 开关镜盖
 * @note
 * 关闭镜盖指令可能触发停止观测计划
 */
bool ObservationSystem::process_mcover(apmcover proto) {
	return tcpc_telescope_.use_count();
}

/*!
 * @brief 改变调焦器零点位置
 */
bool ObservationSystem::process_focusync() {
	return tcpc_telescope_.use_count();
}

/*!
 * @brief 通知望远镜改变焦点位置
 * @param proto 通信协议
 */
bool ObservationSystem::process_focus(apfocus proto) {
	return tcpc_telescope_.use_count();
}

/*!
 * @brief 通过数据处理统计得到的FWHM, 通知望远镜调焦
 * @param proto 通信协议
 */
bool ObservationSystem::process_fwhm(apfwhm proto) {
	return tcpc_telescope_.use_count();
}

/*
 * void process_takeimage() 手动曝光
 */
void ObservationSystem::process_takeimage(aptakeimg proto) {
	string abbr;
	IMAGE_TYPE imgtyp = check_imgtype(proto->imgtype, abbr);
	if (!cameras_.size() || data_->exposing || plan_now_.use_count()
			|| imgtyp == IMGTYPE_ERROR
			|| (imgtyp >= IMGTYPE_FLAT && nftele_->state == TELESCOPE_ERROR)) return;
	// 构建观测计划
	plan_now_ = boost::make_shared<ObservationPlan>(proto);
	plan_now_->plan->gid = gid_;
	plan_now_->plan->uid = uid_;
	plan_now_->plan->cid = proto->cid;
	// 构建待发送信息结构体
	apobject obj = boost::make_shared<ascii_proto_object>(*(plan_now_->plan));
	obj->filter = proto->filter;
	obj->expdur = proto->expdur;
	obj->frmcnt = proto->frmcnt;
	if (imgtyp >= IMGTYPE_OBJECT) {
		obj->ra = nftele_->ra;
		obj->dec= nftele_->dc;
	}
	// 构建发送给相机的格式化信息
	int n;
	const char *s = ascproto_->CompactObject(obj, n);
	write_to_camera(s, n, proto->cid.c_str());
	if (imgtyp != IMGTYPE_FLAT) {
		s = ascproto_->CompactExpose(EXPOSE_START, n);
		write_to_camera(s, n, proto->cid.c_str());
	}
	else PostMessage(MSG_FLAT_RESLEW);
}

/*
 * void process_abortimage() 中止曝光
 * - cid == NULL: 中止观测计划
 * - cid != NULL: 中止相机曝光
 */
void ObservationSystem::process_abortimage(const char *cid) {
	if (!cid || iequals(cid, "")) interrupt_plan();
	else {
		int n;
		const char *s = ascproto_->CompactExpose(EXPOSE_STOP, n);
		write_to_camera(s, n, cid);
	}
}

/*!
 * @brief 删除观测计划
 * @note
 * 该指令将触发停止观测计划
 */
void ObservationSystem::process_abortplan(apabtplan proto) {
	int plan_sn = proto->plan_sn;

	process_abortplan(plan_sn);
	if (plan_now_.use_count() && (plan_sn == -1 || (*plan_now_) == plan_sn)) {
		interrupt_plan(OBSPLAN_DELETE);
	}
}

/*!
 * @brief 启动自动化天文观测流程
 */
void ObservationSystem::process_start(apstart proto) {
	if (!data_->automode) {
		_gLog.Write("Observation System [%s:%s] enter AUTO mode",
				gid_.c_str(), uid_.c_str());
		data_->automode = true;
		if (data_->state == OBSS_STOP) {// 更新系统工作状态
			data_->state = OBSS_RUN;
			if (!thrd_idle_.unique()) {
				thrd_idle_.reset(new boost::thread(boost::bind(&ObservationSystem::thread_idle, this)));
			}
		}
	}
}

void ObservationSystem::process_stop(apstop proto) {
	if (data_->automode) {
		_gLog.Write("Observation System [%s:%s] enter MANUAL mode", gid_.c_str(), uid_.c_str());
		interrupt_thread(thrd_idle_);
		data_->automode = false;
		if (data_->state == OBSS_RUN) data_->state = OBSS_STOP;
		interrupt_plan();
	}
}

/*!
 * @brief 启用指定相机或观测系统
 * @param proto 通信协议
 */
void ObservationSystem::process_enable(apenable proto) {
	string cid = proto->cid;
	bool empty = cid.empty();

	if (empty && !data_->enabled) {
		_gLog.Write("Observation System [%s:%s] is enabled", gid_.c_str(), uid_.c_str());
		data_->enabled = true;
	}
	else if (!empty && cameras_.size()) {
		mutex_lock lck(mtx_camera_);
		ObssCamVec::iterator it;
		for (it = cameras_.begin(); it != cameras_.end() && (**it) != cid; ++it);
		if (!(it == cameras_.end() || (*it)->enabled)) {
			_gLog.Write("Camera [%s:%s:%s] is enabled", gid_.c_str(), uid_.c_str(), cid.c_str());
			(*it)->enabled = true;
		}
	}
}

void ObservationSystem::process_disable(apdisable proto) {
	string cid = proto->cid;
	bool empty = cid.empty();

	if (empty && data_->enabled) {
		_gLog.Write("Observation System [%s:%s] is disabled", gid_.c_str(), uid_.c_str());
		data_->enabled = false;
		interrupt_plan();
	}
	else if (!empty && cameras_.size()) {
		process_abortimage(cid.c_str()); // 中止曝光

		mutex_lock lck(mtx_camera_);
		ObssCamVec::iterator it;
		for (it = cameras_.begin(); it != cameras_.end() && (**it) != cid; ++it);
		if (it != cameras_.end() && (*it)->enabled) {
			_gLog.Write("Camera [%s:%s:%s] is disabled", gid_.c_str(), uid_.c_str(), cid.c_str());
			(*it)->enabled = false;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
/*!
 * @brief 处理望远镜状态信息
 * @param proto 通信协议
 */
void ObservationSystem::process_info_telescope(aptele proto) {
	double ra(proto->ra), dc(proto->dc);
	TELESCOPE_STATE state = proto->state;
	// 更新望远镜指向坐标
	*nftele_ = *proto;

	if (proto->ele * D2R <= minEle_) {
		if (state != TELESCOPE_PARKING) {
			_gLog.Write(LOG_WARN, NULL, "telescope [%s:%s] position [%.4f, %.4f] is out of safe limit",
					gid_.c_str(), uid_.c_str(), ra, dc);
			PostMessage(MSG_OUT_SAFELIMIT);
		}
	}
	else if (state == TELESCOPE_TRACKING || state == TELESCOPE_PARKED) {
		if (nftele_->StableArrive()) {
			_gLog.Write("telescope [%s:%s] arrived at [%.4f, %.4f]",
					gid_.c_str(), uid_.c_str(), ra, dc);
			if (state == TELESCOPE_TRACKING) PostMessage(MSG_TELESCOPE_TRACKING);
		}
	}
}

/*!
 * @brief 处理调焦器状态信息
 * @param proto 通信协议
 */
void ObservationSystem::process_info_focus(apfocus proto) {

}

/*!
 * @brief 处理镜盖状态信息
 * @param proto 通信协议
 */
void ObservationSystem::process_info_mcover(apmcover proto) {

}

void ObservationSystem::process_info_camera(ObssCamPtr camptr, apcam proto) {
	CAMCTL_STATUS old_stat = CAMCTL_STATUS(camptr->info->state);
	CAMCTL_STATUS new_stat = CAMCTL_STATUS(proto->state);
	camptr->info = proto;

	if (old_stat != new_stat) {
		switch(new_stat) {
		case CAMCTL_IDLE:
			if (old_stat > CAMCTL_IDLE) {
				/* 记录断点 */
				if (plan_now_->state == OBSPLAN_INT) {
					apobject ptbreak = plan_now_->GetBreakPoint(camptr->cid);
					if (ptbreak.use_count()) ptbreak->frmno = proto->frmno;
				}
				/* 处理观测计划 */
				if (data_->leave_expoing(plan_now_->imgtype)) {
					if (plan_now_->state == OBSPLAN_RUN) {
						change_planstate(plan_now_, OBSPLAN_OVER);
						cb_plan_finished_(plan_now_);

						_gLog.Write("plan [%d] on [%s:%s] is %s", plan_now_->plan->plan_sn,
								gid_.c_str(), uid_.c_str(), OBSPLAN_STATUS_STR[plan_now_->state]);
					}
					plan_now_.reset();
					PostMessage(MSG_NEW_PLAN);
				}
			}
			break;
		case CAMCTL_EXPOSE:
			if (old_stat == CAMCTL_IDLE) data_->enter_exposing(plan_now_->imgtype);
			break;
		case CAMCTL_WAIT_FLAT:// 相机完成一次曝光后即暂停并返回WAIT_FLAT状态
			if (data_->enter_waitflat()) {
				PostMessage(MSG_FLAT_RESLEW);
				data_->waitflat = 0;
			}
			break;
		default:
			break;
		}
	}
}

/*
 * void on_new_plan() 处理新的观测计划
 * 触发时间:
 * - GeneralControl通过NotifyPlan通知可用的观测计划
 * - ObservationSystem在完成计划观测/中断流程后立即检查是否有可执行计划
 */
void ObservationSystem::on_new_plan(const long, const long) {
	/* 计划还在等待区 */
	if (plan_wait_.use_count() && (!plan_now_.use_count() || interrupt_plan())) {
		plan_now_ = plan_wait_;
		plan_wait_.reset();

		change_planstate(plan_now_, OBSPLAN_RUN);
		_gLog.Write("plan[%d] on [%s:%s] is %s", plan_now_->plan->plan_sn,
				gid_.c_str(), uid_.c_str(), OBSPLAN_STATUS_STR[plan_now_->state]);
	}
	/* 开始执行计划 */
	if (plan_now_.use_count() && plan_now_->state == OBSPLAN_RUN) {
		resolve_obsplan(); // 将曝光参数发给相机
		if (plan_now_->imgtype <= IMGTYPE_DARK) command_expose(plan_now_->plan->cid, EXPOSE_START);
		else if (plan_now_->imgtype >= IMGTYPE_OBJECT) {
			apappplan plan = plan_now_->plan;
			nftele_->SetObject(plan->ra, plan->dec);
			if (target_arrived()) command_expose(plan->cid, EXPOSE_START);
			else process_slewto(plan->ra, plan->dec, plan->epoch);
		}
		else {// plan_now_->imgtype == IMGTYPE_FLAT
			on_flat_reslew(0, 0);
		}
	}
}
