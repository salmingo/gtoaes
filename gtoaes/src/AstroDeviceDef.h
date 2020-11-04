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

#include <string.h>

/* 状态与指令 */
/*!
 * @class StateMount 定义: 转台状态
 */
class StateMount {
protected:
	static const char* desc[];

public:
	enum {// 转台状态
		ERROR,		//< 错误
		FREEZE,		//< 静止
		HOMING,		//< 找零
		HOMED,		//< 找到零点
		PARKING,	//< 复位
		PARKED,		//< 已复位
		SLEWING,	//< 指向
		TRACKING	//< 跟踪
	};

public:
	static const char* ToString(int state) {
		if (state < ERROR || state > TRACKING)
			state = TRACKING + 1;
		return desc[state];
	}
};
const char* StateMount::desc[] = {
	"Error",
	"Freeze",
	"Homing",
	"Homed",
	"Parking",
	"Parked",
	"Slewing",
	"Tracking",
	"Unknown"
};

/*!
 * @class CommandMirrorCover 定义: 镜盖指令
 */
class CommandMirrorCover {
protected:
	static const char* desc[];

public:
	enum {// 镜盖指令
		CLOSE,	//< 关闭镜盖
		OPEN	//< 打开镜盖
	};

public:
	static const char* ToString(int cmd) {
		if (cmd < CLOSE || cmd > OPEN)
			cmd = OPEN + 1;
		return desc[cmd];
	}
};
const char* CommandMirrorCover::desc[] = {
	"close",
	"open",
	"unknown"
};

typedef CommandMirrorCover CommandMC;

/*!
 * @class StateMirrorCover 定义: 镜盖状态
 */
class StateMirrorCover {
protected:
	static const char* desc[];

public:
	enum {// 镜盖状态
		ERROR,		// 错误
		OPENING,	// 正在打开
		OPEN,		// 已打开
		CLOSING,	// 正在关闭
		CLOSED		// 已关闭
	};

public:
	static const char* ToString(int state) {
		if (state < ERROR || state > CLOSED)
			state = CLOSED + 1;
		return desc[state];
	}
};
const char* StateMirrorCover::desc[] = {
	"Error",
	"Opening",
	"Opened",
	"Closing",
	"Closed",
	"Unknown"
};
typedef StateMirrorCover StateMC;

/*!
 * @class StateFocus 调焦器工作状态
 */
class StateFocus {
protected:
	static const char* desc[];

public:
	enum {// 调焦器状态
		ERROR,	//< 错误
		FREEZE,	//< 静止
		MOVING	//< 定位
	};

public:
	static const char* ToString(int state) {
		if (state < ERROR || state > MOVING)
			state = MOVING + 1;
		return desc[state];
	}
};
const char* StateFocus::desc[] = {
	"Error",
	"Freeze",
	"Moving"
	"Unknown"
};

/*!
 * @class TypeImage 定义: 图像类型
 */
class TypeImage {
protected:
	static const char* desc[];

public:
	enum {// 图像类型
		ERROR,	// 错误
		BIAS,	// 本底
		DARK,	// 暗场
		FLAT,	// 平场
		OBJECT,	// 目标
		LIGHT,	// 天光
		FOCUS	// 调焦
	};

public:
	static const char* ToString(int type) {
		if (type < BIAS || type > FOCUS)
			type = 0;
		return desc[type];
	}

	static int FromString(const char* imgtype) {
		int type;
		for (type = BIAS; type <= FOCUS && strcmp(desc[type], imgtype); ++type);
		return type > FOCUS ? ERROR : type;
	}
};
const char* TypeImage::desc[] = {
	"ERROR",
	"BIAS",
	"DARK"
	"FLAT",
	"OBJECT"
	"LIGHT",
	"FOCUS"
};

/*!
 * @class CommandExpose 定义: 相机控制接口级的控制指令
 */
class CommandExpose {
protected:
	static const char* desc[];

public:
	enum {// 相机控制指令
		ERROR,	//< 错误
		INIT,	//< 初始化
		START,	//< 开始曝光
		STOP,	//< 中止曝光
		PAUSE,	//< 暂停曝光
		RESUME	//< EXPOSE_START分支: 当处理暂停过程中收到开始曝光指令, 指令记录为RESUME
	};

public:
	static const char* ToString(int cmd) {
		if (cmd < INIT || cmd > RESUME)
			cmd = ERROR;
		return desc[cmd];
	}
};
const char* CommandExpose::desc[] = {
	"unknown",
	"init",
	"start",
	"stop",
	"pause",
	"resume"
};
typedef CommandExpose CommandExp;

