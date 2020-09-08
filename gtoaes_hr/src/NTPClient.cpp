/*
 * @file NTPClient.h 类NTPClient定义文件
 * @author       卢晓猛
 * @description  检查本机与NTP服务器的时间偏差, 并修正本机时钟
 * @version      1.0
 * @date         2016年10月29日
 */

#include <stdio.h>
#include <string.h>
#include <boost/make_shared.hpp>
#include "NTPClient.h"
#include "GLog.h"

#define JAN_1970			0x83AA7E80
#define NTP_PCK_LEN		48

#define NTPFRAC(x)	(4294 * (x) + ((1981 * (x)) >> 11))
#define USEC(x)		(((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16))

#define LI			0
#define VN			3
#define MODE			3
#define STRATUM		0
#define POLL			4
#define PREC			-6
#define UINTMAX		4294967295.0

NTPPtr make_ntp(const char* hostIP, const uint16_t port, const int tSync) {
	return boost::make_shared<NTPClient>(hostIP, port, tSync);
}

NTPClient::NTPClient(const char* hostIP, const uint16_t port, const int tSync) {
	host_ = hostIP;
	port_ = port;
	sock_ = -1;
	tSync_ = tSync * 0.001;
	pack_.reset(new char[NTP_PCK_LEN * 8]);
	offset_ = 0.0;
	valid_  = false;
	nfail_  = 0;
	autoSync_ = false;
	thrd_.reset(new boost::thread(boost::bind(&NTPClient::thread_body, this)));;
}

NTPClient::~NTPClient() {
	if (thrd_.unique()) {
		thrd_->interrupt();
		thrd_->join();
	}
	if (sock_ >= 0) close(sock_);
}

void NTPClient::SetHost(const char* ip, const uint16_t port) {
	mutex_lock lock(mtx_);
	host_ = ip;
	port_ = port;
	if (sock_ >= 0) {
		close(sock_);
		sock_ = -1;
	}
}

void NTPClient::SetSyncLimit(const int tSync) {
	tSync_ = tSync * 0.001;
}

void NTPClient::SynchClock() {
	if (valid_ && (offset_ >= tSync_ || offset_ <= -tSync_)) {
		struct timeval  tv;
		double t;

		gettimeofday(&tv, NULL);
		t = tv.tv_sec + tv.tv_usec * 1E-6 + offset_;
		tv.tv_sec = (time_t) t;
		tv.tv_usec= (suseconds_t) ((t - tv.tv_sec) * 1E6);
		settimeofday(&tv, NULL);

		valid_ = false;
	}
}

