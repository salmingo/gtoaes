/*!
 * @file KvProtocol.cpp 定义文件, 定义GWAC/GFT系统中字符串型通信协议相关操作
 * @version 0.1
 * @date 2017-11-17
 **/

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <stdlib.h>
#include "AstroDeviceDef.h"
#include "KvProtocol.h"

using namespace boost;

//////////////////////////////////////////////////////////////////////////////
// 检查赤经/赤纬有效性
bool valid_ra(double ra) {
	return 0.0 <= ra && ra < 360.0;
}

bool valid_dec(double dec) {
	return -90.0 <= dec && dec <= 90.0;
}

//////////////////////////////////////////////////////////////////////////////
KvProtocol::KvProtocol()
	: szProto_(1400) {
	iBuf_ = 0;
	buff_.reset(new char[szProto_ * 10]); // 存储区
}

KvProtocol::~KvProtocol() {
}

const char* KvProtocol::output_compacted(string& output, int& n) {
	trim_right_if(output, is_punct() || is_space());
	return output_compacted(output.c_str(), n);
}

const char* KvProtocol::output_compacted(const char* s, int& n) {
	MtxLck lck(mtx_);
	char* buff = buff_.get() + iBuf_ * szProto_;
	if (++iBuf_ == 10) iBuf_ = 0;
	n = sprintf(buff, "%s\n", s);
	return buff;
}

void KvProtocol::compact_base(kvbase base, string &output) {
	output = base->type + " ";
	base->utc = to_iso_extended_string(second_clock::universal_time());
	join_kv(output, "utc",  base->utc);
	if (base->gid.size()) join_kv(output, "gid", base->gid);
	if (base->uid.size()) join_kv(output, "uid", base->uid);
	if (base->cid.size()) join_kv(output, "cid", base->cid);
}

void KvProtocol::resolve_rcvd(const char* rcvd, kv_proto_base &basis, likv &kvs) {
	const char seps[] = ",", *ptr;
	listring tokens;
	string type, keyword, value;

	// 提取协议类型
	for (ptr = rcvd; *ptr && *ptr != ' '; ++ptr) type += *ptr;
	while (*ptr && *ptr == ' ') ++ptr;
	// 分解键值对
	if (*ptr) algorithm::split(tokens, ptr, is_any_of(seps), token_compress_on);

	for (listring::iterator it = tokens.begin(); it != tokens.end(); ++it) {// 遍历键值对
		if (!resolve_kv(*it, keyword, value)) continue;
		// 识别通用项
		if      (iequals(keyword, "utc")) basis.utc = value;
		else if (iequals(keyword, "gid")) basis.gid = value;
		else if (iequals(keyword, "uid")) basis.uid = value;
		else if (iequals(keyword, "cid")) basis.cid = value;
		else {// 存储非通用项
			key_val kv;
			kv.keyword = keyword;
			kv.value   = value;
			kvs.push_back(kv);
		}
	}
}

bool KvProtocol::resolve_kv(string& kv, string& keyword, string& value) {
	char seps[] = "=";	// 分隔符: 等号
	listring tokens;

	keyword = "";
	value   = "";
	algorithm::split(tokens, kv, is_any_of(seps), token_compress_on);
	if (!tokens.empty()) { keyword = tokens.front(); trim(keyword); tokens.pop_front(); }
	if (!tokens.empty()) { value   = tokens.front(); trim(value); }
	return (!(keyword.empty() || value.empty()));
}

