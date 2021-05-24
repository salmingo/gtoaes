/*
 * @file ObservationSystem.h 封装管理观测系统
 * @author 卢晓猛
 * @date 2017-1-29
 * @version 0.3
 *
 * @date 2017-05-07
 * @version 0.4
 * @date 2017-05-17
 * @note
 * - 导星方向(以期望运动方向定义导星方向): 向东: -; 向西: +; 向北: +； 向南: -
 * - 所有坐标全采用J2000
 * - 转台反馈位置坐标系为J2000
 *
 * @version 0.5
 * @date 2017-06-03
 * @note
 * - 增加导星延时, 300秒内最多执行一次导星
 * - 增加相机状态CAMERA_WAIT_RESUME
 *
 * @versionn 0.6
 * @date 2020-09-22
 * @note
 * - 初始执行观测计划时, 仅向FFoV发送曝光指令
 * - 当数据处理反馈位置偏差小于阈值(5角分)时, 通知所有相机曝光
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include <vector>
#include <list>
#include <string.h>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "ATimeSpace.h"
#include "msgque_base.h"
#include "tcp_asio.h"
#include "asciiproto.h"
#include "mountproto.h"

using boost::posix_time::ptime;

/*
enum CameraState {// 相机工作状态
	CAMERA_ERROR,		//< 错误
	CAMERA_IDLE,		//< 空闲
	CAMERA_EXPOSE,		//< 曝光过程
	CAMERA_COMPLETE,	//< 曝光正常结束
	CAMERA_PAUSE,		//< 暂停: 外界指令触发
	CAMERA_PAUSE_TIME,	//< 暂停: 帧间延时或曝光序列之前
	CAMERA_PAUSE_FLAT,	//< 暂停: 完成一帧平场后等待转台重新指向
};
*/

enum CAMCTL_STATUS {// 相机控制状态
	CAMCTL_ERROR,		// 错误
	CAMCTL_IDLE,		// 空闲
	CAMCTL_EXPOSE,		// 曝光过程中
	CAMCTL_COMPLETE,	// 已完成曝光
	CAMCTL_ABORT,		// 已中止曝光
	CAMCTL_PAUSE,		// 已暂停曝光
	CAMCTL_WAIT_TIME,	// 等待曝光流传起始时间到达
	CAMCTL_WAIT_FLAT,	// 平场间等待--等待转台重新指向
	CAMCTL_LAST			// 占位
};

enum MOUNT_STATE {// 转台状态
	MOUNT_FIRST = -1,	//< 占位, 无效态
	MOUNT_ERROR,		//< 错误
	MOUNT_FREEZE,		//< 静止
	MOUNT_HOMING,		//< 找零
	MOUNT_HOMED,		//< 找到零点
	MOUNT_PARKING,		//< 复位
	MOUNT_PARKED,		//< 已复位
	MOUNT_SLEWING,		//< 指向
	MOUNT_TRACKING,		//< 跟踪
	MOUNT_LAST			//< 占位, 无效态
};

static std::string mount_state_desc[] = {// 转台状态描述
	"Error",
	"Freeze",
	"Homing",
	"Homed",
	"Parking",
	"Parked",
	"Slewing",
	"Tracking"
};

enum MIRRORCOVER_STATE {// 镜盖状态
	MC_FIRST = -3,
	MC_CLOSING,
	MC_CLOSED,
	MC_UNKNOWN,
	MC_OPEN,
	MC_OPENING,
	MC_LAST
};

static std::string mc_state_desc[] = {// 镜盖状态描述
	"Closing",
	"Closed",
	"unknown",
	"Open",
	"Opening"
};

class ObservationSystem: public msgque_base {
public:
	ObservationSystem(const std::string& gid, const std::string& uid);
	virtual ~ObservationSystem();

public:
	typedef boost::unique_lock<boost::mutex> mutex_lock;
	typedef boost::shared_ptr<boost::thread> threadptr;

	struct one_ascproto {// ASCII网络消息
		std::string type;	//< 消息类型
		apbase      body;	//< 消息主体
	};
	typedef boost::shared_ptr<one_ascproto> protoptr;
	typedef std::list<protoptr> protovec;

