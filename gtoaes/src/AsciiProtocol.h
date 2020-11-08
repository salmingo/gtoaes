/*!
 * @file AsciiProtocol.h 声明文件, 声明GWAC/GFT系统中字符串型通信协议
 * @version 0.1
 * @date 2017-11-17
 * - 通信协议采用Struct声明
 * - 通信协议继承自ascii_protocol_base
 * @version 0.2
 * @date 2020-11-08
 * - 优化
 * - 增加: 气象信息
 */

#ifndef ASCIIPROTOCOL_H_
#define ASCIIPROTOCOL_H_

#include <boost/thread/mutex.hpp>
#include "AsciiProtocolBase.h"
#include "ObservationPlanBase.h"

using std::list;
using std::vector;

typedef list<string> listring;	///< string列表

//////////////////////////////////////////////////////////////////////////////
/* 宏定义: 通信协议类型 */
#define APTYPE_REG		"register"		///< 注册: 设备注册编号; 用户关联观测系统
#define APTYPE_UNREG	"unregister"	///< 取消注册
#define APTYPE_START	"start"			///< 启动自动观测
#define APTYPE_STOP		"stop"			///< 停止自动观测
#define APTYPE_ENABLE	"enable"		///< 启用设备
#define APTYPE_DISABLE	"disable"		///< 禁用设备

#define APTYPE_OBSS		"obss"			///< 观测系统实时状态
#define APTYPE_OBSITE	"obsite"		///< 测站信息

#define APTYPE_APPPLAN	"append_plan"	///< 追加计划, 计划进入队列
#define APTYPE_IMPPLAN	"implement_plan"///< 执行计划, 计划优先级最高的话, 立即执行
#define APTYPE_ABTPLAN	"abort_plan"	///< 中止计划
#define APTYPE_CHKPLAN	"check_plan"	///< 检查计划状态
#define APTYPE_PLAN		"plan"			///< 计划状态

#define APTYPE_FINDHOME	"find_home"		///< 搜索零点
#define APTYPE_HOMESYNC	"home_sync"		///< 同步零点
#define APTYPE_SLEWTO	"slewto"		///< 指向目标位置
#define APTYPE_PARK		"park"			///< 复位
#define APTYPE_GUIDE	"guide"			///< 导星
#define APTYPE_ABTSLEW	"abort_slew"	///< 中止指向
#define APTYPE_MOUNT	"mount"			///< 转台实时状态

#define APTYPE_DOME		"dome"			///< 圆顶实时状态
#define APTYPE_SLIT		"slit"			///< 天窗指令与状态
#define APTYPE_MCOVER	"mcover"		///< 镜盖指令与状态

#define APTYPE_TAKIMG	"take_image"	///< 采集图像
#define APTYPE_ABTIMG	"abort_image"	///< 中止图像采集过程
#define APTYPE_OBJECT	"object"		///< 观测目标信息
#define APTYPE_EXPOSE	"expose"		///< 曝光指令
#define APTYPE_CAMERA	"camera"		///< 相机实时状态

#define APTYPE_FWHM		"fwhm"			///< 图像的FWHM
#define APTYPE_FOCUS	"focus"			///< 调焦指令与实时位置

#define APTYPE_FILEINFO	"fileinfo"		///< 文件信息
#define APTYPE_FILESTAT	"filestat"		///< 文件统计信息

#define APTYPE_COOLER	"cooler"		///< 单独的相机制冷
#define APTYPE_VACUUM	"vacuum"		///< 单独的真空度

#define APTYPE_RAINFALL	"rainfall"		///< 雨量
#define APTYPE_WIND		"wind"			///< 风速与风向
#define APTYPE_CLOUD	"cloud"			///< 云量

