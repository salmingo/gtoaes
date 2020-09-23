/*
 * @file ioservice_keep.h 封装boost::asio::io_service, 维持run()在生命周期内的有效性
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * @li boost::asio::io_service::run()在响应所注册的异步调用后自动退出. 为了避免退出run()函数,
 * 建立ioservice_keep维护其长期有效性
 * @li 使用shared_ptr管理指针
 */

#ifndef IOSERVICE_KEEP_H_
#define IOSERVICE_KEEP_H_

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>

using boost::asio::io_service;

class ioservice_keep : private boost::noncopyable {
public:
	// 构造函数与析构函数
	ioservice_keep();
	virtual ~ioservice_keep();

public:
	// 数据类型
	typedef io_service::work work;
	typedef boost::thread thread;

public:
	// 属性函数
	io_service& get_service();

private:
	// 成员变量
	io_service ios_;					//< io_service对象
	boost::shared_ptr<work> work_;		//< io_service守护对象
	boost::shared_ptr<thread> thread_;	//< 线程
};

#endif /* IOSERVICE_KEEP_H_ */
