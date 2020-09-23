/*
 * @file asciiproto.cpp 封装与控制台、数据库和相机相关的通信协议
 * @author         卢晓猛
 * @version        2.0
 * @date           2017年2月16日
 */

#include <stdio.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#include "asciiproto.h"
#include "ADefine.h"
#include "GLog.h"

using namespace std;
using namespace boost;

//////////////////////////////////////////////////////////////////////////////
bool valid_ra(double ra) {// 赤经有效性
	return (0.0 <= ra && ra < 360.0);
}

bool valid_dc(double dc) {// 赤纬有效性
	return (-90.0 <= dc && dc <= 90.0);
}
//////////////////////////////////////////////////////////////////////////////

ascii_proto::ascii_proto() {
	ibuff_ = 0;
	buff_.reset(new char[10240]); // 缓冲区: 1024*10 = 10240
	proto_camnf_ = boost::make_shared<ascproto_camera_info>();
}

ascii_proto::~ascii_proto() {

}

char *ascii_proto::get_buffptr() {
	mutex_lock lck(mtxbuff_);
	char *buff = buff_.get() + ibuff_ * 1024;
	if (++ibuff_ == 10) ibuff_ = 0;
	return buff;
}

// 封装abort_image协议
const char* ascii_proto::compact_abort_image(int& n) {
	char *buff = get_buffptr();
	n = sprintf(buff, "abort_image\n");
	return buff;
}

// 封装focus协议
const char* ascii_proto::compact_focus(const int value, int& n) {
	char *buff = get_buffptr();
	n = sprintf(buff, "focus value=%d", value);
	return buff;
}

const char* ascii_proto::compact_camera_info(const ascproto_camera_info* proto, int& n) {
	char *buff = get_buffptr();
	string output = "camera_info ";

	if (!proto->group_id.empty())	output += "group_id="   + proto->group_id                        + ", ";
	if (!proto->unit_id.empty())	output += "unit_id="    + proto->unit_id                         + ", ";
	if (!proto->camera_id.empty())	output += "cam_id="     + proto->camera_id                       + ", ";
	if (proto->bitdepth > 0)		output += "bitdepth="   + lexical_cast<string>(proto->bitdepth)  + ", ";
	if (proto->readport >= 0)		output += "readport="   + lexical_cast<string>(proto->readport)  + ", ";
	if (proto->readrate >= 0)		output += "readrate="   + lexical_cast<string>(proto->readrate)  + ", ";
	if (proto->vrate >= 0)			output += "vrate="      + lexical_cast<string>(proto->vrate)     + ", ";
	if (proto->gain >= 0)			output += "gain="       + lexical_cast<string>(proto->gain)      + ", ";
	if (proto->emsupport) {
									output += "emsupport="  + lexical_cast<string>(proto->emsupport) + ", ";
									output += "emon="       + lexical_cast<string>(proto->emon)      + ", ";
									output += "emgain="     + lexical_cast<string>(proto->emgain)    + ", ";
	}
	if (proto->coolset <= 0)		output += "coolset="    + lexical_cast<string>(proto->coolset)   + ", ";
	if (proto->coolget <= 0)		output += "coolget="    + lexical_cast<string>(proto->coolget)   + ", ";
	if (!proto->utc.empty())		output += "utc="        + proto->utc                             + ", ";
	if (proto->state >= 0)			output += "state="      + lexical_cast<string>(proto->state)     + ", ";
	if (proto->freedisk >= 0)		output += "freedisk="   + lexical_cast<string>(proto->freedisk)  + ", ";
	if (!proto->filepath.empty())	output += "filepath="   + proto->filepath                        + ", ";
	if (!proto->filename.empty())	output += "filename="   + proto->filename                        + ", ";
	if (!proto->objname.empty())	output += "objname="    + proto->objname                         + ", ";
	if (proto->expdur >= 0.0)		output += "expdur="     + lexical_cast<string>(proto->expdur)    + ", ";
	if (!proto->filter.empty())		output += "filter="     + proto->filter                          + ", ";
	if (proto->frmtot > 0)			output += "frmtot="     + lexical_cast<string>(proto->frmtot)    + ", ";
	if (proto->frmnum >= 0)			output += "frmnum="     + lexical_cast<string>(proto->frmnum)    + ", ";

	trim_right_if(output, is_punct() || is_space());
	n = sprintf(buff, "%s\n", output.c_str());
	return buff;
}

