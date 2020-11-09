/**
 * @file     NonkvProtocol.cpp
 * @brief    解析和构建非键值对类型字符串通信协议
 * @version  1.0
 * @date     2020-11-09
 * @author   卢晓猛
 */

#include <string.h>
#include <stdlib.h>
#include <boost/algorithm/string.hpp>
#include "NonkvProtocol.h"

using namespace boost;

NonkvProtocol::NonkvProtocol() :
		szProto_(1400),
		lenGid_(3),
		lenUid_(3),
		lenCid_(3),
		lenReady_ (strlen(NONKVTYPE_READY)),
		lenState_ (strlen(NONKVTYPE_STATE)),
		lenUtc_   (strlen(NONKVTYPE_UTC)),
		lenMount_ (strlen(NONKVTYPE_MOUNT)),
		lenSlit_  (strlen(NONKVTYPE_SLIT)),
		lenMCover_(strlen(NONKVTYPE_MCOVER)),
		lenFocus_ (strlen(NONKVTYPE_FOCUS)),
		lenRain_  (strlen(NONKVTYPE_RAIN)) {
	iBuf_ = 0;
	buff_.reset(new char[szProto_ * 10]); // 存储区
}

NonkvProtocol::~NonkvProtocol() {
}

nonkvbase NonkvProtocol::Resove(const char* rcvd) {
	/*
	 * rcvd: 结束符 = %
	 */
	nonkvbase proto;
	const char *ptr;

	if      ((ptr = strstr(rcvd, NONKVTYPE_UTC)))    proto = resolve_utc        (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_MOUNT)))  proto = resolve_mount      (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_FOCUS)))  proto = resolve_focus      (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_RAIN)))   proto = resolve_rain       (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_SLIT)))   proto = resolve_slit       (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_READY)))  proto = resolve_ready      (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_STATE)))  proto = resolve_state      (rcvd, ptr - rcvd);
	else if ((ptr = strstr(rcvd, NONKVTYPE_MCOVER))) proto = resolve_mirr_cover (rcvd, ptr - rcvd);

	return proto;
}

const char* NonkvProtocol::CompactFindHome(const string& gid, const string& uid, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%sra%ddec%d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_FINDHOME, 1, 1);
	return buff;
}

const char* NonkvProtocol::CompactHomeSync(const string& gid, const string& uid, double ra, double dec, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%07d%%%+07d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_HOMESYNC,
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* NonkvProtocol::CompactSlewto(const string& gid, const string& uid, double ra, double dec, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%07d%%%+07d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_SLEWTO,
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* NonkvProtocol::CompactGuide(const string& gid, const string& uid, double d_ra, double d_dec, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%+05d%%%+05d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_GUIDE,
			int(d_ra * 3600), int(d_dec * 3600));
	return buff;
}

const char* NonkvProtocol::CompactPark(const string& gid, const string& uid, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_PARK);
	return buff;
}

const char* NonkvProtocol::CompactAbortSlew(const string& gid, const string& uid, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_ABTSLEW);
	return buff;
}

const char* NonkvProtocol::CompactSlit(const string& gid, const string& uid, int cmd, int& n) {
	char* buff = get_buffer();
	n = sprintf (buff, "g#");
	if (gid.size()) n += sprintf (buff + n, "%s", gid.c_str());
	if (uid.size()) n += sprintf (buff + n, "%s", uid.c_str());
	n += sprintf (buff + n, "%s%02d%%\n", NONKVTYPE_SLIT, cmd);
	return buff;
}

const char* NonkvProtocol::CompactMirrCover(const string& gid, const string& uid, const string& cid, int cmd, int& n) {
	char* buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%s%s%%\n", gid.c_str(), uid.c_str(), cid.c_str(), NONKVTYPE_MCOVER,
			cmd ? "open" : "close");
	return buff;
}

const char* NonkvProtocol::CompactFWHM(const string& gid, const string& uid, const string& cid, double fwhm, int& n) {
	char *buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%s%04d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_FWHM, cid.c_str(),
			int(fwhm * 100));
	return buff;
}

const char* NonkvProtocol::CompactFocus(const string& gid, const string& uid, const string& cid, int pos, int& n) {
	char *buff = get_buffer();
	n = sprintf(buff, "g#%s%s%s%s%+05d%%\n", gid.c_str(), uid.c_str(), NONKVTYPE_FOCUS, cid.c_str(), pos);
	return buff;
}

char* NonkvProtocol::get_buffer() {
	MtxLck lck(mtx_);
	char* buff = buff_.get() + iBuf_ * szProto_;
	if (++iBuf_ == 10) iBuf_ = 0;
	return buff;
}

