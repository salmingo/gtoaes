/**
 * @file     AsciiNonkvProtocol.h
 * @brief    解析和构建非键值对类型字符串通信协议
 * @version  1.0
 * @date     2020-11-09
 * @author   卢晓猛
 */

#ifndef ASCIINONKVPROTOCOL_H_
#define ASCIINONKVPROTOCOL_H_

#include <boost/thread/mutex.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/shared_array.hpp>
#include <string>

using std::string;

//////////////////////////////////////////////////////////////////////////////
/* 协议关键字 */
#define NONKVTYPE_READY			"ready"
#define NONKVTYPE_STATE			"status"
#define NONKVTYPE_UTC			"utc"
#define NONKVTYPE_FINDHOME		"home"
#define NONKVTYPE_HOMESYNC		"sync"
#define NONKVTYPE_SLEWTO		"slew"
#define NONKVTYPE_GUIDE			"guide"
#define NONKVTYPE_PARK			"park"
#define NONKVTYPE_ABTSLEW		"abortslew"
#define NONKVTYPE_MOUNT			"currentpos"
#define NONKVTYPE_SLIT			"slit"
#define NONKVTYPE_MCOVER		"mirr"
#define NONKVTYPE_FWHM			"fwhm"
#define NONKVTYPE_FOCUS			"focus"
#define NONKVTYPE_RAIN			"rain"

//////////////////////////////////////////////////////////////////////////////
/* 协议基类 */
struct nonkv_proto_base {
	string type;	///< 协议类型
	string gid;		///< 组编号
	string uid;		///< 单元编号
	string cid;		///< 相机编号

public:
	nonkv_proto_base &operator=(const nonkv_proto_base &other) {
		if (this != &other) {
			type = other.type;
			gid  = other.gid;
			uid  = other.uid;
			cid  = other.cid;
		}
		return *this;
	}
};
typedef boost::shared_ptr<nonkv_proto_base> nonkvbase;

/*!
 * @brief 将nonkv_proto_base继承类的boost::shared_ptr型指针转换为nonkvbase类型
 * @param proto 协议指针
 * @return
 * nonkvbase类型指针
 */
template <class T> nonkvbase to_nonkvbase(T proto) {
	return boost::static_pointer_cast<nonkv_proto_base>(proto);
}

/*!
 * @brief 将nonkvbase类型指针转换为其继承类的boost::shared_ptr型指针
 * @param proto 协议指针
 * @return
 * nonkvbase继承类指针
 */
