/*
 * @file NTPClient.h 类NTPClient声明文件
 * @author       卢晓猛
 * @description  检查本机与NTP服务器的时间偏差, 并修正本机时钟
 * @version      1.0
 * @date         2016年10月29日
 * @note
 * (1) 每分钟检查一次本机与NTP的时间偏差. 当时间偏差较大时, 在日志文件中记录并提示
 * (2) 当需要修正本机时钟时, 直接采用最近一次的时间偏差
 * (3) 修正本机时钟
 */

#ifndef NTPCLIENT_H_
#define NTPCLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>
#include <boost/thread.hpp>
#include <boost/smart_ptr.hpp>

class NTPClient {
public:
	typedef struct _ntp_time {
		unsigned int coarse;
		unsigned int fine;
	}ntp_time;

	/**
	 * offset = ((T2 - T1) + (T3 - T4)) / 2
	 * delay  = (T4 - T1) - (T3 - T2)
	 *
	 * local_time_corrected = local_time_pc + offset
	 */

	struct ntp_packet {
		unsigned char leap_ver_mode;
		unsigned char stratum;
		char poll;
		char precision;
		int root_delay;
		int root_dispersion;
		unsigned char reference_identifier[4];		// in the case of stratum 0 or 1, this is a four-octet, left-justified
										// zero-padded ASCII string, referring to clock resource.
										// in the case of stratum 2 and greater, this is the four-octet internet
										// address of the primary reference host
		ntp_time reference_timestamp;	// 本地时钟最后设置或修正的时间
		ntp_time originate_timestamp;	// T1: 由本机向NTP服务器发送请求的本机时间
		ntp_time receive_timestamp;		// T2: 请求抵达服务器时的本地时间
		ntp_time transmit_timestamp;	// T3: 反馈离开服务器时的本地实际
										// T4: 反馈抵达本机时的时间
	};
	/* 声明数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock; //< 基于boost::mutex的互斥锁
	typedef boost::shared_ptr<boost::thread> threadptr;  //< 线程指针类型
	typedef boost::shared_array<char> carray;		//< 字符型数组

public:
	/*!
	 * @brief 构造函数
	 * @param hostIP  NTP服务IPv4地址
	 * @param port    NTP服务端口, 默认123
	 * @param tSyn    修正时钟的最大时钟偏差, 量纲: 毫秒
	 */
	NTPClient(const char* hostIP, const uint16_t port = 123, const int tSync = 5);
	virtual ~NTPClient();

protected:
	/*!
	 * @brief 构建待发送网络信息
	 */
	void construct_packet();
	/*!
	 * @brief 采集NTP时钟
	 * @param addr      地址
	 * @param ret_time  NTP数据包
	 * @return
	 */
	int get_time(struct addrinfo* addr, struct ntp_packet* ret_time);
	/*!
	 * @brief 线程主体
	 */
	void thread_body();

public:
	/*!
	 * @brief 设置NTP服务器
	 * @param ip   IPv4地址
	 * @param port 服务端口
	 */
	void SetHost(const char* ip, const uint16_t port = 123);
	/*!
	 * @brief 设置时钟修正阈值
	 * @param tSync 时钟修正阈值, 量纲: 毫秒
	 */
	void SetSyncLimit(const int tSync = 5);
	/*!
	 * @brief 同步本机时钟
	 */
	void SynchClock();
	/*!
	 * @brief 启用或禁止自动时钟同步
	 */
	void EnableAutoSynch(bool bEnabled = true);

protected:
	/* 声明成员变量 */
	boost::mutex mtx_;		//< 互斥区
	threadptr    thrd_;		//< 线程指针
	std::string  host_;		//< NTP服务器的IPv4地址
	uint16_t     port_;		//< NTP服务器的端口
	int          sock_;		//< SOCKET套接字
	carray       pack_;		//< 网络交互信息
	double       offset_;	//< 时钟偏差, 量纲: 秒
	bool         valid_;		//< 数据有效性
	int          nfail_;		//< 时钟偏差检查失败次数
	double       tSync_;		//< 修正本地时钟的最大时钟偏差
	bool         autoSync_;	//< 是否自动修正时钟偏差
};
typedef boost::shared_ptr<NTPClient> NTPPtr; //< NTPclient指针
/*!
 * @brief 工厂函数, 生成新的NTPClient指针
 * @return
 * 指针创建结果
 */
extern NTPPtr make_ntp(const char* hostIP, const uint16_t port, const int tSync);

#endif /* NTPCLIENT_H_ */
