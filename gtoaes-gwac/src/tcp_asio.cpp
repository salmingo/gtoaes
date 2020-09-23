/*
 * @file tcp_asio.cpp 封装基于boost::asio实现的TCP服务器和客户端
 * @date 2017-01-27
 * @version 0.1
 * @author Xiaomeng Lu
 */

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include "tcp_asio.h"
#include "GLog.h"

using boost::asio::ip::tcp;

///////////////////////////////////////////////////////////////////////////////
/* 定义: tcp_client*/
// 构造函数
tcp_client::tcp_client()
	: socket_(keep_.get_service())
	, tmflag_(0) {
	bufrcv_.reset(new char[TCP_BUFF_SIZE]);
	bufsnd_.reset(new char[TCP_BUFF_SIZE]);
	crcrcv_ = boost::make_shared<crcbuff>(TCP_BUFF_SIZE * 10);
	crcsnd_ = boost::make_shared<crcbuff>(TCP_BUFF_SIZE * 10);
}

// 析构函数
tcp_client::~tcp_client() {
	if (socket_.is_open()) socket_.close();
}

int tcp_client::get_timeflag() {
	return tmflag_;
}

void tcp_client::update_timeflag() {
	tmflag_ = boost::posix_time::second_clock::universal_time().time_of_day().total_seconds();
}

/* 属性函数 */
// 套接口开关标志
bool tcp_client::is_open() {
	return socket_.is_open();
}

tcp::socket& tcp_client::get_socket() {
	return socket_;
}

// 注册响应connect()的回调函数
void tcp_client::register_connect(const tcpc_cbtype& slot) {
	cbconnect_.connect(slot);
}

// 注册响应receive()的回调函数
void tcp_client::register_receive(const tcpc_cbtype& slot) {
	mutex_lock lock(mtxrecv_);
	if (!cbrecv_.empty()) cbrecv_.disconnect_all_slots();
	cbrecv_.connect(slot);
}

// 注册响应send()的回调函数
void tcp_client::register_send(const tcpc_cbtype& slot) {
	mutex_lock lock(mtxsend_);
	if (!cbsend_.empty()) cbsend_.disconnect_all_slots();
	cbsend_.connect(slot);
}

// 处理网络连接结果
void tcp_client::handle_connect(const boost::system::error_code& ec) {
	cbconnect_((const long) this, ec.value());
	if (!ec) {
		if (!crcrcv_->empty()) crcrcv_->clear();	// 当重用实例对象时
		if (!crcsnd_->empty()) crcsnd_->clear();
		update_timeflag();
		start_receive();
	}
}

// 处理信息接收结果
void tcp_client::handle_receive(const boost::system::error_code& ec, const int n) {
	if (!ec) {
		mutex_lock lock(mtxrecv_);
		for(int i = 0; i < n; ++i) crcrcv_->push_back(bufrcv_[i]);
		update_timeflag();
	}
	cbrecv_((const long) this, !ec ? 0 : 1);
	if (!ec) start_receive();
}

// 处理信息发送结果
void tcp_client::handle_send(const boost::system::error_code& ec, const int n) {
	if (!ec) {
		mutex_lock lock(mtxsend_);
		crcsnd_->erase_begin(n);
		update_timeflag();
		cbsend_((const long) this, n);
		start_send();
	}
}

// 尝试连接远程主机
void tcp_client::try_connect(const std::string host, const int port) {
	tcp::resolver resolver(keep_.get_service());
	tcp::resolver::query query(host, boost::lexical_cast<std::string>(port));
	tcp::resolver::iterator itertor = resolver.resolve(query);

	socket_.async_connect(*itertor,
			boost::bind(&tcp_client::handle_connect, this, boost::asio::placeholders::error));
}

// 关闭套接口
void tcp_client::close() {
	if (socket_.is_open()) socket_.close();
}

