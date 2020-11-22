/**
 * @file AsioTCP.cpp 定义文件, 基于boost::asio实现TCP通信接口
 */

#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include "AsioTCP.h"

using namespace boost::system;
using namespace boost::placeholders;
using namespace boost::asio;
using std::string;

/////////////////////////////////////////////////////////////////////
/*--------------------- 客户端 ---------------------*/
TcpClient::TcpClient(bool modeAsync)
	: sock_(keep_.GetIOService()) {
	mode_async_ = modeAsync;
	byte_read_  = 0;
	buf_read_.reset(new char[TCP_PACK_SIZE]);
	if (mode_async_) {
		crcbuf_read_.set_capacity(TCP_PACK_SIZE * 50);
		crcbuf_write_.set_capacity(TCP_PACK_SIZE * 50);
	}
}

TcpClient::~TcpClient() {

}

TcpClient::TCP::socket& TcpClient::Socket() {
	return sock_;
}

bool TcpClient::Connect(const std::string& host, const uint16_t port) {
	try {
		TCP::resolver resolver(keep_.GetIOService());
		TCP::resolver::query query(host, boost::lexical_cast<string>(port));
		TCP::resolver::iterator itertor = resolver.resolve(query);

		if (mode_async_) {
			sock_.async_connect(*itertor,
					boost::bind(&TcpClient::handle_connect, shared_from_this(),
						placeholders::error));
		}
		else {
			sock_.connect(*itertor);
			sock_.set_option(socket_base::keep_alive(true));
			start_read();
		}
		return true;
	}
	catch(std::exception& ex) {
		return false;
	}
}

bool TcpClient::ShutDown(int how) {
	try {
		sock_.shutdown(how == 0 ? TCP::socket::shutdown_receive
				: (how == 1 ? TCP::socket::shutdown_send : TCP::socket::shutdown_both));
		return true;
	}
	catch(std::exception& ex) {
		return false;
	}
}

bool TcpClient::Close() {
	try {
		sock_.close();
		return true;
	}
	catch(std::exception& ex) {
		return false;
	}
}

bool TcpClient::IsOpen() {
	return sock_.is_open();
}

int TcpClient::Read(char* data, const int n, const int from) {
	if (!data || n <= 0 || from < 0)
		return 0;

	MtxLck lck(mtx_read_);
	int end(from + n), to_read;
	if (mode_async_) {
		to_read = crcbuf_read_.size() > end ? n : crcbuf_read_.size() - from;
		if (to_read) {
			int i, j;
			for (i = 0, j = from; i < to_read; ++i, ++j)
				data[i] = crcbuf_read_[j];
			crcbuf_read_.erase_begin(end);
		}
	}
	else {
		to_read = byte_read_ > end ? n : byte_read_ - from;
		if (to_read) {
			memcpy(data, buf_read_.get() + from, to_read);
			byte_read_ -= end;
		}
	}
	return to_read;
}

int TcpClient::Write(const char* data, const int n) {
	if (!data || n <= 0)
		return 0;

	MtxLck lck(mtx_write_);
	int had_write(n);
	if (mode_async_) {
		int wait_write(crcbuf_write_.size());
		for (int i = 0; i < n; ++i)
			crcbuf_write_.push_back(data[i]);
		if (!wait_write)
			start_write();
	}
	else {
		had_write = sock_.write_some(buffer(data, n));
	}
	return had_write;
}

int TcpClient::Lookup(char* first) {
	MtxLck lck(mtx_read_);
	int n = mode_async_ ? crcbuf_read_.size() : byte_read_;
	if (first && n) *first = mode_async_ ? crcbuf_read_[0] : buf_read_[0];
	return n;
}

