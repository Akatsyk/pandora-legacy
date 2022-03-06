#pragma once
#include "../../SDK/sdk.hpp"
// vader.tech invite https://discord.gg/GV6JNW3nze
struct TickbaseShift_t {
	TickbaseShift_t( ) = delete;
	TickbaseShift_t( int _cmdnum, int _tickbase );
	~TickbaseShift_t( ) = default;

	int cmdnum, tickbase;
};

class TickbaseSystem {
public:
	size_t s_nSpeed = 14;

	size_t s_nTickRate = 64;
	float s_flTickInterval = 1.f / ( float )s_nTickRate;

	// an unreplicated convar: sv_clockcorrection_msecs
	float s_flClockCorrectionSeconds = 30.f / 1000.f;

	int s_iClockCorrectionTicks = ( int )( s_flClockCorrectionSeconds * s_flTickInterval + 0.5f );
	int s_iNetBackup = 64;

	bool s_bFreshFrame = false;
	bool s_bAckedBuild = true;

	bool m_bSupressRecharge = false;

	float s_flTimeRequired = 0.4f;
	size_t s_nTicksRequired = ( int )( s_flTimeRequired / s_flTickInterval + 0.5f );
	size_t s_nTicksDelay = 32u;

	bool s_bInMove = false;
	int s_iMoveTickBase = 0;
	size_t s_nTicksSinceUse = 0u;
	size_t s_nTicksSinceStarted = 0u;

	int s_iServerIdealTick = 0;
	bool s_bBuilding = false;

	size_t s_nExtraProcessingTicks = 0;
	std::vector<TickbaseShift_t> g_iTickbaseShifts;

	bool IsTickcountValid( int nTick );

	void OnRunSimulation( void* this_, int iCommandNumber, CUserCmd* pCmd, size_t local );
	void OnPredictionUpdate( void* prediction, void*, int startframe, bool validframe, int incoming_acknowledged, int outgoing_command );
};

extern TickbaseSystem g_TickbaseController;