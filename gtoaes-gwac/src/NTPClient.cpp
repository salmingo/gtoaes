/*
 * @file NTPClient.h 类NTPClient定义文件
 * @author       卢晓猛
 * @description  检查本机与NTP服务器的时间偏差, 并修正本机时钟
 * @version      1.0
 * @date         2016年10月29日
 */

#include <stdio.h>
#include "NTPClient.h"
#include "GLog.h"
#include "globaldef.h"

#define JAN_1970		0x83AA7E80
#define NTP_PCK_LEN		48

#define NTPFRAC(x)	(4294 * (x) + ((1981 * (x)) >> 11))
#define USEC(x)		(((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16))

#define LI			0
#define VN			3
#define MODE		3
#define STRATUM		0
#define POLL		4
#define PREC		-6
#define UINTMAX		4294967295

NTPClient::NTPClient(const char* hostIP, const int port, const int tSyn)
	: m_port(port),
	  m_sock(-1),
	  m_tSyn(tSyn * 0.001) {
	strcpy(m_host, hostIP);
	m_packet = new char[NTP_PCK_LEN*8];
	m_offset = m_delay = 0.0;
	m_valid  = false;
	m_nFail  = 0;
	m_autoSync = false;
	m_thread = new boost::thread(boost::bind(&NTPClient::ThreadBody, this));;
}

NTPClient::~NTPClient() {
	if (m_thread) {
		m_thread->interrupt();
		m_thread->join();
		delete m_thread;
	}
	if (m_sock >= 0) close(m_sock);
	delete []m_packet;
}

void NTPClient::SetHost(const char* ip, const int port) {
	mutex_lock lock(m_mutex);
	strcpy(m_host, ip);
	m_port = port;
	if (m_sock >= 0) {
		close(m_sock);
		m_sock = -1;
	}
}

void NTPClient::SynchClock() {
	if (m_valid && (m_offset >= m_tSyn || m_offset <= -m_tSyn)) {
		struct timeval  tv;
		double t;

		gettimeofday(&tv, NULL);
		t = tv.tv_sec + tv.tv_usec * 1E-6 + m_offset;
		tv.tv_sec = (time_t) t;
		tv.tv_usec= (suseconds_t) ((t - tv.tv_sec) * 1E6);
		settimeofday(&tv, NULL);

		m_valid = false;
	}
}

void NTPClient::ThreadBody() {
	boost::chrono::minutes duration(1);
	int rc;
	struct addrinfo addr, *res = NULL;
	struct ntp_packet new_packet;
	char portstr[10];
	struct timeval  tv;
	double t1, t2, t3, t4;
	unsigned char *id;

	while (1) {
		boost::this_thread::sleep_for(duration);

		if (m_sock < 0) {
			mutex_lock lock(m_mutex);
			sprintf(portstr, "%d", m_port);
			memset(&addr, 0, sizeof(addr));
			addr.ai_family = AF_UNSPEC;
			addr.ai_socktype = SOCK_DGRAM;
			addr.ai_protocol = IPPROTO_UDP;
			if ((rc = getaddrinfo(m_host, portstr, &addr, &res)) != 0) {
				continue;
			}
			if ((m_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
				continue;
			}
		}

		if (GetNTPTime(m_sock, res, &new_packet)) {
			gettimeofday(&tv, NULL);
			t1 = new_packet.originate_timestamp.coarse + (double) new_packet.originate_timestamp.fine / UINTMAX;
			t2 = new_packet.receive_timestamp.coarse + (double) new_packet.receive_timestamp.fine / UINTMAX;
			t3 = new_packet.transmit_timestamp.coarse + (double) new_packet.transmit_timestamp.fine / UINTMAX;
			t4 = JAN_1970 + tv.tv_sec + tv.tv_usec * 1E-6;

			m_offset = ((t2 - t1) + (t3 - t4)) * 0.5;
			m_delay  = (t4 - t1) - (t3 - t2);

			m_valid = true;
			m_nFail = 0;

			if (m_offset >= m_tSyn || m_offset <= -m_tSyn) {
				id = new_packet.reference_identifier;
				gLog.Write(NULL, LOG_WARN, "Clock drifts %.6f seconds. RefSrc=%c%c%c%c. delay=%.3f msecs",
						m_offset, id[0], id[1], id[2], id[3], m_delay * 1000);

				if (m_autoSync) SynchClock();
			}
		}
		else {
			gLog.Write(NULL, LOG_WARN, "Failed to communicate with NTP server");
			// 时钟偏差有效期: 5周期
			if (++m_nFail >= 5 && m_valid) m_valid = false;
		}
	}
}

void NTPClient::ConstructPacket() {
	char version = 1;
	long tmp_wrd;
	struct timeval  tv;

	memset(m_packet, 0, NTP_PCK_LEN);
	version = 4;
	tmp_wrd = htonl((LI << 30) | (version << 27)
			| (MODE << 24) | (STRATUM << 16) | (POLL << 8) |(PREC & 0xff));
	memcpy(m_packet, &tmp_wrd, sizeof(tmp_wrd));
	tmp_wrd = htonl(1 << 16);
	memcpy (&m_packet[4], &tmp_wrd, sizeof(tmp_wrd));
	memcpy (&m_packet[8], &tmp_wrd, sizeof(tmp_wrd));
	gettimeofday(&tv, NULL);
	tmp_wrd = htonl(JAN_1970 + tv.tv_sec);
	memcpy (&m_packet[40], &tmp_wrd, sizeof(tmp_wrd));
	tmp_wrd = htonl((unsigned int) ((double) tv.tv_usec * UINTMAX * 1E-6));
	memcpy(&m_packet[44], &tmp_wrd, sizeof(tmp_wrd));
}

int NTPClient::GetNTPTime(int sock, struct addrinfo *addr, struct ntp_packet *ret_time) {
	fd_set pending_data;
	struct timeval block_time;
	unsigned int data_len = addr->ai_addrlen;
	int count = 0;
	int result;
	char *data = m_packet;

	ConstructPacket();
	if ((result = sendto(sock, data, NTP_PCK_LEN, 0, addr->ai_addr, data_len)) < 0) {
		gLog.Write("NTPClient::sendto", LOG_WARN, strerror(errno));
		return 0;
	}

	FD_ZERO(&pending_data);
	FD_SET(sock, &pending_data);
	block_time.tv_sec = 10;
	block_time.tv_usec= 0;
	if (select(sock + 1, &pending_data, NULL, NULL, &block_time) > 0) {
		if ((count = recvfrom(sock, (void*)data, (unsigned long int)(NTP_PCK_LEN * 8),
				0, addr->ai_addr, &data_len)) < 0) {
			gLog.Write("NTPClient::recvfrom", LOG_WARN, strerror(errno));
			return 0;
		}

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

		return 1;
	}

	return 0;
}

void NTPClient::EnableAutoSynch(bool bEnabled) {
	m_autoSync = bEnabled;
}