/*!
 * @class StateCameraControl 定义: 相机控制接口工作状态
 */
class StateCameraControl {
protected:
	static const char* desc[];

public:
	enum {// 相机工作状态
		ERROR,			// 错误
		IDLE,			// 空闲
		EXPOSING,		// 曝光过程中
		IMAGEREADY,		// 已完成曝光
		ABORTED,		// 已中止曝光
		PAUSEED,		// 已暂停曝光
		WAITING_TIME,	// 等待曝光流传起始时间到达
		WAITING_SYNC,	// 完成曝光, 等待其它相机完成曝光
		WAITING_FLAT	// 平场间等待--等待转台重新指向
	};

public:
	static const char* ToString(int state) {
		if (state < IDLE || state > WAITING_FLAT)
			state = ERROR;
		return desc[state];
	}
};
const char* StateCameraControl::desc[] = {
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
typedef StateCameraControl StateCamCtl;	// 类名称缩写

/*!
 * class StateObservationPlan 定义: 观测计划工作状态
 */
class StateObservationPlan {
protected:
	static const char *desc[];

public:
	enum {// 观测计划状态
		ERROR,		// 错误
		CATALOGED,	// 入库
		INTERRUPTED,// 中断
		WAITING,	// 等待执行
		RUNNING,	// 执行中
		OVER,		// 完成
		ABANDONED,	// 自动抛弃
		DELETED		// 手动删除
	};

public:
	static const char* ToString(int state) {
		if (state < CATALOGED || state > DELETED)
			state = ERROR;
		return desc[state];
	}
};
const char *StateObservationPlan::desc[] = {
	"error",
	"cataloged",
	"interrupted",
	"waiting",
	"running",
	"over",
	"abandoned",
	"deleted"
};
typedef StateObservationPlan StateObsPlan;	// 类名称缩写

/*!
 * @class TypeObservationDuration 定义: 观测时段类型
 * @note
 * 将每天分为三个时段: 白天; 夜晚; 平场时间(晨昏)
 */
class TypeObservationDuration {
protected:
	static const char* desc[];

public:
	enum {// 观测时间分类
		ERROR,
		DAYTIME,//< 白天, 可执行BIAS\DARK\FOCUS计划
		FLAT,	//< 平场, 可执行平场计划
		NIGHT	//< 夜间, 可执行非平场计划
	};

public:
	static const char* ToString(int type) {
		if (type < DAYTIME || type > NIGHT)
			type = ERROR;
		return desc[type];
	}

	static int FromString(const char* name) {
		int type;
		for (type = DAYTIME; type <= NIGHT && strcmp(name, desc[type]); ++type);
		return type > NIGHT ? ERROR : type;
	}
};
const char* TypeObservationDuration::desc[] = {
	"unknown",
	"daytime",
	"flat",
	"night"
};
typedef TypeObservationDuration TypeOD;	///< 类名称缩写

/*!
 * @class AutoFocusOperator
 * @brief 定义: 自动调焦执行机构
 */
class AutoFocusOperator {
protected:
	static const char* desc[];

public:
	enum {///< 定义执行机构
		ERROR,
		MOUNT,			///< 转台
		CAMERA,			///< 相机
		MOUNTANNEX,		///< 转台附属
		CAMERAANNEX		///< 相机附属
	};

public:
	static const char* ToString(int type) {
		if (type < MOUNT || type > CAMERAANNEX)
			type = ERROR;
		return desc[type];
	}

	static int FromString(const char* name) {
		int type;
		for (type = MOUNT; type <= CAMERAANNEX && strcmp(name, desc[type]); ++type);
		return type > CAMERAANNEX ? ERROR : type;
	}
};
const char* AutoFocusOperator::desc[] = {
	"unknown",
	"mount",
	"camera",
	"mount-annex",
	"camera-annex"
};
typedef AutoFocusOperator AutoFcsOp;	///< 类名称缩写

#endif
