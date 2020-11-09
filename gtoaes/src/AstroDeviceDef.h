/*!
 * @file AstroDeviceDef.h 声明与光学天文望远镜观测相关的状态、指令、类型等相关的常量
 * @version 0.1
 * @date 2017-11-24
 * @version 0.2
 * @date 2020-11-01
 * @note
 * - 重新定义enum类型名称和数值
 * - 重新定义enum类型对应字符串的获取方式
 */

#ifndef ASTRO_DEVICE_DEFINE_H_
#define ASTRO_DEVICE_DEFINE_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 状态与指令 */
/////////////////////////////////////////////////////////////////////////////
static const char* coorsys_desc[] = {
	"AltAzimuth",
	"Equatorial",
	"TwoLineElement"
};

/*!
 * @class TypeCoorSys
 * @breif 定义: 坐标系类型
 */
class TypeCoorSys {
public:
	enum {///< 坐标系类型
		COORSYS_MIN = -1,
		COORSYS_ALTAZ,	///< 地平系
		COORSYS_EQUA,	///< 赤道系
		COORSYS_ORBIT,	///< 引导
		COORSYS_MAX
	};

public:
	static bool IsValid(int type) {
		return type > COORSYS_MIN && type < COORSYS_MAX;
	}

	static const char* ToString(int type) {
		return IsValid(type) ? coorsys_desc[type] : NULL;
	}

