/*
 * @file GLog.cpp 类GLog的定义文件
 * @version      2.0
 * @date    2016年10月28日
 */

#include <sys/stat.h>
#include <sys/types.h>	// Linux需要
#include <unistd.h>
#include <stdarg.h>
#include <string>
#include "GLog.h"
#include "globaldef.h"

using namespace std;
using namespace boost::posix_time;

GLog::GLog(FILE *out) {
	m_day = -1;
	m_fd  = out;
	bytecache_ = 0;
}

GLog::~GLog() {
	if (thrdCycle_.unique()) {
		thrdCycle_->interrupt();
		thrdCycle_->join();
	}
	if (m_fd && m_fd != stdout && m_fd != stderr) fclose(m_fd);
}

bool GLog::valid_file(ptime &t) {
	if (m_fd == stdout || m_fd == stderr) return true;
	ptime::date_type date = t.date();
	if (m_day != date.day()) {// 日期变更
		m_day = date.day();
		if (m_fd) {// 关闭已打开的日志文件
			fprintf(m_fd, "%s continue\n", string(69, '>').c_str());
			fclose(m_fd);
			m_fd = NULL;
		}
	}

	if (m_fd == NULL) {
		char pathname[200];

		if (access(gLogDir, F_OK)) mkdir(gLogDir, 0755);	// 创建目录
		sprintf(pathname, "%s/%s%s.log",
				gLogDir, gLogPrefix, to_iso_string(date).c_str());
		m_fd = fopen(pathname, "a+t");
		tmlast_ = t;
		bytecache_ = fprintf(m_fd, "%s\n", string(79, '-').c_str());

		if (!thrdCycle_.unique()) {
			thrdCycle_.reset(new boost::thread(boost::bind(&GLog::thread_cycle, this)));
		}
	}

	return (m_fd != NULL);
}

void GLog::thread_cycle() {
	boost::chrono::seconds period(1);
	time_duration td;

	while(1) {
		boost::this_thread::sleep_for(period);

		mutex_lock lck(m_mutex);
		if (bytecache_ > 0 && (second_clock::local_time() - tmlast_).total_seconds() > 1) {
			fflush(m_fd);
			bytecache_ = 0;
		}
	}
}

void GLog::Write(const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(m_mutex);
	ptime t(second_clock::local_time());

	if (valid_file(t)) {
		// 时间标签
		bytecache_ += fprintf(m_fd, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		// 日志描述的格式与内容
		va_list vl;
		va_start(vl, format);
		bytecache_ += vfprintf(m_fd, format, vl);
		va_end(vl);
		bytecache_ += fprintf(m_fd, "\n");
		tmlast_ = t;
	}
}

void GLog::Write(const char* where, const LOG_TYPE type, const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(m_mutex);
	ptime t(second_clock::local_time());

	if (valid_file(t)) {
		// 时间标签
		bytecache_ += fprintf(m_fd, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		// 日志类型
		if (type == LOG_WARN)       bytecache_ += fprintf(m_fd, "WARN: ");
		else if (type == LOG_FAULT) bytecache_ += fprintf(m_fd, "ERROR: ");
		// 事件位置
		if (where) bytecache_ += fprintf(m_fd, "%s, ", where);
		// 日志描述的格式与内容
		va_list vl;
		va_start(vl, format);
		bytecache_ += vfprintf(m_fd, format, vl);
		va_end(vl);
		bytecache_ += fprintf(m_fd, "\n");
		tmlast_ = t;
	}
}
