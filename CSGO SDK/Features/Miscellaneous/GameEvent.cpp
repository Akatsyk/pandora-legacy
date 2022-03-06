#include "GameEvent.hpp"
#include "../Visuals/EventLogger.hpp"
#include "../../source.hpp"
#include "../../Utils/FnvHash.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../Rage/LagCompensation.hpp"
#include <sstream>
#include "BulletBeamTracer.hpp"
#include "../Visuals/Hitmarker.hpp"
#include "AutoBuy.hpp"
#include "../../SDK/core.hpp"
#include "../Visuals/ESP.hpp"
#include "../Visuals/CChams.hpp"
#include "../Rage/ShotInformation.hpp"
#pragma comment(lib,"Winmm.lib")
#include "../Rage/TickbaseShift.hpp"

#include <fstream>


#define ADD_GAMEEVENT(n)  Interfaces::m_pGameEvent->AddListener(this, XorStr(#n), false)

class C_GameEvent : public GameEvent {
public: // GameEvent interface
	virtual void Register( );
	virtual void Shutdown( );

	C_GameEvent( ) { };
	virtual ~C_GameEvent( ) { };
public: // IGameEventListener
	virtual void FireGameEvent( IGameEvent* event );
	virtual int  GetEventDebugID( void );
};

Encrypted_t<GameEvent> GameEvent::Get( ) {
	static C_GameEvent instance;
	return &instance;
}

void C_GameEvent::Register( ) {
	ADD_GAMEEVENT( player_hurt );
	ADD_GAMEEVENT( bullet_impact );
	ADD_GAMEEVENT( weapon_fire );
	ADD_GAMEEVENT( bomb_planted );
	ADD_GAMEEVENT( player_death );
	ADD_GAMEEVENT( round_start );
	ADD_GAMEEVENT( item_purchase );
	ADD_GAMEEVENT( bomb_begindefuse );
	ADD_GAMEEVENT( bomb_abortdefuse );
	ADD_GAMEEVENT( bomb_pickup );
	ADD_GAMEEVENT( bomb_beginplant );
	ADD_GAMEEVENT( bomb_abortplant );
	ADD_GAMEEVENT( item_pickup );
	ADD_GAMEEVENT( round_mvp );
	ADD_GAMEEVENT( grenade_thrown );
	ADD_GAMEEVENT( buytime_ended );
	ADD_GAMEEVENT( round_end );
	ADD_GAMEEVENT( game_newmap );
	ADD_GAMEEVENT( bomb_beep );
	ADD_GAMEEVENT( bomb_defused );
	ADD_GAMEEVENT( bomb_exploded );
}

void C_GameEvent::Shutdown( ) {
	Interfaces::m_pGameEvent->RemoveListener( this );
}