const char* ascii_proto::compact_object_info(const ascproto_object_info* proto, int& n) {
	char *buff = get_buffptr();
	string output = "object_info ";

	if (proto->op_sn >= 0)        output += "op_sn="    + lexical_cast<string>(proto->op_sn) + ", ";
	if (!proto->op_time.empty())  output += "op_time="  + proto->op_time                     + ", ";
	if (!proto->op_type.empty())  output += "op_type="  + proto->op_type                     + ", ";
	if (!proto->obstype.empty())  output += "obstype="  + proto->obstype                     + ", ";
	if (!proto->grid_id.empty())  output += "grid_id="  + proto->grid_id                     + ", ";
	if (!proto->field_id.empty()) output += "field_id=" + proto->field_id                    + ", ";
	if (valid_ra(proto->ra) && valid_dc(proto->dec)) {
		output += "ra="    + lexical_cast<string>(proto->ra)    + ", ";
		output += "dec="   + lexical_cast<string>(proto->dec)   + ", ";
		output += "epoch=" + lexical_cast<string>(proto->epoch) + ", ";
	}
	if (valid_ra(proto->objra) && valid_dc(proto->objdec)) {
		output += "objra="    + lexical_cast<string>(proto->objra)    + ", ";
		output += "objdec="   + lexical_cast<string>(proto->objdec)   + ", ";
		output += "objepoch=" + lexical_cast<string>(proto->objepoch) + ", ";
	}
	if (!proto->objerror.empty())   output += "objerror="   + proto->objerror                      + ", ";
	if (!proto->begin_time.empty()) output += "begin_time=" + proto->begin_time                    + ", ";
	if (!proto->end_time.empty())   output += "end_time="   + proto->end_time                      + ", ";
	if (proto->pair_id >= 0)        output += "pair_id="    + lexical_cast<string>(proto->pair_id) + ", ";
	if (!proto->filter.empty())     output += "filter="     + proto->filter                        + ", ";

	output += "obj_id="   + proto->obj_id                         + ", ";
	output += "imgtype="  + proto->imgtype                        + ", ";
	output += "iimgtype=" + lexical_cast<string>(proto->iimgtype) + ", ";
	output += "expdur="   + lexical_cast<string>(proto->expdur)   + ", ";
	output += "delay="    + lexical_cast<string>(proto->delay)    + ", ";
	output += "frmcnt="   + lexical_cast<string>(proto->frmcnt)   + ", ";
	output += "priority=" + lexical_cast<string>(proto->priority) + ", ";

	trim_right_if(output, is_punct() || is_space());
	n = sprintf(buff, "%s\n", output.c_str());
	return buff;
}

const char* ascii_proto::compact_expose(const EXPOSE_COMMAND cmd, int& n) {
	char *buff = get_buffptr();
	n = sprintf(buff, "expose command=%d\n", cmd);
	return buff;
}

