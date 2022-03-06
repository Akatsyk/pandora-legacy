#include "../Hooked.hpp"
#include "../../Features/Game/Prediction.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../Features/Miscellaneous/Miscellaneous.hpp"
#include <deque>
#include "../../Features/Rage/TickbaseShift.hpp"

#ifndef DEV
#include "../../Utils/InputSys.hpp"
#endif


void FixViewmodel( CUserCmd* cmd, bool restore ) {
	static float cycleBackup = 0.0f;
	static bool weaponAnimation = false;

	C_CSPlayer* player = C_CSPlayer::GetLocalPlayer( );
	auto viewModel = player->m_hViewModel( ).Get( );
	if( viewModel ) {
		if( restore ) {
			weaponAnimation = cmd->weaponselect > 0 || cmd->buttons & ( IN_ATTACK2 | IN_ATTACK );
			cycleBackup = *( float* )( uintptr_t( viewModel ) + 0xA14 );
		}
		else if( weaponAnimation && !g_Vars.globals.FixCycle ) {
			g_Vars.globals.FixCycle = *( float* )( uintptr_t( viewModel ) + 0xA14 ) == 0.0f && cycleBackup > 0.0f;
		}
	}
}

namespace Hooked
{
	void __fastcall RunCommand( void* ecx, void* edx, C_CSPlayer* player, CUserCmd* ucmd, IMoveHelper* moveHelper ) {
		g_Vars.globals.szLastHookCalled = XorStr( "32" );
		C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );
		if( !local || !player || player != local ) {
			oRunCommand( ecx, player, ucmd, moveHelper );
			return;
		}

		if( g_TickbaseController.IsTickcountValid( ucmd->tick_count ) ) {
			ucmd->hasbeenpredicted = true;
			return;
		}

		FixViewmodel( ucmd, true );

		auto backup = g_Vars.sv_show_impacts->GetInt( );
		if( g_Vars.misc.impacts_spoof ) {
			g_Vars.sv_show_impacts->SetValue( 2 );
		}

		static int nTickbaseRecords[ 150 ] = { };
		static bool bInAttackRecords[ 150 ] = { };
		static bool bCanShootRecords[ 150 ] = { };

		nTickbaseRecords[ ucmd->command_number % 150 ] = player->m_nTickBase( );
		bInAttackRecords[ ucmd->command_number % 150 ] = ( ucmd->buttons & ( IN_ATTACK2 | IN_ATTACK ) ) != 0;
		bCanShootRecords[ ucmd->command_number % 150 ] = player->CanShoot(  true );

		auto FixPostponeTime = [ player ] ( int command_number ) {
			auto weapon = ( C_WeaponCSBaseGun* )player->m_hActiveWeapon( ).Get( );
			if( weapon ) {
				auto postpone = FLT_MAX;
				if( weapon->m_iItemDefinitionIndex( ) == WEAPON_REVOLVER ) {
					auto tick_rate = int( 1.0f / Interfaces::m_pGlobalVars->interval_per_tick );
					if( tick_rate >> 1 > 1 ) {
						auto cmd_nr = command_number - 1;
						auto shoot_nr = 0;
						for( int i = 1; i < tick_rate >> 1; ++i ) {
							shoot_nr = cmd_nr;
							if( !bInAttackRecords[ cmd_nr % 150 ] || !bCanShootRecords[ cmd_nr % 150 ] )
								break;

							--cmd_nr;
						}

						if( shoot_nr ) {
							auto tick = 1 - ( signed int )( float )( -0.03348f / Interfaces::m_pGlobalVars->interval_per_tick );
							if( command_number - shoot_nr >= tick )
								postpone = TICKS_TO_TIME( nTickbaseRecords[ ( tick + shoot_nr ) % 150 ] ) + 0.2f;
						}
					}
					weapon->m_flPostponeFireReadyTime( ) = postpone;
				} 
			}
		};

		float flVelocityModifierBackup = local->m_flVelocityModifier( );

		FixPostponeTime( ucmd->command_number );

	//	if( g_Vars.globals.m_bInCreateMove && ucmd->command_number == Interfaces::m_pClientState->m_nLastCommandAck( ) + 1 )
	//		local->m_flVelocityModifier( ) = g_Vars.globals.LastVelocityModifier;

		Engine::Prediction::Instance()->StoreNetvarCompression(ucmd);

		oRunCommand( ecx, player, ucmd, moveHelper );

		Engine::Prediction::Instance()->RestoreNetvarCompression(ucmd);

		FixPostponeTime( ucmd->command_number );

	//	if( !g_Vars.globals.m_bInCreateMove )
	//		local->m_flVelocityModifier( ) = flVelocityModifierBackup;

		if( g_Vars.misc.impacts_spoof ) {
			g_Vars.sv_show_impacts->SetValue( backup );
		}

		FixViewmodel( ucmd, false );

		local->m_vphysicsCollisionState( ) = 0;

		if( !local->IsDead( ) ) {
			auto& prediction = Engine::Prediction::Instance( );
			prediction.OnRunCommand( local, ucmd );
		}
	}
}