bool KvProtocol::compact_plan(ObsPlanItemPtr plan, string& output) {
	if (!plan.use_count()
			|| !TypeCoorSys::IsValid(plan->coorsys))
		return false;

	int m(plan->filters.size()), i;
	string filter;

	if (plan->gid.size()) join_kv(output, "gid", plan->gid);
	if (plan->uid.size()) join_kv(output, "uid", plan->uid);
	join_kv(output, "plan_sn",     plan->plan_sn);
	if (plan->plan_time.size())  join_kv(output, "plan_time",   plan->plan_time);
	if (plan->plan_type.size())  join_kv(output, "plan_type",   plan->plan_type);
	if (plan->obstype.size())    join_kv(output, "obstype",     plan->obstype);
	if (plan->grid_id.size())    join_kv(output, "grid_id",     plan->grid_id);
	if (plan->field_id.size())   join_kv(output, "field_id",    plan->field_id);
	if (plan->observer.size())   join_kv(output, "observer",    plan->observer);
	if (plan->objname.size())    join_kv(output, "objname",     plan->objname);
	if (plan->runname.size())    join_kv(output, "runname",     plan->runname);
	join_kv(output, "coorsys", plan->coorsys);
	if (plan->coorsys != TypeCoorSys::COORSYS_ORBIT
			&& valid_ra(plan->lon)
			&& valid_dec(plan->lat)) {
		join_kv(output, "lon",     plan->lon);
		join_kv(output, "lat",     plan->lat);
		join_kv(output, "epoch",   plan->epoch);
	}
	else {
		join_kv(output, "line1",   plan->line1);
		join_kv(output, "line2",   plan->line2);
	}
	if (valid_ra(plan->objra) && valid_dec(plan->objdec)) {
		join_kv(output, "objra",    plan->objra);
		join_kv(output, "objdec",   plan->objdec);
		join_kv(output, "objepoch", plan->objepoch);
	}
	if (plan->objerror.size())   join_kv(output, "objerror",    plan->objerror);
	join_kv(output, "imgtype",  plan->imgtype);
	for (i = 0; i < m; ++i) {
		filter += plan->filters[i] + "|";
	}
	if (filter.size())  join_kv(output, "filter",   filter);
	join_kv(output, "expdur",   plan->expdur);
	join_kv(output, "delay",    plan->delay);
	join_kv(output, "frmcnt",   plan->frmcnt);
	join_kv(output, "loopcnt",  plan->loopcnt);
	join_kv(output, "priority", plan->priority);

	string tmbegin, tmend;
	if (!plan->tmbegin.is_special())
		tmbegin = to_iso_extended_string(plan->tmbegin);
	if (!plan->tmend.is_special())
		tmend   = to_iso_extended_string(plan->tmend);
	if (tmbegin.size())      join_kv(output, "btime",   tmbegin);
	if (tmend.size())        join_kv(output, "etime",   tmend);
	if (plan->pair_id >= 0)  join_kv(output, "pair_id", plan->pair_id);

	ObservationPlanItem::KVVec& kvs = plan->kvs;
	ObservationPlanItem::KVVec::iterator it;
	for (it = kvs.begin(); it != kvs.end(); ++it) {
		join_kv(output, it->first, it->second);
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////
/*---------------- 封装通信协议 ----------------*/
/*
 * @note
 * register协议功能:
 * 1/ 设备在服务器上注册其ID编号
 * 2/ 服务器通知注册结果. ostype在相机设备上生效, 用于创建不同的目录结构和文件名及FITS头
 */
const char *KvProtocol::CompactRegister(kvreg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	return output_compacted(output, n);
}

const char *KvProtocol::CompactUnregister(kvunreg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactStart(const string& gid, const string& uid, int &n) {
	string output = KVTYPE_START;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactStop(const string& gid, const string& uid, int &n) {
	string output = KVTYPE_STOP;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactEnable(kvenable proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactDisable(kvdisable proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactObsSite(kvobsite proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "sitename",  proto->sitename);
	join_kv(output, "longitude", proto->lon);
	join_kv(output, "latitude",  proto->lat);
	join_kv(output, "altitude",  proto->alt);
	join_kv(output, "timezone",  proto->timezone);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactObss(kvobss proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output, camid;
	int m(proto->camera.size());

	compact_base(to_kvbase(proto), output);
	join_kv(output, "state",    proto->state);
	if (proto->plan_sn.size()) {
		join_kv(output, "plan_sn",  proto->plan_sn);
		join_kv(output, "op_time",  proto->op_time);
	}
	join_kv(output, "mount",    proto->mount);
	for (int i = 0; i < m; ++i) {
		camid = "cam#" + proto->camera[i].cid;
		join_kv(output, camid, proto->camera[i].state);
	}
	return output_compacted(output, n);
}

const char *KvProtocol::CompactAppendPlan(ObsPlanItemPtr plan, int &n) {
	string strplan, output;
	output = KVTYPE_APPPLAN;
	output += " ";
	if (compact_plan(plan, strplan))
		output += strplan;
	return output_compacted(output, n);
}

const char *KvProtocol::CompactImplementPlan(ObsPlanItemPtr plan, int &n) {
	string strplan, output;
	output = KVTYPE_IMPPLAN;
	output += " ";
	if (compact_plan(plan, strplan))
		output += strplan;
	return output_compacted(output, n);
}

const char *KvProtocol::CompactAbortPlan(const string& plan_sn, int &n) {
	string output = KVTYPE_ABTPLAN;
	output += " ";
	join_kv(output, "plan_sn", plan_sn);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactCheckPlan(const string& plan_sn, int &n) {
	string output = KVTYPE_CHKPLAN;
	output += " ";
	join_kv(output, "plan_sn", plan_sn);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactPlan(kvplan proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "plan_sn", proto->plan_sn);
	join_kv(output, "state",   proto->state);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactFindHome(const string& gid, const string& uid, int &n) {
	string output = KVTYPE_FINDHOME;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactHomeSync(const string& gid,
		const string& uid, double ra, double dec, int &n) {
	string output = KVTYPE_HOMESYNC;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	join_kv(output, "ra",  ra);
	join_kv(output, "dec", dec);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactSlewto(kvslewto proto, int &n) {
	if (!(proto.use_count() && TypeCoorSys::IsValid(proto->coorsys)))
		return NULL;
	if (!(proto->coorsys == TypeCoorSys::COORSYS_ORBIT
			|| (valid_ra(proto->lon) && valid_dec(proto->lat))))
		return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "coorsys", proto->coorsys);
	if (proto->coorsys == TypeCoorSys::COORSYS_ORBIT) {
		join_kv(output, "line1",  proto->line1);
		join_kv(output, "line2",  proto->line2);
	}
	else {
		join_kv(output, "lon",   proto->lon);
		join_kv(output, "lat",   proto->lat);
		join_kv(output, "epoch", proto->epoch);
	}

	return output_compacted(output, n);
}

const char *KvProtocol::CompactPark(const string& gid, const string& uid, int &n) {
	string output = KVTYPE_PARK;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactGuide(kvguide proto, int &n) {
	if (!proto.use_count()
			|| !(valid_ra(proto->ra) && valid_dec(proto->dec)))
		return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
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

const char *KvProtocol::CompactAbortSlew(const string& gid, const string& uid, int &n) {
	string output = KVTYPE_ABTSLEW;
	output += " ";
	if (gid.size()) join_kv(output, "gid", gid);
	if (uid.size()) join_kv(output, "uid", uid);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactMount(kvmount proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "state",    proto->state);
	join_kv(output, "errcode",  proto->errcode);
	join_kv(output, "ra",       proto->ra);
	join_kv(output, "dec",      proto->dec);
	join_kv(output, "azi",      proto->azi);
	join_kv(output, "alt",      proto->alt);

	return output_compacted(output, n);
}

const char *KvProtocol::CompactFWHM(kvfwhm proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "value", proto->value);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactFocus(kvfocus proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "state", proto->state);
	join_kv(output, "value", proto->position);

	return output_compacted(output, n);
}

const char *KvProtocol::CompactDome(kvdome proto, int& n) {
	if (!proto.use_count()) return NULL;
	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "azi", proto->azi);
	join_kv(output, "alt", proto->alt);
	if (valid_ra(proto->objazi))  join_kv(output, "objazi", proto->objazi);
	if (valid_dec(proto->objalt)) join_kv(output, "objalt", proto->objalt);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactSlit(kvslit proto, int& n) {
	if (!proto.use_count()) return NULL;
	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "command", proto->command);
	join_kv(output, "state",   proto->state);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactMirrorCover(kvmcover proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "command", proto->command);
	join_kv(output, "state",   proto->state);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactTakeImage(kvtakeimg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "objname",  proto->objname);
	join_kv(output, "imgtype",  proto->imgtype);
	join_kv(output, "filter",   proto->filter);
	join_kv(output, "expdur",   proto->expdur);
	join_kv(output, "frmcnt",   proto->frmcnt);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactAbortImage(kvabortimg proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactObject(kvobject proto, int &n) {
	if (!proto.use_count() || proto->plan_sn.empty()) return NULL;

	string output, tmbegin, tmend;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "plan_sn",    proto->plan_sn);
	if (proto->plan_time.size()) join_kv(output, "plan_time",  proto->plan_time);
	if (proto->plan_type.size()) join_kv(output, "plan_type",  proto->plan_type);
	if (proto->observer.size())  join_kv(output, "observer",   proto->observer);
	if (proto->obstype.size())   join_kv(output, "obstype",    proto->obstype);
	if (proto->grid_id.size())   join_kv(output, "grid_id",    proto->grid_id);
	if (proto->field_id.size())  join_kv(output, "field_id",   proto->field_id);
	if (proto->objname.size())   join_kv(output, "objname",    proto->objname);
	if (proto->runname.size())   join_kv(output, "runname",    proto->runname);
	join_kv(output, "coorsys",    proto->coorsys);
	if (proto->coorsys == TypeCoorSys::COORSYS_ORBIT) {
		join_kv(output, "line1", proto->line1);
		join_kv(output, "line2", proto->line2);
	}
	else if (valid_ra(proto->lon) && valid_dec(proto->lat)) {
		join_kv(output, "lon",      proto->lon);
		join_kv(output, "lat",      proto->lat);
		join_kv(output, "epoch",    proto->epoch);
	}
	if (valid_ra(proto->objra) && valid_dec(proto->objdec)) {
		join_kv(output, "objra",    proto->objra);
		join_kv(output, "objdec",   proto->objdec);
		join_kv(output, "objepoch", proto->objepoch);
	}
	if (!proto->objerror.empty())  join_kv(output, "objerror", proto->objerror);
	join_kv(output, "imgtype",    proto->imgtype);
	join_kv(output, "filter",     proto->filter);
	join_kv(output, "expdur",     proto->expdur);
	join_kv(output, "delay",      proto->delay);
	join_kv(output, "frmcnt",     proto->frmcnt);
	join_kv(output, "priority",   proto->priority);
	tmbegin = to_iso_extended_string(proto->tmbegin);
	tmend   = to_iso_extended_string(proto->tmend);
	join_kv(output, "btime",      tmbegin);
	join_kv(output, "etime",      tmend);
	join_kv(output, "pair_id",    proto->pair_id);
	join_kv(output, "iloop",      proto->iloop);

	likv& kvs = proto->kvs;
	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {
		join_kv(output, it->keyword, it->value);
	}

	return output_compacted(output, n);
}

const char *KvProtocol::CompactExpose(int cmd, int &n) {
	string output = KVTYPE_EXPOSE;
	output += " ";
	join_kv(output, "command", cmd);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactCamera(kvcam proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "state",     proto->state);
	join_kv(output, "errcode",   proto->errcode);
	join_kv(output, "coolget",   proto->coolget);
	join_kv(output, "filter",    proto->filter);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactCooler(kvcooler proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "voltage",  proto->voltage);
	join_kv(output, "current",  proto->current);
	join_kv(output, "hotend",   proto->hotend);
	join_kv(output, "coolget",  proto->coolget);
	join_kv(output, "coolset",  proto->coolset);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactVacuum(kvvacuum proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "voltage",  proto->voltage);
	join_kv(output, "current",  proto->current);
	join_kv(output, "pressure", proto->pressure);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactFileInfo(kvfileinfo proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "grid_id",  proto->grid);
	join_kv(output, "field_id", proto->field);
	join_kv(output, "tmobs",    proto->tmobs);
	join_kv(output, "subpath",  proto->subpath);
	join_kv(output, "filename", proto->filename);
	join_kv(output, "filesize", proto->filesize);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactFileStat(kvfilestat proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);

	join_kv(output, "status", proto->status);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactRainfall(kvrain proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "value", proto->value);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactWind(kvwind proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "orient", proto->orient);
	join_kv(output, "speed",  proto->speed);
	return output_compacted(output, n);
}

const char *KvProtocol::CompactCloud(kvcloud proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_kvbase(proto), output);
	join_kv(output, "value", proto->value);
	return output_compacted(output, n);
}

//////////////////////////////////////////////////////////////////////////////
kvbase KvProtocol::Resolve(const char *rcvd) {
	kvbase proto;
	kv_proto_base basis;
	likv kvs;
	string type;
	char ch;

	resolve_rcvd(rcvd, basis, kvs);
	type = basis.type;
	ch = type[0];
	// 按照协议类型解析键值对
	if (ch == 'a' || ch == 'A') {
		if      (iequals(type, KVTYPE_ABTSLEW))  proto = resolve_abortslew  (kvs);
		else if (iequals(type, KVTYPE_ABTIMG))   proto = resolve_abortimg   (kvs);
		else if (iequals(type, KVTYPE_APPPLAN))  proto = resolve_append_plan(kvs);
		else if (iequals(type, KVTYPE_ABTPLAN))  proto = resolve_abort_plan (kvs);
	}
	else if (ch == 'c' || ch == 'C') {
		if      (iequals(type, KVTYPE_CAMERA))   proto = resolve_camera    (kvs);
		else if (iequals(type, KVTYPE_COOLER))   proto = resolve_cooler    (kvs);
		else if (iequals(type, KVTYPE_CLOUD))    proto = resolve_cloud     (kvs);
		else if (iequals(type, KVTYPE_CHKPLAN))  proto = resolve_check_plan(kvs);
	}
	else if (ch == 'd' || ch == 'D') {
		if      (iequals(type, KVTYPE_DOME))     proto = resolve_dome   (kvs);
		else if (iequals(type, KVTYPE_DISABLE))  proto = resolve_disable(kvs);
	}
	else if (ch == 'e' || ch == 'E') {
		if      (iequals(type, KVTYPE_EXPOSE))   proto = resolve_expose(kvs);
		else if (iequals(type, KVTYPE_ENABLE))   proto = resolve_enable(kvs);
	}
	else if (ch == 'f' || ch == 'F') {
		if      (iequals(type, KVTYPE_FILEINFO)) proto = resolve_fileinfo(kvs);
		else if (iequals(type, KVTYPE_FILESTAT)) proto = resolve_filestat(kvs);
		else if (iequals(type, KVTYPE_FWHM))     proto = resolve_fwhm    (kvs);
		else if (iequals(type, KVTYPE_FOCUS))    proto = resolve_focus   (kvs);
		else if (iequals(type, KVTYPE_FINDHOME)) proto = resolve_findhome(kvs);
	}
	else if (ch == 'o' || ch == 'O') {
		if      (iequals(type, KVTYPE_OBSS))     proto = resolve_obss  (kvs);
		else if (iequals(type, KVTYPE_OBJECT))   proto = resolve_object(kvs);
		else if (iequals(type, KVTYPE_OBSITE))   proto = resolve_obsite(kvs);
	}
	else if (ch == 'p' || ch == 'P') {
		if      (iequals(type, KVTYPE_PLAN))     proto = resolve_plan(kvs);
		else if (iequals(type, KVTYPE_PARK))     proto = resolve_park(kvs);
	}
	else if (ch == 'r' || ch == 'R') {
		if      (iequals(type, KVTYPE_RAINFALL)) proto = resolve_rainfall(kvs);
		else if (iequals(type, KVTYPE_REG))      proto = resolve_register(kvs);
	}
	else if (ch == 's' || ch == 'S') {
		if      (iequals(type, KVTYPE_SLEWTO))   proto = resolve_slewto(kvs);
		else if (iequals(type, KVTYPE_START))    proto = resolve_start (kvs);
		else if (iequals(type, KVTYPE_STOP))     proto = resolve_stop  (kvs);
		else if (iequals(type, KVTYPE_SLIT))     proto = resolve_slit  (kvs);
	}
	else if (ch == 'm' || ch == 'M') {
		if      (iequals(type, KVTYPE_MOUNT))    proto = resolve_mount (kvs);
		else if (iequals(type, KVTYPE_MCOVER))   proto = resolve_mcover(kvs);
	}
	else if (iequals(type, KVTYPE_WIND))     proto = resolve_wind          (kvs);
	else if (iequals(type, KVTYPE_VACUUM))   proto = resolve_vacuum        (kvs);
	else if (iequals(type, KVTYPE_GUIDE))    proto = resolve_guide         (kvs);
	else if (iequals(type, KVTYPE_HOMESYNC)) proto = resolve_homesync      (kvs);
	else if (iequals(type, KVTYPE_IMPPLAN))  proto = resolve_implement_plan(kvs);
	else if (iequals(type, KVTYPE_TAKIMG))   proto = resolve_takeimg       (kvs);
	else if (iequals(type, KVTYPE_UNREG))    proto = resolve_unregister    (kvs);

	if (proto.unique()) *proto = basis;
	return proto;
}

kvbase KvProtocol::ResolveClient(const char* rcvd) {
	kvbase proto;
	kv_proto_base basis;
	likv kvs;
	string type;
	char ch;

	resolve_rcvd(rcvd, basis, kvs);
	type = basis.type;
	ch = type[0];
	/*---------------- 分项解析 ----------------*/
	if (ch == 'a' || ch == 'A') {
		if      (iequals(type, KVTYPE_ABTSLEW))  proto = resolve_abortslew  (kvs);
		else if (iequals(type, KVTYPE_ABTIMG))   proto = resolve_abortimg   (kvs);
		else if (iequals(type, KVTYPE_APPPLAN))  proto = resolve_append_plan(kvs);
		else if (iequals(type, KVTYPE_ABTPLAN))  proto = resolve_abort_plan (kvs);
	}
	else if (ch == 'f' || ch == 'F') {
		if (iequals(type, KVTYPE_FWHM))          proto = resolve_fwhm    (kvs);
		else if (iequals(type, KVTYPE_FOCUS))    proto = resolve_focus   (kvs);
		else if (iequals(type, KVTYPE_FINDHOME)) proto = resolve_findhome(kvs);
	}
	else if (ch == 's' || ch == 'S') {
		if      (iequals(type, KVTYPE_SLEWTO))   proto = resolve_slewto(kvs);
		else if (iequals(type, KVTYPE_START))    proto = resolve_start (kvs);
		else if (iequals(type, KVTYPE_STOP))     proto = resolve_stop  (kvs);
		else if (iequals(type, KVTYPE_SLIT))     proto = resolve_slit  (kvs);
	}
	else if (iequals(type, KVTYPE_CHKPLAN))  proto = resolve_check_plan    (kvs);
	else if (iequals(type, KVTYPE_DISABLE))  proto = resolve_disable       (kvs);
	else if (iequals(type, KVTYPE_ENABLE))   proto = resolve_enable        (kvs);
	else if (iequals(type, KVTYPE_PARK))     proto = resolve_park          (kvs);
	else if (iequals(type, KVTYPE_REG))      proto = resolve_register      (kvs);
	else if (iequals(type, KVTYPE_MCOVER))   proto = resolve_mcover        (kvs);
	else if (iequals(type, KVTYPE_GUIDE))    proto = resolve_guide         (kvs);
	else if (iequals(type, KVTYPE_HOMESYNC)) proto = resolve_homesync      (kvs);
	else if (iequals(type, KVTYPE_IMPPLAN))  proto = resolve_implement_plan(kvs);
	else if (iequals(type, KVTYPE_TAKIMG))   proto = resolve_takeimg       (kvs);
	else if (iequals(type, KVTYPE_UNREG))    proto = resolve_unregister    (kvs);

	/*---------------- 分项解析 ----------------*/
	if (proto.unique()) *proto = basis;
	return proto;
}

kvbase KvProtocol::ResolveMount(const char* rcvd) {
	kvbase proto;
	kv_proto_base basis;
	likv kvs;
	string type;
	char ch;

	resolve_rcvd(rcvd, basis, kvs);
	type = basis.type;
	ch = type[0];
	/*---------------- 分项解析 ----------------*/
	if (ch == 'f' || ch == 'F') {
		if      (iequals(type, KVTYPE_FWHM))     proto = resolve_fwhm    (kvs);
		else if (iequals(type, KVTYPE_FOCUS))    proto = resolve_focus   (kvs);
		else if (iequals(type, KVTYPE_FINDHOME)) proto = resolve_findhome(kvs);
	}
	else if (ch == 's' || ch == 'S') {
		if      (iequals(type, KVTYPE_SLEWTO))   proto = resolve_slewto(kvs);
		else if (iequals(type, KVTYPE_SLIT))     proto = resolve_slit  (kvs);
	}
	else if (ch == 'm' || ch == 'M') {
		if      (iequals(type, KVTYPE_MOUNT))    proto = resolve_mount (kvs);
		else if (iequals(type, KVTYPE_MCOVER))   proto = resolve_mcover(kvs);
	}
	else if (iequals(type, KVTYPE_ABTSLEW))  proto = resolve_abortslew(kvs);
	else if (iequals(type, KVTYPE_DOME))     proto = resolve_dome     (kvs);
	else if (iequals(type, KVTYPE_PARK))     proto = resolve_park     (kvs);
	else if (iequals(type, KVTYPE_GUIDE))    proto = resolve_guide    (kvs);
	else if (iequals(type, KVTYPE_HOMESYNC)) proto = resolve_homesync (kvs);
	/*---------------- 分项解析 ----------------*/
	if (proto.unique()) *proto = basis;
	return proto;
}

kvbase KvProtocol::ResolveCamera(const char* rcvd) {
	kvbase proto;
	kv_proto_base basis;
	likv kvs;
	string type;
	char ch;

	resolve_rcvd(rcvd, basis, kvs);
	type = basis.type;
	ch = type[0];
	/*---------------- 分项解析 ----------------*/
	if (ch == 'f' || ch == 'F') {
		if      (iequals(type, KVTYPE_FILEINFO)) proto = resolve_fileinfo(kvs);
		else if (iequals(type, KVTYPE_FILESTAT)) proto = resolve_filestat(kvs);
		else if (iequals(type, KVTYPE_FOCUS))    proto = resolve_focus   (kvs);
	}
	else if (ch == 'o' || ch == 'O') {
		if      (iequals(type, KVTYPE_OBJECT))   proto = resolve_object(kvs);
		else if (iequals(type, KVTYPE_OBSITE))   proto = resolve_obsite(kvs);
	}
	else if (iequals(type, KVTYPE_ABTIMG))   proto = resolve_abortimg(kvs);
	else if (iequals(type, KVTYPE_CAMERA))   proto = resolve_camera  (kvs);
	else if (iequals(type, KVTYPE_EXPOSE))   proto = resolve_expose  (kvs);
	else if (iequals(type, KVTYPE_SLEWTO))   proto = resolve_slewto  (kvs);
	else if (iequals(type, KVTYPE_MCOVER))   proto = resolve_mcover  (kvs);
	else if (iequals(type, KVTYPE_TAKIMG))   proto = resolve_takeimg (kvs);
	/*---------------- 分项解析 ----------------*/
	if (proto.unique()) *proto = basis;
	return proto;
}

kvbase KvProtocol::ResolveEnv(const char* rcvd) {
	kvbase proto;
	kv_proto_base basis;
	likv kvs;
	string type;
	char ch;

	resolve_rcvd(rcvd, basis, kvs);
	type = basis.type;
	ch = type[0];
	/*---------------- 分项解析 ----------------*/
	if      (iequals(type, KVTYPE_RAINFALL)) proto = resolve_rainfall(kvs);
	else if (iequals(type, KVTYPE_WIND))     proto = resolve_wind    (kvs);
	else if (iequals(type, KVTYPE_CLOUD))    proto = resolve_cloud   (kvs);
	/*---------------- 分项解析 ----------------*/
	if (proto.unique()) *proto = basis;
	return proto;
}

kvbase KvProtocol::resolve_register(likv &kvs) {
	kvreg proto = boost::make_shared<kv_proto_reg>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_unregister(likv &kvs) {
	kvunreg proto = boost::make_shared<kv_proto_unreg>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_start(likv &kvs) {
	kvstart proto = boost::make_shared<kv_proto_start>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_stop(likv &kvs) {
	kvstop proto = boost::make_shared<kv_proto_stop>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_enable(likv &kvs) {
	kvenable proto = boost::make_shared<kv_proto_enable>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_disable(likv &kvs) {
	kvdisable proto = boost::make_shared<kv_proto_disable>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_obsite(likv &kvs) {
	kvobsite proto = boost::make_shared<kv_proto_obsite>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "sitename"))  proto->sitename = it->value;
		else if (iequals(keyword, "longitude")) proto->lon      = stod(it->value);
		else if (iequals(keyword, "latitude"))  proto->lat      = stod(it->value);
		else if (iequals(keyword, "altitude"))  proto->alt      = stod(it->value);
		else if (iequals(keyword, "timezone"))  proto->timezone = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_obss(likv &kvs) {
	kvobss proto = boost::make_shared<kv_proto_obss>();
	string keyword, precid = "cam#";
	int nprecid = precid.size();

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "state"))   proto->state   = stoi(it->value);
		else if (iequals(keyword, "plan_sn")) proto->plan_sn = it->value;
		else if (iequals(keyword, "op_time")) proto->op_time = it->value;
		else if (iequals(keyword, "mount"))   proto->mount   = stoi(it->value);
		else if (keyword.find(precid) == 0) {// 相机工作状态. 关键字 cam#xxx
			kv_proto_obss::camera_state cs;
			cs.cid   = keyword.substr(nprecid);
			cs.state = stoi(it->value);
			proto->camera.push_back(cs);
		}
	}
	return to_kvbase(proto);
}

void KvProtocol::resolve_plan(likv& kvs, ObsPlanItemPtr plan) {
	string keyword;
	char ch;
	ObservationPlanItem::KVVec &plan_kvs = plan->kvs;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		ch = keyword[0];

		if (ch == 'e') {
			if      (iequals(keyword, "epoch"))   plan->epoch  = stod(it->value);
			else if (iequals(keyword, "expdur"))  plan->expdur = stoi(it->value);
			else if (iequals(keyword, "etime"))   plan->SetTimeEnd(it->value);
		}
		else if (ch == 'f') {
			if      (iequals(keyword, "filter"))    plan->AppendFilter(it->value);
			else if (iequals(keyword, "frmcnt"))    plan->frmcnt   = stoi(it->value);
			else if (iequals(keyword, "field_id"))  plan->field_id = it->value;
		}
		else if (ch == 'g') {
			if      (iequals(keyword, "gid"))       plan->gid = it->value;
			else if (iequals(keyword, "grid_id"))   plan->grid_id  = it->value;
		}
		else if (ch == 'i') {
			if      (iequals(keyword, "imgtype")) plan->imgtype = it->value;
			else if (iequals(keyword, "iloop"))   plan->iloop   = stoi(it->value);
		}
		else if (ch == 'l') {
			if      (iequals(keyword, "lon"))     plan->lon   = stod(it->value);
			else if (iequals(keyword, "lat"))     plan->lat   = stod(it->value);
			else if (iequals(keyword, "line1"))   plan->line1 = it->value;
			else if (iequals(keyword, "line2"))   plan->line2 = it->value;
		}
		else if ((ch = keyword[0]) == 'o') {
			if      (iequals(keyword, "objname"))   plan->objname  = it->value;
			else if (iequals(keyword, "observer"))  plan->observer = it->value;
			else if (iequals(keyword, "obstype"))   plan->obstype  = it->value;
			else if (iequals(keyword, "objra"))     plan->objra    = stod(it->value);
			else if (iequals(keyword, "objdec"))    plan->objdec   = stod(it->value);
			else if (iequals(keyword, "objepoch"))  plan->objepoch = stod(it->value);
			else if (iequals(keyword, "objerror"))  plan->objerror = it->value;
		}
		else if (ch == 'p') {
			if      (iequals(keyword, "priority"))  plan->priority  = stoi(it->value);
			else if (iequals(keyword, "plan_sn"))   plan->plan_sn   = it->value;
			else if (iequals(keyword, "plan_time")) plan->plan_time = it->value;
			else if (iequals(keyword, "plan_type")) plan->plan_type = it->value;
			else if (iequals(keyword, "pair_id"))   plan->pair_id   = stoi(it->value);
		}
		else if (iequals(keyword, "btime"))    plan->SetTimeBegin(it->value);
		else if (iequals(keyword, "coorsys"))  plan->coorsys  = stoi(it->value);
		else if (iequals(keyword, "delay"))    plan->delay    = stod(it->value);
		else if (iequals(keyword, "runname"))  plan->runname  = it->value;
		else if (iequals(keyword, "uid"))      plan->uid      = it->value;
		else {
			ObservationPlanItem::KVPair kv(it->keyword, it->value);
			plan_kvs.push_back(kv);
		}
	}
}

kvbase KvProtocol::resolve_append_plan(likv &kvs) {
	kvappplan proto = boost::make_shared<kv_proto_append_plan>();
	ObsPlanItemPtr plan = proto->plan;
	resolve_plan(kvs, plan);
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_implement_plan(likv &kvs) {
	kvimpplan proto = boost::make_shared<kv_proto_implement_plan>();
	ObsPlanItemPtr plan = proto->plan;
	resolve_plan(kvs, plan);
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_abort_plan(likv &kvs) {
	kvabtplan proto = boost::make_shared<kv_proto_abort_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "plan_sn")) proto->plan_sn = it->value;
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_check_plan(likv &kvs) {
	kvchkplan proto = boost::make_shared<kv_proto_check_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "plan_sn")) proto->plan_sn = it->value;
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_plan(likv &kvs) {
	kvplan proto = boost::make_shared<kv_proto_plan>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "plan_sn")) proto->plan_sn = it->value;
		else if (iequals(keyword, "state"))   proto->state   = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_findhome(likv &kvs) {
	kvfindhome proto = boost::make_shared<kv_proto_find_home>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_homesync(likv &kvs) {
	kvhomesync proto = boost::make_shared<kv_proto_home_sync>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "ra"))     proto->ra    = stod(it->value);
		else if (iequals(keyword, "dec"))    proto->dec    = stod(it->value);
		else if (iequals(keyword, "epoch"))  proto->epoch = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_slewto(likv &kvs) {
	kvslewto proto = boost::make_shared<kv_proto_slewto>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "coorsys"))  proto->coorsys  = stoi(it->value);
		else if (iequals(keyword, "lon"))      proto->lon      = stod(it->value);
		else if (iequals(keyword, "lat"))      proto->lat      = stod(it->value);
		else if (iequals(keyword, "epoch"))    proto->epoch    = stod(it->value);
		else if (iequals(keyword, "line1"))    proto->line1    = it->value;
		else if (iequals(keyword, "line2"))    proto->line2    = it->value;
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_park(likv &kvs) {
	kvpark proto = boost::make_shared<kv_proto_park>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_guide(likv &kvs) {
	kvguide proto = boost::make_shared<kv_proto_guide>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "ra"))       proto->ra     = stod(it->value);
		else if (iequals(keyword, "dec"))      proto->dec    = stod(it->value);
		else if (iequals(keyword, "objra"))    proto->objra  = stod(it->value);
		else if (iequals(keyword, "objdec"))   proto->objdec = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_abortslew(likv &kvs) {
	kvabortslew proto = boost::make_shared<kv_proto_abort_slew>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_mount(likv &kvs) {
	kvmount proto = boost::make_shared<kv_proto_mount>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "state"))    proto->state   = stoi(it->value);
		else if (iequals(keyword, "errcode"))  proto->errcode = stoi(it->value);
		else if (iequals(keyword, "ra"))       proto->ra      = stod(it->value);
		else if (iequals(keyword, "dec"))      proto->dec     = stod(it->value);
		else if (iequals(keyword, "azi"))      proto->azi     = stod(it->value);
		else if (iequals(keyword, "alt"))      proto->alt     = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_fwhm(likv &kvs) {
	kvfwhm proto = boost::make_shared<kv_proto_fwhm>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "value"))  proto->value = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_focus(likv &kvs) {
	kvfocus proto = boost::make_shared<kv_proto_focus>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "state"))     proto->state    = stoi(it->value);
		else if (iequals(keyword, "position"))  proto->position = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_dome(likv &kvs) {
	kvdome proto = boost::make_shared<kv_proto_dome>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "azi"))    proto->azi    = stod(it->value);
		else if (iequals(keyword, "alt"))    proto->azi    = stod(it->value);
		else if (iequals(keyword, "objazi")) proto->objazi = stod(it->value);
		else if (iequals(keyword, "objalt")) proto->objalt = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_slit(likv &kvs) {
	kvslit proto = boost::make_shared<kv_proto_slit>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "command")) proto->command = stoi(it->value);
		else if (iequals(keyword, "state"))   proto->state   = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_mcover(likv &kvs) {
	kvmcover proto = boost::make_shared<kv_proto_mcover>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "command")) proto->command = stoi(it->value);
		else if (iequals(keyword, "state"))   proto->state   = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_takeimg(likv &kvs) {
	kvtakeimg proto = boost::make_shared<kv_proto_take_image>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "objname"))  proto->objname = it->value;
		else if (iequals(keyword, "imgtype"))  proto->imgtype = it->value;
		else if (iequals(keyword, "filter"))   proto->filter  = it->value;
		else if (iequals(keyword, "expdur"))   proto->expdur  = stod(it->value);
		else if (iequals(keyword, "frmcnt"))   proto->frmcnt  = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_abortimg(likv &kvs) {
	kvabortimg proto = boost::make_shared<kv_proto_abort_image>();
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_object(likv &kvs) {
	kvobject proto = boost::make_shared<kv_proto_object>();
	likv& objkvs = proto->kvs;
	string keyword;
	char ch;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		ch = keyword[0];

		if (ch == 'e') {
			if      (iequals(keyword, "epoch"))  proto->epoch  = stod(it->value);
			else if (iequals(keyword, "expdur")) proto->expdur = stoi(it->value);
			else if (iequals(keyword, "etime"))  {
				try {
					proto->tmend = from_iso_extended_string(it->value);
				}
				catch(std::out_of_range& ex) {
					proto->tmend = second_clock::universal_time() + hours(24);
				}
			}
		}
		else if (ch == 'i') {
			if      (iequals(keyword, "imgtype")) proto->imgtype = it->value;
			else if (iequals(keyword, "iloop"))   proto->iloop   = stoi(it->value);
		}
		else if (ch == 'l') {
			if      (iequals(keyword, "lon"))   proto->lon   = stod(it->value);
			else if (iequals(keyword, "lat"))   proto->lat   = stod(it->value);
			else if (iequals(keyword, "line1")) proto->line1 = it->value;
			else if (iequals(keyword, "line2")) proto->line2 = it->value;
		}
		else if ((ch = keyword[0]) == 'o') {
			if      (iequals(keyword, "objname"))   proto->objname   = it->value;
			else if (iequals(keyword, "observer"))  proto->observer  = it->value;
			else if (iequals(keyword, "obstype"))   proto->obstype   = it->value;
			else if (iequals(keyword, "objra"))     proto->objra     = stod(it->value);
			else if (iequals(keyword, "objdec"))    proto->objdec    = stod(it->value);
			else if (iequals(keyword, "objepoch"))  proto->objepoch  = stod(it->value);
			else if (iequals(keyword, "objerror"))  proto->objerror  = it->value;
		}
		else if (ch == 'p') {
			if      (iequals(keyword, "priority"))  proto->priority  = stoi(it->value);
			else if (iequals(keyword, "plan_sn"))   proto->plan_sn   = it->value;
			else if (iequals(keyword, "plan_time")) proto->plan_time = it->value;
			else if (iequals(keyword, "plan_type")) proto->plan_type = it->value;
			else if (iequals(keyword, "pair_id"))   proto->pair_id   = stoi(it->value);
		}
		else if (ch == 'f') {
			if      (iequals(keyword, "filter"))    proto->filter   = it->value;
			if      (iequals(keyword, "frmcnt"))    proto->frmcnt   = stoi(it->value);
			else if (iequals(keyword, "field_id"))  proto->field_id = it->value;
		}
		else if (iequals(keyword, "coorsys"))   proto->coorsys  = stoi(it->value);
		else if (iequals(keyword, "delay"))     proto->delay    = stod(it->value);
		else if (iequals(keyword, "grid_id"))   proto->grid_id  = it->value;
		else if (iequals(keyword, "runname"))   proto->runname  = it->value;
		else if (iequals(keyword, "btime")) {
			try {
				proto->tmbegin = from_iso_extended_string(it->value);
			}
			catch(std::out_of_range& ex) {
				proto->tmbegin = second_clock::universal_time();
			}
		}
		else {
			key_val kv;
			kv.keyword = it->keyword;
			kv.value   = it->value;
			objkvs.push_back(kv);
		}
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_expose(likv &kvs) {
	kvexpose proto = boost::make_shared<kv_proto_expose>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "command")) proto->command = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_camera(likv &kvs) {
	kvcam proto = boost::make_shared<kv_proto_camera>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "state"))    proto->state   = stoi(it->value);
		else if (iequals(keyword, "errcode"))  proto->errcode = stoi(it->value);
		else if (iequals(keyword, "coolget"))  proto->coolget = stoi(it->value);
		else if (iequals(keyword, "filter"))   proto->filter  = it->value;
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_cooler(likv &kvs) {
	kvcooler proto = boost::make_shared<kv_proto_cooler>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "voltage"))  proto->voltage = stod(it->value);
		else if (iequals(keyword, "current"))  proto->current = stod(it->value);
		else if (iequals(keyword, "hotend"))   proto->hotend  = stod(it->value);
		else if (iequals(keyword, "coolget"))  proto->coolget = stod(it->value);
		else if (iequals(keyword, "coolset"))  proto->coolset = stod(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_vacuum(likv &kvs) {
	kvvacuum proto = boost::make_shared<kv_proto_vacuum>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "voltage"))  proto->voltage  = stod(it->value);
		else if (iequals(keyword, "current"))  proto->current  = stod(it->value);
		else if (iequals(keyword, "pressure")) proto->pressure = it->value;
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_fileinfo(likv &kvs) {
	kvfileinfo proto = boost::make_shared<kv_proto_fileinfo>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "grid_id"))  proto->grid     = it->value;
		else if (iequals(keyword, "field_id")) proto->field    = it->value;
		else if (iequals(keyword, "tmobs"))    proto->tmobs    = it->value;
		else if (iequals(keyword, "subpath"))  proto->subpath  = it->value;
		else if (iequals(keyword, "filename")) proto->filename = it->value;
		else if (iequals(keyword, "filesize")) proto->filesize = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_filestat(likv &kvs) {
	kvfilestat proto = boost::make_shared<kv_proto_filestat>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "state")) proto->status = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_rainfall(likv &kvs) {
	kvrain proto = boost::make_shared<kv_proto_rainfall>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "value"))  proto->value  = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_wind(likv &kvs) {
	kvwind proto = boost::make_shared<kv_proto_wind>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if      (iequals(keyword, "orient")) proto->orient = stoi(it->value);
		else if (iequals(keyword, "speed"))  proto->speed  = stoi(it->value);
	}
	return to_kvbase(proto);
}

kvbase KvProtocol::resolve_cloud(likv &kvs) {
	kvcloud proto = boost::make_shared<kv_proto_cloud>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;

		if (iequals(keyword, "value"))  proto->value = stoi(it->value);
	}
	return to_kvbase(proto);
}
