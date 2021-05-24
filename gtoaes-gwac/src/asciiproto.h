/*
 * @file asciiproto.h 封装与控制台、数据库和相机相关的通信协议
 * @author         卢晓猛
 * @version        2.0
 * @date           2017年2月16日
 * ============================================================================
 * @date 2017年5月16日
 * @note
 * - 图像类型定义迁移至本文件: 在网络接口层约定采用相同数值
 * - 曝光指令定义迁移至本文件: 在网络接口层约定采用相同数值
 * - 减弱take_image功能, 该指令仅对应控制台或数据库直接触发的曝光指令, 不再用于向相机发送指令
 * - 向相机发送曝光指令的take_image, 拆分为两条协议: object_info和expose, 在类定义上为:
 *   ascproto_object_info和ascproto_expose. object_info通知观测参数及目标信息, 当其中
 *   包含滤光片时, 可提前切换滤光片; expose触发曝光指令
 * @date 2017年6月6日
 * - buff_在ObservationSystem中, 被可能并发的多线程访问, 缺少同步机制. 为减少内存分配次数,
 *   采用长度为(10, 1500)的二维数组管理buff_
 */

#ifndef ASCIIPROTO_H_
#define ASCIIPROTO_H_

#include <limits.h>
#include <string>
#include <list>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>

enum IMAGE_TYPE {// 曝光类型
	IMGTYPE_BIAS = 1,	// 本底
	IMGTYPE_DARK,	// 暗场
	IMGTYPE_FLAT,	// 平场
	IMGTYPE_OBJECT,	// 目标
	IMGTYPE_FOCUS,	// 调焦
	IMGTYPE_LAST	// 占位, 无效类型
};

enum EXPOSE_COMMAND {// 曝光指令
	EXPOSE_START = 1,	//< 开始曝光
	EXPOSE_STOP,		//< 中止曝光
	EXPOSE_PAUSE,		//< 暂停曝光
	EXPOSE_RESUME,		//< EXPOSE_START分支: 当处理暂停过程中收到开始曝光指令, 指令记录为RESUME
	EXPOSE_LAST			//< 占位, 无效指令
};

/*!
 * @brief 通信协议基类, 包含组标志和单元标志
 */
struct ascproto_base {
	std::string group_id;	//< 组标志
	std::string unit_id;	//< 单元标志

public:
	ascproto_base() {
		group_id = "";
		unit_id  = "";
	}

	virtual ~ascproto_base() {
	}

	virtual void reset() {// 虚函数, 重置成员变量
		group_id = "";
		unit_id  = "";
	}
};

/*!
 * @brief 中止转台指向过程
 */
struct ascproto_abort_slew : public ascproto_base {
};

/*!
 * @brief 专用于GWAC系统的观测计划
 */
struct ascproto_append_gwac : public ascproto_base {
	int    op_sn;			//< 计划序列号
	std::string op_time;	//< 观测计划生成时间
	std::string op_type;	//< 观测计划类型
	std::string obstype;	//< 观测类型
	std::string grid_id;	//< 天区划分模式
	std::string field_id;	//< 天区标志
	std::string obj_id;		//< 目标名
	double ra;				//< 指向中心赤经, 量纲: 角度
	double dec;				//< 指向中心赤纬, 量纲: 角度
	double epoch;			//< 指向中心位置的坐标系, 历元
	double objra;			//< 目标赤经, 量纲: 角度
	double objdec;			//< 目标赤纬, 量纲: 角度
	double objepoch;		//< 目标位置的坐标系, 历元
	std::string objerror;	//< 目标位置误差
	std::string imgtype;	//< 图像类型, 字符串
	IMAGE_TYPE  iimgtype;	//< 图像类型, 整数
	int flatmode;			//< 平场模式. 1: Sky; 2: Dome
	double expdur;			//< 曝光时间, 量纲: 秒
	double delay;			//< 延迟时间, 量纲: 秒
	int    frmcnt;			//< 总帧数
	int    priority;		//< 优先级
	std::string begin_time;	//< 观测起始时间
	std::string end_time;	//< 观测结束时间
	int    pair_id;			//< 分组标志

public:
	ascproto_append_gwac() {
		op_sn   = -1;
		op_time = "";
		op_type = "";
		obstype = "";
		grid_id = "";
		field_id = "";
		obj_id   = "";
		ra = dec = objra = objdec = -1000.0;
		epoch = objepoch = 2000.0;
		objerror = "";
		imgtype = "";
		iimgtype= IMGTYPE_LAST;
		flatmode = 1;
		expdur = delay = -1.0;
		frmcnt = 0;
		priority = 0;
		begin_time = end_time = "";
		pair_id = -1;
	}
};

