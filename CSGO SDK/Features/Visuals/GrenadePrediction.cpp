#include "GrenadePrediction.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../SDK/Classes/entity.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Renderer/Render.hpp"
#include "../Rage/Autowall.h"

class CGrenadePrediction : public IGrenadePrediction {
public:
	void View( ) override;
	void Paint( ) override;
private:
	void Setup( C_BasePlayer* pl, Vector& vecSrc, Vector& vecThrow, const QAngle& angEyeAngles );
	void Simulate( QAngle& Angles, C_BasePlayer* pLocal );
	int Step( Vector& vecSrc, Vector& vecThrow, int tick, float interval );
	bool CheckDetonate( const Vector& vecThrow, const CGameTrace& tr, int tick, float interval );
	void TraceHull( Vector& src, Vector& end, CGameTrace& tr );
	void AddGravityMove( Vector& move, Vector& vel, float frametime, bool onground );
	void PushEntity( Vector& src, const Vector& move, CGameTrace& tr );
	void ResolveFlyCollisionCustom( CGameTrace& tr, Vector& vecVelocity, float interval );
	int PhysicsClipVelocity( const Vector& in, const Vector& normal, Vector& out, float overbounce );

	int wpn_index = 0;
	float flThrowVelocity = 0.f;
	float flThrowStrength = 0.f;
	std::vector<Vector> vecPath;
	std::vector<std::pair<Vector, Color>> vecBounces;
	std::vector< C_BaseEntity* > vecIgnoredEntities;
};

class CPredTraceFilter : public ITraceFilter {
public:
	CPredTraceFilter( ) = default;

	bool ShouldHitEntity( IHandleEntity* pEntityHandle, int /*contentsMask*/ ) {
		if( !pEntityHandle || entities.empty( ) )
			return false;

		auto it = std::find( entities.begin( ), entities.end( ), pEntityHandle );
		if( it != entities.end( ) )
			return false;

		ClientClass* pEntCC = ( ( IClientEntity* )pEntityHandle )->GetClientClass( );
		if( pEntCC && strcmp( ccIgnore, "" ) ) {
			if( pEntCC->m_pNetworkName == ccIgnore )
				return false;
		}

		return true;
	}

	virtual TraceType GetTraceType( ) const { return TraceType::TRACE_EVERYTHING; }

	inline void SetIgnoreClass( const char* Class ) { ccIgnore = Class; }

	std::vector< C_BaseEntity* > entities;
	const char* ccIgnore = "";
};

IGrenadePrediction* IGrenadePrediction::Get( ) {
	static CGrenadePrediction instance;
	return &instance;
}

void CGrenadePrediction::View( ) {
	if( !g_Vars.esp.NadePred )
		return;

	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );

	if( !pLocal || pLocal->IsDead( ) )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( ).Xor( );

	if( !pWeaponData )
		return;
	flThrowStrength = 0.f;
	flThrowVelocity = 0.f;

	if( pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE && pWeapon->m_fThrowTime( ) <= 0 ) {
		QAngle angThrow;
		Interfaces::m_pEngine->GetViewAngles( angThrow );

		flThrowStrength = pWeapon->m_flThrowStrength( );
		flThrowVelocity = pWeaponData->m_flThrowVelocity;

		wpn_index = pWeapon->m_iItemDefinitionIndex();
		Simulate( angThrow, pLocal );
	}
	else {
		wpn_index = -1;
	}
}

inline float CSGO_Armor( float flDamage, int ArmorValue ) {
	float flArmorRatio = 0.5f;
	float flArmorBonus = 0.5f;
	if( ArmorValue > 0 ) {
		float flNew = flDamage * flArmorRatio;
		float flArmor = ( flDamage - flNew ) * flArmorBonus;

		if( flArmor > static_cast< float >( ArmorValue ) ) {
			flArmor = static_cast< float >( ArmorValue ) * ( 1.f / flArmorBonus );
			flNew = flDamage - flArmor;
		}

		flDamage = flNew;
	}
	return flDamage;
}


