/**
 * @file globaldef.h 声明全局唯一参数
 * @version 0.1
 * @date 2019/10/22
 */

#ifndef GLOBALDEF_H_
#define GLOBALDEF_H_

// 软件名称、版本与版权
#define DAEMON_NAME			"gtoaes_hn"
#define DAEMON_VERSION		"v0.1 @ Oct, 2019"
#define DAEMON_AUTHORITY	"© SVOM Group, NAOC"

// 日志文件路径与文件名前缀
const char gLogDir[]    = "/var/log/gtoaes";
const char gLogPrefix[] = "gtoaes_";

// 软件配置文件
const char gConfigPath[] = "/usr/local/etc/gtoaes.xml";

// 文件锁位置
const char gPIDPath[] = "/var/run/gtoaes.pid";

#endif /* GLOBALDEF_H_ */