/*!
 * @brief 导星. 微调转台指向位置
 * @date Apr 14, 2017
 * 导星指令有两种工作模式:
 * 模式一:
 * objra和objdec为无效值, ra和dec为导星量
 * 模式二：
 * objra和objdec及ra和dec为有效值时，偏差量=目标坐标-定位坐标.
 * 若偏差量大于阈值, 则执行零点同步; 小于阈值时, 执行导星
 * 阈值定为: 2度
 */
struct ascproto_guide : public ascproto_base {
	/* 当(ra,dec),(objra,objdec)同时有效即代表坐标时, 其坐标系为J2000.0 */
	double ra;		//< 定位赤经或偏差量, 量纲: 角度
	double dec;		//< 定位赤纬或偏差量, 量纲: 角度
	double objra;	//< 目标赤经, 量纲: 角度
	double objdec;	//< 目标赤纬, 量纲: 角度

public:
	ascproto_guide() {
		ra = dec = -1000.0;
		objra = objdec = -1000.0;
	}
};

/*!
 * @brief 调焦. 通知转台控制程序改变焦点位置
 */
struct ascproto_focus : public ascproto_base {
	std::string camera_id;	//< 相机全局标志
	int value;

public:
	ascproto_focus() {
		camera_id = "";
		value = 0;
	}
};

/*!
 * @brief 调焦. 通知转台控制程序星像统计FWHM
 */
struct ascproto_fwhm : public ascproto_base {
	std::string camera_id;	//< 相机全局标志
	double value;

public:
	ascproto_fwhm() {
		camera_id = "";
		value = 0.0;
	}
};

/*!
 * @brief 启动GWAC自动工作流程
 */
struct ascproto_start_gwac : public ascproto_base {
};

/*!
 * @brief 停止GWAC自动工作流程
 */
struct ascproto_stop_gwac : public ascproto_base {
};

/*!
 * @brief 启用设备
 */
struct ascproto_enable : public ascproto_base {
	std::string camera_id;

public:
	ascproto_enable() {
		camera_id = "";
	}
};

/*!
 * @brief 禁用设备
 */
struct ascproto_disable : public ascproto_base {
	std::string camera_id;

public:
	ascproto_disable() {
		camera_id = "";
	}
};

/*!
 * @brief 搜索零点
 */
struct ascproto_find_home : public ascproto_base {
};

/*!
 * @brief 同步零点
 */
struct ascproto_home_sync : public ascproto_base {
	double ra;		//< 转台当前指向位置对应的赤经坐标, 量纲: 角度
	double dec;		//< 转台当前指向位置对应的赤纬坐标, 量纲: 角度
	double epoch;	//< 转台当前指向位置对应的天球坐标所在的坐标系, 量纲: 年

public:
	ascproto_home_sync() {
		ra = dec = -1000.0;
		epoch = 2000.0;
	}
};

/*!
 * @brief 指向
 */
struct ascproto_slewto : public ascproto_base {
	double ra;		//< 目标的赤经坐标, 量纲: 角度
	double ha;		//< 目标的时角坐标, 量纲: 角度
	double dec;		//< 目标的的赤纬坐标, 量纲: 角度
	double epoch;	//< 目标坐标所在的坐标系, 量纲: 年

public:
	ascproto_slewto() {
		ra = ha = dec = -1000.0;
		epoch = 2000.0;
	}
};

/*!
 * @brief 复位
 */
struct ascproto_park : public ascproto_base {
};

/*!
 * @brief 开关镜盖
 */
struct ascproto_mcover : public ascproto_base {
	std::string camera_id;	//< 相机标志
	int command;		//< 0: 关闭; 1: 打开; -1: 未定义

public:
	ascproto_mcover() {
		camera_id = "";
		command   = -1;
	}
};

/*!
 * @brief 中止曝光
 */
struct ascproto_abort_image : public ascproto_base {
	std::string camera_id;	//< 相机标志

public:
	ascproto_abort_image() {
		camera_id = "";
	}
};

/*
 * 开始曝光
 * @li 相机共享该协议
 * @li 相机收到的协议中若包含曝光时间则按照begin_time和end_time开始曝光
 * @li 相机收到的协议中若不包含曝光时间, 则按照其它参数调整滤光片或准备图像文件
 * @li 协议中的无效信息不会改变当前存储的有效信息
 * @date 2017年5月16日
 * - 相机不共享该协议
 * - 取消无关项
 * - 取消无关定义
 */