void CGrenadePrediction::Paint( ) {
	if( !g_Vars.esp.NadePred )
		return;

	C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );

	if( !pLocal || pLocal->IsDead( ) )
		return;

	auto pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );
	if( !pWeapon )
		return;

	auto pWeaponData = pWeapon->GetCSWeaponData( ).Xor( );

	if( !pWeaponData )
		return;

	static CTraceFilter filter{ };
	CGameTrace	                   trace;
	std::pair< float, C_CSPlayer* >    target{ 0.f, nullptr };

	// setup trace filter for later.
	filter.pSkip = ( pLocal );

	if( ( wpn_index ) && vecPath.size( ) > 1 && pWeaponData->m_iWeaponType == WEAPONTYPE_GRENADE && pWeapon->m_fThrowTime( ) <= 0 ) {
		Vector2D ab, cd;
		Vector prev = vecPath[ 0 ];

		// iterate all players.
		for( int i{ 1 }; i <= Interfaces::m_pGlobalVars->maxClients; ++i ) {
			C_CSPlayer* player = ( C_CSPlayer* )Interfaces::m_pEntList->GetClientEntity( i );
			if( !player )
				continue;

			if( player->IsDead( ) || player->m_bGunGameImmunity( ) || player->IsTeammate( pLocal ) )
				continue;

			// get center of mass for player.
			Vector center = player->WorldSpaceCenter( );

			// get delta between center of mass and final nade pos.
			Vector delta = center - vecPath.back( );

			if( wpn_index == WEAPON_HEGRENADE ) {
				// is within damage radius?
				if( delta.Length( ) > 475.f )
					continue;

				// check if our path was obstructed by anything using a trace.
				Interfaces::m_pEngineTrace->TraceRay( Ray_t( vecPath.back( ), center ), MASK_SHOT, ( ITraceFilter* )&filter, &trace );

				// something went wrong here.
				if( !trace.hit_entity || trace.hit_entity != player )
					continue;

				static float a = 105.0f;
				static float b = 25.0f;
				static float c = 140.0f;

				float d = ( ( ( ( player->m_vecOrigin( ) ) - vecPath.back( ) ).Length( ) - b ) / c );
				float flDamage = a * exp( -d * d );

				auto damage = std::max( static_cast< int >( ceilf( CSGO_Armor( flDamage, player->m_ArmorValue( ) ) ) ), 0 );

				// better target?
				if( damage > target.first ) {
					target.first = damage;
					target.second = player;
				}
			}
			else if( wpn_index == WEAPON_MOLOTOV || wpn_index == WEAPON_FIREBOMB ) {
				// is within damage radius?
				if( delta.Length( ) > 131.f )
					continue;

				// hardcoded bullshit /shrug
				target.first = 0.f;
				target.second = player;
			}
		}

		if( wpn_index == WEAPON_MOLOTOV || wpn_index == WEAPON_FIREBOMB || wpn_index == WEAPON_HEGRENADE || wpn_index == WEAPON_SMOKE ) {
			auto color = Color( 255, 255, 255, 120 );

			if( target.second ) {
				if( target.first >= target.second->m_iHealth( ) ) {
					color = Color( 173, 208, 37, 120 );
				}
			}

			Render::Engine::WorldCircle( *vecPath.rbegin( ), 131.f, color, Color( 0, 0, 0, 0 ) );
		}

		// we have a target for damage.
		if( target.second ) {
			Vector2D screen;

			g_Vars.globals.bReleaseGrenade = target.first >= target.second->m_iHealth( );

			// replace the last bounce with green.
			if( !vecBounces.empty( ) )
				vecBounces.back( ).second = { 0, 255, 0, 255 };

			if( WorldToScreen( vecBounces.back( ).first, screen ) ) {
				if( wpn_index == WEAPON_MOLOTOV || wpn_index == WEAPON_FIREBOMB ) {
					Render::Engine::segoe.string( screen.x, screen.y + 5,
						Color( 255, 255, 255, 180 ),
						std::string( XorStr( "Reach" ) ), Render::Engine::ALIGN_CENTER );
				}
				else {
					Render::Engine::segoe.string( screen.x, screen.y + 5,
						target.first >= target.second->m_iHealth( ) ? Color( 173, 208, 37, 180 ) : Color( 255, 255, 255, 180 ),
						std::string( XorStr( "-" ) + std::to_string( ( int )target.first ) ), Render::Engine::ALIGN_CENTER );
				}
			}
		}

		// path.
		for( auto it = vecPath.begin( ), end = vecPath.end( ); it != end; ++it ) {
			if( WorldToScreen( prev, ab ) && WorldToScreen( *it, cd ) ) {
				Render::Engine::Line( ab, cd, g_Vars.esp.nade_pred_color.ToRegularColor( ) );

				// hmm?
				Render::Engine::Line( ab + Vector2D( 1, 0 ), cd + Vector2D( 1, 0 ), g_Vars.esp.nade_pred_color.ToRegularColor( ) );
			}

			prev = *it;
		}

		Vector2D temp;
		// bounces
		/*for( auto it = vecBounces.begin( ), end = vecBounces.end( ); it != end; ++it ) {
			if( WorldToScreen( it->first, temp ) ) {
				Render::Engine::CircleFilled( temp_point.x, temp_point.y, 2, 4, it->second );
			}
		}*/

		// last bounce
		if( WorldToScreen( vecBounces.rbegin( )->first, temp ) ) {
			Render::Engine::CircleFilled( temp.x, temp.y, 2, 4, vecBounces.rbegin( )->second );
		}
	}
}