	/* 声明数据类型 */
	struct camera_info {// 相机状态信息
		int         bitdepth;	//< A/D位数
		int         readport;	//< 读出端口档位
		int         readrate;	//< 读出速度档位
		int         vrate;		//< 垂直转移速度档位
		int         gain;		//< 增益档位
		bool        emsupport;	//< 是否支持EM模式
		bool        emon;		//< 启用EM模式
		int         emgain;		//< EM增益
		int         coolset;	//< 制冷温度, 量纲: 摄氏度
		int         coolget;	//< 芯片温度, 量纲: 摄氏度
		std::string utc;		//< 时标, 格式: CCYY-MM-DDThh:mm:ss.ssssss
		int         state;		//< 工作状态. 0: 错误; 1: 空闲; 2: 曝光中; 3: 等待中
		int         freedisk;	//< 空闲磁盘空间, 量纲: MB
		std::string filepath;	//< 文件存储路径
		std::string filename;	//< 文件名称
		std::string objname;	//< 当前观测目标名称
		IMAGE_TYPE  imgtype;	//< 图像类型
		double      expdur;		//< 当前曝光时间
		std::string filter;		//< 滤光片
		int         frmtot;		//< 曝光总帧数
		int         frmnum;		//< 曝光完成帧数
		int         focus;		//< 焦点位置
		int         mcover;		//< 镜盖开关状态
		bool        validexp;		//< 有效曝光

	private:
		boost::mutex mtx;

	public:
		camera_info() {
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
			imgtype = IMGTYPE_LAST;
			expdur = -1.0;
			frmtot = frmnum = -1;
			focus  = INT_MIN;
			mcover = -1;
			validexp = false;
		}

		camera_info& operator=(const ascproto_camera_info& proto) {
			mutex_lock lck(mtx);

			if (proto.state == CAMCTL_COMPLETE) validexp = proto.frmnum >= 1 && proto.frmnum != frmnum;

			if (proto.bitdepth > 0)			bitdepth	= proto.bitdepth;
			if (proto.readport >= 0)		readport	= proto.readport;
			if (proto.readrate >= 0)		readrate	= proto.readrate;
			if (proto.vrate >= 0)			vrate		= proto.vrate;
			if (proto.gain >= 0)			gain		= proto.gain;
			if (proto.coolset <= 0)			coolset		= proto.coolset;
			if (proto.coolget <= 0)			coolget		= proto.coolget;
			if (!proto.utc.empty())			utc			= proto.utc;
			if (proto.state >= 0)           state       = proto.state;
			if (proto.freedisk >= 0)		freedisk	= proto.freedisk;
			if (!proto.objname.empty())     objname     = proto.objname;
			if (!proto.filepath.empty())	filepath	= proto.filepath;
			if (!proto.filename.empty())	filename	= proto.filename;
			if (proto.expdur >= 0.0)		expdur		= proto.expdur;
			if (!proto.filter.empty())		filter		= proto.filter;
			if (proto.frmtot >= 0)			frmtot		= proto.frmtot;
			if (proto.frmnum >= 0)			frmnum		= proto.frmnum;
			if (proto.emsupport && proto.emgain >= 0) {
				emsupport	= proto.emsupport;
				emon 		= proto.emon;
				emgain		= proto.emgain;
			}

			return *this;
		}
	};

	/* 累积偏差量说明: 2017-05-16
	 * 当累积偏差量超过2°时, 触发一次同步零点操作, 避免下一次指向时偏差太大.
	 */
	struct mount_info {// 转台状态信息
		bool        ready;		//< 是否完成准备
		int         state;		//< 工作状态
		std::string utc;		//< UTC时标
		/* 位置量纲: 角度 */
		int coorsys;	//< 目标坐标系. 1: RA/DEC; 2: HA/DEC; ...
		int tmflag;		//< 接收到坐标时的时标
		double      ra00, ha00, dc00;	//< 转台当前指向平位置, J2000
		double      ora00, oha00, odc00;//< 转台目标平位置, J2000
		double      dra, ddc;			//< 累积偏差量, 量纲: 角度

	public:
		mount_info() {
			ready = true;
			state = -1;
			utc   = "";
			coorsys = 1;
			tmflag  = 0;
			ra00 = ha00 = dc00 = 0.0;
			ora00 =  oha00 = odc00 = -1000.0;
			dra = ddc = 0.0;
		}

		/*!
		 * @brief 目标位置置为当前转台位置
		 */
		void actual2object() {
			coorsys = 1;
			ora00 = ra00;
			odc00 = dc00;
			dra = ddc = 0.0;
		}

