/**
 * @file AnnexProtocol.cpp 附属设备通信协议
 * @version 0.1
 * @date 2019-09-29
 */

#include <boost/make_shared.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include "AnnexProtocol.h"
#include "GLog.h"
using namespace boost;

AnnexProtocol::AnnexProtocol() {
	ibuf_ = 0;
	buff_.reset(new char[100 * 10]); //< 存储区
}

AnnexProtocol::AnnexProtocol(const string &gid, const string &uid)
	: AnnexProtocol() {
	gid_ = gid;
	uid_ = uid;
	ibuf_ = 0;
	buff_.reset(new char[100 * 10]); //< 存储区
}

AnnexProtocol::~AnnexProtocol() {

}

char* AnnexProtocol::get_buff() {
	mutex_lock lck(mtx_);
	char* buff = buff_.get() + ibuf_ * 100;
	if (++ibuf_ == 10) ibuf_ = 0;
	return buff;
}
//////////////////////////////////////////////////////////////////////////////
/*------------------ 接口: 封装通信协议, 用于套接字输出 ------------------*/
const char* AnnexProtocol::CompactSlit(const int command, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#slit%02d%%\n", command);
	return buff;
}

const char* AnnexProtocol::CompactSlit(const string& gid, const string& uid, const int command, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sslit%02d%%\n", gid.c_str(), uid.c_str(), command);
	return buff;
}

const char* AnnexProtocol::CompactFwhm(const string& cid, const double fwhm, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sfwhm%s%04d%%\n", gid_.c_str(), uid_.c_str(), cid.c_str(),
			int(fwhm * 100));
	return buff;
}

const char* AnnexProtocol::CompactFocus(const string& cid, const int position, int& n) {
	char *buff = get_buff();

	n = sprintf(buff, "g#%s%sfocus%s%+05d%%\n", gid_.c_str(), uid_.c_str(), cid.c_str(),
			position);
	return buff;
}

//////////////////////////////////////////////////////////////////////////////
/*------------------ 接口: 解析通信协议 ------------------*/
annpbase AnnexProtocol::Resolve(const char* rcvd) {
	annpbase proto;
	string_ref sref(rcvd);
	string_ref prefix("g#");			// 定义引导符
	string_ref suffix("%");				// 定义结束符: 同时还是中间符!!!(区别处理)
	string_ref type_rain("rain");		// 定义协议: rain
	string_ref type_slit("slit");		// 定义协议: slit
	string_ref type_focus("focus");		// 定义协议: focus
	string_ref type_fwhm("fwhm");		// 定义协议: fwhm
	char sep      = '%';	// 数据间分隔符
	int unit_len  = 3;		// 约定: 单元标志长度为3字节
	int cid_len   = 3;		// 约定: 相机标志长度为3字节
	int slit_len  = 2;		// 约定: 镜盖状态长度为2字节
	int focus_len = 5;		// 约定: 焦点位置长度为5字节
	int n(sref.length() - suffix.length()), pos, pos1, i, j;
	char buff[10];

#ifdef NDEBUG
	_gLog.Write("annex received [%s]", rcvd);
#endif

	if (!(sref.starts_with(prefix) && sref.ends_with(suffix))) {// 非法前缀/后缀
		return proto;
	}

	/* 解析通信协议 */
	if      ((pos = sref.find(type_rain)) > 0) {
		// 格式: g#rain<value>%
		annprain body = boost::make_shared<annexproto_rain>();
		for (i = pos + type_rain.length(), j = 0; sref.at(i) != sep; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->value = atoi(buff);
		proto = static_pointer_cast<annexproto_base>(body);
	}
	else if ((pos = sref.find(type_slit)) > 0) {
		// 格式: g#slit<value>%
		annpslit body = boost::make_shared<annexproto_slit>();
		for (j = 0, i = pos + type_slit.length(); i < n && j < slit_len; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->state = atoi(buff);
		proto = static_pointer_cast<annexproto_base>(body);
	}
	else if ((pos = sref.find(type_focus)) > 0) {
		// 格式: g#<group_id><unit_id>focus<cam_id><value>%
		annpfocus body = boost::make_shared<annexproto_focus>();
		pos1 = pos - unit_len;
		for (i = prefix.length(); i < pos1; ++i) body->gid += sref.at(i);
		for (; i < pos; ++i) body->uid += sref.at(i);
		for (i = pos + type_focus.length(), j = 0; i < n && j < cid_len; ++i, ++j) body->cid += sref.at(i);
		for (j = 0; i < n && j < focus_len; ++i, ++j) buff[j] = sref.at(i);
		buff[j] = '\0';
		body->position = atoi(buff);
		proto = static_pointer_cast<annexproto_base>(body);
	}

	return proto;
}
