/*!
 * class Parameter 使用XML文件格式管理配置参数
 */

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include "globaldef.h"
#include "Parameter.h"

using namespace boost::posix_time;
using namespace boost::property_tree;

Parameter::Parameter() {
	portClient		= 4010;
	portTele		= 4011;
	portMount		= 4012;
	portCamera		= 4013;
	portTeleAnnex	= 4014;
	portMountAnnex	= 4015;
	portCameraAnnex	= 4016;
	ntpEnable	= false;
	ntpHost		= "127.0.0.1";
	dbEnable	= false;
	dbUrl		= "http://172.28.8.8:8080/gwebend/";
	modified_   = false;
}

Parameter::~Parameter() {
	if (modified_ && filepath_.size()) {
		ptree pt;

		pt.add("version", DAEMON_VERSION);
		pt.add("date", to_iso_string(second_clock::universal_time()));

		ptree &node1 = pt.add("Server", "");
		node1.add("Client.<xmlattr>.Port",			portClient);
		node1.add("Telescope.<xmlattr>.Port",		portTele);
		node1.add("Mount.<xmlattr>.Port",		    portMount);
		node1.add("Camera.<xmlattr>.Port",			portCamera);
		node1.add("TelescopeAnnex.<xmlattr>.Port",	portTeleAnnex);
		node1.add("MountAnnex.<xmlattr>.Port",	    portMountAnnex);
		node1.add("CameraAnnex.<xmlattr>.Port",		portCameraAnnex);
		node1.add("<xmlcomment>", "Normal type use Telescope");
		node1.add("<xmlcomment>", "GWAC type use Mount");

		ptree &node2 = pt.add("NTP", "");
		node2.add("<xmlattr>.Enable",	ntpEnable);
		node2.add("<xmlattr>.Host",		ntpHost);

		ptree &node3 = pt.add("Database", "");
		node3.add("<xmlattr>.Enable",	dbEnable);
		node3.add("<xmlattr>.URL",		dbUrl);

		for (ObssPrmVec::iterator it = prmOBSS_.begin(); it != prmOBSS_.end(); ++it) {
			ptree &node4 = pt.add("ObservationSystem", "");
			node4.add("GroupID", it->gid);
			node4.add("Type", it->type);
			node4.add("<xmlcomment>", "Type #1: Normal");
			node4.add("<xmlcomment>", "Type #2: GWAC");
			node4.add("Site.<xmlattr>.Name", it->sitename);
			node4.add("Site.<xmlattr>.Lon",	 it->sitelon);
			node4.add("Site.<xmlattr>.Lat",	 it->sitelat);
			node4.add("Site.<xmlattr>.Alt",  it->sitealt);
			node4.add("Site.<xmlattr>.TZ",	 it->timezone);
			node4.add("EleLimit", it->elelimit);
		}

		xml_writer_settings<string> settings(' ', 4);
		write_xml(filepath_, pt, std::locale(), settings);
	}
}

void Parameter::Init(const string &filepath) {
	ptree pt;

	pt.add("version", DAEMON_VERSION);
	pt.add("date", to_iso_string(second_clock::universal_time()));

	ptree &node1 = pt.add("Server", "");
	node1.add("Client.<xmlattr>.Port",			portClient);
	node1.add("Telescope.<xmlattr>.Port",		portTele);
	node1.add("Mount.<xmlattr>.Port",			portMount);
	node1.add("Camera.<xmlattr>.Port",			portCamera);
	node1.add("TelescopeAnnex.<xmlattr>.Port",	portTeleAnnex);
	node1.add("MountAnnex.<xmlattr>.Port",		portMountAnnex);
	node1.add("CameraAnnex.<xmlattr>.Port",		portCameraAnnex);

	ptree &node2 = pt.add("NTP", "");
	node2.add("<xmlattr>.Enable",	ntpEnable);
	node2.add("<xmlattr>.Host",		ntpHost);

	ptree &node3 = pt.add("Database", "");
	node3.add("<xmlattr>.Enable",	dbEnable);
	node3.add("<xmlattr>.URL",		dbUrl);

	ptree &node4 = pt.add("ObservationSystem", "");
	node4.add("GroupID", "001");
	node4.add("Type", 1);
	node4.add("<xmlcomment>", "Type #1: Normal");
	node4.add("<xmlcomment>", "Type #2: GWAC");
	node4.add("Site.<xmlattr>.Name", "Xinglong");
	node4.add("Site.<xmlattr>.Lon",	117.57454166666667);
	node4.add("Site.<xmlattr>.Lat",	40.395933333333333);
	node4.add("Site.<xmlattr>.Alt", 900);
	node4.add("Site.<xmlattr>.TZ",	8);
	node4.add("EleLimit", 20);

	xml_writer_settings<string> settings(' ', 4);
	write_xml(filepath, pt, std::locale(), settings);
}

