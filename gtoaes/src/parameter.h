/*!
 * @struct Parameter 使用XML文件格式管理配置参数
 */

#ifndef SRC_PARAMETER_H_
#define SRC_PARAMETER_H_

#include <string>
#include <vector>
#include "AstroDeviceDef.h"

using std::string;

/**
 * @struct OBSSParam 观测系统参数
 */
struct OBSSParam {
	string		gid;		/// 组标志
	OBSS_TYPE	type;		/// 观测系统类型
	string		sitename;	/// 测站名称
	double		sitelon;	/// 地理经度, 东经为正, 量纲: 角度
	double 		sitelat;	/// 地理纬度, 北纬为正, 量纲: 角度
	double		sitealt;	/// 海拔高度, 量纲: 米
	int			timezone;	/// 时区, 量纲: 小时
	double		elelimit;	/// 水平限位, 最低仰角, 量纲: 角度

public:
	OBSSParam & operator=(const OBSSParam &other) {
		if (this != &other) {
			gid			= other.gid;
			type		= other.type;
			sitename	= other.sitename;
			sitelon		= other.sitelon;
			sitelat		= other.sitelat;
			sitealt		= other.sitealt;
			timezone	= other.timezone;
			elelimit	= other.elelimit;
		}
		return *this;
	}
};
typedef std::vector<OBSSParam> ObssPrmVec;

/**
 * @struct Parameter 全局配置参数
 */
struct Parameter {
public:
	Parameter();
	virtual ~Parameter();

/* 成员变量 */
public:
	/* 网络服务端口 */
	int portClient;
	/**
	 * portTele:        通用望远镜
	 * portMount:       GWAC转台
	 * portTeleAnnex:   通用望远镜附属设备
	 * portMountAnnex:  GWAC转台附属设备端口, 镜盖+调焦+天窗
	 * portCameraAnnex: GWAC相机附属设备端口, 温控+真空
	 */
	int portTele;
	int portMount;
	int portCamera;
	int portTeleAnnex;
	int portMountAnnex;
	int portCameraAnnex;
	/* NTP时间服务器 */
	bool ntpEnable;
	string ntpHost;
	/* 数据库服务器 */
	bool dbEnable;
	string dbUrl;

private:
	string filepath_;
	string errmsg_;
	bool modified_;
	/* 观测系统参数 */
	ObssPrmVec prmOBSS_;

public:
	/*!
	 * @brief 使用缺省参数创建配置文件
	 * @param filepath 文件路径
	 */
	void Init(const string &filepath);
	/*!
	 * @brief 从配置文件中加载配置参数
	 * @param filepath 文件路径
	 * @return
	 * 成功读取配置文件时返回NULL, 否则返回错误提示
	 */
	const char* Load(const string &filepath);
	/*!
	 * @brief 查看观测系统的参数
	 * @param gid    组标志
	 * @param param  参数
	 * @return
	 * 若观测系统已配置则返回true, 否则返回false
	 */
	bool GetParamOBSS(const string &gid, OBSSParam &param);
	/*!
	 * @brief 更改已配置观测系统的参数, 或增加新的观测系统参数
	 * @param param  参数
	 * @return
	 * 操作结果.
	 * -1 : 参数错误
	 * -2 : 写入配置文件错误
	 *  1 : 更改配置参数
	 *  2 : 新增观测系统
	 */
	int SetParamOBSS(const OBSSParam &param);
};

#endif /* SRC_PARAMETER_H_ */
