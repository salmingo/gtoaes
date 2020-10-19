/**
 * class CurlBase 使用curl, 构建数据/文件传输基类
 * @version 0.1
 **/

#include <stdio.h>
#include <string.h>
#include "CurlBase.h"

using std::string;

int CurlBase::cnt_use_ = 0;

CurlBase::CurlBase(const string &urlRoot) {
	urlRoot_ = urlRoot;
#ifdef WINDOWS
	if (urlRoot.back() != '\\') urlRoot_ += "\\";
#else
	if (urlRoot.back() != '/') urlRoot_ += "/";
#endif
	if (!cnt_use_) curl_global_init(CURL_GLOBAL_ALL);
	++cnt_use_;
}

CurlBase::~CurlBase() {
	kvs_.clear();
	files_.clear();
	if (--cnt_use_ == 0)
		curl_global_cleanup();
}

const char * CurlBase::GetError() {
	return errmsg_;
}

void CurlBase::prepare() {
	kvs_.clear();
	files_.clear();
}

void CurlBase::append_pair_kv(const string &keyword, const string &value) {
	kvs_.insert({keyword, value});
}

void CurlBase::append_pair_file(const string &keyword, const string &filepath) {
	files_.insert({keyword, filepath});
}

int CurlBase::upload(const std::string &urlRel) {
	if (kvs_.empty() && files_.empty()) {
		strcpy (errmsg_, "nothing for post");
		return -1;
	}

	CURL *hCurl = curl_easy_init();
	if (!hCurl) {
		strcpy (errmsg_, "curl_easy_init() failed");
		return -2;
	}
#if defined(NDEBUG) || defined(DEBUG)
	curl_easy_setopt(hCurl, CURLOPT_VERBOSE, 1);
#endif

	CURLcode code;
	string url = urlRoot_ + urlRel;
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;
	/* 构建键值对 */
	for (MultiMapStr::iterator it = kvs_.begin(); it != kvs_.end(); ++it) {
		curl_formadd (&post, &last, CURLFORM_COPYNAME, it->first.data(),
				CURLFORM_COPYCONTENTS, it->second.data(), CURLFORM_END);
	}
	/* 构建待上传文件 */
	for (MultiMapStr::iterator it = files_.begin(); it != files_.end(); ++it) {
		curl_formadd (&post, &last, CURLFORM_COPYNAME, it->first.data(),
				CURLFORM_FILE, it->second.data(), CURLFORM_END);
	}
	/* 执行上传操作 */
	curl_easy_setopt(hCurl, CURLOPT_URL,      url.c_str());
	curl_easy_setopt(hCurl, CURLOPT_HTTPPOST, post);
	if ((code = curl_easy_perform(hCurl))) {
		long http_code(0);
		code = curl_easy_getinfo(hCurl, CURLINFO_RESPONSE_CODE, &http_code);
	}
	if (code) strcpy (errmsg_, curl_easy_strerror(code));
	curl_formfree (post);
	curl_easy_cleanup (hCurl);

	return code;
}
