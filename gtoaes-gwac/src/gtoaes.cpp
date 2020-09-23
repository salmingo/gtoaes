/*-----------------------------------------------------------------------------
 Name        : gtoaes.cpp
 Author      : Xiaomeng Lu(lxm@nao.cas.cn)
 Copyright   : 中国科学院国家天文台, SVOM团组
 Description : GWAC观测辅助执行系统
 ==============================================================================
 Date        : 2017-01-19
 Version     : 0.3
 Note        :
 @li 依据关联软件工作模式, 更改软件结构
 @li 优化几个核心类库
 ==============================================================================
 Date        : 2017-05-06
 Version     : 0.4
 Note        :
 @li 重新整理文件结构, 检查并解决代码复用中可能出现的冲突
 ==============================================================================
 Date        : 2017-06-02
 Version     : 0.5
 Note        :
 @li 改进gwac观测计划工作逻辑
 @li 改进转台反馈处理逻辑
 ==============================================================================
 Date        : 2017-06-08
 Version     : 0.6
 Note        :
 @li 分离相机状态与指令
 *---------------------------------------------------------------------------*/

#include "globaldef.h"
#include "GLog.h"
#include "daemon.h"
#include "GeneralControl.h"

GLog gLog; // 日志文件, 全局唯一定义

int main(void) {
	boost::asio::io_service ios;
	boost::asio::signal_set signals(ios, SIGINT, SIGTERM);  // interrupt signal
	signals.async_wait(boost::bind(&boost::asio::io_service::stop, &ios));

	if (!MakeItDaemon(ios)) return 1;
	if (!isProcSingleton(gPIDPath)) {
		gLog.Write("%s is already running or failed to access PID file", DAEMON_NAME);
		return 2;
	}

	gLog.Write("Try to launch %s %s %s as daemon", DAEMON_NAME, DAEMON_VERSION, DAEMON_AUTHORITY);
	// 主程序入口
	GeneralControl gc(ios);
	if (gc.Start()) {
		gLog.Write("Daemon goes running");
		ios.run();
		gc.Stop();
	}
	else {
		gLog.Write(NULL, LOG_FAULT, "Fail to launch %s", DAEMON_NAME);
	}
	gLog.Write("Daemon stopped");

	return 0;
}
