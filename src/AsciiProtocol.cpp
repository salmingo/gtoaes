/*!
 * @file AsciiProtocol.cpp 定义文件, 定义GWAC/GFT系统中字符串型通信协议相关操作
 * @version 0.1
 * @date 2017-11-17
 **/

#include <boost/make_shared.hpp>
#include <stdlib.h>
#include "AsciiProtocol.h"

using namespace std;
using namespace boost;

//////////////////////////////////////////////////////////////////////////////
// 检查赤经/赤纬有效性
bool valid_ra(double ra) {
	return 0.0 <= ra && ra < 360.0;
}

bool valid_dec(double dec) {
	return -90.0 <= dec && dec <= 90.0;
}

/*!
 * @brief 检查图像类型
 */
IMAGE_TYPE check_imgtype(string imgtype, string &sabbr) {
	IMAGE_TYPE itype(IMGTYPE_ERROR);

	if (boost::iequals(imgtype, "bias")) {
		itype = IMGTYPE_BIAS;
		sabbr = "bias";
	}
	else if (boost::iequals(imgtype, "dark")) {
		itype = IMGTYPE_DARK;
		sabbr = "dark";
	}
	else if (boost::iequals(imgtype, "flat")) {
		itype = IMGTYPE_FLAT;
		sabbr = "flat";
	}
	else if (boost::iequals(imgtype, "object")) {
		itype = IMGTYPE_OBJECT;
		sabbr = "objt";
	}
	else if (boost::iequals(imgtype, "focus")) {
		itype = IMGTYPE_FOCUS;
		sabbr = "focs";
	}

	return itype;
}

AscProtoPtr make_ascproto() {
	return boost::make_shared<AsciiProtocol>();
}

//////////////////////////////////////////////////////////////////////////////
AsciiProtocol::AsciiProtocol() {
	ibuf_ = 0;
	buff_.reset(new char[1024 * 10]); //< 存储区
}

AsciiProtocol::~AsciiProtocol() {
}

const char* AsciiProtocol::output_compacted(string& output, int& n) {
	trim_right_if(output, is_punct() || is_space());
	return output_compacted(output.c_str(), n);
}

const char* AsciiProtocol::output_compacted(const char* s, int& n) {
	mutex_lock lck(mtx_);
	char* buff = buff_.get() + ibuf_ * 1024;
	if (++ibuf_ == 10) ibuf_ = 0;
	n = sprintf(buff, "%s\n", s);
	return buff;
}

void AsciiProtocol::compact_base(apbase base, string &output) {
	base->set_timetag(); // 为输出信息加上时标

	output = base->type + " ";
	join_kv(output, "utc",      base->utc);
	if (!base->gid.empty()) join_kv(output, "group_id", base->gid);
	if (!base->uid.empty()) join_kv(output, "unit_id",  base->uid);
	if (!base->cid.empty()) join_kv(output, "cam_id",   base->cid);
}

bool AsciiProtocol::resolve_kv(string& kv, string& keyword, string& value) {
	char seps[] = "=";	// 分隔符: 等号
	listring tokens;

	keyword = "";
	value   = "";
	algorithm::split(tokens, kv, is_any_of(seps), token_compress_on);
	if (!tokens.empty()) { keyword = tokens.front(); trim(keyword); tokens.pop_front(); }
	if (!tokens.empty()) { value   = tokens.front(); trim(value); }
	return (!(keyword.empty() || value.empty()));
}

