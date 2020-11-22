/**
 * @file DomeSlit.h
 * @brief 天窗控制接口
 * - 单模天窗: 一个天窗内仅有一个观测系统
 * - 多模天窗: 一个天窗内有多个观测系统
 * @version 0.1
 * @date 2020-11-20
 * @author 卢晓猛
 */

#ifndef SRC_DOMESLIT_H_
#define SRC_DOMESLIT_H_

#include <string>
#include <vector>
#include <boost/smart_ptr/shared_ptr.hpp>
#include "AsioTCP.h"

/*!
 * @brief 多模天窗
 */
struct SlitMultiplex {
	using Pointer = boost::shared_ptr<SlitMultiplex>;
	std::string gid;	///< 组标志
	TcpCPtr client;		///< 网络连接
	bool kvtype;		///< 通信协议格式
	int state;			///< 天窗状态

public:
	SlitMultiplex(const std::string& _gid) {
		gid    = _gid;
		kvtype = true;
		state  = -1;
	}

	static Pointer Create(const std::string& _gid) {
		return Pointer(new SlitMultiplex(_gid));
	}

	int IsMatched(const std::string& _gid) {
		if (gid == _gid) return 1;
		if (_gid.empty()) return 2;
		return 0;
	}

	bool IsOpen() {
		bool isOpen = client.use_count() && client->IsOpen();
		if (!isOpen && client.use_count()) {
			client.reset();
			state = -1;
		}
		return isOpen;
	}
};
using SlitMulPtr = SlitMultiplex::Pointer;
using SlitMulVec = std::vector<SlitMulPtr>;

/*!
 * @brief 单模天窗
 */
struct SlitSimplex {
	using Pointer = boost::shared_ptr<SlitSimplex>;
	std::string gid;	///< 组标志
	std::string uid;	///< 单元标志
	TcpCPtr client;		///< 网络连接
	bool kvtype;		///< 通信协议格式
	int state;			///< 天窗状态

public:
	SlitSimplex(const std::string& _gid, const std::string& _uid) {
		gid    = _gid;
		uid    = _uid;
		kvtype = true;
		state  = -1;
	}

	static Pointer Create(const std::string& _gid, const std::string& _uid) {
		return Pointer(new SlitSimplex(_gid, _uid));
	}

	bool IsOpen() {
		bool isOpen = client.use_count() && client->IsOpen();
		if (!isOpen && client.use_count()) {
			client.reset();
			state = -1;
		}
		return isOpen;
	}
};
using SlitSimPtr = SlitSimplex::Pointer;
using SlitSimVec = std::vector<SlitSimPtr>;

#endif /* SRC_DOMESLIT_H_ */
