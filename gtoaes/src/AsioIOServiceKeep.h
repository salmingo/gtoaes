/**
 * @file AsioIOServiceKeep.h 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * @li boost::asio::io_service::run()在响应所注册的异步调用后自动退出. 为了避免退出run()函数,
 * 建立ioservice_keep维护其长期有效性
 */

#ifndef SRC_ASIOIOSERVICEKEEP_H_
#define SRC_ASIOIOSERVICEKEEP_H_

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

class AsioIOServiceKeep {
public:
	using IOService = boost::asio::io_service;

protected:
	using Work = IOService::work;
	/* 成员变量 */
	IOService ios_;
	Work work_;
	boost::thread thrd_keep_;

public:
	AsioIOServiceKeep();
	virtual ~AsioIOServiceKeep();
	IOService& GetIOService();

protected:
	void thread_keep();
};

#endif /* SRC_ASIOIOSERVICEKEEP_H_ */
