#include "Prediction.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../source.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/displacement.hpp"
#include "../Rage/TickbaseShift.hpp"
#include "../../Hooking/Hooked.hpp"
#include <deque>

namespace Engine
{
	// C_BasePlayer::PhysicsSimulate & CPrediction::RunCommand rebuild
	void Prediction::Begin( Encrypted_t<CUserCmd> _cmd, bool* send_packet, int command_number ) {
		predictionData->m_pCmd = _cmd;
		predictionData->m_RestoreData.is_filled = false;
		predictionData->m_pSendPacket = send_packet;

		if( !predictionData->m_pCmd.IsValid( ) )
			return;

		predictionData->m_pPlayer = C_CSPlayer::GetLocalPlayer( );

		if( !predictionData->m_pPlayer || predictionData->m_pPlayer->IsDead( ) )
			return;

		predictionData->m_pWeapon = ( C_WeaponCSBaseGun* )predictionData->m_pPlayer->m_hActiveWeapon( ).Get( );

		if( !predictionData->m_pWeapon )
			return;

		predictionData->m_pWeaponInfo = predictionData->m_pWeapon->GetCSWeaponData( );

		if( !predictionData->m_pWeaponInfo.IsValid( ) )
			return;

		m_bInPrediction = true;

		Engine::Prediction::Instance( )->RunGamePrediction( );

		// correct tickbase.
		if( !m_pLastCmd || m_pLastCmd->hasbeenpredicted )
			m_iSeqDiff = command_number - predictionData->m_pPlayer->m_nTickBase( );

		int nTickBaseBackup = predictionData->m_pPlayer->m_nTickBase( );

		predictionData->m_nTickBase = MAX( predictionData->m_pPlayer->m_nTickBase( ), command_number - m_iSeqDiff );

		m_pLastCmd = predictionData->m_pCmd.Xor( );

		predictionData->m_fFlags = predictionData->m_pPlayer->m_fFlags( );
		predictionData->m_vecVelocity = predictionData->m_pPlayer->m_vecVelocity( );

		predictionData->m_flCurrentTime = Interfaces::m_pGlobalVars->curtime;
		predictionData->m_flFrameTime = Interfaces::m_pGlobalVars->frametime;

		if( Engine::Displacement.Function.m_MD5PseudoRandom ) {
			predictionData->m_pCmd->random_seed = ( ( uint32_t( __thiscall* )( uint32_t ) )Displacement.Function.m_MD5PseudoRandom )( predictionData->m_pCmd->command_number ) & 0x7fffffff;
		}

		bool bEnginePaused = *( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 10 );

		// StartCommand rebuild
		predictionData->m_pPlayer->SetCurrentCommand( predictionData->m_pCmd.Xor( ) );
		C_BaseEntity::SetPredictionRandomSeed( predictionData->m_pCmd.Xor( ) );
		C_BaseEntity::SetPredictionPlayer( predictionData->m_pPlayer );

		*reinterpret_cast< CUserCmd** >( uint32_t( predictionData->m_pPlayer ) + 0x3314 ) = predictionData->m_pCmd.Xor( ); // m_pCurrentCommand
		*reinterpret_cast< CUserCmd** >( uint32_t( predictionData->m_pPlayer ) + 0x326C ) = predictionData->m_pCmd.Xor( ); // unk01

		Interfaces::m_pGlobalVars->curtime = TICKS_TO_TIME( predictionData->m_pPlayer->m_nTickBase( ) );
		Interfaces::m_pGlobalVars->frametime = Interfaces::m_pGlobalVars->interval_per_tick;

		Interfaces::m_pGameMovement->StartTrackPredictionErrors( predictionData->m_pPlayer );

		// Setup input.
		Interfaces::m_pPrediction->SetupMove( predictionData->m_pPlayer, predictionData->m_pCmd.Xor( ), Interfaces::m_pMoveHelper.Xor( ), &predictionData->m_MoveData );

		predictionData->m_RestoreData.m_MoveData = predictionData->m_MoveData;
		predictionData->m_RestoreData.is_filled = true;
		predictionData->m_RestoreData.Setup( predictionData->m_pPlayer );

		predictionData->m_bFirstTimePrediction = *( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 24 );
		predictionData->m_bInPrediction = *( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 8 );

		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 8 ) = true;
		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 24 ) = false;

		Interfaces::m_pMoveHelper->SetHost( predictionData->m_pPlayer );

		Interfaces::m_pGameMovement->ProcessMovement( predictionData->m_pPlayer, &predictionData->m_MoveData );

		Interfaces::m_pPrediction->FinishMove( predictionData->m_pPlayer, predictionData->m_pCmd.Xor( ), &predictionData->m_MoveData );

		predictionData->m_nTickBase = nTickBaseBackup;

		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 24 ) = predictionData->m_bFirstTimePrediction;
		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 8 ) = predictionData->m_bInPrediction;

		predictionData->m_pWeapon->UpdateAccuracyPenalty( );

		m_flSpread = predictionData->m_pWeapon->GetSpread( );
		m_flInaccuracy = predictionData->m_pWeapon->GetInaccuracy( );
		m_flWeaponRange = predictionData->m_pWeaponInfo->m_flWeaponRange;
	}

	void Prediction::Repredict( ) {
		Engine::Prediction::Instance( )->RunGamePrediction( );

		predictionData->m_RestoreData.Apply( predictionData->m_pPlayer );

		// set move data members that getting copied from CUserCmd
		// rebuilded from CPrediction::SetupMove
		predictionData->m_MoveData = predictionData->m_RestoreData.m_MoveData;
		predictionData->m_MoveData.m_flForwardMove = predictionData->m_pCmd->forwardmove;
		predictionData->m_MoveData.m_flSideMove = predictionData->m_pCmd->sidemove;
		predictionData->m_MoveData.m_flUpMove = predictionData->m_pCmd->upmove;
		predictionData->m_MoveData.m_nButtons = predictionData->m_pCmd->buttons;
		predictionData->m_MoveData.m_vecOldAngles.y = predictionData->m_pCmd->viewangles.pitch;
		predictionData->m_MoveData.m_vecOldAngles.z = predictionData->m_pCmd->viewangles.yaw;
		predictionData->m_MoveData.m_outStepHeight = predictionData->m_pCmd->viewangles.roll;
		predictionData->m_MoveData.m_vecViewAngles.pitch = predictionData->m_pCmd->viewangles.pitch;
		predictionData->m_MoveData.m_vecViewAngles.yaw = predictionData->m_pCmd->viewangles.yaw;
		predictionData->m_MoveData.m_vecViewAngles.roll = predictionData->m_pCmd->viewangles.roll;
		predictionData->m_MoveData.m_nImpulseCommand = predictionData->m_pCmd->impulse;

		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 8 ) = true;
		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 24 ) = false;

		Interfaces::m_pMoveHelper->SetHost( predictionData->m_pPlayer );

		Interfaces::m_pGameMovement->ProcessMovement( predictionData->m_pPlayer, &predictionData->m_MoveData );

		Interfaces::m_pPrediction->FinishMove( predictionData->m_pPlayer, predictionData->m_pCmd.Xor( ), &predictionData->m_MoveData );

		Interfaces::m_pMoveHelper->ProcessImpacts( );

		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 24 ) = predictionData->m_bFirstTimePrediction;
		*( bool* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 8 ) = predictionData->m_bInPrediction;

		predictionData->m_pWeapon->UpdateAccuracyPenalty( );

		m_flSpread = predictionData->m_pWeapon->GetSpread( );
		m_flInaccuracy = predictionData->m_pWeapon->GetInaccuracy( );
		m_flWeaponRange = predictionData->m_pWeaponInfo->m_flWeaponRange;
	}

	void Prediction::PostEntityThink( C_CSPlayer* player ) {
		static auto PostThinkVPhysics = reinterpret_cast< bool( __thiscall* )( C_BaseEntity* ) >( Memory::Scan( "client.dll", "55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 8B D9 56 57 83 BB ? ? ? ? ? 75 50 8B 0D" ) );
		static auto SimulatePlayerSimulatedEntities = reinterpret_cast< void( __thiscall* )( C_BaseEntity* ) >( Memory::Scan( "client.dll", "56 8B F1 57 8B BE ? ? ? ? 83 EF 01 78 72 90 8B 86" ) );

		if( player && !player->IsDead( ) ) {
			using UpdateCollisionBoundsFn = void( __thiscall* )( void* );
			Memory::VCall<UpdateCollisionBoundsFn>( player, 329 )( player );

			if( player->m_fFlags( ) & FL_ONGROUND )
				*reinterpret_cast< uintptr_t* >( uintptr_t( player ) + 0x3004 ) = 0;

			if( *reinterpret_cast< int* >( uintptr_t( player ) + 0x28AC ) == -1 ) {
				using SetSequenceFn = void( __thiscall* )( void*, int );
				Memory::VCall<SetSequenceFn>( player, 213 )( player, 0 );
			}

			using StudioFrameAdvanceFn = void( __thiscall* )( void* );
			Memory::VCall<StudioFrameAdvanceFn>( player, 214 )( player );

			PostThinkVPhysics( player );
		}
		SimulatePlayerSimulatedEntities( player );
	}

	void Prediction::End( ) {
		if( !predictionData->m_pCmd.IsValid( ) || !predictionData->m_pPlayer || !predictionData->m_pWeapon )
			return;

		Interfaces::m_pGlobalVars->curtime = predictionData->m_flCurrentTime;
		Interfaces::m_pGlobalVars->frametime = predictionData->m_flFrameTime;

		//CPrediction::RunPostThink rebuild
		PostEntityThink( predictionData->m_pPlayer );

		Interfaces::m_pGameMovement->FinishTrackPredictionErrors( predictionData->m_pPlayer );

		auto& correct = predictionData->m_CorrectionData.emplace_front( );
		correct.command_nr = predictionData->m_pCmd->command_number;
		correct.tickbase = predictionData->m_pPlayer->m_nTickBase( );
		correct.tickbase_shift = 0;
		correct.chokedcommands = Interfaces::m_pClientState->m_nChokedCommands( ) + 1;
		correct.tickcount = Interfaces::m_pGlobalVars->tickcount;

		auto& out = predictionData->m_OutgoingCommands.emplace_back( );
		out.is_outgoing = *predictionData->m_pSendPacket != false;
		out.command_nr = predictionData->m_pCmd->command_number;
		out.is_used = false;
		out.prev_command_nr = 0;

		if( *predictionData->m_pSendPacket ) {
			predictionData->m_ChokedNr.clear( );
		}
		else {
			predictionData->m_ChokedNr.push_back( predictionData->m_pCmd->command_number );
		}

		while( predictionData->m_CorrectionData.size( ) > int( 2.0f / Interfaces::m_pGlobalVars->interval_per_tick ) ) {
			predictionData->m_CorrectionData.pop_back( );
		}

		Interfaces::m_pMoveHelper->SetHost( nullptr );

		// Finish command
		predictionData->m_pPlayer->SetCurrentCommand( nullptr );
		C_BaseEntity::SetPredictionRandomSeed( nullptr );
		C_BaseEntity::SetPredictionPlayer( nullptr );

		Interfaces::m_pGameMovement->Reset( );

		m_bInPrediction = false;
	}

	void Prediction::Invalidate( ) {
		predictionData->m_pCmd = nullptr;
		predictionData->m_pPlayer = nullptr;
		predictionData->m_pWeapon = nullptr;
		predictionData->m_pSendPacket = nullptr;
		predictionData->m_OutgoingCommands.clear( );

		predictionData->m_RestoreData.Reset( );
		predictionData->m_RestoreData.is_filled = false;
	}

	void Prediction::RunGamePrediction( ) {
		// force game to repredict data
		//if( g_Vars.globals.LastVelocityModifier < 1.0f ) {
		//	// https://github.com/pmrowla/hl2sdk-csgo/blob/49e950f3eb820d88825f75e40f56b3e64790920a/game/client/prediction.cpp#L1533
		//	*( uint8_t* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 0x24 ) = 1; // m_bPreviousAckHadErrors 
		//	*( uint32_t* )( uintptr_t( Interfaces::m_pPrediction.Xor( ) ) + 0x1C ) = 0; // m_nCommandsPredicted 
		//}

		if( Interfaces::m_pClientState->m_nDeltaTick( ) > 0 ) {
			Interfaces::m_pPrediction->Update( Interfaces::m_pClientState->m_nDeltaTick( ), Interfaces::m_pClientState->m_nDeltaTick( ) > 0,
				Interfaces::m_pClientState->m_nLastCommandAck( ),
				Interfaces::m_pClientState->m_nLastOutgoingCommand( ) + Interfaces::m_pClientState->m_nChokedCommands( ) );
		}
	}

	int Prediction::GetFlags( ) {
		return predictionData->m_fFlags;
	}

	Vector Prediction::GetVelocity( ) {
		return predictionData->m_vecVelocity;
	}

	Encrypted_t<CUserCmd> Prediction::GetCmd( ) {
		return predictionData->m_pCmd;
	}

	float Prediction::GetFrametime( ) {
		return predictionData->m_flFrameTime;
	}

	float Prediction::GetCurtime( ) {
		return predictionData->m_flCurrentTime;
	}

	float Prediction::GetSpread( ) {
		return m_flSpread;
	}

	float Prediction::GetInaccuracy( ) {
		return m_flInaccuracy;
	}

	float Prediction::GetWeaponRange( ) {
		return m_flWeaponRange;
	}

	// Netvar compression fix
	void Prediction::OnFrameStageNotify( ClientFrameStage_t stage ) {
		if( stage != FRAME_NET_UPDATE_POSTDATAUPDATE_START )
			return;

		auto local = C_CSPlayer::GetLocalPlayer( );

		if( !local )
			return;

		auto slot = local->m_nTickBase( );
		auto data = &predictionData->m_Data[ slot % 150 ];

		if( !data )
			return;

		if( data->m_nTickbase != slot )
			return;

		if( !m_bGetNetvarCompressionData )
			return;

		auto GetDelta = [ ] ( const QAngle& a, const QAngle& b ) {
			auto delta = a - b;
			return std::sqrt( delta.x * delta.x + delta.y * delta.y + delta.z * delta.z );
		};

		if( GetDelta( local->m_aimPunchAngle( ), data->m_aimPunchAngle ) < 0.03125f ) {
			local->m_aimPunchAngle( ) = data->m_aimPunchAngle;
		}

		if( GetDelta( local->m_aimPunchAngleVel( ), data->m_aimPunchAngleVel ) < 0.03125f ) {
			local->m_aimPunchAngleVel( ) = data->m_aimPunchAngleVel;
		}

		if( local->m_vecViewOffset( ).Distance( data->m_vecViewOffset ) < 0.03125f ) {
			local->m_vecViewOffset( ) = data->m_vecViewOffset;
		}

		//if( fabs( local->m_flVelocityModifier( ) - data->m_flVelocityModifier ) < 0.03125f ) {
		//	local->m_flVelocityModifier( ) = data->m_flVelocityModifier;
		//}
	}

	void Prediction::OnRunCommand( C_CSPlayer* player, CUserCmd* cmd ) {
		if( !player )
			return;

		auto local = C_CSPlayer::GetLocalPlayer( );

		if( !local || local != player || !predictionData.IsValid( ) )
			return;

		if( cmd->hasbeenpredicted ) {
			for( auto& data : predictionData->m_Data ) {
				data.m_nTickbase--;
			}

			return;
		}
	}

	bool Prediction::ShouldSimulate( int command_number ) {
		return true;
	}

	void Prediction::PacketCorrection( uintptr_t cl_state ) {

		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) ) {
			predictionData->m_CorrectionData.clear( );
			return;
		}

		// Did we get any messages this tick (i.e., did we call PreEntityPacketReceived)?
		if( *( int* )( uintptr_t( cl_state ) + 0x164 ) == *( int* )( uintptr_t( cl_state ) + 0x16C ) ) {
			auto ack_cmd = *( int* )( uintptr_t( cl_state ) + 0x4D2C );
			auto correct = std::find_if( predictionData->m_CorrectionData.begin( ), predictionData->m_CorrectionData.end( ), [ ack_cmd ] ( const CorrectionData& a ) {
				return a.command_nr == ack_cmd;
			} );

			if( correct != predictionData->m_CorrectionData.begin( ) && correct != predictionData->m_CorrectionData.end( ) ) {
				//if( g_Vars.globals.LastVelocityModifier > ( local->m_flVelocityModifier( ) + ( TIME_TO_TICKS( Interfaces::m_pClientState->m_nChokedCommands( ) ) * 0.4f ) ) ) {
				//	auto weapon = ( C_WeaponCSBaseGun* )local->m_hActiveWeapon( ).Get( );
				//	if( !weapon || ( weapon && weapon->m_iItemDefinitionIndex( ) != WEAPON_REVOLVER && weapon->GetCSWeaponData( )->m_iWeaponType != WEAPONTYPE_GRENADE ) ) {
				//		for( auto nr : predictionData->m_ChokedNr ) {
				//			auto cmd = &Interfaces::m_pInput->m_pCommands[ nr % 150 ];
				//			auto verified = &Interfaces::m_pInput->m_pVerifiedCommands[ nr % 150 ];
				//
				//			if( cmd->buttons & ( IN_ATTACK2 | IN_ATTACK ) ) {
				//				cmd->buttons &= ~IN_ATTACK;
				//				verified->m_cmd = *cmd;
				//				verified->m_crc = cmd->GetChecksum( );
				//			}
				//		}
				//	}
				//}
				//
				//g_Vars.globals.LastVelocityModifier = local->m_flVelocityModifier( );
			}
		}
	}

	void Prediction::KeepCommunication( bool* bSendPacket, int command_number ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		auto chan = Interfaces::m_pEngine->GetNetChannelInfo( );
		if( !chan ) {
			g_Vars.globals.cmds.clear( );
			return;
		}

		//if( chan->m_nChokedPackets > 0 && !( chan->m_nChokedPackets % 4 ) ) {
		//	//const auto choked = chan->m_nChokedPackets;
		//	//chan->m_nChokedPackets = 0;
		//	//chan->SendDatagram( );
		//	//--chan->m_nOutSequenceNr;
		//	//chan->m_nChokedPackets = choked;
		//}
		//else { 
		//	g_Vars.globals.cmds.push_back( command_number );
		//}
	}

	void Prediction::StoreNetvarCompression( CUserCmd* cmd )
	{
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local )
			return;

		// collect data for netvar compression fix
		auto slot = cmd->command_number;
		auto data = &predictionData->m_Data[ slot % 150 ];

		data->m_nCommandNumber = cmd->command_number;
		data->m_aimPunchAngle = local->m_aimPunchAngle( );
		data->m_aimPunchAngleVel = local->m_aimPunchAngleVel( );
		data->m_vecViewOffset = local->m_vecViewOffset( );
		//data->m_flVelocityModifier = local->m_flVelocityModifier( );
		m_bGetNetvarCompressionData = true;
	}

	void Prediction::RestoreNetvarCompression( CUserCmd* cmd )
	{
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local )
			return;

		// restore data for netvar compression fix
		auto slot = cmd->command_number;
		auto data = predictionData->m_Data[ slot % 150 ];
		if( m_bGetNetvarCompressionData ) {
			cmd->command_number = data.m_nCommandNumber;
			local->m_aimPunchAngle( ) = data.m_aimPunchAngle;
			local->m_aimPunchAngleVel( ) = data.m_aimPunchAngleVel;
			local->m_vecViewOffset( ) = data.m_vecViewOffset;
			//local->m_flVelocityModifier( ) = data.m_flVelocityModifier;

			m_bGetNetvarCompressionData = false;
		}
	}

	static PredictionData _xoreddata;
	Prediction::Prediction( ) : predictionData( &_xoreddata ) {
	}

	Prediction::~Prediction( ) {
	}

	void RestoreData::Setup( C_CSPlayer* player ) {
		this->m_aimPunchAngle = player->m_aimPunchAngle( );
		this->m_aimPunchAngleVel = player->m_aimPunchAngleVel( );
		this->m_viewPunchAngle = player->m_viewPunchAngle( );

		this->m_vecViewOffset = player->m_vecViewOffset( );
		this->m_vecBaseVelocity = player->m_vecBaseVelocity( );
		this->m_vecVelocity = player->m_vecVelocity( );
		this->m_vecOrigin = player->m_vecOrigin( );

		this->m_flFallVelocity = player->m_flFallVelocity( );
		//this->m_flVelocityModifier = player->m_flVelocityModifier( );
		this->m_flDuckAmount = player->m_flDuckAmount( );
		this->m_flDuckSpeed = player->m_flDuckSpeed( );

		static int playerSurfaceFrictionOffset = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_surfaceFriction" ) );
		float playerSurfaceFriction = *( float* )( uintptr_t( player ) + playerSurfaceFrictionOffset );

		this->m_surfaceFriction = playerSurfaceFriction;

		static int hGroundEntity = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_hGroundEntity" ) );
		int playerhGroundEntity = *( int* )( uintptr_t( player ) + hGroundEntity );

		this->m_hGroundEntity = playerhGroundEntity;
		this->m_nMoveType = player->m_MoveType( );
		this->m_nFlags = player->m_fFlags( );
		//this->m_nTickBase = player->m_nTickBase( );

		auto weapon = ( C_WeaponCSBaseGun* )player->m_hActiveWeapon( ).Get( );
		if( weapon ) {
			static int fAccuracyPenalty = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_fAccuracyPenalty" ) );
			float playerfAccuracyPenalty = *( float* )( uintptr_t( player ) + fAccuracyPenalty );

			this->m_fAccuracyPenalty = playerfAccuracyPenalty;
			this->m_flRecoilIndex = weapon->m_flRecoilIndex( );
		}

		this->is_filled = true;
	}

	void RestoreData::Apply( C_CSPlayer* player ) {
		if( !this->is_filled )
			return;

		player->m_aimPunchAngle( ) = this->m_aimPunchAngle;
		player->m_aimPunchAngleVel( ) = this->m_aimPunchAngleVel;
		player->m_viewPunchAngle( ) = this->m_viewPunchAngle;

		player->m_vecViewOffset( ) = this->m_vecViewOffset;
		player->m_vecBaseVelocity( ) = this->m_vecBaseVelocity;
		player->m_vecVelocity( ) = this->m_vecVelocity;
		player->m_vecOrigin( ) = this->m_vecOrigin;

		player->m_flFallVelocity( ) = this->m_flFallVelocity;
		//player->m_flVelocityModifier( ) = this->m_flVelocityModifier;
		player->m_flDuckAmount( ) = this->m_flDuckAmount;
		player->m_flDuckSpeed( ) = this->m_flDuckSpeed;

		static int playerSurfaceFrictionOffset = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_surfaceFriction" ) );
		float playerSurfaceFriction = *( float* )( uintptr_t( player ) + playerSurfaceFrictionOffset );
		*( float* )( uintptr_t( player ) + playerSurfaceFrictionOffset ) = this->m_surfaceFriction;

		static int hGroundEntity = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_hGroundEntity" ) );
		*( int* )( uintptr_t( player ) + hGroundEntity ) = this->m_hGroundEntity;

		player->m_MoveType( ) = this->m_nMoveType;
		player->m_fFlags( ) = this->m_nFlags;
		//player->m_nTickBase( ) = this->m_nTickBase;

		auto weapon = ( C_WeaponCSBaseGun* )player->m_hActiveWeapon( ).Get( );
		if( weapon ) {
			static int fAccuracyPenalty = Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_fAccuracyPenalty" ) );

			*( float* )( uintptr_t( player ) + fAccuracyPenalty ) = this->m_fAccuracyPenalty;
			weapon->m_flRecoilIndex( ) = this->m_flRecoilIndex;
		}
	}
}