apbase ascii_proto::resolve(const char* rcvd, string& type) {
	listring tokens;
	char seps[] = ","; // 协议主体键-值对之间的分隔符
	char *ptr = (char*) rcvd;
	apbase proto;

	// 提取协议类型
	// @li 协议类型与主体之间用空格分隔
	// @li 协议主体由键-值对组成, 每个键-值对之间使用逗号作为分隔符
	type = "";
	while (*ptr == ' ') ++ptr;
	for (; *ptr != '\0' && *ptr != ' '; ++ptr) type += *ptr;
	while (*ptr == ' ') ++ptr;

	if (*ptr != '\0') algorithm::split(tokens, ptr, is_any_of(seps), token_compress_on);
	/* 按重要性和发生概率排序 */
	if      (iequals(type, "camera_info"))   proto = resolve_camera_info(tokens);
	else if (iequals(type, "append_gwac"))   proto = resolve_append_gwac(tokens);
	else if (iequals(type, "take_image"))    proto = resolve_take_image(tokens);
	else if (iequals(type, "object_info"))   proto = resolve_object_info(tokens);
	else if (iequals(type, "expose"))        proto = resolve_expose(tokens);
	else if (iequals(type, "focus"))         proto = resolve_focus(tokens);
	else if (iequals(type, "fwhm"))          proto = resolve_fwhm(tokens);
	else if (iequals(type, "guide"))         proto = resolve_guide(tokens);
	else if (iequals(type, "slewto"))        proto = resolve_slewto(tokens);
	else if (iequals(type, "park"))          proto = resolve_park(tokens);
	else if (iequals(type, "abort_slew"))    proto = resolve_abort_slew(tokens);
	else if (iequals(type, "abort_image"))   proto = resolve_abort_image(tokens);
	else if (iequals(type, "home_sync"))     proto = resolve_home_sync(tokens);
	else if (iequals(type, "start_gwac"))    proto = resolve_start_gwac(tokens);
	else if (iequals(type, "stop_gwac"))     proto = resolve_stop_gwac(tokens);
	else if (iequals(type, "find_home"))     proto = resolve_find_home(tokens);
	else if (iequals(type, "mcover"))        proto = resolve_mcover(tokens);
	else if (iequals(type, "enable"))        proto = resolve_enable(tokens);
	else if (iequals(type, "disable"))       proto = resolve_disable(tokens);
	else gLog.Write("ascii_proto::resolve", LOG_WARN, "undefined or wrong protocol <%s>", rcvd);

	return proto;
}

// 分解键-值对
void ascii_proto::resolve_kv(string* kv, string &keyword, string &value) {
	char seps[] = "=";	// 分隔符: 等号
	listring tokens;

	keyword = "";
	value   = "";
	algorithm::split(tokens, *kv, is_any_of(seps), token_compress_on);
	if (!tokens.empty()) { keyword = tokens.front(); trim(keyword); tokens.pop_front(); }
	if (!tokens.empty()) { value   = tokens.front(); trim(value); }
}

