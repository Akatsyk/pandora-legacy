#include "LagCompensation.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/sdk.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"

#include "../../SDK/CVariables.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/core.hpp"
#include "../../Utils/FnvHash.hpp"

#include "../Visuals/EventLogger.hpp"

#include "Autowall.h"
#include "../Miscellaneous/Movement.hpp"

#include "../Game/SetupBones.hpp"
#include "../../Utils/Threading/Threading.h"
#include "../../Utils/Threading/shared_mutex.h"

#include "../../Utils/InputSys.hpp"

#include "../Rage/FakeLag.hpp"

#include "../../Hooking/Hooked.hpp"

#include "AnimationSystem.hpp"
#include "TickbaseShift.hpp"

#include <sstream>

#define MT_SETUP_BONES
//#define DEBUG_RESOLVER
namespace Engine
{
	struct LagCompData {
		std::map< int, Engine::C_EntityLagData > m_PlayerHistory;

		float m_flLerpTime, m_flOutLatency, m_flServerLatency;
		bool m_GetEvents = false;
	};

	static LagCompData _lagcomp_data;

	class C_LagCompensation : public Engine::LagCompensation {
	public:
		virtual void Update( );
		virtual bool IsRecordOutOfBounds( const Engine::C_LagRecord& record, float target_time = 0.2f, int tickbase_shift = -1, bool bDeadTimeCheck = true ) const;

		virtual Encrypted_t<C_EntityLagData> GetLagData( int entindex ) {
			C_EntityLagData* data = nullptr;
			if( lagData->m_PlayerHistory.count( entindex ) > 0 )
				data = &lagData->m_PlayerHistory.at( entindex );
			return Encrypted_t<C_EntityLagData>( data );
		}

		virtual float GetLerp( ) const {
			return lagData.Xor( )->m_flLerpTime;
		}

		virtual void ClearLagData( ) {
			lagData->m_PlayerHistory.clear( );
		}

		C_LagCompensation( ) : lagData( &_lagcomp_data ) { };
		virtual ~C_LagCompensation( ) { };
	private:
		virtual void SetupLerpTime( );
		Encrypted_t<LagCompData> lagData;
	};

	C_LagCompensation g_LagComp;
	Engine::LagCompensation* Engine::LagCompensation::Get( ) {
		return &g_LagComp;
	}

	Engine::C_EntityLagData::C_EntityLagData( ) {
		;
	}

	void C_LagCompensation::Update( ) {
		if( !Interfaces::m_pEngine->IsInGame( ) || !Interfaces::m_pEngine->GetNetChannelInfo( ) ) {
			lagData->m_PlayerHistory.clear( );
			return;
		}

		auto updateReason = 0;

		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || !g_Vars.globals.HackIsReady )
			return;

		SetupLerpTime( );