struct ascproto_take_image : public ascproto_base {
	std::string camera_id;	//< 相机标志
	std::string obj_id;		//< 目标名
	std::string imgtype;	//< 图像类型, 字符串
	IMAGE_TYPE  iimgtype;	//< 图像类型, 整数
	int flatmode;			//< 平场模式. 1: Sky; 2: Dome
	double expdur;			//< 曝光时间, 量纲: 秒
	double delay;			//< 延迟时间, 量纲: 秒
	int    frmcnt;			//< 总帧数
	std::string filter;		//< 滤光片

public:
	ascproto_take_image() {
		obj_id = "";
		imgtype = "";
		iimgtype= IMGTYPE_LAST;
		flatmode= 1;
		expdur = delay = -1.0;
		frmcnt = 0;
		filter  = "";
	}
};

/*!
 * @breif 通知相机观测目标信息和观测需求
 */
struct ascproto_object_info : public ascproto_base {
	/* 兼容GWAC参数项 */
	int    op_sn;			//< 计划序列号
	std::string op_time;	//< 观测计划生成时间
	std::string op_type;	//< 观测计划类型
	std::string obstype;	//< 观测类型
	std::string grid_id;	//< 天区划分模式
	std::string field_id;	//< 天区标志
	std::string obj_id;		//< 目标名
	double ra;				//< 指向中心赤经, 量纲: 角度
	double dec;				//< 指向中心赤纬, 量纲: 角度
	double epoch;			//< 指向中心位置的坐标系, 历元
	double objra;			//< 目标赤经, 量纲: 角度
	double objdec;			//< 目标赤纬, 量纲: 角度
	double objepoch;		//< 目标位置的坐标系, 历元
	std::string objerror;	//< 目标位置误差
	std::string imgtype;	//< 图像类型, 字符串
	IMAGE_TYPE  iimgtype;	//< 图像类型, 整数
	int flatmode;			//< 平场模式. 1: Sky; 2: Dome
	double expdur;			//< 曝光时间, 量纲: 秒
	double delay;			//< 延迟时间, 量纲: 秒
	int    frmcnt;			//< 总帧数
	int    priority;		//< 优先级, 默认最高优先级, 必须明确中断
	std::string begin_time;	//< 观测起始时间
	std::string end_time;	//< 观测结束时间
	int    pair_id;			//< 分组标志
	/* 兼容通用望远镜 */
	std::string filter;		//< 滤光片

public:
	ascproto_object_info() {
		reset();
	}

	void reset() {
		op_sn = -1;
		ra = dec = objra = objdec = -1000.0;
		epoch = objepoch = 2000.0;
		flatmode = 1;
		expdur = delay = -1.0;
		frmcnt = -1;
		priority = 0;
		pair_id = -1;
		op_time = op_type = obstype = grid_id = field_id = obj_id = "";
		objerror = imgtype = begin_time = end_time = filter = "";
		iimgtype = IMGTYPE_LAST;
	}

	ascproto_object_info &operator=(const ascproto_object_info &other) {
		if (this != &other) {
			op_sn		= other.op_sn;
			op_time		= other.op_time;
			op_type		= other.op_type;
			obstype		= other.obstype;
			grid_id		= other.grid_id;
			field_id	= other.field_id;
			obj_id		= other.obj_id;
			ra			= other.ra;
			dec			= other.dec;
			objra		= other.objra;
			objdec		= other.objdec;
			epoch		= other.epoch;
			objepoch	= other.objepoch;
			objerror	= other.objerror;
			imgtype		= other.imgtype;
			flatmode	= other.flatmode;
			expdur		= other.expdur;
			delay		= other.delay;
			frmcnt		= other.frmcnt;
			priority	= other.priority;
			begin_time	= other.begin_time;
			end_time	= other.end_time;
			pair_id		= other.pair_id;
			filter		= other.filter;
			iimgtype	= other.iimgtype;
		}

		return *this;
	}