		/*!
		 * @brief 设置目标位置
		 * @param ra J2000赤经, 量纲: 角度
		 * @param dc J2000赤纬, 量纲: 角度
		 */
		void set_object(double ra, double dc) {
			coorsys = 1;
			ora00 = ra;
			odc00 = dc;
			dra = ddc = 0.0;
		}

		/*!
		 * @brief 设置目标位置
		 * @param ha J2000时角, 量纲: 角度
		 * @param dc J2000赤纬, 量纲: 角度
		 */
		void set_hd(double ha, double dc) {
			coorsys = 2;
			oha00 = ha;
			odc00 = dc;
			dra = ddc = 0.0;
		}

		/*!
		 * @brief 累计偏差量
		 * @param r  新的赤经偏差量
		 * @param d  新的赤纬偏差量
		 * @return
		 * 累计偏差量是否超出阈值(2度)
		 */
		bool add_offset(double r, double d) {
			dra += r;
			ddc += d;
			return (fabs(dra) > 2.0 || fabs(ddc) > 2.0);
		}

		void reset_object() {
			ora00 = odc00 = -1000.0;
		}

		void reset_offset() {
			dra = ddc = 0.0;
		}
	};

	struct system_state {// 系统状态
		bool running;	//< 运行模式. 当running为true时, 需执行自检流程; 当为false时, 需执行关机流程
		int  slewing;	//< 指向目标
		int  guiding;	//< 导星
		int  parking;	//< 复位
		int  exposing;	//< 相机曝光标志.    0: 未曝光
		int  lighting;	//< 相机天光曝光标志. 0: 未曝光
		int  flatting;	//< 相机平场标志
		int  waitflat;	//< 相机等待平场标志. 等待平场时, 需要重新定位转台
		bool validflat;	//< 完成一次有效平场
		ptime lastflat;	//< 最后一次平场模式下改变转台位置的时间

	private:
		boost::mutex mtx;	//< 互斥锁

	public:
		system_state() {
			running  = false;	// 默认为未运行模式
			slewing  = 0;
			guiding  = 0;
			parking  = 0;
			exposing = 0;
			lighting = 0;
			flatting = 0;
			waitflat = 0;
			validflat= false;
		}

		void begin_slew() {
			slewing = 10;
		}

		void begin_guide() {
			guiding = 10;
		}

		void begin_park() {
			parking = 10;
		}

		bool stable_slew() {
			return (slewing && --slewing == 0);
		}

		bool stable_guide() {
			return (guiding && --guiding == 0);
		}

		bool stable_park() {
			return (parking && --parking == 0);
		}

		void enter_tracking() {
			mutex_lock lck(mtx);
			slewing = 0;
			guiding = 0;
			parking = 0;
		}

		void enter_expose(int imgtype) {// 单相机进入曝光状态
			if (imgtype >= IMGTYPE_BIAS && imgtype < IMGTYPE_LAST) {
				mutex_lock lck(mtx);
				++exposing;
				if (imgtype > IMGTYPE_DARK)  ++lighting;
				if (imgtype == IMGTYPE_FLAT) ++flatting;
			}
		}

		bool leave_expose(int imgtype) {// 单相机离开曝光状态
			if (imgtype >= IMGTYPE_BIAS && imgtype < IMGTYPE_LAST) {
				mutex_lock lck(mtx);
				if (lighting && imgtype > IMGTYPE_DARK)  --lighting;
				if (imgtype == IMGTYPE_FLAT && flatting) --flatting;
				if (exposing && --exposing == 0) {
					waitflat = 0;
					return true;
				}
			}
			return false;
		}

		bool enter_waitflat() {// 单相机进入平场等待状态: CAMERA_WAIT_FLAT
			return (++waitflat == flatting);
		}

		void leave_waitflat() {// 单相机离开平场等待状态
			if (waitflat) --waitflat;
		}
	};

	typedef boost::shared_ptr<camera_info> camnfptr;
	typedef boost::shared_ptr<mount_info>  mntnfptr;
	typedef boost::shared_ptr<ascproto_object_info> planptr;

	enum {
		CAMERA_TYPE_FFOV = 1,
		CAMERA_TYPE_JFOV
	};

