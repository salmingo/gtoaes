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

struct ObsSite {// 测站信息
	std::string name;		//< 测站名称
	std::string group_id;	//< 在网络系统中的组标志
	double lgt;			//< 地理经度, 东经为正, 量纲: 角度
	double lat;			//< 地理纬度, 北纬为正, 量纲: 角度
	double alt;			//< 海拔高度, 量纲: 米
};
typedef std::vector<ObsSite> SiteVec;

struct param_config {// 软件配置参数
	int	portClient;		//< 面向客户端网络服务端口
	int portDB;			//< 面向数据库网络服务端口
	int portMount;		//< 面向转台网络服务端口
	int portCamera;		//< 面向相机网络服务端口
	int portMountAnnex;	//< 面向转台附加设备(调焦+镜盖)的网络服务端口
	
	bool enableNTP;			//< NTP启用标志
	std::string hostNTP;	//< NTP服务器IP地址
	int  maxDiffNTP;		//< 采用自动校正时钟策略时, 本机时钟与NTP时钟所允许的最大偏差, 量纲: 毫秒

	SiteVec sites;		//< 测站位置信息

public:
	/*!
	 * @brief 初始化文件filepath, 存储缺省配置参数
	 * @param filepath 文件路径
	 */
	void InitFile(const std::string& filepath) {
		using namespace boost::posix_time;
		using boost::property_tree::ptree;

		ptree pt;

		pt.add("version", "0.4");
		pt.add("date", to_iso_string(second_clock::universal_time()));

		ptree& node1 = pt.add("Server", "");
		node1.add("Client",     portClient     = 4010);
		node1.add("Database",   portDB         = 4011);
		node1.add("Mount",      portMount      = 4012);
		node1.add("Camera",     portCamera     = 4013);
		node1.add("MountAnnex", portMountAnnex = 4014);

		ptree& node2 = pt.add("NTP", "");
		node2.add("Enable", enableNTP = false);
		node2.add("IP",     hostNTP = "127.0.0.1");
		node2.add("MaximumDifference", maxDiffNTP = 100);

		ptree& node3 = pt.add("Observatory", "");
		ObsSite site;
		node3.add("name",      site.name     = "Xinglong");
		node3.add("group_id",  site.group_id = "001");
		node3.add("longitude", site.lgt      = 117.57454166666667);
		node3.add("latitude",  site.lat      = 40.395933333333333);
		node3.add("altitude",  site.alt      = 900);
		sites.push_back(site);

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

			portClient     = pt.get("Server.Client",     4010);
			portDB         = pt.get("Server.Database",   4011);
			portMount      = pt.get("Server.Mount",      4012);
			portCamera     = pt.get("Server.Camera",     4013);
			portMountAnnex = pt.get("Server.MountAnnex", 4014);

			enableNTP  = pt.get("NTP.Enable", false);
			hostNTP    = pt.get("NTP.IP",     "127.0.0.1");
			maxDiffNTP = pt.get("NTP.MaximumDifference", 5);

			BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
				if (boost::iequals(child.first, "Observatory")) {
					ObsSite site;
					site.name     = child.second.get("name",         "");
					site.group_id = child.second.get("group_id",     "");
					site.lgt      = child.second.get("longitude",   0.0);
					site.lat      = child.second.get("latitude",    0.0);
					site.alt      = child.second.get("altitude",  100.0);
					sites.push_back(site);
				}
			}
		}
		catch(boost::property_tree::xml_parser_error &ex) {
			InitFile(filepath);
		}
	}

	/*!
	 * @brief 从配置项中查找group_id对应的测站地理位置
	 * @param group_id  GWAC组标志
	 * @param lgt       地理经度, 东经为正, 量纲: 角度
	 * @param lat		地理纬度, 北纬为正, 量纲: 角度
	 * @param alt		海拔高度, 量纲: 米
	 */
	void GetGeosite(const std::string& group_id, std::string& name, double &lgt, double &lat, double &alt) {
		SiteVec::iterator it;
		name = "";
		lgt = lat = alt = 0.0;
		for (it = sites.begin(); it != sites.end() && !boost::iequals(group_id, (*it).group_id); ++it);
		if (it != sites.end()) {
			name = (*it).name;
			lgt  = (*it).lgt;
			lat  = (*it).lat;
			alt  = (*it).alt;
		}
	}
};

#endif // PARAMETER_H_
