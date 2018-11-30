/*!
 * @file AsciiProtocol.h 声明文件, 声明GWAC/GFT系统中字符串型通信协议
 * @version 0.1
 * @date 2017-11-17
 * - 通信协议采用Struct声明
 * - 通信协议继承自ascii_protocol_base
 */

#ifndef ASCIIPROTOCOL_H_
#define ASCIIPROTOCOL_H_

#include <list>
#include <vector>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include "AsciiProtocolBase.h"
#include "AstroDeviceState.h"

using std::list;
using std::vector;

typedef list<string> listring;	//< string列表

//////////////////////////////////////////////////////////////////////////////
struct pair_key_val {// 关键字-键值对
	string keyword;
	string value;
};
typedef list<pair_key_val> likv;	//< pair_key_val列表

/* 宏定义: 通信协议类型 */
#define APTYPE_REG		"register"
#define APTYPE_UNREG	"unregister"
#define APTYPE_START	"start"
#define APTYPE_STOP		"stop"
#define APTYPE_ENABLE	"enable"
#define APTYPE_DISABLE	"disable"
#define APTYPE_RELOAD	"reload"
#define APTYPE_REBOOT	"reboot"
#define APTYPE_APPGWAC	"append_gwac"
#define APTYPE_APPPLAN	"append_plan"
#define APTYPE_ABTPLAN	"abort_plan"
#define APTYPE_CHKPLAN	"check_plan"
#define APTYPE_PLAN		"plan"
#define APTYPE_FINDHOME	"find_home"
#define APTYPE_HOMESYNC	"home_sync"
#define APTYPE_SLEWTO	"slewto"
#define APTYPE_PARK		"park"
#define APTYPE_GUIDE	"guide"
#define APTYPE_ABTSLEW	"abort_slew"
#define APTYPE_TELE		"telescope"
#define APTYPE_FWHM		"fwhm"
#define APTYPE_FOCUS	"focus"
#define APTYPE_MCOVER	"mcover"
#define APTYPE_TAKIMG	"take_image"
#define APTYPE_ABTIMG	"abort_image"
#define APTYPE_OBJECT	"object"
#define APTYPE_EXPOSE	"expose"
#define APTYPE_CAMERA	"camera"
#define APTYPE_COOLER	"cooler"
#define APTYPE_VACUUM	"vacuum"
#define APTYPE_OBSS		"obss"
#define APTYPE_FILEINFO	"fileinfo"
#define APTYPE_FILESTAT	"filestat"

/*--------------------------------- 声明通信协议 ---------------------------------*/
struct ascii_proto_reg : public ascii_proto_base {// 注册设备/用户
	int ostype;	//< 观测系统类型. 通知相机观测系统类型, 区别创建目录及文件名和文件头

public:
	ascii_proto_reg() {
		type = APTYPE_REG;
		ostype = INT_MIN;
	}
};
typedef boost::shared_ptr<ascii_proto_reg> apreg;

struct ascii_proto_unreg : public ascii_proto_base {// 注销设备/用户
public:
	ascii_proto_unreg() {
		type = APTYPE_UNREG;
	}
};
typedef boost::shared_ptr<ascii_proto_unreg> apunreg;

struct ascii_proto_start : public ascii_proto_base {// 启动开机流程
public:
	ascii_proto_start() {
		type = APTYPE_START;
	}
};
typedef boost::shared_ptr<ascii_proto_start> apstart;

struct ascii_proto_stop : public ascii_proto_base {// 启动关机流程
public:
	ascii_proto_stop() {
		type = APTYPE_STOP;
	}
};
typedef boost::shared_ptr<ascii_proto_stop> apstop;

struct ascii_proto_enable : public ascii_proto_base {// 启用设备
public:
	ascii_proto_enable() {
		type = APTYPE_ENABLE;
	}
};
typedef boost::shared_ptr<ascii_proto_enable> apenable;

struct ascii_proto_disable : public ascii_proto_base {// 禁用设备
public:
	ascii_proto_disable() {
		type = APTYPE_DISABLE;
	}
};
typedef boost::shared_ptr<ascii_proto_disable> apdisable;

struct ascii_proto_reload : public ascii_proto_base {// 重新加载参数
public:
	ascii_proto_reload() {
		type = APTYPE_RELOAD;
	}
};
typedef boost::shared_ptr<ascii_proto_reload> apreload;

