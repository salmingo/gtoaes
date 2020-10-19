/**
 * @file AsioIOServiceKeep.h 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * @li boost::asio::io_service::run()在响应所注册的异步调用后自动退出. 为了避免退出run()函数,
 * 建立ioservice_keep维护其长期有效性
 * @li 使用shared_ptr管理指针
 */

#ifndef SRC_ASIOIOSERVICEKEEP_H_
#define SRC_ASIOIOSERVICEKEEP_H_

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

class AsioIOServiceKeep {
public:
	using IOService = boost::asio::io_service;

protected:
	using Work = IOService::work;
	using WorkPtr = boost::shared_ptr<Work>;
	using ThreadPtr = boost::shared_ptr<boost::thread>;
	/* 成员变量 */
	IOService io_service_;
	WorkPtr work_;
	ThreadPtr thrd_keep_;

public:
	AsioIOServiceKeep();
	virtual ~AsioIOServiceKeep();
	IOService& GetIOService();

protected:
	void thread_keep();
};

#endif /* SRC_ASIOIOSERVICEKEEP_H_ */