	struct one_camera {
		int			type;	//< 1: FFoV; 2: JFoV. FFoV的编号是5的倍数
		tcpcptr     tcpcli;	//< 网络连接
		std::string id;		//< 相机编号
		camnfptr    info;	//< 工作状态
		double      fwhm;	//< 半高全宽

	public:
		one_camera(tcpcptr client, std::string cid) {
			type = std::stoi(cid) % 5 == 0 ? CAMERA_TYPE_FFOV : CAMERA_TYPE_JFOV;
			tcpcli = client;
			id = cid;
			info = boost::make_shared<camera_info>();
			fwhm = 0.0;
		}

		virtual ~one_camera() {
			tcpcli.reset();
			info.reset();
		}
	};
	typedef boost::shared_ptr<one_camera> camptr;
	typedef std::vector<camptr> camvec;		// 封装观测系统的所有相机资源

	/* 常量 */
	enum MSG_OBSS {// 定义消息
		MSG_RECEIVE_CAMERA = MSG_USER,	//< 消息: 收到相机信息
		MSG_CLOSE_CAMERA,				//< 消息: 相机远程主机断开
		MSG_MOUNT_CHANGED,				//< 消息: 转台状态发生变化
		MSG_NEW_PLAN,					//< 消息: 收到新的观测计划
		MSG_OUT_LIMIT,					//< 消息: 转台超出限位区域
		MSG_NEW_PROTOCOL,				//< 消息: 收到新的网络消息
		MSG_FLAT_RESLEW,				//< 消息: 为拍摄平场重新定位
		MSG_WAIT_OVER,					//< 消息: 等待线程已经结束
		MSG_OS_END						//< 消息: 观测系统结束消息占位符
	};

