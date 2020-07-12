/*!
 * @file GLog.h  类GLog声明文件
 * @author       卢晓猛
 * @description  日志文件访问接口
 * @version      2.0
 * @date         2016年10月28日
 * @version      2.1
 * @date         2020年7月4日
 * - 日志时标由本地时变更为UTC. 原因: 将一个观测夜的日志记录在一个文件中
 * - 当待写盘数据超过1KB时, 立即写盘
 * - 当持续10秒无新日志时, 立即写盘
 * - 增加线程:
 * @note
 * 使用互斥锁管理文件写入操作, 将并行操作转换为串性操作, 避免日志混淆
 */

#ifndef GLOG_H_
#define GLOG_H_

#include <stdio.h>
#include <string>
#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "NTPClient.h"

using std::string;

enum LOG_TYPE {// 日志类型
	LOG_NORMAL,	// 普通
	LOG_WARN,	// 警告, 可以继续操作
	LOG_FAULT	// 错误, 需清除错误再继续操作
};

enum LOGPLAN_TYPE {// 观测计划日志类型
	LOGPLAN_OVER,	// 完成
	LOGPLAN_INTR,	// 中断
	LOGPLAN_IGNR	// 废弃
};

class GLog {
public:
	GLog(FILE *out = NULL);
	GLog(const char* dirname, const char* prefix);
	virtual ~GLog();

protected:
	/*!
	 * @brief 检查日志文件有效性
	 * @param t 本地时间
	 * @return
	 * 文件有效性. true: 可继续操作文件; false: 文件访问错误
	 * @note
	 * 当日期变更时, 需重新创建日志文件
	 */
	bool valid_file(boost::posix_time::ptime &t);
	/*!
	 * @brief 定时检查是否有数据等待写盘
	 */
	void thread_flush();

public:
	/*!
	 * @brief 设置NTP接口
	 * @param ntp NTP接口
	 */
	void SetNTP(NTPPtr ntp);
	/*!
	 * @brief 记录一条日志
	 * @param format  日志描述的格式和内容
	 */
	void Write(const char* format, ...);
	/*!
	 * @brief 记录一条日志
	 * @param type    日志类型
	 * @param where   事件位置
	 * @param format  日志描述的格式和内容
	 */
	void Write(const LOG_TYPE type, const char* where, const char* format, ...);
	/*!
	 * @brief 记录一条观测计划日志
	 * @param type    日志类型
	 * @param format  日志描述的格式和内容
	 */
	void Write(const LOGPLAN_TYPE type, const char* format, ...);

protected:
	/* 数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock; //< 基于boost::mutex的互斥锁
	typedef boost::shared_ptr<boost::thread> threadptr;

protected:
	/* 成员变量 */
	boost::mutex mtx_;	//< 互斥区
	NTPPtr ntp_;		//< NTP接口
	int  day_;			//< UTC日期
	int  waitflush_;	//< 待写盘数据长度
	FILE *fd_;			//< 日志文件描述符
	string dirname_;	//< 目录名
	string prefix_;		//< 文件名前缀
	threadptr thrd_flush_;	//< 写盘线程
};
typedef boost::shared_ptr<GLog> GLogPtr;

extern GLogPtr _gLog;		//< 工作日志
extern GLogPtr _gLogPlan;	//< 观测计划日志

#endif /* GLOG_H_ */
