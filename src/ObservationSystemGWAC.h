/*
 * @file ObservationSystemGWAC.h 声明GWAC观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#ifndef OBSERVATIONSYSTEMGWAC_H_
#define OBSERVATIONSYSTEMGWAC_H_

#include "ObservationSystem.h"
#include "AsciiProtocol.h"
#include "MountProtocol.h"
#include "ObservationPlan.h"

class ObservationSystemGWAC: public ObservationSystem {
public:
	ObservationSystemGWAC(const string& gid, const string& uid);
	virtual ~ObservationSystemGWAC();

protected:
	// 成员变量
	MountPtr mntproto_;		//< GWAC转台编码协议解析接口

public:
	/*
	 * notify_xxx一般在主程序接口的消息队列里触发. 为避免notify_xxx耗时过长导致的主程序阻塞,
	 * 在本类中以消息队列形式管理notify_xxx, 即为其生成消息
	 */
	/*!
	 * @brief 通知下位机状态: 是否完成准备工作
	 */
	void NotifyReady(mpready proto);
	/*!
	 * @brief 通知下位机工作状态
	 */
	void NotifyState(mpstate proto);
	/*!
	 * @brief 通知下位机下位机UTC时钟
	 */
	void NotifyUtc(mputc proto);
	/*!
	 * @brief 通知下位机指向位置
	 */
	void NotifyPosition(mpposition proto);
	/*!
	 * @brief 通知制冷信息
	 */
	void NotifyCooler(apcooler proto);

protected:
	/*!
	 * @brief 检查坐标是否在安全范围内
	 * @param ra   赤经, 量纲: 角度
	 * @param dec  赤纬, 量纲: 角度
	 * @param azi  方位角, 量纲: 角度
	 * @param ele  高度角, 量纲: 角度
	 * @return
	 * 坐标在安全范围内返回true
	 * @note
	 * - 检查坐标在当前时间的高度角是否大于阈值
	 */
	bool safe_position(double ra, double dec, double &azi, double &ele);
	/*!
	 * @brief 计算x与当前可用观测计划的相对优先级
	 * @param x 待评估优先级
	 * @return
	 * 相对优先级
	 */
	int relative_priority(int x);
	/*!
	 * @brief 依据观测系统类型, 修正观测计划工作状态
	 * @param plan       观测计划
	 * @param old_state  观测计划当前状态
	 * @param new_state  新的状态
	 * @return
	 * 状态改变结果
	 */
	bool change_planstate(ObsPlanPtr plan, OBSPLAN_STATUS old_state, OBSPLAN_STATUS new_state);
	/*!
	 * @brief 解析GWAC观测计划, 形成ascii_proto_object并发送给对应相机
	 */
	void resolve_obsplan();
	/*!
	 * @brief 检查望远镜是否指向到位
	 * @return
	 * 望远镜指向到位标志
	 * @note
	 * GWAC系统与通用系统稳定度要求不同
	 */
	bool target_arrived();

protected:
	/*!
	 * @brief 导星
	 * @param proto 通信协议
	 */
	void process_guide(apguide proto);
	/*!
	 * @brief 通过数据处理统计得到的FWHM, 通知望远镜调焦
	 * @param proto 通信协议
	 */
	bool process_fwhm(apfwhm proto);
	/*!
	 * @brief 通知望远镜改变焦点位置
	 * @param proto 通信协议
	 */
	bool process_focus(apfocus proto);
	/*!
	 * @brief 中止指向和跟踪过程
	 * @note
	 * 该指令将触发停止观测计划
	 */
	bool process_abortslew();
	/*!
	 * @brief 复位
	 * @note
	 * 该指令将触发停止观测计划
	 */
	bool process_park();
	/*!
	 * @brief 指向目标位置
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param epoch 历元
	 */
	bool process_slewto(double ra, double dec, double epoch);
	/*!
	 * @brief 开关镜盖
	 * @note
	 * 关闭镜盖指令可能触发停止观测计划
	 */
	bool process_mcover(apmcover proto);
	/*!
	 * @brief 望远镜搜索零点
	 * @note
	 * 在观测计划执行过程中可能拒绝该指令
	 */
	bool process_findhome();
	/*!
	 * @brief 改变望远镜零点位置
	 * @param ra    赤经, 量纲: 角度
	 * @param dc    赤纬, 量纲: 角度
	 * @param epoch 目标位置坐标系, 量纲: 历元
	 */
	bool process_homesync(aphomesync proto);
};
typedef boost::shared_ptr<ObservationSystemGWAC> ObsSysGWACPtr;
extern ObsSysGWACPtr make_obss_gwac(const string& gid, const string& uid);

#endif /* OBSERVATIONSYSTEMGWAC_H_ */
