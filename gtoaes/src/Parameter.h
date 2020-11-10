/*!
 * @struct Parameter 使用XML文件格式管理配置参数
 */

#ifndef SRC_PARAMETER_H_
#define SRC_PARAMETER_H_

#include <string>
#include <vector>

using std::string;

/**
 * @struct OBSSParam 观测系统参数
 */
struct OBSSParam {
	string		gid;		///< 组标志
	string		siteName;	///< 测站名称
	double		siteLon;	///< 地理经度, 东经为正, 量纲: 角度
	double 		siteLat;	///< 地理纬度, 北纬为正, 量纲: 角度
	double		siteAlt;	///< 海拔高度, 量纲: 米
	int			timeZone;	///< 时区, 量纲: 小时
	double		altLimit;	///< 水平限位, 最低仰角, 量纲: 角度
	bool		doNormalObs;///< 执行一般观测流程: 本底、暗场、平场

	/* 天窗与随动圆顶 */
	bool		useDomeFollow;	///< 使用: 随动圆顶
	bool		useDomeSlit;	///< 使用: 天窗
	int			opDome;			///< 执行对象: 转台; 转台附属
	/* 镜盖 */
	bool		useMirrorCover;	///< 使用: 镜盖
	int			opMirrorCover;	///< 执行对象: 转台; 转台附属; 相机; 相机附属
	/* 转台 */
	bool		useHomeSync;	///< 使用: 同步零点
	bool		useGuide;		///< 使用: 导星
	/* 调焦 */
	bool		useAutoFocus;	///< 使用: 自动调焦
	int			opAutoFocus;	///< 执行对象: 相机; 相机附属
	/* 消旋 */
	bool		useTermDerot;	///< 使用: 终端消旋
	int			opTermDerot;	///< 执行对象: 转台; 转台附属; 相机; 相机附属
	/* 环境信息 */
	bool		useRainfall;	///< 使用: 雨量
	bool		useWindSpeed;	///< 使用: 风速
	bool		useCloudCamera;	///< 使用: 云量
	int			maxWindSpeed;	///< 最大风速: 可观测, 米/秒
	int			maxCloudPerent;	///< 最大云量: 可观测, 百分比

public:
	OBSSParam & operator=(const OBSSParam &other) {
		if (this != &other) {
			gid			= other.gid;
			siteName	= other.siteName;
			siteLon		= other.siteLon;
			siteLat		= other.siteLat;
			siteAlt		= other.siteAlt;
			timeZone	= other.timeZone;
			altLimit	= other.altLimit;
			doNormalObs = other.doNormalObs;
			useDomeFollow	= other.useDomeFollow;
			useDomeSlit		= other.useDomeSlit;
			opDome			= other.opDome;
			useMirrorCover	= other.useMirrorCover;
			opMirrorCover	= other.opMirrorCover;
			useHomeSync		= other.useHomeSync;
			useGuide		= other.useGuide;
			useAutoFocus	= other.useAutoFocus;
			opAutoFocus		= other.opAutoFocus;
			useTermDerot	= other.useTermDerot;
			opTermDerot		= other.opTermDerot;
			useRainfall		= other.useRainfall;
			useWindSpeed	= other.useWindSpeed;
			useCloudCamera	= other.useCloudCamera;
			maxWindSpeed	= other.maxWindSpeed;
			maxCloudPerent	= other.maxCloudPerent;
		}
		return *this;
	}
};
typedef std::vector<OBSSParam> ObssPrmVec;

/**
 * @struct Parameter 全局配置参数
 */
struct Parameter {
/* 成员变量 */
public:
	/* 网络服务端口 */
	int portClient;		///< TCP服务端口: 客户端. 兼容历史版本
	int portMount;		///< TCP服务端口: 转台
	int portCamera;		///< TCP服务端口: 相机
	int portMountAnnex;	///< TCP服务端口: 转台附属
	int portCameraAnnex;///< TCP服务端口: 相机附属
	int udpPortEnv;		///< UDP服务端口: 气象环境

	/* NTP时间服务器 */
	bool ntpEnable;		///< 启用NTP
	string ntpHost;		///< NTP主机地址
	int ntpDiffMax;		///< 时钟校正阈值, 毫秒
	/* 数据库服务器 */
	bool dbEnable;		///< 启用数据库接口
	string dbUrl;		///< 数据库接口地址

private:
	string errmsg_;		///< 错误提示
	/* 观测系统参数 */
	ObssPrmVec prmOBSS_;///< 观测系统参数集合

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
	const OBSSParam* GetParamOBSS(const string &gid);
};

#endif /* SRC_PARAMETER_H_ */
