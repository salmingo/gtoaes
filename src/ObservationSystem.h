/*
 * @file ObservationSystem.h 声明文件, 建立观测系统基类
 * @version 0.2
 * @date 2017-10-06
 * @note
 * - 构造数据结构, 供外界用户或数据库连接访问、查看观测系统工作状态
 * - 统一、分类处理GWAC系统和通用望远镜系统
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include <boost/container/stable_vector.hpp>
#include <boost/container/deque.hpp>
#include "MessageQueue.h"
#include "AsciiProtocol.h"
#include "tcpasio.h"
#include "ATimeSpace.h"
#include "ObservationPlan.h"

/*!
 * @struct InformationTelescope 封装观测系统中望远镜状态信息
 */
struct InformationTelescope {
	TELESCOPE_STATE state;	//< 工作状态
	int errcode;		//< 错误代码
	string utc;			//< 望远镜时标
	double ra, dec;		//< 指向坐标, 量纲: 角度. 赤道系
	double azi, ele;	//< 指向坐标, 量纲: 角度. 地平系
	double ora, odec;	//< 目标坐标, 量纲: 角度. 赤道系
	double dra, ddec;	//< 导星量, 量纲: 角度. 赤道系
	int slewing;		//< 望远镜到达稳定态的控制量

public:
	InformationTelescope& operator=(ascii_proto_telescope &x) {
		state   = (TELESCOPE_STATE) x.state;
		errcode = x.errcode;
		utc     = x.utc;
		ra      = x.ra;
		dec     = x.dec;
		azi     = x.azi;
		ele     = x.ele;

		return *this;
	}

	/*!
	 * @brief 改变目标位置
	 * @param r 赤经, 量纲: 角度
	 * @param d 赤纬, 量纲: 角度
	 */
	void SetObject(double r, double d) {
		ora = r, odec = d;
		dra = ddec = 0.0;
		BeginSlew();
	}

	/*!
	 * @brief 将目标位置设置为当前指向位置
	 */
	void Actual2Object() {
		ora = ra, odec = dec;
		dra = ddec = 0.0;
	}

	/*!
	 * @brief 导星
	 * @param dr 赤经修正量, 量纲: 角度
	 * @param dd 赤纬修正量, 量纲: 角度
	 */
	void Guide(double dr, double dd) {
		dra  += dr;
		ddec += dd;
		BeginSlew();
	}

	/*!
	 * @brief 重置导星量
	 */
	void ResetOffset() {
		dra = ddec = 0.0;
	}

	/*!
	 * @brief 开始指向过程
	 * @note
	 * 指向过程包括: 指向, 复位, 导星
	 */
	void BeginSlew() {
		slewing = 3;
	}

	/*!
	 * @brief 是否进入稳定态
	 * @return
	 * 稳定态标志
	 */
	bool StableArrive() {
		return (slewing && --slewing == 0);
	}

	void UnstableArrive() {
		if (slewing < 3) ++slewing;
	}
};
typedef InformationTelescope NFTele;
typedef boost::shared_ptr<NFTele> NFTelePtr;

/*!
 * @struct ObssCamera 为观测系统中的相机封装访问接口
 */
struct ObservationSystemCamera {// 相机标志与网络连接
	string  cid;		//< 相机标志
	bool    enabled;	//< 启用标志
	TcpCPtr tcptr;		//< 网络连接
	apcam   info;		//< 工作状态
	double  fwhm;		//< 半高全宽

public:
	ObservationSystemCamera(const string& id);
	virtual ~ObservationSystemCamera() {
		tcptr.reset();
		info.reset();
	}

	bool operator==(const string& id) {
		return cid == id;
	}

	bool operator!=(const string& id) {
		return cid != id;
	}

	bool operator==(const TcpCPtr ptr) {
		return tcptr == ptr;
	}

	bool operator!=(const TcpCPtr ptr) {
		return tcptr != ptr;
	}

	bool operator==(const TCPClient* ptr) {
		return (tcptr.get() == ptr);
	}

	bool operator!=(const TCPClient* ptr) {
		return (tcptr.get() != ptr);
	}
};
typedef ObservationSystemCamera ObssCamera;
typedef boost::shared_ptr<ObssCamera> ObssCamPtr;
typedef boost::container::stable_vector<ObssCamPtr> ObssCamVec;

