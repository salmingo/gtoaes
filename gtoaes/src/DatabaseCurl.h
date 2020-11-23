/**
 * @file DatabaseCurl.h
 * @brief 基于curl的数据库访问接口
 * @version 1.0
 * @date 2020-11-10
 * @author 卢晓猛
 */

#ifndef SRC_DATABASECURL_H_
#define SRC_DATABASECURL_H_

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_types.hpp>
#include <string>
#include "CurlBase.h"
#include "ObservationPlanBase.h"
#include "KvProtocol.h"

using std::string;

class DatabaseCurl: public CurlBase {
public:
	DatabaseCurl(const std::string &urlRoot);
	virtual ~DatabaseCurl();

public:
	using Pointer = boost::shared_ptr<DatabaseCurl>;
	using MtxLck  = boost::unique_lock<boost::mutex>;

protected:
	/* 成员变量 */
	string upObsPlan_;			///< 观测计划
	string updObsPlanState_;	///< 观测计划状态
	string updMountLinked_;	///< 转台连接/断开
	string updMountState_;		///< 转台状态
	string updCameraLinked_;	///< 相机连接/断开
	string updCameraState_;	///< 相机状态
	string updDomeLinked_;		///< 圆顶连接/断开
	string updDomeState_;		///< 圆顶状态
	string regOrigImg_;		///< 注册原始图像
	string upFile_;			///< 上传文件

public:
	static Pointer Create(const std::string &urlRoot) {
		return Pointer(new DatabaseCurl(urlRoot));
	}

	int UploadObservationPlan(ObsPlanItemPtr plan);
	int UpdateObservationPlanState(const string&plan_sn, const string& state, const string& utc);
	int UpdateMountLinked(const string& gid, const string& uid, bool linked);
	int UpdateMountState(kvmount proto);
	int UpdateCameraLinked(const string& gid, const string& uid, const string& cid, bool linked);
	int UpdateCameraState(kvcamera proto);
	int UpdateDomeLinked(const string& gid, bool linked);
	int UpdateDomeState(const string& gid, const string& state);
};
using DBCurlPtr = DatabaseCurl::Pointer;

#endif /* SRC_DATABASECURL_H_ */
