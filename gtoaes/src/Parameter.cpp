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

using namespace boost;
using namespace posix_time;
using namespace property_tree;

void Parameter::Init(const string &filepath) {
	ptree pt;

	pt.add("version", DAEMON_VERSION);
	pt.add("date", to_iso_string(second_clock::universal_time()));

	ptree &node1 = pt.add("Server", "");
	node1.add("Client.<xmlattr>.Port",       4010);
	node1.add("Mount.<xmlattr>.Port",        4011);
	node1.add("Camera.<xmlattr>.Port",       4012);
	node1.add("MountAnnex.<xmlattr>.Port",   4013);
	node1.add("CameraAnnex.<xmlattr>.Port",  4014);
	node1.add("Environment.<xmlattr>.Port",  4015);

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
	node4.add("Site.<xmlattr>.Lon",	117.574542);
	node4.add("Site.<xmlattr>.Lat",	 40.395933);
	node4.add("Site.<xmlattr>.Alt", 900);
	node4.add("Site.<xmlattr>.TZ",	8);
	node4.add("AltLimit", 20);
	node4.add("Robotic.<xmlattr>.Enable", false);

	ptree& node5 = node4.add("SunCenterAlt", "");
	node5.add("Daylight.<xmlattr>.Min", -6);
	node5.add("Night.<xmlattr>.Max", -12);

	ptree &node6 = node4.add("NormalFlow", "");
	node6.add("Bias.<xmlattr>.Use", true);
	node6.add("Dark.<xmlattr>.Use", true);
	node6.add("Flat.<xmlattr>.Use", true);
	node6.add("Exposure.<xmlattr>.FrameCount", 10);
	node6.add("Exposure.<xmlattr>.Duration",   10.0);

	node4.add("P2H.<xmlattr>.Mount",       false);
	node4.add("P2H.<xmlattr>.Camera",      false);
	node4.add("P2H.<xmlattr>.MountAnnex",  false);
	node4.add("P2H.<xmlattr>.CameraAnnex", false);

	node4.add("Dome.<xmlattr>.FollowMount",     false);
	node4.add("Dome.<xmlattr>.Slit",            false);
	node4.add("Dome.<xmlattr>.Operator",        "mount-annex");
	node4.add("MirrorCover.<xmlattr>.Use",      false);
	node4.add("MirrorCover.<xmlattr>.Operator", "mount-annex");
	node4.add("Mount.<xmlattr>.HomeSync",       false);
	node4.add("Mount.<xmlattr>.Guide",          false);
	node4.add("Slewto.<xmlattr>.Tolerance",     0.5);
	node4.add("AutoFocus.<xmlattr>.Use",        false);
	node4.add("AutoFocus.<xmlattr>.Operator",   "mount-annex");

	ptree& node7 = node4.add("Environment", "");
	node7.add("Rainfall.<xmlattr>.Use",    false);
	node7.add("WindSpeed.<xmlattr>.Use",   false);
	node7.add("WindSpeed.<xmlattr>.MaxPermitObserve", 15);
	node7.add("CloudCamera.<xmlattr>.Use", false);
	node7.add("CloudCamera.<xmlattr>.MaxPercentPermitObserve", 50);

	xml_writer_settings<string> settings(' ', 4);
	write_xml(filepath, pt, std::locale(), settings);
}

