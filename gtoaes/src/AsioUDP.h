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

#ifndef SRC_ASIOUDP_H_
#define SRC_ASIOUDP_H_

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/signals2.hpp>
#include <boost/smart_ptr/shared_array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>
#include "AsioIOServiceKeep.h"

#define UDP_PACK_SIZE		1500

using std::string;
using namespace boost::system;

class UdpSession : public boost::enable_shared_from_this<UdpSession> {
public:
	UdpSession();
	virtual ~UdpSession();

public:
	/* 数据类型 */
	using Pointer = boost::shared_ptr<UdpSession>;
	/*!
	 * @brief 声明回调函数及插槽
	 * @param 1 客户端对象
	 * @param 2 错误描述
	 */
	using CallbackFunc = boost::signals2::signal<void (const Pointer, const error_code&)>;
	using CBSlot = CallbackFunc::slot_type;
	using UDP = boost::asio::ip::udp;	// boost::ip::udp类型
	using CBuff = boost::shared_array<char>;	//< char型数组
	using MtxLck = boost::unique_lock<boost::mutex>;	//< 信号灯互斥锁

protected:
	AsioIOServiceKeep keep_;	//< 提供boost::asio::io_service对象, 并在实例存在期间保持其运行
	UDP::socket sock_;			//< 套接口
	UDP::endpoint remote_;		//< 远程套接口
	bool connected_;			//< 连接标志
	boost::mutex mtx_read_;		//< 互斥锁: 从套接口读取
	boost::mutex mtx_write_;	//< 互斥锁: 向套接口写入
	CBuff buf_read_;			//< 缓冲区: 单次接收
	int byte_read_;				//< 单次接收数据长度
	CallbackFunc cbconn_;	//< 连接回调函数
	CallbackFunc cbread_;	//< 接收回调函数

	bool block_reading_;	//< 阻塞式读出
	boost::condition_variable cvread_;	//< 阻塞读出时的条件变量

public:
	/* 接口 */
	/*!
	 * @brief 创建对象实例, 并返回其指针
	 */
	static Pointer Create() {
		return Pointer(new UdpSession);
	}

	/*!
	 * @brief 打开端口
	 * @param port 服务端口. 当==0时由系统指定
	 * @param v6   是否使用V6协议
	 */
	bool Open(uint16_t port = 0, bool v6 = false);
	/*!
	 * @brief 关闭套接口
	 */
	void Close();
	/*!
	 * @brief 检查套接口是否已经打开
	 * @return
	 * 套接口打开标识
	 */
	bool IsOpen();
	/*!
	 * @brief 查看套接字
	 * @return
	 * UDP套接字
	 */
	UDP::socket &GetSocket();
	/*!
	 * @brief 显示连接远程主机
	 * @param ipPeer 远程主机IP地址
	 * @param port   远程主机端口
	 * @return
	 * 连接结果
	 */
	bool Connect(const string& ipPeer, const uint16_t port);
	/*!
	 * @brief 读取已接收数据
	 * @param buff  存储网络接收数据的缓冲区, 由用户分配存储空间
	 * @param n     数据长度, 量纲: 字节
	 * @return
	 * 存储数据缓冲区地址
	 */
	const char *Read(char *buff, int &n);
	/*!
	 * @brief 阻塞直至有数据可以读出, 或限时到达
	 * @param buff     存储网络接收数据的缓冲区, 由用户分配存储空间
	 * @param n        可读出数据长度
	 * @param millisec 阻塞时长, 量纲: 毫秒
	 * @return
	 * 可读出数据地址
	 */
	const char* BlockRead(char *buff, int& n, const int millisec = 100);
	/*!
	 * @brief 将数据写入套接口
	 * @param data 待发送数据
	 * @param n    待发送数据长度, 量纲: 字节
	 */
	void Write(const void *data, const int n);
	/*!
	 * @brief 将数据发送给远程主机
	 * @param ipPeer     远程主机IP地址
	 * @param portPeer   远程主机端口
	 * @param data       待发送数据
	 * @param n          待发送数据长度, 量纲: 字节
	 */
	void WriteTo(const string& ipPeer, const uint16_t portPeer, const void *data, int n);
	/*!
	 * @brief 注册异步连接回调函数
	 * @param slot 插槽
	 */
	void RegisterConnect(const CBSlot &slot);
	/*!
	 * @brief 注册异步接收回调函数
	 * @param slot 插槽
	 */
	void RegisterRead(const CBSlot &slot);

protected:
	/*!
	 * @brief 尝试从UDP端口读取接收到的信息
	 */
	void start_read();
	/*!
	 * @brief 处理收到的网络信息
	 * @param ec 错误代码
	 * @param n  接收数据长度, 量纲: 字节
	 */
	void handle_read(const error_code& ec, const int n);
};
using UdpPtr = UdpSession::Pointer;

#endif /* SRC_ASIOUDP_H_ */
