/*
 * @file mountproto.cpp 封装与转台相关的通信协议
 * @author         卢晓猛
 * @version        2.0
 * @date           2017年2月20日
 */

#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include "GLog.h"
#include "mountproto.h"

using namespace boost;

mount_proto::mount_proto() {
	proto_type_ = "";
	ibuff_ = 0;
	buff_.reset(new char[5120]); // 512*10=5120
	/* 为不同类型通信协议分别分配存储空间 */
	proto_focus_ = boost::make_shared<mntproto_focus>();
	proto_mcover_ = boost::make_shared<mntproto_mcover>();
	proto_position_ = boost::make_shared<mntproto_position>();
	proto_ready_ = boost::make_shared<mntproto_ready>();
	proto_status_ = boost::make_shared<mntproto_status>();
	proto_utc_ = boost::make_shared<mntproto_utc>();
}

mount_proto::~mount_proto() {
}

const char* mount_proto::resolve(const char* rcvd, mpbase& body) {
	bool retv(true);
	string_ref sref(rcvd);
	string_ref prefix("g#");				// 定义引导符
	string_ref suffix("%");					// 定义结束符: 同时还是中间符!!!
	string_ref type_ready("ready");			// 定义协议: ready
	string_ref type_state("status");		// 定义协议: status
	string_ref type_utc("utc");				// 定义协议: utc
	string_ref type_pos("currentpos");		// 定义协议: currentpos
	string_ref type_focus("focus");			// 定义协议: focus
	string_ref type_mcover("mirr");			// 定义协议: mirr
	char sep = '%';							// 数据间分隔符
	int unit_len = 3;						// 约定: 单元标志长度为3字节
	int camera_len = 3;						// 约定: 相机标志长度为3字节
	int focus_len  = 4;						// 约定: 焦点位置长度为4字节
	int mc_len = 2;							// 约定: 镜盖状态长度为2字节
	int n(sref.length() - suffix.length()), pos, i, j, k;
	char buff[10], ch;

	if (!sref.starts_with(prefix) || !sref.ends_with(suffix)) {
		gLog.Write("mount_proto::resolve", LOG_WARN, "illegal protocol: <%s>", rcvd);
		retv = false;
	}
	else {
		if      ((pos = sref.find(type_ready)) > 0) {// ready
			boost::shared_ptr<mntproto_ready> proto = proto_ready_;
			proto->reset();
			proto_type_ = "ready";
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			for (i = pos + type_ready.length(), j = 0; i < n; ++i, ++j, ++proto->n) proto->ready[j] = sref.at(i) - '0';
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else if ((pos = sref.find(type_state)) > 0) {// state
			boost::shared_ptr<mntproto_status> proto = proto_status_;
			proto->reset();
			proto_type_ = "state";
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			for (i = pos + type_state.length(), j = 0; i < n; ++i, ++j, ++proto->n) proto->state[j] = sref.at(i) - '0';
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else if ((pos = sref.find(type_utc)) > 0) {// utc
			boost::shared_ptr<mntproto_utc> proto = proto_utc_;
			proto->reset();
			proto_type_ = "utc";
			pos -= unit_len;
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			pos += unit_len;
			for (; i < pos; ++i) proto->unit_id += sref.at(i);
			for (i = pos + type_utc.length(); i < n; ++i) proto->utc += sref.at(i);
			boost::replace_first(proto->utc, "%", "T");
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else if ((pos = sref.find(type_pos)) > 0) {// currentpos
			boost::shared_ptr<mntproto_position> proto = proto_position_;
			proto->reset();
			proto_type_ = "position";
			pos -= unit_len;
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			pos += unit_len;
			for (; i < pos; ++i) proto->unit_id += sref.at(i);
			for (i = pos + type_pos.length(), j = 0; i < n && (ch = sref.at(i) != sep); ++i, ++j) buff[j] = sref.at(i);
			buff[j] = '\0';
			proto->ra = atoi(buff) * 1E-4;
			for (++i, j = 0; i < n; ++i, ++j) buff[j] = sref.at(i);
			buff[j] = '\0';
			proto->dec = atoi(buff) * 1E-4;
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else if ((pos = sref.find(type_focus)) > 0) {// focus
			boost::shared_ptr<mntproto_focus> proto = boost::make_shared<mntproto_focus>();
			proto->reset();
			proto_type_ = "focus";
			pos -= unit_len;
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			pos += unit_len;
			for (; i < pos; ++i) proto->unit_id += sref.at(i);
			for (i = pos + type_focus.length(), j = 0; i < n && j < camera_len; ++i, ++j) proto->camera_id += sref.at(i);
			for (j = 0; i < n && j < focus_len; ++i, ++j) buff[j] = sref.at(i);
			buff[j] = '\0';
			proto->position = atoi(buff);
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else if ((pos = sref.find(type_mcover)) > 0) {// mirr
			boost::shared_ptr<mntproto_mcover> proto = proto_mcover_;
			proto->reset();
			proto_type_ = "mcover";
			pos -= unit_len;
			for (i = prefix.length(); i < pos; ++i) proto->group_id += sref.at(i);
			pos += unit_len;
			for (; i < pos; ++i) proto->unit_id += sref.at(i);

			k = 0;
			i = pos + type_mcover.length();
			while (i < n) {
				mntproto_mc_state& state = proto->state[k];
				for (; i < n; ++i) state.camera_id += sref.at(i);
				for (j = 0; i < n && j < mc_len; ++i, ++j)  buff[j] = sref.at(i);
				buff[j] = '\0';
				state.state = atoi(buff);
				++k;
			}
			proto->n = k;
			body = boost::static_pointer_cast<mntproto_base>(proto);
		}
		else {
			gLog.Write("mount_proto::resolve", LOG_WARN, "undefined or wrong protocol type");
			retv = false;
		}
	}

	return retv ? proto_type_.c_str() : "";
}

char *mount_proto::get_buffptr() {
	mutex_lock lck(mtxbuff_);
	char *buff = buff_.get() + ibuff_ * 512;
	if (++ibuff_ == 10) ibuff_ = 0;
	return buff;
}

const char* mount_proto::compact_find_home(const std::string& group_id, const std::string& unit_id,
		const bool ra, const bool dec, int& n) {
	/* 组装搜索零点指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%shomera%ddec%d%%\n", group_id.c_str(), unit_id.c_str(),
			ra ? 1 : 0, dec ? 1 : 0);
	return buff;
}

const char* mount_proto::compact_home_sync(const std::string& group_id, const std::string& unit_id,
		const double ra, const double dec, int& n) {
	/* 组装同步零点指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%ssync%07d%%%+07d%%\n", group_id.c_str(), unit_id.c_str(),
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* mount_proto::compact_slew(const std::string& group_id, const std::string& unit_id,
		const double ra, const double dec, int& n) {
	/* 组装指向指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%sslew%07d%%%+07d%%\n", group_id.c_str(), unit_id.c_str(),
			int(ra * 10000), int(dec * 10000));
	return buff;
}

const char* mount_proto::compact_guide(const std::string& group_id, const std::string& unit_id,
		const int ra, const int dec, int& n) {
	/* 组装导星指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%sguide%+05d%%%+05d%%\n", group_id.c_str(), unit_id.c_str(),
			ra, dec);
	return buff;
}

const char* mount_proto::compact_park(const std::string& group_id, const std::string& unit_id, int& n) {
	/* 组装复位指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%spark%%\n", group_id.c_str(), unit_id.c_str());
	return buff;
}

const char* mount_proto::compact_abort_slew(const std::string& group_id, const std::string& unit_id, int& n) {
	/* 组装停止指向指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%sabortslew%%\n", group_id.c_str(), unit_id.c_str());
	return buff;
}

const char* mount_proto::compact_fwhm(const std::string& group_id, const std::string& unit_id,
		const std::string& camera_id, double fwhm, int& n) {
	/* 组装调焦指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%sfwhm%s%04d%%\n", group_id.c_str(), unit_id.c_str(), camera_id.c_str(),
			int(fwhm * 100));
	return buff;
}

const char* mount_proto::compact_focus(const std::string& group_id, const std::string& unit_id,
		const std::string& camera_id, const int focus, int& n) {
	/* 组装调焦指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%sfocus%s%+05d%%\n", group_id.c_str(), unit_id.c_str(), camera_id.c_str(),
			focus);
	return buff;
}

const char* mount_proto::compact_mirror_cover(const std::string& group_id, const std::string& unit_id,
		const std::string& camera_id, const int command, int& n) {
	/* 组装镜盖操作指令 */
	char *buff = get_buffptr();
	n = sprintf(buff, "g#%s%smirr%s%s%%\n", group_id.c_str(), unit_id.c_str(), camera_id.c_str(),
			command == 1 ? "open" : "close");
	return buff;
}
