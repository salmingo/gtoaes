/**
 * @file ObservationSystem.h
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#ifndef OBSERVATIONSYSTEM_H_
#define OBSERVATIONSYSTEM_H_

#include <math.h>
#include <deque>
#include <boost/enable_shared_from_this.hpp>
#include "MessageQueue.h"
#include "ATimeSpace.h"
#include "KvProtocol.h"
#include "NonkvProtocol.h"
#include "ObservationPlan.h"
#include "Parameter.h"
#include "ADefine.h"
#include "ATimeSpace.h"
#include "DatabaseCurl.h"
#include "DomeSlit.h"
#include "TcpReceived.h"

class ObservationSystem
		: public boost::enable_shared_from_this<ObservationSystem>
		, public MessageQueue {
public:
	ObservationSystem(const string& gid, const string& uid);
	virtual ~ObservationSystem();

	/* 数据类型 */
public:
	using Pointer = boost::shared_ptr<ObservationSystem>;
	using KvProtoQue = std::deque<kvbase>;
	using NonkvProtoQue = std::deque<nonkvbase>;

	/*!
	 * @brief 回调函数, 尝试从队列里获取可用的观测计划
	 */
	using AcquirePlanFunc = boost::signals2::signal<ObsPlanItemPtr (const Pointer)>;
	using AcqPlanCBSlot = AcquirePlanFunc::slot_type;

	enum {///< 观测系统工作模式
		OBSS_ERROR,		///< 错误
		OBSS_MANUAL,	///< 手动
		OBSS_AUTO		///< 自动
	};
	//////////////////////////////////////////////////////////////////////////////
	enum {
		MSG_TCP_RECEIVE = MSG_USER,	///< 收到TCP消息
		MSG_RECEIVE_KV,		///< 收到KV类型信息, 来自GeneralControl的客户端的投递
		MSG_RECEIVE_NONKV,	///< 收到NonKV类型信息, 来自GeneralControl的转台或转台附属的投递
		MSG_MOUNT_LINKED,	///< 转台连接或断开
		MSG_MOUNT_CHANGED,	///< 转台状态发生变化
		MSG_CAMERA_LINKED,	///< 相机连接或断开
		MSG_CAMERA_CHANGED,	///< 相机状态发生变化
		MSG_SWITCH_OBSFLOW	///< 启动/结束观测流
	};

