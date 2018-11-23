/*
 * @file MountProto.cpp 定义文件, 定义与转台相关通信协议格式, 并封装其操作
 * @version 0.2
 * @date 2017-10-02
 */

#include <boost/make_shared.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include "MountProtocol.h"

using namespace boost;
//////////////////////////////////////////////////////////////////////////////
/// 工厂函数, 用于构建MountProto指针
MountPtr make_mount() {
	return boost::make_shared<MountProto>();
}

MountPtr make_mount(const string& gid, const string& uid) {
	return boost::make_shared<MountProto>(gid, uid);
}

//////////////////////////////////////////////////////////////////////////////
MountProto::MountProto() {
	ibuf_ = 0;
	buff_.reset(new char[100 * 10]); //< 存储区
}

MountProto::MountProto(const string& gid, const string& uid) {
	gid_ = gid;
	uid_ = uid;
	ibuf_ = 0;
	buff_.reset(new char[100 * 10]); //< 存储区
}

MountProto::~MountProto() {
}

char* MountProto::get_buff() {
	mutex_lock lck(mtx_);
	char* buff = buff_.get() + ibuf_ * 100;
	if (++ibuf_ == 10) ibuf_ = 0;
	return buff;
}

//////////////////////////////////////////////////////////////////////////////
/*------------------ 接口: 封装通信协议, 用于套接字输出 ------------------*/
const char* MountProto::CompactFindhome(const bool ra, const bool dec, int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%shomera%ddec%d%%\n", gid_.c_str(), uid_.c_str(),
			ra ? 1 : 0, dec ? 1 : 0);
	return buff;
}

const char* MountProto::CompactHomesync(const double ra, const double dec, int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%ssync%07d%%%+07d%%\n", gid_.c_str(), uid_.c_str(),
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* MountProto::CompactSlew(const double ra, const double dec, int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%sslew%07d%%%+07d%%\n", gid_.c_str(), uid_.c_str(),
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* MountProto::CompactGuide(const double ra, const double dec, int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%sguide%+05d%%%+05d%%\n", gid_.c_str(), uid_.c_str(),
			int(ra * 3600), int(dec * 3600));
	return buff;
}

const char* MountProto::CompactPark(int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%spark%%\n", gid_.c_str(), uid_.c_str());
	return buff;
}

const char* MountProto::CompactAbortslew(int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%salortslew%%\n", gid_.c_str(), uid_.c_str());
	return buff;
}

const char* MountProto::CompactMCover(const string& cid, const int command, int& n) {
	char* buff = get_buff();

	n = sprintf(buff, "g#%s%smirr%s%s%%\n", gid_.c_str(), uid_.c_str(), cid.c_str(),
			command ? "open" : "close");
	return buff;
}

const char* MountProto::CompactFwhm(const string& cid, const double fwhm, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sfwhm%s%04d%%\n", gid_.c_str(), uid_.c_str(), cid.c_str(),
			int(fwhm * 100));
	return buff;
}

const char* MountProto::CompactFocus(const string& cid, const int position, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sfocus%s%+05d%%\n", gid_.c_str(), uid_.c_str(), cid.c_str(),
			position);
	return buff;
}

