/*
 * @file GLog.h  类GLog声明文件
 * @author       卢晓猛
 * @description  日志文件访问接口
 * @version      2.0
 * @date         2016年10月28日
 * @note
 * 使用互斥锁管理文件写入操作, 将并行操作转换为串性操作, 避免日志混淆
 * @note
 * 当输入\n后执行硬盘写入, 因此不需要使用内存缓冲区减少IO操作策略
 */

#ifndef GLOG_H_
#define GLOG_H_

#include <stdio.h>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/smart_ptr.hpp>

enum LOG_TYPE {// 日志类型
	LOG_NORMAL,	// 普通
	LOG_WARN,	// 警告, 可以继续操作
	LOG_FAULT	// 错误, 需清除错误再继续操作
};

class GLog {
public:
	GLog(FILE *out = NULL);
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
 	 * @brief 周期线程, 延时写入硬盘
 	 */
	void thread_cycle();

public:
	/*!
	 * @brief 记录一条日志
	 * @param format  日志描述的格式和内容
	 */
	void Write(const char* format, ...);
	/*!
	 * @brief 记录一条日志
	 * @param where   事件位置
	 * @param type    日志类型
	 * @param format  日志描述的格式和内容
	 */
	void Write(const char* where, const LOG_TYPE type, const char* format, ...);

protected:
	/* 声明数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock; //< 基于boost::mutex的互斥锁
	typedef boost::shared_ptr<boost::thread> threadptr;

	/* 声明成员变量 */
	boost::mutex m_mutex;	//< 互斥区
	int  m_day;				//< UTC日期
	FILE *m_fd;				//< 日志文件描述符
	boost::posix_time::ptime tmlast_;	//< 最后一次生成日志的时间
	int bytecache_;		//< 待刷新数据长度
	threadptr thrdCycle_;	//< 周期线程
};

extern GLog gLog;

#endif /* GLOG_H_ */