nonkvbase NonkvProtocol::resolve_ready(const char* rcvd, int pos) {
	nonkvready proto;
	char tmp[10];
	int n(strlen(rcvd));

	if (pos == (lenGid_ + lenUid_) && (n - pos) > lenReady_) {
		proto.reset(new nonkv_proto_ready);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenReady_;
		n -= (pos + 1);
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->ready = atoi(tmp);
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_state(const char* rcvd, int pos) {
	nonkvstate proto;
	char tmp[10];
	int n(strlen(rcvd));

	if (pos == (lenGid_ + lenUid_) && (n - pos) > lenState_) {
		proto.reset(new nonkv_proto_state);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenState_;
		n -= (pos + 1);
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->state = atoi(tmp);
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_utc(const char* rcvd, int pos) {
	nonkvutc proto;
	char tmp[10];
	int n(strlen(rcvd));

	if (pos == (lenGid_ + lenUid_) && (n - pos) > lenUtc_) {
		proto.reset(new nonkv_proto_utc);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenUtc_;
		n -= (pos + 1);
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->utc = tmp;
		replace_first(proto->utc, "%", "T");
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_mount(const char* rcvd, int pos) {
	nonkvmount proto;
	char tmp[10];
	const char* ptr;
	int n(strlen(rcvd)), i;

	if (pos == (lenGid_ + lenUid_) && (n - pos - 2) > lenMount_) {
		proto.reset(new nonkv_proto_mount);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenMount_;
		n -= (pos + 1);
		ptr = rcvd + pos;

		for (i = 0; *ptr && *ptr != '%'; ++i, ++ptr)
			tmp[i] = *ptr;
		tmp[i] = 0;
		proto->ra = atof(tmp) * 1E-4;

		if (*ptr) {
			for (++ptr, i = 0; *ptr && *ptr != '%'; ++i, ++ptr)
				tmp[i] = *ptr;
			tmp[i] = 0;
			proto->dec = atof(tmp) * 1E-4;
		}
		else
			proto.reset();
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_slit(const char* rcvd, int pos) {
	nonkvslit proto;
	char tmp[10];
	int n(strlen(rcvd));

	proto.reset(new nonkv_proto_slit);

	if (pos >= lenGid_) {
		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
	}
	if (pos == (lenGid_ + lenUid_)) {
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;
	}

	pos += lenSlit_;
	n -= (pos + 1);
	if (!n)
		proto.reset();
	else {
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->state = atoi(tmp);
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_mirr_cover(const char* rcvd, int pos) {
	nonkvmcover proto;
	char tmp[10];
	int n(strlen(rcvd));

	if (pos == (lenGid_ + lenUid_) && (n - pos - lenCid_) > lenMCover_) {
		proto.reset(new nonkv_proto_mcover);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenMCover_;
		strncpy (tmp, rcvd + pos, lenCid_);
		tmp[lenCid_] = 0;
		proto->cid = tmp;

		pos += lenCid_;
		n -= (pos + 1);
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->state = atoi(tmp);
	}
	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_focus(const char* rcvd, int pos) {
	nonkvfocus proto;
	char tmp[10];
	int n(strlen(rcvd));

	if (pos == (lenGid_ + lenUid_) && (n - pos - lenCid_) > lenFocus_) {
		proto.reset(new nonkv_proto_focus);

		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
		strncpy (tmp, rcvd + lenGid_, lenUid_);
		tmp[lenUid_] = 0;
		proto->uid = tmp;

		pos += lenFocus_;
		strncpy (tmp, rcvd + pos, lenCid_);
		tmp[lenCid_] = 0;
		proto->cid = tmp;

		pos += lenCid_;
		n -= (pos + 1);
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->position = atoi(tmp);
	}

	return to_nonkvbase(proto);
}

nonkvbase NonkvProtocol::resolve_rain(const char* rcvd, int pos) {
	nonkvrain proto;
	char tmp[10];
	int n(strlen(rcvd));

	proto.reset(new nonkv_proto_rain);
	if (pos >= lenGid_) {
		strncpy (tmp, rcvd, lenGid_);
		tmp[lenGid_] = 0;
		proto->gid = tmp;
	}

	pos += lenRain_;
	n -= (pos + 1);
	if (!n)
		proto.reset();
	else {
		strncpy (tmp, rcvd + pos, n);
		tmp[n] = 0;
		proto->state = atoi(tmp);
	}
	return to_nonkvbase(proto);
}