	/*!
	 * @brief 重载赋值, 输入参数: 观测计]划
	 * @param plan 观测计划
	 * @return
	 * 为各成员赋值
	 */
	ascproto_object_info &operator=(const ascproto_append_gwac &plan) {
		op_sn		= plan.op_sn;
		op_time		= plan.op_time;
		op_type		= plan.op_type;
		obstype		= plan.obstype;
		grid_id		= plan.grid_id;
		field_id	= plan.field_id;
		obj_id		= plan.obj_id;
		ra			= plan.ra;
		dec			= plan.dec;
		objra		= plan.objra;
		objdec		= plan.objdec;
		epoch		= plan.epoch;
		objepoch	= plan.objepoch;
		objerror	= plan.objerror;
		imgtype		= plan.imgtype;
		flatmode	= plan.flatmode;
		expdur		= plan.expdur;
		delay		= plan.delay;
		frmcnt		= plan.frmcnt;
		priority	= plan.priority;
		begin_time	= plan.begin_time;
		end_time	= plan.end_time;
		pair_id		= plan.pair_id;
		filter = "";
		iimgtype	= plan.iimgtype;

		return *this;
	}

	/*!
	 * @brief 重载赋值, 输入参数: 观测计]划
	 * @param plan 观测计划
	 * @return
	 * 为各成员赋值
	 */
	ascproto_object_info &operator=(const ascproto_take_image &proto) {
		op_sn		= -1;
		op_time		= "";
		op_type		= "";
		obstype		= "";
		grid_id		= "";
		field_id	= "";
		obj_id		= proto.obj_id;
		ra			= -1000.0;
		dec			= -1000.0;
		objra		= -1000.0;
		objdec		= -1000.0;
		epoch		= 2000.0;
		objepoch	= 2000.0;
		objerror	= "";
		imgtype		= proto.imgtype;
		flatmode	= proto.flatmode;
		expdur		= proto.expdur;
		delay		= proto.delay;
		frmcnt		= proto.frmcnt;
		priority	= 0;
		begin_time	= "";
		end_time	= "";
		pair_id		= -1;
		filter 		= proto.filter;
		iimgtype	= proto.iimgtype;

		return *this;
	}
};

/*!
 * @brief 通知相机曝光指令
 */
struct ascproto_expose : public ascproto_base {
	EXPOSE_COMMAND command;	//< 曝光指令, 有效范围: EXPOSE_COMMAND
};

/*!
 * 相机工作状态
 * @li 相机发出的协议
 * @li 控制台和数据库共享该协议
 * @li 相机共享该协议
 */
struct ascproto_camera_info : public ascproto_base {
	std::string camera_id;	//< 相机全局标志
	int bitdepth;		//< A/D位数
	int readport;		//< 读出端口档位
	int readrate;		//< 读出速度档位
	int vrate;			//< 垂直转移速度档位
	int gain;			//< 增益档位
	bool emsupport;		//< 是否支持EM模式
	bool emon;			//< 启用EM模式
	int emgain;			//< EM增益
	int coolset;		//< 制冷温度, 量纲: 摄氏度
	int coolget;		//< 芯片温度, 量纲: 摄氏度
	std::string utc;	//< 时标
	int state;			//< 工作状态. 0: 错误; 1: 空闲; 2: 曝光中; 3: 等待中; 4: 暂停
	int freedisk;		//< 空闲磁盘空间, 量纲: MB
	std::string filepath;//< 文件存储路径
	std::string filename;//< 文件名称
	std::string objname;//< 当前观测目标名称
	double expdur;		//< 当前曝光时间
	std::string filter;	//< 滤光片
	int frmtot;			//< 曝光总帧数
	int frmnum;			//< 曝光完成帧数

public:
	void reset() {// 重置为无效值
		camera_id = "";
		bitdepth = readport = readrate = vrate = gain = -1;
		emsupport = emon = false;
		emgain = -1;
		coolset = 100;
		coolget = 100;
		utc = "";
		freedisk = -1;
		state = -1;
		filepath = filename = "";
		objname = "";
		expdur = -1.0;
		frmtot = frmnum = -1;
		ascproto_base::reset();
	}
};

typedef boost::shared_ptr<ascproto_base> apbase;

