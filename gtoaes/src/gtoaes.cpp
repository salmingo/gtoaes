/*!
 Name        : gtoaes.cpp
 Author      : Xiaomeng Lu
 Copyright   : SVOM Group, NAOC
 Description : GWAC观测控制服务器
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
 ==============================================================================
 Date        : 2017-09-23
 Version     : 0.7
 Note        :
 @li 优化软件
 @li 为调焦器添加服务端口
 @li 改变转台网络处理模式
 ==============================================================================
 Date        : 2018-03
 Version     : 0.8
 Note        :
 @li 优化软件
 @li 集成GWAC与通用望远镜
 ==============================================================================
 Date        : 2020-07
 Version     : 1.0
 Note        :
 @li 优化软件各模块
 @li 集成GWAC与通用望远镜
 */

#include <boost/make_shared.hpp>
#include <boost/asio.hpp>
#include "globaldef.h"
#include "daemon.h"
#include "GLog.h"
#include "GeneralControl.h"
#include "Parameter.h"

GLog _gLog(gLogDir, gLogPrefix);		/// 工作日志

int main(int argc, char **argv) {
	_gLog = boost::make_shared<GLog>();

	if (argc >= 2) {// 处理命令行参数
		if (strcmp(argv[1], "-d") == 0) {
			Parameter param;
			param.Init("gtoaes.xml");
		}
		else printf("Usage: gtoaes <-d>\n");
	}
	else {// 常规工作模式
		boost::asio::io_service ios;
		boost::asio::signal_set signals(ios, SIGINT, SIGTERM);  // interrupt signal
		signals.async_wait(boost::bind(&boost::asio::io_service::stop, &ios));

		if (!MakeItDaemon(ios)) return 1;
		if (!isProcSingleton(gPIDPath)) {
			_gLog.Write("%s is already running or failed to access PID file", DAEMON_NAME);
			return 2;
		}
		_gLog.Write("Try to launch %s %s %s as daemon", DAEMON_NAME, DAEMON_VERSION, DAEMON_AUTHORITY);
		// 主程序入口
		GeneralControl gc;
		if (gc.Start()) {
			_gLog.Write("Daemon goes running");
			ios.run();
			gc.Stop();
			_gLog.Write("Daemon stop running");
		}
		else {
			_gLog.Write(LOG_FAULT, NULL, "Fail to launch %s", DAEMON_NAME);
		}
	}
}
