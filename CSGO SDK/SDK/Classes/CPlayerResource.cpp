#include "CPlayerResource.hpp"

int CSPlayerResource::GetPlayerPing(int idx) {
	static auto m_iPing = Engine::PropManager::Instance()->GetOffset(XorStr("DT_PlayerResource"), XorStr("m_iPing"));
	return *(int*)((uintptr_t)this + m_iPing + idx * 4);
}

int CSPlayerResource::GetPlayerAssists(int idx) {
	static auto m_iAssists = Engine::PropManager::Instance()->GetOffset(XorStr("DT_PlayerResource"), XorStr("m_iAssists"));
	return *(int*)((uintptr_t)this + m_iAssists + idx * 4);
}

int CSPlayerResource::GetPlayerKills(int idx) {
	static auto m_iKills = Engine::PropManager::Instance()->GetOffset(XorStr("DT_PlayerResource"), XorStr("m_iKills"));
	return *(int*)((uintptr_t)this + m_iKills + idx * 4);
}

int CSPlayerResource::GetPlayerDeaths(int idx) {
	static auto m_iDeaths = Engine::PropManager::Instance()->GetOffset(XorStr("DT_PlayerResource"), XorStr("m_iDeaths"));
	return *(int*)((uintptr_t)this + m_iDeaths + idx * 4);
}