void NTPClient::thread_body() {
	boost::chrono::minutes duration(1);
	struct addrinfo *res = NULL;
	struct ntp_packet pack;
	struct timeval  tv;
	double t1, t2, t3, t4, delay;
	unsigned char *id;

	while (1) {
		boost::this_thread::sleep_for(duration);

		if (sock_ < 0) {// 尝试建立网络连接
			mutex_lock lock(mtx_);
			char portstr[10];
			struct addrinfo addr;

			sprintf(portstr, "%d", port_);
			memset(&addr, 0, sizeof(addr));
			addr.ai_family   = AF_UNSPEC;
			addr.ai_socktype = SOCK_DGRAM;
			addr.ai_protocol = IPPROTO_UDP;
			if (getaddrinfo(host_.c_str(), portstr, &addr, &res)
					|| ((sock_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)) {
				continue;
			}
		}

		if (get_time(res, &pack)) {
			gettimeofday(&tv, NULL);
			t1 = pack.originate_timestamp.coarse + (double) pack.originate_timestamp.fine / UINTMAX;
			t2 = pack.receive_timestamp.coarse   + (double) pack.receive_timestamp.fine   / UINTMAX;
			t3 = pack.transmit_timestamp.coarse  + (double) pack.transmit_timestamp.fine  / UINTMAX;
			t4 = JAN_1970 + tv.tv_sec + tv.tv_usec * 1E-6;

			offset_ = ((t2 - t1) + (t3 - t4)) * 0.5;
			delay   = (t4 - t1) - (t3 - t2);
			valid_  = delay < 1E-3;

			if (offset_ >= tSync_ || offset_ <= -tSync_) {
				if (autoSync_) SynchClock();
				id = pack.reference_identifier;
				_gLog.Write(LOG_WARN, NULL, "Clock drifts %.6f seconds. RefSrc=%c%c%c%c. delay=%.3f msecs",
						offset_, id[0], id[1], id[2], id[3], delay * 1000);
			}
		}
		else {
			_gLog.Write(LOG_WARN, NULL, "Failed to communicate with NTP server<%s:%u>", host_.c_str(), port_);
			// 时钟偏差有效期: 5周期
			if (++nfail_ >= 5 && valid_) valid_ = false;
		}
	}
}

void NTPClient::construct_packet() {
	char version = 1;
	long tmp_wrd;
	struct timeval tv;
	char* data = pack_.get();

	memset(data, 0, NTP_PCK_LEN);
	version = 4;
	tmp_wrd = htonl((LI << 30) | (version << 27)
			| (MODE << 24) | (STRATUM << 16) | (POLL << 8) |(PREC & 0xff));
	memcpy(data, &tmp_wrd, sizeof(tmp_wrd));
	tmp_wrd = htonl(1 << 16);
	memcpy (data + 4, &tmp_wrd, sizeof(tmp_wrd));
	memcpy (data + 8, &tmp_wrd, sizeof(tmp_wrd));
	gettimeofday(&tv, NULL);
	tmp_wrd = htonl(JAN_1970 + tv.tv_sec);
	memcpy (data + 40, &tmp_wrd, sizeof(tmp_wrd));
	tmp_wrd = htonl((unsigned int) ((double) tv.tv_usec * UINTMAX * 1E-6));
	memcpy(data + 44, &tmp_wrd, sizeof(tmp_wrd));
}

int NTPClient::get_time(struct addrinfo *addr, struct ntp_packet *ret_time) {
	fd_set pending_data;
	struct timeval block_time = {10, 0};
	uint32_t len = addr->ai_addrlen;
	int rslt(0);
	char* data = pack_.get();

	construct_packet();
	if (sendto(sock_, data, NTP_PCK_LEN, 0, addr->ai_addr, len) < 0) {
		_gLog.Write(LOG_WARN, "NTPClient::sendto", strerror(errno));
		close(sock_);
		sock_ = -1;
	}
	else {
		FD_ZERO(&pending_data);
		FD_SET(sock_, &pending_data);
		if (select(sock_ + 1, &pending_data, NULL, NULL, &block_time) > 0) {
			if (recvfrom(sock_, (void*)data, NTP_PCK_LEN * 8, 0, addr->ai_addr, &len) < 0) {
				_gLog.Write(LOG_WARN, "NTPClient::recvfrom", strerror(errno));
				close(sock_);
				sock_ = -1;
			}
			else {
				valid_ = true;
				nfail_ = 0;
				rslt   = 1;

				ret_time->leap_ver_mode				= data[0];//ntohl(data[0]);
				ret_time->stratum					= data[1];//ntohl(data[1]);
				ret_time->poll						= data[2];//ntohl(data[2]);
				ret_time->precision					= data[3];//ntohl(data[3]);
				ret_time->root_delay				= ntohl(*(int*)&(data[4]));
				ret_time->root_dispersion			= ntohl(*(int*)&(data[8]));
				strcpy((char*) ret_time->reference_identifier, &data[12]);
				ret_time->reference_timestamp.coarse= ntohl(*(int*)&(data[16]));
				ret_time->reference_timestamp.fine	= ntohl(*(int*)&(data[20]));
				ret_time->originate_timestamp.coarse= ntohl(*(int*)&(data[24]));
				ret_time->originate_timestamp.fine	= ntohl(*(int*)&(data[28]));
				ret_time->receive_timestamp.coarse	= ntohl(*(int*)&(data[32]));
				ret_time->receive_timestamp.fine	= ntohl(*(int*)&(data[36]));
				ret_time->transmit_timestamp.coarse	= ntohl(*(int*)&(data[40]));
				ret_time->transmit_timestamp.fine	= ntohl(*(int*)&(data[44]));
			}
		}
	}

	return rslt;
}

void NTPClient::EnableAutoSynch(bool bEnabled) {
	autoSync_ = bEnabled;
}
