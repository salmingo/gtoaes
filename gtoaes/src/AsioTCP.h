/**
 * @file AsioTCP 声明文件, 基于boost::asio实现TCP通信接口
 * @version 0.2
 * @date 2017-10-01
 * - 基于v0.1优化重新实现
 * - 类TCPClient封装客户端接口
 * - 类TCPServer封装服务器接口
 * @version 0.3
 * @date 2017-11-11
 * - 支持无缓冲工作模式
 * - 客户端建立连接后设置KEEP_ALIVE
 * - 优化缓冲区操作
 * @version 1.0
 * @date 2020-10-02
 * @note
 * - 优化
 */

#ifndef SRC_ASIOTCP_H_
#define SRC_ASIOTCP_H_

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/smart_ptr/shared_array.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string.h>
#include <string>
#include <vector>
#include "AsioIOServiceKeep.h"

using namespace boost::system;

/////////////////////////////////////////////////////////////////////
/*--------------------- 客户端 ---------------------*/
#define TCP_PACK_SIZE		1500

class TcpClient : public boost::enable_shared_from_this<TcpClient> {
public:
	using Pointer = boost::shared_ptr<TcpClient>;
	/*!
	 * @brief 声明回调函数及插槽
	 * @param 1 客户端对象
	 * @param 2 实例指针
	 */
	using CallbackFunc = boost::signals2::signal<void (const Pointer, const error_code&)>;
	using CBSlot = CallbackFunc::slot_type;
	using TCP = boost::asio::ip::tcp;	// boost::ip::tcp类型
	using CRCBuff = boost::circular_buffer<char>;	// 字符型循环数组
	using CBuff = boost::shared_array<char>;	//< char型数组
	using MtxLck = boost::unique_lock<boost::mutex>;	//< 信号灯互斥锁

protected:
	bool mode_async_;		//< 异步读写/模式
	AsioIOServiceKeep keep_;	//< 提供boost::asio::io_service对象, 并在实例存在期间保持其运行
	TCP::socket sock_;			//< 套接口
	boost::mutex mtx_read_;		//< 互斥锁: 从套接口读取
	boost::mutex mtx_write_;	//< 互斥锁: 向套接口写入
	CBuff buf_read_;			//< 缓冲区: 单次接收
	int byte_read_;				//< 单次接收数据长度
	CRCBuff crcbuf_read_;		//< 缓冲区: 所有接收
	CRCBuff crcbuf_write_;		//< 缓冲区: 所有待写入
	CallbackFunc  cbconn_;	//< connect回调函数
	CallbackFunc  cbread_;	//< read回调函数
	CallbackFunc  cbwrite_;	//< write回调函数

public:
	TcpClient(bool modeAsync = true);
	virtual ~TcpClient();
	/*!
	 * @brief 创建TcpClient::Pointer实例
	 * @return
	 * shared_ptr<TcpClient>类型实例指针
	 */
	static Pointer Create() {
		return Pointer(new TcpClient);
	}
	/*!
	 * @brief 查看套接字
	 * @return
	 * TCP套接字
	 */
	TCP::socket& Socket();
	/*!
	 * @brief 同步方式尝试连接服务器
	 * @param host  服务器地址或名称
	 * @param port  服务端口
	 * @return
	 * 连接结果
	 */
	bool Connect(const std::string& host, const uint16_t port);
	/*!
	 * @brief 关闭在套接口上的操作
	 * @param how 操作类型. 0: 读; 1: 写; 2: 读写
	 * @return
	 * 套接字关闭结果.
	 */
	bool ShutDown(int how);
	/*!
	 * @brief 关闭套接字
	 * @return
	 * 套接字关闭结果.
	 */
	bool Close();
	/*!
	 * @brief 检查套接字状态
	 * @return
	 * 套接字状态
	 */
	bool IsOpen();
	/*!
	 * @brief 从已接收信息中读取指定数据长度, 并从缓冲区中清除被读出数据
	 * @param data 输出存储区
	 * @param n    待读取数据长度
	 * @param from 从from开始读取
	 * @return
	 * 实际读取数据长度
	 */
	int Read(char* data, const int n, const int from = 0);
	/*!
	 * @brief 发送指定数据
	 * @param data 待发送数据存储区指针
	 * @param n    待发送数据长度
	 * @return
	 * 实际发送数据长度
	 */
	int Write(const char* data, const int n);
	/*!
	 * @brief 查找已接收信息中第一个字符
	 * @param flag 标识符
	 * @return
	 * 已接收信息长度
	 */
	int Lookup(char* first = NULL);
	/*!
	 * @brief 查找已接收信息中第一次出现flag的位置
	 * @param flag 标识字符串
	 * @param n    标识字符串长度
	 * @param from 从from开始查找flag
	 * @return
	 * 标识串第一次出现位置. 若flag不存在则返回-1
	 */
	int Lookup(const char* flag, const int n, const int from = 0);
	/*!
	 * @brief 服务器端建立网络连接后调用, 启动接收流程
	 */
	void Start();
	/*!
	 * @brief 注册connect回调函数, 处理与服务器的连接结果
	 * @param slot 函数插槽
	 */
	void RegisterConnect(const CBSlot& slot);
	/*!
	 * @brief 注册read_some回调函数, 处理收到的网络信息
	 * @param slot 函数插槽
	 */
	void RegisterRead(const CBSlot& slot);
	/*!
	 * @brief 注册write_some回调函数, 处理网络信息发送结果
	 * @param slot 函数插槽
	 */
	void RegisterWrite(const CBSlot& slot);

protected:
	/*!
	 * @brief 尝试接收网络信息
	 */
	void start_read();
	/*!
	 * @brief 尝试发送缓冲区数据
	 */
	void start_write();
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
	void handle_read(const boost::system::error_code& ec, int n);
	/*!
	 * @brief 处理异步网络信息发送结果
	 * @param ec 错误代码
	 * @param n  发送数据长度, 量纲: 字节
	 */
	void handle_write(const boost::system::error_code& ec, int n);
};
using TcpCPtr = TcpClient::Pointer;
using TcpCVec = std::vector<TcpCPtr> ; ///< 网络连接存储区