protected:
	struct NetworkMount {
		TcpCPtr client;		///< 网络连接
		bool kvtype;		///< 通信协议类型
		int state;			///< 工作状态
		int errcode;		///< 错误代码
		int coorsys;		///< 目标坐标系
		double ra, dec;		///< 赤经/赤纬, 角度
		double azi, alt;	///< 方位/高度, 角度
		double objra, objdec;	///< 目标赤经/赤纬, 角度
		double objazi, objalt;	///< 目标方位/高度, 角度
		// 导星量: 导星仅在恒星跟踪模式下生效
		double d_ra, d_dec;	///< 累积导星偏差量, 角度

	protected:
		int to_slew;		///< 准备指向目标
		int stable_track;	///< 稳定跟踪
		boost::mutex mtx;	///< 互斥锁

	public:
		NetworkMount() {
			kvtype = false;
			state = errcode = coorsys = 0;
			ra = dec = 1E30;
			azi = alt = 1E30;
			objra = objdec = 1E30;
			objazi = objalt = 1E30;
			to_slew = 0;
			stable_track = 0;
			d_ra = d_dec = 0.0;
		}

		/*!
		 * @brief 重定义操作符(), 引用网络连接
		 * @return
		 * 网络连接
		 */
		TcpCPtr operator()() {
			return client;
		}

		void Reset() {
			client.reset();
			state = -1;
		}

		bool IsOpen() {
			return (client.use_count() && client->IsOpen());
		}

		bool IsMoving() {
			return (state == StateMount::MOUNT_HOMING
					|| state == StateMount::MOUNT_PARKING
					|| state == StateMount::MOUNT_SLEWING);
		}

		/*!
		 * @brief 设置监视条件
		 * @param type 坐标系类型
		 * @param lon  经度, 角度
		 * @param lat  纬度, 角度
		 */
		void BeginSlew(int type, double lon = 0.0, double lat = 0.0) {
			MtxLck lck(mtx);
			to_slew = 5;
			stable_track = 5;
			state = StateMount::MOUNT_SLEWING;

			coorsys = type;
			if (coorsys == TypeCoorSys::COORSYS_ALTAZ) {
				objazi = lon;
				objalt = lat;
			}
			else if (coorsys == TypeCoorSys::COORSYS_EQUA) {
				objra  = lon;
				objdec = lat;
			}
			d_ra = d_dec = 0.0;
		}

		/*!
		 * @brief 设置导星监视条件
		 * @param ra   赤经导星量, 角度
		 * @param dec  赤纬导星量, 角度
		 * @note
		 * - 通用 :  ra: 赤经偏差, ra = object - sky
		 * - GWAC:  ra: 时角偏差, ra = sky - object
		 */
		void BeginGuide(double ra, double dec) {
			MtxLck lck(mtx);
			to_slew = 5;
			stable_track = 5;
			state = StateMount::MOUNT_SLEWING;
			d_ra  += ra;
			d_dec += dec;
		}

		/*!
		 * @brief 设置复位监视条件
		 */
		void BeginPark() {
			MtxLck lck(mtx);
			state = StateMount::MOUNT_PARKING;
		}

		NetworkMount& operator=(kvmount proto) {
			MtxLck lck(mtx);
			if (state != proto->state) {
				if (proto->state == StateMount::MOUNT_SLEWING) {
					if (to_slew) to_slew = 0;
					state = proto->state;
				}
				else if (proto->state != StateMount::MOUNT_TRACKING
						|| ((!to_slew || --to_slew == 0) && (!stable_track || --stable_track == 0))) {
					state = proto->state;
				}
			}
			errcode  = proto->errcode;
			ra  = proto->ra;
			dec = proto->dec;
			azi = proto->azi;
			alt = proto->alt;

			return *this;
		}

		/*!
		 * @brief 重定义操作符=. 用于GWAC转台
		 */
		NetworkMount& operator=(nonkvmount proto) {
			MtxLck lck(mtx);
			double tslew(0.008);	// 指向阈值. 0.008°==28.8″
			double e_lon, e_lat;	// 位置偏差

			if (state == StateMount::MOUNT_SLEWING) {// 判定指向或导星是否到位
				e_lon = fabs(proto->ra - ra);
				e_lat = fabs(proto->dec - dec);
				if (e_lon <= tslew && e_lat <= tslew && (stable_track && --stable_track == 0))
					state = StateMount::MOUNT_TRACKING;
			}
			else if (state == StateMount::MOUNT_PARKING) {// 判定复位是否到位
				e_lon = fabs(proto->azi - azi);
				e_lat = fabs(proto->alt - alt);
				if (e_lon <= tslew && e_lat <= tslew)
					state = StateMount::MOUNT_PARKED;
			}
			ra  = proto->ra;
			dec = proto->dec;
			azi = proto->azi;
			alt = proto->alt;

			return *this;
		}

		/*!
		 * @brief 计算指向偏差
		 * @return
		 * 指向偏差, 角度
		 */
		double ArriveError() {
			double d_lon, d_lat, lat;
			d_lon = d_lat = lat = 0.0;
			if (coorsys == TypeCoorSys::COORSYS_ALTAZ) {
				d_lon = fabs(azi - objazi);
				d_lat = fabs(alt - objalt);
				lat   = (alt + objalt) * 0.5;
			}
			else if (coorsys == TypeCoorSys::COORSYS_EQUA) {
				lat   = (dec + objdec) * 0.5;
#ifdef GWAC  // GWAC
				d_lon = fabs(ra - objra + d_ra);	// d_ra: 在时角上的累积量
				d_lat = fabs(dec - objdec - d_dec);
#else  // 通用
				d_lon = fabs(ra - objra - d_ra);	// d_ra: 在赤经上的累积量
				d_lat = fabs(dec - objdec - d_dec);
#endif
			}
			if (d_lon > 180.0) d_lon = 360.0 - d_lon;
			d_lon *= cos(lat * D2R);
			return sqrt(d_lon * d_lon + d_lat * d_lat);
		}
	};

	struct NetworkCamera {
		using Pointer = boost::shared_ptr<NetworkCamera>;

		TcpCPtr client;		///< 网络连接
		string  cid;		///< 相机编号
		string	filter;		///< 滤光片
		int		state;		///< 工作状态
		int		errcode;	///< 错误代码
		int		coolget;	///< 探测器温度, 量纲: 摄氏度
		bool    enabled;	///< 启用

	public:
		NetworkCamera() {
			state = errcode = 0;
			coolget = 0;
			enabled = true;
		}

		static Pointer Create() {
			return Pointer(new NetworkCamera);
		}

		/*!
		 * @brief 重定义操作符(), 引用网络连接
		 * @return
		 * 网络连接
		 */
		TcpCPtr operator()() {
			return client;
		}

		void Reset() {
			client.reset();
			state = -1;
		}

		bool IsOpen() {
			return (client.use_count() && client->IsOpen());
		}

		/*!
		 * @brief 检查曝光序列是否结束
		 * @return
		 * - 0: 没有结束, 仍在曝光
		 * - 1: 结束
		 * - 2: 没有结束, 进入序列中暂停状态
		 */
		int IsOver() {
			if (state <= StateCameraControl::CAMCTL_IDLE) return 1;
			if (state >= StateCameraControl::CAMCTL_PAUSEED) return 2;
			return 0;
		}

		NetworkCamera& operator=(kvcamera proto) {
			state   = proto->state;
			errcode = proto->errcode;
			coolget = proto->coolget;
			filter  = proto->filter;

			return *this;
		}
	};
	using NetCamPtr = NetworkCamera::Pointer;
	using NetCamVec = std::vector<NetCamPtr>;

