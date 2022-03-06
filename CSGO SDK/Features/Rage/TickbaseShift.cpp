#include "TickbaseShift.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../Miscellaneous/Movement.hpp"
#include "../Game/Prediction.hpp"
#include "../../Libraries/minhook-master/include/MinHook.h"
#include "../../Hooking/Hooked.hpp"
#include "../../SDK/Displacement.hpp"
// vader.tech invite https://discord.gg/GV6JNW3nze
void* g_pLocal = nullptr;
TickbaseSystem g_TickbaseController;

TickbaseShift_t::TickbaseShift_t( int _cmdnum, int _tickbase ) :
	cmdnum( _cmdnum ), tickbase( _tickbase )
{
	;
}

#define OFFSET_LASTOUTGOING 0x4CAC
#define OFFSET_CHOKED 0x4CB0
#define OFFSET_TICKBASE 0x3404

bool TickbaseSystem::IsTickcountValid( int nTick ) {
	return nTick >= ( Interfaces::m_pGlobalVars->tickcount + int( 1 / Interfaces::m_pGlobalVars->interval_per_tick ) + g_Vars.sv_max_usercmd_future_ticks->GetInt( ) );
}

//lol you wish this was still here monkey, go paste supremacy dt.

//also hi artens i know you want some p training wheel features!

//artens ip 67.162.21.17

void InvokeRunSimulation( void* this_, float curtime, int cmdnum, CUserCmd* cmd, size_t local ) {
	__asm {
		push local
		push cmd
		push cmdnum

		movss xmm2, curtime
		mov ecx, this_

		call Hooked::RunSimulationDetor.m_pOldFunction
	}
}

void TickbaseSystem::OnRunSimulation( void* this_, int iCommandNumber, CUserCmd* pCmd, size_t local ) {
	g_pLocal = ( void* )local;

	float curtime;
	__asm
	{
		movss curtime, xmm2
	}

	for( int i = 0; i < ( int )g_iTickbaseShifts.size( ); i++ )
	{
		if( ( g_iTickbaseShifts[ i ].cmdnum < iCommandNumber - s_iNetBackup ) ||
			( g_iTickbaseShifts[ i ].cmdnum > iCommandNumber + s_iNetBackup ) )
		{
			g_iTickbaseShifts.erase( g_iTickbaseShifts.begin( ) + i );
			i--;
		}
	}

	int tickbase = -1;
	for( size_t i = 0; i < g_iTickbaseShifts.size( ); i++ )
	{

		//TO:DO this is completely wrong. needs redone.
		const auto& elem = g_iTickbaseShifts[ i ];

		if( elem.cmdnum == iCommandNumber )
		{
			tickbase = elem.tickbase;
			break;
		}
	}

	if( tickbase != -1 && local )
	{
		*( int* )( local + OFFSET_TICKBASE ) = tickbase;
		curtime = tickbase * s_flTickInterval;
	}
	InvokeRunSimulation( this_, curtime, iCommandNumber, pCmd, local );
}

void TickbaseSystem::OnPredictionUpdate( void* prediction, void*, int startframe, bool validframe, int incoming_acknowledged, int outgoing_command ) {
	typedef void( __thiscall* PredictionUpdateFn_t )( void*, int, bool, int, int );
	PredictionUpdateFn_t fn = ( PredictionUpdateFn_t )Hooked::PredictionUpdateDetor.m_pOldFunction;
	fn( prediction, startframe, validframe, incoming_acknowledged, outgoing_command );

	if( s_bInMove && g_pLocal ) {
		*( int* )( ( size_t )g_pLocal + OFFSET_TICKBASE ) = s_iMoveTickBase;
	}

	if( g_pLocal ) {
		for( size_t i = 0; i < g_iTickbaseShifts.size( ); i++ ) {
			const auto& elem = g_iTickbaseShifts[ i ];

			if( elem.cmdnum == ( outgoing_command + 1 ) ) {
				*( int* )( ( size_t )g_pLocal + OFFSET_TICKBASE ) = elem.tickbase;
				break;
			}
		}
	}
}