struct ObservationSystemData {//< 观测系统数据
	bool		enabled;	//< 启用标志
	bool		running;	//< 工作标志
	bool		automode;	//< 自动工作模式
	OBSS_STATUS	state;		//< 系统工作状态
	/* 相机统计状态 */
	int			exposing;	//< 开始曝光的相机数量
	int			waitsync;	//< 等待平场重新曝光的相机数量
	int			waitflat;	//< 等待平场重新定位的相机数量

public:
	ObservationSystemData(const string& gid, const string& uid) {
		enabled = true;
		running = false;
		automode= false;
		state   = OBSS_ERROR;
		exposing= 0;
		waitsync= 0;
		waitflat= 0;
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
		return ((++waitflat + waitsync) == exposing);
	}

	bool leave_waitflat() {
		if (waitflat) --waitflat;
		return (!(waitflat || waitsync));
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
typedef ObservationSystemData ObssData;
typedef boost::shared_ptr<ObservationSystemData> ObssDataPtr;	//< 观测系统数据

class ObservationSystem: public MessageQueue {
public:
	ObservationSystem(const string& gid, const string& uid);
	virtual ~ObservationSystem();

public:
	/*!
	 * @function PlanFinished 声明ObservationSystem回调函数类型, 当观测计划终止时触发
	 * @param ObsPlanPtr 观测计划指针
	 * @note
	 * - 响应观测计划正常/异常结束
	 */
	typedef boost::signals2::signal<void (ObsPlanPtr)> PlanFinished;
	typedef PlanFinished::slot_type PlanFinishedSlot;
	/*!
	 * @function AcquireNewPlan 声明ObservationSystem回调函数类型, 申请新的观测计划时触发
	 * @param _1 组标志
	 * @param _2 单元标志
	 */
	typedef boost::signals2::signal<void (const string&, const string&)> AcquireNewPlan;
	typedef AcquireNewPlan::slot_type AcquireNewPlanSlot;

protected:
	/* 数据类型 */
	enum MSG_OBSS {// 总控服务消息
		MSG_RECEIVE_TELESCOPE = MSG_USER,	//< 收到望远镜信息
		MSG_RECEIVE_CAMERA,		//< 收到相机信息
		MSG_CLOSE_TELESCOPE,	//< 通用望远镜断开连接
		MSG_CLOSE_CAMERA,		//< 相机断开网络连接
		MSG_TELESCOPE_TRACK,	//< 望远镜进入跟踪状态
		MSG_OUT_SAFELIMIT,		//< 望远镜超出限位范围
		MSG_NEW_PROTOCOL,		//< 新的通信协议需要观测系统处理
		MSG_NEW_PLAN,			//< 新的观测计划需要执行
		MSG_FLAT_RESLEW,		//< 为平场重新指向
		MSG_LAST	//< 占位, 不使用
	};

	typedef boost::container::deque<apbase> apque;				//< ASCII通信协议队列
	typedef boost::container::stable_vector<TcpCPtr> TcpCVec;	//< 网络连接存储区

protected:
	boost::mutex mtx_queap_;	//< 互斥锁: 通信协议队列
	boost::mutex mtx_client_;	//< 互斥锁: 客户端
	boost::mutex mtx_camera_;	//< 互斥锁: 相机
	boost::mutex mtx_ats_;		//< 互斥锁: ats_
//////////////////////////////////////////////////////////////////////
	/* 成员变量 */
	string	gid_;		//< 组标志
	string	uid_;		//< 单元标志
	OBSS_TYPE obsstype_;	//< 观测系统类型
	apobsite  obsite_;		//< 测站位置
	double	minEle_;	//< 最小仰角, 量纲: 弧度
	double	tslew_;		//< 指向到位阈值
	double	tguide_;	//< 导星阈值
	OBSERVATION_DURATION odtype_;			//< 观测周期类型
	boost::posix_time::ptime tmLast_;		//< 时标, 记录: 系统创建时间, 最后一条网络连接断开时间
	boost::posix_time::ptime lastflat_;		//< 时标: 最后一次平场重新指向时间
	boost::posix_time::ptime lastguide_;	//< 时标: 最后一次导星的UTC时间
	boost::shared_ptr<AstroUtil::ATimeSpace> ats_;	//< 天文时-空转换接口

//////////////////////////////////////////////////////////////////////
	ObssDataPtr data_;		//< 观测系统共享性数据
	ObsPlanPtr	plan_now_;	//< 当前执行计划
	ObsPlanPtr	plan_wait_;	//< 等待执行计划
	NFTelePtr	nftele_;	//< 望远镜工作状态

//////////////////////////////////////////////////////////////////////
	/* 回调函数 */
	PlanFinished cb_plan_finished_;	//< 观测计划状态发生变更
	AcquireNewPlan  cb_acqnewplan_;	//< 申请新的观测计划

//////////////////////////////////////////////////////////////////////
	/* 网络资源 */
	boost::shared_array<char> bufrcv_;	//< 网络信息存储区
	AscProtoPtr ascproto_;				//< 通用ASCII编码协议解析接口
	apque que_ap_;						//< 缓存由GC转发的用户/数据库信息
	TcpCVec tcpc_client_;				//< 客户端网络连接
	TcpCPtr tcpc_telescope_;			//< 望远镜网络连接(通用望远镜或GWAC转台)
	ObssCamVec cameras_;				//< 相机访问接口

//////////////////////////////////////////////////////////////////////
	/* 线程 */
	threadptr thrd_idle_;	//< 线程: 系统空闲时, 定时检查是否有可执行观测计划
	threadptr thrd_client_;	//< 线程: 向客户端发送系统工作状态
	threadptr thrd_time_;	//< 线程: 更新时间信息

public:
///////////////////////////////////////////////////////////////////////////////
	/* 接口 */
	/*!
	 * @brief 启动观测系统
	 * @return
	 * 启动结果
	 */
	virtual bool StartService();
	/*!
	 * @brief 停用观测系统
	 */
	virtual void StopService();
	/*!
	 * @brief 检查标志是否一致
	 * @param gid 组标志
	 * @param uid 单元标志
	 * @return
	 * 标志匹配一致性
	 *  1: 两个标志完全一致
	 *  0: 两个标志弱一致
	 * -1: 标志不一致
	 */
	int IsMatched(const string& gid, const string& uid);
	/*!
	 * @brief 设置观测系统部署位置的地理信息
	 * @brief 测站名称
	 * @param lgt      地理经度, 东经为正, 量纲: 角度
	 * @param lat      地理纬度, 北纬为正, 量纲: 角度
	 * @param alt      海拔高度, 量纲: 米
	 * @param timezone 时区, 量纲: 小时
	 */
	void SetGeosite(const string& name, const double lgt, const double lat,
			const double alt, const int timezone);
	/*!
	 * @brief 设置最小仰角
	 * @param value 最小仰角, 量纲: 角度
	 * @note
	 * 低于该值则禁止望远镜指向
	 */
	void SetElevationLimit(double value);
	/*!
	 * @brief 获得观测系统标志
	 * @param gid 组标志
	 * @param uid 单元标志
	 */
	void GetID(string& gid, string& uid);
	/*!
	 * @brief 获得系统工作状态
	 * @return
	 * 观测系统工作状态
	 */
	OBSS_STATUS GetState();
	/*!
	 * @brief 获得望远镜信息
	 * @return
	 * 望远镜信息存储地址
	 */
	NFTelePtr GetTelescope();
	/*!
	 * @brief 获得相机控制接口
	 * @return
	 * 相机控制接口
	 * @note
	 * 封装相机控制、相机信息接口
	 */
	ObssCamVec& GetCameras();
	/*!
	 * @brief 获得执行计划的标志
	 * @param sn 计划编号
	 * @return
	 * 计划开始执行时间
	 */
	const char* GetPlan(int& sn);
	/*!
	 * @brief 光联客户端与观测系统
	 * @param ptr 网络连接
	 * @return
	 * 关联结果
	 */
	bool CoupleClient(TcpCPtr ptr);
	/*!
	 * @brief 解除客户端与观测系统的关联
	 * @param ptr 网络连接
	 * @return
	 * 解联结果
	 */
	bool DecoupleClient(TcpCPtr ptr);
	/*!
	 * @brief 关联望远镜网络连接与观测系统
	 * @param ptr 网络连接
	 * @return
	 * 关联结果
	 */
	virtual bool CoupleTelescope(TcpCPtr ptr);
	/*!
	 * @brief 解除望远镜网络连接与观测系统的关联
	 * @param ptr 网络连接
	 */
	void DecoupleTelescope(TcpCPtr ptr);
	/*!
	 * @brief 关联相机网络连接与观测系统
	 * @param ptr    网络连接
	 * @param cid    相机标志
	 * @return
	 * 关联结果
	 */
	bool CoupleCamera(TcpCPtr ptr, const string& cid);
	/*!
	 * @brief 检查是否存在与硬件设备的有效关联
	 * @return
	 * true:  与任意硬件设备间存在网络连接
	 * false: 未建立与任意硬件设备的网络连接
	 */
	virtual bool HasAnyConnection();
	/* 处理GC转发的、来自客户端或数据库的信息 */
	/*!
	 * @brief 缓冲消息
	 * @param body 消息主体
	 */
	void NotifyProtocol(apbase body);
	/*!
	 * @brief 接收新的观测计划
	 * @param plan 以通信协议格式记录的观测计划
	 * @note
	 * - plan指向空指针, 代表无可执行计划
	 * - plan非空时, 该计划需要立即执行: 在GC中已完成检测
	 */
	void NotifyPlan(ObsPlanPtr plan);
	/*!
	 * @brief 计算观测计划的相对优先级
	 * @param plan 观测计划
	 * @param now  当前UTC时间
	 * @return
	 * 相对优先级
	 * @note
	 * 相对优先级由系统可用状态、目标位置、计划优先级与系统当前计划优先级共同决定,
	 * 相对优先级大于0时, 其数值越大, 该系统获得该观测计划执行权的概率越高.
	 * - 系统不可用: 相对优先级==0
	 * - 目标位置超出限位阈值: 相对优先级==0
	 * - 相对优先级 = priority - 当前计划优先级
	 */
	int PlanRelativePriority(apappplan plan, boost::posix_time::ptime& now);
	/*!
	 * @brief 注册观测计划终止回调函数
	 * @param slot 插槽函数
	 */
	void RegisterPlanFinished(const PlanFinishedSlot& slot);
	/*!
	 * @brief 注册申请观测计划回调函数
	 * @param slot 插槽函数
	 */
	void RegisterAcquireNewPlan(const AcquireNewPlanSlot& slot);

protected:
///////////////////////////////////////////////////////////////////////////////
	/*!
	 * @brief 由网络连接地址查找对应的相机访问接口
	 * @param ptr 网络资源地址
	 * @return
	 * 对应的相机访问接口
	 */
	ObssCamPtr find_camera(TCPClient *ptr);
	/*!
	 * @brief 由相机标志查找对应的相机访问接口
	 * @param cid 相机标志
	 * @return
	 * 对应的相机访问接口
	 */
	ObssCamPtr find_camera(const string &cid);
	/*!
	 * @brief 为平场随机生成一个天顶附近位置
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param epoch 历元
	 * @note
	 * - 上午方位角范围: [180, 360)
	 * - 下午方位角范围: [0, 180)
	 */
	void flat_position(double& ra, double& dec, double& epoch);
	/*!
	 * @brief 检查坐标是否在安全范围内
	 * @param ra   赤经, 量纲: 角度
	 * @param dec  赤纬, 量纲: 角度
	 * @return
	 * 坐标在安全范围内返回true
	 * @note
	 * - 检查坐标在当前时间的高度角是否大于阈值
	 */
	bool safe_position(double ra, double dec);
	/*!
	 * @brief 计算修正儒略日对应的民用晨昏时
	 * @param mjd   当前本地时对应的修正儒略日
	 * @param dawn  民用晨光始, 量纲: 小时
	 * @param dusk  民用昏影终, 量纲: 小时
	 */
	void civil_twilight(double mjd, double& dawn, double& dusk);
	/*!
	 * @brief 计算x与当前可用观测计划的相对优先级
	 * @param x 待评估优先级
	 * @return
	 * 相对优先级
	 */
	virtual int relative_priority(int x) = 0;
	/*!
	 * @brief 中止观测计划执行过程
	 * @param state 计划中止方式
	 * @return
	 * 被中止计划立即结束则返回true, 否则返回false
	 */
	bool interrupt_plan(OBSPLAN_STATUS state = OBSPLAN_INT);
	/*!
	 * @brief 改变观测计划工作状态
	 * @param plan   观测计划
	 * @param state  工作状态
	 * @return
	 * 状态改变结果
	 */
	bool change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS state);
	/*!
	 * @brief 依据观测系统类型, 修正观测计划工作状态
	 * @param plan       观测计划
	 * @param old_state  观测计划当前状态
	 * @param new_state  新的状态
	 * @return
	 * 状态改变结果
	 */
	virtual bool change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS old_state, OBSPLAN_STATUS new_state) = 0;
	/*!
	 * @brief 解析观测计划, 尤其是其中的曝光参数, 形成ascii_proto_object并发送给对应相机
	 * @return
	 * 观测计划解析结果
	 */
	virtual void resolve_obsplan() = 0;
	/*!
	 * @brief 将格式化字符串信息发送给相机
	 * @param s    格式化字符串
	 * @param n    待发送字符串长度, 量纲: 字节
	 * @param cid  相机标志
	 */
	void write_to_camera(const char *s, const int n, const char *cid = NULL);
	/*!
	 * @brief 控制相机曝光操作
	 * @param cid 相机标志
	 * @param cmd 曝光指令
	 */
	void command_expose(const string &cid, EXPOSE_COMMAND cmd);
	/*!
	 * @brief 检查切换系统工作状态
	 */
	void switch_state();
	/*!
	 * @brief 检查望远镜是否指向到位
	 * @return
	 * 望远镜指向到位标志
	 * @note
	 * GWAC系统与通用系统稳定度要求不同
	 */
	bool slew_arrived();

protected:
///////////////////////////////////////////////////////////////////////////////
	/* 多线程 */
	/*!
	 * @brief 线程: 未执行观测计划时执行一些操作
	 */
	void thread_idle();
	/*!
	 * @brief 线程: 定时向客户端发送系统工作状态
	 */
	void thread_client();
	/*!
	 * @brief 线程: 计算当前时间的天光特性
	 * @note
	 * 依据太阳高度角设置tdflat_, 以确定在自动模式下可执行的观测计划类型:
	 * tdflat_  == true: 可执行flat图像类型计划
	 */
	void thread_time();

protected:
///////////////////////////////////////////////////////////////////////////////
	/*!
	 * @brief 处理相机信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_camera(const long client, const long ec);
	/* 消息响应函数 */
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_message();
	/*!
	 * @brief 响应消息MSG_RECEIVE_TELESCOPE
	 */
	void on_receive_telescope(const long, const long);
	/*!
	 * @brief 响应消息MSG_RECEIVE_CAMERA
	 * @param client 网络连接
	 */
	void on_receive_camera(const long client, const long);
	/*!
	 * @brief 响应消息MSG_CLOSE_TELESCOPE
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_telescope(const long client = 0, const long ec = 0);
	/*!
	 * @brief 响应消息MSG_CLOSE_CAMERA
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确; 其它: 错误
	 */
	void on_close_camera(const long client, const long ec);
	/*!
	 * @brief 响应消息MSG_TELESCOPE_TRACK
	 */
	void on_telescope_track(const long, const long);
	/*!
	 * @brief 响应消息MSG_OUT_SAFELIMIT
	 */
	void on_out_safelimit(const long, const long);
	/*!
	 * @brief 处理由GeneralControl转发的ascii_proto_xxx通信协议
	 */
	void on_new_protocol(const long, const long);
	/*!
	 * @brief 处理接收的观测计划
	 */
	void on_new_plan(const long, const long);
	/*!
	 * @brief 为采集平场重新指向至中天附近位置
	 */
	void on_flat_reslew(const long, const long);

protected:
///////////////////////////////////////////////////////////////////////////////
	/* 处理由GeneralControl转发的控制协议 */
	/* 响应处理消息: MSG_NEW_PROTOCOL */
	/*!
	 * @brief 望远镜搜索零点
	 * @note
	 * 在观测计划执行过程中可能拒绝该指令
	 */
	virtual bool process_findhome();
	/*!
	 * @brief 改变望远镜零点位置
	 * @note
	 * 该指令将触发暂停观测计划及望远镜重新指向
	 */
	virtual bool process_homesync(aphomesync proto);
	/*!
	 * @brief 指向目标位置
	 * @param proto 通信协议
	 * @note
	 * 响应外界slewto协议
	 */
	bool process_slewto(apslewto proto);
	/*!
	 * @brief 指向目标位置
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param epoch 历元
	 */
	virtual bool process_slewto(double ra, double dec, double epoch) = 0;
	/*!
	 * @brief 中止指向和跟踪过程
	 * @param proto 通信协议
	 * @note
	 * 该指令将触发停止观测计划
	 */
	virtual bool process_abortslew();
	/*!
	 * @brief 复位
	 * @param proto 通信协议
	 * @note
	 * 该指令将触发停止观测计划
	 */
	virtual bool process_park();
	/*!
	 * @brief 导星
	 * @param proto 通信协议
	 */
	void process_guide(apguide proto);
	/*!
	 * @brief 执行导星
	 * @param dra  赤经导星量
	 * @param ddec 赤纬导星量
	 * @return
	 * 导星结果
	 * @note
	 * 继承类中可能修改导星量. GWAC系统通信协议中, 最大导星量绝对值为9999角秒
	 */
	virtual bool process_guide(double &dra, double &ddec) = 0;
	/*!
	 * @brief 开关镜盖
	 * @note
	 * 关闭镜盖指令可能触发停止观测计划
	 */
	virtual bool process_mcover(apmcover proto);

