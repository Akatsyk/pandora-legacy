#include "ZeusBot.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "LagCompensation.hpp"
#include "TickbaseShift.hpp"
#include "Autowall.h"
#include "Ragebot.hpp"
#include "ShotInformation.hpp"
#include "../Visuals/CChams.hpp"

// onetap zeusbot
namespace Interfaces {
	struct ZeusBotData {
		C_CSPlayer* m_pCurrentTarget = nullptr;
		C_CSPlayer* m_pLocalPlayer = nullptr;
		C_WeaponCSBaseGun* m_pLocalWeapon = nullptr;
		Encrypted_t<CCSWeaponInfo> m_pWeaponInfo = nullptr;
		Encrypted_t<CUserCmd> m_pCmd = nullptr;
		Vector m_vecEyePos;
	};

	ZeusBotData _zeus_bot_data;

	class CZeusBot : public ZeusBot {
	public:
		CZeusBot( ) : zeusBotData( &_zeus_bot_data ) { }

		void Main( Encrypted_t<CUserCmd> pCmd, bool* sendPacket ) override;
	private:
		bool TargetEntity( C_CSPlayer* pPlayer, bool* sendPacket, Engine::C_LagRecord* record );

		Encrypted_t<ZeusBotData> zeusBotData;
	};

	ZeusBot* ZeusBot::Get( ) {
		static CZeusBot instance;
		return &instance;
	}

	void CZeusBot::Main( Encrypted_t<CUserCmd> pCmd, bool* sendPacket ) {
		if( !Interfaces::m_pEngine->IsInGame( ) || !g_Vars.rage.enabled|| !g_Vars.misc.zeus_bot )
			return;

		zeusBotData->m_pLocalPlayer = C_CSPlayer::GetLocalPlayer( );
		if( !zeusBotData->m_pLocalPlayer || zeusBotData->m_pLocalPlayer->IsDead( ) )
			return;

		zeusBotData->m_pLocalWeapon = ( C_WeaponCSBaseGun* )zeusBotData->m_pLocalPlayer->m_hActiveWeapon( ).Get( );
		if( !zeusBotData->m_pLocalWeapon || !zeusBotData->m_pLocalWeapon->IsWeapon( ) )
			return;

		zeusBotData->m_pWeaponInfo = zeusBotData->m_pLocalWeapon->GetCSWeaponData( );
		if( !zeusBotData->m_pWeaponInfo.IsValid( ) )
			return;

		zeusBotData->m_pCmd = pCmd;
		if( zeusBotData->m_pLocalPlayer->m_flNextAttack( ) > Interfaces::m_pGlobalVars->curtime || zeusBotData->m_pLocalWeapon->m_flNextPrimaryAttack( ) > Interfaces::m_pGlobalVars->curtime )
			return;

		if( zeusBotData->m_pLocalWeapon->m_iItemDefinitionIndex( ) != WEAPON_ZEUS )
			return;

		zeusBotData->m_vecEyePos = zeusBotData->m_pLocalPlayer->GetEyePosition( );
		for( int i = 1; i <= Interfaces::m_pGlobalVars->maxClients; i++ ) {
			C_CSPlayer* Target = ( C_CSPlayer* )Interfaces::m_pEntList->GetClientEntity( i );
			if( !Target
				|| Target->IsDead( )
				|| Target->IsDormant( )
				|| !Target->IsPlayer( )
				|| Target->m_iTeamNum( ) == zeusBotData->m_pLocalPlayer->m_iTeamNum( )
				|| Target->m_bGunGameImmunity( ) )
				continue;

			auto lag_data = Engine::LagCompensation::Get( )->GetLagData( Target->m_entIndex );
			if( !lag_data.IsValid( ) || lag_data->m_History.empty( ) )
				continue;

			Engine::C_LagRecord* previousRecord = nullptr;
			Engine::C_LagRecord backup;
			backup.Setup( Target );
			for( auto& record : lag_data->m_History ) {
				if( Engine::LagCompensation::Get( )->IsRecordOutOfBounds( record, 0.2f )
					|| record.m_bSkipDueToResolver
					|| !record.m_bIsValid ) {
					continue;
				}

				if( !previousRecord
					|| previousRecord->m_vecOrigin != record.m_vecOrigin
					|| previousRecord->m_flEyeYaw != record.m_flEyeYaw
					|| previousRecord->m_angAngles.yaw != record.m_angAngles.yaw
					|| previousRecord->m_vecMaxs != record.m_vecMaxs
					|| previousRecord->m_vecMins != record.m_vecMins ) {
					previousRecord = &record;

					record.Apply( Target );
					if( TargetEntity( Target, sendPacket, &record ) ) {
						zeusBotData->m_pCmd->tick_count = TIME_TO_TICKS( record.m_flSimulationTime + Engine::LagCompensation::Get( )->GetLerp( ) );
						break;
					}
				}
			}

			backup.Apply( Target );
		}
	}

