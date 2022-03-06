#include "AntiAim.hpp"
#include "../../SDK/CVariables.hpp"
#include "../Miscellaneous/Movement.hpp"
#include "../../source.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "LagCompensation.hpp"
#include "Autowall.h"
#include "../Game/SimulationContext.hpp"
#include "../../SDK/displacement.hpp"
#include "../../Renderer/Render.hpp"
#include "TickbaseShift.hpp"
#include "../Visuals/ESP.hpp"
#include <random>

namespace Interfaces
{
	class C_AntiAimbot : public AntiAimbot {
	public:
		void Main( bool* bSendPacket, bool* bFinalPacket, Encrypted_t<CUserCmd> cmd, bool ragebot ) override;
		void PrePrediction( bool* bSendPacket, Encrypted_t<CUserCmd> cmd ) override;
		void ImposterBreaker( bool* bSendPacket, Encrypted_t<CUserCmd> cmd ) override;
	private:
		virtual float GetAntiAimX( Encrypted_t<CVariables::ANTIAIM_STATE> settings );
		virtual float GetAntiAimY( Encrypted_t<CVariables::ANTIAIM_STATE> settings, Encrypted_t<CUserCmd> cmd );

		virtual void Distort( Encrypted_t<CUserCmd> cmd );

		enum class Directions : int {
			YAW_RIGHT = -1,
			YAW_BACK,
			YAW_LEFT,
			YAW_NONE,
		};
		virtual Directions HandleDirection( Encrypted_t<CUserCmd> cmd );

		virtual bool IsEnabled( Encrypted_t<CUserCmd> cmd, Encrypted_t<CVariables::ANTIAIM_STATE> settings );

		bool m_bNegate = false;
		float m_flLowerBodyUpdateTime = 0.f;
		float m_flLowerBodyUpdateYaw = FLT_MAX;

		float  m_auto;
		float  m_auto_dist;
		float  m_auto_last;
		float  m_view;
		float  m_auto_time;
	};

	bool C_AntiAimbot::IsEnabled( Encrypted_t<CUserCmd> cmd, Encrypted_t<CVariables::ANTIAIM_STATE> settings ) {
		C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer( );
		if ( !LocalPlayer || LocalPlayer->IsDead( ) )
			return false;

		if ( !( g_Vars.antiaim.bomb_activity && g_Vars.globals.BobmActivityIndex == LocalPlayer->EntIndex( ) ) || !g_Vars.antiaim.bomb_activity )
			if ( ( cmd->buttons & IN_USE ) && ( !settings->desync_e_hold || LocalPlayer->m_bIsDefusing( ) ) )
				return false;

		if ( LocalPlayer->m_MoveType( ) == MOVETYPE_NOCLIP )
			return false;

		static auto g_GameRules = *( uintptr_t** )( Engine::Displacement.Data.m_GameRules );
		if ( g_GameRules && *( bool* )( *( uintptr_t* )g_GameRules + 0x20 ) || ( LocalPlayer->m_fFlags( ) & ( 1 << 6 ) ) )
			return false;

		C_WeaponCSBaseGun* Weapon = ( C_WeaponCSBaseGun* )LocalPlayer->m_hActiveWeapon( ).Get( );

		if ( !Weapon )
			return false;

		auto WeaponInfo = Weapon->GetCSWeaponData( );
		if ( !WeaponInfo.IsValid( ) )
			return false;

		if ( WeaponInfo->m_iWeaponType == WEAPONTYPE_GRENADE ) {
			if ( !Weapon->m_bPinPulled( ) || ( cmd->buttons & ( IN_ATTACK | IN_ATTACK2 ) ) ) {
				float throwTime = Weapon->m_fThrowTime( );
				if ( throwTime > 0.f )
					return false;
			}
		}
		else {
			if ( ( WeaponInfo->m_iWeaponType == WEAPONTYPE_KNIFE && cmd->buttons & ( IN_ATTACK | IN_ATTACK2 ) ) || cmd->buttons & IN_ATTACK ) {
				if ( LocalPlayer->CanShoot( ) )
					return false;
			}
		}

		if ( LocalPlayer->m_MoveType( ) == MOVETYPE_LADDER )
			return false;

		return true;
	}

