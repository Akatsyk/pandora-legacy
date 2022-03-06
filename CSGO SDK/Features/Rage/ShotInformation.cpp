#include "ShotInformation.hpp"
#include "../Visuals/EventLogger.hpp"
#include <sstream>
#include "Autowall.h"
#include "Resolver.hpp"

namespace Engine
{
	struct C_TraceData {
		bool is_resolver_issue;
		bool is_correct;
	};

	bool CanHitPlayer( C_LagRecord* pRecord, int iSide, const Vector& vecEyePos, const Vector& vecEnd, int iHitboxIndex ) {
		auto hdr = *( studiohdr_t** )pRecord->player->m_pStudioHdr( );
		if( !hdr )
			return false;

		auto pHitboxSet = hdr->pHitboxSet( pRecord->player->m_nHitboxSet( ) );

		if( !pHitboxSet )
			return false;

		auto pHitbox = pHitboxSet->pHitbox( iHitboxIndex );

		if( !pHitbox )
			return false;

		bool bIsCapsule = pHitbox->m_flRadius != -1.0f;
		bool bHitIntersection = false;

		CGameTrace tr;

		//Interfaces::m_pDebugOverlay->AddLineOverlay( eyePos, end, 255, 0, 0, false, 5.f );

		matrix3x4_t* pBone = pRecord->GetBoneMatrix( );

		Vector vecMin = pHitbox->bbmin.Transform( pBone[ pHitbox->bone ] );
		Vector vecMax = pHitbox->bbmax.Transform( pBone[ pHitbox->bone ] );

		bHitIntersection = bIsCapsule ?
			Math::IntersectSegmentToSegment( vecEyePos, vecEnd, vecMin, vecMax, pHitbox->m_flRadius ) : Math::IntersectionBoundingBox( vecEyePos, vecEnd, vecMin, vecMax );//( tr.hit_entity == pRecord->player && ( tr.hitgroup >= Hitgroup_Head && tr.hitgroup <= Hitgroup_RightLeg ) || tr.hitgroup == Hitgroup_Gear );

		return bHitIntersection;
	};

	void TraceMatrix( const Vector& vecStart, const Vector& vecEnd, C_LagRecord* pRecord, C_CSPlayer* Player,
		std::vector<C_TraceData>& TracesData, int iSide, bool bDidHit, int iHitboxIndex ) {
		auto& TraceData = TracesData.emplace_back( );

		pRecord->Apply( Player );

		TraceData.is_resolver_issue = CanHitPlayer( pRecord, iSide, vecStart, vecEnd, iHitboxIndex );
		TraceData.is_correct = TraceData.is_resolver_issue == bDidHit;
	}

	Encrypted_t<C_ShotInformation> C_ShotInformation::Get( ) {
		static C_ShotInformation instance;
		return &instance;
	}

	void C_ShotInformation::Start( ) {
		auto netchannel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
		if( !netchannel.IsValid( ) ) {
			return;
		}

		ProcessEvents( );

		auto latency = netchannel->GetAvgLatency( FLOW_OUTGOING ) * 1000.f;

		const auto pLocal = C_CSPlayer::GetLocalPlayer( );
		auto it = this->m_Shapshots.begin( );
		while( it != this->m_Shapshots.end( ) ) {
			if( it->correctSequence && Interfaces::m_pClientState->m_nLastCommandAck( ) >= it->outSequence + latency ) {
#if defined(DEBUG_MODE) || defined(DEV)
				ILoggerEvent::Get( )->PushEvent( XorStr( "Missed shot due to prediction error" ), FloatColor( 0.5f, 0.5f, 0.5f ), false );
#endif

				it = this->m_Shapshots.erase( it );
			}
			else {
				it++;
			}
		}
	}

