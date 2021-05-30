/**
 * @file DatabaseCurl.cpp
 * @brief 基于curl的数据库访问接口
 * @version 1.0
 * @date 2020-11-10
 * @author 卢晓猛
 */

#include "DatabaseCurl.h"

DatabaseCurl::DatabaseCurl(const std::string &urlRoot)
		: CurlBase(urlRoot) {
	upObsPlan_       = "uploadObservationPlan.action";      ///< 观测计划
	updObsPlanState_ = "updateObservationPlanState.action"; ///< 观测计划状态
	updMountLinked_  = "updateMountLinked.action";	///< 转台连接/断开
	updMountState_   = "updateMountState.action";	///< 转台状态
	updCameraLinked_ = "updateCameraLinked.action";	///< 相机连接/断开
	updCameraState_  = "updateCameraState.action";	///< 相机状态
	updDomeLinked_   = "updateDomeLinked.action";	///< 圆顶连接/断开
	updDomeState_    = "updateDomeState.action";	///< 圆顶状态
	regOrigImg_      = "regOrigImg.action";		///< 注册原始图像
	upFile_          = "uploadFile.action";		///< 上传文件
}

DatabaseCurl::~DatabaseCurl() {
}

int DatabaseCurl::UploadObservationPlan(ObsPlanItemPtr plan) {
	prepare();
	append_pair_kv("plan_sn", plan->plan_sn);
	//...
	return upload(upObsPlan_);
}

int DatabaseCurl::UpdateObservationPlanState(const string&plan_sn, const string& state, const string& utc) {
	prepare();
	append_pair_kv("plan_sn", plan_sn);
	append_pair_kv("state",   state);
	append_pair_kv("utc",     utc);
	return upload(updObsPlanState_);
}

int DatabaseCurl::UpdateMountLinked(const string& gid, const string& uid, bool linked) {
	prepare();
	append_pair_kv("gid",    gid);
	append_pair_kv("uid",    uid);
	append_pair_kv("linked", std::to_string(linked));

	return upload(updMountLinked_);
}

int DatabaseCurl::UpdateMountState(kvmount proto) {
	prepare();
	return upload(updMountState_);
}

int DatabaseCurl::UpdateCameraLinked(const string& gid, const string& uid, const string& cid, bool linked) {
	prepare();
	append_pair_kv("gid",    gid);
	append_pair_kv("uid",    uid);
	append_pair_kv("cid",    cid);
	append_pair_kv("linked", std::to_string(linked));

	return upload(updMountLinked_);
}

int DatabaseCurl::UpdateCameraState(kvcamera proto) {
	prepare();
	append_pair_kv("gid",    proto->gid);
	append_pair_kv("uid",    proto->uid);
	append_pair_kv("cid",    proto->cid);
	append_pair_kv("state",  std::to_string(proto->state));
	return upload(updCameraState_);
}

int DatabaseCurl::UpdateDomeLinked(const string& gid, bool linked) {
	prepare();
	append_pair_kv("gid",    gid);
	append_pair_kv("linked", std::to_string(linked));

	return upload(updDomeLinked_);
}

int DatabaseCurl::UpdateDomeState(const string& gid, const string& state) {
	prepare();
	append_pair_kv("gid",    gid);
	append_pair_kv("state",  state);
	return upload(updDomeState_);
}
