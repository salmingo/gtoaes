/**
 * @file  KvProtocolBase.h
 * @brief 基于键值对的字符串型通信协议的基类
 * @note
 * - 通信协议采用编码字符串格式, 字符串以换行符结束
 * - 协议由三部分组成, 其格式为:
 *   type [keyword=value,]+<term>
 *   type   : 协议类型
 *   keyword: 单项关键字
 *   value  : 单项值
 *   term   : 换行符
 **/

#ifndef _KV_PROTOCOL_BASE_H_
#define _KV_PROTOCOL_BASE_H_

#include <string>
#include <list>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/format.hpp>

using std::string;

//////////////////////////////////////////////////////////////////////////////
/****************************************************************************/
/*!
 *  系统标志定义:
 * - 观测系统由gid:uid构成唯一关键字
 * - 相机设备由gid:uid构成唯一关键字
 * - 关键字归属关系为: cid < uid < gid
 * - 子关键字可以为空, 为空时代表通配符
 * - 子关键字不为空时, 父关键字不可为空
 */
/****************************************************************************/
struct key_val {///< 关键字-键值对
	string keyword;
	string value;
};
typedef std::list<key_val> likv;	///< pair_key_val列表

struct kv_proto_base {
	string type;	///< 协议类型
	string utc;		///< 时间标签. 格式: YYYY-MM-DDThh:mm:ss
	string gid;		///< 组编号
	string uid;		///< 单元编号
	string cid;		///< 相机编号

public:
	kv_proto_base &operator=(const kv_proto_base &other) {
		if (this != &other) {
			type = other.type;
			utc  = other.utc;
			gid  = other.gid;
			uid  = other.uid;
			cid  = other.cid;
		}
		return *this;
	}
};
typedef boost::shared_ptr<kv_proto_base> kvbase;

/*!
 * @brief 将kv_proto_base继承类的boost::shared_ptr型指针转换为kvbase类型
 * @param proto 协议指针
 * @return
 * kvbase类型指针
 */
template <class T> kvbase to_kvbase(T proto) {
	return boost::static_pointer_cast<kv_proto_base>(proto);
}

/*!
 * @brief 将kvbase类型指针转换为其继承类的boost::shared_ptr型指针
 * @param proto 协议指针
 * @return
 * kvbase继承类指针
 */
template <class T> boost::shared_ptr<T> from_kvbase(kvbase proto) {
	return boost::static_pointer_cast<T>(proto);
}

#endif
