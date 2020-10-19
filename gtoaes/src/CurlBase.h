/**
 * class CurlBase 使用curl, 构建数据/文件传输基类
 * @version 0.1
 * @date 2020-10-01
 * @note
 * - 构建发送数据/文件的通用接口
 * @note
 * 使用方法:
 * 1. prepare()
 * 2. 循环调用append_pair_kv和/或append_pair_file, 完成所有键值对/键-文件遍历后,
 * 3. upload()
 */

#ifndef SRC_CURLBASE_H_
#define SRC_CURLBASE_H_

#include <curl/curl.h>
#include <map>
#include <string>

class CurlBase {
protected:
	using MultiMapStr = std::multimap<std::string, std::string>;

protected:
	static int cnt_use_;	//< CURL库使用次数
	std::string urlRoot_;	//< URL地址: 根
	char errmsg_[512];	//< 错误提示
	MultiMapStr kvs_;	//< 键值对
	MultiMapStr files_;	//< 待上传文件

public:
	CurlBase(const std::string &urlRoot);
	virtual ~CurlBase();
	const char *GetError();

protected:
	/*!
	 * @brief 清空历史记录, 完成上传准备
	 */
	void prepare();
	/*!
	 * @brief 增加待上传的键值对
	 * @param keyword  关键字
	 * @param value    string格式的数值
	 */
	void append_pair_kv(const std::string &keyword, const std::string &value);
	/*!
	 * @brief 增加待上传的文件
	 * @param keywor    关键字
	 * @param filepath  文件可访问路径
	 */
	void append_pair_file(const std::string &keyword, const std::string &filepath);
	/*!
	 * @brief 上传键值对和文件
	 * @param urlRel  相对URL地址
	 * @return
	 * 0 - 成功
	 * 其它 - 失败
	 * @note
	 * 完整URL地址=urlRoot_ + urlRel
	 * @note
	 * 失败时可以调用
	 */
	int upload(const std::string &urlRel);
};

#endif /* SRC_CURLBASE_H_ */
