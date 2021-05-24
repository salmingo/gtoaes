/*
 * @file mountproto.h 封装与转台相关的通信协议
 * @author         卢晓猛
 * @version        2.0
 * @date           2017年2月20日
 * ============================================================================
 * @date 2017年6月6日
 * - 参照asciiproto.h, 为buff_添加互斥锁
 */

#ifndef MOUNTPROTO_H_
#define MOUNTPROTO_H_

#include <string>
#include <limits.h>
#include <string.h>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>

/*!
 * @brief 通信协议基类, 包含组标志
 */
struct mntproto_base {
	std::string group_id;	//< 组标志
	std::string unit_id;		//< 单元标志

public:
	virtual ~mntproto_base() {
	}

	virtual void reset() {// 虚函数, 重置成员变量
		group_id = "";
		unit_id  = "";
	}
};

/*!
 * @brief 转台完成准备标志
 */
struct mntproto_ready : public mntproto_base {
private:
	int nmax;		//< 支持转台数量

public:
	int n;			//< 转台实际数量
	char ready[40];	//< 完成准备. 0: 未完成; 1: 已完成; -1: 未知或转台不存在

public:
	mntproto_ready() {
		nmax = 40;
		n    = 0;
	}

	/*!
	 * @brief 查看可支持转台的最大数量
	 * @return
	 * 可支持转台的最大数量
	 */
	int count() {
		return nmax;
	}

	void reset() {
		n    = 0;
		memset(ready, -1, sizeof(ready));
		mntproto_base::reset();
	}
};

/*!
 * @brief 转台实时工作状态
 */
struct mntproto_status : public mntproto_base {
private:
	int nmax;		//< 支持转台数量

public:
	int n;			//< 转台实际数量
	char state[40];	//< 转台状态
					//< @li -1: 未定义
					//< @li  0: 错误
					//< @li  1: 静止
					//< @li  2: 正在找零点
					//< @li  3: 找到零点
					//< @li  4: 正在复位
					//< @li  5: 已复位
					//< @li  6: 指向目标
					//< @li  7: 跟踪中

public:
	mntproto_status() {
		nmax = 40;
		n = 0;
	}

	/*!
	 * @brief 查看可支持转台的最大数量
	 * @return
	 * 可支持转台的最大数量
	 */
	int count() {
		return n;
	}

	void reset() {
		n = 0;
		memset(state, -1, sizeof(state));
		mntproto_base::reset();
	}
};

/*!
 * @brief 实时UTC时间
 */
struct mntproto_utc : public mntproto_base {
	std::string utc;			//< UTC时间

public:
	void reset() {
		utc     = "";
		mntproto_base::reset();
	}
};

/*!
 * @brief 实时转台指向位置
 */
struct mntproto_position : public mntproto_base {
	double ra;			//< 转台当前指向位置的赤经坐标, 量纲: 角度
	double dec;			//< 转台当前指向位置的赤纬坐标, 量纲: 角度

public:
	void reset() {
		ra = dec = -1000.0;
		mntproto_base::reset();
	}
};

/*!
 * @brief 焦点位置
 */
struct mntproto_focus : public mntproto_base {
	std::string camera_id;
	int position;

public:
	mntproto_focus() {
		position = 0;
	}
};

struct mntproto_mc_state {// 镜盖开关状态
	std::string camera_id;	//< 相机标志
	int state;			//< 镜盖状态
						//	-2：正在关闭
						//	-1：已关闭
						//	1：已打开
						//	2：正在打开
						//	0：未知

public:
	void reset() {
		camera_id = "";
		state     = 0;
	}
};

/*!
 * @breif 镜盖状态
 */
struct mntproto_mcover : public mntproto_base {
	int n;				//< 相机数量
	mntproto_mc_state state[10];	//< 镜盖状态

public:
	mntproto_mcover() {
		n = 10;
	}

	void reset() {
		for (int i = 0; i < n; ++i) state[i].reset();
		n = 0;
		mntproto_base::reset();
	}
};

typedef boost::shared_ptr<mntproto_base> mpbase;

class mount_proto {
public:
	mount_proto();
	virtual ~mount_proto();

protected:
	/* 声明数据类型 */
	typedef std::list<std::string> listring;
	typedef boost::unique_lock<boost::mutex> mutex_lock;

