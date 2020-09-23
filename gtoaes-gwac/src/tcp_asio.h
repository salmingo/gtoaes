/*
 * @file tcp_asio.h 封装基于boost::asio实现的TCP服务器和客户端
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 * @note
 * @li 封装TCP服务器
 * @li 封装TCP客户端
 */

#ifndef TCP_ASIO_H_
#define TCP_ASIO_H_

#include <boost/signals2.hpp>
#include <boost/circular_buffer.hpp>
#include <string>
#include "ioservice_keep.h"

#define TCP_BUFF_SIZE	1500

// 声明tcp_client回调函数
typedef boost::signals2::signal<void (const long, const long)> tcpc_cbfunc;
// 基于boost::signals2声明插槽类型
typedef tcpc_cbfunc::slot_type tcpc_cbtype;

class tcp_client {
public:
	/* 构造函数和析构函数 */
	tcp_client();
	virtual ~tcp_client();

public:
	friend class tcp_server;	// 声明tcp_server为友元对象
	/* 声明数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock; //< 基于boost::mutex的互斥锁
	typedef boost::circular_buffer<char> crcbuff;

public:
	/* 属性函数 */
	/*!
	 * @brief 检查套接口是否处于打开状态
	 * @return
	 * 套接口是否打开标志
	 */
	bool is_open();

public:
	/* 注册回调函数 */
	/*!
	 * @brief 注册响应connect()的回调函数
	 * @param slot 插槽函数
	 */
	void register_connect(const tcpc_cbtype& slot);
	/*!
	 * @brief 注册响应receive()的回调函数
	 * @param slot 插槽函数
	 */
	void register_receive(const tcpc_cbtype& slot);
	/*!
	 * @brief 注册响应send()的回调函数
	 * @param slot 插槽函数
	 */
	void register_send(const tcpc_cbtype& slot);

public:
	/* 套接口操作接口函数 */
	/*!
	 * @brief 尝试连接服务器
	 * @param host 服务器名称或地址
	 * @param port 服务器端口
	 */
	void try_connect(const std::string host, const int port);
	/*!
	 * @brief 关闭套接口, 断开网络连接
	 * @note
	 * 当主程序需要主动断开网络连接时, 调用本函数安全释放相关资源
	 */
	void close();
	/*!
	 * @brief 查看接收缓冲区中第一个字符
	 * @param first 存储第一个字符的变量
	 * @return
	 * 缓冲区数据长度
	 */
	int lookup(char& first);
	/*!
	 * @brief 查找指定标志符的起始位置
	 * @param flag  一条完整信息的结束符
	 * @param n     结束符长度
	 * @return
	 * 结束符的起始位置. 若找不到则返回-1
	 */
	int lookup(const char* flag, const int len);
	/*!
	 * @brief 从缓冲区中读取指定长度数据, 并清除缓冲区
	 * @param buff 输出缓冲区
	 * @param len  待读取数据长度
	 * @return
	 * 实际读取数据长度
	 */
	int read(char* buff, const int len);
	/*!
	 * @brief 发送指定数据
	 * @param buff 待发送数据缓冲区
	 * @param len  待发送数据长度
	 * @return
	 * 实际发送数据
	 */
	int write(const char* buff, const int len);
	/*!
	 * @brief 查看时间戳
	 * @return
	 * 最后一次发送或接收信息的时间
	 */
	int get_timeflag();

protected:
	/*!
	 * @brief 访问套接口
	 * @return
	 * 套接口
	 */
	boost::asio::ip::tcp::socket& get_socket();

	/* 响应async_函数的回调函数 */
	/*!
	 * @brief 处理网络连接结果
	 * @param ec 错误代码
	 */
	void handle_connect(const boost::system::error_code& ec);
	/*!
	 * @brief 处理收到的网络信息
	 * @param ec 错误代码
	 * @param n  接收数据长度, 量纲: 字节
	 */
	void handle_receive(const boost::system::error_code& ec, const int n);
	/*!
	 * @brief 处理异步网络信息发送结果
	 * @param ec 错误代码
	 * @param n  发送数据长度, 量纲: 字节
	 */
	void handle_send(const boost::system::error_code& ec, const int n);

	/*!
	 * @brief 尝试异步接收收到的信息
	 */
	void start_receive();
	/*!
	 * @brief 继续异步发送缓冲区中的信息
	 */
	void start_send();
	/*!
	 * @brief 启动工作流程
	 */
	void start();
	/*!
	 * @brief 更新时间戳
	 */
	void update_timeflag();

private:
	// 成员变量
	int tmflag_;	//< 最后一次接收或发送信息的时间戳, 量纲: 秒
	ioservice_keep keep_;	//< 长效io_service
	boost::asio::ip::tcp::socket socket_;	//< 套接字

	tcpc_cbfunc cbconnect_;		//< 回调函数: 连接
	tcpc_cbfunc cbrecv_;			//< 回调函数: 接收
	tcpc_cbfunc cbsend_;			//< 回调函数: 发送

	boost::mutex mtxrecv_;				//< receive互斥锁
	boost::mutex mtxsend_;				//< send互斥锁
	boost::shared_array<char> bufrcv_;	//< 接收缓冲区
	boost::shared_array<char> bufsnd_;	//< 发送缓冲区
	boost::shared_ptr<crcbuff> crcrcv_;	//< 循环接收缓冲区
	boost::shared_ptr<crcbuff> crcsnd_;	//< 循环发送缓冲区
};

typedef boost::shared_ptr<tcp_client> tcpcptr;
// 声明tcp_server的回调函数
typedef boost::signals2::signal<void (const tcpcptr&, const long)> tcps_cbfunc;
// 基于boost::signals2声明tcp_server回调函数插槽类型
typedef tcps_cbfunc::slot_type tcps_cbtype;

class tcp_server {
public:
	explicit tcp_server();
	virtual ~tcp_server();

protected:
	/*!
	 * @brief 处理收到的网络连接请求
	 * @param ec 错误代码
	 */
	void handle_accept(const tcpcptr& client, const boost::system::error_code& ec);

	/*!
	 * @brief 启动异步接收客户端的网络连接请求
	 */
	void start_accept();

public:
	/* 注册回调函数 */
	/*!
	 * @brief 注册响应accept()的回调函数
	 * @param slot 插槽函数
	 */
	void register_accept(const tcps_cbtype& slot);
	/*!
	 * @brief 启动网络监听服务
	 * @param port 监听端口
	 * @return
	 * 服务启动结果. true, 成功; false, 失败
	 */
	bool start(const int port);

private:
	/* 声明成员变量 */
	ioservice_keep keep_;		//< 封装维护io_serive的长期有效性
	boost::asio::ip::tcp::acceptor acceptor_;	//< 套接口
	tcps_cbfunc cbaccept_;		//< accept回调函数
};
typedef boost::shared_ptr<tcp_server> tcpsptr;

#endif /* TCP_ASIO_H_ */
