#pragma once
#include "../Hooked.hpp"
#include "../../source.hpp"
#include "../../Features/Game/Prediction.hpp"

#include "../../SDK/Classes/Player.hpp"

void __fastcall Hooked::PacketStart( void* ecx, void*, int incoming_sequence, int outgoing_acknowledged ) {
	g_Vars.globals.szLastHookCalled = XorStr( "19" );

	C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );
	if( !local || local->IsDead( ) || !Interfaces::m_pEngine->IsInGame( ) || g_Vars.globals.cmds.empty() )
		return oPacketStart( ecx, incoming_sequence, outgoing_acknowledged );

	for( auto it = g_Vars.globals.cmds.begin( ); it != g_Vars.globals.cmds.end( ); ++it )	{
		if( *it == outgoing_acknowledged )		{
			g_Vars.globals.cmds.erase( it );
			return oPacketStart( ecx, incoming_sequence, outgoing_acknowledged );
		}
	}
}

void __fastcall Hooked::PacketEnd( void* ecx, void* ) {
	g_Vars.globals.szLastHookCalled = XorStr( "20" );
	Engine::Prediction::Instance( )->PacketCorrection( reinterpret_cast< uintptr_t >( ecx ) );
	oPacketEnd( ecx );
}

bool __fastcall Hooked::ProcessTempEntities( void* ecx, void*, void* msg ) {
	auto backup = Interfaces::m_pClientState->m_nMaxClients( );

	Interfaces::m_pClientState->m_nMaxClients( ) = 1;
	auto ret = oProcessTempEntities( ecx, msg );
	Interfaces::m_pClientState->m_nMaxClients( ) = backup;

	Hooked::CL_FireEvents( );

	return ret;
}