	void C_ShotInformation::ProcessEvents( ) {
		auto TranslateHitbox = [ ] ( int hitbox ) -> std::string {
			std::string result = { };
			switch( hitbox ) {
			case HITBOX_HEAD:
				result = XorStr( "head" ); break;
			case HITBOX_NECK:
			case HITBOX_LOWER_NECK:
				result = XorStr( "neck" ); break;
			case HITBOX_CHEST:
			case HITBOX_LOWER_CHEST:
			case HITBOX_UPPER_CHEST:
				result = XorStr( "chest" ); break;
			case HITBOX_RIGHT_FOOT:
			case HITBOX_RIGHT_CALF:
			case HITBOX_RIGHT_THIGH:
			case HITBOX_LEFT_FOOT:
			case HITBOX_LEFT_CALF:
			case HITBOX_LEFT_THIGH:
				result = XorStr( "leg" ); break;
			case HITBOX_LEFT_FOREARM:
			case HITBOX_LEFT_HAND:
			case HITBOX_LEFT_UPPER_ARM:
			case HITBOX_RIGHT_FOREARM:
			case HITBOX_RIGHT_HAND:
			case HITBOX_RIGHT_UPPER_ARM:
				result = XorStr( "arm" ); break;
			case HITBOX_STOMACH:
			case HITBOX_PELVIS:
				result = XorStr( "stomach" ); break;
			default:
				result = XorStr( "-" );
			}

			return result;
		};

		auto FixedStrLength = [ ] ( std::string str ) -> std::string {
			if( ( int )str[ 0 ] > 255 )
				return XorStr( "" );

			if( str.size( ) < 15 )
				return str;

			std::string result;
			for( size_t i = 0; i < 15u; i++ )
				result.push_back( str.at( i ) );
			return result;
		};

		if( !this->m_GetEvents ) {
			return;
		}

		this->m_GetEvents = false;

		if( this->m_Shapshots.empty( ) ) {
			this->m_Weaponfire.clear( );
			return;
		}

		if( this->m_Weaponfire.empty( ) ) {
			return;
		}

		try {
			auto it = this->m_Weaponfire.begin( );
			while( it != this->m_Weaponfire.end( ) ) {
				if( this->m_Shapshots.empty( ) || this->m_Weaponfire.empty( ) ) {
					this->m_Weaponfire.clear( );
					break;
				}

				auto snapshot = it->snapshot;

				if( !( &it->snapshot ) || !&( *it->snapshot ) ) {
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				if( snapshot == this->m_Shapshots.end( ) ) {
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				auto player = snapshot->player;
				if( !player ) {
					this->m_Shapshots.erase( it->snapshot );
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				if( player != C_CSPlayer::GetPlayerByIndex( it->snapshot->playerIdx ) ) {
					this->m_Shapshots.erase( it->snapshot );
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				auto anim_data = AnimationSystem::Get( )->GetAnimationData( player->m_entIndex );
				if( !anim_data ) {
					this->m_Shapshots.erase( it->snapshot );
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				if( it->impacts.empty( ) ) {
					this->m_Shapshots.erase( it->snapshot );
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				auto& lag_data = Engine::LagCompensation::Get( )->GetLagData( player->m_entIndex );
				if( player->IsDead( ) ) {
					this->m_Shapshots.erase( it->snapshot );
					it = this->m_Weaponfire.erase( it );
					continue;
				}

				auto did_hit = it->damage.size( ) > 0;

				// last reseived impact
				auto last_impact = it->impacts.back( );

				C_BaseLagRecord backup;
				backup.Setup( player );

				std::vector<C_TraceData> trace_data;
				TraceMatrix( it->snapshot->eye_pos, last_impact, &it->snapshot->resolve_record, player, trace_data, 0, did_hit, it->snapshot->Hitbox );

				backup.Apply( player );

				g_Vars.globals.m_iFiredShots++;

				if( !did_hit ) {
					auto aimpoint_distance = it->snapshot->eye_pos.Distance( it->snapshot->AimPoint ) - 32.f;
					auto impact_distance = it->snapshot->eye_pos.Distance( last_impact );
					float aimpoint_lenght = it->snapshot->AimPoint.Length( );
					float impact_lenght = last_impact.Length( );

					auto td = &trace_data[ 0 ];

					auto AddMissLog = [ & ] ( std::string reason ) -> void {
						std::stringstream msg;

						player_info_t info;
						if( Interfaces::m_pEngine->GetPlayerInfo( it->snapshot->playerIdx, &info ) ) {
#if defined (BETA_MODE) || defined(DEV)
							msg << XorStr( "reason: " ) << reason.data( ) << XorStr( " | " );
							msg << XorStr( "flick: " ) << int( it->snapshot->resolve_record.m_iResolverMode == EResolverModes::RESOLVE_PRED ) << XorStr( " | " );
							msg << XorStr( "dmg: " ) << it->snapshot->m_nSelectedDamage << XorStr( " | " );
							msg << XorStr( "hitgroup: " ) << TranslateHitbox( it->snapshot->Hitbox ).data( ) << XorStr( " | " );
							msg << XorStr( "player: " ) << FixedStrLength( info.szName ).data( );

							ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 128, 128 ), !g_Vars.misc.undercover_log, XorStr( "missed shot " ) );
#else
							msg << XorStr( "reason: " ) << reason.data( ) << XorStr( " | " );
							msg << XorStr( "hitgroup: " ) << TranslateHitbox( it->snapshot->Hitbox ).data( ) << XorStr( " | " );
							msg << XorStr( "ent: " ) << FixedStrLength( info.szName ).data( );

							ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 128, 128 ), false, XorStr( "missed shot " ) );
#endif
						}
					};

					if( td->is_resolver_issue ) {
						if ( it->snapshot->ResolverType == EResolverModes::RESOLVE_PRED )
							lag_data->m_iMissedShotsLBY++;
						else
							lag_data->m_iMissedShots++;

						if( g_Vars.esp.event_resolver ) {
							AddMissLog( XorStr( "resolver" ) );
						}
					}
					else if( aimpoint_distance > impact_distance ) { // occulusion issue
						if( g_Vars.esp.event_resolver ) {
							AddMissLog( XorStr( "occlusion" ) );
						}
					}
					else { // spread issue
						if( g_Vars.esp.event_resolver ) {
							AddMissLog( XorStr( "spread" ) );
						}
					}
				}
				else {
					bool shoud_break = false;
					auto best_damage = it->damage.end( );
					auto dmg = it->damage.begin( );
					while( dmg != it->damage.end( ) ) {
						shoud_break = true;
						if( best_damage == it->damage.end( )
							|| dmg->damage > best_damage->damage ) {
							best_damage = dmg;
						}

						dmg++;
					}


					if( it->snapshot->Hitgroup != best_damage->hitgroup ) {
				
					}
					else {
						
					}
				}

				this->m_Shapshots.erase( it->snapshot );
				it = this->m_Weaponfire.erase( it );
			}
		}
		catch( const std::exception& ) {
			return;
		}
	}

	void C_ShotInformation::EventCallback( IGameEvent* gameEvent, uint32_t hash ) {
		if( this->m_Shapshots.empty( ) ) {
			return;
		}
		auto net_channel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
		if( !net_channel.IsValid( ) ) {
			this->m_Shapshots.clear( );
			return;
		}

		C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) ) {
			this->m_Shapshots.clear( );
			return;
		}

		C_WeaponCSBaseGun* weapon = ( C_WeaponCSBaseGun* )( local->m_hActiveWeapon( ).Get( ) );
		if( !weapon ) {
			this->m_Shapshots.clear( );
			return;
		}

		auto weapon_data = weapon->GetCSWeaponData( );
		if( !weapon_data.IsValid( ) ) {
			this->m_Shapshots.clear( );
			return;
		}

		auto it = this->m_Shapshots.begin( );
		while( it != this->m_Shapshots.end( ) ) {
			// unhandled snapshots
			if( std::fabsf( it->time - Interfaces::m_pGlobalVars->realtime ) >= 2.5f ) {
				it = this->m_Shapshots.erase( it );
			}
			else {
				it++;
			}
		}

		auto snapshot = this->m_Shapshots.end( );

		switch( hash ) {
		case hash_32_fnv1a_const( "player_hurt" ):
		{
			if( this->m_Weaponfire.empty( ) || Interfaces::m_pEngine->GetPlayerForUserID( gameEvent->GetInt( XorStr( "attacker" ) ) ) != local->EntIndex( ) )
				return;

			// TODO: check if need backtrack
			auto target = C_CSPlayer::GetPlayerByIndex( Interfaces::m_pEngine->GetPlayerForUserID( gameEvent->GetInt( XorStr( "userid" ) ) ) );
			if( !target || target == local || local->IsTeammate( target ) || target->IsDormant( ) )
				return;

			auto& player_damage = this->m_Weaponfire.back( ).damage.emplace_back( );
			player_damage.playerIdx = target->m_entIndex;
			player_damage.player = target;
			player_damage.damage = gameEvent->GetInt( XorStr( "dmg_health" ) );
			player_damage.hitgroup = gameEvent->GetInt( XorStr( "hitgroup" ) );
			break;
		}
		case hash_32_fnv1a_const( "bullet_impact" ):
		{
			if( this->m_Weaponfire.empty( ) || Interfaces::m_pEngine->GetPlayerForUserID( gameEvent->GetInt( XorStr( "userid" ) ) ) != local->EntIndex( ) )
				return;

			this->m_Weaponfire.back( ).impacts.emplace_back( gameEvent->GetFloat( XorStr( "x" ) ), gameEvent->GetFloat( XorStr( "y" ) ), gameEvent->GetFloat( XorStr( "z" ) ) );
			break;
		}
		case hash_32_fnv1a_const( "weapon_fire" ):
		{
			if( Interfaces::m_pEngine->GetPlayerForUserID( gameEvent->GetInt( XorStr( "userid" ) ) ) != local->EntIndex( ) )
				return;

			int nElement = this->m_Weaponfire.size( ) / weapon_data->m_iBullets;

			// will get iBullets weapon_fire events
			if( nElement != this->m_Shapshots.size( ) ) {
				snapshot = this->m_Shapshots.begin( ) + nElement;
				auto& fire = this->m_Weaponfire.emplace_back( );
				fire.snapshot = snapshot;
			}
			break;
		}
		case hash_32_fnv1a_const( "player_death" ):
		{
			int id = Interfaces::m_pEngine->GetPlayerForUserID( gameEvent->GetInt( XorStr( "userid" ) ) );
			auto player = C_CSPlayer::GetPlayerByIndex( id );

			if( !player || player == local )
				return;

			auto lagData = Engine::LagCompensation::Get( )->GetLagData( id );
			if( lagData.IsValid( ) ) {
				lagData->m_iMissedShots = 0;
				lagData->m_iMissedShotsLBY = 0;
				g_Vars.globals.m_iFiredShots = 0;
			}

			break;
		}
		case hash_32_fnv1a_const( "round_start" ):
		{
			for( int i = 1; i < Interfaces::m_pGlobalVars->maxClients; ++i ) {
				auto lagData = Engine::LagCompensation::Get( )->GetLagData( i );
				if( lagData.IsValid( ) ) {
					lagData->m_iMissedShots = 0;
					lagData->m_iMissedShotsLBY = 0;
					g_Vars.globals.m_iFiredShots = 0;
				}
			}

			break;
		}
		}

		this->m_GetEvents = true;
	}

