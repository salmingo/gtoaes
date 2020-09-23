/*
 * @file ioservice_keep.cpp 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
  */

#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include "ioservice_keep.h"

ioservice_keep::ioservice_keep() {
	work_.reset(new work(ios_));
	thread_.reset(new thread(boost::bind(&io_service::run, &ios_)));
}

ioservice_keep::~ioservice_keep() {
	work_.reset();
	ios_.stop();
	thread_->join();
}

io_service& ioservice_keep::get_service() {
	return ios_;
}
