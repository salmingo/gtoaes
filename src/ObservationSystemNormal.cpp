/*
 * @file ObservationSystemNormal.cpp 定义GWAC观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#include "ObservationSystemNormal.h"
using namespace boost::posix_time;

ObsSysNormalPtr make_obss_normal(const string& gid, const string& uid) {
	return boost::make_shared<ObservationSystemNormal>(gid, uid);
}

ObservationSystemNormal::ObservationSystemNormal(const string& gid, const string& uid)
	: ObservationSystem(gid, uid) {

}

ObservationSystemNormal::~ObservationSystemNormal() {
}

bool ObservationSystemNormal::CoupleTelescope(TcpCPtr ptr) {
	if (!ObservationSystem::CoupleTelescope(ptr)) return false;
	// 重定向读出函数
	const TCPClient::CBSlot& slot = boost::bind(&ObservationSystemNormal::receive_telescope, this, _1, _2);
	ptr->RegisterRead(slot);

	return true;
}

bool ObservationSystemNormal::NotifyPlan(ObsPlanPtr plan) {
	if (!ObservationSystem::NotifyPlan(plan)) return false;

	return true;
}

int ObservationSystemNormal::relative_priority(int x) {
	int r1(x), r2(x);
	if (plan_now_.use_count())  r1 = x - plan_now_->plan->priority;
	if (plan_wait_.use_count()) r2 = x - plan_wait_->plan->priority;
	return (r1 < r2 ? r1 : r2);
}

bool ObservationSystemNormal::resolve_obsplan() {
	mutex_lock lck(mtx_camera_);

	// 检查图像类型
	apappplan plan = plan_now_->plan;
	IMAGE_TYPE imgtyp;
	string abbr;

	imgtyp = check_imgtype(plan->imgtype, abbr);
	if (imgtyp <= IMGTYPE_DARK) {
		plan->filter.clear();
		plan->delay.clear();
	}
	if (imgtyp != IMGTYPE_OBJECT || plan->objname.empty()) plan->objname = abbr;

	// 构建并发送格式化字符串
	vector<string> &filter = plan->filter;
	vector<double> &expdur = plan->expdur;
	vector<double> &delay  = plan->delay;
	vector<int>    &frmcnt = plan->frmcnt;
	int ncam = cameras_.size(), n, i;
	int nfilter(filter.size()), nexpdur(expdur.size()), ndelay(delay.size()), nfrmcnt(frmcnt.size());
	int ifilter(0), iexpdur(0), idelay(0), ifrmcnt(0);
	apobject object = boost::make_shared<ascii_proto_object>(); // 为各个相机分别生成参数
	const char *s;

	*object = *plan;
	for (i = 0; i < ncam; ++i) {
		// 为各相机分别构建ascii_proto_object对象
		object->expdur = expdur[iexpdur];
		object->frmcnt = frmcnt[ifrmcnt];
		if (nfilter) object->filter = filter[ifilter];
		if (ndelay)  object->delay  = delay[idelay];

		if (++ifilter >= nfilter) ifilter = 0;
		if (++iexpdur >= nexpdur) iexpdur = 0;
		if (++idelay  >= ndelay)  idelay  = 0;
		if (++ifrmcnt >= nfrmcnt) ifrmcnt = 0;

		// 构建并发送格式化字符串
		s = ascproto_->CompactObject(object, n);
		cameras_[i]->tcptr->Write(s, n);
	}
	return true;
}

void ObservationSystemNormal::receive_telescope(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_TELESCOPE : MSG_RECEIVE_TELESCOPE);
}

void ObservationSystemNormal::on_new_plan(const long, const long) {

}

bool ObservationSystemNormal::process_guide(apguide proto) {
	if (!ObservationSystem::process_guide(proto)) return false;
	int n;
	const char *s = ascproto_->CompactGuide(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_fwhm(apfwhm proto) {
	if (!ObservationSystem::process_fwhm(proto)) return false;
	int n;
	const char *s = ascproto_->CompactFWHM(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_focus(apfocus proto) {
	if (!ObservationSystem::process_focus(proto)) return false;
	int n;
	const char *s = ascproto_->CompactFocus(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_abortslew() {
	if (!ObservationSystem::process_abortslew()) return false;
	int n;
	const char *s = ascproto_->CompactAbortSlew(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_park() {
	if (!ObservationSystem::process_park()) return false;
	int n;
	const char *s = ascproto_->CompactPark(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_slewto(double ra, double dec, double epoch) {
	if (!tcpc_telescope_.use_count()) return false;
	int n;
	const char *s = ascproto_->CompactSlewto(ra, dec, epoch, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_mcover(apmcover proto) {
	if (!ObservationSystem::process_mcover(proto)) return false;
	int n;
	const char *s = ascproto_->CompactMirrorCover(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_findhome() {
	if (!ObservationSystem::process_findhome()) return false;
	int n;
	const char *s = ascproto_->CompactFindHome(n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_homesync(aphomesync proto) {
	if (!ObservationSystem::process_homesync(proto)) return false;
	int n;
	const char *s = ascproto_->CompactHomeSync(proto, n);
	return tcpc_telescope_->Write(s, n);
}

bool ObservationSystemNormal::process_focusync() {
	if (!ObservationSystem::process_focusync()) return false;
	int n;
	const char *s = ascproto_->CompactFocusSync(n);
	return tcpc_telescope_->Write(s, n);
}

void ObservationSystemNormal::process_abortplan(int plan_sn) {
	if (plan_wait_.use_count() && (plan_sn == -1 || (*plan_wait_) == plan_sn)) {
		change_planstate(plan_wait_, OBSPLAN_CAT);
		cb_planstate_changed_(plan_wait_);
		plan_wait_.reset();
	}
}
