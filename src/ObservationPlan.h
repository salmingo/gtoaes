/*
 * @file ObservationPlan.h 声明文件, 建立观测计划数据结构
 * @version 0.1
 * @date Nov 7, 2018
 * @note
 * - 基于通信协议, 建立观测计划数据结构
 * - 关联观测计划和观测系统
 * - 封装部分观测计划关联操作
 */

#ifndef OBSERVATIONPLAN_H_
#define OBSERVATIONPLAN_H_

#include <boost/container/stable_vector.hpp>
#include <boost/smart_ptr.hpp>
#include "AsciiProtocol.h"
#include "AstroDeviceState.h"

// 数据类型
typedef boost::container::stable_vector<apobject> ObsPlanBreakPtVec;

/*!
 * @struct ObservationPlan 观测计划基类
 */
struct ObservationPlan {
	apappplan		plan;		//< 以通信协议格式记录的观测计划
	IMAGE_TYPE      imgtype;	//< 图像类型
	OBSPLAN_STATUS	state;		//< 计划状态
	string			op_time;	//< 计划最后一次被执行起始时间
	ObsPlanBreakPtVec ptBreak;	//< 中断断点

public:
	ObservationPlan() {
		imgtype = IMGTYPE_ERROR;
		state = OBSPLAN_CAT;	// 初始化为: 入库
	}

	ObservationPlan(apappplan ap) {
		string abbr;
		plan    = ap;
		state   = OBSPLAN_CAT;	// 初始化为: 入库
		imgtype = check_imgtype(ap->imgtype, abbr);
		if (imgtype != IMGTYPE_OBJECT) plan->objname = abbr;
	}

	ObservationPlan(aptakeimg ap) {
		namespace pt = boost::posix_time;
		pt::ptime now = pt::second_clock::universal_time();
		string abbr;
		state   = OBSPLAN_RUN;	// 初始化为: 执行
		imgtype = check_imgtype(ap->imgtype, abbr);
		plan    = boost::make_shared<ascii_proto_append_plan>();
		plan->plan_sn		= INT_MAX;
		plan->plan_time		= pt::to_iso_extended_string(now);
		plan->plan_type		= "Manual";
		plan->objname		= abbr;
		plan->ra			= 1E30;
		plan->dec			= 1E30;
		plan->epoch			= 2000.0;
		plan->objra			= 1E30;
		plan->objdec		= 1E30;
		plan->objepoch		= 2000.0;
		plan->imgtype		= ap->imgtype;
		plan->expdur		= ap->expdur;
		plan->frmcnt		= ap->frmcnt;
		plan->priority		= 0xFFFF;
		plan->begin_time	= plan->plan_time;
		plan->end_time		= pt::to_iso_extended_string(now + pt::hours(2));
		plan->pair_id		= -1;
		plan->filter.push_back(ap->filter);
	}

	/*!
	 * @brief 验证计划编号的一致性
	 * @param plan_sn 计划编号
	 * @return
	 * 一致性
	 */
	bool operator==(int plan_sn) {
		return (plan.use_count() && plan->plan_sn == plan_sn);
	}

	/*!
	 * @brief 关联计划与观测系统
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @note
	 * 当计划状态变更为OBSPLAN_RUN时调用
	 */
	void CoupleOBSS(const string &_gid, const string &_uid) {
		namespace pt = boost::posix_time;
		if (plan->gid.empty()) plan->gid = _gid;
		if (plan->uid.empty()) plan->uid = _uid;
		op_time = pt::to_iso_extended_string(pt::second_clock::universal_time());
	}

	/*!
	 * @brief 检查/确认闲置计划是否与标志匹配
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @return
	 *  1: 强匹配. gid:uid完全相同
	 *  0: 弱匹配. gid:uid弱一致
	 * -1: gid:uid不一致
	 * -2: 计划不可被选择
	 */
	int IdleMatched(const string& _gid, const string& _uid) {
		if (state > OBSPLAN_INT) return -2;
		string gid = plan->gid;
		string uid = plan->uid;
		if (gid == _gid && uid == _uid) return 1;
		if (uid.empty() && (gid == _gid || gid.empty())) return 0;
		return -1;
	}

	/*!
	 * @brief 计算相对优先级
	 * @param x 参与比对优先级
	 * @return
	 * 观测计划优先级相对比对数的优先级
	 */
	int RelativePriority(int x) {
		if (!plan.use_count()) return -1;
		return (plan->priority - x);
	}

	/*!
	 * @brief 查找相机标志对应的断点
	 * @param cid 相机标志
	 * @return
	 * 断点地址
	 */
	apobject GetBreakPoint(const string &cid) {
		apobject object;
		ObsPlanBreakPtVec::iterator it;
		for (it = ptBreak.begin(); it != ptBreak.end() && (*it)->cid != cid; ++it);
		if (it != ptBreak.end()) object = *it;
		return object;
	}
};
typedef boost::shared_ptr<ObservationPlan> ObsPlanPtr;
typedef boost::container::stable_vector<ObsPlanPtr> ObsPlanVec;

#endif /* OBSERVATIONPLAN_H_ */