const char* MountProto::CompactFocusync(const string& cid, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sfocsync%s%%\n", gid_.c_str(), uid_.c_str(), cid.c_str());
	return buff;
}
//////////////////////////////////////////////////////////////////////////////
/*------------------ 接口: 解析通信协议 ------------------*/
mpbase MountProto::Resolve(const char* rcvd) {
	mpbase proto;
	string_ref sref(rcvd);
	string_ref prefix("g#");			// 定义引导符
	string_ref suffix("%");				// 定义结束符: 同时还是中间符!!!(区别处理)
	string_ref type_ready("ready");		// 定义协议: ready
	string_ref type_state("status");	// 定义协议: status
	string_ref type_utc("utc");			// 定义协议: utc
	string_ref type_pos("currentpos");	// 定义协议: currentpos
	string_ref type_focus("focus");		// 定义协议: focus
	string_ref type_mcover("mirr");		// 定义协议: mirr
	char sep      = '%';	// 数据间分隔符
	int unit_len  = 3;		// 约定: 单元标志长度为3字节
	int cid_len   = 3;		// 约定: 相机标志长度为3字节
	int focus_len = 5;		// 约定: 焦点位置长度为5字节
	int mc_len    = 2;		// 约定: 镜盖状态长度为2字节
	int n(sref.length() - suffix.length()), pos, pos1, i, j, k;
	char buff[10], ch;

	if (!(sref.starts_with(prefix) && sref.ends_with(suffix))) {// 非法前缀/后缀
		return proto;
	}

	/* 解析通信协议 */
	if ((pos = sref.find(type_ready)) > 0) {
		// 格式: g#<group_id><unit_id>ready<state>%
		mpready body = boost::make_shared<mntproto_ready>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) proto->gid += sref.at(i);
		for (; i < pos; ++i) proto->uid += sref.at(i);
		body->ready = sref.at(pos + type_ready.length()) - '0';
		proto = static_pointer_cast<mntproto_base>(body);
	}
	else if ((pos = sref.find(type_state)) > 0) {
		// 格式: g#<group_id><unit_id>status<state>%
		mpstate body = boost::make_shared<mntproto_state>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) proto->gid += sref.at(i);
		for (; i < pos; ++i) proto->uid += sref.at(i);
		body->state = sref.at(pos + type_state.length()) - '0';
		proto = static_pointer_cast<mntproto_base>(body);
	}
	else if ((pos = sref.find(type_utc)) > 0) {
		// 格式: g#<group_id><unit_id>utc<YYYY-MM-DD>%<hh:mm:ss.sss>%
		mputc body = boost::make_shared<mntproto_utc>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) body->gid += sref.at(i);
		for (; i < pos; ++i) body->uid += sref.at(i);
		for (i = pos + type_utc.length(); i < n; ++i) body->utc += sref.at(i);
		replace_first(body->utc, "%", "T");
		proto = static_pointer_cast<mntproto_base>(body);
	}
	else if ((pos = sref.find(type_pos)) > 0) {
		// 格式: g#<group_id><unit_id>currentpos<ra>%<dec>%
		mpposition body = boost::make_shared<mntproto_position>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) body->gid += sref.at(i);
		for (; i < pos; ++i) body->uid += sref.at(i);
		for (i = pos + type_pos.length(), j = 0; i < n && (ch = sref.at(i) != sep); ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->ra = atoi(buff) * 1E-4;
		for (++i, j = 0; i < n; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->dc = atoi(buff) * 1E-4;
		proto = static_pointer_cast<mntproto_base>(body);
	}
	else if ((pos = sref.find(type_focus)) > 0) {
		// 格式: g#<group_id><unit_id>focus<cam_id><value>%
		mpfocus body = boost::make_shared<mntproto_focus>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) body->gid += sref.at(i);
		for (; i < pos; ++i) body->uid += sref.at(i);
		for (i = pos + type_focus.length(), j = 0; i < n && j < cid_len; ++i, ++j) body->cid += sref.at(i);
		for (j = 0; i < n && j < focus_len; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->position = atoi(buff);
		proto = static_pointer_cast<mntproto_base>(body);
	}
	else if ((pos = sref.find(type_mcover)) > 0) {
		// 格式: g#<group_id><unit_id>mirr<cam_id><state>%
		mpmcover body = boost::make_shared<mntproto_mcover>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) body->gid += sref.at(i);
		for (; i < pos; ++i) body->uid += sref.at(i);
		for (i = pos + type_mcover.length(), j = 0; i < n && j < cid_len; ++i, ++j) body->cid += sref.at(i);
		for (j = 0; i < n && j < mc_len; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->state = atoi(buff);
		proto = static_pointer_cast<mntproto_base>(body);
	}

	return proto;
}