// 解析协议: abort_slew
apbase ascii_proto::resolve_abort_slew(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_abort_slew> proto = boost::make_shared<ascproto_abort_slew>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: append_gwac
apbase ascii_proto::resolve_append_gwac(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_append_gwac> proto = boost::make_shared<ascproto_append_gwac>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "op_sn"))      proto->op_sn      = atoi(value.c_str());
		else if (iequals(keyword, "op_time"))    proto->op_time    = value;
		else if (iequals(keyword, "op_type"))    proto->op_type    = value;
		else if (iequals(keyword, "group_id"))   proto->group_id   = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id    = value;
		else if (iequals(keyword, "obstype"))    proto->obstype    = value;
		else if (iequals(keyword, "grid_id"))    proto->grid_id    = value;
		else if (iequals(keyword, "field_id"))   proto->field_id   = value;
		else if (iequals(keyword, "obj_id"))     proto->obj_id     = value;
		else if (iequals(keyword, "ra"))         proto->ra         = atof(value.c_str());
		else if (iequals(keyword, "dec"))        proto->dec        = atof(value.c_str());
		else if (iequals(keyword, "epoch"))      proto->epoch      = atof(value.c_str());
		else if (iequals(keyword, "objra"))      proto->objra      = atof(value.c_str());
		else if (iequals(keyword, "objdec"))     proto->objdec     = atof(value.c_str());
		else if (iequals(keyword, "objepoch"))   proto->objepoch   = atof(value.c_str());
		else if (iequals(keyword, "objerror"))   proto->objerror   = value;
		else if (iequals(keyword, "imgtype"))    proto->imgtype    = value;
		else if (iequals(keyword, "expdur"))     proto->expdur     = atof(value.c_str());
		else if (iequals(keyword, "delay"))      proto->delay      = atof(value.c_str());
		else if (iequals(keyword, "frmcnt"))     proto->frmcnt     = atoi(value.c_str());
		else if (iequals(keyword, "priority"))   proto->priority   = atoi(value.c_str());
		else if (iequals(keyword, "begin_time")) proto->begin_time = value;
		else if (iequals(keyword, "end_time"))   proto->end_time   = value;
		else if (iequals(keyword, "pair_id"))    proto->pair_id    = atoi(value.c_str());
	}
	// 检查数据有效性
	if (proto->op_sn < 0) {
		errtxt += "op_sn is less than 0; ";
		retv = false;
	}
	if (proto->priority < 0) proto->priority = 0;
	// 组标志
	if (proto->group_id.empty()) {
		errtxt += "group_id is empty; ";
		retv = false;
	}
	// 单元标志
	if (proto->unit_id.empty()) {
		errtxt += "unit_id is empty; ";
		retv = false;
	}
	// op_type == OBS
	if (!iequals(proto->op_type, "obs") && !iequals(proto->op_type, "1")) {
		errtxt += "op_type is not OBS; ";
		retv = false;
	}
	// 图像类型
	if      (iequals(proto->imgtype, "object") || iequals(proto->imgtype, "objt") || iequals(proto->imgtype, "4")) {
		proto->imgtype = "OBJECT";
		proto->iimgtype= IMGTYPE_OBJECT;
		if (proto->obj_id.empty()) proto->obj_id = "object";
		if (!valid_ra(proto->ra)) {
			errtxt += "RA is out of range; ";
			retv = false;
		}
		if (!valid_dc(proto->dec)) {
			errtxt += "DEC is out of range; ";
			retv = false;
		}
	}
	else if (iequals(proto->imgtype, "bias") || iequals(proto->imgtype, "1")) {
		proto->imgtype = "BIAS";
		proto->iimgtype= IMGTYPE_BIAS;
		proto->obj_id  = "bias";
		proto->expdur = 0.0;
		proto->delay   = 0.0;
	}
	else if (iequals(proto->imgtype, "dark") || iequals(proto->imgtype, "2")) {
		proto->imgtype = "DARK";
		proto->iimgtype= IMGTYPE_DARK;
		proto->obj_id = "dark";
		proto->delay  = 0.0;
	}
	else if (iequals(proto->imgtype, "flat") || iequals(proto->imgtype, "3")) {
		proto->imgtype = "FLAT";
		proto->iimgtype= IMGTYPE_FLAT;
		proto->obj_id = "flat";
		proto->delay  = 0.0;
	}
	else if (iequals(proto->imgtype, "focus") || iequals(proto->imgtype, "focs") || iequals(proto->imgtype, "5")) {
		proto->imgtype = "FOCUS";
		proto->iimgtype= IMGTYPE_FOCUS;
		proto->obj_id = "focus";
		proto->delay  = 0.0;
	}
	else {
		errtxt += "unknown image type; ";
		retv = false;
	}
	// 曝光帧数
	if (proto->frmcnt == 0) {// frmcnt < 0表示无限制
		errtxt += "frame count should not be zero; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("append_gwac", LOG_WARN, "%s", errtxt.c_str());
	}
	if (!retv) proto.reset();

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: guide
apbase ascii_proto::resolve_guide(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_guide> proto = boost::make_shared<ascproto_guide>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
		else if (iequals(keyword, "ra"))       proto->ra       = atof(value.c_str());
		else if (iequals(keyword, "dec"))      proto->dec      = atof(value.c_str());
		else if (iequals(keyword, "objra"))    proto->objra    = atof(value.c_str());
		else if (iequals(keyword, "objdec"))   proto->objdec   = atof(value.c_str());
	}
	if (proto->group_id.empty()) {
		errtxt = "group_id is empty; ";
		retv = false;
	}
	// 单元标志
	if (proto->unit_id.empty()) {
		errtxt += "unit_id is empty; ";
		retv = false;
	}

	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("guide", LOG_WARN, "%s", errtxt.c_str());
	}
	if (!retv) proto.reset();
	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: fwhm
