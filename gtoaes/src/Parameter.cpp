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
#include "AstroDeviceDef.h"

using namespace boost::posix_time;
using namespace boost::property_tree;

void Parameter::Init(const string &filepath) {
	ptree pt;

	pt.add("version", DAEMON_VERSION);
	pt.add("date", to_iso_string(second_clock::universal_time()));

	ptree &node1 = pt.add("Server", "");
	node1.add("Client.<xmlattr>.Port",			4010);
	node1.add("Mount.<xmlattr>.Port",			4011);
	node1.add("Camera.<xmlattr>.Port",			4012);
	node1.add("MountAnnex.<xmlattr>.Port",		4013);
	node1.add("CameraAnnex.<xmlattr>.Port",		4014);
	node1.add("Environment.<xmlattr>.Port",		4015);

	ptree &node2 = pt.add("NTP", "");
	node2.add("<xmlattr>.Enable",			false);
	node2.add("<xmlattr>.Host",				"172.28.1.3");
	node2.add("<xmlattr>.SyncOnDiffMax",	1000);

	ptree &node3 = pt.add("Database", "");
	node3.add("<xmlattr>.Enable",	false);
	node3.add("<xmlattr>.URL",		"http://172.28.8.8:8080/gwebend/");

	ptree &node4 = pt.add("ObservationSystem", "");
	node4.add("GroupID", "001");
	node4.add("Site.<xmlattr>.Name", "Xinglong");
	node4.add("Site.<xmlattr>.Lon",	117.57454166666667);
	node4.add("Site.<xmlattr>.Lat",	40.395933333333333);
	node4.add("Site.<xmlattr>.Alt", 900);
	node4.add("Site.<xmlattr>.TZ",	8);
	node4.add("AltLimit", 20);
	node4.add("NormalFlow.<xmlattr>.Use",			false);
	node4.add("Dome.<xmlattr>.FollowMount",			false);
	node4.add("Dome.<xmlattr>.Slit",				false);
	node4.add("Dome.<xmlattr>.Operator",			"mount-annex");
	node4.add("MirrorCover.<xmlattr>.Use",			false);
	node4.add("MirrorCover.<xmlattr>.Operator",		"mount-annex");
	node4.add("Mount.<xmlattr>.HomeSync",			false);
	node4.add("Mount.<xmlattr>.Guide",				false);
	node4.add("AutoFocus.<xmlattr>.Use",			false);
	node4.add("AutoFocus.<xmlattr>.Operator",		"mount-annex");
	node4.add("TermDerotator.<xmlattr>.Use",		false);
	node4.add("TermDerotator.<xmlattr>.Operator",	"camera");

	ptree& node5 = node4.add("Environment", "");
	node5.add("Rainfall.<xmlattr>.Use", false);
	node5.add("WindSpeed.<xmlattr>.Use", false);
	node5.add("WindSpeed.<xmlattr>.MaxPermitObserve", 15);
	node5.add("CloudCamera.<xmlattr>.Use", false);
	node5.add("CloudCamera.<xmlattr>.MaxPercentPermitObserve", 50);

	xml_writer_settings<string> settings(' ', 4);
	write_xml(filepath, pt, std::locale(), settings);
}

const char* Parameter::Load(const string &filepath) {

	try {
		ptree pt;
		read_xml(filepath, pt, xml_parser::trim_whitespace);

		BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
			if (boost::iequals(child.first, "Server")) {
				portClient		= child.second.get("Client.<xmlattr>.Port",			4010);
				portMount		= child.second.get("Mount.<xmlattr>.Port",		    4011);
				portCamera		= child.second.get("Camera.<xmlattr>.Port",			4012);
				portMountAnnex	= child.second.get("MountAnnex.<xmlattr>.Port",		4013);
				portCameraAnnex	= child.second.get("CameraAnnex.<xmlattr>.Port",	4014);
				portEnv			= child.second.get("Environment.<xmlattr>.Port",	4015);
			}
			else if (boost::iequals(child.first, "NTP")) {
				ntpEnable	= child.second.get("<xmlattr>.Enable",			false);
				ntpHost		= child.second.get("<xmlattr>.Host",			"127.0.0.1");
				ntpDiffMax	= child.second.get("<xmlattr>.SyncOnDiffMax",	1000);
			}
			else if (boost::iequals(child.first, "Database")) {
				dbEnable	= child.second.get("<xmlattr>.Enable",	false);
				dbUrl		= child.second.get("<xmlattr>.URL",		"http://172.28.8.8:8080/gwebend/");
			}
			else if (boost::iequals(child.first, "ObservationSystem")) {
				OBSSParam prm;
				prm.gid		= child.second.get("GroupID", "");

				prm.siteName	= child.second.get("Site.<xmlattr>.Name",		"");
				prm.siteLon		= child.second.get("Site.<xmlattr>.Lon",		0.0);
				prm.siteLat		= child.second.get("Site.<xmlattr>.Lat",		0.0);
				prm.siteAlt		= child.second.get("Site.<xmlattr>.Alt",		0.0);
				prm.timeZone	= child.second.get("Site.<xmlattr>.TZ",			8);
				prm.altLimit	= child.second.get("AltLimit",					20.0);
				prm.doNormalObs	= child.second.get("NormalFlow.<xmlattr>.Use",	false);

				prm.useDomeFollow	= child.second.get("Dome.<xmlattr>.FollowMount",	false);
				prm.useDomeSlit		= child.second.get("Dome.<xmlattr>.Slit",			false);
				prm.opDome			= ObservationOperator::FromString(child.second.get("Dome.<xmlattr>.Operator", "mount").c_str());

				prm.useMirrorCover	= child.second.get("MirrorCover.<xmlattr>.Use",		false);
				prm.opMirrorCover	= ObservationOperator::FromString(child.second.get("MirrorCover.<xmlattr>.Operator","mount-annex").c_str());

				prm.useHomeSync		= child.second.get("Mount.<xmlattr>.HomeSync",	false);
				prm.useGuide		= child.second.get("Mount.<xmlattr>.Guide",		false);

				prm.useAutoFocus	= child.second.get("AutoFocus.<xmlattr>.Use",	false);
				prm.opAutoFocus		= ObservationOperator::FromString(child.second.get("AutoFocus.<xmlattr>.Operator", "mount-annex").c_str());

				prm.useTermDerot	= child.second.get("TermDerotator.<xmlattr>.Use",	false);
				prm.opTermDerot		= ObservationOperator::FromString(child.second.get("TermDerotator.<xmlattr>.Operator", "camera").c_str());

				const ptree& node_env = child.second.get_child("Environment");
				prm.useRainfall		= node_env.get("Rainfall.<xmlattr>.Use",  false);
				prm.useWindSpeed	= node_env.get("WindSpeed.<xmlattr>.Use", false);
				prm.maxWindSpeed	= node_env.get("WindSpeed.<xmlattr>.MaxPermitObserve", 15);
				prm.useCloudCamera	= node_env.get("CloudCamera.<xmlattr>.Use", false);
				prm.maxCloudPerent	= node_env.get("CloudCamera.<xmlattr>.MaxPercentPermitObserve", 50);

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

const OBSSParam* Parameter::GetParamOBSS(const string &gid) {
	ObssPrmVec::const_iterator it;
	ObssPrmVec::const_iterator itend = prmOBSS_.end();

	for (it = prmOBSS_.begin(); it != itend && gid != it->gid; ++it);
	return it != itend ? &(*it) : NULL;
}