const char* Parameter::Load(const string &filepath) {

	try {
		ptree pt;
		read_xml(filepath, pt, xml_parser::trim_whitespace);

		BOOST_FOREACH(ptree::value_type &x, pt.get_child("")) {
			if (iequals(x.first, "Server")) {
				portClient		= x.second.get("Client.<xmlattr>.Port",       4010);
				portMount		= x.second.get("Mount.<xmlattr>.Port",        4011);
				portCamera		= x.second.get("Camera.<xmlattr>.Port",       4012);
				portMountAnnex	= x.second.get("MountAnnex.<xmlattr>.Port",   4013);
				portCameraAnnex	= x.second.get("CameraAnnex.<xmlattr>.Port",  4014);
				portEnv			= x.second.get("Environment.<xmlattr>.Port",  4015);
			}
			else if (iequals(x.first, "NTP")) {
				ntpEnable	= x.second.get("<xmlattr>.Enable",        false);
				ntpHost		= x.second.get("<xmlattr>.Host",          "127.0.0.1");
				ntpDiffMax	= x.second.get("<xmlattr>.SyncOnDiffMax", 1000);
			}
			else if (iequals(x.first, "Database")) {
				dbEnable	= x.second.get("<xmlattr>.Enable",	false);
				dbUrl		= x.second.get("<xmlattr>.URL",		"http://172.28.8.8:8080/gwebend/");
			}
			else if (iequals(x.first, "ObservationSystem")) {
				OBSSParam prm;
				prm.gid		= x.second.get("GroupID", "");

				prm.siteName   = x.second.get("Site.<xmlattr>.Name",  "");
				prm.siteLon    = x.second.get("Site.<xmlattr>.Lon",   0.0);
				prm.siteLat    = x.second.get("Site.<xmlattr>.Lat",   0.0);
				prm.siteAlt    = x.second.get("Site.<xmlattr>.Alt",   0.0);
				prm.timeZone   = x.second.get("Site.<xmlattr>.TZ",    8);
				prm.altLimit   = x.second.get("AltLimit",             20.0);
				prm.robotic    = x.second.get("Robotic.<xmlattr>.Enable", false);

				prm.altDay     = x.second.get("SunCenterAlt.Daylight.<xmlattr>.Min",  -6);
				prm.altNight   = x.second.get("SunCenterAlt.Night.<xmlattr>.Max",    -12);
				if (prm.altDay > 0.0 || prm.altDay < -10.0)
					prm.altDay = -6.0;
				if (prm.altNight > -10.0 || prm.altNight < -18.0)
					prm.altNight = -12.0;

				prm.autoBias   = x.second.get("NormalFlow.Bias.<xmlattr>.Use", true);
				prm.autoDark   = x.second.get("NormalFlow.Dark.<xmlattr>.Use", true);
				prm.autoFlat   = x.second.get("NormalFlow.Flat.<xmlattr>.Use", true);
				prm.autoFrmCnt = x.second.get("NormalFlow.Exposure.<xmlattr>.FrameCount", 10);
				prm.autoExpdur = x.second.get("NormalFlow.Exposure.<xmlattr>.Duration",   10.0);

				prm.p2hMount       = x.second.get("P2H.<xmlattr>.Mount",       false);
				prm.p2hCamera      = x.second.get("P2H.<xmlattr>.Camera",      false);
				prm.p2hMountAnnex  = x.second.get("P2H.<xmlattr>.MountAnnex",  false);
				prm.p2hCameraAnnex = x.second.get("P2H.<xmlattr>.CameraAnnex", false);

				prm.useDomeFollow  = x.second.get("Dome.<xmlattr>.FollowMount", false);
				prm.useDomeSlit    = x.second.get("Dome.<xmlattr>.Slit",        false);
				prm.opDome         = ObservationOperator::FromString(x.second.get("Dome.<xmlattr>.Operator", "mount").c_str());

				prm.useMirrorCover = x.second.get("MirrorCover.<xmlattr>.Use", false);
				prm.opMirrorCover  = ObservationOperator::FromString(x.second.get("MirrorCover.<xmlattr>.Operator","mount-annex").c_str());

				prm.useHomeSync    = x.second.get("Mount.<xmlattr>.HomeSync",   false);
				prm.useGuide       = x.second.get("Mount.<xmlattr>.Guide",      false);
				prm.tArrive        = x.second.get("Slewto.<xmlattr>.Tolerance", 0.5);

				prm.useAutoFocus   = x.second.get("AutoFocus.<xmlattr>.Use",  false);
				prm.opAutoFocus	   = ObservationOperator::FromString(x.second.get("AutoFocus.<xmlattr>.Operator", "mount-annex").c_str());

				prm.useRainfall    = x.second.get("Environment.Rainfall.<xmlattr>.Use",    false);
				prm.useWindSpeed   = x.second.get("Environment.WindSpeed.<xmlattr>.Use",   false);
				prm.maxWindSpeed   = x.second.get("Environment.WindSpeed.<xmlattr>.MaxPermitObserve", 15);
				prm.useCloudCamera = x.second.get("Environment.CloudCamera.<xmlattr>.Use", false);
				prm.maxCloudPerent = x.second.get("Environment.CloudCamera.<xmlattr>.MaxPercentPermitObserve", 50);

				if (prm.gid.size()) prmOBSS_.push_back(prm);
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

	for (it = prmOBSS_.begin(); it != prmOBSS_.end() && gid != it->gid; ++it);
	return it != prmOBSS_.end() ? &(*it) : NULL;
}