struct ascii_proto_reboot : public ascii_proto_base {// 重新启动软件
public:
	ascii_proto_reboot() {
		type = APTYPE_REBOOT;
	}
};
typedef boost::shared_ptr<ascii_proto_reboot> apreboot;

/* 观测计划 */
/*!
 * @struct ascii_proto_append_plan 观测计划
 * @note
 * 复用为通信协议和观测计划
 */
struct ascii_proto_append_plan : public ascii_proto_base {
	int		plan_sn;	//< 计划编号
	string	plan_time;	//< 计划生成时间
	string	plan_type;	//< 计划类型
	string	obstype;	//< 观测类型
	string	grid_id;	//< 天区划分模式
	string	field_id;	//< 天区编号
	string	observer;	//< 观测者或触发源
	string	objname;	//< 目标名称
	string	runname;	//< 目标轮次名称
	double	ra;			//< 视场中心赤经, 量纲: 角度
	double	dec;		//< 视场中心赤纬, 量纲: 角度
	double	epoch;		//< 视场中心位置坐标系
	string	objerror;	//< 目标坐标误差
	double	objra;		//< 目标赤经, 量纲: 角度
	double	objdec;		//< 目标赤纬, 量纲: 角度
	double	objepoch;	//< 目标位置坐标系
	string	imgtype;	//< 图像类型
	//<< 单望远镜多相机观测系统, 各相机使用不同参数
	vector<string>		filter;		//< 滤光片名称或滤光片组合名称
	vector<double>		expdur;		//< 曝光时间或曝光时间组合
	vector<double>		delay;		//< 帧间延时, 量纲: 秒
	vector<int>			frmcnt;		//< 总帧数
	//>> 单望远镜多相机观测系统, 各相机使用不同参数
	int		priority;	//< 优先级
	string	begin_time;	//< 曝光开始时间
	string	end_time;	//< 曝光结束时间
	int		pair_id;	//< 配对标志

public:
	ascii_proto_append_plan() {
		type = APTYPE_APPGWAC;
		plan_sn = -1;
		ra = dec = 1E30;
		epoch = 2000.0;
		objra = objdec = 1E30;
		objepoch = 2000.0;
		priority = 0;
		pair_id = INT_MIN;
	}

	/*!
	 * @brief 为gid:uid具备通配性的bias、dark、flat等计划类型生成拷贝
	 */
	ascii_proto_append_plan& operator=(const ascii_proto_append_plan& ap) {
		if (this != &ap) {
			int n, i;

			gid			= ap.gid;
			uid			= ap.uid;
			cid			= ap.cid;
			utc			= ap.utc;
			plan_sn		= ap.plan_sn;
			plan_time	= ap.plan_time;
			plan_type	= ap.plan_type;
			obstype		= ap.obstype;
			grid_id		= ap.grid_id;
			field_id	= ap.field_id;
			observer	= ap.observer;
			objname		= ap.objname;
			runname		= ap.runname;
			ra			= ap.ra;
			dec			= ap.dec;
			epoch		= ap.epoch;
			objra		= ap.objra;
			objdec		= ap.objdec;
			objepoch	= ap.objepoch;
			objerror	= ap.objerror;
			imgtype		= ap.imgtype;
			priority	= ap.priority;
			begin_time	= ap.begin_time;
			end_time	= ap.end_time;
			pair_id		= ap.pair_id;

			for (i = 0, n = ap.filter.size(); i < n; ++i) {// 复制滤光片
				filter.push_back(ap.filter[i]);
			}
			for (i = 0, n = ap.expdur.size(); i < n; ++i) {// 复制曝光时间
				expdur.push_back(ap.expdur[i]);
			}
			for (i = 0, n = ap.delay.size(); i < n; ++i) {// 复制延迟时间
				delay.push_back(ap.delay[i]);
			}
			for (i = 0, n = ap.frmcnt.size(); i < n; ++i) {// 复制总帧数
				frmcnt.push_back(ap.frmcnt[i]);
			}
		}
		return *this;
	}
};
typedef boost::shared_ptr<ascii_proto_append_plan> apappplan;

