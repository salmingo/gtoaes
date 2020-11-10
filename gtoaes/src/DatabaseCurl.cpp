/**
 * @file DatabaseCurl.cpp
 * @brief 基于curl的数据库访问接口
 * @version 1.0
 * @date 2020-11-10
 * @author 卢晓猛
 */

#include "DatabaseCurl.h"

DatabaseCurl::DatabaseCurl(const std::string &urlRoot)
		: CurlBase(urlRoot) {

}

DatabaseCurl::~DatabaseCurl() {

}
