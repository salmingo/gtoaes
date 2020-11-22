/**
 * @file ObservationPlanBase.h
 * @brief 定义: 天文观测计划
 * @version 0.1
 * @date 2020-11-07
 * @author 卢晓猛
 */

#ifndef OBSERVATIONPLANBASE_H_
#define OBSERVATIONPLANBASE_H_

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tokenizer.hpp>
#include <string>
#include <utility>
#include <vector>
#include "AstroDeviceDef.h"

using std::string;
using std::vector;
using namespace boost::posix_time;

/////////////////////////////////////////////////////////////////////////////
/*!
 * @struct ObservationPlanItem
 * @brief 观测计划
 */
struct ObservationPlanItem {
	/*!
	 * StrPair: 键值对
	 * - first:  键名称
	 * - second: 值
	 */
	using KVPair = std::pair<std::string, std::string>;
	using KVVec  = std::vector<KVPair>;
	using Tokenizer = boost::tokenizer<boost::char_separator<char>>;
	using Pointer   = boost::shared_ptr<ObservationPlanItem>;

	/* 计划: 主体 */
	string gid;			///< 组标志
	string uid;			///< 单元标志
	string plan_sn;		///< 计划编号
	string plan_time;	///< 计划生成时间
	string plan_type;	///< 计划类型
	string obstype;		///< 观测类型
	string grid_id;		///< 天区划分模式
	string field_id;	///< 天区编号
	string observer;	///< 观测者或触发源
	string objname;		///< 目标名称
	string runname;		///< 轮次编号
	int coorsys;		///< 指向位置的坐标系
	double lon;			///< 指向位置的经度, 角度
	double lat;			///< 指向位置的纬度, 角度
	string line1;		///< TLE第一行
	string line2;		///< TLE第二行
	double epoch;		///< 历元
	double objra;		///< 目标赤经, 角度
	double objdec;		///< 目标赤纬, 角度
	double objepoch;	///< 目标历元
	string objerror;	///< 坐标误差
	string imgtype;		///< 图像类型
	vector<string> filters;		///< 滤光片名称. 多滤光片之间使用|作为分隔符
	double expdur;		///< 曝光时间, 秒
	double delay;		///< 帧间延时, 秒
	int frmcnt;			///< 帧数
	int loopcnt;		///< 循环次数
	int priority;		///< 优先级
	ptime tmbegin;		///< 观测起始时间
	ptime tmend;		///< 观测结束时间
	int pair_id;		///< 配对观测标志
	KVVec kvs;			///< 未定义键值对

	/* 计划: 控制 */
	int iimgtype;		///< 图像类型的整数形式
	int ifilter;		///< 滤光片起始编号
	int iloop;			///< 循环完成次数
	int state;			///< 状态
	int period;			///< 计划需要的完成周期, 秒

public:
	ObservationPlanItem() {
		coorsys = TypeCoorSys::COORSYS_EQUA;
		lon = lat = 1E30;
		epoch = 2000.0;
		objra = objdec = 1E30;
		objepoch = 2000.0;
		expdur = delay = 0.0;
		frmcnt = loopcnt = 1;
		priority = 0;
		pair_id = -1;
		iimgtype = 0;
		ifilter = iloop = 0;
		state  = 0;
		period = 0;
		tmbegin = ptime(not_a_date_time);
		tmend   = ptime(not_a_date_time);
	}

	virtual ~ObservationPlanItem() {
		filters.clear();
		kvs.clear();
	}

	static Pointer Create() {
		return Pointer(new ObservationPlanItem);
	}

	/*!
	 * @brief 检查计划是否适用于观测系统
	 * @param _gid  观测系统组标志
	 * @param _uid  观测系统单元标志
	 * @return
	 * 检查结果
	 * - true:  观测计划可以在该系统上执行
	 * - false: 观测计划不可以在该系统上执行
	 */
	bool IsMatched(const string& _gid, const string& _uid) {
		return gid.empty() || (gid == _gid && (uid.empty() || uid == _uid));
	}

	/*!
	 * @brief 追加滤光片
	 * @param filname 滤光片名称
	 * @note
	 * 滤光片名中用"|;"等分隔符连接多个滤光片
	 */
	void AppendFilter(const string& filname) {
		boost::char_separator<char> seps{" |;+"};
		Tokenizer tok{filname, seps};
		for (const auto &t : tok)
			filters.push_back(t);
	}

	/*!
	 * @brief 设置观测起始时间
	 * @param str  字符串, 格式: CCYY-MM-DDThh:mm:ss
	 */
	void SetTimeBegin(const string& str) {
		try {
			tmbegin = from_iso_extended_string(str);
		}
		catch(std::out_of_range& ex) {
			tmbegin = second_clock::universal_time();
		}
	}

	/*!
	 * @brief 设置观测结束时间
	 * @param str  字符串, 格式: CCYY-MM-DDThh:mm:ss
	 */
	void SetTimeEnd(const string& str) {
		try {
			tmend = from_iso_extended_string(str);
		}
		catch(std::out_of_range& ex) {
			tmend = second_clock::universal_time() + hours(24);
		}
	}

	/*!
	 * @brief 计划主体填充完毕后, 检查计划的有效性, 并初始化控制参数
	 * @return
	 * - 本地的观测计划生命周期是1天. 避免某些计划长期沉淀
	 */
	bool CompleteCheck() {
		bool rslt;
		rslt = plan_sn.size()
			&& (iimgtype = TypeImage::FromString(imgtype.c_str())) != TypeImage::IMGTYP_MIN
			&& expdur >= 0.0
			&& frmcnt != 0;

		if (rslt) {
			ptime now = second_clock::universal_time();
			double t;

			if (tmbegin.is_special()) tmbegin = now;
			if (tmend.is_special())   tmend   = tmbegin + hours(24);
			if ((tmend - tmbegin).total_seconds() > 259200)
				tmend = tmbegin + ptime::date_duration_type(3);
			if ((t = expdur + delay) < 0.001) t = 0.001;
			if (filters.size()) t *= filters.size();
			period = int(t * frmcnt * loopcnt);
			rslt = (tmend - now).total_seconds() > period;
		}
		if (rslt)
			state = StateObservationPlan::OBSPLAN_CATALOGED;
		return rslt;
	}
};
using ObsPlanItemPtr = ObservationPlanItem::Pointer;

#endif /* SRC_OBSERVATIONPLANBASE_H_ */