	enum EXPMODE {
		EXPMODE_GUIDE,
		EXPMODE_JOINT,
		EXPMODE_ALL
	};

public:
	/*!
	 * @brief 启动观测系统
	 * @return
	 * 观测系统启动结果
	 */
	bool Start();
	/*!
	 * @brief 查看时间戳
	 * @return
	 * 最后一次收到转台或相近信息的时间
	 */
	int get_timeflag();
	/*!
	 * @brief 检查观测系统唯一性标志是否与输入条件一致
	 * @param gid 组标志
	 * @param uid  单元标志
	 * @return
	 * 匹配一致则返回true, 否则返回false
	 */
	bool is_matched(const std::string& gid, const std::string& uid);
	/*!
	 * @brief 设置观测系统部署位置的地理信息
	 * @brief 测站名称
	 * @param lgt 地理经度, 东经为正, 量纲: 角度
	 * @param lat 地理纬度, 北纬为正, 量纲: 角度
	 * @param alt 海拔高度, 量纲: 米
	 */
	void set_geosite(const std::string& name, double lgt, double lat, double alt);
	/* 设备与系统的关系 */
	/*!
	 * @brief 关联观测系统与转台网络资源
	 * @param client 转台网络资源
	 * @note
	 * 同一转台网络资源, 可与多个观测系统建立关联
	 */
	void CoupleMount(tcpcptr& client);
	/*!
	 * @brief 解联观测系统与转台网络资源
	 * @param client 转台网络资源
	 */
	void DecoupleMount(tcpcptr& client);
	/*!
	 * @brief 关联观测系统与相机网络资源
	 * @param client 相机网络资源
	 * @param gid    组标志
	 * @param uid    单元标志
	 * @param cid    相机标志
	 * @note
	 * 同一相机网络资源, 仅且仅可与一个观测系统建立关联
	 */
	bool CoupleCamera(tcpcptr client, const std::string gid, const std::string uid, const std::string cid);
	/*!
	 * @brief 解联观测系统与相机网络资源
	 * @param param 相机网络资源
	 * @note
	 * 处理异常事件: 观测系统已关联相机, 但相机的关闭事件由GeneralControl触发
	 */
	bool DecoupleCamera(const long param);
	/*!
	 * @brief 关联观测系统与转台附属设备网络资源
	 * @param client 转台网络资源
	 * @note
	 * 同一转台网络资源, 可与多个观测系统建立关联
	 */
	void CoupleMountAnnex(tcpcptr& client);
	/*!
	 * @brief 解联观测系统与转台附属设备网络资源
	 * @param client 转台网络资源
	 */
	void DecoupleMountAnnex(tcpcptr& client);

public:
	/* 响应网络信息 */
	/*!
	 * @brief 向缓冲区添加一条收到的网络信息
	 * @param type  消息类型
	 * @param proto 消息主体
	 */
	void append_protocol(const char* type, apbase proto);
	/*!
	 * @brief 曝光
	 * @param proto 协议主体
	 */
	void notify_take_image(apbase proto);
	/*!
	 * @brief 终止指向
	 */
	void notify_abort_slew();
	/*!
	 * @brief GWAC观测计划
	 * @param proto 协议主体
	 */
	void notify_append_gwac(apbase proto);
	/*!
	 * @brief 指向
	 * @param proto 协议主体
	 */
	void notify_slewto(apbase proto);
	/*!
	 * @brief 导星
	 * @param proto 协议主体
	 */
	void notify_guide(apbase proto);
	/*!
	 * @brief 手动调焦
	 * @param proto 协议主体
	 */
	void notify_focus(apbase proto);
	/*!
	 * @brief 自动调焦
	 * @param proto 协议主体
	 */
	void notify_fwhm(apbase proto);
	/*!
	 * @brief 终止曝光
	 * @param proto 协议主体
	 */
	void notify_abort_image(apbase proto);
	/*!
	 * @brief 同步零点
	 * @param proto 协议主体
	 */
	void notify_home_sync(apbase proto);
	/*!
	 * @brief 启动自动观测
	 */
	void notify_start_gwac();
	/*!
	 * @brief 停止自动观测
	 */
	void notify_stop_gwac();
	/*!
	 * @brief 搜索零点
	 */
	void notify_find_home();
	/*!
	 * @brief 复位
	 */
	void notify_park();
	/*!
	 * @brief 开/关镜盖
	 * @param proto 协议主体
	 */
	void notify_mcover(apbase proto);

public:
	/* 转台反馈状态 */
	/*!
	 * @brief 转台工作状态
	 */
	void notify_mount_state(int state);
	/*!
	 * @brief 转台时标
	 * @param body 协议主体
	 */
	void notify_mount_utc(boost::shared_ptr<mntproto_utc> proto);
	/*!
	 * @brief 转台位置
	 * @param body 协议主体
	 */
	void notify_mount_position(boost::shared_ptr<mntproto_position> proto);
	/*!
	 * @brief 焦点位置
	 * @param body 协议主体
	 */
	void notify_mount_focus(boost::shared_ptr<mntproto_focus> proto);
	/*!
	 * @brief 镜盖开关状态
	 * @param body 协议主体
	 */
	void notify_mount_mcover(boost::shared_ptr<mntproto_mcover> proto);

private:
	/*!
	 * @breif 将常规信息发送给指定相机
	 * @param data  协议内容
	 * @param n     协议长度, 量纲: 字节
	 * @param cid   相机标志. 为空时发送给所有相机
	 */
	void WriteToCamera(const char* data, const int n, const std::string& cid);
	/*!
	 * @brief 向相机发送控制指令
	 * @param cmd   曝光指令
	 * @param mode  模式
	 *              0: 导星相机FFoV
	 *              1: 拼接相机JFoV
	 *              2: 所有相机
	 */
	void WriteToCamera(EXPOSE_COMMAND cmd, EXPMODE mode);
	/*!
	 * @brief 将观测目标信息发送给指定相机
	 * @param nf   目标信息
	 * @param cid  相机标志. 为空时发送给所有相机
	 */
	void WriteObject(boost::shared_ptr<ascproto_object_info> nf, const std::string &cid);
	/*!
	 * @brief 检查相机是否处于曝光状态
	 * @param cid 相机编号
	 * @return
	 * -1: 相机不可用
	 *  0: 未曝光
	 *  1: 曝光中, 但不需要天光
	 *  2: 曝光中, 且需要天光
	 */
	int IsExposing(const std::string& cid);
	/*!
	 * @brief 检查是否有任一相机处于曝光状态
	 * @return
	 * @return
	 * -1: 相机不可用
	 *  0: 未曝光
	 *  1: 曝光中, 但不需要天光
	 *  2: 曝光中, 且需要天光
	 */
	int AnyExposing();
	/*!
	 * @brief 检测输入位置(J2000)在指定时间是否处于安全工作区
	 * @param ha0 输入时角, 量纲: 角度
	 * @param dc0 输入赤纬, 量纲: 角度
	 * @return
	 * 若(ha0, dc0)在指定时间位于安全区则返回true, 否则返回false
	 */
	bool SafePositionHD(double ha0, double dc0);
	/*!
	 * @brief 检测输入位置(J2000)在指定时间是否处于安全工作区
	 * @param ra0 输入赤经, 量纲: 角度
	 * @param dc0 输入赤纬, 量纲: 角度
	 * @param at  检测时间. 若为空则取当前系统时间
	 * @return
	 * 若(ra0, dc0)在指定时间位于安全区则返回true, 否则返回false
	 */
	bool SafePosition(double ra0, double dc0, ptime *at = NULL);
	/*!
	 * @brief 生成天顶附近随机位置, 用于自动平场流程中转台重新指向
	 * @param ra 赤经, 量纲: 角度
	 * @param dc 赤纬, 量纲: 角度
	 */
	void RandomZenith(double &ra, double &dc);
	/*!
	 * @brief 检测当前是否适合执行平场计划, 并计算平场的最早起始时间与最晚结束时间
	 * @param start 最早起始时间, UTC
	 * @param stop  最晚结束时间, UTC
	 * @return
	 * 0: 不满足平场时间条件
	 * 1: 早平场时段
	 * 2: 晚平场时段
	 * 3: 极昼或极夜, 无限制?
	 */
	int ValidFlat(ptime &start, ptime &stop);

private:
	/* 回调函数 */
	/*!
	 * @brief 处理来自相机客户端的网络信息
	 * @param client 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param ec     错误代码. 0: 无错误; 1: 远程主机已断开
	 */
	void ReceiveCamera(const long client, const long ec);

private:
	/* 消息响应函数 */
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_messages();
	/*!
	 * @brief 处理消息MSG_RECEIVE_CAMERA, 即来自相机的网络连接请求
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnReceiveCamera(long param1, long param2);
	/*!
	 * @brief 处理消息MSG_RECEIVE_CAMERA, 即与相机之间的网络已经断开
	 * @param param1 收到网络信息的本地网络资源, 数据类型为tcp_client*
	 * @param param2 保留
	 */
	void OnCloseCamera(long param1, long param2);
	/*!
	 * @brief 处理消息MSG_MOUNT_CHANGED, 即转台工作状态发生变更
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnMountChanged(long param1, long param2);
	/*!
	 * @brief 处理消息MSG_NEW_PLAN, 即有新观测计划需要处理
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnNewPlan(long param1, long param2);
	/*!
	 * @brief 处理消息MSG_OUT_LIMIT, 即转台超出限位区域
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnOutLimit(long param1, long param2);
	/*!
	 * @brief 处理MSG_NEW_PROTOCOL, 即收到的主程序转发的网络消息
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnNewProtocol(long param1, long param2);
	/*!
	 * @brief 处理MSG_FLAT_RESLEW, 即重新指向天顶
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnFlatReslew(long param1, long param2);
	/*!
	 * @brief 处理MSG_WAIT_OVER, 即观测计划执行前等待线程正常结束
	 * @param param1 保留
	 * @param param2 保留
	 */
	void OnWaitOver(long param1, long param2);
	/*!
	 * @brief 处理收到的相机信息
	 * @param nf    存储对应相机的缓冲区
	 * @param proto 收到的网络信息
	 */
	void ProcessCameraProtocol(camnfptr nf, boost::shared_ptr<ascproto_camera_info> proto);
	/*!
	 * @brief 处理曝光信息
	 * @param proto 收到的网络信息
	 */
	void ProcessTakeImage(boost::shared_ptr<ascproto_take_image> proto);
	/*!
	 * @brief 处理终止曝光信息
	 * @param proto 收到的网络信息
	 */
	void ProcessAbortImage(boost::shared_ptr<ascproto_abort_image> proto);
	/*!
	 * @brief 处理终止指向过程信息
	 * @param sn 观测计划序列号. -1表示手动终止指向过程; >= 0表示终止该计划
	 */
	void ProcessAbortSlew(int sn = -1);
	/*!
	 * @brief 处理观测计划
	 * @param proto 收到的网络信息
	 */
	void ProcessAppendPlan(boost::shared_ptr<ascproto_append_gwac> proto);
	/*!
	 * @brief 处理搜索零点
	 */
	void ProcessFindHome();
	/*!
	 * @brief 处理调焦反馈
	 * @param proto 收到的网络信息
	 */
	void ProcessFocus(boost::shared_ptr<ascproto_focus> proto);
	/*!
	 * @brief 处理调焦反馈
	 * @param proto 收到的网络信息
	 */
	void ProcessFWHM(boost::shared_ptr<ascproto_fwhm> proto);
	/*!
	 * @brief 处理导星信息
	 * @param proto 收到的网络信息
	 */
	void ProcessGuide(boost::shared_ptr<ascproto_guide> proto);
	/*!
	 * @brief 处理同步零点信息
	 * @param proto 收到的网络信息
	 */
	void ProcessHomeSync(boost::shared_ptr<ascproto_home_sync> proto);
	/*!
	 * @brief 处理镜盖信息
	 * @param proto 收到的网络信息
	 */
	void ProcessMirrorCover(boost::shared_ptr<ascproto_mcover> proto);
	/*!
	 * @brief 处理转台复位
	 * @param proto 收到的网络信息
	 */
	void ProcessPark();
	/*!
	 * @brief 处理指向信息
	 * @param proto 收到的网络信息
	 */
	void ProcessSlewto(boost::shared_ptr<ascproto_slewto> proto);
	/*!
	 * @brief 处理启动自动模式
	 * @param proto 收到的网络信息
	 */
	void ProcessStartGwac();
	/*!
	 * @brief 处理停止自动模式
	 * @param proto 收到的网络信息
	 */
	void ProcessStopGwac();

private:
	/*!
	 * @brief 线程, 监测待观测计划活动
	 * @param seconds 观测计划开始前的等待时间
	 */
	void ThreadWaitPlan(int seconds);

private:
	/* 成员变量 */
	std::string groupid_;	//< 组标志
	std::string unitid_;	//< 单元标志
	int tmflag_;	//< 时间戳, 最后一次收到转台或相机信息的时间, 量纲: 秒