		for( int i = 1; i <= Interfaces::m_pGlobalVars->maxClients; ++i ) {
			auto player = C_CSPlayer::GetPlayerByIndex( i );
			if( !player || player == local || !player->IsPlayer( ) )
				continue;

			player_info_t player_info;
			if( !Interfaces::m_pEngine->GetPlayerInfo( player->m_entIndex, &player_info ) ) {
				continue;
			}

			if( !player->GetClientRenderable( ) )
				continue;

			if( Hooked::player_hooks.count( player->m_entIndex ) < 1 )
				continue;

			auto lag_data = Encrypted_t<C_EntityLagData>( &lagData->m_PlayerHistory[ player->m_entIndex ] );
			lag_data->UpdateRecordData( lag_data, player, player_info, updateReason );
		}
	}

	bool C_LagCompensation::IsRecordOutOfBounds( const Engine::C_LagRecord& record, float flTargetTime, int nTickbaseShiftTicks, bool bDeadTimeCheck ) const {
		Encrypted_t<INetChannel> pNetChannel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
		if( !pNetChannel.IsValid( ) )
			return true;

		C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal )
			return true;

		// note from alpha: i forgot wo wipe my ass
		
		// use prediction curtime for this.
		float curtime = TICKS_TO_TIME( pLocal->m_nTickBase( ) * g_TickbaseController.s_nExtraProcessingTicks );

		// correct is the amount of time we have to correct game time,
		float correct = lagData.Xor()->m_flLerpTime + lagData.Xor( )->m_flServerLatency;

		// stupid fake latency goes into the incoming latency.
		//correct += lagData.Xor( )->m_flServerLatency;

		// check bounds [ 0, sv_maxunlag ]
		Math::Clamp( correct, 0.f, 1.0f );

		// calculate difference between tick sent by player and our latency based tick.
		// ensure this record isn't too old.
		return std::fabsf( correct - ( curtime - record.m_flSimulationTime ) ) < flTargetTime;
	}

	void C_LagCompensation::SetupLerpTime( ) {
		float updaterate = g_Vars.cl_updaterate->GetFloat( );

		float minupdaterate = g_Vars.sv_minupdaterate->GetFloat( );
		float maxupdaterate = g_Vars.sv_maxupdaterate->GetFloat( );

		float min_interp = g_Vars.sv_client_min_interp_ratio->GetFloat( );
		float max_interp = g_Vars.sv_client_max_interp_ratio->GetFloat( );

		float flLerpAmount = g_Vars.cl_interp->GetFloat( );
		float flLerpRatio = g_Vars.cl_interp_ratio->GetFloat( );
		flLerpRatio = Math::Clamp( flLerpRatio, min_interp, max_interp );
		if( flLerpRatio == 0.0f )
			flLerpRatio = 1.0f;

		float updateRate = Math::Clamp( updaterate, minupdaterate, maxupdaterate );
		lagData->m_flLerpTime = std::fmaxf( flLerpAmount, flLerpRatio / updateRate );

		auto netchannel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
		lagData->m_flOutLatency = netchannel->GetLatency( FLOW_OUTGOING );
		lagData->m_flServerLatency = netchannel->GetLatency( FLOW_INCOMING );
	}

	void Engine::C_EntityLagData::UpdateRecordData( Encrypted_t< C_EntityLagData > pThis, C_CSPlayer* player, const player_info_t& info, int updateType ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		auto team_check = g_Vars.rage.enabled && !g_Vars.rage.team_check && player->IsTeammate( C_CSPlayer::GetLocalPlayer( ) );
		if( player->IsDead( ) || team_check ) {
			pThis->m_History.clear( );
			pThis->m_flLastUpdateTime = 0.0f;
			pThis->m_flLastScanTime = 0.0f;
			return;
		}

		bool isDormant = player->IsDormant( );

		// no need to store insane amount of data
		while( pThis->m_History.size( ) > int( 1.0f / Interfaces::m_pGlobalVars->interval_per_tick ) ) {
			pThis->m_History.pop_back( );
		}

		if( isDormant ) {
			pThis->m_flLastUpdateTime = 0.0f;
			if( pThis->m_History.size( ) > 0 && pThis->m_History.front( ).m_bTeleportDistance ) {
				pThis->m_History.clear( );
			}

			return;
		}

		if( info.userId != pThis->m_iUserID ) {
			pThis->Clear( );
			pThis->m_iUserID = info.userId;
		}

		// did player update?
		float simTime = player->m_flSimulationTime( );
		if( pThis->m_flLastUpdateTime >= simTime ) {
			return;
		}

		if ( player->m_flOldSimulationTime( ) > simTime ) {
			return;
		}

		auto anim_data = AnimationSystem::Get( )->GetAnimationData( player->m_entIndex );
		if( !anim_data )
			return;

		if( anim_data->m_AnimationRecord.empty( ) )
			return;

		auto anim_record = &anim_data->m_AnimationRecord.front( );
		if( anim_record->m_bShiftingTickbase )
			return;

		pThis->m_flLastUpdateTime = simTime;

		bool isTeam = local->IsTeammate( player );

		// invalidate all records, if player abusing teleport distance
		/*if( !anim_record->m_bIsInvalid && anim_record->m_bTeleportDistance && pThis->m_History.size( ) > 0 ) {
			for( auto& record : pThis->m_History )
				record.m_bSkipDueToResolver = true;
		}*/

		// add new record and get reference to newly added record.
		auto record = Encrypted_t<C_LagRecord>( &pThis->m_History.emplace_front( ) );

		record->Setup( player );
		record->m_flRealTime = Interfaces::m_pEngine->GetLastTimeStamp( );
		record->m_flServerLatency = Engine::LagCompensation::Get( )->m_flServerLatency;
		record->m_flDuckAmount = anim_record->m_flDuckAmount;
		record->m_flEyeYaw = anim_record->m_angEyeAngles.yaw;
		record->m_flEyePitch = anim_record->m_angEyeAngles.pitch;
		record->m_bIsShoting = anim_record->m_bIsShoting;
		record->m_bIsValid = !anim_record->m_bIsInvalid;
		record->m_bBonesCalculated = anim_data->m_bBonesCalculated;
		record->m_flAnimationVelocity = player->m_PlayerAnimState( )->m_velocity;
		record->m_bTeleportDistance = anim_record->m_bTeleportDistance;
		record->m_flAbsRotation = player->m_PlayerAnimState( )->m_flAbsRotation;
		record->m_iLaggedTicks = TIME_TO_TICKS( player->m_flSimulationTime( ) - player->m_flOldSimulationTime( ) );
		record->m_bResolved = anim_record->m_bResolved;
		record->m_iResolverMode = anim_record->m_iResolverMode;

		std::memcpy( record->m_BoneMatrix, anim_data->m_Bones, player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );
	}

	bool Engine::C_EntityLagData::DetectAutoDirerction( Encrypted_t< C_EntityLagData > pThis, C_CSPlayer* player ) {
		if( !player || player->IsDead( ) )
			return false;

		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) )
			return false;

		auto weapon = ( C_WeaponCSBaseGun* ) local->m_hActiveWeapon( ).Get( );
		if( !weapon )
			return false;

		auto weaponData = weapon->GetCSWeaponData( );
		if( !weaponData.IsValid( ) )
			return false;

		auto eye_position = g_Vars.globals.m_vecFixedEyePosition;
		auto enemy_eye_position = player->GetEyePosition( );

		auto delta = enemy_eye_position - eye_position;
		delta.Normalize( );

		int iterator = 0;

		bool result = false;

		CTraceFilterWorldOnly filter;

		float edges[ 4 ] = { 0.0f };

		// TODO: find out, can we increase accuracy of this method?
		float directionRag = atan2( delta.y, delta.x );
		float atTargetYaw = RAD2DEG( directionRag );
		for( int i = 0; i < 4; ++i ) {
			float radAngle = DEG2RAD( ( i * 90.0f ) + atTargetYaw );
			float angleSin, angleCos;
			DirectX::XMScalarSinCos( &angleSin, &angleCos, radAngle );

			Vector pos;
			pos.x = enemy_eye_position.x + ( angleCos * 16.0f );
			pos.y = enemy_eye_position.y + ( angleSin * 16.0f );
			pos.z = enemy_eye_position.z;

			Vector extrapolated = eye_position;

			Autowall::C_FireBulletData data;

			data.m_bPenetration = true;
			data.m_Player = local;
			data.m_Weapon = weapon;
			data.m_WeaponData = weaponData.Xor( );
			data.m_vecStart = extrapolated;
			data.m_bShouldIgnoreDistance = true;

			data.m_vecDirection = pos - extrapolated;
			data.m_flPenetrationDistance = data.m_vecDirection.Normalize( );
			data.m_Filter = &filter;

			float dmg = Autowall::FireBullets( &data );

			if( dmg >= 1.0f )
				result = true;
			else if( dmg < 0.0f )
				dmg = 0.0f;

			edges[ i ] = dmg;
		}

		if( result
			&& ( int( edges[ 0 ] ) != int( edges[ 1 ] )
				|| int( edges[ 0 ] ) != int( edges[ 2 ] )
				|| int( edges[ 0 ] ) != int( edges[ 3 ] ) ) ) {
			pThis->m_flEdges[ 0 ] = edges[ 0 ];
			pThis->m_flEdges[ 1 ] = edges[ 1 ];
			pThis->m_flEdges[ 2 ] = edges[ 2 ];
			pThis->m_flEdges[ 3 ] = edges[ 3 ];

			pThis->m_flDirection = atTargetYaw;
		}
		else {
			result = false;
		}

		return result;
	}

	void Engine::C_EntityLagData::Clear( ) {
		this->m_History.clear( );
		m_iUserID = -1;
		m_flLastScanTime = 0.0f;
		m_flLastUpdateTime = 0.0f;
	}

	float C_LagRecord::GetAbsYaw( ) {
		return this->m_angAngles.yaw;
	}

	matrix3x4_t* C_LagRecord::GetBoneMatrix( ) {
		if( !this->m_bBonesCalculated )
			return this->m_BoneMatrix;

		return this->m_BoneMatrix;
	}

	void Engine::C_LagRecord::Setup( C_CSPlayer* player ) {
		auto collidable = player->m_Collision( );
		this->m_vecMins = collidable->m_vecMins;
		this->m_vecMaxs = collidable->m_vecMaxs;

		this->m_vecOrigin = player->m_vecOrigin( );
		this->m_angAngles = player->GetAbsAngles( );

		this->m_vecVelocity = player->m_vecVelocity( );
		this->m_flSimulationTime = player->m_flSimulationTime( );

		this->m_iFlags = player->m_fFlags( );

		std::memcpy( this->m_BoneMatrix, player->m_CachedBoneData( ).Base( ),
			player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

		this->player = player;
	}

	void Engine::C_LagRecord::Apply( C_CSPlayer* player ) {
		auto collidable = player->m_Collision( );
		collidable->SetCollisionBounds( this->m_vecMins, this->m_vecMaxs );

		player->m_flSimulationTime( ) = this->m_flSimulationTime;

		QAngle absAngles = this->m_angAngles;
		absAngles.yaw = this->GetAbsYaw( );

		player->m_fFlags( ) = this->m_iFlags;

		player->SetAbsAngles( absAngles );
		player->SetAbsOrigin( this->m_vecOrigin );

		matrix3x4_t* matrix = GetBoneMatrix( );

		if( matrix ) {
			std::memcpy( player->m_CachedBoneData( ).Base( ), matrix,
				player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

			// force bone cache
			player->m_iMostRecentModelBoneCounter( ) = *( int* )Engine::Displacement.Data.m_uModelBoneCounter;
			player->m_BoneAccessor( ).m_ReadableBones = player->m_BoneAccessor( ).m_WritableBones = 0xFFFFFFFF;
			player->m_flLastBoneSetupTime( ) = FLT_MAX;
		}
	}

	void C_BaseLagRecord::Setup( C_CSPlayer* player ) {
		auto collidable = player->m_Collision( );
		this->m_vecMins = collidable->m_vecMins;
		this->m_vecMaxs = collidable->m_vecMaxs;

		this->m_flSimulationTime = player->m_flSimulationTime( );

		this->m_angAngles = player->GetAbsAngles( );
		this->m_vecOrigin = player->m_vecOrigin( );

		if( player->m_PlayerAnimState( ) != nullptr )
			this->m_flAbsRotation = player->m_PlayerAnimState( )->m_flAbsRotation;

		std::memcpy( this->m_BoneMatrix, player->m_CachedBoneData( ).Base( ),
			player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

		this->player = player;
	}

	void C_BaseLagRecord::Apply( C_CSPlayer* player ) {
		auto collidable = player->m_Collision( );
		collidable->SetCollisionBounds( this->m_vecMins, this->m_vecMaxs );

		player->m_flSimulationTime( ) = this->m_flSimulationTime;

		player->SetAbsAngles( this->m_angAngles );
		player->SetAbsOrigin( this->m_vecOrigin );

		if( player->m_PlayerAnimState( ) != nullptr )
			player->m_PlayerAnimState( )->m_flAbsRotation = this->m_flAbsRotation;

		std::memcpy( player->m_CachedBoneData( ).Base( ), this->m_BoneMatrix,
			player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );

		// force bone cache
		player->m_iMostRecentModelBoneCounter( ) = *( int* )Engine::Displacement.Data.m_uModelBoneCounter;
		player->m_BoneAccessor( ).m_ReadableBones = player->m_BoneAccessor( ).m_WritableBones = 0xFFFFFFFF;
		player->m_flLastBoneSetupTime( ) = FLT_MAX;
	}
}