void AsciiProtocol::resolve_kv_array(listring &tokens, likv &kvs, ascii_proto_base &basis) {
	string keyword, value;

	for (listring::iterator it = tokens.begin(); it != tokens.end(); ++it) {// 遍历键值对
		if (!resolve_kv(*it, keyword, value)) continue;
		// 识别通用项
		if      (iequals(keyword, "utc"))      basis.utc = value;
		else if (iequals(keyword, "group_id")) basis.gid = value;
		else if (iequals(keyword, "unit_id"))  basis.uid = value;
		else if (iequals(keyword, "cam_id"))   basis.cid = value;
		else {// 存储非通用项
			pair_key_val kv;
			kv.keyword = keyword;
			kv.value   = value;
			kvs.push_back(kv);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
/*---------------- 封装通信协议 ----------------*/
/*
 * @note
 * register协议功能:
 * 1/ 设备在服务器上注册其ID编号
 * 2/ 服务器通知注册结果. ostype在相机设备上生效, 用于创建不同的目录结构和文件名及FITS头
 */
const char *AsciiProtocol::CompactRegister(apreg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	if (proto->ostype != INT_MIN) join_kv(output, "ostype",   proto->ostype);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactRegister(int ostype, int &n) {
	string output = APTYPE_REG;
	output += " ";
	join_kv(output, "ostype", ostype);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactUnregister(apunreg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactStart(apstart proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactStop(apstop proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactEnable(apenable proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactDisable(apdisable proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactReload(apreload proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactReboot(apreboot proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactObss(apobss proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output, camid;
	int m(proto->camera.size());

	compact_base(to_apbase(proto), output);
	join_kv(output, "state",    proto->state);
	if (proto->op_sn >= 0) {
		join_kv(output, "op_sn",    proto->op_sn);
		join_kv(output, "op_time",  proto->op_time);
	}
	join_kv(output, "mount",    proto->mount);
	for (int i = 0; i < m; ++i) {
		camid = "cam#" + proto->camera[i].cid;
		join_kv(output, camid, proto->camera[i].state);
	}

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFindHome(apfindhome proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFindHome(int &n) {
	return output_compacted(APTYPE_FINDHOME, n);
}

const char *AsciiProtocol::CompactHomeSync(aphomesync proto, int &n) {
	if (!proto.use_count()
			|| !(valid_ra(proto->ra) && valid_dec(proto->dec)))
		return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "ra",       proto->ra);
	join_kv(output, "dec",      proto->dec);
	join_kv(output, "epoch",    proto->epoch);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactHomeSync(double ra, double dec, int &n) {
	string output = APTYPE_HOMESYNC;
	output += " ";
	join_kv(output, "ra",    ra);
	join_kv(output, "dec",   dec);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactSlewto(apslewto proto, int &n) {
	if (!proto.use_count()
			|| !(valid_ra(proto->ra) && valid_dec(proto->dec)))
		return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "ra",       proto->ra);
	join_kv(output, "dec",      proto->dec);
	join_kv(output, "epoch",    proto->epoch);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactSlewto(double ra, double dec, double epoch, int &n) {
	string output = APTYPE_SLEWTO;
	output += " ";
	join_kv(output, "ra",    ra);
	join_kv(output, "dec",   dec);
	join_kv(output, "epoch", epoch);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactPark(appark proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactPark(int &n) {
	return output_compacted(APTYPE_PARK, n);
}

const char *AsciiProtocol::CompactGuide(apguide proto, int &n) {
	if (!proto.use_count()
			|| !(valid_ra(proto->ra) && valid_dec(proto->dec)))
		return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	// 真实指向位置或位置偏差
	join_kv(output, "ra",       proto->ra);
	join_kv(output, "dec",      proto->dec);
	// 当(ra,dec)和目标位置同时有效时, (ra,dec)指代真实位置而不是位置偏差
	if (valid_ra(proto->objra) && valid_dec(proto->objdec)) {
		join_kv(output, "objra",    proto->objra);
		join_kv(output, "objdec",   proto->objdec);
	}

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactGuide(double ra, double dec, int &n) {
	string output = APTYPE_GUIDE;
	output += " ";
	join_kv(output, "ra",    ra);
	join_kv(output, "dec",   dec);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactAbortSlew(apabortslew proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactAbortSlew(int &n) {
	return output_compacted(APTYPE_ABTSLEW, n);
}

const char *AsciiProtocol::CompactTelescope(aptele proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "state",    proto->state);
	join_kv(output, "errcode",  proto->errcode);
	join_kv(output, "ra",       proto->ra);
	join_kv(output, "dec",      proto->dec);
	join_kv(output, "azi",      proto->azi);
	join_kv(output, "ele",      proto->ele);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFWHM(apfwhm proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "value", proto->value);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFocus(apfocus proto, int &n) {
	if (!proto.use_count() || proto->value == INT_MIN) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "value", proto->value);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFocus(int position, int &n) {
	string output = APTYPE_FOCUS;
	output += " ";
	join_kv(output, "value", position);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactMirrorCover(apmcover proto, int &n) {
	if (!proto.use_count() || proto->value == INT_MIN) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "value", proto->value);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactMirrorCover(int state, int &n) {
	string output = APTYPE_MCOVER;
	output += " ";
	join_kv(output, "value", state);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactTakeImage(aptakeimg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "objname",  proto->objname);
	join_kv(output, "imgtype",  proto->imgtype);
	join_kv(output, "filter",   proto->filter);
	join_kv(output, "expdur",   proto->expdur);
	join_kv(output, "frmcnt",   proto->frmcnt);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactAbortImage(apabortimg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactAppendPlan(apappplan proto, int &n) {
	if (!proto.use_count()) return NULL;

	int size, i;
	string output, tmp;
	compact_base(to_apbase(proto), output);

	join_kv(output, "plan_sn",     proto->plan_sn);
	if (!proto->plan_time.empty()) join_kv(output, "plan_time",   proto->plan_time);
	if (!proto->plan_type.empty()) join_kv(output, "plan_type",   proto->plan_type);
	if (!proto->obstype.empty())   join_kv(output, "obstype",     proto->obstype);
	if (!proto->grid_id.empty())   join_kv(output, "grid_id",     proto->grid_id);
	if (!proto->field_id.empty())  join_kv(output, "field_id",    proto->field_id);
	if (!proto->observer.empty())  join_kv(output, "observer",    proto->observer);
	if (!proto->objname.empty())   join_kv(output, "objname",     proto->objname);
	if (!proto->runname.empty())   join_kv(output, "runname",     proto->runname);
	if (valid_ra(proto->ra) && valid_dec(proto->dec)) {
		join_kv(output, "ra",          proto->ra);
		join_kv(output, "dec",         proto->dec);
		join_kv(output, "epoch",       proto->epoch);
	}
	if (valid_ra(proto->objra) && valid_dec(proto->dec)) {
		join_kv(output, "objra",       proto->objra);
		join_kv(output, "objdec",      proto->objdec);
		join_kv(output, "objepoch",    proto->objepoch);
	}
	if (!proto->objerror.empty())   join_kv(output, "objerror",    proto->objerror);

	join_kv(output, "imgtype",     proto->imgtype);
	if ((size = proto->filter.size())) {
		tmp = proto->filter[0];
		for (i = 1; i < size; ++i) {
			tmp += "; " + proto->filter[i];
		}
		join_kv(output, "filter", tmp);
	}

	join_kv(output, "expdur",      proto->expdur);
	join_kv(output, "delay",       proto->delay);
	join_kv(output, "frmcnt",      proto->frmcnt);
	join_kv(output, "loopcnt",     proto->loopcnt);
	join_kv(output, "priority",    proto->priority);
	if (!proto->begin_time.empty()) join_kv(output, "begin_time",  proto->begin_time);
	if (!proto->end_time.empty())   join_kv(output, "end_time",    proto->end_time);
	if (proto->pair_id >= 0)        join_kv(output, "pair_id",     proto->pair_id);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactAbortPlan(apabtplan proto, int &n) {
	if (!proto.use_count() || proto->plan_sn < 0) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "plan_sn", proto->plan_sn);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactCheckPlan(apchkplan proto, int &n) {
	if (!proto.use_count() || proto->plan_sn < 0) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "plan_sn", proto->plan_sn);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactPlan(applan proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	join_kv(output, "plan_sn", proto->plan_sn);
	join_kv(output, "state",   proto->state);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactObject(apobject proto, int &n) {
	if (!proto.use_count() || proto->plan_sn < 0 ) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "plan_sn",    proto->plan_sn);
	if (!proto->plan_time.empty()) join_kv(output, "plan_time",  proto->plan_time);
	if (!proto->plan_type.empty()) join_kv(output, "plan_type",  proto->plan_type);
	if (!proto->observer.empty())  join_kv(output, "observer",   proto->observer);
	if (!proto->obstype.empty())   join_kv(output, "obstype",    proto->obstype);
	if (!proto->grid_id.empty())   join_kv(output, "grid_id",    proto->grid_id);
	if (!proto->field_id.empty())  join_kv(output, "field_id",   proto->field_id);
	if (!proto->objname.empty())   join_kv(output, "objname",    proto->objname);
	if (!proto->runname.empty())   join_kv(output, "runname",    proto->runname);
	if (valid_ra(proto->ra) && valid_dec(proto->dec)) {
		join_kv(output, "ra",       proto->ra);
		join_kv(output, "dec",      proto->dec);
		join_kv(output, "epoch",    proto->epoch);
	}
	if (valid_ra(proto->objra) && valid_dec(proto->objdec)) {
		join_kv(output, "objra",    proto->objra);
		join_kv(output, "objdec",   proto->objdec);
		join_kv(output, "objepoch", proto->objepoch);
	}
	if (!proto->objerror.empty())  join_kv(output, "objerror", proto->objerror);
	join_kv(output, "imgtype",     proto->imgtype);
	join_kv(output, "expdur",      proto->expdur);
	join_kv(output, "delay",       proto->delay);
	join_kv(output, "frmcnt",      proto->frmcnt);
	join_kv(output, "loopcnt",     proto->loopcnt);
	join_kv(output, "ifilter",     proto->ifilter);
	join_kv(output, "frmno",       proto->frmno);
	join_kv(output, "loopno",      proto->loopno);
	join_kv(output, "priority",    proto->priority);
	join_kv(output, "begin_time",  proto->begin_time);
	join_kv(output, "end_time",    proto->end_time);
	join_kv(output, "pair_id",     proto->pair_id);

	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactExpose(int cmd, int &n) {
	string output = APTYPE_EXPOSE;
	output += " ";
	join_kv(output, "command", cmd);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactCamera(apcam proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "state",     proto->state);
	join_kv(output, "errcode",   proto->errcode);
	join_kv(output, "mcstate",   proto->mcstate);
	join_kv(output, "focus",     proto->focus);
	join_kv(output, "coolget",   proto->coolget);
	join_kv(output, "objname",   proto->objname);
	join_kv(output, "filename",  proto->filename);
	join_kv(output, "imgtype",   proto->imgtype);
	join_kv(output, "filter",    proto->filter);
	join_kv(output, "expdur",    proto->expdur);
	join_kv(output, "delay",     proto->delay);
	join_kv(output, "frmcnt",    proto->frmcnt);
	join_kv(output, "loopcnt",   proto->loopcnt);
	join_kv(output, "ifilter",   proto->ifilter);
	join_kv(output, "frmno",     proto->frmno);
	join_kv(output, "loopno",    proto->loopno);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactCooler(apcooler proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "voltage",  proto->voltage);
	join_kv(output, "current",  proto->current);
	join_kv(output, "hotend",   proto->hotend);
	join_kv(output, "coolget",  proto->coolget);
	join_kv(output, "coolset",  proto->coolset);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactVacuum(apvacuum proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "voltage",  proto->voltage);
	join_kv(output, "current",  proto->current);
	join_kv(output, "pressure", proto->pressure);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFileInfo(apfileinfo proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "grid_id",  proto->grid);
	join_kv(output, "field_id", proto->field);
	join_kv(output, "tmobs",    proto->tmobs);
	join_kv(output, "subpath",  proto->subpath);
	join_kv(output, "filename", proto->filename);
	join_kv(output, "filesize", proto->filesize);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactFileStat(apfilestat proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);

	join_kv(output, "status", proto->status);
	return output_compacted(output, n);
}

/**
 * @brief 拷贝生成新的通用观测计划
 */
apappplan AsciiProtocol::CopyAppendPlan(apappplan proto) {
	apappplan plan = boost::make_shared<ascii_proto_append_plan>();
	*plan = *proto;

	return plan;
}

//////////////////////////////////////////////////////////////////////////////
apbase AsciiProtocol::Resolve(const char *rcvd) {
	const char seps[] = ",", *ptr;
	char ch;
	listring tokens;
	apbase proto;
	string type;
	likv kvs;
	ascii_proto_base basis;

	// 提取协议类型
	for (ptr = rcvd; *ptr && *ptr != ' '; ++ptr) type += *ptr;
	while (*ptr && *ptr == ' ') ++ptr;
	// 分解键值对
	if (*ptr) algorithm::split(tokens, ptr, is_any_of(seps), token_compress_on);
	resolve_kv_array(tokens, kvs, basis);
	// 按照协议类型解析键值对
	if ((ch = type[0]) == 'a') {
		if      (iequals(type, APTYPE_ABTSLEW))  proto = resolve_abortslew(kvs);
		else if (iequals(type, APTYPE_ABTIMG))   proto = resolve_abortimg(kvs);
		else if (iequals(type, APTYPE_APPGWAC) || iequals(type, APTYPE_APPPLAN))  proto = resolve_append_plan(kvs);
		else if (iequals(type, APTYPE_ABTPLAN))  proto = resolve_abort_plan(kvs);
	}
	else if (ch == 'c') {
		if      (iequals(type, APTYPE_COOLER))   proto = resolve_cooler(kvs);
		else if (iequals(type, APTYPE_CAMERA))   proto = resolve_camera(kvs);
		else if (iequals(type, APTYPE_CHKPLAN))  proto = resolve_check_plan(kvs);
	}
	else if (ch == 'e') {
		if      (iequals(type, APTYPE_EXPOSE))   proto = resolve_expose(kvs);
		else if (iequals(type, APTYPE_ENABLE))   proto = resolve_enable(kvs);
	}
	else if (ch == 'f') {
		if      (iequals(type, APTYPE_FILEINFO)) proto = resolve_fileinfo(kvs);
		else if (iequals(type, APTYPE_FILESTAT)) proto = resolve_filestat(kvs);
		else if (iequals(type, APTYPE_FINDHOME)) proto = resolve_findhome(kvs);
		else if (iequals(type, APTYPE_FWHM))     proto = resolve_fwhm(kvs);
		else if (iequals(type, APTYPE_FOCUS))    proto = resolve_focus(kvs);
	}
	else if (ch == 'o') {
		if      (iequals(type, APTYPE_OBJECT))   proto = resolve_object(kvs);
		else if (iequals(type, APTYPE_OBSS))     proto = resolve_obss(kvs);
	}
	else if (ch == 'p') {
		if      (iequals(type, APTYPE_PARK))     proto = resolve_park(kvs);
		else if (iequals(type, APTYPE_PLAN))     proto = resolve_plan(kvs);
	}
	else if (ch == 's') {
		if      (iequals(type, APTYPE_SLEWTO))   proto = resolve_slewto(kvs);
		else if (iequals(type, APTYPE_START))    proto = resolve_start(kvs);
		else if (iequals(type, APTYPE_STOP))     proto = resolve_stop(kvs);
	}
	else if (ch == 't'){
		if      (iequals(type, APTYPE_TELE))     proto = resolve_telescope(kvs);
		else if (iequals(type, APTYPE_TAKIMG))   proto = resolve_takeimg(kvs);
	}
	else if (iequals(type, APTYPE_GUIDE))    proto = resolve_guide(kvs);
	else if (iequals(type, APTYPE_REG))      proto = resolve_register(kvs);
	else if (iequals(type, APTYPE_UNREG))    proto = resolve_unregister(kvs);
	else if (iequals(type, APTYPE_HOMESYNC)) proto = resolve_homesync(kvs);
	else if (iequals(type, APTYPE_MCOVER))   proto = resolve_mcover(kvs);
	else if (iequals(type, APTYPE_DISABLE))  proto = resolve_disable(kvs);
	else if (iequals(type, APTYPE_VACUUM))   proto = resolve_vacuum(kvs);

	if (proto.use_count()) {
		proto->type = type;
		proto->utc  = basis.utc;
		proto->gid  = basis.gid;
		proto->uid  = basis.uid;
		proto->cid  = basis.cid;
	}

	return proto;
}

apbase AsciiProtocol::resolve_register(likv &kvs) {
	apreg proto = boost::make_shared<ascii_proto_reg>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "ostype"))  proto->ostype = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_unregister(likv &kvs) {
	apunreg proto = boost::make_shared<ascii_proto_unreg>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_start(likv &kvs) {
	apstart proto = boost::make_shared<ascii_proto_start>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_stop(likv &kvs) {
	apstop proto = boost::make_shared<ascii_proto_stop>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_enable(likv &kvs) {
	apenable proto = boost::make_shared<ascii_proto_enable>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_disable(likv &kvs) {
	apdisable proto = boost::make_shared<ascii_proto_disable>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_reload(likv &kvs) {
	apreload proto = boost::make_shared<ascii_proto_reload>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_reboot(likv &kvs) {
	apreboot proto = boost::make_shared<ascii_proto_reboot>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_obss(likv &kvs) {
	apobss proto = boost::make_shared<ascii_proto_obss>();
	string keyword, precid = "cam#";
	int nprecid = precid.size();

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "state"))   proto->state   = stoi((*it).value);
		else if (iequals(keyword, "op_sn"))   proto->op_sn   = stoi((*it).value);
		else if (iequals(keyword, "op_time")) proto->op_time = (*it).value;
		else if (iequals(keyword, "mount"))   proto->mount   = stoi((*it).value);
		else if (keyword.find(precid) == 0) {// 相机工作状态. 关键字 cam#xxx
			ascii_proto_obss::camera_state cs;
			cs.cid   = keyword.substr(nprecid);
			cs.state = stoi((*it).value);

			proto->camera.push_back(cs);
		}
	}
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_findhome(likv &kvs) {
	apfindhome proto = boost::make_shared<ascii_proto_find_home>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_homesync(likv &kvs) {
	aphomesync proto = boost::make_shared<ascii_proto_home_sync>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "ra"))     proto->ra    = stod((*it).value);
		else if (iequals(keyword, "dec"))    proto->dec    = stod((*it).value);
		else if (iequals(keyword, "epoch"))  proto->epoch = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_slewto(likv &kvs) {
	apslewto proto = boost::make_shared<ascii_proto_slewto>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "ra"))     proto->ra    = stod((*it).value);
		else if (iequals(keyword, "dec"))    proto->dec    = stod((*it).value);
		else if (iequals(keyword, "epoch"))  proto->epoch = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_park(likv &kvs) {
	appark proto = boost::make_shared<ascii_proto_park>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_guide(likv &kvs) {
	apguide proto = boost::make_shared<ascii_proto_guide>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "ra"))       proto->ra     = stod((*it).value);
		else if (iequals(keyword, "dec"))      proto->dec    = stod((*it).value);
		else if (iequals(keyword, "objra"))    proto->objra  = stod((*it).value);
		else if (iequals(keyword, "objdec"))   proto->objdec = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_abortslew(likv &kvs) {
	apabortslew proto = boost::make_shared<ascii_proto_abort_slew>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_telescope(likv &kvs) {
	aptele proto = boost::make_shared<ascii_proto_telescope>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "state"))    proto->state   = stoi((*it).value);
		else if (iequals(keyword, "errcode"))  proto->errcode = stoi((*it).value);
		else if (iequals(keyword, "ra"))       proto->ra      = stod((*it).value);
		else if (iequals(keyword, "dec"))      proto->dec      = stod((*it).value);
		else if (iequals(keyword, "azi"))      proto->azi     = stod((*it).value);
		else if (iequals(keyword, "ele"))      proto->ele     = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_fwhm(likv &kvs) {
	apfwhm proto = boost::make_shared<ascii_proto_fwhm>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "value"))  proto->value = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_focus(likv &kvs) {
	apfocus proto = boost::make_shared<ascii_proto_focus>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "state"))  proto->state = stoi((*it).value);
		else if (iequals(keyword, "value"))  proto->value = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_mcover(likv &kvs) {
	apmcover proto = boost::make_shared<ascii_proto_mcover>();
	string keyword, value;
	int val;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "value")) {
			value = (*it).value;
			if (isdigit(value[0])) val = stoi((*it).value);
			else if (iequals(value, "open"))  val = MCC_OPEN;
			else if (iequals(value, "close")) val = MCC_CLOSE;
		}
	}
	if (val < MC_ERROR || val > MC_CLOSED) proto.reset();
	else proto->value = val;

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_takeimg(likv &kvs) {
	aptakeimg proto = boost::make_shared<ascii_proto_take_image>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "obj_id") || iequals(keyword, "objname"))
			proto->objname = (*it).value;
		else if (iequals(keyword, "imgtype"))  proto->imgtype = (*it).value;
		else if (iequals(keyword, "filter"))   proto->filter  = (*it).value;
		else if (iequals(keyword, "expdur"))   proto->expdur  = stod((*it).value);
		else if (iequals(keyword, "frmcnt"))   proto->frmcnt  = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_abortimg(likv &kvs) {
	apabortimg proto = boost::make_shared<ascii_proto_abort_image>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_object(likv &kvs) {
	apobject proto = boost::make_shared<ascii_proto_object>();
	string keyword;
	char ch;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if ((ch = keyword[0]) == 'o') {
			if      (iequals(keyword, "objname"))   proto->objname   = (*it).value;
			else if (iequals(keyword, "observer"))  proto->observer  = (*it).value;
			else if (iequals(keyword, "obstype"))   proto->obstype   = (*it).value;
			else if (iequals(keyword, "objra"))     proto->objra     = stod((*it).value);
			else if (iequals(keyword, "objdec"))    proto->objdec    = stod((*it).value);
			else if (iequals(keyword, "objepoch"))  proto->objepoch  = stod((*it).value);
			else if (iequals(keyword, "objerror"))  proto->objerror  = (*it).value;
		}
		else if (ch == 'p') {
			if      (iequals(keyword, "priority"))  proto->priority  = stoi((*it).value);
			else if (iequals(keyword, "plan_sn"))   proto->plan_sn   = stoi((*it).value);
			else if (iequals(keyword, "plan_time")) proto->plan_time = (*it).value;
			else if (iequals(keyword, "plan_type")) proto->plan_type = (*it).value;
			else if (iequals(keyword, "pair_id"))   proto->pair_id   = stoi((*it).value);
		}
		else if (ch == 'd') {
			if      (iequals(keyword, "dec"))       proto->dec      = stod((*it).value);
			else if (iequals(keyword, "delay"))     proto->delay    = stod((*it).value);
		}
		else if (ch == 'e') {
			if      (iequals(keyword, "epoch"))     proto->epoch    = stod((*it).value);
			else if (iequals(keyword, "expdur"))    proto->expdur   = stoi((*it).value);
			else if (iequals(keyword, "end_time"))  proto->end_time = (*it).value;
		}
		else if (ch == 'f') {
			if      (iequals(keyword, "filter"))    proto->filter   = (*it).value;
			if      (iequals(keyword, "frmcnt"))    proto->frmcnt   = stoi((*it).value);
			else if (iequals(keyword, "frmno"))     proto->frmno    = stoi((*it).value);
			else if (iequals(keyword, "field_id"))  proto->field_id = (*it).value;
		}
		else if (ch == 'r') {
			if      (iequals(keyword, "ra"))        proto->ra       = stod((*it).value);
			else if (iequals(keyword, "runname"))   proto->runname  = (*it).value;
		}
		else if (ch == 'i') {
			if      (iequals(keyword, "imgtype"))   proto->imgtype = (*it).value;
			else if (iequals(keyword, "ifilter"))   proto->ifilter = stoi((*it).value);
		}
		else if (ch == 'l') {
			if      (iequals(keyword, "loopcnt"))   proto->loopcnt = stoi((*it).value);
			else if (iequals(keyword, "loopno"))    proto->loopno  = stoi((*it).value);
		}
		else if (iequals(keyword, "begin_time"))  proto->begin_time = (*it).value;
		else if (iequals(keyword, "grid_id"))     proto->grid_id    = (*it).value;
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_expose(likv &kvs) {
	apexpose proto = boost::make_shared<ascii_proto_expose>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "command")) proto->command = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_camera(likv &kvs) {
	apcam proto = boost::make_shared<ascii_proto_camera>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "state"))     proto->state     = stoi((*it).value);
		else if (iequals(keyword, "errcode"))   proto->errcode   = stoi((*it).value);
		else if (iequals(keyword, "mcstate"))   proto->mcstate   = stoi((*it).value);
		else if (iequals(keyword, "focus"))     proto->focus     = stoi((*it).value);
		else if (iequals(keyword, "coolget"))   proto->coolget   = stod((*it).value);
		else if (iequals(keyword, "objname"))   proto->objname   = (*it).value;
		else if (iequals(keyword, "filename"))  proto->filename  = (*it).value;
		else if (iequals(keyword, "imgtype"))   proto->imgtype   = (*it).value;
		else if (iequals(keyword, "filter"))    proto->filter    = (*it).value;
		else if (iequals(keyword, "expdur"))    proto->expdur    = stod((*it).value);
		else if (iequals(keyword, "delay"))     proto->delay     = stod((*it).value);
		else if (iequals(keyword, "frmcnt"))    proto->frmcnt    = stoi((*it).value);
		else if (iequals(keyword, "loopcnt"))   proto->loopcnt   = stoi((*it).value);
		else if (iequals(keyword, "ifilter"))   proto->ifilter   = stoi((*it).value);
		else if (iequals(keyword, "frmno"))     proto->frmno     = stoi((*it).value);
		else if (iequals(keyword, "loopno"))    proto->loopno    = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_cooler(likv &kvs) {
	apcooler proto = boost::make_shared<ascii_proto_cooler>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "voltage"))  proto->voltage = stod((*it).value);
		else if (iequals(keyword, "current"))  proto->current = stod((*it).value);
		else if (iequals(keyword, "hotend"))   proto->hotend  = stod((*it).value);
		else if (iequals(keyword, "coolget"))  proto->coolget = stod((*it).value);
		else if (iequals(keyword, "coolset"))  proto->coolset = stod((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_vacuum(likv &kvs) {
	apvacuum proto = boost::make_shared<ascii_proto_vacuum>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "voltage"))  proto->voltage  = stod((*it).value);
		else if (iequals(keyword, "current"))  proto->current  = stod((*it).value);
		else if (iequals(keyword, "pressure")) proto->pressure = (*it).value;
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_fileinfo(likv &kvs) {
	apfileinfo proto = boost::make_shared<ascii_proto_fileinfo>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "grid_id"))  proto->grid     = (*it).value;
		else if (iequals(keyword, "field_id")) proto->field    = (*it).value;
		else if (iequals(keyword, "tmobs"))    proto->tmobs    = (*it).value;
		else if (iequals(keyword, "subpath"))  proto->subpath  = (*it).value;
		else if (iequals(keyword, "filename")) proto->filename = (*it).value;
		else if (iequals(keyword, "filesize")) proto->filesize = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_filestat(likv &kvs) {
	apfilestat proto = boost::make_shared<ascii_proto_filestat>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "status")) proto->status = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_plan(likv &kvs) {
	applan proto = boost::make_shared<ascii_proto_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "plan_sn")) proto->plan_sn = stoi((*it).value);
		else if (iequals(keyword, "state"))   proto->state   = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_check_plan(likv &kvs) {
	apchkplan proto = boost::make_shared<ascii_proto_check_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "plan_sn")) proto->plan_sn = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_abort_plan(likv &kvs) {
	apabtplan proto = boost::make_shared<ascii_proto_abort_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if (iequals(keyword, "plan_sn")) proto->plan_sn = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_append_plan(likv &kvs) {
	apappplan proto = boost::make_shared<ascii_proto_append_plan>();
	string keyword, value;
	char ch;
	const char seps[] = ";";

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		ch = keyword[0];
		// 识别关键字
		if (ch == 'p') {
			if      (iequals(keyword, "plan_sn"))   proto->plan_sn   = stoi((*it).value);
			else if (iequals(keyword, "plan_time")) proto->plan_time = (*it).value;
			else if (iequals(keyword, "plan_type")) proto->plan_type = (*it).value;
			else if (iequals(keyword, "priority"))  proto->priority  = stoi((*it).value);
			else if (iequals(keyword, "pair_id"))   proto->pair_id   = stoi((*it).value);
		}
		else if (ch == 'o') {
			if      (iequals(keyword, "observer"))   proto->observer  = (*it).value;
			else if (iequals(keyword, "obstype"))    proto->obstype   = (*it).value;
			else if (iequals(keyword, "objerror"))   proto->objerror  = (*it).value;
			else if (iequals(keyword, "objra"))      proto->objra     = stod((*it).value);
			else if (iequals(keyword, "objdec"))     proto->objdec    = stod((*it).value);
			else if (iequals(keyword, "objepoch"))   proto->objepoch  = stod((*it).value);
			else if (iequals(keyword, "objname") || iequals(keyword, "obj_id"))
				proto->objname   = (*it).value;
		}
		else if (ch == 'e') {
			if      (iequals(keyword, "epoch"))    proto->epoch    = stod((*it).value);
			else if (iequals(keyword, "end_time")) proto->end_time = (*it).value;
			else if (iequals(keyword, "expdur"))   proto->expdur   = stod((*it).value);
		}
		else if (ch == 'd') {
			if      (iequals(keyword, "dec"))   proto->dec   = stod((*it).value);
			else if (iequals(keyword, "delay")) proto->delay = stod((*it).value);
		}
		else if (ch == 'f') {
			if      (iequals(keyword, "filter")) {// 滤光片
				listring tokens;
				algorithm::split(tokens, (*it).value, is_any_of(seps), token_compress_on);
				for (listring::iterator it1 = tokens.begin(); it1 != tokens.end(); ++it1) {
					proto->filter.push_back(*it1);
				}
			}
			else if (iequals(keyword, "frmcnt"))   proto->frmcnt   = stoi((*it).value);
			else if (iequals(keyword, "field_id")) proto->field_id = (*it).value;
		}
		else if (ch == 'r') {
			if      (iequals(keyword, "ra"))      proto->ra      = stod((*it).value);
			else if (iequals(keyword, "runname")) proto->runname = (*it).value;
		}
		else if (iequals(keyword, "imgtype"))    proto->imgtype    = (*it).value;
		else if (iequals(keyword, "loopcnt"))    proto->loopcnt    = stoi((*it).value);
		else if (iequals(keyword, "begin_time")) proto->begin_time = (*it).value;
		else if (iequals(keyword, "grid_id"))    proto->grid_id    = (*it).value;
	}
	/* 基本参数检查 */
	if (proto->imgtype.empty()) proto.reset();

	return to_apbase(proto);
}
