/*!
 * @project gtoaes_hn HN观测系统总控服务器
 * @date 2019-09-20
 * @version 0.1
 * @note
 * 连接转台、相机、观测计划、附属设备(雨量、天窗、调焦)、用户等对象, 完成天文自动观测流程
 * @date 2019-10-22
 * @version 0.2
 * 提高系统健壮性
 */
#include "globaldef.h"
#include "daemon.h"
#include "parameter.h"
#include "GLog.h"
#include "GeneralControl.h"

GLog _gLog;

int main(int argc, char **argv) {
	if (argc >= 2) {// 处理命令行参数
		if (strcmp(argv[1], "-d") == 0) {
			param_config param;
			param.InitFile("gtoaes.xml");
		}
		else _gLog.Write("Usage: gtoaes <-d>\n");
	}
	else {// 常规工作模式
		boost::asio::io_service ios;
		boost::asio::signal_set signals(ios, SIGINT, SIGTERM);  // interrupt signal
		signals.async_wait(boost::bind(&boost::asio::io_service::stop, &ios));

		if (!MakeItDaemon(ios)) return 1;
		if (!isProcSingleton(gPIDPath)) {
			_gLog.Write("%s was already running or failed to access PID file", DAEMON_NAME);
			return 2;
		}

		_gLog.Write("Try to launch %s %s %s as daemon", DAEMON_NAME, DAEMON_VERSION, DAEMON_AUTHORITY);
		// 主程序入口
		boost::shared_ptr<GeneralControl> gc = boost::make_shared<GeneralControl>();
		if (gc->Start()) {
			_gLog.Write("Daemon goes running");
			ios.run();
			gc->Stop();
			_gLog.Write("Daemon stop running");
		}
		else {
			_gLog.Write(LOG_FAULT, NULL, "Fail to launch %s", DAEMON_NAME);
		}
	}

	return 0;
}