protected:
	/* 成员变量 */
	/* OBSS标志 */
	string gid_;	///< 组标志
	string uid_;	///< 单元标志
	/*!
	 * @brief 自动化模式
	 * - true: 自动化模式
	 *   当转台、相机等设备状态良好时, 系统切换到自动观测模式
	 * - false: 非自动化模式
	 *   当转台、相机等设备状态良好时, 系统切换到手动响应模式
	 */
	bool robotic_;
	int mode_run_;		///< 系统运行模式. 只和转台、相机相关
	int odt_;			///< 可观测时间类型
	double altLimit_;	///< 高度限位, 弧度
	const OBSSParam* param_;	///< 观测系统工作参数
	AstroUtil::ATimeSpace ats_;	///< 时空坐标转换接口
	boost::mutex mtx_ats_;		///< ats_互斥锁

	/* 转台 */
	NetworkMount net_mount_;	///< 网络+转台

	/* 相机 */
	NetCamVec net_camera_;		///< 网络+相机
	int usable_camera_;			///< 可用相机数量
	boost::mutex mtx_camera_;	///< 互斥锁: 相机

	/* 转台附属 */

	/* 相机附属 */

	/* 天窗 */
	SlitSimVec slit_;		///< 单一的天窗
	boost::mutex mtx_slit_;	///< 互斥锁：天窗

	/* 客户端 */
	TcpCVec tcpc_client_;		///< 客户端
	boost::mutex mtx_client_;	///< 互斥锁: 客户端

	/* 网络通信 */
	boost::shared_array<char> bufTcp_;	///< 网络信息存储区: 消息队列中调用
	KvProtoPtr kvProto_;		///< 键值对格式协议访问接口
	NonkvProtoPtr nonkvProto_;	///< 非键值对格式协议访问接口
	boost::mutex mtx_queKv_;	///< 互斥锁: 键值对协议队列
	boost::mutex mtx_queNonkv_;	///< 互斥锁: 非键值对协议队列

	TcpRcvQue que_tcpRcv_;		///< 网络事件队列
	KvProtoQue queKv_;			///< 被投递的键值对协议队列
	NonkvProtoQue queNonkv_;	///< 被投递的非键值对协议队列
	boost::mutex mtx_tcpRcv_;	///< 互斥锁: 网络事件

	/* 观测计划 */
	ObsPlanPtr obsPlans_;		///< 观测计划集合, 维护定标用的观测计划
	ObsPlanItemPtr plan_now_;	///< 观测计划: 正在执行
	ObsPlanItemPtr plan_wait_;	///< 观测计划: 等待执行
	AcquirePlanFunc acqPlan_;	///< 回调函数, 尝试获取观测计划
	boost::condition_variable cv_acqPlan_;	///< 条件变量: 获取观测计划

	/* 数据库 */
	DBCurlPtr dbPtr_;	///< 数据库访问接口

	/* 多线程 */
	ThreadPtr thrd_acqPlan_;	///< 线程: 尝试获取观测计划
	ThreadPtr thrd_calPlan_;	///< 线程: 生成定标观测计划