// Returns A + (B-A)*flPercent.
	// float Lerp( float flPercent, float A, float B );
template <class T>
__forceinline T Lerp( float flPercent, T const& A, T const& B )
{
	return A + ( B - A ) * flPercent;
}

void CGrenadePrediction::Setup( C_BasePlayer* pl, Vector& vecSrc, Vector& vecThrow, const QAngle& angEyeAngles ) {
	if( !pl )
		return;

	QAngle angThrow = angEyeAngles;
	float pitch = angThrow.pitch;

	if( pitch <= 90.0f ) {
		if( pitch < -90.0f ) {
			pitch += 360.0f;
		}
	}
	else {
		pitch -= 360.0f;
	}
	float a = pitch - ( 90.0f - fabs( pitch ) ) * 10.0f / 90.0f;
	angThrow.pitch = a;

	// get ThrowVelocity from weapon files.
	float flVel = flThrowVelocity * 0.9f;

	// clipped to [ 15, 750 ]
	Math::Clamp( flVel, 15.f, 750.f );

	//clamp the throw strength ranges just to be sure
	float flClampedThrowStrength = flThrowStrength;
	flClampedThrowStrength = std::clamp( flClampedThrowStrength, 0.0f, 1.0f );

	flVel *= Lerp( flClampedThrowStrength, 0.3f, 1.0f );

	Vector vForward, vRight, vUp;
	vForward = angThrow.ToVectors( &vRight, &vUp );

	// danke DucaRii, ich liebe dich
	// dieses code snippet hat mir so sehr geholfen https://cdn.discordapp.com/attachments/755873329151475845/762297342623088640/unknown.png
	// thanks DucaRii, you are the greatest
	// loveeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
	// kochamy DucaRii
	vecSrc = pl->GetAbsOrigin( ) + pl->m_vecViewOffset( );
	float off = Lerp( flClampedThrowStrength, -12.f, 0.0f );
	vecSrc.z += off;

	// Game calls UTIL_TraceHull here with hull and assigns vecSrc tr.endpos
	CGameTrace tr;
	Vector vecDest = vecSrc;
	vecDest = ( vecDest + vForward * 22.0f );
	TraceHull( vecSrc, vecDest, tr );

	// After the hull trace it moves 6 units back along vForward
	// vecSrc = tr.endpos - vForward * 6
	Vector vecBack = vForward; vecBack *= 6.0f;
	vecSrc = tr.endpos;
	vecSrc -= vecBack;

	// kurwa fix for anti-aim micromovements (c) NICO
	auto velocity = pl->m_vecVelocity( );
	if( velocity.Length2D( ) > 3.4 ) {
		vecThrow = velocity;
	}
	else {
		vecThrow.Init( );
	}

	vecThrow *= 1.25f;
	vecThrow += ( vForward * flVel );
}

