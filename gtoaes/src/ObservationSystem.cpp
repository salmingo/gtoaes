/**
 * @file ObservationSystem.cpp
 * @brief 定义: 观测系统, 集成观测计划、转台、相机和其它附属设备的控制
 * @version 1.0
 * @date 2020-11-08
 * @author 卢晓猛
 */

#include "ObservationSystem.h"

ObservationSystem::ObservationSystem() {

}

ObservationSystem::~ObservationSystem() {

}

bool ObservationSystem::Start() {
	return false;
}

void ObservationSystem::Stop() {

}

int ObservationSystem::ActiveCount() {
	return 0;
}

bool ObservationSystem::IsMatched(const string& gid, const string& uid) {
	return (gid.empty() || (gid == gid_ && (uid.empty() || uid == uid_)));
}

void ObservationSystem::CoupleClient(const TcpCPtr client) {

}

void ObservationSystem::CoupleMount(const TcpCPtr client, int type) {

}

void ObservationSystem::CoupleCamera(const TcpCPtr client, const string& cid) {

}

void ObservationSystem::CoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::CoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleClient(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMount(const TcpCPtr client) {

}

void ObservationSystem::DecoupleMountAnnex(const TcpCPtr client) {

}

void ObservationSystem::DecoupleCameraAnnex(const TcpCPtr client) {

}

void ObservationSystem::interrupt_thread(ThreadPtr& thrd) {
	if (thrd.unique()) {
		thrd->interrupt();
		thrd->join();
		thrd.reset();
	}
}
