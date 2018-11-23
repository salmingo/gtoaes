/*!
 * @file parameter.h 使用XML文件格式管理配置参数
 */

#ifndef PARAMETER_H_
#define PARAMETER_H_

#include <string>
#include <vector>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>

using std::string;

enum ObservationSystemType {// 观测系统类型
	OST_GWAC = 1,	//< GWAC
	OST_NORMAL		//< 常规系统
};

struct ObservationSystemTrait {// 观测系统关键特征
	string gid;		//< 在网络系统中的组标志
	int obsmode;	//< 计划调度模式
					//< 1: 替换模式. 高优先级替换在执行计划, 取消在执行计划
					//< 2: 插入模式. 高优先级替换在执行计划, 在执行计划进入缓存队列
	int ostype;		//< 观测系统类型, 用于通知相机的文件存储格式
					//< 1: GWAC
					//< 2: NORMAL
	string sitename;//< 测站名称
	double lgt;		//< 地理经度, 东经为正, 量纲: 角度
	double lat;		//< 地理纬度, 北纬为正, 量纲: 角度
	double alt;		//< 海拔高度, 量纲: 米
	int timezone;	//< 时区, 量纲: 小时

public:
	ObservationSystemTrait & operator=(const ObservationSystemTrait &other) {
		if (this != &other) {
			gid			= other.gid;
			obsmode		= other.obsmode;
			ostype		= other.ostype;
			sitename	= other.sitename;
			lgt			= other.lgt;
			lat			= other.lat;
			alt			= other.alt;
			timezone	= other.timezone;
		}
		return *this;
	}
};
typedef ObservationSystemTrait ObssTrait;
typedef boost::shared_ptr<ObssTrait> ObssTraitPtr;
typedef std::vector<ObssTraitPtr> ObssTraitVec;

struct MountLimit {// 转台限位保护参数
	string gid;		//< 组标志
	string uid;		//< 单元标志
	double value;	//< 水平限位, 最低仰角, 量纲: 角度

public:
	MountLimit() {
		value = 20.0;
	}
};
typedef boost::shared_ptr<MountLimit> MountLimitPtr;
typedef std::vector<MountLimitPtr> MountLimitVec;

struct param_config {// 软件配置参数
	int	portClient;		//< 面向客户端网络服务端口
	int portTele;		//< 通用望远镜网络服务端口
	int portMount;		//< GWAC转台网络服务端口
	int portCamera;		//< 面向相机网络服务端口
	int portCameraAnnex;//< 面向相机配套设备网络服务端口

	bool enableNTP;		//< NTP启用标志
	string hostNTP;		//< NTP服务器IP地址
	int  maxDiffNTP;	//< 采用自动校正时钟策略时, 本机时钟与NTP时钟所允许的最大偏差, 量纲: 毫秒

	string urlDB;		//< 数据库访问地址
	bool enableDB;		//< 数据库启用标志

	ObssTraitVec  obsst;	//< 测站系统参数
	MountLimitVec mntlimit;	//< 转台限位保护

private:
	string filepath_config;	//< 配置参数文件路径

public:
	/*!
	 * @brief 初始化文件filepath, 存储缺省配置参数
	 * @param filepath 文件路径
	 */
	void InitFile(const std::string& filepath) {
		using namespace boost::posix_time;
		using boost::property_tree::ptree;

		ptree pt;

		pt.add("version", "0.8");
		pt.add("date", to_iso_string(second_clock::universal_time()));

		ptree& node1 = pt.add("Server", "");
		node1.add("Client",     portClient     = 4010);
		node1.add("Telescope",  portTele       = 4011);
		node1.add("GWACMount",  portMount      = 4012);
		node1.add("Camera",     portCamera     = 4013);
		node1.add("CameraAnnex",portCameraAnnex= 4014);

		ptree& node2 = pt.add("NTP", "");
		node2.add("Enable", enableNTP = true);
		node2.add("IP",     hostNTP = "172.28.1.3");
		node2.add("MaximumDifference", maxDiffNTP = 100);

		ptree& node3 = pt.add("Database", "");
		node3.add("Enable", enableDB = true);
		node3.add("URL",    urlDB    = "http://172.28.8.8:8080/gwebend/");

		ptree& node4 = pt.add("ObservationSytemTrait", "");
		ObssTraitPtr trait = boost::make_shared<ObssTrait>();
		node4.add("group_id",  trait->gid       = "001");
		node4.add("sitename",  trait->sitename  = "Xinglong");
		node4.add("longitude", trait->lgt       = 117.57454166666667);
		node4.add("latitude",  trait->lat       = 40.395933333333333);
		node4.add("altitude",  trait->alt       = 900);
		node4.add("timezone",  trait->timezone  = 8);
		node4.add("obsmode",   trait->obsmode   = 1);
		node4.add("<xmlcomment>", "obsmode #1: Replace current plan");
		node4.add("<xmlcomment>", "obsmode #2: Append the new plan then do scheduling");
		node4.add("ostype",    trait->ostype    = 1);
		node4.add("<xmlcomment>", "ostype #1: GWAC");
		node4.add("<xmlcomment>", "ostype #2: Normal");
		obsst.push_back(trait);

		ptree& node5 = pt.add("MountLimit", "");
		MountLimitPtr limit = boost::make_shared<MountLimit>();
		node5.add("<xmlattr>.GroupID",  limit->gid   = "001");
		node5.add("<xmlattr>.UnitID",   limit->uid   = "");
		node5.add("<xmlattr>.Value",    limit->value = 10.0);
		mntlimit.push_back(limit);

		boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filepath, pt, std::locale(), settings);