int TcpClient::Lookup(const char* flag, const int n, const int from) {
	if (!flag || n <= 0 || from < 0)
		return -1;

	MtxLck lck(mtx_read_);
	int end = (mode_async_ ? crcbuf_read_.size() : byte_read_) - n;
	int pos, i(0), j;

	if (mode_async_) {
		for (pos = from; pos <= end; ++pos) {
			for (i = 0, j = pos; i < n && flag[i] == crcbuf_read_[j]; ++i, ++j);
			if (i == n) break;
		}
	}
	else {
		for (pos = from; pos <= end; ++pos) {
			for (i = 0, j = pos; i < n && flag[i] == buf_read_[j]; ++i, ++j);
			if (i == n) break;
		}
	}

	return (i == n ? pos : -1);
}

void TcpClient::Start() {
	sock_.set_option(socket_base::keep_alive(true));
	start_read();
}

void TcpClient::RegisterConnect(const CBSlot& slot) {
	cbconn_.disconnect_all_slots();
	cbconn_.connect(slot);
}

void TcpClient::RegisterRead(const CBSlot& slot) {
	cbread_.disconnect_all_slots();
	cbread_.connect(slot);
}

void TcpClient::RegisterWrite(const CBSlot& slot) {
	cbwrite_.disconnect_all_slots();
	cbwrite_.connect(slot);
}

void TcpClient::start_read() {
	if (sock_.is_open()) {
		sock_.async_read_some(buffer(buf_read_.get(), TCP_PACK_SIZE),
				boost::bind(&TcpClient::handle_read, shared_from_this(),
					placeholders::error, placeholders::bytes_transferred));
	}
}

void TcpClient::start_write() {
	int towrite(crcbuf_write_.size());
	if (towrite) {
		sock_.async_write_some(buffer(crcbuf_write_.linearize(), towrite),
				boost::bind(&TcpClient::handle_write, shared_from_this(),
					placeholders::error, placeholders::bytes_transferred));
	}
}

/* 响应async_函数的回调函数 */
void TcpClient::handle_connect(const error_code& ec) {
	cbconn_(shared_from_this(), ec);
	if (!ec) {
		sock_.set_option(socket_base::keep_alive(true));
		start_read();
	}
}

void TcpClient::handle_read(const error_code& ec, int n) {
	if (!ec) {
		MtxLck lck(mtx_read_);
		if (mode_async_) {
			for (int i = 0; i < n; ++i)
				crcbuf_read_.push_back(buf_read_[i]);
		}
		else byte_read_ = n;
	}
	cbread_(shared_from_this(), ec);
	if (!ec) start_read();
}

void TcpClient::handle_write(const error_code& ec, int n) {
	if (!ec) {
		MtxLck lck(mtx_write_);
		crcbuf_write_.erase_begin(n);
		start_write();
	}
	cbwrite_(shared_from_this(), ec);
}

/////////////////////////////////////////////////////////////////////
/*--------------------- 服务器 ---------------------*/
TcpServer::TcpServer()
	: accept_(keep_.GetIOService()) {
}

TcpServer::~TcpServer() {
	if (accept_.is_open()) {
		error_code ec;
		accept_.close(ec);
	}
}

void TcpServer::RegisterAccept(const CBSlot &slot) {
	cbfunc_.connect(slot);
}

bool TcpServer::CreateServer(uint16_t port, bool v6) {
	try {
		TCP::endpoint endpt(v6 ? TCP::v6() : TCP::v4(), port);
		accept_.open(endpt.protocol());
		accept_.bind(endpt);
		accept_.listen(TCP::socket::max_listen_connections);
		start_accept();
		return true;
	}
	catch (std::exception& ex) {
		return false;
	}
}

void TcpServer::start_accept() {
	if (accept_.is_open()) {
		TcpCPtr client = TcpClient::Create();
		accept_.async_accept(client->Socket(), boost::bind(&TcpServer::handle_accept, this, client, placeholders::error));
	}
}

void TcpServer::handle_accept(const TcpCPtr client, const error_code& ec) {
	if (!ec) {
		cbfunc_(client, shared_from_this());
		client->Start();
	}
	start_accept();
}

/////////////////////////////////////////////////////////////////////
