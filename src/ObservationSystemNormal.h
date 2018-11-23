/*
 * @file ObservationSystemNormal.h 声明通用观测系统
 * @version 0.1
 * @date 2018-10-28
 */

#ifndef OBSERVATIONSYSTEMNORMAL_H_
#define OBSERVATIONSYSTEMNORMAL_H_

#include "ObservationSystem.h"
#include "AsciiProtocol.h"
#include "ObservationPlan.h"

class ObservationSystemNormal: public ObservationSystem {
public:
	ObservationSystemNormal(const string& gid, const string& uid);
	virtual ~ObservationSystemNormal();

public:
	/*!
	 * @brief 关联望远镜网络连接与观测系统
	 * @param ptr 网络连接
	 * @return
	 * 关联结果
	 */
	bool CoupleTelescope(TcpCPtr ptr);
	/*!
	 * @brief 接收新的观测计划
	 * @param plan 以通信协议格式记录的观测计划
	 */
	bool NotifyPlan(ObsPlanPtr plan);

protected:
	/*!
	 * @brief 计算x与当前可用观测计划的相对优先级
	 * @param x 待评估优先级
	 * @return
	 * 相对优先级
	 */
	int relative_priority(int x);
	/*!
	 * @brief 解析通用观测计划, 尤其是其中的曝光参数, 形成ascii_proto_object并发送给对应相机
	 * @return
	 * 观测计划解析结果
	 */
	bool resolve_obsplan();

protected:
	/*!
	 * @brief 处理望远镜信息
	 * @param client 网络资源
	 * @param ec     错误代码
	 */
	void receive_telescope(const long client, const long ec);
	/*!
	 * @brief 处理接收的观测计划
	 */
	void on_new_plan(const long, const long);
	/*!
	 * @brief 导星
	 * @param proto 通信协议
	 */
	bool process_guide(apguide proto);
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
	/*!
	 * @brief 改变调焦器零点位置
	 */
	bool process_focusync();
	/*!
	 * @brief 继承类处理待执行计划
	 * @param plan_sn 计划编号
	 * @note
	 * 若编号匹配:
	 * - GWAC系统: 删除该计划
	 * - 通用系统: 退还计划队列
	 */
	void process_abortplan(int plan_sn);
};
typedef boost::shared_ptr<ObservationSystemNormal> ObsSysNormalPtr;
extern ObsSysNormalPtr make_obss_normal(const string& gid, const string& uid);

#endif /* OBSERVATIONSYSTEMNORMAL_H_ */