class ascii_proto {
protected:
	/* 声明数据类型 */
	typedef std::list<std::string> listring;
	typedef boost::unique_lock<boost::mutex> mutex_lock;

public:
	ascii_proto();
	virtual ~ascii_proto();

public:
	// 构造发送给设备的通信协议
	/*!
	 * @brief 封装abort_image协议
	 * @param n 封装后协议长度, 量纲: 字节
	 * @return
	 * 协议首地址
	 */
	const char* compact_abort_image(int& n);
	/*!
	 * @brief 焦点位置发生变化
	 * @param value 焦点特征值
	 * @param n     封装后协议长度, 量纲: 字节
	 * @return
	 */
	const char* compact_focus(const int value, int& n);
	/*!
	 * @brief 封装camera_info协议
	 * @param proto 输入参数, 依据其中有效信息封装协议
	 * @param n     封装后协议长度, 量纲: 字节
	 * @return
	 * 协议首地址
	 */
	const char* compact_camera_info(const ascproto_camera_info* proto, int& n);
	/*!
	 * @brief 封装object_info协议
	 * @param proto 输入参数, 依据其中有效信息封装协议
	 * @param n     封装后协议长度, 量纲: 字节
	 * @return
	 */
	const char* compact_object_info(const ascproto_object_info* proto, int& n);
	/*!
	 * @brief 封装expose协议
	 * @param cmd 曝光指令
	 * @param n   封装后协议长度, 量纲: 字节
	 * @return
	 */
	const char* compact_expose(const EXPOSE_COMMAND cmd, int& n);
	/*!
	 * @brief 解析通信协议
	 * @param rcvd   从网络中收到的信息
	 * @param type   协议类型
	 * @return
	 * 解析后的信息主体, 转换为ascproto_base类型
	 */
	apbase resolve(const char* rcvd, std::string& type);

protected:
	/*!
	 * @brief 获得可写入数据的缓冲区地址
	 * @return
	 * 缓冲区首地址
	 */
	char *get_buffptr();
	/*!
	 * @brief 以等号分隔关键字-数值
	 * @param keyval   键-值对
	 * @param keyword  关键字
	 * @param value    数值
	 */
	void resolve_kv(std::string* kv, std::string &keyword, std::string &value);
	/*!
	 * @brief 解析abort_slew协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 * @note
	 * 当未提供unit_id或group_id时，终止所有相关转台指向
	 */
	apbase resolve_abort_slew(listring& tokens);
	/*!
	 * @brief 解析append_gwac协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_append_gwac(listring& tokens);
	/*!
	 * @brief 解析guide协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 * @note
	 * 本协议复用:
	 * @li 控制台或数据库发送给总控, 用于修正转台指向
	 * @li 总控发送给相机, 用于暂停或恢复曝光
	 */
	apbase resolve_guide(listring& tokens);
	/*!
	 * @brief 解析focus协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_focus(listring& tokens);
	/*!
	 * @brief 解析fwhm协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_fwhm(listring& tokens);
	/*!
	 * @brief 解析start_gwac协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_start_gwac(listring& tokens);
	/*!
	 * @brief 解析stop_gwac协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_stop_gwac(listring& tokens);
	/*!
	 * @brief 解析enable协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_enable(listring& tokens);
	/*!
	 * @brief 解析disable协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_disable(listring& tokens);
	/*!
	 * @brief 解析find_home协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_find_home(listring& tokens);
	/*!
	 * @brief 解析home_sync协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_home_sync(listring& tokens);
	/*!
	 * @brief 解析slewto协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_slewto(listring& tokens);
	/*!
	 * @brief 解析park协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_park(listring& tokens);
	/*!
	 * @brief 解析mcover协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_mcover(listring& tokens);
	/*!
	 * @brief 解析abort_image协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_abort_image(listring& tokens);
	/*!
	 * @brief 解析take_image协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_take_image(listring& tokens);
	/*!
	 * @brief 解析camera_info协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_camera_info(listring& tokens);
	/*!
	 * @brief 解析object_info协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_object_info(listring& tokens);
	/*!
	 * @brief 解析expose协议
	 * @param tokens 协议主体
	 * @return
	 * 解析成功标志
	 */
	apbase resolve_expose(listring& tokens);

protected:
	// 成员变量
	boost::mutex mtxbuff_;	//< 存储区互斥锁
	int ibuff_; // 存储区索引. 缓冲区采用一维长度为10*1024=10240字节数组, 通过软件分为10份循环使用
	boost::shared_array<char> buff_;	//< 通信协议存储区, 用于格式化输出通信协议
	boost::shared_ptr<ascproto_camera_info> proto_camnf_;		//< camera_info
};

typedef boost::shared_ptr<ascii_proto> ascptr;
/*!
 * @brief 检测赤经的有效性
 * @param ra 赤经, 量纲: 角度. 有效范围: [0, 360)
 * @return
 * 赤经有效性
 */
extern bool valid_ra(double ra);
/*!
 * @brief 检测赤纬的有效性
 * @param dc 赤纬, 量纲: 角度. 有效范围: [-90, +90]
 * @return
 * 赤纬有效性
 */
extern bool valid_dc(double dc);

#endif /* ASCIIPROTO_H_ */
