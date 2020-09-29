/**
 * @class ObservationSystem 观测系统集成控制接口
 * @version 0.1
 * @date 2019-10-22
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include <deque>
#include "MessageQueue.h"
#include "AsciiProtocol.h"
#include "AnnexProtocol.h"
#include "ObservationPlan.h"
#include "tcpasio.h"
#include "ATimeSpace.h"
#include "DBCurl.h"

using AstroUtil::ATimeSpace;

/*!
 * @struct MountInfo 封装观测系统中转台状态
 */
struct MountInfo {
	string utc;			//< 时标
	int state;			//< 工作状态
	int errcode;		//< 故障字
	double ra, dec;		//< 指向位置, 量纲: 角度, 赤道系
	double azi, alt;	//< 指向位置, 量纲: 角度, 地平系
	double ora, odc;	//< 目标坐标, 量纲: 角度. 赤道系
	double oaz, oal;	//< 目标坐标, 量纲: 角度. 地平系
	int coorsys;		//< 1: 地平; 2: 赤道
	bool slewing;		//< 指向标志

public:
	MountInfo &operator=(apmount proto) {
		utc     = proto->utc;
		state   = proto->state;
		errcode = proto->errcode;
		ra      = proto->ra;
		dec     = proto->dec;
		azi     = proto->azi;
		alt     = proto->alt;

		return *this;
	}

	/*!
	 * @brief 改变目标位置
	 * @param r 赤经, 量纲: 角度
	 * @param d 赤纬, 量纲: 角度
	 */
	void SetObject(int coor, double coor1, double coor2) {
		slewing = true;
		coorsys = coor;
		if (coor == 1) {
			oaz = coor1;
			oal = coor2;
		}
		else {
			ora = coor1;
			odc = coor2;
		}
	}

	/*!
	 * @brief 将目标位置设置为当前指向位置
	 */
	void Actual2Object() {
		ora = ra;
		odc = dec;
	}

	/*!
	 * @brief 通过检查目标位置和指向位置偏差, 确认是否指向到位
	 */
	bool HasArrived() {
		double err1, err2;
		double tArrive(1.0);
		if (coorsys == COORSYS_ALTAZ) {
			err1 = fabs(azi - oaz);
			err2 = fabs(alt - oal);
		}
		else {
			err1 = fabs(ra - ora);
			err2 = fabs(dec - odc);
		}
		if (err1 > 180) err1 = 360 - err1;
		if (coorsys == COORSYS_GUIDE) return (!(err1 < 0.1 && err2 < 0.1));
		else return (err1 < tArrive && err2 < tArrive);
	}

	/*!
	 * @brief 检查转台是否处于稳定态
	 */
	bool IsStable() {
		return (state == MOUNT_FREEZE || state == MOUNT_PARKED || state == MOUNT_TRACKING);
	}
};
typedef MountInfo NFMount;
typedef boost::shared_ptr<NFMount> NFMountPtr;

/*!
 * @struct InformationCamera 封装观测系统中相机状态信息
 */
struct CameraInfo {
	string utc;		//< 时标
	int state;		//< 工作状态
	int errcode;	//< 错误代码
	float coolget;	//< 探测器温度

public:
	CameraInfo& operator=(ascii_proto_camera &x) {
		utc     = x.utc;
		state   = x.state;
		errcode = x.errcode;
		coolget = x.coolget;

		return *this;
	}
};
typedef CameraInfo NFCamera;
typedef boost::shared_ptr<NFCamera> NFCameraPtr;

struct OBSSInfo {//< 观测系统数据
	bool	automode;	//< 工作模式
	int		state;		//< 系统工作状态
	/* 相机统计状态 */
	ptime	lastflat;	//< 最后一次平场指向时间
	int		exposing;	//< 开始曝光的相机数量
	int		waitsync;	//< 等待平场重新曝光的相机数量
	int		waitflat;	//< 等待平场重新定位的相机数量

public:
	OBSSInfo() {
		automode = true;
		state    = OBSS_ERROR;
		exposing = 0;
		waitsync = 0;
		waitflat = 0;
	}

	/*!
	 * @brief 为某台相机进入曝光模式设置控制参量
	 */
	void enter_exposing() {
		++exposing;
	}

