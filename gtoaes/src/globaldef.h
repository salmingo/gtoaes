/*!
 * @file globaldef.h 声明全局唯一参数
 * @version 0.6
 * @date 2017/06/08
 * @note
 * - 更新相机控制指令
 * - 更新相机工作状态
 * - 为ascproto和mountproto中buff_添加互斥机制
 * @date 2018/09/05
 * @note
 * - 暂不考虑通用性, 仅支持GWAC系统与兴隆60cm GFT系统
 */

#ifndef GLOBALDEF_H_
#define GLOBALDEF_H_

// 软件名称、版本与版权
#define DAEMON_NAME			"gtoaes"
#define DAEMON_AUTHORITY	"© SVOM Group, NAOC"
#define DAEMON_VERSION		"v1.0 @ July, 2020"

// 日志文件路径与文件名前缀
const char gLogDir[]    = "/var/log/gtoaes";
const char gLogPrefix[] = "gtoaes_";

// 软件配置文件
const char gConfigPath[] = "/usr/local/etc/gtoaes.xml";

// 文件锁位置
const char gPIDPath[] = "/var/run/gtoaes.pid";

#endif /* GLOBALDEF_H_ */