		filepath_config = filepath;
	}

	/*!
	 * @brief 从文件filepath加载配置参数
	 * @param filepath 文件路径
	 */
	void LoadFile(const std::string& filepath) {
		try {
			using boost::property_tree::ptree;

			ptree pt;
			read_xml(filepath, pt, boost::property_tree::xml_parser::trim_whitespace);

			obsst.clear();
			BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
				if (boost::iequals(child.first, "Server")) {
					portClient     = child.second.get("Client",     4010);
					portTele       = child.second.get("Telescope",  4011);
					portMount      = child.second.get("GWACMount",  4012);
					portCamera     = child.second.get("Camera",     4013);
					portCameraAnnex= child.second.get("CameraAnnex",4014);
				}
				else if (boost::iequals(child.first, "NTP")) {
					enableNTP  = child.second.get("Enable", true);
					hostNTP    = child.second.get("IP",     "172.28.1.3");
					maxDiffNTP = child.second.get("MaximumDifference", 100);
				}
				else if (boost::iequals(child.first, "Database")) {
					enableDB   = pt.get("Enable",  false);
					urlDB      = pt.get("URL",     "http://172.28.8.8:8080/gwebend/");
				}
				else if (boost::iequals(child.first, "ObservationSytemTrait")) {
					ObssTraitPtr trait = boost::make_shared<ObssTrait>();
					trait->gid      = child.second.get("group_id",     "");
					trait->sitename = child.second.get("sitename",     "");
					trait->lgt      = child.second.get("longitude",   0.0);
					trait->lat      = child.second.get("latitude",    0.0);
					trait->alt      = child.second.get("altitude",  100.0);
					trait->timezone = child.second.get("timezone",      8);
					trait->obsmode  = child.second.get("obsmode",       1);
					trait->ostype   = child.second.get("ostype",        1);
					obsst.push_back(trait);
				}
				else if (boost::iequals(child.first, "MountLimit")) {
					MountLimitPtr limit = boost::make_shared<MountLimit>();
					limit->gid   = child.second.get("<xmlattr>.GroupID", "001");
					limit->gid   = child.second.get("<xmlattr>.UnitID",  "001");
					limit->value = child.second.get("<xmlattr>.Value",    10.0);
				}
			}
			filepath_config = filepath;
		}
		catch(boost::property_tree::xml_parser_error &ex) {
			InitFile(filepath);
		}
	}

	/*!
	 * @brief 从配置项中查找group_id对应的观测系统信息
	 * @param group_id  GWAC组标志
	 * @param lgt       地理经度, 东经为正, 量纲: 角度
	 * @param lat		地理纬度, 北纬为正, 量纲: 角度
	 * @param alt		海拔高度, 量纲: 米
	 */
	ObssTraitPtr GetObservationSystemTrait(const std::string& group_id) {
		ObssTraitVec::iterator it;
		ObssTraitPtr trait;
		for (it = obsst.begin(); it != obsst.end() && !boost::iequals(group_id, (*it)->gid); ++it);
		if (it != obsst.end()) trait = *it;
		return trait;
	}

	MountLimitPtr GetMountSafeLimit(const string& group_id, const string& unit_id) {
		MountLimitVec::iterator it;
		MountLimitPtr limit;
		string uid;

		for (it = mntlimit.begin(); it != mntlimit.end(); ++it) {
			limit = *it;
			uid   = limit->uid;
			if (limit->gid == group_id && (uid == unit_id || uid.empty())) break;
		}
		return limit;
	}
};

#endif // PARAMETER_H_