	// 成员变量
	boost::mutex mtxbuff_;	//< 存储区互斥锁
	int ibuff_; // 存储区索引. 缓冲区采用一维长度为10*512=5120字节数组, 通过软件分为10份循环使用
	boost::shared_array<char> buff_;	//< 通信协议存储区, 用于格式化输出通信协议
	std::string proto_type_;		//< 通信协议类型
	/* 为避免高频率触发的网络通信带来的内存碎片, 在创建对象时为所有类型协议一次性分配存储空间 */
	boost::shared_ptr<mntproto_focus>     proto_focus_;			//< focus
	boost::shared_ptr<mntproto_mcover>    proto_mcover_;		//< mcover
	boost::shared_ptr<mntproto_position>  proto_position_;		//< position
	boost::shared_ptr<mntproto_ready>     proto_ready_;			//< ready
	boost::shared_ptr<mntproto_status>    proto_status_;		//< status
	boost::shared_ptr<mntproto_utc>       proto_utc_;			//< utc

protected:
	/*!
	 * @brief 获得可写入数据的缓冲区地址
	 * @return
	 * 缓冲区首地址
	 */
	char *get_buffptr();

public:
	/*!
	 * @brief 解析通信协议
	 * @param rcvd   从网络中收到的信息
	 * @param body   解析后的信息主体, 转换为mntproto_base类型. 主程度按照协议类型再转换为对应数据类型
	 * @return
	 * 协议类型. 若无法识别协议类型则返回NULL
	 */
	const char* resolve(const char* rcvd, mpbase& body);

public:
	/*!
	 * @brief 组装构建find_home指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @param ra       赤经轴指令
	 * @param dec      赤纬轴指令
	 * @return
	 * 组装后协议
	 */
	const char* compact_find_home(const std::string& group_id, const std::string& unit_id, const bool ra, const bool dec, int& n);
	/*!
	 * @brief 组装构建home_sync指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @param ra       当前转台指向位置对应的当前历元赤经位置, 量纲: 角度
	 * @param dec      当前转台指向位置对应的当前历元赤纬位置, 量纲: 角度
	 * @return
	 * 组装后协议
	 */
	const char* compact_home_sync(const std::string& group_id, const std::string& unit_id, const double ra, const double dec, int& n);
	/*!
	 * @brief 组装构建slew指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @param ra       当前历元赤经位置, 量纲: 角度
	 * @param dec      当前历元赤纬位置, 量纲: 角度
	 * @return
	 * 组装后协议
	 */
	const char* compact_slew(const std::string& group_id, const std::string& unit_id, const double ra, const double dec, int& n);
	/*!
	 * @brief 组装构建slewhd指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @param ha       当前历元时角位置, 量纲: 角度
	 * @param dec      当前历元赤纬位置, 量纲: 角度
	 * @return
	 * 组装后协议
	 */
	const char* compact_slewhd(const std::string& group_id, const std::string& unit_id, double ha, double dec, int& n);
	/*!
	 * @brief 组装构建guide指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @param ra       赤经偏差量, 量纲: 角秒
	 * @param dec      赤纬偏差量, 量纲: 角秒
	 * @return
	 * 组装后协议
	 */
	const char* compact_guide(const std::string& group_id, const std::string& unit_id, const int ra, const int dec, int& n);
	/*!
	 * @brief 组装构建park指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @return
	 * 组装后协议
	 */
	const char* compact_park(const std::string& group_id, const std::string& unit_id, int& n);
	/*!
	 * @brief 组装构建abort_slew指令
	 * @param group_id 组标志
	 * @param unit_id  单元标志
	 * @return
	 * 组装后协议
	 */
	const char* compact_abort_slew(const std::string& group_id, const std::string& unit_id, int& n);
	/*!
	 * @brief 组装构建focus指令
	 * @param group_id  组标志
	 * @param unit_id   单元标志
	 * @param camera_id 相机标志
	 * @param fwhm      星像FWHM, 量纲: 像素
	 * @return
	 * 组装后协议
	 */
	const char* compact_fwhm(const std::string& group_id, const std::string& unit_id, const std::string& camera_id, const double fwhm, int& n);
	/*!
	 * @brief 由focus组装构建focus指令
	 * @param group_id  组标志
	 * @param unit_id   单元标志
	 * @param camera_id 相机标志
	 * @param focus     焦点位置
	 * @return
	 * 组装后协议
	 */
	const char* compact_focus(const std::string& group_id, const std::string& unit_id, const std::string& camera_id, const int focus, int& n);
	/*!
	 * @brief 组装构建mirror_cover指令
	 * @param group_id  组标志
	 * @param unit_id   单元标志
	 * @param camera_id 相机标志
	 * @param command   镜盖开关指令. 0: 关闭; 1: 打开
	 * @return
	 * 组装后协议
	 */
	const char* compact_mirror_cover(const std::string& group_id, const std::string& unit_id, const std::string& camera_id, const int command, int& n);
};
typedef boost::shared_ptr<mount_proto> mntptr;

#endif /* MOUNTPROTO_H_ */