const char* Parameter::Load(const string &filepath) {
	filepath_ = filepath;

	try {
		ptree pt;
		read_xml(filepath, pt, xml_parser::trim_whitespace);

		BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
			if (boost::iequals(child.first, "Server")) {
				portClient		= child.second.get("Client.<xmlattr>.Port",			4010);
				portTele		= child.second.get("Telescope.<xmlattr>.Port",		0);
				portMount		= child.second.get("Mount.<xmlattr>.Port",		    0);
				portCamera		= child.second.get("Camera.<xmlattr>.Port",			0);
				portTeleAnnex	= child.second.get("TelescopeAnnex.<xmlattr>.Port",	0);
				portMountAnnex	= child.second.get("MountAnnex.<xmlattr>.Port",		0);
				portCameraAnnex	= child.second.get("CameraAnnex.<xmlattr>.Port",	0);
			}
			else if (boost::iequals(child.first, "NTP")) {
				ntpEnable	= child.second.get("<xmlattr>.Enable",	false);
				ntpHost		= child.second.get("<xmlattr>.Host",	"127.0.0.1");
			}
			else if (boost::iequals(child.first, "Database")) {
				dbEnable	= child.second.get("<xmlattr>.Enable",	false);
				dbUrl		= child.second.get("<xmlattr>.URL",		"http://172.28.8.8:8080/gwebend/");
			}
			else if (boost::iequals(child.first, "ObservationSystem")) {
				OBSSParam prm;
				prm.gid		= child.second.get("GroupID", "");
				prm.type	= OBSS_TYPE(child.second.get("Type", 1));
				prm.sitename	= child.second.get("Site.<xmlattr>.Name", "");
				prm.sitelon		= child.second.get("Site.<xmlattr>.Lon", 0.0);
				prm.sitelat		= child.second.get("Site.<xmlattr>.Lat", 0.0);
				prm.sitealt		= child.second.get("Site.<xmlattr>.Alt", 0.0);
				prm.timezone	= child.second.get("Site.<xmlattr>.TZ",  8);
				prm.elelimit	= child.second.get("EleLimit", 20.0);
				prmOBSS_.push_back(prm);
			}
		}

		return NULL;
	}
	catch(xml_parser_error &ex) {
		Init(filepath);
		errmsg_ = ex.what();
		return errmsg_.c_str();
	}
}

bool Parameter::GetParamOBSS(const string &gid, OBSSParam &param) {
	ObssPrmVec::iterator it;
	ObssPrmVec::iterator itend = prmOBSS_.end();

	for (it = prmOBSS_.begin(); it != itend && gid != it->gid; ++it);
	if (it != itend) param = *it;
	return it != itend;
}

/*!
 * 返回值:
 * -1 : 参数错误
 * -2 : 写入配置文件错误
 * 1 : 更改配置参数
 * 2 : 新增观测系统
 */
int Parameter::SetParamOBSS(const OBSSParam &param) {
	string gid = param.gid;
	if (gid.empty()) return -1;

	ObssPrmVec::iterator it;
	ObssPrmVec::iterator itend = prmOBSS_.end();

	for (it = prmOBSS_.begin(); it != itend && gid != it->gid; ++it);
	if (it != itend) *it = param;
	else prmOBSS_.push_back(param);
	modified_ = true;

	return 2;
}