	Encrypted_t<AntiAimbot> AntiAimbot::Get( ) {
		static C_AntiAimbot instance;
		return &instance;
	}

	std::random_device random;
	std::mt19937 generator( random( ) );
	void C_AntiAimbot::Main( bool* bSendPacket, bool* bFinalPacket, Encrypted_t<CUserCmd> cmd, bool ragebot ) {
		C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer( );

		if ( !LocalPlayer || LocalPlayer->IsDead( ) )
			return;

		auto animState = LocalPlayer->m_PlayerAnimState( );
		if ( !animState )
			return;

		if ( !g_Vars.antiaim.enabled )
			return;

		Encrypted_t<CVariables::ANTIAIM_STATE> settings( &g_Vars.antiaim_stand );

		C_WeaponCSBaseGun* Weapon = ( C_WeaponCSBaseGun* )LocalPlayer->m_hActiveWeapon( ).Get( );

		if ( !Weapon )
			return;

		auto WeaponInfo = Weapon->GetCSWeaponData( );
		if ( !WeaponInfo.IsValid( ) )
			return;

		if ( !IsEnabled( cmd, settings ) )
			return;

		if ( LocalPlayer->m_MoveType( ) == MOVETYPE_LADDER ) {
			auto eye_pos = LocalPlayer->GetEyePosition( );

			CTraceFilterWorldAndPropsOnly filter;
			CGameTrace tr;
			Ray_t ray;
			float angle = 0.0f;
			while ( true ) {
				float cosa, sina;
				DirectX::XMScalarSinCos( &cosa, &sina, angle );

				Vector pos;
				pos.x = ( cosa * 32.0f ) + eye_pos.x;
				pos.y = ( sina * 32.0f ) + eye_pos.y;
				pos.z = eye_pos.z;

				ray.Init( eye_pos, pos,
					Vector( -1.0f, -1.0f, -4.0f ),
					Vector( 1.0f, 1.0f, 4.0f ) );
				Interfaces::m_pEngineTrace->TraceRay( ray, MASK_SOLID, &filter, &tr );
				if ( tr.fraction < 1.0f )
					break;

				angle += DirectX::XM_PIDIV2;
				if ( angle >= DirectX::XM_2PI ) {
					return;
				}
			}

			float v23 = atan2( tr.plane.normal.x, std::fabsf( tr.plane.normal.y ) );
			float v24 = RAD2DEG( v23 ) + 90.0f;
			cmd->viewangles.pitch = 89.0f;
			if ( v24 <= 180.0f ) {
				if ( v24 < -180.0f ) {
					v24 = v24 + 360.0f;
				}
				cmd->viewangles.yaw = v24;
			}
			else {
				cmd->viewangles.yaw = v24 - 360.0f;
			}

			if ( cmd->buttons & IN_BACK ) {
				cmd->buttons |= IN_FORWARD;
				cmd->buttons &= ~IN_BACK;
			}
			else  if ( cmd->buttons & IN_FORWARD ) {
				cmd->buttons |= IN_BACK;
				cmd->buttons &= ~IN_FORWARD;
			}

			return;
		}

		bool move = LocalPlayer->m_vecVelocity( ).Length2D( ) > 0.1f && !g_Vars.globals.Fakewalking;

		// save view, depending if locked or not.
		if ( ( g_Vars.antiaim.freestand_lock && move ) || !g_Vars.antiaim.freestand_lock )
			m_view = cmd->viewangles.y;

		cmd->viewangles.x = GetAntiAimX( settings );
		float flYaw = GetAntiAimY( settings, cmd );

		// https://github.com/VSES/SourceEngine2007/blob/master/se2007/engine/cl_main.cpp#L1877-L1881
		if ( !*bSendPacket || !*bFinalPacket ) {
			cmd->viewangles.y = flYaw;

			Distort( cmd );
		}
		else {
			// make our fake 180 degrees away from our real, and let's add a jitter 
			// ranging from -90 to 90 to make shit even fuckier 
			std::uniform_int_distribution random( -90, 90 );

			cmd->viewangles.y = Math::AngleNormalize( flYaw + 180 + random( generator ) );
		}

		static bool bNegative = false;
		auto bSwitch = std::fabs( Interfaces::m_pGlobalVars->curtime - g_Vars.globals.m_flBodyPred ) < Interfaces::m_pGlobalVars->interval_per_tick;
		auto bSwap = std::fabs( Interfaces::m_pGlobalVars->curtime - g_Vars.globals.m_flBodyPred ) > 1.1 - ( Interfaces::m_pGlobalVars->interval_per_tick * 5 );
		if ( !Interfaces::m_pClientState->m_nChokedCommands( )
			&& Interfaces::m_pGlobalVars->curtime >= g_Vars.globals.m_flBodyPred
			&& LocalPlayer->m_fFlags( ) & FL_ONGROUND && g_Vars.globals.m_bUpdate ) {
			// fake yaw.
			switch( settings->yaw ) {
			case 1: // dynamic
				bSwitch ? cmd->viewangles.y += 90.f : cmd->viewangles.y -= 90.f;
				bSwitch = !bSwitch;
				break;
			case 2: // sway 
				bNegative ? cmd->viewangles.y += 110.f : cmd->viewangles.y -= 110.f;
				break;
			case 3: // static		
				bSwap ? cmd->viewangles.y += g_Vars.antiaim.break_lby : cmd->viewangles.y -= g_Vars.antiaim.break_lby;
				break;
			default:
				break;
			}

			m_flLowerBodyUpdateYaw = LocalPlayer->m_flLowerBodyYawTarget( );
		}

		/*if ( g_Vars.antiaim.imposta ) {
			Interfaces::AntiAimbot::Get( )->ImposterBreaker( bSendPacket, cmd );
		}*/
	}