	/*!
	 * @brief 为某台相机离开曝光模式设置控制参量
	 * @return
	 * 任一相机仍处于曝光模式时返回false; 所有相机都离开曝光模式时返回true
	 */
	bool leave_expoing() {
		return (exposing && --exposing == 0);
	}

	/*!
	 * @brief 检查是否所有相机都进入等待平场状态
	 * @return
	 * 所有相机都进入等待平场状态
	 */
	bool enter_waitflat() {
		return (++waitflat == exposing);
	}

	bool leave_waitflat() {
		if (waitflat) --waitflat;
		return (!waitflat);
	}

	/*!
	 * @brief 检查是否所有相机都进入等待平场状态
	 * @return
	 * 所有相机都进入等待平场状态
	 */
	bool enter_waitsync() {
		return ((++waitsync + waitflat) == exposing);
	}

	bool leave_waitsync() {
		if (waitsync) --waitsync;
		return (!(waitflat || waitsync));
	}
};
typedef OBSSInfo NFObss;
typedef boost::shared_ptr<NFObss> NFObssPtr;	//< 观测系统数据

class ObservationSystem : public MessageQueue {
public:
	ObservationSystem(const string& gid, const string& uid);
	virtual ~ObservationSystem();

public:
	/* 声明数据结构 */
	/*!
	 * @function AcquirePlan 声明ObservationSystem回调函数类型, 申请观测计划时触发
	 * @param _1 组标志
	 * @param _2 单元标志
	 * @return
	 * 观测计划地址
	 */
	typedef boost::signals2::signal<ObsPlanPtr (const string&, const string&)> AcquirePlan;
	typedef AcquirePlan::slot_type AcquirePlanSlot;

protected:
	/* 声明数据结构 */
	enum {// 消息ID
		MSG_RECEIVE_MOUNT = MSG_USER,
		MSG_RECEIVE_CAMERA,
		MSG_CLOSE_MOUNT,
		MSG_CLOSE_CAMERA,
		MSG_NEW_PROTOCOL,
		MSG_NEW_PLAN,
		MSG_FLAT_RESLEW
	};
	typedef std::deque<apbase> apdeque;

protected:
	/* 成员变量 */
	/* 观测系统 */
	string gid_;			//< 组标志
	string uid_;			//< 单元标志
	string cid_;			//< 相机标志
	int keep_alive_;		//< 非活跃时维持的时间长度
	ptime tmclosed_;		//< 时标: 断开网络连接
	apobsite  obsite_;		//< 测站位置
	double minEle_;			//< 限位: 最小高度角, 量纲: 角度
	ATimeSpace ats_;		//< 功能: 天文时间-位置变换接口
	NFMountPtr nfMount_;	//< 状态: 转台
	NFCameraPtr nfCamera_;	//< 状态: 相机
	NFObssPtr nfObss_;		//< 状态: 观测系统
	/* 数据库 */
	boost::shared_ptr<DBCurl> dbt_; //< 数据库访问接口
	boost::mutex mtx_database_;		//< 互斥锁: 数据库
	/* 网络通信 */
	TcpCPtr tcpc_mount_;	//< 客户端: 转台
	TcpCPtr tcpc_camera_;	//< 客户端: 集成相机
	TcpCPtr tcpc_focus_;	//< 客户端: 调焦
	boost::shared_array<char> netrcv_;	//< 存储区: 网络信息
	AscProtoPtr ascproto_;	//< 通信协议: AsciiProtocol接口
	AnnProtoPtr annproto_;	//< 通信协议: AnnexProtocol接口
	apdeque apque_;			//< 通信协议: apbase队列
	boost::mutex mtx_apque_;	//< 互斥锁: apbase队列
	threadptr thrd_cycle_;		//< 周期线程: 1. 向数据库发送信息; 2. 检查转台和相机连接的有效性