/*--------------------------------- 声明通信协议 ---------------------------------*/
//////////////////////////////////////////////////////////////////////////////
struct ascii_proto_reg : public ascii_proto_base {// 注册设备/用户
public:
	ascii_proto_reg() {
		type = APTYPE_REG;
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

struct ascii_proto_start : public ascii_proto_base {// 启动自动观测流程
public:
	ascii_proto_start() {
		type = APTYPE_START;
	}
};
typedef boost::shared_ptr<ascii_proto_start> apstart;

struct ascii_proto_stop : public ascii_proto_base {// 启动自动观测流程
public:
	ascii_proto_stop() {
		type = APTYPE_STOP;
	}
};
typedef boost::shared_ptr<ascii_proto_stop> apstop;

struct ascii_proto_enable : public ascii_proto_base {// 启用设备或观测系统
public:
	ascii_proto_enable() {
		type = APTYPE_ENABLE;
	}
};
typedef boost::shared_ptr<ascii_proto_enable> apenable;

struct ascii_proto_disable : public ascii_proto_base {// 禁用设备或观测系统
public:
	ascii_proto_disable() {
		type = APTYPE_DISABLE;
	}
};
typedef boost::shared_ptr<ascii_proto_disable> apdisable;

//////////////////////////////////////////////////////////////////////////////
struct ascii_proto_obss : public ascii_proto_base {
	struct camera_state {///< 相机状态
		string cid;		///< 相机编号
		int state;		///< 工作状态
	};

	int state;		///< 系统工作状态
	string plan_sn;	///< 在执行观测计划编号
	string op_time;	///< 计划开始执行时间, CCYY-MM-DDThh:mm:ss.ssssss
	int mount;		///< 望远镜工作状态
	vector<camera_state> camera;	///< 相机工作状态

public:
	ascii_proto_obss() {
		type = APTYPE_OBSS;
		state   = -1;
		mount   = -1;
	}
};
typedef boost::shared_ptr<ascii_proto_obss> apobss;

struct ascii_proto_obsite : public ascii_proto_base {// 测站位置
	string  sitename;	//< 测站名称
	double	lon;		//< 地理经度, 量纲: 角度
	double	lat;		//< 地理纬度, 量纲: 角度
	double	alt;		//< 海拔, 量纲: 米
	int timezone;		//< 时区, 量纲: 小时

public:
	ascii_proto_obsite() {
		type = APTYPE_OBSITE;
		lon = lat = alt = 1E30;
		timezone = 8;
	}
};
typedef boost::shared_ptr<ascii_proto_obsite> apobsite;

//////////////////////////////////////////////////////////////////////////////
/* 观测计划 */
/*!
 * @struct ascii_proto_append_plan
 * @brief 新的观测计划
 */
struct ascii_proto_append_plan : public ascii_proto_base {
	ObsPlanItemPtr plan;

public:
	ascii_proto_append_plan() {
		type = APTYPE_APPPLAN;
		plan = ObservationPlanItem::Create();
	}
};
typedef boost::shared_ptr<ascii_proto_append_plan> apappplan;

struct ascii_proto_implement_plan : public ascii_proto_base {
	ObsPlanItemPtr plan;

public:
	ascii_proto_implement_plan() {
		type = APTYPE_IMPPLAN;
		plan = ObservationPlanItem::Create();
	}
};
typedef boost::shared_ptr<ascii_proto_implement_plan> apimpplan;

struct ascii_proto_abort_plan : public ascii_proto_base {// 中止并删除指定计划
	string plan_sn;	//< 计划编号

public:
	ascii_proto_abort_plan() {
		type = APTYPE_ABTPLAN;
	}
};
typedef boost::shared_ptr<ascii_proto_abort_plan> apabtplan;

struct ascii_proto_check_plan : public ascii_proto_base {// 检查关机计划执行状态
	string plan_sn;	//< 计划编号

public:
	ascii_proto_check_plan() {
		type = APTYPE_CHKPLAN;
	}
};
typedef boost::shared_ptr<ascii_proto_check_plan> apchkplan;

struct ascii_proto_plan : public ascii_proto_base {// 观测计划执行状态
	string plan_sn;	//< 计划编号
	int state;		//< 状态

public:
	ascii_proto_plan() {
		type    = APTYPE_PLAN;
		state   = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_plan> applan;

//////////////////////////////////////////////////////////////////////////////
/* 转台 */
struct ascii_proto_find_home : public ascii_proto_base {// 搜索零点
public:
	ascii_proto_find_home() {
		type = APTYPE_FINDHOME;
	}
};
typedef boost::shared_ptr<ascii_proto_find_home> apfindhome;

struct ascii_proto_home_sync : public ascii_proto_base {// 同步零点
	double ra;		//< 赤经, 量纲: 角度
	double dec;		//< 赤纬, 量纲: 角度
	double epoch;	//< 历元

public:
	ascii_proto_home_sync() {
		type = APTYPE_HOMESYNC;
		ra = dec = 1E30;
		epoch = 2000.0;
	}
};
typedef boost::shared_ptr<ascii_proto_home_sync> aphomesync;

struct ascii_proto_slewto : public ascii_proto_base {// 指向
	int coorsys;	///< 坐标系. 0: 地平系; 1: 赤道系; 2: 引导TLE
	double lon;		///< 经度, 角度
	double lat;		///< 纬度, 角度
	double epoch;	///< 历元, 适用于赤道系
	string line1;	///< TLE的第一行
	string line2;	///< TLE的第二行

public:
	ascii_proto_slewto() {
		type = APTYPE_SLEWTO;
		coorsys = -1;
		lon = lat = 1E30;
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
	double dec;		//< 指向位置对应的天球坐标-赤纬, 或赤纬偏差, 量纲: 角度
	double objra;	//< 目标赤经, 量纲: 角度
	double objdec;	//< 目标赤纬, 量纲: 角度

public:
	ascii_proto_guide() {
		type = APTYPE_GUIDE;
		ra = dec = 1E30;
		objra = objdec = 1E30;
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

struct ascii_proto_mount : public ascii_proto_base {///< 转台信息
	int state;		///< 工作状态
	int errcode;	///< 错误代码
	double ra;		///< 指向赤经, 量纲: 角度
	double dec;		///< 指向赤纬, 量纲: 角度
	double azi;		///< 指向方位, 量纲: 角度
	double alt;		///< 指向高度, 量纲: 角度

public:
	ascii_proto_mount() {
		type    = APTYPE_MOUNT;
		state   = 0;
		errcode = 0;
		ra = dec = 1E30;
		azi = alt = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_mount> apmount;

//////////////////////////////////////////////////////////////////////////////
struct ascii_proto_dome : public ascii_proto_base {///< 圆顶实时状态
	double azi;
	double alt;
	double objazi;
	double objalt;

public:
	ascii_proto_dome() {
		type = APTYPE_DOME;
		azi = alt = 1E30;
		objazi = objalt = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_dome> apdome;

struct ascii_proto_slit : public ascii_proto_base {///< 天窗指令与状态
	int command;
	int state;

public:
	ascii_proto_slit() {
		type = APTYPE_SLIT;
		command = state = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_slit> apslit;

struct ascii_proto_mcover : public ascii_proto_base {///< 开关镜盖
	int command;
	int state;

public:
	ascii_proto_mcover() {
		type = APTYPE_MCOVER;
		command = state = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_mcover> apmcover;

//////////////////////////////////////////////////////////////////////////////
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

struct ascii_proto_object : public ascii_proto_base {// 目标信息与曝光参数
	/* 观测目标描述信息 */
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
	string filter;		///< 滤光片名称
	double expdur;		///< 曝光时间, 秒
	double delay;		///< 帧间延时, 秒
	int frmcnt;			///< 帧数
	int priority;		///< 优先级
	ptime tmbegin;		///< 观测起始时间
	ptime tmend;		///< 观测结束时间
	int pair_id;		///< 配对观测标志
	int iloop;			///< 循环序号
	likv kvs;			///< 未定义关键字的键值对

public:
	ascii_proto_object() {
		type = APTYPE_OBJECT;
		coorsys = -1;
		lon = lat = 1E30;
		epoch = 2000.0;
		objra = objdec = 1E30;
		objepoch = 2000.0;
		expdur   = 0.0;
		delay    = 0.0;
		frmcnt   = 0;
		priority = 0;
		pair_id  = 0;
		iloop    = 0;
	}

	ascii_proto_object(ObsPlanItemPtr plan, int ifilter, ptime& start) {
		type = APTYPE_OBJECT;
		plan_sn		= plan->plan_sn;
		plan_time	= plan->plan_time;
		plan_type	= plan->plan_type;
		obstype		= plan->obstype;
		grid_id		= plan->grid_id;
		field_id	= plan->field_id;
		observer	= plan->observer;
		objname		= plan->objname;
		runname		= plan->runname;
		coorsys		= plan->coorsys;
		lon			= plan->lon;
		lat			= plan->lat;
		line1		= plan->line1;
		line2		= plan->line2;
		epoch		= plan->epoch;
		objra		= plan->objra;
		objdec		= plan->objdec;
		objepoch	= plan->objepoch;
		objerror	= plan->objerror;
		imgtype		= plan->imgtype;
		filter		= plan->filters[ifilter];
		expdur		= plan->expdur;
		delay		= plan->delay;
		frmcnt		= plan->frmcnt;
		priority	= plan->priority;
		tmbegin		= start;
		tmend		= plan->tmend;
		pair_id		= plan->pair_id;
		iloop		= plan->iloop;

		ObservationPlanItem::KVVec& kvs_plan = plan->kvs;
		ObservationPlanItem::KVVec::iterator it;
		for (it = kvs_plan.begin(); it != kvs_plan.end(); ++it) {
			key_val kv;
			kv.keyword = it->first;
			kv.value   = it->second;
			kvs.push_back(kv);
		}
	}

	virtual ~ascii_proto_object() {
		kvs.clear();
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

struct ascii_proto_camera : public ascii_proto_base {///< 相机信息
	int		state;		///< 工作状态
	int		errcode;	///< 错误代码
	int		coolget;	///< 探测器温度, 量纲: 摄氏度
	string	filter;		///< 滤光片

public:
	ascii_proto_camera() {
		type    = APTYPE_CAMERA;
		state   = 0;
		errcode = 0;
		coolget = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_camera> apcam;

//////////////////////////////////////////////////////////////////////////////
struct ascii_proto_fwhm : public ascii_proto_base {///< 半高全宽
	double value;	//< 半高全宽, 量纲: 像素

public:
	ascii_proto_fwhm() {
		type = APTYPE_FWHM;
		value = 1E30;
	}
};
typedef boost::shared_ptr<ascii_proto_fwhm> apfwhm;

struct ascii_proto_focus : public ascii_proto_base {///< 焦点位置
	int state;		///< 调角器工作状态. 0: 未知; 1: 静止; 2: 调焦
	int position;	///< 焦点位置, 量纲: 微米

public:
	ascii_proto_focus() {
		type = APTYPE_FOCUS;
		state = position = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_focus> apfocus;

//////////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////
/* FITS文件传输 */
struct ascii_proto_fileinfo : public ascii_proto_base {///< 文件描述信息, 客户端=>服务器
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

struct ascii_proto_filestat : public ascii_proto_base {///< 文件传输结果, 服务器=>客户端
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
struct ascii_proto_rainfall : public ascii_proto_base {
	int value;

public:
	ascii_proto_rainfall() {
		type = APTYPE_RAINFALL;
		value = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_rainfall> aprain;

struct ascii_proto_wind : public ascii_proto_base {
	int orient;	///< 风向...正南0点, 顺时针增加?
	int speed;	///< 风速, 米/秒

public:
	ascii_proto_wind() {
		type = APTYPE_WIND;
		orient = speed = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_wind> apwind;

struct ascii_proto_cloud : public ascii_proto_base {
	int value;

public:
	ascii_proto_cloud() {
		type = APTYPE_CLOUD;
		value = 0;
	}
};
typedef boost::shared_ptr<ascii_proto_cloud> apcloud;

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
	typedef boost::shared_ptr<AsciiProtocol> Pointer;
	typedef boost::unique_lock<boost::mutex> MtxLck;	///< 互斥锁
	typedef boost::shared_array<char> ChBuff;	///< 字符数组

protected:
	/* 成员变量 */
	boost::mutex mtx_;	///< 互斥锁
	const int szProto_;	///< 协议最大长度: 1400
	int iBuf_;			///< 存储区索引
	ChBuff buff_;		///< 存储区

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
	template <class T>
	void join_kv(string& output, const string& keyword, T& value) {
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

	/**
	 * @brief 封装通用观测计划
	 */
	bool compact_plan(ObsPlanItemPtr plan, string& output);

public:
	static Pointer Create() {
		return Pointer(new AsciiProtocol);
	}

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
	/*!
	 * @brief 封装设备注销和注销结果
	 */
	const char *CompactUnregister(apunreg proto, int &n);
	/*!
	 * @brief 封装开机自检
	 */
	const char *CompactStart(const string& gid, const string& uid, int &n);
	/*!
	 * @brief 封装关机/复位
	 */
	const char *CompactStop(const string& gid, const string& uid, int &n);
	/*!
	 * @brief 启用设备
	 */
	const char *CompactEnable(apenable proto, int &n);
	/*!
	 * @brief 禁用设备
	 */
	const char *CompactDisable(apdisable proto, int &n);

	/*!
	 * @brief 封装测站位置参数
	 */
	const char *CompactObsSite(apobsite proto, int &n);
	/*!
	 * @brief 观测系统工作状态
	 */
	const char *CompactObss(apobss proto, int &n);

	/**
	 * @brief 封装通用观测计划: 计划进入队列
	 */
	const char *CompactAppendPlan(ObsPlanItemPtr plan, int &n);
	/**
	 * @brief 封装通用观测计划: 计划进入队列, 并尝试立即执行
	 */
	const char *CompactImplementPlan(ObsPlanItemPtr plan, int &n);
	/**
	 * @brief 封装删除观测计划
	 */
	const char *CompactAbortPlan(const string& plan_sn, int &n);
	/**
	 * @brief 封装检查观测计划
	 */
	const char *CompactCheckPlan(const string& plan_sn, int &n);
	/**
	 * @brief 封装观测执行状态
	 */
	const char *CompactPlan(applan proto, int &n);

	/**
	 * @brief 封装搜索零点指令
	 * @param gid  组标志
	 * @param uid  单元标志
	 * @note
	 * - 指定设备搜索零点
	 */
	const char *CompactFindHome(const string& gid, const string& uid, int &n);
	/**
	 * @brief 封装同步零点指令
	 */
	const char *CompactHomeSync(const string& gid, const string& uid, double ra, double dec, int &n);
	/**
	 * @brief 封装指向指令
	 */
	const char *CompactSlewto(apslewto proto, int &n);
	/**
	 * @brief 封装复位指令
	 */
	const char *CompactPark(const string& gid, const string& uid, int &n);
	/**
	 * @brief 封装导星指令
	 */
	const char *CompactGuide(apguide proto, int &n);
	/**
	 * @brief 封装中止指向指令
	 */
	const char *CompactAbortSlew(const string& gid, const string& uid, int &n);
	/**
	 * @brief 封装望远镜实时信息
	 */
	const char *CompactMount(apmount proto, int &n);

	/**
	 * @brief 封装半高全宽指令和数据
	 */
	const char *CompactFWHM(apfwhm proto, int &n);
	/**
	 * @brief 封装调焦指令和数据
	 */
	const char *CompactFocus(apfocus proto, int &n);

	/*!
	 * @brief 封装圆顶实时状态
	 */
	const char *CompactDome(apdome proto, int& n);
	/*!
	 * @brief 封装天窗指令和状态
	 */
	const char *CompactSlit(apslit proto, int& n);
	/**
	 * @brief 封装镜盖指令和状态
	 */
	const char *CompactMirrorCover(apmcover proto, int &n);

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

	/*!
	 * @brief 封装雨量
	 */
	const char *CompactRainfall(aprain proto, int &n);
	/*!
	 * @brief 封装风速和风向
	 */
	const char *CompactWind(apwind proto, int &n);
	/*!
	 * @brief 封装云量
	 */
	const char *CompactCloud(apcloud proto, int &n);
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

	/*!
	 * @brief 解析测站参数
	 */
	apbase resolve_obsite(likv &kvs);
	/**
	 * @brief 观测系统工作状态
	 */
	apbase resolve_obss(likv &kvs);

	/*!
	 * @brief 从通信协议解析观测计划
	 */
	void resolve_plan(likv& kvs, ObsPlanItemPtr plan);
	/**
	 * @brief 追加一条常规观测计划
	 */
	apbase resolve_append_plan(likv &kvs);
	/**
	 * @brief 尝试执行一条常规观测计划
	 */
	apbase resolve_implement_plan(likv &kvs);
	/*!
	 * @brief 删除计划
	 */
	apbase resolve_abort_plan(likv &kvs);
	/*!
	 * @brief 检查计划
	 */
	apbase resolve_check_plan(likv &kvs);
	/*!
	 * @brief 计划执行状态
	 */
	apbase resolve_plan(likv &kvs);

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
	 * @brief 转台实时信息
	 */
	apbase resolve_mount(likv &kvs);

	/**
	 * @brief 星象半高全宽
	 */
	apbase resolve_fwhm(likv &kvs);
	/**
	 * @brief 调焦指令和位置
	 */
	apbase resolve_focus(likv &kvs);

	/**
	 * @brief 圆顶实时状态
	 */
	apbase resolve_dome(likv &kvs);
	/**
	 * @brief 天窗指令和状态
	 */
	apbase resolve_slit(likv &kvs);
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
	 * @brief 雨量
	 */
	apbase resolve_rainfall(likv &kvs);
	/**
	 * @brief 风速和风向
	 */
	apbase resolve_wind(likv &kvs);
	/**
	 * @brief 云量
	 */
	apbase resolve_cloud(likv &kvs);
};
typedef AsciiProtocol::Pointer AscProtoPtr;
//////////////////////////////////////////////////////////////////////////////

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
//////////////////////////////////////////////////////////////////////////////

#endif /* ASCIIPROTOCOL_H_ */