	/*!
	 * @brief 通知望远镜改变焦点位置
	 * @param proto 通信协议
	 */
	virtual bool process_focus(apfocus proto);
	/*!
	 * @brief 通过数据处理统计得到的FWHM, 通知望远镜调焦
	 * @param proto 通信协议
	 */
	virtual bool process_fwhm(apfwhm proto);

	/*!
	 * @brief 手动曝光
	 * @param proto 通信协议
	 */
	void process_takeimage(aptakeimg proto);
	/*!
	 * @brief 中止曝光过程
	 * @param proto 通信协议
	 * @note
	 * 当cid.empty()判为真或适用所有相机时, 该指令将触发停止观测计划
	 */
	void process_abortimage(const string &cid);
	/*!
	 * @brief 删除观测计划
	 * @param plan_sn 计划编号
	 * @note
	 * 该指令将触发停止观测计划
	 */
	void process_abortplan(apabtplan proto);
	/*!
	 * @brief 启动自动化天文观测流程
	 */
	void process_start(apstart proto);
	/*!
	 * @brief 停止自动化天文观测流程
	 * @note
	 * 该指令将触发停止观测计划
	 */
	void process_stop(apstop proto);
	/*!
	 * @brief 启用指定相机或观测系统
	 * @param proto 通信协议
	 */
	void process_enable(apenable proto);
	/*!
	 * @brief 禁用指定相机或观测系统
	 * @param proto 通信协议
	 * @note
	 * 该指令可能触发停止观测计划
	 */
	void process_disable(apdisable proto);

protected:
///////////////////////////////////////////////////////////////////////////////
	/* 处理来自通用观测系统下位机的信息 */
	/*!
	 * @brief 处理望远镜状态信息
	 * @param proto 通信协议
	 */
	void process_info_telescope(aptele proto);
	/*!
	 * @brief 处理调焦器状态信息
	 * @param proto 通信协议
	 */
	void process_info_focus(apfocus proto);
	/*!
	 * @brief 处理镜盖状态信息
	 * @param proto 通信协议
	 */
	void process_info_mcover(apmcover proto);
	/*!
	 * @brief 处理相机状态信息
	 * @param camptr 相机封装访问接口
	 * @param proto  通信协议
	 */
	void process_info_camera(ObssCamPtr camptr, apcam proto);
};
typedef boost::shared_ptr<ObservationSystem> ObsSysPtr;

/*!
 * @brief 将ObservationSystem继承类的boost::shared_ptr型指针转换为ObsSPtr类型
 * @param proto 观测系统指针
 * @return
 * ObsSPtr类型指针
 */
template <class T>
ObsSysPtr to_obss(T obss) {
	return boost::static_pointer_cast<ObservationSystem>(obss);
}

/*!
 * @brief 将ObsSPtr类型指针转换为其继承类的boost::shared_ptr型指针
 * @param proto 观测系统指针
 * @return
 * ObsSPtr继承类指针
 */
template <class T>
boost::shared_ptr<T> from_obss(ObsSysPtr obss) {
	return boost::static_pointer_cast<T>(obss);
}

#endif /* OBSERVATIONSYSTEM_H_ */