void CGrenadePrediction::Simulate( QAngle& Angles, C_BasePlayer* pLocal ) {
	if( !pLocal )
		return;

	Vector vecSrc, vecThrow;
	Setup( pLocal, vecSrc, vecThrow, Angles );

	float interval = Interfaces::m_pGlobalVars->interval_per_tick;

	// Log positions 20 times per sec
	int logstep = TIME_TO_TICKS( 0.05f );

	int logtimer = 0;

	vecIgnoredEntities.clear( );
	vecPath.clear( );
	vecBounces.clear( );

	vecIgnoredEntities.push_back( pLocal );

	for( unsigned int i = 0; i < 2048; ++i ) {
		if( !logtimer )
			vecPath.push_back( vecSrc );

		int s = Step( vecSrc, vecThrow, i, interval );
		if( ( s & 1 ) || vecThrow == Vector( 0, 0, 0 ) )
			break;

		// Reset the log timer every logstep OR we bounced
		if( ( s & 2 ) || logtimer >= logstep ) logtimer = 0;
		else ++logtimer;

		if( vecThrow == Vector( ) )
			break;
	}

	vecPath.push_back( vecSrc );
}

int CGrenadePrediction::Step( Vector& vecSrc, Vector& vecThrow, int tick, float interval ) {
	// Apply gravity
	Vector move;
	AddGravityMove( move, vecThrow, interval, false );

	// Push entity
	CGameTrace tr;
	PushEntity( vecSrc, move, tr );

	int result = 0;
	// Check ending conditions
	if( CheckDetonate( vecThrow, tr, tick, interval ) ) {
		result |= 1;
	}

	// Resolve collisions
	if( tr.fraction != 1.0f && !tr.plane.normal.IsZero( ) ) {
		result |= 2; // Collision!
		ResolveFlyCollisionCustom( tr, vecThrow, interval );
	}

	if( ( result & 1 ) || vecThrow == Vector( 0, 0, 0 ) || tr.fraction != 1.0f ) {
		QAngle angles;
		Math::AngleVectors( angles, ( tr.endpos - tr.startpos ).Normalized( ) );
		vecBounces.push_back( std::make_pair( tr.endpos, Color( 255, 0, 0 ) ) );
	}

	// Set new position
	vecSrc = tr.endpos;

	return result;
}

bool CGrenadePrediction::CheckDetonate( const Vector& vecThrow, const CGameTrace& tr, int tick, float interval ) {
	// convert current simulation tick to time.
	float time = TICKS_TO_TIME( tick );

	switch( wpn_index ) {
	case WEAPON_SMOKE:
		return vecThrow.Length( ) <= 0.1f && !( tick % TIME_TO_TICKS( 0.2f ) );
	case WEAPON_DECOY:
		return vecThrow.Length( ) <= 0.2f && !( tick % TIME_TO_TICKS( 0.2f ) );
	case WEAPON_MOLOTOV:
	case WEAPON_FIREBOMB:
		// Detonate when hitting the floor
		if( tr.fraction != 1.0f && ( std::cos( DEG2RAD( g_Vars.weapon_molotov_maxdetonateslope->GetFloat( ) ) ) <= tr.plane.normal.z ) )
			return true;

		// detonate if we have traveled for too long.
		// checked every 0.1s
		return time >= g_Vars.molotov_throw_detonate_time->GetFloat( ) && !( tick % TIME_TO_TICKS( 0.1f ) );

	case WEAPON_FLASHBANG:
	case WEAPON_HEGRENADE:
	{
		return time >= 1.5f && !( tick % TIME_TO_TICKS( 0.2f ) );
	}
	default:
		return false;
	}

	return false;
}

void CGrenadePrediction::TraceHull( Vector& src, Vector& end, CGameTrace& tr ) {
	// Setup grenade hull
	static const Vector hull[ 2 ] = { Vector( -2.0f, -2.0f, -2.0f ), Vector( 2.0f, 2.0f, 2.0f ) };

	CPredTraceFilter filter;
	filter.SetIgnoreClass( XorStr( "CBaseCSGrenadeProjectile" ) );
	filter.entities = vecIgnoredEntities;

	Ray_t ray;
	ray.Init( src, end, hull[ 0 ], hull[ 1 ] );

	const unsigned int mask = 0x200400B;
	Interfaces::m_pEngineTrace->TraceRay( ray, mask, &filter, &tr );
}

