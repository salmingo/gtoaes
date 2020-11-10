/**
 * @file DatabaseCurl.h
 * @brief 基于curl的数据库访问接口
 * @version 1.0
 * @date 2020-11-10
 * @author 卢晓猛
 */

#ifndef SRC_DATABASECURL_H_
#define SRC_DATABASECURL_H_

#include <boost/smart_ptr/shared_ptr.hpp>
#include "CurlBase.h"

class DatabaseCurl: public CurlBase {
public:
	DatabaseCurl(const std::string &urlRoot);
	virtual ~DatabaseCurl();

public:
	using Pointer = boost::shared_ptr<DatabaseCurl>;

public:
	static Pointer Create(const std::string &urlRoot) {
		return Pointer(new DatabaseCurl(urlRoot));
	}
};
using DBCurlPtr = DatabaseCurl::Pointer;

#endif /* SRC_DATABASECURL_H_ */
