/**
 * @file AnnexProtocol.h 附属设备通信协议
 * @version 0.1
 * @date 2019-09-29
 */

#ifndef ANNEXPROTOCOL_H_
#define ANNEXPROTOCOL_H_

#include <string.h>
#include <list>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>

using std::string;
//////////////////////////////////////////////////////////////////////////////
#define MPTYPE_RAIN		"rain"
#define MPTYPE_SLIT		"slit"
#define MPTYPE_FOCUS	"focus"
#define MPTYPE_FWHM		"fwhm"

/*------------------ ASCII协议基类 ------------------*/
struct annexproto_base {
	string type;//< 协议类型
};
typedef boost::shared_ptr<annexproto_base> annpbase;

struct annexproto_rain : public annexproto_base {// 雨量
	int value;	//< 降水量

public:
	annexproto_rain() {
		type = MPTYPE_RAIN;
		value = 0;
	}
};
typedef boost::shared_ptr<annexproto_rain> annprain;

struct annexproto_slit : public annexproto_base {// 天窗指令与状态
	string gid;
	string uid;
	int cmd;	//< 控制指令
	int state;	//< 天窗状态

public:
	annexproto_slit() {
		type = MPTYPE_SLIT;
		cmd   = INT_MIN;
		state = INT_MIN;
	}
};
typedef boost::shared_ptr<annexproto_slit> annpslit;

struct annexproto_focus : public annexproto_base {// 焦点位置
	string gid;		//< 组标志
	string uid;		//< 单元标志
	string cid;		//< 相机标志
	int position;	//< 焦点位置, 量纲: 微米

public:
	annexproto_focus() {
		type = MPTYPE_FOCUS;
		position = INT_MIN;
	}
};
typedef boost::shared_ptr<annexproto_focus> annpfocus;

struct annexproto_fwhm : public annexproto_base {// 半高全宽
	string gid;		//< 组标志
	string uid;		//< 单元标志
	string cid;		//< 相机标志
	double value;	//< 半高全宽, 量纲: 像素

public:
	annexproto_fwhm() {
		type = MPTYPE_FWHM;
		value = 0.0;
	}
};
typedef boost::shared_ptr<annexproto_fwhm> annpfwhm;

class AnnexProtocol {
public:
	AnnexProtocol();
	AnnexProtocol(const string &gid, const string &uid);
	virtual ~AnnexProtocol();

protected:
	/* 数据类型 */
	typedef boost::unique_lock<boost::mutex> mutex_lock;	//< 互斥锁
	typedef std::list<string> listring;			//< string列表

protected:
	/* 成员变量 */
	boost::shared_array<char> buff_;		//< 存储区
	boost::mutex mtx_;	//< 互斥锁
	int ibuf_;			//< 存储区索引
	string gid_;		//< 组标志
	string uid_;		//< 单元标志

public:
	/*------------------ 接口: 封装通信协议, 用于套接字输出 ------------------*/
	/*!
	 * @brief 封装协议slit
	 * @param command 天窗开关指令. 0: 关闭; 1: 打开; 2: 停止
	 * @param n       封装后数据长度, 量纲: 字节
	 * @return
	 * 封装数据地址
	 */
	const char* CompactSlit(const int command, int& n);
	const char* CompactSlit(const string& gid, const int command, int& n);
	const char* CompactSlit(const string& gid, const string& uid, const int command, int& n);
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

public:
	/*------------------ 接口: 解析通信协议 ------------------*/
	/*!
	 * @brief 解析通信协议
	 * @param rcvd 接收到的信息
	 * @param type 协议类型
	 * @return
	 * 转换为annpbase类型的通信协议
	 */
	annpbase Resolve(const char* rcvd);

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
typedef boost::shared_ptr<AnnexProtocol> AnnProtoPtr;	//< AnnexProtocol指针

#endif /* ANNEXPROTOCOL_H_ */