struct ascii_proto_abort_plan : public ascii_proto_base {// 中止并删除指定计划
	int plan_sn;	//< 计划编号

public:
	ascii_proto_abort_plan() {
		type = APTYPE_ABTPLAN;
		plan_sn = -1;
	}
};
typedef boost::shared_ptr<ascii_proto_abort_plan> apabtplan;

struct ascii_proto_check_plan : public ascii_proto_base {// 检查关机计划执行状态
	int plan_sn;	//< 计划编号

public:
	ascii_proto_check_plan() {
		type = APTYPE_CHKPLAN;
		plan_sn = -1;
	}
};
typedef boost::shared_ptr<ascii_proto_check_plan> apchkplan;

struct ascii_proto_plan : public ascii_proto_base {// 观测计划执行状态
	int plan_sn;	//< 计划编号
	int state;		//< 状态

public:
	ascii_proto_plan() {
		type    = APTYPE_PLAN;
		plan_sn = INT_MIN;
		state   = 0;
	}

	ascii_proto_plan& operator=(const ascii_proto_append_plan &plan) {
		plan_sn = plan.plan_sn;
		gid     = plan.gid;
		uid     = plan.uid;

		return *this;
	}
};
typedef boost::shared_ptr<ascii_proto_plan> applan;

/* 转台/望远镜 */
struct ascii_proto_find_home : public ascii_proto_base {// 搜索零点
public:
	ascii_proto_find_home() {
		type = APTYPE_FINDHOME;
	}
};
typedef boost::shared_ptr<ascii_proto_find_home> apfindhome;

struct ascii_proto_home_sync : public ascii_proto_base {// 同步零点
	double ra;		//< 赤经, 量纲: 角度
	double dc;		//< 赤纬, 量纲: 角度
	double epoch;	//< 历元

public:
	ascii_proto_home_sync() {
		type = APTYPE_HOMESYNC;
		ra = dc = 1E30;
		epoch = 2000.0;
	}
};
typedef boost::shared_ptr<ascii_proto_home_sync> aphomesync;

struct ascii_proto_slewto : public ascii_proto_base {// 指向
	double ra;		//< 赤经, 量纲: 角度
	double dc;		//< 赤纬, 量纲: 角度
	double epoch;	//< 历元

public:
	ascii_proto_slewto() {
		type = APTYPE_SLEWTO;
		ra = dc = 1E30;
		epoch = 2000.0;
	}
};
typedef boost::shared_ptr<ascii_proto_slewto> apslewto;

struct ascii_proto_park : public ascii_proto_base {// 复位
public:
	ascii_proto_park() {
		type = APTYPE_PARK;
	}
};
typedef boost::shared_ptr<ascii_proto_park> appark;