void C_GameEvent::FireGameEvent( IGameEvent* pEvent ) {
	if( !pEvent )
		return;

	C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer( );
	if( !LocalPlayer || !Interfaces::m_pEngine->IsInGame( ) )
		return;

	auto event_hash = hash_32_fnv1a_const( pEvent->GetName( ) );
	auto event_string = std::string( pEvent->GetName( ) );

	Engine::C_ShotInformation::Get( )->EventCallback( pEvent, event_hash );
	g_Vars.globals.m_bLocalPlayerHarmedThisTick = false;

	auto HitgroupToString = [ ] ( int hitgroup ) -> std::string {
		switch( hitgroup ) {
		case Hitgroup_Generic:
			return XorStr( "generic" );
		case Hitgroup_Head:
			return XorStr( "head" );
		case Hitgroup_Chest:
			return XorStr( "chest" );
		case Hitgroup_Stomach:
			return XorStr( "stomach" );
		case Hitgroup_LeftArm:
			return XorStr( "left arm" );
		case Hitgroup_RightArm:
			return XorStr( "right arm" );
		case Hitgroup_LeftLeg:
			return XorStr( "left leg" );
		case Hitgroup_RightLeg:
			return XorStr( "right leg" );
		case Hitgroup_Neck:
			return XorStr( "neck" );
		}
		return XorStr( "generic" );
	};

	static auto sv_showimpacts_time = Interfaces::m_pCvar->FindVar( XorStr( "sv_showimpacts_time" ) );

	// Force constexpr hash computing 
	switch( event_hash ) {
	case hash_32_fnv1a_const( "game_newmap" ):
	{
		Engine::LagCompensation::Get( )->ClearLagData( );
		g_Vars.globals.m_bNewMap = true;
		g_Vars.globals.BobmActivityIndex = -1;
	}
	case hash_32_fnv1a_const( "bullet_impact" ):
	{
		auto ent = ( C_CSPlayer* )Interfaces::m_pEntList->GetClientEntity( Interfaces::m_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) ) );

		bool bCameFromLocal = LocalPlayer && ent && LocalPlayer == ent;
		bool bCameFromEnemy = LocalPlayer && ent && ent->m_iTeamNum( ) != LocalPlayer->m_iTeamNum( );
		if( LocalPlayer && !LocalPlayer->IsDead( ) ) {

			float x = pEvent->GetFloat( XorStr( "x" ) ), y = pEvent->GetFloat( XorStr( "y" ) ), z = pEvent->GetFloat( XorStr( "z" ) );
			if( g_Vars.esp.beam_enabled ) {
				if( bCameFromLocal ) {
					IBulletBeamTracer::Get( )->PushBeamInfo( { Interfaces::m_pGlobalVars->curtime, LocalPlayer->GetEyePosition( ), Vector( x, y, z ), Color( ), ent->EntIndex( ), LocalPlayer->m_nTickBase( ) } );
				}
				else if( bCameFromEnemy ) {
					if( !ent->IsDormant( ) )
						IBulletBeamTracer::Get( )->PushBeamInfo( { Interfaces::m_pGlobalVars->curtime, ent->GetEyePosition( ), Vector( x, y, z ), Color( ), ent->EntIndex( ), -1 } );
				}
			}

			if( bCameFromLocal ) {
				int color[ 4 ] = { g_Vars.esp.server_impacts.r * 255, g_Vars.esp.server_impacts.g * 255, g_Vars.esp.server_impacts.b * 255, g_Vars.esp.server_impacts.a * 255 };

				if( g_Vars.misc.server_impacts_spoof ) // draw server impact
					Interfaces::m_pDebugOverlay->AddBoxOverlay( Vector( x, y, z ), Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), QAngle( 0, 0, 0 ), color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ],
						sv_showimpacts_time->GetFloat( ) );
			}
		}

		break;
	}
	case hash_32_fnv1a_const( "round_end" ):
	{
		g_Vars.globals.BobmActivityIndex = -1;
		//IRoundFireBulletsStore::Get( )->EventCallBack( pEvent, 1, nullptr );
		break;
	}
	case hash_32_fnv1a_const( "round_freeze_end" ):
	{
		g_Vars.globals.BobmActivityIndex = -1;
		g_Vars.globals.IsRoundFreeze = false;
		break;
	}
	case hash_32_fnv1a_const( "round_prestart" ):
	{
		g_Vars.globals.BobmActivityIndex = -1;
		g_Vars.globals.IsRoundFreeze = true;
		break;
	}
	case hash_32_fnv1a_const( "bomb_beep" ):
	{
		break;
	}
	case hash_32_fnv1a_const( "bomb_defused" ):
	{
		g_Vars.globals.BobmActivityIndex = -1;
		g_Vars.globals.bBombActive = false;
		break;
	}
	case hash_32_fnv1a_const( "bomb_exploded" ):
	{
		g_Vars.globals.BobmActivityIndex = -1;
		g_Vars.globals.bBombActive = false;
		break;
	}
	case hash_32_fnv1a_const( "player_hurt" ):
	{
		auto enemy = pEvent->GetInt( XorStr( "userid" ) );
		auto attacker = pEvent->GetInt( XorStr( "attacker" ) );
		auto remaining_health = pEvent->GetString( XorStr( "health" ) );
		auto dmg_to_health = pEvent->GetInt( XorStr( "dmg_health" ) );
		auto hitgroup = pEvent->GetInt( XorStr( "hitgroup" ) );

		auto enemy_index = Interfaces::m_pEngine->GetPlayerForUserID( enemy );
		auto attacker_index = Interfaces::m_pEngine->GetPlayerForUserID( attacker );
		auto pEnemy = C_CSPlayer::GetPlayerByIndex( enemy_index );
		auto pAttacker = C_CSPlayer::GetPlayerByIndex( attacker_index );

		player_info_t attacker_info;
		player_info_t enemy_info;

		if( pEnemy && pAttacker && Interfaces::m_pEngine->GetPlayerInfo( attacker_index, &attacker_info ) && Interfaces::m_pEngine->GetPlayerInfo( enemy_index, &enemy_info ) ) {
			auto local = reinterpret_cast< C_CSPlayer* >( Interfaces::m_pEntList->GetClientEntity( Interfaces::m_pEngine->GetLocalPlayer( ) ) );
			auto entity = reinterpret_cast< C_CSPlayer* >( Interfaces::m_pEntList->GetClientEntity( Interfaces::m_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) ) ) );

			if( !entity || !local )
				return;

			if( attacker_index != Interfaces::m_pEngine->GetLocalPlayer( ) ) {
				if( enemy_index == local->EntIndex( ) ) {
					if( g_Vars.esp.event_harm ) {
						std::stringstream msg;

						msg << XorStr( "harmed " ) << enemy_info.szName << XorStr( " for " ) << dmg_to_health << XorStr( " in " ) << HitgroupToString( hitgroup ).data( );

						ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "" ) );
					}

					g_Vars.globals.m_bLocalPlayerHarmedThisTick = true;
				}
			}
			else {
				if( g_Vars.esp.event_dmg ) {
					std::stringstream msg;

					msg << XorStr( "hit " ) << enemy_info.szName << XorStr( " for " ) << dmg_to_health << XorStr( " in " ) << HitgroupToString( hitgroup ).data( );

					ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "" ) );
				}

				if( g_Vars.misc.hitsound ) {
					if( g_Vars.misc.hitsound_type && !g_Vars.globals.m_hitsounds.empty( ) ) {
						// DIRECTORY XD!!!
						int idx = g_Vars.misc.hitsound_custom;
						if( idx >= g_Vars.globals.m_hitsounds.size( ) )
							idx = g_Vars.globals.m_hitsounds.size( ) - 1;
						else if( idx < 0 )
							idx = 0;

						std::string curfile = g_Vars.globals.m_hitsounds[ idx ];
						if( !curfile.empty( ) ) {
							std::string dir = GetDocumentsDirectory( ).append( XorStr( "\\ams\\" ) ).append( curfile );

							auto ReadWavFileIntoMemory = [ & ] ( std::string fname, BYTE** pb, DWORD* fsize ) {
								std::ifstream f( fname, std::ios::binary );

								f.seekg( 0, std::ios::end );
								int lim = f.tellg( );
								*fsize = lim;

								*pb = new BYTE[ lim ];
								f.seekg( 0, std::ios::beg );

								f.read( ( char* )*pb, lim );

								f.close( );
							};

							DWORD dwFileSize;
							BYTE* pFileBytes;
							ReadWavFileIntoMemory( dir.data( ), &pFileBytes, &dwFileSize );

							// danke anarh1st47, ich liebe dich
							// dieses code snippet hat mir so sehr geholfen https://i.imgur.com/ybWTY2o.png
							// thanks anarh1st47, you are the greatest
							// loveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
							// kochamy anarh1st47
							auto modify_volume = [ & ] ( BYTE* bytes ) {
								int offset = 0;
								for( int i = 0; i < dwFileSize / 2; i++ ) {
									if( bytes[ i ] == 'd' && bytes[ i + 1 ] == 'a'
										&& bytes[ i + 2 ] == 't' && bytes[ i + 3 ] == 'a' )
									{
										offset = i;
										break;
									}
								}

								if( !offset )
									return;

								BYTE* pDataOffset = ( bytes + offset );
								DWORD dwNumSampleBytes = *( DWORD* )( pDataOffset + 4 );
								DWORD dwNumSamples = dwNumSampleBytes / 2;

								SHORT* pSample = ( SHORT* )( pDataOffset + 8 );
								for( DWORD dwIndex = 0; dwIndex < dwNumSamples; dwIndex++ )
								{
									SHORT shSample = *pSample;
									shSample = ( SHORT )( shSample * ( g_Vars.misc.hitsound_volume / 100.f ) );
									*pSample = shSample;
									pSample++;
									if( ( ( BYTE* )pSample ) >= ( bytes + dwFileSize - 1 ) )
										break;
								}
							};

							if( pFileBytes ) {
								modify_volume( pFileBytes );
								PlaySoundA( ( LPCSTR )pFileBytes, NULL, SND_MEMORY | SND_ASYNC );
							}
						}
					}
					else {
						Interfaces::m_pSurface->PlaySound_( XorStr( "buttons\\arena_switch_press_02.wav" ) );
					}
				}

				Hitmarkers::m_nLastDamageData = { hitgroup == Hitgroup_Head ? Color( 255, 0, 00 ) : Color( 255, 255, 255 ), dmg_to_health };
				Hitmarkers::AddScreenHitmarker( hitgroup == Hitgroup_Head ? Color( 0, 150, 255 ) : Color( 255, 255, 255 ) );
			}
		}
		break;
	}
	case hash_32_fnv1a_const( "item_purchase" ):
	{
		if( !g_Vars.esp.event_buy )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		player_info_t info;

		auto player = C_CSPlayer::GetPlayerByIndex( index );
		auto local = C_CSPlayer::GetLocalPlayer( );

		if( !player || !local || player->IsTeammate( local ) )
			return;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		std::stringstream msg;
		msg << XorStr( "wpn: " ) << pEvent->GetString( XorStr( "weapon" ) ) << XorStr( " | " );
		msg << XorStr( "money: " ) << std::string( XorStr( "$" ) ).append( std::to_string( player->m_iAccount( ) ) ).data( ) << XorStr( " | " );
		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "buy" ) );

		break;
	}
	case hash_32_fnv1a_const( "bomb_begindefuse" ):
	{
		if( !g_Vars.esp.event_bomb )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		if( index == Interfaces::m_pEngine->GetLocalPlayer( ) )
			return;

		player_info_t info;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		bool has_defuse = pEvent->GetBool( XorStr( "haskit" ) );

		std::stringstream msg;
		if( has_defuse )
			msg << XorStr( "act: " ) << XorStr( "started defusing (kit)" ) << XorStr( " | " );
		else
			msg << XorStr( "act: " ) << XorStr( "started defusing" ) << XorStr( " | " );

		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "bomb" ) );

		g_Vars.globals.BobmActivityIndex = index;
		break;
	}
	case hash_32_fnv1a_const( "bomb_abortdefuse" ):
	{
		if( !g_Vars.esp.event_bomb )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		if( index == Interfaces::m_pEngine->GetLocalPlayer( ) )
			return;

		player_info_t info;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		std::stringstream msg;
		msg << XorStr( "act: " ) << XorStr( "stopped defusing" ) << XorStr( " | " );
		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "bomb" ) );

		g_Vars.globals.BobmActivityIndex = -1;
		break;
	}
	case hash_32_fnv1a_const( "bomb_pickup" ):
	{
		if( !g_Vars.esp.event_bomb )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		if( index == Interfaces::m_pEngine->GetLocalPlayer( ) )
			return;

		player_info_t info;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		std::stringstream msg;
		msg << XorStr( "act: " ) << XorStr( "picked up the bomb" ) << XorStr( " | " );
		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "bomb" ) );

		break;
	}
	case hash_32_fnv1a_const( "bomb_beginplant" ):
	{
		if( !g_Vars.esp.event_bomb )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		if( index == Interfaces::m_pEngine->GetLocalPlayer( ) )
			return;

		player_info_t info;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		std::stringstream msg;
		msg << XorStr( "act: " ) << XorStr( "started planting" ) << XorStr( " | " );
		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "bomb" ) );

		g_Vars.globals.BobmActivityIndex = index;
		break;
	}
	case hash_32_fnv1a_const( "bomb_abortplant" ):
	{
		if( !g_Vars.esp.event_bomb )
			return;

		auto userid = pEvent->GetInt( XorStr( "userid" ) );

		if( !userid )
			return;

		int index = Interfaces::m_pEngine->GetPlayerForUserID( userid );

		if( index == Interfaces::m_pEngine->GetLocalPlayer( ) )
			return;

		player_info_t info;

		if( !Interfaces::m_pEngine->GetPlayerInfo( index, &info ) )
			return;

		std::stringstream msg;
		msg << XorStr( "act: " ) << XorStr( "stopped planting" ) << XorStr( " | " );
		msg << XorStr( "ent: " ) << info.szName;

		ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 255, 255, 255 ), true, XorStr( "bomb" ) );

		g_Vars.globals.BobmActivityIndex = -1;
		break;
	}
	case hash_32_fnv1a_const( "bomb_planted" ):
	{
		g_Vars.globals.bBombActive = true;
		break;
	}
	case hash_32_fnv1a_const( "round_start" ):
	{
		g_Vars.globals.Fakewalking = g_Vars.misc.fakeduck_bind.enabled = false;

		for( int i = 0; i < Interfaces::g_pDeathNotices->m_vecDeathNotices.Count( ); i++ ) {
			auto cur = &Interfaces::g_pDeathNotices->m_vecDeathNotices[ i ];
			if( !cur ) {
				continue;
			}

			cur->m_flStartTime = 0.f;
		}

		IAutoBuy::Get( )->Main( );

		for( size_t i = 1; i <= 64; i++ ) {
			IEsp::Get( )->SetAlpha( i );

			auto player = ( C_CSPlayer* )Interfaces::m_pEntList->GetClientEntity( i );
			if( !player || player == C_CSPlayer::GetLocalPlayer( ) || player->IsTeammate( C_CSPlayer::GetLocalPlayer( ) ) )
				continue;

			// hacky fix for dormant esp at start of round, i guess.
			// haven't tested this.
			player->m_iHealth( ) = 100;
		}

		g_Vars.globals.bBombActive = false;
		g_Vars.globals.BobmActivityIndex = -1;
		break;
	}
	case hash_32_fnv1a_const( "player_death" ):
	{
		int iUserID = Interfaces::m_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "userid" ) ) );
		int iAttacker = Interfaces::m_pEngine->GetPlayerForUserID( pEvent->GetInt( XorStr( "attacker" ) ) );
		auto iEnemyIndex = Interfaces::m_pEngine->GetPlayerForUserID( iUserID );

		C_CSPlayer* pAttacker = ( C_CSPlayer* )Interfaces::m_pEntList->GetClientEntity( iAttacker );
		if( pAttacker ) {
			if( iAttacker == Interfaces::m_pEngine->GetLocalPlayer( ) && iUserID != Interfaces::m_pEngine->GetLocalPlayer( ) ) {
				Hitmarkers::AddScreenHitmarker( Color( 255, 0, 0 ) );
			}
		}

		IEsp::Get( )->SetAlpha( iEnemyIndex );
		break;
	}
	}
}

int C_GameEvent::GetEventDebugID( void ) {
	return 42;
}
