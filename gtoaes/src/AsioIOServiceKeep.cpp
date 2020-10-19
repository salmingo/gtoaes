/**
 * @file AsioIOServiceKeep.cpp 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 */

#include <boost/bind/bind.hpp>
#include "AsioIOServiceKeep.h"

using namespace boost::placeholders;

AsioIOServiceKeep::AsioIOServiceKeep() {
	work_.reset(new Work(io_service_));
	thrd_keep_.reset(new boost::thread(boost::bind(&AsioIOServiceKeep::thread_keep, this)));
}

AsioIOServiceKeep::~AsioIOServiceKeep() {
	io_service_.stop();
	thrd_keep_->join();
}

AsioIOServiceKeep::IOService& AsioIOServiceKeep::GetIOService() {
	return io_service_;
}

void AsioIOServiceKeep::thread_keep() {
	io_service_.run();
}