void CGrenadePrediction::AddGravityMove( Vector& move, Vector& vel, float frametime, bool onground ) {
	// gravity for grenades.
	float gravity = g_Vars.sv_gravity->GetFloat( ) * 0.4f;

	// move one tick using current velocity.
	move.x = vel.x * Interfaces::m_pGlobalVars->interval_per_tick;
	move.y = vel.y * Interfaces::m_pGlobalVars->interval_per_tick;

	// apply linear acceleration due to gravity.
	// calculate new z velocity.
	float z = vel.z - ( gravity * Interfaces::m_pGlobalVars->interval_per_tick );

	// apply velocity to move, the average of the new and the old.
	move.z = ( ( vel.z + z ) / 2.f ) * Interfaces::m_pGlobalVars->interval_per_tick;

	// write back new gravity corrected z-velocity.
	vel.z = z;
}

void CGrenadePrediction::PushEntity( Vector& src, const Vector& move, CGameTrace& tr ) {
	Vector vecAbsEnd = src;
	vecAbsEnd += move;

	// Trace through world
	TraceHull( src, vecAbsEnd, tr );
}

void CGrenadePrediction::ResolveFlyCollisionCustom( CGameTrace& tr, Vector& vecVelocity, float interval ) {
	// Calculate elasticity
	float flSurfaceElasticity = 1.0;  // Assume all surfaces have the same elasticity

	 //Don't bounce off of players with perfect elasticity
	if( tr.hit_entity && ( ( C_BaseEntity* )tr.hit_entity )->IsPlayer( ) ) {
		flSurfaceElasticity = 0.3f;
	}

	// if its breakable glass and we kill it, don't bounce.
	// give some damage to the glass, and if it breaks, pass
	// through it.
	if( Autowall::IsBreakable( ( C_BaseEntity* )tr.hit_entity ) ) {
		auto player = ( C_CSPlayer* )tr.hit_entity;

		if( player && player->m_iHealth( ) <= 10 ) { // todo: need scale damage and other stuff
			vecIgnoredEntities.push_back( ( C_BaseEntity* )tr.hit_entity );

			// slow our flight a little bit
			vecVelocity *= 0.4f;
			return;
		}
	}

	float flGrenadeElasticity = 0.45f;
	float flTotalElasticity = flGrenadeElasticity * flSurfaceElasticity;
	if( flTotalElasticity > 0.9f )
		flTotalElasticity = 0.9f;

	if( flTotalElasticity < 0.0f )
		flTotalElasticity = 0.0f;

	// Calculate bounce
	Vector vecAbsVelocity;
	PhysicsClipVelocity( vecVelocity, tr.plane.normal, vecAbsVelocity, 2.0f );
	vecAbsVelocity *= flTotalElasticity;

	// Stop completely once we move too slow
	float flSpeedSqr = vecAbsVelocity.Length2DSquared( );
	static const float flMinSpeedSqr = 20.0f * 20.0f; // 30.0f * 30.0f in CSS
	if( flSpeedSqr < flMinSpeedSqr ) {
		vecAbsVelocity.x = vecAbsVelocity.y = vecAbsVelocity.z = 0;
	}

	// Stop if on ground
	if( tr.plane.normal.z > 0.7f ) {
		vecVelocity = vecAbsVelocity;
		vecAbsVelocity.Mul( ( 1.0f - tr.fraction ) * interval );
		PushEntity( tr.endpos, vecAbsVelocity, tr );
	}
	else {
		vecVelocity = vecAbsVelocity;
	}
}

int CGrenadePrediction::PhysicsClipVelocity( const Vector& in, const Vector& normal, Vector& out, float overbounce ) {
	static const float STOP_EPSILON = 0.1f;

	float    backoff;
	float    change;
	float    angle;
	int        i, blocked;

	blocked = 0;

	angle = normal[ 2 ];

	if( angle > 0 ) {
		blocked |= 1;        // floor
	}
	if( !angle ) {
		blocked |= 2;        // step
	}

	backoff = in.Dot( normal ) * overbounce;

	for( i = 0; i < 3; i++ ) {
		change = normal[ i ] * backoff;
		out[ i ] = in[ i ] - change;
		if( out[ i ] > -STOP_EPSILON && out[ i ] < STOP_EPSILON ) {
			out[ i ] = 0;
		}
	}

	return blocked;
}