	static int FromString(const char* name) {
		int type(COORSYS_MIN);

		if (name) {
			if (isdigit(name[0])) {
				type = atoi(name);
			}
			else {
				for (type = COORSYS_MIN + 1; type < COORSYS_MAX && strcmp(name, coorsys_desc[type]); ++type);
			}
		}
		return IsValid(type) ? type : COORSYS_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* mount_desc[] = {
	"Error",
	"Freeze",
	"Homing",
	"Homed",
	"Parking",
	"Parked",
	"Slewing",
	"Tracking"
};

/*!
 * @class StateMount
 * @brief 定义: 转台状态
 */
class StateMount {
public:
	enum {// 转台状态
		MOUNT_MIN = -1,
		MOUNT_ERROR,	///< 错误
		MOUNT_FREEZE,	///< 静止
		MOUNT_HOMING,	///< 找零
		MOUNT_HOMED,	///< 找到零点
		MOUNT_PARKING,	///< 复位
		MOUNT_PARKED,	///< 已复位
		MOUNT_SLEWING,	///< 指向
		MOUNT_TRACKING,	///< 跟踪
		MOUNT_MAX
	};

public:
	static bool IsValid(int state) {
		return state > MOUNT_MIN && state < MOUNT_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? mount_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(MOUNT_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = MOUNT_MIN + 1; state < MOUNT_MAX && strcmp(name, mount_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : MOUNT_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* slitc_desc[] = {
	"close",
	"open",
	"stop"
};

class CommandSlit {
public:
	enum {// 镜盖指令
		SLITC_MIN = -1,
		SLITC_CLOSE,	///< 关闭
		SLITC_OPEN,	///< 打开
		SLITC_STOP,	///< 停止
		SLITC_MAX
	};

public:
	static bool IsValid(int cmd) {
		return cmd > SLITC_MIN && cmd < SLITC_MAX;
	}

	static const char* ToString(int cmd) {
		return IsValid(cmd) ? slitc_desc[cmd] : NULL;
	}

	static int FromString(const char* name) {
		int cmd(SLITC_MIN);

		if (name) {
			if (isdigit(name[0])) {
				cmd = atoi(name);
			}
			else {
				for (cmd = SLITC_MIN + 1; cmd < SLITC_MAX && strcmp(name, slitc_desc[cmd]); ++cmd);
			}
		}
		return IsValid(cmd) ? cmd : SLITC_MIN;
	}
};

static const char* slit_desc[] = {
	"Error",
	"Opening",
	"Opened",
	"Closing",
	"Closed"
};

class StateSlit {
public:
	enum {// 镜盖指令
		SLIT_MIN = -1,
		SLIT_ERROR,		///< 错误
		SLIT_OPENING,	///< 打开中
		SLIT_OPENED,	///< 已打开
		SLIT_CLOSING,	///< 关闭中
		SLIT_CLOSED,	///< 已关闭
		SLIT_MAX
	};

public:
	static bool IsValid(int state) {
		return state > SLIT_MIN && state < SLIT_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? slit_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(SLIT_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = SLIT_MIN + 1; state < SLIT_MAX && strcmp(name, slit_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : SLIT_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* mcc_desc[] = {
	"close",
	"open"
};

/*!
 * @class CommandMirrorCover
 * @brief 定义: 镜盖指令
 */
class CommandMirrorCover {
public:
	enum {// 镜盖指令
		MCC_MIN = -1,
		MCC_CLOSE,	///< 关闭镜盖
		MCC_OPEN,	///< 打开镜盖
		MCC_MAX
	};

public:
	static bool IsValid(int cmd) {
		return cmd > MCC_MIN && cmd < MCC_MAX;
	}

	static const char* ToString(int cmd) {
		return IsValid(cmd) ? mcc_desc[cmd] : NULL;
	}

	static int FromString(const char* name) {
		int cmd(MCC_MIN);

		if (name) {
			if (isdigit(name[0])) {
				cmd = atoi(name);
			}
			else {
				for (cmd = MCC_MIN + 1; cmd < MCC_MAX && strcmp(name, mcc_desc[cmd]); ++cmd);
			}
		}
		return IsValid(cmd) ? cmd : MCC_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* mc_desc[] = {
	"Error",
	"Opening",
	"Opened",
	"Closing",
	"Closed"
};

/*!
 * @class StateMirrorCover
 * @brief 定义: 镜盖状态
 */
class StateMirrorCover {
public:
	enum {///< 镜盖状态
		MC_MIN = -1,
		MC_ERROR,	///< 错误
		MC_OPENING,	///< 正在打开
		MC_OPEN,	///< 已打开
		MC_CLOSING,	///< 正在关闭
		MC_CLOSED,	///< 已关闭
		MC_MAX
	};

public:
	static bool IsValid(int state) {
		return state > MC_MIN && state < MC_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? mc_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(MC_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = MC_MIN + 1; state < MC_MAX && strcmp(name, mc_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : MC_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* focus_desc[] = {
	"Error",
	"Freeze",
	"Moving"
};

/*!
 * @class StateFocus
 * @brief 调焦器工作状态
 */
class StateFocus {
public:
	enum {///< 调焦器状态
		FOCUS_MIN = -1,
		FOCUS_ERROR,	///< 错误
		FOCUS_FREEZE,	///< 静止
		FOCUS_MOVING,	///< 定位
		FOCUS_MAX
	};

public:
	static bool IsValid(int state) {
		return state > FOCUS_MIN && state < FOCUS_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? focus_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(FOCUS_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = FOCUS_MIN + 1; state < FOCUS_MAX && strcmp(name, focus_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : FOCUS_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* imgtyp_desc[] = {
	"BIAS",
	"DARK"
	"FLAT",
	"OBJECT"
	"LIGHT",
	"FOCUS"
};

/*!
 * @class TypeImage 定义: 图像类型
 */
class TypeImage {
public:
	enum {///< 图像类型
		IMGTYP_MIN = -1,
		IMGTYP_BIAS,	///< 本底
		IMGTYP_DARK,	///< 暗场
		IMGTYP_FLAT,	///< 平场
		IMGTYP_OBJECT,	///< 目标
		IMGTYP_LIGHT,	///< 天光
		IMGTYP_FOCUS,	///< 调焦
		IMGTYP_MAX
	};

public:
	static bool IsValid(int type) {
		return type > IMGTYP_MIN && type < IMGTYP_MAX;
	}

	static const char* ToString(int type) {
		return IsValid(type) ? imgtyp_desc[type] : NULL;
	}

	static int FromString(const char* name) {
		int type(IMGTYP_MIN);

		if (name) {
			if (isdigit(name[0])) {
				type = atoi(name);
			}
			else {
				for (type = IMGTYP_MIN + 1; type < IMGTYP_MAX && strcmp(name, imgtyp_desc[type]); ++type);
			}
		}
		return IsValid(type) ? type : IMGTYP_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* exp_desc[] = {
	"init",
	"start",
	"stop",
	"pause",
	"resume"
};

/*!
 * @class CommandExpose
 * @brief 定义: 相机控制接口级的控制指令
 */
class CommandExpose {
public:
	enum {///< 相机控制指令
		EXP_MIN = -1,
		EXP_INIT,	///< 初始化
		EXP_START,	///< 开始曝光
		EXP_STOP,	///< 中止曝光
		EXP_PAUSE,	///< 暂停曝光
		EXP_RESUME,	///< EXPOSE_START分支: 当处理暂停过程中收到开始曝光指令, 指令记录为RESUME
		EXP_MAX
	};

public:
	static bool IsValid(int cmd) {
		return cmd > EXP_MIN && cmd < EXP_MAX;
	}

	static const char* ToString(int cmd) {
		return IsValid(cmd) ? exp_desc[cmd] : NULL;
	}

	static int FromString(const char* name) {
		int cmd(EXP_MIN);

		if (name) {
			if (isdigit(name[0])) {
				cmd = atoi(name);
			}
			else {
				for (cmd = EXP_MIN + 1; cmd < EXP_MAX && strcmp(name, exp_desc[cmd]); ++cmd);
			}
		}
		return IsValid(cmd) ? cmd : EXP_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* camctl_desc[] = {
	"Error",
	"Idle",
	"Exposing",
	"Image Ready",
	"Aborted",
	"Paused",
	"Waiting until Time",
	"Waiting for sync",
	"Waiting during FLAT"
};

/*!
 * @class StateCameraControl
 * @brief 定义: 相机控制接口工作状态
 */
class StateCameraControl {
public:
	enum {///< 相机调度状态
		CAMCTL_MIN = -1,
		CAMCTL_ERROR,			///< 错误
		CAMCTL_IDLE,			///< 空闲
		CAMCTL_EXPOSING,		///< 曝光过程中
		CAMCTL_IMAGEREADY,		///< 已完成曝光
		CAMCTL_ABORTED,			///< 已中止曝光
		CAMCTL_PAUSEED,			///< 已暂停曝光
		CAMCTL_WAITING_TIME,	///< 等待曝光流传起始时间到达
		CAMCTL_WAITING_SYNC,	///< 完成曝光, 等待其它相机完成曝光
		CAMCTL_WAITING_FLAT,	///< 平场间等待--等待转台重新指向
		CAMCTL_MAX
	};

public:
	static bool IsValid(int state) {
		return state > CAMCTL_MIN && state < CAMCTL_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? camctl_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(CAMCTL_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = CAMCTL_MIN + 1; state < CAMCTL_MAX && strcmp(name, camctl_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : CAMCTL_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char *obsplan_desc[] = {
	"error",
	"cataloged",
	"interrupted",
	"waiting",
	"running",
	"over",
	"abandoned",
	"deleted"
};

/*!
 * class StateObservationPlan
 * @brief 定义: 观测计划工作状态
 */
class StateObservationPlan {
public:
	enum {///< 观测计划状态
		OBSPLAN_MIN = -1,
		OBSPLAN_ERROR,		///< 错误. 特指不存在plan_sn指向的计划
		OBSPLAN_CATALOGED,	///< 入库
		OBSPLAN_INTERRUPTED,///< 中断
		OBSPLAN_WAITING,	///< 等待执行
		OBSPLAN_RUNNING,	///< 执行中
		OBSPLAN_OVER,		///< 完成
		OBSPLAN_ABANDONED,	///< 自动抛弃
		OBSPLAN_DELETED,	///< 手动删除
		OBSPLAN_MAX
	};

public:
	static bool IsValid(int state) {
		return state > OBSPLAN_MIN && state < OBSPLAN_MAX;
	}

	static const char* ToString(int state) {
		return IsValid(state) ? obsplan_desc[state] : NULL;
	}

	static int FromString(const char* name) {
		int state(OBSPLAN_MIN);

		if (name) {
			if (isdigit(name[0])) {
				state = atoi(name);
			}
			else {
				for (state = OBSPLAN_MIN + 1; state < OBSPLAN_MAX && strcmp(name, obsplan_desc[state]); ++state);
			}
		}
		return IsValid(state) ? state : OBSPLAN_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* odt_desc[] = {
	"daytime",
	"flat",
	"night"
};

/*!
 * @class TypeObservationDuration
 * @brief 定义: 观测时段类型
 * @note
 * 将每天分为三个时段: 白天; 夜晚; 平场时间(晨昏)
 */
class TypeObservationDuration {
public:
	enum {///< 观测时间分类
		ODT_MIN = -1,
		ODT_DAYTIME,///< 白天, 可执行BIAS\DARK\FOCUS计划
		ODT_FLAT,	///< 平场, 可执行平场计划
		ODT_NIGHT,	///< 夜间, 可执行非平场计划
		ODT_MAX
	};

public:
	static bool IsValid(int type) {
		return type > ODT_MIN && type < ODT_MAX;
	}

	static const char* ToString(int type) {
		return IsValid(type) ? odt_desc[type] : NULL;
	}

	static int FromString(const char* name) {
		int type(ODT_MIN);

		if (name) {
			if (isdigit(name[0])) {
				type = atoi(name);
			}
			else {
				for (type = ODT_MIN + 1; type < ODT_MAX && strcmp(name, odt_desc[type]); ++type);
			}
		}
		return IsValid(type) ? type : ODT_MIN;
	}
};

/////////////////////////////////////////////////////////////////////////////
static const char* opobs_desc[] = {
	"mount",
	"camera",
	"mount-annex",
	"camera-annex",
	"environment"
};

/*!
 * @class ObservationOperator
 * @brief 定义: 天文观测指令执行对象
 */
class ObservationOperator {
public:
	enum {///< 定义执行机构
		OPOBS_MIN = -1,
		OPOBS_MOUNT,		///< 转台
		OPOBS_CAMERA,		///< 相机
		OPOBS_MOUNTANNEX,	///< 转台附属
		OPOBS_CAMERAANNEX,	///< 相机附属
		OPOBS_ENVIRONMENT,	///< 环境监测
		OPOBS_MAX
	};

public:
	static bool IsValid(int type) {
		return type > OPOBS_MIN && type < OPOBS_MAX;
	}

	static const char* ToString(int type) {
		return IsValid(type) ? opobs_desc[type] : NULL;
	}

	static int FromString(const char* name) {
		int type(OPOBS_MIN);
		if (name) {
			if (isdigit(name[0])) {
				type = atoi(name);
			}
			else {
				for (type = OPOBS_MIN + 1; type < OPOBS_MAX && strcmp(name, opobs_desc[type]); ++type);
			}
		}
		return IsValid(type) ? type : OPOBS_MIN;
	}
};

#endif