apbase ascii_proto::resolve_fwhm(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_fwhm> proto = boost::make_shared<ascproto_fwhm>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id"))   proto->group_id = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id  = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id= value;
		else if (iequals(keyword, "value"))      proto->value    = atof(value.c_str());
	}
	// 检查数据有效性
	// 组标志
	if (proto->group_id.empty()) {
		errtxt = "group_id is empty; ";
		retv = false;
	}
	// 单元标志
	if (proto->unit_id.empty()) {
		errtxt += "unit_id is empty; ";
		retv = false;
	}
	// 相机标志
	if (proto->camera_id.empty()) {
		errtxt += "cam_id is empty; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("fwhm", LOG_WARN, "%s", errtxt.c_str());
	}

	if (!retv) proto.reset();
	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: focus
apbase ascii_proto::resolve_focus(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_focus> proto = boost::make_shared<ascproto_focus>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id"))   proto->group_id = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id  = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id= value;
		else if (iequals(keyword, "value"))      proto->value    = atoi(value.c_str());
	}
	// 检查数据有效性
	// 组标志
	if (proto->group_id.empty()) {
		errtxt = "group_id is empty; ";
		retv = false;
	}
	// 单元标志
	if (proto->unit_id.empty()) {
		errtxt += "unit_id is empty; ";
		retv = false;
	}
	// 相机标志
	if (proto->camera_id.empty()) {
		errtxt += "cam_id is empty; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("focus", LOG_WARN, "%s", errtxt.c_str());
	}

	if (!retv) proto.reset();
	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: start_gwac
