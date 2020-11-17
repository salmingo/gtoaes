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
	bool		autoBias;	///< 标准流程: 本底
	bool		autoDark;	///< 标准流程: 暗场
	bool		autoFlat;	///< 标准流程：平场. 若系统有滤光片, 则遍历所有
	int			autoFrmCnt;	///< 标准流程: 帧数
	double		autoExpdur;	///< 标准流程: 曝光时间

	/*!
	 * 网络连接与设备对象的对应关系, 分为两类:
	 * - p2p, 索引0, 一条网络连接对应一个设备
	 * - p2h, 索引1, 一条网络连接对应多个设备
	 */
	bool		p2hMount;	///< 与转台是否Peer-Hub关系
	bool		p2hCamera;	///< 与相机是否Peer-Hub关系
	bool		p2hMountAnnex;	///< 与转台附属设备是否Point-Hub关系
	bool		p2hCameraAnnex;	///< 与相机附属设备是否Point-Hub关系

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
	/* 环境信息 */
	bool		useRainfall;	///< 使用: 雨量
	bool		useWindSpeed;	///< 使用: 风速
	bool		useCloudCamera;	///< 使用: 云量
	int			maxWindSpeed;	///< 最大风速: 可观测, 米/秒
	int			maxCloudPerent;	///< 最大云量: 可观测, 百分比
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
	int portEnv;		///< UDP服务端口: 气象环境

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
	ObssPrmVec prmOBSS_;	///< 观测系统参数集合

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