	bool CZeusBot::TargetEntity( C_CSPlayer* pPlayer, bool* sendPacket, Engine::C_LagRecord* record ) {
		if( !record )
			return false;
		
		zeusBotData->m_pCurrentTarget = pPlayer;

		//g_Vars.misc.zeus_bot_hitchance
		auto hdr = *( studiohdr_t** )( pPlayer->m_pStudioHdr( ) );
		if( hdr ) {
			auto hitboxSet = hdr->pHitboxSet( pPlayer->m_nHitboxSet( ) );
			if( hitboxSet ) {
				auto pStomach = hitboxSet->pHitbox( HITBOX_STOMACH );
				auto vecHitboxPos = ( pStomach->bbmax + pStomach->bbmin ) * 0.5f;
				vecHitboxPos = vecHitboxPos.Transform( pPlayer->m_CachedBoneData().Base()[ pStomach->bone ] );

				// run awall
				Autowall::C_FireBulletData fireData;

				Vector vecDirection = vecHitboxPos - zeusBotData->m_vecEyePos;
				vecDirection.Normalize( );

				fireData.m_bPenetration = false;
				fireData.m_vecStart = zeusBotData->m_vecEyePos;
				fireData.m_vecDirection = vecDirection;
				fireData.m_iHitgroup = Hitgroup_Stomach;
				fireData.m_Player = zeusBotData->m_pLocalPlayer;
				fireData.m_TargetPlayer = pPlayer;
				fireData.m_WeaponData = zeusBotData->m_pWeaponInfo.Xor();
				fireData.m_Weapon = zeusBotData->m_pLocalWeapon;
				fireData.m_iPenetrationCount = 0;

				// note - alpha; 
				// ghetto, shit, but good enough for zeusbot;
				// have fun doing ragebot hitchance with this implementation
				const bool bIsAccurate = !( zeusBotData->m_pLocalWeapon->GetInaccuracy( ) >= ( ( 100.0f - g_Vars.misc.zeus_bot_hitchance ) * 0.65f * 0.01125f ) );
				const float flDamage = Autowall::FireBullets( &fireData );
				if( flDamage >= 105.f && bIsAccurate ) {
					Engine::C_ShotInformation::Get( )->CreateSnapshot( pPlayer, zeusBotData->m_vecEyePos, vecHitboxPos, record, g_Vars.globals.m_iResolverSide[ pPlayer->m_entIndex ], Hitgroup_Stomach, HITBOX_STOMACH, int( flDamage ) );
					if( g_Vars.esp.esp_enable && g_Vars.esp.hitmatrix )
						IChams::Get( )->AddHitmatrix( pPlayer, record->GetBoneMatrix( ) );
					
					zeusBotData->m_pCmd->viewangles = vecDirection.ToEulerAngles( );
					zeusBotData->m_pCmd->buttons |= IN_ATTACK;
					
					//if( !g_TickbaseController.bInRapidFire )
					//	*sendPacket = true;

					return true;
				}
			}
		}

		return false;
	}
}