#include "../hooked.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/Exploits.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../Features/Rage/ExtendedBactrack.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

struct CIncomingSequence {
	int InSequence;
	int ReliableState;
};

std::vector<CIncomingSequence> IncomingSequences;

void WriteUsercmd( bf_write* buf, CUserCmd* incmd, CUserCmd* outcmd ) {
	__asm
	{
		mov     ecx, buf
		mov     edx, incmd
		push    outcmd
		call    Engine::Displacement.Function.m_WriteUsercmd
		add     esp, 4
	}
}

void BypassChokeLimit( CCLCMsg_Move_t* CL_Move, INetChannel* pNetChan ) {
	// not shifting or dont need do extra fakelag
	if( CL_Move->m_nNewCommands != 15 || Interfaces::m_pClientState->m_nChokedCommands( ) <= 14 )
		return;

	using assign_lol = std::string& ( __thiscall* )( void*, uint8_t*, size_t );
	auto assign_std_autistic_string = ( assign_lol )Engine::Displacement.Function.m_StdStringAssign;

	// rebuild CL_SendMove
	uint8_t data[ 4000 ];
	bf_write buf;
	buf.m_nDataBytes = 4000;
	buf.m_nDataBits = 32000;
	buf.m_pData = data;
	buf.m_iCurBit = false;
	buf.m_bOverflow = false;
	buf.m_bAssertOnOverflow = false;
	buf.m_pDebugName = false;
	int numCmd = Interfaces::m_pClientState->m_nChokedCommands( ) + 1;
	int nextCmdNr = Interfaces::m_pClientState->m_nLastOutgoingCommand( ) + numCmd;
	if( numCmd > 62 )
		numCmd = 62;

	bool bOk = true;

	auto to = nextCmdNr - numCmd + 1;
	auto from = -1;
	if( to <= nextCmdNr ) {
		int newcmdnr = to >= ( nextCmdNr - numCmd + 1 );
		do {
			bOk = bOk && Interfaces::m_pInput->WriteUsercmdDeltaToBuffer( 0, &buf, from, to, to >= newcmdnr );
			from = to++;
		} while( to <= nextCmdNr );
	}

	if( bOk ) {
		/*if( g_TickbaseController.iCommandsToShift > 0 ) {
			CUserCmd from_cmd, to_cmd;
			from_cmd = Interfaces::m_pInput->m_pCommands[ nextCmdNr % MULTIPLAYER_BACKUP ];
			to_cmd = from_cmd;
			to_cmd.tick_count = INT_MAX;

			do {
				if( numCmd >= 62 ) {
					g_TickbaseController.iCommandsToShift = 0;
					break;
				}

				to_cmd.command_number++;
				WriteUsercmd( &buf, &to_cmd, &from_cmd );

				g_TickbaseController.iCommandsToShift--;
				numCmd++;
			} while( g_TickbaseController.iCommandsToShift > 0 );
		}
		else {
			g_TickbaseController.iCommandsToShift = 0;
		}*/

		// bypass choke limit
		CL_Move->m_nNewCommands = numCmd;
		CL_Move->m_nBackupCommands = 0;

		int curbit = ( buf.m_iCurBit + 7 ) >> 3;
		assign_std_autistic_string( CL_Move->m_data, buf.m_pData, curbit );
	}
}

bool __fastcall Hooked::SendNetMsg( INetChannel* pNetChan, void* edx, INetMessage& msg, bool bForceReliable, bool bVoice ) {
	g_Vars.globals.szLastHookCalled = XorStr( "33" );
	if( pNetChan != Interfaces::m_pEngine->GetNetChannelInfo( ) || !g_Vars.globals.HackIsReady )
		return oSendNetMsg( pNetChan, msg, bForceReliable, bVoice );

	if( msg.GetType( ) == 14 ) // Return and don't send messsage if its FileCRCCheck
		return false;

	if( msg.GetGroup( ) == 11 ) {
		BypassChokeLimit( ( CCLCMsg_Move_t* )&msg, pNetChan );
	}
	else if( msg.GetGroup( ) == 9 ) { // group 9 is VoiceData
	// Fixing fakelag with voice
		bVoice = true;
		g_Vars.globals.VoiceEnable = true;
	}
	else
		g_Vars.globals.VoiceEnable = false;

	return oSendNetMsg( pNetChan, msg, bForceReliable, bVoice );
}

#define NET_FRAMES_BACKUP 64 // must be power of 2. 
#define NET_FRAMES_MASK ( NET_FRAMES_BACKUP - 1 )
int __fastcall Hooked::SendDatagram( INetChannel* pNetChan, void* edx, void* buf ) {
	g_Vars.globals.szLastHookCalled = XorStr( "33" );
	if( pNetChan != Interfaces::m_pEngine->GetNetChannelInfo( ) || !g_Vars.globals.HackIsReady || !g_Vars.misc.extended_backtrack || !g_Vars.misc.extended_backtrack_key.enabled )
		return oSendDatagram( pNetChan, buf );

	auto v10 = pNetChan->m_nInSequenceNr;
	auto v16 = pNetChan->m_nInReliableState;
	auto v17 = pNetChan->GetLatency( FLOW_OUTGOING );
	if( v17 < g_Vars.misc.extended_backtrack_time ) {
		auto v13 = pNetChan->m_nInSequenceNr - TIME_TO_TICKS( g_Vars.misc.extended_backtrack_time - v17 );
		pNetChan->m_nInSequenceNr = v13;
		for( auto& seq : IncomingSequences ) {
			if( seq.InSequence != v13 )
				continue;

			pNetChan->m_nInReliableState = seq.ReliableState;
		}
	}

	auto result = oSendDatagram( pNetChan, buf );
	pNetChan->m_nInSequenceNr = v10;
	pNetChan->m_nInReliableState = v16;
	return result;
}

void __fastcall Hooked::ProcessPacket( INetChannel* pNetChan, void* edx, void* packet, bool header ) {
	g_Vars.globals.szLastHookCalled = XorStr( "34" );
	oProcessPacket( pNetChan, packet, header );

	IncomingSequences.push_back( CIncomingSequence{ pNetChan->m_nInSequenceNr, pNetChan->m_nInReliableState } );
	for( auto it = IncomingSequences.begin( ); it != IncomingSequences.end( ); ++it ) {
		auto delta = abs( pNetChan->m_nInSequenceNr - it->InSequence );
		if( delta > 128 ) {
			it = IncomingSequences.erase( it );
		}
	}

	// get this from CL_FireEvents string "Failed to execute event for classId" in engine.dll
	for( CEventInfo* it{ Interfaces::m_pClientState->m_pEvents( ) }; it != nullptr; it = it->m_next ) {
		if( !it->m_class_id )
			continue;

		// set all delays to instant.
		it->m_fire_delay = 0.f;
	}

	// game events are actually fired in OnRenderStart which is WAY later after they are received
	// effective delay by lerp time, now we call them right after theyre received (all receive proxies are invoked without delay).
	Interfaces::m_pEngine->FireEvents( );
}

void __fastcall Hooked::Shutdown( INetChannel* pNetChan, void* EDX, const char* reason ) {
	g_Vars.globals.szLastHookCalled = XorStr( "35" );
	return oShutdown( pNetChan, reason );
}

bool __fastcall Hooked::LooseFileAllowed( void* ecx, void* edx ) {
	return true;
}

void __fastcall Hooked::CheckFileCRCsWithServer( void* ecx, void* edx ) {
	return;
}