	void C_ShotInformation::CreateSnapshot( C_CSPlayer* player, const Vector& shootPosition, const Vector& aimPoint, Engine::C_LagRecord* record, int resolverSide, int hitgroup, int hitbox, int nDamage, bool doubleTap ) {
		auto& snapshot = this->m_Shapshots.emplace_back( );

		snapshot.playerIdx = player->m_entIndex;
		snapshot.player = player;
		snapshot.resolve_record = *record;
		snapshot.eye_pos = shootPosition;
		snapshot.time = Interfaces::m_pGlobalVars->realtime;
		snapshot.correctSequence = false;
		snapshot.correctEyePos = false;
		snapshot.Hitbox = hitbox;
		snapshot.doubleTap = doubleTap;
		snapshot.ResolverType = resolverSide;

		snapshot.AimPoint = aimPoint;
		snapshot.Hitgroup = hitgroup;
		snapshot.m_nSelectedDamage = nDamage;

		auto data = AnimationSystem::Get( )->GetAnimationData( player->m_entIndex );
		if( data ) {
			data->m_flLastScannedYaw = record->m_flEyeYaw;
		}

		//auto lagData = Engine::LagCompensation::Get( )->GetLagData( player->m_entIndex );
		//if( lagData.IsValid( ) ) {
		//	switch( snapshot.ResolverType ) {
		//	case EResolverModes::RESOLVE_STAND1:
		//		// predicting updates or freestanding them.
		//		if( !Engine::g_ResolverData[ player->m_entIndex ].m_bPredictingUpdates && !Engine::g_ResolverData[ player->m_entIndex ].m_bCollectedFreestandData )
		//			break;

		//		// valid move data.
		//		if( Engine::g_ResolverData[ player->m_entIndex ].m_bCollectedValidMoveData ) {
		//			// clear any past yaws.
		//			lagData->m_flLastMoveYaw.clear( );

		//			// send back our LastMove LBY.
		//			lagData->m_flLastMoveYaw.push_front( Engine::g_ResolverData[ player->m_entIndex ].m_sMoveData.m_flLowerBodyYawTarget );

		//			lagData->m_bHitLastMove = true;
		//		}
		//		break;
		//	default:
		//		break;
		//	}
		//}
	}

	void C_ShotInformation::CorrectSnapshots( bool is_sending_packet ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local )
			return;

		auto netchannel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
		if( !netchannel.IsValid( ) )
			return;

		for( auto& snapshot : this->m_Shapshots ) {
			if( is_sending_packet && !snapshot.correctSequence ) {
				snapshot.outSequence = Interfaces::m_pClientState->m_nLastOutgoingCommand( ) + Interfaces::m_pClientState->m_nChokedCommands( ) + 1;
				snapshot.correctSequence = true;
			}
		}
	}
}

