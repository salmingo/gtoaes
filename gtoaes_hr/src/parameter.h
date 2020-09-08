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
#include "AstroDeviceDef.h"

using std::string;

struct ObservationSystemTrait {// 观测系统关键特征
	string gid;		//< 在网络系统中的组标志
	string sitename;//< 测站名称
	double lon;		//< 地理经度, 东经为正, 量纲: 角度
	double lat;		//< 地理纬度, 北纬为正, 量纲: 角度
	double alt;		//< 海拔高度, 量纲: 米
	int timezone;	//< 时区, 量纲: 小时

public:
	ObservationSystemTrait & operator=(const ObservationSystemTrait &other) {
		if (this != &other) {
			gid			= other.gid;
			sitename	= other.sitename;
			lon			= other.lon;
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
	uint16_t portClient;	//< 客户端网络服务端口
	uint16_t portMount;		//< 通用望远镜网络服务端口
	uint16_t portCamera;	//< 相机网络服务端口
//	uint16_t portFocus;		//< 调焦网络服务端口
	uint16_t portAnnex;		//< 配套设备网络服务端口: 雨量+天窗

	bool ntpEnable;		//< NTP启用标志
	string ntpHost;		//< NTP服务器IP地址
	int ntpMaxDiff;		//< 采用自动校正时钟策略时, 本机时钟与NTP时钟所允许的最大偏差, 量纲: 毫秒

	string planPath;	//< 观测计划根目录
	string planCheck;	//< 本地时, 格式: hh:mm

	bool dbEnable;		//< 数据库启用标志
	string dbUrl;		//< 数据库访问地址

	ObssTraitVec  obsst;	//< 测站系统参数
	MountLimitVec mntlimit;	//< 转台限位保护

	double odtDay;		//< 观测时段: 太阳高度角大于该值时进入白天时段
	double odtNight;	//< 观测时段: 太阳高度角小于该值时进入夜间时段

public:
	/*!
	 * @brief 初始化文件filepath, 存储缺省配置参数
	 * @param filepath 文件路径
	 */
	void InitFile(const std::string& filepath) {
		using namespace boost::posix_time;
		using boost::property_tree::ptree;

		ptree pt;

		pt.add("version", "0.1");
		pt.add("date", to_iso_string(second_clock::universal_time()));

		ptree& node1 = pt.add("NetworkServer", "");
		node1.add("Client.<xmlattr>.Port", 4010);
		node1.add("Mount.<xmlattr>.Port",  4011);
		node1.add("Camera.<xmlattr>.Port", 4012);
//		node1.add("Focus.<xmlattr>.Port",  4013);
		node1.add("Annex.<xmlattr>.Port",  4013);

		ptree& node2 = pt.add("NTP", "");
		node2.add("<xmlattr>.Enable",        false);
		node2.add("<xmlattr>.IPv4",          "172.28.1.3");
		node2.add("MaxDiff.<xmlattr>.Value", 100);
		node2.add("<xmlcomment>", "Difference is in millisec");

		ptree& node3 = pt.add("Database", "");
		node3.add("<xmlattr>.Enable",    false);
		node3.add("URL.<xmlattr>.Addr",  "http://172.28.8.8:8080/gwebend/");

		ptree& node4 = pt.add("ObsPlan", "");
		node4.add("Location.<xmlattr>.Path",   "/home/wata/plan");
		node4.add("Check.<xmlattr>.LocalTime", "17:00");

		ptree& node5 = pt.add("ObservationSytemTrait", "");
		node5.add("ID.<xmlattr>.Group",          "001");
		node5.add("Geosite.<xmlattr>.Name",      "Xinglong");
		node5.add("Geosite.<xmlattr>.Lon",       117.57454166666667);
		node5.add("Geosite.<xmlattr>.Lat",       40.395933333333333);
		node5.add("Geosite.<xmlattr>.Alt",       900);
		node5.add("Geosite.<xmlattr>.Timezone",  8);

		ptree& node6 = pt.add("MountLimit", "");
		node6.add("ID.<xmlattr>.Group",   "001");
		node6.add("ID.<xmlattr>.Unit",    "");
		node6.add("Ele.<xmlattr>.Min",    10.0);

		ptree& node7 = pt.add("SunCenterAlt", "");
		node7.add("Daylight.<xmlattr>.Value", -6);
		node7.add("Night.<xmlattr>.Value",   -11);

		boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filepath, pt, std::locale(), settings);
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
				if (boost::iequals(child.first, "NetworkServer")) {
					portClient     = child.second.get("Client.<xmlattr>.Port",     4010);
					portMount      = child.second.get("Mount.<xmlattr>.Port",      4011);
					portCamera     = child.second.get("Camera.<xmlattr>.Port",     4012);
//					portFocus      = child.second.get("Focus.<xmlattr>.Port",      4013);
					portAnnex      = child.second.get("Annex.<xmlattr>.Port",      4013);
				}
				else if (boost::iequals(child.first, "NTP")) {
					ntpEnable  = child.second.get("<xmlattr>.Enable",        true);
					ntpHost    = child.second.get("<xmlattr>.IPv4",          "172.28.1.3");
					ntpMaxDiff = child.second.get("MaxDiff.<xmlattr>.Value", 100);
				}
				else if (boost::iequals(child.first, "Database")) {
					dbEnable   = child.second.get("<xmlattr>.Enable",    false);
					dbUrl      = child.second.get("URL.<xmlattr>.Addr",  "http://172.28.8.8:8080/gwebend/");
				}
				else if (boost::iequals(child.first, "ObsPlan")) {
					planPath = child.second.get("Location.<xmlattr>.Path",    "");
					planCheck = child.second.get("Check.<xmlattr>.LocalTime", "17:00");
				}
				else if (boost::iequals(child.first, "ObservationSytemTrait")) {
					ObssTraitPtr trait = boost::make_shared<ObssTrait>();
					trait->gid      = child.second.get("ID.<xmlattr>.Group",         "");
					trait->sitename = child.second.get("Geosite.<xmlattr>.Name",     "");
					trait->lon      = child.second.get("Geosite.<xmlattr>.Lon",      0.0);
					trait->lat      = child.second.get("Geosite.<xmlattr>.Lat",      0.0);
					trait->alt      = child.second.get("Geosite.<xmlattr>.Alt",      10.0);
					trait->timezone = child.second.get("Geosite.<xmlattr>.Timezone", 8);
					obsst.push_back(trait);
				}
				else if (boost::iequals(child.first, "MountLimit")) {
					MountLimitPtr limit = boost::make_shared<MountLimit>();
					limit->gid   = child.second.get("ID.<xmlattr>.Group",   "001");
					limit->uid   = child.second.get("ID.<xmlattr>.Unit",    "001");
					limit->value = child.second.get("Ele.<xmlattr>.Min",    20.0);
					mntlimit.push_back(limit);
				}
				else if (boost::iequals(child.first, "SunCenterAlt")) {
					odtDay   = child.second.get("Daylight.<xmlattr>.Value",    -6);
					odtNight = child.second.get("Night.<xmlattr>.Value", -11);
				}
			}
			if ((odtDay - odtNight) < 3.0) {
				odtDay   = -6.0;
				odtNight = -12.0;
			}
			else {
				if (odtDay > -3.0)    odtDay = -3.0;
				if (odtDay < -18.0)   odtDay = -6.0;
				if (odtNight > -6.0)  odtNight = -6.0;
				if (odtNight < -18.0) odtNight = -18.0;
			}
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

	double GetMountSafeLimit(const string& group_id, const string& unit_id) {
		MountLimitVec::iterator it;
		MountLimitPtr limit;

		for (it = mntlimit.begin(); it != mntlimit.end(); ++it) {
			if ((*it)->gid == group_id) {
				limit = *it;
				if (limit->uid == unit_id) break;
			}
		}
		return limit.use_count() ? limit->value : 20.0;
	}
};

#endif // PARAMETER_H_
