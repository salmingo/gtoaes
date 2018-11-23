/*
 * @file MountProto.h 声明文件, 定义与转台相关通信协议格式, 并封装其操作
 * @version 0.2
 * @date 2017-10-02
 */

#ifndef MOUNTPROTOCOL_H_
#define MOUNTPROTOCOL_H_

#include <string.h>
#include <list>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>

using std::string;
//////////////////////////////////////////////////////////////////////////////
#define MPTYPE_READY		"ready"
#define MPTYPE_STATE		"state"
#define MPTYPE_UTC			"utc"
#define MPTYPE_POSITION		"position"
#define MPTYPE_FOCUS		"focus"
#define MPTYPE_MCOVER		"mcover"

/*------------------ ASCII协议基类 ------------------*/
struct mntproto_base {
	string type;//< 协议类型
	string gid;	//< 组标志
	string uid;	//< 单元标志
};
typedef boost::shared_ptr<mntproto_base> mpbase;

struct mntproto_ready : public mntproto_base {// 转台是否准备就绪
	int ready;

public:
	mntproto_ready() {
		type  = MPTYPE_READY;
		ready = 0;
	}
};
typedef boost::shared_ptr<mntproto_ready> mpready;

struct mntproto_state : public mntproto_base {// 转台工作状态
	int state;

public:
	mntproto_state() {
		type  = MPTYPE_STATE;
		state = 0;
	}
};
typedef boost::shared_ptr<mntproto_state> mpstate;

struct mntproto_utc : public mntproto_base {// 转台UTC时间
	string utc;

public:
	mntproto_utc() {
		type = MPTYPE_UTC;
	}
};
typedef boost::shared_ptr<mntproto_utc> mputc;

struct mntproto_position : public mntproto_base {// 转台位置
	double ra;	//< 赤经, 量纲: 角度
	double dc;	//< 赤纬, 量纲: 角度

public:
	mntproto_position() {
		type = MPTYPE_POSITION;
		ra = dc = 1E30;
	}
};
typedef boost::shared_ptr<mntproto_position> mpposition;

struct mntproto_focus : public mntproto_base {// 焦点位置
	string cid;		//< 相机标志
	int position;	//< 焦点位置, 量纲: 微米

public:
	mntproto_focus() {
		type = MPTYPE_FOCUS;
		position = INT_MIN;
	}
};
typedef boost::shared_ptr<mntproto_focus> mpfocus;

struct mntproto_mcover : public mntproto_base {// 镜盖状态
	string cid;		//< 相机标志
	int state;		//< 镜盖状态

public:
	mntproto_mcover() {
		type  = MPTYPE_MCOVER;
		state = 0;
	}
};
typedef boost::shared_ptr<mntproto_mcover> mpmcover;

//////////////////////////////////////////////////////////////////////////////
class MountProto {
public:
	MountProto();
	MountProto(const string& gid, const string& uid);
	virtual ~MountProto();

protected:
	/* 数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock;	//< 互斥锁
	typedef boost::shared_array<char> carray;	//< 字符数组
	typedef std::list<string> listring;			//< string列表

protected:
	/* 成员变量 */
	boost::mutex mtx_;	//< 互斥锁
	int ibuf_;		//< 存储区索引
	carray buff_;	//< 存储区
	string gid_;		//< 组标志
	string uid_;		//< 单元标志

public:
	/*------------------ 接口: 封装通信协议, 用于套接字输出 ------------------*/
	/*!
	 * @brief 封装协议home
	 * @param ra    赤经轴搜索零点指令
	 * @param dec   赤纬轴搜索零点指令
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactFindhome(const bool ra, const bool dec, int& n);
	/*!
	 * @brief 封装协议sync
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactHomesync(const double ra, const double dec, int& n);
	/*!
	 * @brief 封装协议slew
	 * @param ra    赤经, 量纲: 角度
	 * @param dec   赤纬, 量纲: 角度
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactSlew(const double ra, const double dec, int& n);
	/*!
	 * @brief 封装协议guide
	 * @param ra    赤经偏差, 量纲: 角度
	 * @param dec   赤纬偏差, 量纲: 角度
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactGuide(const double ra, const double dec, int& n);
	/*!
	 * @brief 封装协议park
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactPark(int& n);
	/*!
	 * @brief 封装协议abortslew
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactAbortslew(int& n);
	/*!
	 * @brief 封装协议mirr
	 * @param cid     相机标志
	 * @param command 镜盖开关指令. 0: 关闭; 其它: 打开
	 * @param n       封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactMCover(const string& cid, const int command, int& n);
	/*!
	 * @brief 封装协议fwhm
	 * @param cid   相机标志
	 * @param fwhm  半高全宽, 量纲: 像素
	 * @param n     封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactFwhm(const string& cid, const double fwhm, int& n);
	/*!
	 * @brief 封装协议focus
	 * @param cid       相机标志
	 * @param position  焦点位置, 量纲: 微米
	 * @param n         封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactFocus(const string& cid, const int position, int& n);
	/*!
	 * @brief 封装协议focsync
	 * @param cid    相机标志
	 * @param n      封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactFocusync(const string& cid, int& n);

public:
	/*------------------ 接口: 解析通信协议 ------------------*/
	/*!
	 * @brief 解析通信协议
	 * @param rcvd 接收到的信息
	 * @param type 协议类型
	 * @return
	 * 转换为apbase类型的通信协议
	 */
	mpbase Resolve(const char* rcvd);

protected:
	// 功能
	/*!
	 * @brief 从存储区中选取可用空间
	 * @return
	 * 可用空间地址
	 */
	char* get_buff();
	/*!
	 * @brief 初始化关键字
	 */
	void init_keyword();
};
typedef boost::shared_ptr<MountProto> MountPtr;	//< AsciiProto指针
/*!
 * @brief 工厂函数, 创建MountPtr指针
 * @return
 */
extern MountPtr make_mount();
extern MountPtr make_mount(const string& gid, const string& uid);

/*!
 * @brief 将mntproto_base继承类的boost::shared_ptr型指针转换为mpbase类型
 * @param proto 协议指针
 * @return
 * mpbase类型指针
 */
template <class T>
mpbase to_mpbase(T proto) {
	return boost::static_pointer_cast<mntproto_base>(proto);
}

/*!
 * @brief 将mpbase类型指针转换为其继承类的boost::shared_ptr型指针
 * @param proto 协议指针
 * @return
 * mpbase继承类指针
 */
template <class T>
boost::shared_ptr<T> from_mpbase(mpbase proto) {
	return boost::static_pointer_cast<T>(proto);
}

#endif /* MOUNTPROTOCOL_H_ */