	system_state systate_;	//< 系统工作状态
	mntnfptr mntnf_;		//< 转台状态
	planptr obsplan_;		//< 正在执行的观测计划
	planptr waitplan_;		//< 等待执行的观测计划

/*---------------------------------- 时空转换 ----------------------------------*/
	boost::mutex mutex_ats_;	// ats_的互斥锁
	boost::shared_ptr<AstroUtil::ATimeSpace> ats_;	//< 天文时空函数接口
/*---------------------------------- 网络资源 ----------------------------------*/
	tcpcptr tcp_mount_;	//< 面向转台的网络连接
	camvec tcp_camera_;	//< 面向相机的网络连接
	tcpcptr tcp_mountannex_;	//< 面向转台附属设备的网络连接

	boost::mutex mutex_mount_;	//< 转台资源互斥锁
	boost::mutex mutex_camera_;	//< 相机资源互斥锁
	boost::mutex mutex_mountannex_;	//< 转台附属设备的网络连接

	boost::shared_array<char> bufrcv_;	//< 网络信息接收缓存区. 网络事件经消息队列串行化, 故可使用单一缓冲区
	ascptr ascproto_;	//< ASCII编码网络信息解码编码接口
	mntptr mntproto_;	//< 转台编码网络信息编码解码接口

	protovec protovec_;			//< 收到的网络信息存储区
	boost::mutex mutex_proto_;	//< 网络信息互斥锁
/*---------------------------------- 线程 ----------------------------------*/
	threadptr thrd_waitplan_;	//< 线程: 监测待观测计划是否需要执行
/*---------------------------------- 计数器 ----------------------------------*/
	uint32_t errMountTime_;	//< 转台时间错误
	uint32_t errSafe_;		//< 连续收到超出限位的转台位置
	uint32_t seqGuide_;		//< 当计数大于1时执行导星
	uint32_t seqPosErr_;	//< 位置偏差连续大于10次时, 执行重新指向
	ptime lastGuide_;		//< 最后一次导星时间
};

typedef boost::shared_ptr<ObservationSystem> obssptr;
/*!
 * @brief 工厂函数, 创建一个新的观测系统
 * @param gid  组标志
 * @param uid  单元标志
 * @return
 * 观测系统指针
 */
extern obssptr make_obss(const std::string& gid, const std::string& uid);

#endif /* OBSERVATIONSYSTEM_H_ */
