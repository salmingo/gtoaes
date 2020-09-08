/**
 * @file ObservaitonPlan.h 定义观测计划
 * @note
 * 定义两种观测计划:
 * @li 目标位置类型为双行根数
 * @li 目标位置类型为天球坐标
 */

#ifndef _OBSERVATION_PLAN_H_
#define _OBSERVATION_PLAN_H_

#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/smart_ptr.hpp>
#include "AstroDeviceDef.h"

using std::string;
using boost::posix_time::ptime;

/*!
 * @struct ObservationPlan 观测计划结构
 */
struct ObservationPlan {
	string gid;
	string uid;
	/*---------- 通用 ----------*/
	int plan_type;			//< 计划类型
	string plan_sn;			//< 计划编号
	string objname;			//< 目标名称
	ptime btime;			//< 观测计划期望起始时间, UTC
	ptime etime;			//< 观测计划期望结束时间, UTC

	/*---------- 位置与指向目标 ----------*/
	string line1, line2;	//< 引导跟踪模式
	// 定点指向模式
	int coorsys;			//< 坐标系
	double coor1, coor2;	//< 坐标, 量纲: 角度

	/*---------- 相机及图像文件参数 ----------*/
	string imgtype;			//< 图像类型
	string sabbr;			//< 目标名称缩略字, 用于文件名标志图像类型
	int iimgtyp;			//< 图像类型标志字
	double expdur;			//< 曝光时间, 量纲: 秒
	int frmcnt;				//< 图像帧数

	/* 执行状态 */
	int	state;				//< 计划状态

public:
	ObservationPlan() {
		plan_type  = PLANTYPE_ERROR;
		coorsys    = COORSYS_RADE;
		coor1 = coor2 = 0.0;
		iimgtyp    = IMGTYPE_OBJECT;
		expdur     = 0.0;
		frmcnt     = -1;
		state      = OBSPLAN_CAT;
	}

	/*!
	 * @brief 检查成员变量, 并填补关联参数
	 * @return
	 * 检查结果
	 */
	bool update() {
		// 检查图像类型, 并填充关联参数
		if (boost::iequals(imgtype, "bias")) {
			objname = "bias";
			sabbr   = "bias";
			iimgtyp = IMGTYPE_BIAS;
			expdur  = 0.0;
		}
		else if (boost::iequals(imgtype, "dark")) {
			objname = "dark";
			sabbr   = "dark";
			iimgtyp = IMGTYPE_DARK;
		}
		else if (boost::iequals(imgtype, "flat")) {
			objname = "flat";
			sabbr   = "flat";
			iimgtyp = IMGTYPE_FLAT;
		}
		else if (boost::iequals(imgtype, "object")) {
			sabbr   = "objt";
			iimgtyp = IMGTYPE_OBJECT;
		}
		else if (boost::iequals(imgtype, "focus")) {
			objname = "focus";
			sabbr   = "focs";
			iimgtyp = IMGTYPE_FOCUS;
		}

		return (plan_type != PLANTYPE_ERROR && iimgtyp != IMGTYPE_ERROR);
	}
};

typedef boost::shared_ptr<ObservationPlan> ObsPlanPtr;
typedef std::vector<ObsPlanPtr> ObsPlanVec;

#endif
