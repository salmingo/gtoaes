/*!
 * @file ioservice_keep.cpp 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 */

#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include "IOServiceKeep.h"

using namespace boost::asio;

IOServiceKeep::IOServiceKeep() {
	work_.reset(new io_service::work(ios_));
	thrd_keep_.reset(new boost::thread(boost::bind(&io_service::run, &ios_)));
}

IOServiceKeep::~IOServiceKeep() {
	ios_.stop();
	thrd_keep_->join();
}

io_service& IOServiceKeep::get_service() {
	return ios_;
}