public:
	/* 接口 */
	/*!
	 * @brief 创建实例指针
	 * @return
	 * boost::shared_ptr<>型指针
	 */
	static Pointer Create(const string& gid, const string& uid) {
		return Pointer(new ObservationSystem(gid, uid));
	}
	/*!
	 * @brief 启动系统工作流程
	 * @return
	 * 启动结果
	 */
	bool Start();
	/*!
	 * @brief 停止系统工作流程
	 */
	void Stop();
	/*!
	 * @brief 设置观测系统工作参数
	 * @param param  参数指针
	 */
	void SetParameter(const OBSSParam* param);
	/*!
	 * @brief 设置数据库访问接口
	 */
	void SetDBPtr(DBCurlPtr ptr);
	/*!
	 * @brief 注册回调函数: 请求新的观测计划
	 * @param slot  插槽函数
	 */
	void RegisterAcquirePlan(const AcqPlanCBSlot& slot);
	/*!
	 * @brief 检查观测计划是否在可指向安全范围内
	 * @param plan  观测计划
	 * @param now   当前时间
	 * @return
	 * 是否在安全范围内
	 */
	bool IsSafePoint(ObsPlanItemPtr plan, const ptime& now);
	/*!
	 * @brief 检查系统活跃度
	 * @return
	 * 与系统关联的设备数量
	 */
	int IsActive();
	/*!
	 * @brief 检查是否与系统匹配
	 * @param gid  组标志
	 * @param uid  单元标志
	 * @return
	 * 匹配结果.
	 * - 0: 匹配失败
	 * - 1: 强匹配, gid和uid都相同
	 * - 2: 弱匹配, 符合相同原则
	 */
	int IsMatched(const string& gid, const string& uid);
	/*!
	 * @brief 查看观测系统唯一性标志组合
	 * @param gid  组标志
	 * @param uid  单元标志
	 */
	void GetIDs(string& gid, string& uid);
	/*!
	 * @brief 获取观测系统正在执行计划的优先级
	 * @return
	 * 优先级
	 * @note
	 * - 系统空闲时, 优先级 == 0
	 * - 在执行计划时, \f$ prio = prio_{plan} * T / [T - (t - t_0)] \f$
	 * - 优先级限制: \f$ prio <= 4 * prio_{plan} \f$
	 */
	int GetPriority();
	/*!
	 * @brief 关联观测系统与客户端
	 * @param client  网络连接
	 */
	void CoupleClient(const TcpCPtr client);
	/*!
	 * @brief 关联观测系统与转台
	 * @param client  网络连接
	 * @param base    键值对通信协议
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMount(const TcpCPtr client, kvbase base);
	/*!
	 * @brief 关联观测系统与转台
	 * @param client  网络连接
	 * @param base    非键值对通信协议
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMount(const TcpCPtr client, nonkvbase base);
	/*!
	 * @brief 关联观测系统与相机
	 * @param client  网络连接
	 * @param base    键值对通信协议
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleCamera(const TcpCPtr client, kvbase base);
	/*!
	 * @brief 关联观测系统与转台附属设备
	 * @param client  网络连接
	 * @param base    键值对通信协议
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMountAnnex(const TcpCPtr client, kvbase base);
	/*!
	 * @brief 关联观测系统与转台附属设备
	 * @param client  网络连接
	 * @param base    非键值对通信协议
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleMountAnnex(const TcpCPtr client, nonkvbase base);
	/*!
	 * @brief 关联观测系统与相机附属设备
	 * @param client  网络连接
	 * @return
	 * 0: 失败
	 * 1: 成功; 连接类型是P2P
	 * 2: 成功: 连接类型是P2H
	 */
	int CoupleCameraAnnex(const TcpCPtr client, kvbase base);

	/*!
	 * @brief 解除客户端与观测系统的关联
	 * @param client  网络连接
	 */
	void DecoupleClient(const TcpCPtr client);
	/*!
	 * @brief 解除转台和观测系统的关联
	 * @param client  网络连接
	 */
	void DecoupleMount(const TcpCPtr client);
	/*!
	 * @brief 解除相机和观测系统的关联
	 * @param client  网络连接
	 */
	void DecoupleCamera(const TcpCPtr client);
	/*!
	 * @brief 解除转台附属和观测系统的关联
	 * @param client  网络连接
	 */
	void DecoupleMountAnnex(const TcpCPtr client);
	/*!
	 * @brief 解除相机附属和观测系统的关联
	 * @param client  网络连接
	 */
	void DecoupleCameraAnnex(const TcpCPtr client);

	/*!
	 * @brief 将客户端接收信息投递到观测系统
	 * @param base  键值对通信协议
	 */
	void NotifyKVClient(kvbase base);
	/*!
	 * @brief 投递观测计划
	 * @param plan  观测计划
	 */
	void NotifyPlan(ObsPlanItemPtr plan);
	/*!
	 * @brief 中止观测计划
	 */
	void AbortPlan(ObsPlanItemPtr plan);
	/*!
	 * @brief 通知可观测时间类型
	 * @param type  可观测时间类型
	 */
	void NotifyODT(int type);
	/*!
	 * @brief 设置天窗状态
	 * @param state  天窗状态
	 */
	void NotifySlitState(int state);

	/* 功能 */
