/*!
 * @file GLog.cpp 类GLog的定义文件
 * @version      2.0
 * @date    2016年10月28日
 */

#include <sys/stat.h>
#include <sys/types.h>	// Linux需要
#include <unistd.h>
#include <stdarg.h>
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "GLog.h"
#include "globaldef.h"

using std::string;
using namespace boost::posix_time;

GLog::GLog(FILE *out) {
	day_ = -1;
	fd_  = out;
	dirname_   = gLogDir;
	prefix_    = gLogPrefix;
	waitflush_ = 0;
	thrd_flush_.reset(new boost::thread(boost::bind(&GLog::thread_flush, this)));
}

GLog::GLog(const char* dirname, const char* prefix) {
	day_ = -1;
	fd_  = NULL;
	dirname_   = dirname;
	prefix_    = prefix;
	waitflush_ = 0;
	thrd_flush_.reset(new boost::thread(boost::bind(&GLog::thread_flush, this)));
}

GLog::~GLog() {
	thrd_flush_->interrupt();
	thrd_flush_->join();
	if (fd_ && fd_ != stdout && fd_ != stderr) fclose(fd_);
}

bool GLog::valid_file(ptime &t) {
	if (fd_ == stdout || fd_ == stderr) return true;
	if (ntp_.use_count()) {
		double offset = ntp_->GetOffset();
		t = t + seconds(int(offset));	// 精确到秒
	}

	ptime::date_type date = t.date();
	if (day_ != date.day()) {// 日期变更
		day_ = date.day();
		if (fd_) {// 关闭已打开的日志文件
			fprintf(fd_, "%s continue\n", string(69, '>').c_str());
			fclose(fd_);
			fd_ = NULL;
			waitflush_ = 0;
		}
	}

	if (fd_ == NULL) {
		if (access(dirname_.c_str(), F_OK)) mkdir(dirname_.c_str(), 0755);	// 创建目录
		if (!access(dirname_.c_str(), W_OK | X_OK)) {
			boost::filesystem::path path = dirname_;
			boost::format fmt("%s%s.log");
			fmt % prefix_.c_str() % to_iso_string(date).c_str();
			path /= fmt.str();
			fd_ = fopen(path.c_str(), "a+");
			waitflush_ = fprintf(fd_, "%s\n", string(79, '-').c_str());
			if (boost::iequals(prefix_, gLogPlanPrefix)) {
				waitflush_ += fprintf(fd_, "%s\n", to_iso_extended_string(t).c_str());
				waitflush_ += fprintf(fd_, "%4s %12s %7s %7s %6s %4s %s\n",
						"STAT", "SN  ", "R.A. ", "DEC. ", "IMGTYP", "PRIO", "GID:UID");
				waitflush_ += fprintf(fd_, "%s\n", string(79, '*').c_str());
			}
		}
	}

	return (fd_ != NULL);
}

void GLog::thread_flush() {
	boost::chrono::seconds period(10);	// 定时周期: 10秒

	while (1) {
		boost::this_thread::sleep_for(period);
		mutex_lock lock(mtx_);
		if (waitflush_) {
			fflush(fd_);
			waitflush_ = 0;
		}
	}
}

void GLog::SetNTP(NTPPtr ntp) {
	ntp_ = ntp;
}

void GLog::Write(const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(mtx_);
	ptime t = second_clock::universal_time();
	va_list vl;

	if (valid_file(t)) {
		waitflush_ += fprintf(fd_, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		va_start(vl, format);
		waitflush_ += vfprintf(fd_, format, vl);
		va_end(vl);
		waitflush_ += fprintf(fd_, "\n");
		if (waitflush_ > 1024) {
			fflush(fd_);
			waitflush_ = 0;
		}
	}
}

void GLog::Write(const LOG_TYPE type, const char* where, const char* format, ...) {
	if (format == NULL) return;

	mutex_lock lock(mtx_);
	ptime t = second_clock::universal_time();
	va_list vl;

	if (valid_file(t)) {
		waitflush_ += fprintf(fd_, "%s >> ", to_simple_string(t.time_of_day()).c_str());
		if      (type == LOG_WARN)  waitflush_ += fprintf(fd_, "WARN: ");
		else if (type == LOG_FAULT) waitflush_ += fprintf(fd_, "ERROR: ");
		if (where) waitflush_ += fprintf(fd_, "%s, ", where);
		va_start(vl, format);
		waitflush_ += vfprintf(fd_, format, vl);
		va_end(vl);
		waitflush_ += fprintf(fd_, "\n");
		if (waitflush_ > 1024) {
			fflush(fd_);
			waitflush_ = 0;
		}
	}
}

void GLog::Write(const LOGPLAN_TYPE type, const char* format, ...) {
	if (format == NULL || type < LOGPLAN_OVER || type > LOGPLAN_IGNR) return;

	mutex_lock lock(mtx_);
	ptime t = second_clock::local_time();
	va_list vl;

	if (valid_file(t)) {
		if      (type == LOGPLAN_OVER)  waitflush_ += fprintf(fd_, "OVER  ");
		else if (type == LOGPLAN_INTR)  waitflush_ += fprintf(fd_, "INTR  ");
		else                            waitflush_ += fprintf(fd_, "IGNR  ");
		va_start(vl, format);
		waitflush_ += vfprintf(fd_, format, vl);
		va_end(vl);
		waitflush_ += fprintf(fd_, "\n");
		if (waitflush_ > 1024) {
			fflush(fd_);
			waitflush_ = 0;
		}
	}
}
