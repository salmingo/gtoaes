/**
 * @file AsioIOServiceKeep.cpp 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 */

#include <boost/bind/bind.hpp>
#include "AsioIOServiceKeep.h"

using namespace boost::placeholders;

AsioIOServiceKeep::AsioIOServiceKeep()
	: work_(ios_) {
	thrd_keep_ = boost::thread(boost::bind(&AsioIOServiceKeep::thread_keep, this));
}

AsioIOServiceKeep::~AsioIOServiceKeep() {
	ios_.stop();
	thrd_keep_.join();
}

AsioIOServiceKeep::IOService& AsioIOServiceKeep::GetIOService() {
	return ios_;
}

void AsioIOServiceKeep::thread_keep() {
	ios_.run();
}