/////////////////////////////////////////////////////////////////////
/*--------------------- 服务器 ---------------------*/
class TcpServer : public boost::enable_shared_from_this<TcpServer> {
protected:
	using TCP = boost::asio::ip::tcp;

public:
	using Pointer = boost::shared_ptr<TcpServer>;
	/*!
	 * @brief 声明回调函数及插槽
	 * @param 1 客户端对象
	 * @param 2 实例指针
	 */
	using CallbackFunc = boost::signals2::signal<void (const TcpCPtr, const Pointer)>;
	using CBSlot = CallbackFunc::slot_type;

protected:
	AsioIOServiceKeep keep_;	//< 提供boost::asio::io_service对象, 并在实例存在期间保持其运行
	TCP::acceptor accept_;		//< 网络服务
	CallbackFunc cbfunc_;		//< 回调函数

public:
	TcpServer();
	virtual ~TcpServer();

	/*!
	 * @brief 创建一个实例
	 * @return
	 * 实例指针
	 */
	static Pointer Create() {
		return Pointer(new TcpServer);
	}

	/*!
	 * @brief 注册accept回调函数, 处理服务器收到的网络连接请求
	 * @param slot 函数插槽
	 */
	void RegisterAccept(const CBSlot &slot);
	/*!
	 * @brief 尝试在port指定的端口上创建TCP网络服务
	 * @param port 服务端口
	 * @param v6   服务类型. true: V6, false: V4
	 * @return
	 * TCP网络服务创建结果
	 */
	bool CreateServer(uint16_t port, bool v6 = false);

protected:
	/*!
	 * @brief 启动网络监听
	 */
	void start_accept();
	/*!
	 * @brief 处理收到的网络连接请求
	 * @param client 建立套接字
	 * @param ec     错误代码
	 */
	void handle_accept(const TcpCPtr client, const boost::system::error_code& ec);
};
using TcpSPtr = boost::shared_ptr<TcpServer>;

/////////////////////////////////////////////////////////////////////
#endif /* SRC_ASIOTCP_H_ */