template <class T> boost::shared_ptr<T> from_nonkvbase(nonkvbase proto) {
	return boost::static_pointer_cast<T>(proto);
}
//////////////////////////////////////////////////////////////////////////////
/* 协议 */
struct nonkv_proto_ready : public nonkv_proto_base {
	int ready;

public:
	nonkv_proto_ready() {
		type = NONKVTYPE_READY;
		ready = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_ready> nonkvready;

struct nonkv_proto_state : public nonkv_proto_base {
	int state;

public:
	nonkv_proto_state() {
		type = NONKVTYPE_STATE;
		state = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_state> nonkvstate;

struct nonkv_proto_utc : public nonkv_proto_base {
	string utc;

public:
	nonkv_proto_utc() {
		type = NONKVTYPE_UTC;
	}
};
typedef boost::shared_ptr<nonkv_proto_utc> nonkvutc;

struct nonkv_proto_mount : public nonkv_proto_base {
	double ra, dec;

public:
	nonkv_proto_mount() {
		type = NONKVTYPE_MOUNT;
		ra = dec = 0.0;
	}
};
typedef boost::shared_ptr<nonkv_proto_mount> nonkvmount;

struct nonkv_proto_focus : public nonkv_proto_base {
	int position;

public:
	nonkv_proto_focus() {
		type = NONKVTYPE_FOCUS;
		position = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_focus> nonkvfocus;

struct nonkv_proto_mcover : public nonkv_proto_base {
	int state;

public:
	nonkv_proto_mcover() {
		type = NONKVTYPE_MCOVER;
		state = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_mcover> nonkvmcover;

struct nonkv_proto_slit : public nonkv_proto_base {
	int state;

public:
	nonkv_proto_slit() {
		type  = NONKVTYPE_SLIT;
		state = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_slit> nonkvslit;

struct nonkv_proto_rain : public nonkv_proto_base {
	int state;

public:
	nonkv_proto_rain() {
		type = NONKVTYPE_RAIN;
		state = 0;
	}
};
typedef boost::shared_ptr<nonkv_proto_rain> nonkvrain;

//////////////////////////////////////////////////////////////////////////////
class NonkvProtocol {
public:
	NonkvProtocol();
	virtual ~NonkvProtocol();

public:
	/* 数据类型 */
	typedef boost::shared_ptr<NonkvProtocol> Pointer;
	typedef boost::unique_lock<boost::mutex> MtxLck;	///< 互斥锁
	typedef boost::shared_array<char> ChBuff;	///< 字符数组

protected:
	/* 成员变量 */
	boost::mutex mtx_;	///< 互斥锁
	const int szProto_;	///< 协议最大长度: 1400
	int iBuf_;			///< 存储区索引
	ChBuff buff_;		///< 存储区
	const int lenGid_;	///< 组标志字符串长度
	const int lenUid_;	///< 单元标志字符串长度
	const int lenCid_;	///< 相机标志字符串长度
	const int lenReady_;	///< 关键字长度
	const int lenState_;	///< 关键字长度
	const int lenUtc_;		///< 关键字长度
	const int lenMount_;	///< 关键字长度
	const int lenSlit_;		///< 关键字长度
	const int lenMCover_;	///< 关键字长度
	const int lenFocus_;	///< 关键字长度
	const int lenRain_;		///< 关键字长度

public:
	/* 接口 */
	static Pointer Create() {
		return Pointer(new NonkvProtocol);
	}

	/*!
	 * @brief 解析字符串, 转换为nonkv_xxx协议
	 * @param rcvd  接收到的字符串
	 */
	nonkvbase Resove(const char* rcvd);
	/*!
	 * @brief 封装转台搜索零点
	 */
	const char* CompactFindHome  (const string& gid, const string& uid, int& n);
	/*!
	 * @brief 封装转台同步零点
	 */
	const char* CompactHomeSync  (const string& gid, const string& uid, double ra, double dec, int& n);
	/*!
	 * @brief 封装转台指向赤道坐标
	 */
	const char* CompactSlewto    (const string& gid, const string& uid, double ra, double dec, int& n);
	/*!
	 * @brief 封装转台导星
	 */
	const char* CompactGuide     (const string& gid, const string& uid, double d_ra, double d_dec, int& n);
	/*!
	 * @brief 封装转台复位
	 */
	const char* CompactPark      (const string& gid, const string& uid, int& n);
	/*!
	 * @brief 封装转台中止指向
	 */
	const char* CompactAbortSlew (const string& gid, const string& uid, int& n);
	/*!
	 * @brief 封装开关天窗
	 */
	const char* CompactSlit      (const string& gid, const string& uid, int cmd, int& n);
	/*!
	 * @brief 封装开关镜盖
	 */
	const char* CompactMirrCover (const string& gid, const string& uid, const string& cid, int cmd, int& n);
	/*!
	 * @brief 封装自动调焦
	 */
	const char* CompactFWHM      (const string& gid, const string& uid, const string& cid, double fwhm, int& n);
	/*!
	 * @brief 封装手动调焦
	 */
	const char* CompactFocus     (const string& gid, const string& uid, const string& cid, int pos, int& n);

protected:
	/*!
	 * @brief 从缓冲区中提取通信协议存储区
	 */
	char* get_buffer();
	/*!
	 * @brief 解析转台准备结果
	 */
	nonkvbase resolve_ready     (const char* rcvd, int pos);
	/*!
	 * @brief 解析转台工作状态
	 */
	nonkvbase resolve_state     (const char* rcvd, int pos);
	/*!
	 * @brief 解析转台时标
	 */
	nonkvbase resolve_utc       (const char* rcvd, int pos);
	/*!
	 * @brief 解析转台位置
	 */
	nonkvbase resolve_mount     (const char* rcvd, int pos);
	/*!
	 * @brief 解析天窗状态
	 */
	nonkvbase resolve_slit      (const char* rcvd, int pos);
	/*!
	 * @brief 解析镜盖状态
	 */
	nonkvbase resolve_mirr_cover(const char* rcvd, int pos);
	/*!
	 * @brief 解析焦点位置
	 */
	nonkvbase resolve_focus     (const char* rcvd, int pos);
	/*!
	 * @brief 解析降雨标志
	 */
	nonkvbase resolve_rain      (const char* rcvd, int pos);
};
typedef NonkvProtocol::Pointer NonkvProtoPtr;

#endif /* SRC_ASCIINONKVPROTOCOL_H_ */