	/* 观测计划与自动观测逻辑 */
	AcquirePlan cb_acqplan_;	//< 申请观测计划
	ObsPlanPtr plan_now_;		//< 观测计划: 在执行
	threadptr thrd_acqplan_;	//< 线程: 申请观测计划
	boost::condition_variable cv_acqplan_;	//< 条件: 申请观测计划

public:
//////////////////////////////////////////////////////////////////////////////
	/* 公共接口 */
	/*!
	 * @brief 启动服务
	 */
	bool Start();
	/*!
	 * @brief 停止服务器
	 */
	void Stop();
	/*!
	 * @brief 检查gid:uid编号与观测系统是否一致
	 * @return
	 * 1: gid == gid_ && uid == uid_
	 * 2: (uid == "" && gid == gid_)  or (uid == " && gid == ")
	 * 0: uid != uid_ || gid != gid_
	 */
	int IsMatched(const string &gid, const string &uid);
	/*!
	 * @brief 设置观测系统部署位置的地理信息
	 * @brief 测站名称
	 * @param lon      地理经度, 东经为正, 量纲: 角度
	 * @param lat      地理纬度, 北纬为正, 量纲: 角度
	 * @param alt      海拔高度, 量纲: 米
	 * @param timezone 时区, 量纲: 小时
	 */
	void SetGeosite(const string& name, const double lon, const double lat,
			const double alt, const int timezone);
	/*!
	 * @brief 设置最小仰角
	 * @param value 最小仰角, 量纲: 角度
	 * @note
	 * 低于该值则禁止望远镜指向
	 */
	void SetElevationLimit(double value);
	/*!
	 * @brief 注册申请观测计划回调函数
	 * @param slot 插槽函数
	 */
	void RegisterAcquirePlan(const AcquirePlanSlot& slot);
	/*!
	 * @brief 设置数据库访问地址
	 */
	void SetDBUrl(const char *dburl);
	/*!
	 * @brief 关联转台网络连接与观测系统
	 * @param ptr 网络连接
	 * @return
	 * 关联结果
	 */
	bool CoupleMount(TcpCPtr ptr);
	/*!
	 * @brief 关联相机网络连接与观测系统
	 * @param ptr    网络连接
	 * @param cid    相机标志
	 * @return
	 * 关联结果
	 */
	bool CoupleCamera(TcpCPtr ptr, const string& cid);
	/*!
	 * @brief 关联调焦网络连接与观测系统
	 * @param ptr  网络连接
	 * @param cid  相机标志
	 * @return
	 * 0: 失败
	 * 1: 成功, 且相机在线
	 * 2: 成功, 且相机不在线
	 */
	int CoupleFocus(TcpCPtr ptr, const string& cid);
	/*!
	 * @brief 解除调焦与观测系统的网络连接
	 */
	void DecoupleFocus();
	/*!
	 * @brief 网络消息: 来自总控服务器
	 */
	void NotifyAsciiProtocol(apbase proto);
	/*!
	 * @brief 声明焦点位置
	 * @param cid      相机标志
	 * @param position 焦点位置, 量纲: 微米
	 */
	void NotifyFocus(const string &cid, int position);
	/*!
	 * @brief 查看网络标志
	 */
	void GetID(string &gid, string &uid);
	/*!
	 * @brief 检查观测系统是否活跃
	 */
	bool IsAlive();
	/* 公共接口 */
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 网络通信 */
	/*!
	 * @brief 处理GWAC转台信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_mount(const long ptr, const long ec);
	/*!
	 * @brief 处理相机信息
	 * @param ptr 网络资源
	 * @param ec  错误代码. 0: 正确
	 */
	void receive_camera(const long ptr, const long ec);
	/*!
	 * @brief 处理来自转台的状态信息
	 * @param proto  信息主体
	 */
	void process_info_mount(apmount proto);
	/*!
	 * @brief 处理来自相机的状态信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_info_camera(apcamera proto, TCPClient* client);
	/*!
	 * @brief 启动或停止自动观测流程
	 */
	void process_autobs(bool mode = true);
	/*!
	 * @brief 改变望远镜零点位置
	 * @note
	 * 该指令将触发暂停观测计划及望远镜重新指向
	 */
	void process_homesync(aphomesync proto);
	/*!
	 * @brief 指向目标位置
	 * @param proto 通信协议
	 * @note
	 * 响应外界slewto协议
	 */
	void process_slewto(apslewto proto);
	/*!
	 * @brief 指向目标位置
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param epoch 历元
	 */
	void process_slewto(int coorsys, double coor1, double coor2);
	/*!
	 * @brief 引导跟踪
	 * @param proto
	 * @note
	 * 响应外界track协议
	 */
	void process_track(aptrack proto);
	/*!
	 * @brief 引导跟踪
	 * @param objid 目标名称
	 * @param line1 第一行数据
	 * @param line2 第二行数据
	 */
	void process_track(const string &objid, const string &line1, const string &line2);
	/*!
	 * @brief 中止指向和跟踪过程
	 * @param proto 通信协议
	 * @note
	 * 该指令将触发停止观测计划
	 */
	void process_abortslew();
	/*!
	 * @brief 复位
	 * @param proto 通信协议
	 * @note
	 * 该指令将触发停止观测计划
	 */
	void process_park();
	/*!
	 * @brief 导星
	 * @param proto 通信协议
	 * @note
	 * 依据导星量, 生成发送给转台的导星指令或同步零点指令
	 */
	void process_guide(apguide proto);
	/*!
	 * @brief 预指向, 消除长时间静止带来的阻尼
	 */
	void process_preslew();
	/*!
	 * @brief 手动曝光
	 * @param proto 通信协议
	 * @note
	 * 响应外界take_image协议
	 */
	void process_takeimage(aptakeimg proto);
	/*!
	 * @brief 中止曝光过程
	 */
	void process_abortimage();
	/*!
	 * @brief 统计FWHM
	 */
	void process_fwhm(apfwhm proto);
	/*!
	 * @brief 控制相机改变曝光状态
	 */
	void command_expose(int command);
	/*!
	 * @brief 为平场随机生成一个天顶附近位置
	 * @note
	 * - 上午方位角范围: [180, 360)
	 * - 下午方位角范围: [0, 180)
	 */
	void flat_orientation(double& azi, double& alt);
	/*!
	 * @brief 将转台位置发送给相机
	 */
	void notify_orientation();
	/*!
	 * @brief 解析观测计划, 并将其中目标信息发送给相机
	 */
	void notify_obsplan();
	/*!
	 * @brief 周期线程
	 * @note
	 * - 通过libcurl向数据库发送信息
	 * - 检查转台和相机网络连接的有效性
	 */
	void thread_cycle();
	/* 网络通信 */
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 消息机制 */
	/*!
	 * @brief 注册消息及响应函数
	 */
	void register_message();
	/*!
	 * @brief 消息ID == MSG_RECEIVE_MOUNT, 接收解析网络信息: 转台
	 */
	void on_receive_mount(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_RECEIVE_CAMERA, 接收解析网络信息: 相机
	 */
	void on_receive_camera(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_MOUNT, 断开连接: 转台
	 */
	void on_close_mount(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_CLOSE_CAMERA, 断开连接: 相机
	 */
	void on_close_camera(const long addr, const long);
	/*!
	 * @brief 消息ID == MSG_NEW_PROTOCOL, 处理由总控转发的、来自用户的通信协议
	 */
	void on_new_protocol(const long, const long);
	/*!
	 * @brief 消息ID == MSG_NEW_PLAN, 开始处理新的观测计划
	 */
	void on_new_plan(const long, const long);
	/*!
	 * @brief 消息ID == MSG_FLAT_RESLEW, 平场: 重新指向
	 */
	void on_flat_reslew(const long, const long);
	/* 消息机制 */
//////////////////////////////////////////////////////////////////////////////

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 观测计划与自动观测逻辑 */
	/*!
	 * @brief 依据设备状态及指令改变自动观测逻辑流程和状态
	 */
	void switch_state();
	/*!
	 * @brief 中断在执行计划
	 */
	void interrupt_plan();
	/*!
	 * @brief 线程: 申请观测计划
	 */
	void thread_acqplan();
	/* 观测计划与自动观测逻辑 */
//////////////////////////////////////////////////////////////////////////////

protected:
	/* 数据库 */
};
typedef boost::shared_ptr<ObservationSystem> ObsSysPtr;
extern ObsSysPtr make_obss(const string& gid, const string& uid);

#endif /* OBSERVATIONSYSTEM_H_ */
