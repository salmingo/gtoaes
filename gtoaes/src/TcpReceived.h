/**
 * @file TcpReceived.h
 * @brief 封装定义: TCP客户端的读出结果
 * @version 0.1
 * @date 2020-11-21
 * @author 卢晓猛
 */

#ifndef SRC_TCPRECEIVED_H_
#define SRC_TCPRECEIVED_H_

#include "AsioTCP.h"

/*!
 * @struct TcpReceived
 * @brief 网络事件
 */
struct TcpReceived {
	using Pointer = boost::shared_ptr<TcpReceived>;

	TcpCPtr client;	///< 网络连接
	int peer;		///< 主机类型
	bool hadRcvd;	///< 远程套接口关闭

public:
	TcpReceived(TcpCPtr _client, int _peer, bool _rcvd) {
		client = _client;
		peer   = _peer;
		hadRcvd= _rcvd;
	}

	static Pointer Create(TcpCPtr _client, int _peer, bool _rcvd) {
		return Pointer(new TcpReceived(_client, _peer, _rcvd));
	}
};
using TcpRcvPtr = TcpReceived::Pointer;
using TcpRcvQue = std::deque<TcpRcvPtr>;

#endif /* SRC_TCPRECEIVED_H_ */