	void C_AntiAimbot::PrePrediction( bool* bSendPacket, Encrypted_t<CUserCmd> cmd ) {
		if ( !g_Vars.antiaim.enabled )
			return;

		C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );
		if ( !local || local->IsDead( ) )
			return;

		Encrypted_t<CVariables::ANTIAIM_STATE> settings( &g_Vars.antiaim_stand );

		//g_Vars.globals.m_bInverted = g_Vars.antiaim.desync_flip_bind.enabled;

		if ( !IsEnabled( cmd, settings ) ) {
			g_Vars.globals.RegularAngles = cmd->viewangles;
			return;
		}
	}

	float C_AntiAimbot::GetAntiAimX( Encrypted_t<CVariables::ANTIAIM_STATE> settings ) {
		switch ( settings->pitch ) {
		case 1: // down
			return 89.f;
		case 2: // up 
			return -89.f;
		case 3: // zero
			return 0.f;
		default:
			return FLT_MAX;
			break;
		}
	}

	float C_AntiAimbot::GetAntiAimY( Encrypted_t<CVariables::ANTIAIM_STATE> settings, Encrypted_t<CUserCmd> cmd ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) )
			return FLT_MAX;

		float flViewAnlge = cmd->viewangles.y;
		float flRetValue = flViewAnlge + 180.f;

		bool bUsingManualAA = g_Vars.globals.manual_aa != -1;

		if( bUsingManualAA ) {
			switch( g_Vars.globals.manual_aa ) {
			case 0:
				flRetValue = flViewAnlge + 90.f;
				break;
			case 1:
				flRetValue = flViewAnlge + 180.f;
				break;
			case 2:
				flRetValue = flViewAnlge - 90.f;
				break;
			}
		}


		// lets do our real yaw.'
		switch( settings->base_yaw ) {
		case 1: // backwards.
			flRetValue = flViewAnlge + 180.f;
			break;
		case 2: // freestand.
		{
			const C_AntiAimbot::Directions Direction = HandleDirection( cmd );
			switch( Direction ) {
			case Directions::YAW_BACK:
				// backwards yaw.
				flRetValue = flViewAnlge + 180.f;
				break;
			case Directions::YAW_LEFT:
				// left yaw.
				flRetValue = flViewAnlge + 90.f;
				break;
			case Directions::YAW_RIGHT:
				// right yaw.
				flRetValue = flViewAnlge - 90.f;
				break;
			case Directions::YAW_NONE:			
				// 180z, cuz wat else to do.
				flRetValue = (flViewAnlge + 180.f / 2.f);
				flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * (3.5 * 20.f), 180.f);
				break;
			}
		}
			break;
		case 3: // 180z
			flRetValue = ( flViewAnlge - 180.f / 2.f );
			flRetValue += std::fmod(Interfaces::m_pGlobalVars->curtime * ( 3.5 * 20.f ), 180.f );
			break;
		default:
			break;
		}

		if( !bUsingManualAA && g_Vars.antiaim.preserve ) {
			if( local->m_vecVelocity( ).Length2D( ) > 3.25f && local->m_vecVelocity( ).Length2D( ) < 20.f && !g_Vars.globals.Fakewalking ) {
				flRetValue = flViewAnlge + 180.f;
			}
		}

		static int iUpdates;
		if( iUpdates > pow( 10, 10 ) )
			iUpdates = 1;

		if( !g_Vars.globals.m_bGround ) {
			flRetValue = flViewAnlge + ( iUpdates % 2 ? -155.f : 155.f );
			++iUpdates;
		}

		return flRetValue;
	}

	void C_AntiAimbot::Distort( Encrypted_t<CUserCmd> cmd ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		if ( !local || local->IsDead( ) )
			return;

		if ( !g_Vars.antiaim.distort )
			return;

		bool bDoDistort = true;
		if ( g_Vars.antiaim.distort_disable_fakewalk && g_Vars.globals.Fakewalking )
			bDoDistort = false;

		if ( g_Vars.antiaim.distort_disable_air && !g_Vars.globals.m_bGround )
			bDoDistort = false;

		static float flLastMoveTime = FLT_MAX;
		static float flLastMoveYaw = FLT_MAX;
		static bool bGenerate = true;
		static float flGenerated = 0.f;

		if ( local->m_PlayerAnimState( )->m_velocity > 0.1f && g_Vars.globals.m_bGround && !g_Vars.globals.Fakewalking ) {
			flLastMoveTime = Interfaces::m_pGlobalVars->realtime;
			flLastMoveYaw = local->m_flLowerBodyYawTarget( );

			if ( g_Vars.antiaim.distort_disable_run )
				bDoDistort = false;
		}

		if ( g_Vars.globals.manual_aa != -1 && !g_Vars.antiaim.distort_manual_aa )
			bDoDistort = false;

		if ( flLastMoveTime == FLT_MAX )
			return;

		if ( flLastMoveYaw == FLT_MAX )
			return;

		if ( !bDoDistort ) {
			bGenerate = true;
		}

		if ( bDoDistort ) {
			// don't distort for longer than this
			if ( fabs( Interfaces::m_pGlobalVars->realtime - flLastMoveTime ) > g_Vars.antiaim.distort_max_time && g_Vars.antiaim.distort_max_time > 0.f ) {
				return;
			}

			if ( g_Vars.antiaim.distort_twist ) {
				float flDistortion = std::sin( ( Interfaces::m_pGlobalVars->realtime * g_Vars.antiaim.distort_speed ) * 0.5f + 0.5f );

				cmd->viewangles.y += g_Vars.antiaim.distort_range * flDistortion;
				return;
			}

			if ( bGenerate ) {
				float flNormalised = std::remainderf( g_Vars.antiaim.distort_range, 360.f );

				flGenerated = RandomFloat( -flNormalised, flNormalised );
				bGenerate = false;
			}

			float flDelta = fabs( flLastMoveYaw - local->m_flLowerBodyYawTarget( ) );
			cmd->viewangles.y += flDelta + flGenerated;
		}
	}

	// CX
	void C_AntiAimbot::ImposterBreaker( bool* bSendPacket, Encrypted_t<CUserCmd> cmd ) {
		auto pLocal = C_CSPlayer::GetLocalPlayer( );
		if ( !pLocal )
			return;

		bool bCrouching = pLocal->m_PlayerAnimState( )->m_fDuckAmount > 0.f;
		float flVelocity = ( bCrouching ? 3.25f : 1.01f );
		static int iUpdates = 0;

		if ( !( g_Vars.globals.m_pCmd->buttons & IN_FORWARD ) && !( g_Vars.globals.m_pCmd->buttons & IN_BACK ) && !( g_Vars.globals.m_pCmd->buttons & IN_MOVELEFT ) && !( g_Vars.globals.m_pCmd->buttons & IN_MOVERIGHT ) && !( g_Vars.globals.m_pCmd->buttons & IN_JUMP ) )
		{
			if ( Interfaces::m_pClientState->m_nChokedCommands( ) == 2 )
			{
				cmd->forwardmove = iUpdates % 2 ? -450 : 450;
				++iUpdates;
			}
		}
	}

	C_AntiAimbot::Directions C_AntiAimbot::HandleDirection( Encrypted_t<CUserCmd> cmd ) {
		const auto pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal )
			return Directions::YAW_NONE;

		// best target.
		struct AutoTarget_t { float fov; C_CSPlayer* player; };
		AutoTarget_t target{ 180.f + 1.f, nullptr };

		// iterate players, for closest distance.
		for( int i{ 1 }; i <= Interfaces::m_pGlobalVars->maxClients; ++i ) {
			auto player = C_CSPlayer::GetPlayerByIndex( i );
			if( !player || player->IsDead( ) )
				continue;

			if( player->IsDormant( ) )
				continue;

			bool is_team = player->IsTeammate( pLocal );
			if( is_team )
				continue;

			auto lag_data = Engine::LagCompensation::Get( )->GetLagData( player->m_entIndex );
			if( !lag_data.IsValid( ) )
				continue;

			// get best target based on fov.
			Vector origin = player->m_vecOrigin( );

			auto AngleDistance = [&]( QAngle& angles, const Vector& start, const Vector& end ) -> float {
				auto direction = end - start;
				auto aimAngles = direction.ToEulerAngles( );
				auto delta = aimAngles - angles;
				delta.Normalize( );

				return sqrtf( delta.x * delta.x + delta.y * delta.y );
			};

			float fov = AngleDistance( cmd->viewangles, g_Vars.globals.m_vecFixedEyePosition, origin );

			if( fov < target.fov ) {
				target.fov = fov;
				target.player = player;
			}
		}

		// get best player.
		const auto player = target.player;
		if( !player )
			return Directions::YAW_NONE;

		Vector& bestOrigin = player->m_vecOrigin( );

		// calculate direction from bestOrigin to our origin
		const auto yaw = Math::CalcAngle( bestOrigin, pLocal->m_vecOrigin( ) );

		Vector forward, right, up;
		Math::AngleVectors( yaw, forward, right, up );

		Vector vecStart = pLocal->GetEyePosition( );
		Vector vecEnd = vecStart + forward * 100.0f;

		Ray_t rightRay( vecStart + right * 35.0f, vecEnd + right * 35.0f ), leftRay( vecStart - right * 35.0f, vecEnd - right * 35.0f );

		// setup trace filter
		CTraceFilter filter{ };
		filter.pSkip = pLocal;

		CGameTrace tr{ };

		m_pEngineTrace->TraceRay( rightRay, MASK_SOLID, &filter, &tr );
		float rightLength = ( tr.endpos - tr.startpos ).Length( );

		m_pEngineTrace->TraceRay( leftRay, MASK_SOLID, &filter, &tr );
		float leftLength = ( tr.endpos - tr.startpos ).Length( );

		static auto leftTicks = 0;
		static auto rightTicks = 0;
		static auto backTicks = 0;

		if( rightLength - leftLength > 20.0f )
			leftTicks++;
		else
			leftTicks = 0;

		if( leftLength - rightLength > 20.0f )
			rightTicks++;
		else
			rightTicks = 0;

		if( fabs( rightLength - leftLength ) <= 20.0f )
			backTicks++;
		else
			backTicks = 0;

		Directions direction = Directions::YAW_NONE;

		if( rightTicks > 10 ) {
			direction = Directions::YAW_RIGHT;
		}
		else {
			if( leftTicks > 10 ) {
				direction = Directions::YAW_LEFT;
			}
			else {
				if( backTicks > 10 )
					direction = Directions::YAW_BACK;
			}
		}

		return direction;
	}
}