protected:
	/*----------------- 消息响应 -----------------*/
	/*!
	 * @brief 注册消息响应函数
	 */
	void register_messages();
	/*!
	 * @brief 消息: 收到TCP信息
	 * @param par1  参数1, 保留
	 * @param par2  参数2, 保留
	 * @note
	 * P2P模式设备收到的信息
	 */
	void on_tcp_receive(const long par1, const long par2);
	/*!
	 * @brief 消息: 收到KV类型信息
	 * @param par1  参数1, 保留
	 * @param par2  参数2, 保留
	 */
	void on_receive_kv(const long par1, const long par2);
	/*!
	 * @brief 消息: 收到Non-KV类型信息
	 * @param par1  参数1, 保留
	 * @param par2  参数2, 保留
	 */
	void on_receive_nonkv(const long par1, const long par2);
	/*!
	 * @brief 消息: 转台已连接或断开
	 * @param linked  连接标志
	 * @param par2    保留
	 */
	void on_mount_linked(const long linked, const long par2);
	/*!
	 * @brief 消息: 转台工作状态变化
	 * @param old_state 改变前状态
	 * @param par2      保留
	 */
	void on_mount_changed(const long old_state, const long par2);
	/*!
	 * @brief 消息: 转台已连接或断开
	 * @param linked  连接标志
	 * @param par2    保留
	 */
	void on_camera_linked(const long linked, const long par2);
	/*!
	 * @brief 消息: 转台工作状态变化
	 * @param par1  保留
	 * @param par2  保留
	 */
	void on_camera_changed(const long par1, const long par2);
	/*!
	 * @brief 消息: 启动/结束观测流程
	 * @param par1  保留
	 * @param par2  保留
	 * @note
	 * 启动观测流程的判定条件:
	 * - 工作模式 == Auto
	 * - 时间类型 == FLAT or NIGHT
	 * - 天窗    == Open
	 */
	void on_switch_obsflow(const long par1, const long par2);

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 网络通信 */
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络连接
	 * @param ec     错误代码. 0: 正确
	 * @param peer   远程主机类型
	 */
	void receive_from_peer(const TcpCPtr client, const error_code& ec, int peer);
	/*!
	 * @brief  解析处理转台的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_mount(const TcpCPtr client);
	/*!
	 * @brief  解析处理相机的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_camera(const TcpCPtr client);
	/*!
	 * @brief  解析处理转台附属设备的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_mount_annex (const TcpCPtr client);
	/*!
	 * @brief  解析处理相机附属设备的键值对协议
	 * @param client  网络连接
	 */
	void resolve_kv_camera_annex(const TcpCPtr client);

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 处理由上层程序投递到观测系统的键值对协议 */
	/*!
	 * @brief 启动系统自动工作流程
	 */
	void process_start();
	/*!
	 * @brief 停止系统自动工作流程
	 */
	void process_stop();
	/*!
	 * @brief 启用被禁用相机
	 * @param cid  相机编号
	 */
	void process_enable(const string& cid);
	/*!
	 * @brief 禁用相机
	 * @param cid  相机编号
	 */
	void process_disable(const string& cid);
	/*!
	 * @brief 搜索零点
	 */
	void process_findhome();
	/*!
	 * @brief 指向
	 */
	void process_slewto(kvslewto proto);
	/*!
	 * @brief 中止指向
	 */
	void process_abort_slew();
	/*!
	 * @brief 导星
	 */
	void process_guide(kvguide proto);
	/*!
	 * @brief 同步零点
	 */
	void process_homesync(kvhomesync proto);
	/*!
	 * @brief 复位
	 */
	void process_park();

	/*!
	 * @brief 开始曝光
	 */
	void process_take_image(kvtakeimg proto);
	/*!
	 * @brief 中止曝光
	 */
	void process_abort_image(const string& cid);

	/*!
	 * @brief 自动调焦
	 */
	void process_fwhm(const string& cid, const double fwhm);
	/*!
	 * @brief 调焦
	 */
	void process_focus(const string& cid, const int pos);

	/*!
	 * @brief 开关镜盖
	 */
	void process_mcover(const string& cid, int cmd);
	/* 处理由上层程序投递到观测系统的键值对协议 */
	//////////////////////////////////////////////////////////////////////////////

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 观测系统 */
	/*!
	 * @brief 消息: 观测系统工作模式改变
	 * @param mode  新的工作模式
	 */
	void switch_runmode(int mode);
	/* 观测设备 */
	/*!
	 * @brief 查找对应cid的相机
	 * @param cid    相机标志
	 * @param client 网络连接
	 * @return
	 * 网络相机数据接口
	 */
	NetCamPtr find_camera(const string& cid);
	NetCamPtr find_camera(const TcpCPtr client);

	/*!
	 * @brief 平场, 采集图像前重新指向
	 * @param first  初始执行平场计划
	 */
	void flat_reslew(bool first = false);
	/*!
	 * @brief 执行指向操作
	 * @param type   坐标系类型
	 * @param lon    经度, 角度
	 * @param lat    纬度, 角度
	 * @param epoch  历元, 适用于赤道系
	 * @note
	 * - 历元: 0.0 = 当前历元
	 */
	void process_slewto(int type, double lon, double lat, double epoch = 0.0);
	/*!
	 * @brief 执行曝光操作
	 * @param cmd         曝光指令
	 * @param just_guide  是否仅控制导星相机
	 */
	void process_expose(int cmd, bool just_guide = false);

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 观测计划 */
	/*!
	 * @brief 尝试执行当前观测计划
	 */
	void process_plan();
	/*!
	 * @brief 尝试中止当前观测计划
	 */
	void abort_plan();

	/*!
	 * @brief 生成定标计划的公共部分
	 * @param utcNow 当前UTC时间
	 * @return
	 * 定标观测计划实例指针
	 */
	ObsPlanItemPtr generate_plan_common(const ptime& utcNow);
	/*!
	 * @brief 生成定标用计划: 本底
	 * @param utcNow 当前UTC时间
	 */
	void generate_plan_bias(const ptime& utcNow);
	/*!
	 * @brief 生成定标用计划: 暗场
	 * @param utcNow 当前UTC时间
	 */
	void generate_plan_dark(const ptime& utcNow);
	/*!
	 * @brief 生成定标用计划: 平场
	 * @param utcNow 当前UTC时间
	 */
	void generate_plan_flat(const ptime& utcNow);

protected:
	//////////////////////////////////////////////////////////////////////////////
	/* 多线程 */
	/*!
	 * @brief 线程: 尝试获取新的计划
	 * @note
	 * - 当前计划完成后, 尝试获取新计划
	 * -
	 */
	void thread_acquire_plan();
	/*!
	 * @brief 线程: 当日期变更时, 生成定标观测计划
	 * @note
	 * - 定标计划包括本底、暗场和平场
	 * - 当滤光片为All时指代使用所有滤光片
	 */
	void thread_calbration_plan();
};
using ObsSysPtr = ObservationSystem::Pointer;

#endif /* SRC_OBSERVATIONSYSTEM_H_ */