apbase ascii_proto::resolve_start_gwac(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_start_gwac> proto = boost::make_shared<ascproto_start_gwac>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: stop_gwac
apbase ascii_proto::resolve_stop_gwac(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_stop_gwac> proto = boost::make_shared<ascproto_stop_gwac>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: enable
apbase ascii_proto::resolve_enable(listring &tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_enable> proto = boost::make_shared<ascproto_enable>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: disable
apbase ascii_proto::resolve_disable(listring &tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_disable> proto = boost::make_shared<ascproto_disable>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: find_home
apbase ascii_proto::resolve_find_home(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_find_home> proto = boost::make_shared<ascproto_find_home>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: home_sync
apbase ascii_proto::resolve_home_sync(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_home_sync> proto = boost::make_shared<ascproto_home_sync>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
		else if (iequals(keyword, "ra"))       proto->ra       = atof(value.c_str());
		else if (iequals(keyword, "dec"))      proto->dec      = atof(value.c_str());
		else if (iequals(keyword, "epoch"))    proto->epoch    = atof(value.c_str());
	}
	// 检查数据有效性
	// 组标志
	if (proto->group_id.empty()) {
		errtxt = "group_id is empty; ";
		retv = false;
	}
	// 单元标志
	if (proto->unit_id.empty()) {
		errtxt += "unit_id is empty; ";
		retv = false;
	}
	if (!valid_ra(proto->ra)) {
		errtxt += "RA is out of range; ";
		retv = false;
	}
	if (!valid_dc(proto->dec)) {
		errtxt += "DEC is out of range; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("home_sync", LOG_WARN, "%s", errtxt.c_str());
		proto.reset();
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: slewto
apbase ascii_proto::resolve_slewto(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_slewto> proto = boost::make_shared<ascproto_slewto>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
		else if (iequals(keyword, "ra"))       proto->ra       = atof(value.c_str());
		else if (iequals(keyword, "dec"))      proto->dec      = atof(value.c_str());
		else if (iequals(keyword, "epoch"))    proto->epoch    = atof(value.c_str());
	}
	// 检查数据有效性
	if (!valid_ra(proto->ra)) {
		errtxt = "RA is out of range; ";
		retv = false;
	}
	if (!valid_dc(proto->dec)) {
		errtxt = "DEC is out of range; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("slewto", LOG_WARN, "%s", errtxt.c_str());
		proto.reset();
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: park
apbase ascii_proto::resolve_park(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_park> proto = boost::make_shared<ascproto_park>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id")) proto->group_id = value;
		else if (iequals(keyword, "unit_id"))  proto->unit_id  = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: mcover
apbase ascii_proto::resolve_mcover(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_mcover> proto = boost::make_shared<ascproto_mcover>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id"))   proto->group_id  = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id   = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id = value;
		else if (iequals(keyword, "command"))    proto->command   = atoi(value.c_str());
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: abort_image
apbase ascii_proto::resolve_abort_image(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_abort_image> proto = boost::make_shared<ascproto_abort_image>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id"))   proto->group_id  = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id   = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id = value;
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: take_image
apbase ascii_proto::resolve_take_image(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_take_image> proto = boost::make_shared<ascproto_take_image>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);
		if      (iequals(keyword, "group_id"))   proto->group_id   = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id    = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id  = value;
		else if (iequals(keyword, "obj_id"))     proto->obj_id     = value;
		else if (iequals(keyword, "imgtype"))	 proto->imgtype    = value;
		else if (iequals(keyword, "expdur"))     proto->expdur     = atof(value.c_str());
		else if (iequals(keyword, "delay"))      proto->delay      = atof(value.c_str());
		else if (iequals(keyword, "frmcnt"))     proto->frmcnt     = atoi(value.c_str());
		else if (iequals(keyword, "filter"))     proto->filter     = value;
	}
	// 检查数据有效性
	// 图像类型
	if      (iequals(proto->imgtype, "object") || iequals(proto->imgtype, "objt") || iequals(proto->imgtype, "4")) {
		proto->imgtype = "OBJECT";
		proto->iimgtype= IMGTYPE_OBJECT;
		if (proto->obj_id.empty()) proto->obj_id = "object";
	}
	else if (iequals(proto->imgtype, "bias") || iequals(proto->imgtype, "1")) {
		proto->imgtype = "BIAS";
		proto->iimgtype= IMGTYPE_BIAS;
		proto->obj_id  = "bias";
		proto->expdur = 0.0;
		proto->delay   = 0.0;
	}
	else if (iequals(proto->imgtype, "dark") || iequals(proto->imgtype, "2")) {
		proto->imgtype = "DARK";
		proto->iimgtype= IMGTYPE_DARK;
		proto->obj_id = "dark";
		proto->delay  = 0.0;
	}
	else if (iequals(proto->imgtype, "flat") || iequals(proto->imgtype, "3")) {
		proto->imgtype = "FLAT";
		proto->iimgtype= IMGTYPE_FLAT;
		proto->obj_id = "flat";
		proto->delay  = 0.0;
	}
	else if (iequals(proto->imgtype, "focus") || iequals(proto->imgtype, "focs") || iequals(proto->imgtype, "5")) {
		proto->imgtype = "FOCUS";
		proto->iimgtype= IMGTYPE_FOCUS;
		proto->obj_id = "focus";
		proto->delay  = 0.0;
	}
	else {
		errtxt = "unknown image type; ";
		retv = false;
	}
	// 曝光帧数
	if (proto->frmcnt == 0) {// frmcnt < 0表示无限制
		errtxt += "frame count should not be zero; ";
		retv = false;
	}
	if (!retv) {
		trim_right_if(errtxt, is_punct() || is_space());
		gLog.Write("take_image", LOG_WARN, "%s", errtxt.c_str());
		proto.reset();
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

// 解析协议: camera_info
apbase ascii_proto::resolve_camera_info(listring& tokens) {
	string keyword, value, kv;
	boost::shared_ptr<ascproto_camera_info> proto = proto_camnf_;

	proto->reset();
	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);

		if      (iequals(keyword, "group_id"))   proto->group_id   = value;
		else if (iequals(keyword, "unit_id"))    proto->unit_id    = value;
		else if (iequals(keyword, "cam_id") || iequals(keyword, "camera_id")) proto->camera_id  = value;
		else if (iequals(keyword, "bitdepth"))   proto->bitdepth   = atoi(value.c_str());
		else if (iequals(keyword, "readport"))   proto->readport   = atoi(value.c_str());
		else if (iequals(keyword, "readrate"))   proto->readrate   = atoi(value.c_str());
		else if (iequals(keyword, "vrate"))      proto->vrate      = atoi(value.c_str());
		else if (iequals(keyword, "gain"))       proto->gain       = atoi(value.c_str());
		else if (iequals(keyword, "emon"))       proto->emon       = atoi(value.c_str());
		else if (iequals(keyword, "emgain"))     proto->emgain     = atoi(value.c_str());
		else if (iequals(keyword, "coolset"))    proto->coolset    = atoi(value.c_str());
		else if (iequals(keyword, "coolget"))    proto->coolget    = atoi(value.c_str());
		else if (iequals(keyword, "utc"))        proto->utc        = value;
		else if (iequals(keyword, "state"))      proto->state      = atoi(value.c_str());
		else if (iequals(keyword, "freedisk"))   proto->freedisk   = atoi(value.c_str());
		else if (iequals(keyword, "filepath"))   proto->filepath   = value;
		else if (iequals(keyword, "filename"))   proto->filename   = value;
		else if (iequals(keyword, "objname"))    proto->objname    = value;
		else if (iequals(keyword, "expdur"))     proto->expdur     = atof(value.c_str());
		else if (iequals(keyword, "filter"))     proto->filter     = value;
		else if (iequals(keyword, "frmtot"))     proto->frmtot     = atoi(value.c_str());
		else if (iequals(keyword, "frmnum"))     proto->frmnum     = atoi(value.c_str());
	}

	return boost::static_pointer_cast<ascproto_base>(proto);
}

apbase ascii_proto::resolve_object_info(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_object_info> proto = boost::make_shared<ascproto_object_info>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);

		if      (iequals(keyword, "op_sn"))     proto->op_sn = atoi(value.c_str());
		else if (iequals(keyword, "op_time"))   proto->op_time = value;
		else if (iequals(keyword, "op_type"))   proto->op_type = value;
		else if (iequals(keyword, "obstype"))   proto->obstype = value;
		else if (iequals(keyword, "grid_id"))   proto->grid_id = value;
		else if (iequals(keyword, "field_id"))  proto->field_id = value;
		else if (iequals(keyword, "obj_id"))    proto->obj_id = value;
		else if (iequals(keyword, "ra"))        proto->ra       = atof(value.c_str());
		else if (iequals(keyword, "dec"))       proto->dec      = atof(value.c_str());
		else if (iequals(keyword, "epoch"))     proto->epoch    = atof(value.c_str());
		else if (iequals(keyword, "objra"))     proto->objra    = atof(value.c_str());
		else if (iequals(keyword, "objdec"))    proto->objdec   = atof(value.c_str());
		else if (iequals(keyword, "objepoch"))  proto->objepoch = atof(value.c_str());
		else if (iequals(keyword, "objerror"))  proto->objerror = value;
		else if (iequals(keyword, "imgtype"))   proto->imgtype  = value;
		else if (iequals(keyword, "iimgtype"))  proto->iimgtype = (IMAGE_TYPE) atoi(value.c_str());
		else if (iequals(keyword, "expdur"))    proto->expdur   = atof(value.c_str());
		else if (iequals(keyword, "delay"))     proto->delay    = atof(value.c_str());
		else if (iequals(keyword, "frmcnt"))    proto->frmcnt   = atoi(value.c_str());
		else if (iequals(keyword, "priority"))  proto->priority = atoi(value.c_str());
		else if (iequals(keyword, "begin_time"))  proto->begin_time = value;
		else if (iequals(keyword, "end_time"))    proto->end_time   = value;
		else if (iequals(keyword, "pair_id"))     proto->pair_id    = atoi(value.c_str());
		else if (iequals(keyword, "filter"))      proto->filter     = value;
	}

	return boost::static_pointer_cast<ascproto_object_info>(proto);
}

apbase ascii_proto::resolve_expose(listring& tokens) {
	bool retv(true);
	string keyword, value, kv, errtxt;
	boost::shared_ptr<ascproto_expose> proto = boost::make_shared<ascproto_expose>();

	while (!tokens.empty()) {// 遍历键-值对
		kv = tokens.front();
		tokens.pop_front();
		resolve_kv(&kv, keyword, value);

		if (iequals(keyword, "command")) proto->command = (EXPOSE_COMMAND) atoi(value.c_str());
	}

	return boost::static_pointer_cast<ascproto_expose>(proto);
}
