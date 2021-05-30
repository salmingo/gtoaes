/*!
 * @file UdpSession 基于boost::asio封装UDP通信
 * @version 0.1
 * @date May 23, 2017
 * @note
 * - 基于boost::asio封装实现UDP通信服务器和客户端
 * @version 0.2
 * @date Oct 30, 2020
 * @note
 * - 优化
 */

#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "AsioUDP.h"

using namespace boost::asio;
using namespace boost::placeholders;

UdpSession::UdpSession()
	: sock_(keep_.GetIOService()) {
	connected_     = false;
	block_reading_ = false;
	byte_read_     = 0;
	buf_read_.reset(new char[UDP_PACK_SIZE]);
}

UdpSession::~UdpSession() {
	Close();
}

bool UdpSession::Open(uint16_t port, bool v6) {
	try {
		sock_.open(v6 ? UDP::v6() : UDP::v4());
		if (port) {// 在端口启动UDP接收服务
			sock_.bind(UDP::endpoint(v6 ? UDP::v6() : UDP::v4(), port));
			start_read();
		}
		return true;
	}
	catch(error_code &ex) {
		return false;
	}
}

void UdpSession::Close() {
	if (sock_.is_open()) {
		sock_.close();
		connected_ = false;
	}
}

bool UdpSession::IsOpen() {
	return sock_.is_open();
}

UdpSession::UDP::socket &UdpSession::GetSocket() {
	return sock_;
}

bool UdpSession::Connect(const string& ipPeer, const uint16_t port) {
	try {
		sock_.connect(UDP::endpoint(ip::address::from_string(ipPeer), port));
		connected_ = true;
		start_read();
		return true;
	}
	catch(system_error& ex) {
		return false;
	}
}

const char *UdpSession::Read(char *buff, int &n) {
	MtxLck lck(mtx_read_);
	if ((n = byte_read_)) memcpy(buff, buf_read_.get(), n);
	byte_read_ = 0;
	return n == 0 ? NULL : buff;
}

const char* UdpSession::BlockRead(char *buff, int& n, const int millisec) {
	MtxLck lck(mtx_read_);
	boost::posix_time::milliseconds t(millisec);

	block_reading_ = true;
	cvread_.timed_wait(lck, t);
	if ((n = byte_read_)) memcpy(buff, buf_read_.get(), n);
	byte_read_ = 0;
	return n == 0 ? NULL : buff;
}

void UdpSession::Write(const void *data, const int n) {
	MtxLck lck(mtx_write_);
	if (connected_) sock_.send(buffer(data, n));
	else sock_.send_to(buffer(data, n), remote_);
}

void UdpSession::WriteTo(const string& ipPeer, const uint16_t portPeer, const void *data, int n) {
	try {
		MtxLck lck(mtx_write_);
		UDP::endpoint remote(ip::address::from_string(ipPeer), portPeer);

		sock_.send_to(buffer(data, n), remote);
		start_read();
	}
	catch(system_error& ex) {
	}
}

void UdpSession::RegisterConnect(const CBSlot &slot) {
	cbconn_.disconnect_all_slots();
	cbconn_.connect(slot);
}

void UdpSession::RegisterRead(const CBSlot &slot) {
	cbread_.disconnect_all_slots();
	cbread_.connect(slot);
}

void UdpSession::start_read() {
	if (connected_) {
		sock_.async_receive(buffer(buf_read_.get(), UDP_PACK_SIZE),
				boost::bind(&UdpSession::handle_read, this,
						placeholders::error, placeholders::bytes_transferred));
	}
	else {
		sock_.async_receive_from(buffer(buf_read_.get(), UDP_PACK_SIZE), remote_,
				boost::bind(&UdpSession::handle_read, this,
						placeholders::error, placeholders::bytes_transferred));
	}
}

void UdpSession::handle_read(const error_code& ec, const int n) {
	if (!ec || ec == error::message_size) {
		byte_read_ = n;
		if (block_reading_) cvread_.notify_one();
		else cbread_(shared_from_this(), ec);
		start_read();
	}
}