// 尝试查看第一个已接收字符
int tcp_client::lookup(char& first) {
	mutex_lock lock(mtxrecv_);
	int n(crcrcv_->size());
	if (n > 0) first = crcrcv_->at(0);

	return n;
}

// 尝试查找制定字符串
int tcp_client::lookup(const char* flag, const int len) {
	if (flag == NULL || len <= 0) return -1;
	mutex_lock lock(mtxrecv_);

	int n(crcrcv_->size() - len), pos, i, j;
	for (pos = 0; pos <= n; ++pos) {
		for (i = 0, j = pos; i < len && flag[i] == crcrcv_->at(j); ++i, ++j);
		if (i == len) break;
	}

	return pos > n ? -1 : pos;
}

// 尝试读取已接收信息
int tcp_client::read(char* buff, const int len) {
	if (!buff || len <= 0 ) return 0;
	mutex_lock lock(mtxrecv_);

	int n = crcrcv_->size(), i;
	if (n > 0) {
		if (n > len) n = len;
		for (i = 0; i < n; ++i) buff[i] = crcrcv_->at(i);
		crcrcv_->erase_begin(n);
	}

	return n;
}

// 尝试发送信息
int tcp_client::write(const char* buff, const int len) {
	if (!buff || len <= 0) return 0;
	mutex_lock lock(mtxsend_);

	int n0(crcsnd_->size()), n(crcsnd_->capacity() - n0), i;
	if (n > 0) {
		if (n > len) n = len;
		for (i = 0; i < n; ++i) crcsnd_->push_back(buff[i]);
		if (!n0) start_send();
	}

	return n;
}

// 尝试异步接收信息
void tcp_client::start_receive() {
	if (socket_.is_open()) {
		socket_.async_read_some(boost::asio::buffer(bufrcv_.get(), TCP_BUFF_SIZE),
							boost::bind(&tcp_client::handle_receive, this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
	}
}

// 继续异步发送缓冲区中的信息
void tcp_client::start_send() {
	if (socket_.is_open()) {
		int len(crcsnd_->size()), i;

		if (len > 0) {
			if (len > TCP_BUFF_SIZE) len = TCP_BUFF_SIZE;
			for (i = 0; i < len; ++i) bufsnd_[i] = crcsnd_->at(i);
			socket_.async_write_some(boost::asio::buffer(bufsnd_.get(), len),
					boost::bind(&tcp_client::handle_send, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
		}
	}
}

// 启动工作流程
void tcp_client::start() {
	update_timeflag();
	start_receive();
}

///////////////////////////////////////////////////////////////////////////////
/* 定义: tcp_server*/
// 构造函数
tcp_server::tcp_server()
	: acceptor_(keep_.get_service()) {
}

// 析构函数
tcp_server::~tcp_server() {
	if (acceptor_.is_open()) acceptor_.close();
}

// 注册回调函数
void tcp_server::register_accept(const tcps_cbtype& slot) {
	if (!cbaccept_.empty()) cbaccept_.disconnect_all_slots();
	cbaccept_.connect(slot);
}

// 处理收到连接请求
void tcp_server::handle_accept(const tcpcptr& client, const boost::system::error_code& ec) {
	if (!ec) {
		if (!cbaccept_.empty()) {
			cbaccept_(client, (const long) this);
			client->start();
		}
	}
	start_accept();
}

// 等待接收连接
void tcp_server::start_accept() {
	if (acceptor_.is_open()) {
		tcpcptr client = boost::make_shared<tcp_client>();	//< 客户端连接
		acceptor_.async_accept(client->get_socket(),
				boost::bind(&tcp_server::handle_accept, this, client, boost::asio::placeholders::error));
	}
}

// 启动服务器
bool tcp_server::start(const int port) {
	try {
		tcp::endpoint endpoint(tcp::v4(), port);
		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen(10);
		start_accept();

		return true;
	}
	catch (boost::system::error_code& ec) {
		return false;
	}
}
