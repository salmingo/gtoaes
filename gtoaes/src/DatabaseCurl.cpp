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
	updMountLinked_  = "updateMountLinked.action";          ///< 转台连接/断开
	updMountState_   = "updateMountState.action";           ///< 转台状态
	updCameraLinked_ = "updateCameraLinked.action";         ///< 相机连接/断开
	updCameraState_  = "updateCameraState.action";          ///< 相机状态
	updDomeLinked_   = "updateDomeLinked.action";           ///< 圆顶连接/断开
	updDomeState_    = "updateDomeState.action";            ///< 圆顶状态
	regOrigImg_      = "regOrigImg.action";                 ///< 注册原始图像
	upFile_          = "uploadFile.action";                 ///< 上传文件
}

DatabaseCurl::~DatabaseCurl() {

}

int DatabaseCurl::UploadObservationPlan(ObsPlanItemPtr plan) {
	return 0;
}

int DatabaseCurl::UpdateObservationPlanState(const string&plan_sn, const string& state, const string& utc) {
	return 0;
}

int DatabaseCurl::UpdateMountLinked(const string& gid, const string& uid, bool linked) {
	return 0;
}

int DatabaseCurl::UpdateMountState(kvmount proto) {
	return 0;
}

int DatabaseCurl::UpdateCameraLinked(const string& gid, const string& uid, const string& cid, bool linked) {
	return 0;
}

int DatabaseCurl::UpdateCameraState(kvcamera proto) {
	return 0;
}

int DatabaseCurl::UpdateDomeLinked(const string& gid, bool linked) {
	return 0;
}

int DatabaseCurl::UpdateDomeState(const string& gid, const string& state) {
	return 0;
}