struct ascii_proto_guide : public ascii_proto_base {// 导星
	double ra;		//< 指向位置对应的天球坐标-赤经, 或赤经偏差, 量纲: 角度
	double dc;		//< 指向位置对应的天球坐标-赤纬, 或赤纬偏差, 量纲: 角度
	double objra;	//< 目标赤经, 量纲: 角度
	double objdc;	//< 目标赤纬, 量纲: 角度

public:
	ascii_proto_guide() {
		type = APTYPE_GUIDE;
		ra = dc = 1E30;
		objra = objdc = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_guide> apguide;

struct ascii_proto_abort_slew : public ascii_proto_base {// 中止指向
public:
	ascii_proto_abort_slew() {
		type = APTYPE_ABTSLEW;
	}
};
typedef boost::shared_ptr<ascii_proto_abort_slew> apabortslew;

struct ascii_proto_telescope : public ascii_proto_base {// 望远镜信息
	int state;		//< 工作状态
	int ec;			//< 错误代码
	double ra;		//< 指向赤经, 量纲: 角度
	double dc;		//< 指向赤纬, 量纲: 角度
	double azi;		//< 指向方位, 量纲: 角度
	double ele;		//< 指向高度, 量纲: 角度

public:
	ascii_proto_telescope() {
		type = APTYPE_TELE;
		state = TELESCOPE_ERROR;
		ec = INT_MIN;
		ra = dc = 1E30;
		azi = ele = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_telescope> aptele;

struct ascii_proto_fwhm : public ascii_proto_base {// 半高全宽
	double value;	//< 半高全宽, 量纲: 像素

public:
	ascii_proto_fwhm() {
		type = APTYPE_FWHM;
		value = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_fwhm> apfwhm;

struct ascii_proto_focus : public ascii_proto_base {// 焦点位置
	int state;	//< 调角器工作状态. 0: 未知; 1: 静止; 2: 调焦
	int value;	//< 焦点位置, 量纲: 微米

public:
	ascii_proto_focus() {
		type = APTYPE_FOCUS;
		state = 0;
		value = INT_MIN;
	}
};
typedef boost::shared_ptr<ascii_proto_focus> apfocus;

struct ascii_proto_mcover : public ascii_proto_base {// 开关镜盖
	int value;	//< 复用字
				//< 用户/数据库=>服务器: 指令
				//< 服务器=>用户/数据库: 状态

public:
	ascii_proto_mcover() {
		type = APTYPE_MCOVER;
		value = INT_MIN;
	}
};
typedef boost::shared_ptr<ascii_proto_mcover> apmcover;

/* 相机 -- 上层 */
struct ascii_proto_take_image : public ascii_proto_base {// 采集图像
	string	objname;	//< 目标名
	string	imgtype;	//< 图像类型
	string	filter;		//< 滤光片名称
	double	expdur;		//< 曝光时间, 量纲: 秒
	int		frmcnt;		//< 曝光帧数

public:
	ascii_proto_take_image() {
		type = APTYPE_TAKIMG;
		expdur = 0.0;
		frmcnt = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_take_image> aptakeimg;

struct ascii_proto_abort_image : public ascii_proto_base {// 中止采集图像
public:
	ascii_proto_abort_image() {
		type = APTYPE_ABTIMG;
	}
};
typedef boost::shared_ptr<ascii_proto_abort_image> apabortimg;

/* 相机 -- 底层 */
struct ascii_proto_object : public ascii_proto_base {// 目标信息与曝光参数
	/* 观测目标描述信息 */
	int    plan_sn;		//< 计划编号, [0, INT_MAX - 1). INT_MAX保留用于手动
	string plan_time;	//< 计划生成时间
	string plan_type;	//< 计划类型
	string observer;	//< 观测者
	string obstype;		//< 观测类型
	string grid_id;		//< 天区划分模式
	string field_id;	//< 天区编号
	string objname;		//< 目标名
	string runname;		//< 目标轮次名称
	double ra;			//< 指向赤经, 量纲: 角度
	double dec;			//< 指向赤纬, 量纲: 角度
	double epoch;		//< 指向坐标系
	double objra;		//< 目标赤经, 量纲: 角度
	double objdec;		//< 目标赤纬, 量纲: 角度
	double objepoch;	//< 目标坐标系
	string objerror;	//< 位置误差
	int    priority;	//< 优先级
	string begin_time;	//< 曝光起始时间, 格式: YYYYMMDDThhmmss.sss
	string end_time;	//< 曝光结束时间
	int    pair_id;		//< 分组编号
	/* 曝光控制信息 */
	string imgtype;		//< 图像类型
	string filter;		//< 滤光片名称
	double expdur;		//< 曝光时间, 量纲: 秒
	double delay;		//< 帧间延迟, 量纲: 秒
	int    frmcnt;		//< 曝光帧数
	int    frmno;		//< 曝光起始索引

public:
	ascii_proto_object() {
		type = APTYPE_OBJECT;
		plan_sn = INT_MIN;
		ra = dec = 1E30;
		epoch = 2000.0;
		objra = objdec = 1E30;
		objepoch = 2000.0;
		priority = INT_MIN;
		pair_id  = INT_MIN;
		expdur   = 0.0;
		delay    = 0.0;
		frmcnt   = 0;
		frmno    = 0;
	}

	ascii_proto_object(ascii_proto_append_plan &plan) {
		type = APTYPE_OBJECT;
		plan_sn		= plan.plan_sn;
		plan_time	= plan.plan_time;
		plan_type	= plan.plan_type;
		observer	= plan.observer;
		obstype		= plan.obstype;
		grid_id		= plan.grid_id;
		field_id	= plan.field_id;
		objname		= plan.objname;
		runname		= plan.runname;
		ra			= plan.ra;
		dec			= plan.dec;
		epoch		= plan.epoch;
		objra		= plan.objra;
		objdec		= plan.objdec;
		objepoch	= plan.objepoch;
		objerror	= plan.objerror;
		priority	= plan.priority;
		begin_time	= plan.begin_time;
		end_time	= plan.end_time;
		pair_id		= plan.pair_id;
		imgtype		= plan.imgtype;
		/* 滤光片及曝光参数需要循环采用观测计划相关参数 */
		expdur   = 0.0;
		delay    = 0.0;
		frmcnt   = 0;
		frmno    = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_object> apobject;

struct ascii_proto_expose : public ascii_proto_base {// 曝光指令
	int command; // 控制指令

public:
	ascii_proto_expose() {
		type = APTYPE_EXPOSE;
		command = 0;
	}

	ascii_proto_expose(const string _cid, const int _cmd) {
		type = APTYPE_EXPOSE;
		cid  = _cid;
		command = _cmd;
	}
};
typedef boost::shared_ptr<ascii_proto_expose> apexpose;

struct ascii_proto_camera : public ascii_proto_base {// 相机信息
	int		state;		//< 工作状态
	int		errcode;	//< 错误代码
	int		mcstate;	//< 镜盖状态
	double	coolget;	//< 探测器温度, 量纲: 摄氏度
	int		focus;		//< 焦点位置
	string	objname;	//< 观测目标名称
	string	filename;	//< 文件名
	string	imgtype;	//< 图像类型
	string	filter;		//< 滤光片
	double	expdur;		//< 曝光时间, 量纲: 秒
	double	delay;		//< 延迟时间, 量纲: 秒
	int		frmcnt;		//< 总帧数
	int     loopno;		//< 循环索引
	int     expno;		//< 曝光参数索引
	int		frmno;		//< 帧编号

public:
	ascii_proto_camera() {
		type = APTYPE_CAMERA;
		state = CAMCTL_ERROR;
		errcode = INT_MIN;
		mcstate = INT_MIN;
		coolget = 1E30;
		focus = INT_MIN;
		expdur = 0.0;
		delay  = 0.0;
		frmcnt = 0;
		loopno = 0;
		expno  = 0;
		frmno  = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_camera> apcam;

/* GWAC相机辅助程序通信协议: 温度和真空度 */
struct ascii_proto_cooler : public ascii_proto_base {// 温控参数
	float voltage;	//< 工作电压.   量纲: V
	float current;	//< 工作电流.   量纲: A
	float hotend;	//< 热端温度.   量纲: 摄氏度
	float coolget;	//< 探测器温度. 量纲: 摄氏度
	float coolset;	//< 制冷温度.   量纲: 摄氏度

public:
	ascii_proto_cooler() {
		type = APTYPE_COOLER;
		voltage = current = 1E30;
		hotend = coolget = coolset = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_cooler> apcooler;

struct ascii_proto_vacuum : public ascii_proto_base {// 真空度参数
	float voltage;	//< 工作电压.   量纲: V
	float current;	//< 工作电流.   量纲: A
	string pressure;//< 气压

public:
	ascii_proto_vacuum() {
		type = APTYPE_VACUUM;
		voltage = current = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_vacuum> apvacuum;

/* 观测系统: 转台/望远镜+相机 */
struct ascii_proto_obss : public ascii_proto_base {
	struct camera_state {// 相机状态
		string cid;	//< 相机编号
		int state;	//< 工作状态
	};

	int state;		//< 系统工作状态
	int op_sn;		//< 在执行观测计划编号
	string op_time;	//< 计划开始执行时间
	int mount;		//< 望远镜工作状态
	vector<camera_state> camera;	//< 相机工作状态

public:
	ascii_proto_obss() {
		type = APTYPE_OBSS;
		state   = -1;
		op_sn   = -1;
		mount   = -1;
	}

};
typedef boost::shared_ptr<ascii_proto_obss> apobss;

/* FITS文件传输 */
struct ascii_proto_fileinfo : public ascii_proto_base {// 文件描述信息, 客户端=>服务器
	string grid;		//< 天区划分模式
	string field;		//< 天区编号
	string tmobs;		//< 观测时间
	string subpath;		//< 子目录名称
	string filename;	//< 文件名称
	int filesize;		//< 文件大小, 量纲: 字节

public:
	ascii_proto_fileinfo() {
		type = APTYPE_FILEINFO;
		filesize = INT_MIN;
	}
};
typedef boost::shared_ptr<ascii_proto_fileinfo> apfileinfo;

struct ascii_proto_filestat : public  ascii_proto_base {// 文件传输结果, 服务器=>客户端
	/*!
	 * @member status 文件传输结果
	 * - 1: 服务器完成准备, 通知客户端可以发送文件数据
	 * - 2: 服务器完成接收, 通知客户端可以发送其它文件
	 * - 3: 文件接收错误
	 */
	int status;	//< 文件传输结果

public:
	ascii_proto_filestat() {
		type = APTYPE_FILESTAT;
		status = INT_MIN;
	}
};
typedef boost::shared_ptr<ascii_proto_filestat> apfilestat;

//////////////////////////////////////////////////////////////////////////////
/*!
 * @class AsciiProtocol 通信协议操作接口, 封装协议解析与构建过程
 */
class AsciiProtocol {
public:
	AsciiProtocol();
	virtual ~AsciiProtocol();

public:
	/* 数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock;	//< 互斥锁
	typedef boost::shared_array<char> charray;	//< 字符数组

protected:
	/* 成员变量 */
	boost::mutex mtx_;	//< 互斥锁
	int ibuf_;			//< 存储区索引
	charray buff_;		//< 存储区

protected:
	/*!
	 * @brief 输出编码后字符串
	 * @param compacted 已编码字符串
	 * @param n         输出字符串长度, 量纲: 字节
	 * @return
	 * 编码后字符串
	 */
	const char* output_compacted(string& output, int& n);
	const char* output_compacted(const char* s, int& n);
	/*!
	 * @brief 连接关键字和对应数值, 并将键值对加入output末尾
	 * @param output   输出字符串
	 * @param keyword  关键字
	 * @param value    非字符串型数值
	 */
	template <class T1, class T2>
	void join_kv(string& output, T1& keyword, T2& value) {
		boost::format fmt("%1%=%2%,");
		fmt % keyword % value;
		output += fmt.str();
	}

	/*!
	 * @brief 解析单个关键字和对应数值
	 * @param kv       keyword=value对
	 * @param keyword  关键字
	 * @param value    对应数值
	 * @return
	 * 关键字和数值非空
	 */
	bool resolve_kv(string& kv, string& keyword, string& value);
	/**
	 * @brief 解析键值对集合并提取通用项
	 */
	void resolve_kv_array(listring &tokens, likv &kvs, ascii_proto_base &basis);

public:
	/*---------------- 封装通信协议 ----------------*/
	/* 注册设备与注册结果 */
	/**
	 * @note 协议封装说明
	 * 输入参数: 结构体形式协议
	 * 输出按时: 封装后字符串, 封装后字符串长度
	 */
	/**
	 * @brief 封装设备注册和注册结果
	 */
	const char *CompactRegister(apreg proto, int &n);
	const char *CompactRegister(int ostype, int &n);
	/*!
	 * @brief 封装设备注销和注销结果
	 */
	const char *CompactUnregister(apunreg proto, int &n);
	/*!
	 * @brief 封装开机自检
	 */
	const char *CompactStart(apstart proto, int &n);
	/*!
	 * @brief 封装关机/复位
	 */
	const char *CompactStop(apstop proto, int &n);
	/*!
	 * @brief 启用设备
	 */
	const char *CompactEnable(apenable proto, int &n);
	/*!
	 * @brief 禁用设备
	 */
	const char *CompactDisable(apdisable proto, int &n);
	/*!
	 * @brief 重新加载参数
	 */
	const char *CompactReload(apreload proto, int &n);
	/*!
	 * @brief 重新启动程序
	 */
	const char *CompactReboot(apreboot proto, int &n);
	/*!
	 * @brief 观测系统工作状态
	 */
	const char *CompactObss(apobss proto, int &n);

	/**
	 * @brief 封装搜索零点指令
	 */
	const char *CompactFindHome(apfindhome proto, int &n);
	const char *CompactFindHome(int &n);
	/**
	 * @brief 封装同步零点指令
	 */
	const char *CompactHomeSync(aphomesync proto, int &n);
	const char *CompactHomeSync(double ra, double dec, int &n);
	/**
	 * @brief 封装指向指令
	 */
	const char *CompactSlewto(apslewto proto, int &n);
	const char *CompactSlewto(double ra, double dec, double epoch, int &n);
	/**
	 * @brief 封装复位指令
	 */
	const char *CompactPark(appark proto, int &n);
	const char *CompactPark(int &n);
	/**
	 * @brief 封装导星指令
	 */
	const char *CompactGuide(apguide proto, int &n);
	const char *CompactGuide(double ra, double dec, int &n);
	/**
	 * @brief 封装中止指向指令
	 */
	const char *CompactAbortSlew(apabortslew proto, int &n);
	const char *CompactAbortSlew(int &n);
	/**
	 * @brief 封装望远镜实时信息
	 */
	const char *CompactTelescope(aptele proto, int &n);
	/**
	 * @brief 封装半高全宽指令和数据
	 */
	const char *CompactFWHM(apfwhm proto, int &n);
	/**
	 * @brief 封装调焦指令和数据
	 */
	const char *CompactFocus(apfocus proto, int &n);
	const char *CompactFocus(int position, int &n);
	/**
	 * @brief 封装镜盖指令和状态
	 */
	const char *CompactMirrorCover(apmcover proto, int &n);
	const char *CompactMirrorCover(int state, int &n);
	/**
	 * @brief 封装手动曝光指令
	 */
	const char *CompactTakeImage(aptakeimg proto, int &n);
	/**
	 * @brief 封装手动中止曝光指令
	 */
	const char *CompactAbortImage(apabortimg proto, int &n);
	/**
	 * @brief 封装目标信息, 用于写入FITS头
	 */
	const char *CompactObject(apobject proto, int &n);
	/**
	 * @brief 封装曝光指令
	 */
	const char *CompactExpose(int cmd, int &n);
	/**
	 * @brief 封装相机实时信息
	 */
	const char *CompactCamera(apcam proto, int &n);
	/**
	 * @brief 封装通用观测计划
	 */
	const char *CompactAppendPlan(apappplan proto, int &n);
	/**
	 * @brief 封装删除观测计划
	 */
	const char *CompactAbortPlan(apabtplan proto, int &n);
	/**
	 * @brief 封装检查观测计划
	 */
	const char *CompactCheckPlan(apchkplan proto, int &n);
	/**
	 * @brief 封装观测执行状态
	 */
	const char *CompactPlan(applan proto, int &n);
	/* GWAC相机辅助程序通信协议: 温度和真空度 */
	/*!
	 * @brief 封装温控信息
	 */
	const char *CompactCooler(apcooler proto, int &n);
	/*!
	 * @brief 封装真空度信息
	 */
	const char *CompactVacuum(apvacuum proto, int &n);
	/* FITS文件传输 */
	/*!
	 * @brief 封装文件描述信息
	 */
	const char *CompactFileInfo(apfileinfo proto, int &n);
	/*!
	 * @brief 封装文件传输结果
	 */
	const char *CompactFileStat(apfilestat proto, int &n);
	/**
	 * @brief 拷贝生成新的通用观测计划
	 */
	apappplan CopyAppendPlan(apappplan proto);
	/*---------------- 解析通信协议 ----------------*/
	/*!
	 * @brief 解析字符串生成结构化通信协议
	 * @param rcvd 待解析字符串
	 * @return
	 * 统一转换为apbase类型
	 */
	apbase Resolve(const char *rcvd);

protected:
	/*!
	 * @brief 封装协议共性内容
	 * @param base    转换为基类的协议指针
	 * @param output  输出字符串
	 */
	void compact_base(apbase base, string &output);
	/*---------------- 解析通信协议 ----------------*/
	/**
	 * @note 协议解析说明
	 * 输入参数: 构成协议的字符串, 以逗号为分隔符解析后的keyword=value字符串组
	 * 输出参数: 转换为apbase类型的协议体. 当其指针为空时, 代表字符串不符合规范
	 */
	/**
	 * @brief 注册设备与注册结果
	 * */
	apbase resolve_register(likv &kvs);
	/**
	 * @brief 注销设备与注销结果
	 * */
	apbase resolve_unregister(likv &kvs);
	/**
	 * @brief 开机自检
	 * */
	apbase resolve_start(likv &kvs);
	/**
	 * @brief 关机/复位
	 * */
	apbase resolve_stop(likv &kvs);
	/**
	 * @brief 启用设备
	 * */
	apbase resolve_enable(likv &kvs);
	/**
	 * @brief 禁用设备
	 * */
	apbase resolve_disable(likv &kvs);
	/**
	 * @brief 重新加载参数
	 */
	apbase resolve_reload(likv &kvs);
	/**
	 * @brief 重新启动程序
	 */
	apbase resolve_reboot(likv &kvs);
	/**
	 * @brief 观测系统工作状态
	 */
	apbase resolve_obss(likv &kvs);
	/**
	 * @brief 搜索零点
	 */
	apbase resolve_findhome(likv &kvs);
	/**
	 * @brief 同步零点, 修正转台零点偏差
	 */
	apbase resolve_homesync(likv &kvs);
	/**
	 * @brief 指向赤道坐标, 到位后保持恒动跟踪
	 */
	apbase resolve_slewto(likv &kvs);
	/**
	 * @brief 复位至安全位置, 到位后保持静止
	 */
	apbase resolve_park(likv &kvs);
	/**
	 * @brief 导星, 微量修正当前指向位置
	 */
	apbase resolve_guide(likv &kvs);
	/**
	 * @brief 中止指向过程
	 */
	apbase resolve_abortslew(likv &kvs);
	/**
	 * @brief 望远镜实时信息
	 */
	apbase resolve_telescope(likv &kvs);
	/**
	 * @brief 星象半高全宽
	 */
	apbase resolve_fwhm(likv &kvs);
	/**
	 * @brief 调焦器位置
	 */
	apbase resolve_focus(likv &kvs);
	/**
	 * @brief 镜盖指令与状态
	 */
	apbase resolve_mcover(likv &kvs);
	/**
	 * @brief 手动曝光指令
	 */
	apbase resolve_takeimg(likv &kvs);
	/**
	 * @brief 手动停止曝光指令
	 */
	apbase resolve_abortimg(likv &kvs);
	/**
	 * @brief 观测目标描述信息
	 */
	apbase resolve_object(likv &kvs);
	/**
	 * @brief 曝光指令
	 */
	apbase resolve_expose(likv &kvs);
	/**
	 * @brief 相机实时信息
	 */
	apbase resolve_camera(likv &kvs);
	/**
	 * @brief 温控信息
	 */
	apbase resolve_cooler(likv &kvss);
	/**
	 * @brief 真空度信息
	 */
	apbase resolve_vacuum(likv &kvs);
	/**
	 * @brief FITS文件描述信息
	 */
	apbase resolve_fileinfo(likv &kvs);
	/**
	 * @brief FITS文件传输结果
	 */
	apbase resolve_filestat(likv &kvs);
	/**
	 * @brief 追加一条GWAC观测计划
	 */
	apbase resolve_append_gwac(likv &kvs);
	/**
	 * @brief 追加一条常规观测计划
	 */
	apbase resolve_append_plan(likv &kvs);
	/*!
	 * @brief 检查计划
	 */
	apbase resolve_check_plan(likv &kvs);
	/*!
	 * @brief 删除计划
	 */
	apbase resolve_abort_plan(likv &kvs);
	/*!
	 * @brief 计划执行状态
	 */
	apbase resolve_plan(likv &kvs);
};

typedef boost::shared_ptr<AsciiProtocol> AscProtoPtr;

//////////////////////////////////////////////////////////////////////////////
extern AscProtoPtr make_ascproto();

/*!
 * @brief 检查赤经是否有效
 * @param ra 赤经, 量纲: 角度
 * @return
 * 赤经属于[0, 360.0)返回true; 否则返回false
 */
extern bool valid_ra(double ra);

/*!
 * @brief 检查赤纬是否有效
 * @param dec 赤纬, 量纲: 角度
 * @return
 * 赤经属于【-90, +90.0]返回true; 否则返回false
 */
extern bool valid_dec(double dec);
/*!
 * @brief 检查图像类型有效性, 并生成与其对照的类型索引和缩略名
 * @param imgtype 图像类型
 * @param sabbr   对应的缩略名
 * @return
 * 图像类型有效性
 */
IMAGE_TYPE check_imgtype(string imgtype, string &sabbr);
//////////////////////////////////////////////////////////////////////////////

#endif /* ASCIIPROTOCOL_H_ */
