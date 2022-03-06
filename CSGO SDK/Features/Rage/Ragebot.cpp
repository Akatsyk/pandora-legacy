#include "../../source.hpp"

#include "Ragebot.hpp"
#include "LagCompensation.hpp"
#include "../Miscellaneous/Movement.hpp"
#include "../Game/Prediction.hpp"
#include "Autowall.h"
#include "Fakelag.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Utils/Threading/threading.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include "../../SDK/RayTracer.h"
#include "../../SDK/Displacement.hpp"
#include "ShotInformation.hpp"
#include "../Visuals/EventLogger.hpp"
#include "ShotInformation.hpp"
#include "TickbaseShift.hpp"
#include "../Game/SimulationContext.hpp"
#include "../Visuals/ESP.hpp"
#include "Resolver.hpp"
#include "../Visuals/CChams.hpp"

#include <sstream>
// vader.tech invite https://discord.gg/GV6JNW3nze
extern int LastShotTime;

// TODO: 
// Refactoring
// Rework exploits

enum OverrideConditions {
	OnShot,
	Running,
	Walking,
	InAir,
	Standing,
	Backward,
	Sideways,
	InDuck,
	DoubleTap,
};

typedef __declspec( align( 16 ) ) union {
	float f[ 4 ];
	__m128 v;
} m128;

auto round_to_multiple = [ & ] ( int in, int multiple ) {
	const auto ratio = static_cast< double >( in ) / multiple;
	const auto iratio = std::lround( ratio );
	return static_cast< int >( iratio * multiple );
};


__forceinline __m128 sqrt_ps( const __m128 squared ) {
	return _mm_sqrt_ps( squared );
}

struct BoundingBox {
	Vector min, max;
	int idx;

	BoundingBox( void ) { };
	BoundingBox( const Vector& min, const Vector& max, int idx ) :
		min( min ), max( max ), idx( idx ) {
	};
};

struct CapsuleHitbox {
	CapsuleHitbox( ) = default;
	CapsuleHitbox( const Vector& mins, const Vector& maxs, const float radius, int idx )
		: m_mins( mins ), m_maxs( maxs ), m_radius( radius ), m_idx( idx ) {
	}

	Vector m_mins;
	Vector m_maxs;
	float m_radius;
	int m_idx;
};

#define SMALL_NUM   0.00000001 // anything that avoids division overflow
#define dot(u,v)   ((u).x * (v).x + (u).y * (v).y + (u).z * (v).z)
#define norm(v)    sqrt(dot(v,v))  // norm = length of  vector

int convert_hitbox_to_hitgroup( int hitbox ) {
	switch( hitbox ) {
	case HITBOX_HEAD:
	case HITBOX_NECK:
	case HITBOX_LOWER_NECK:
		return Hitgroup_Head;
	case HITBOX_UPPER_CHEST:
	case HITBOX_CHEST:
	case HITBOX_LOWER_CHEST:
	case HITBOX_LEFT_UPPER_ARM:
	case HITBOX_RIGHT_UPPER_ARM:
		return Hitgroup_Chest;
	case HITBOX_PELVIS:
	case HITBOX_LEFT_THIGH:
	case HITBOX_RIGHT_THIGH:
	case HITBOX_STOMACH:
		return Hitgroup_Stomach;
	case HITBOX_LEFT_CALF:
	case HITBOX_LEFT_FOOT:
		return Hitgroup_LeftLeg;
	case HITBOX_RIGHT_CALF:
	case HITBOX_RIGHT_FOOT:
		return Hitgroup_RightLeg;
	case HITBOX_LEFT_FOREARM:
	case HITBOX_LEFT_HAND:
		return Hitgroup_LeftArm;
	case HITBOX_RIGHT_FOREARM:
	case HITBOX_RIGHT_HAND:
		return Hitgroup_RightArm;
	default:
		return Hitgroup_Stomach;
	}
}

void get_edge( Vector& end, const Vector& start, float range, C_CSPlayer* player ) {
	Ray_t ray;
	if( range > 0.0f )
		ray.Init( start, end, Vector( -range, -range, -range ), Vector( range, range, range ) );
	else
		ray.Init( start, end );

	CTraceFilter filter;
	filter.pSkip = player;

	CGameTrace trace;
	Interfaces::m_pEngineTrace->TraceRay( ray, 0x46004003u, &filter, &trace );
	if( trace.fraction <= 0.99f ) {
		end = start + ( ( end - start ) * trace.fraction );
	}
};

namespace Interfaces
{
	bool EyeCompare( const Vector& a, const Vector& b ) {
		auto round_eye = [ ] ( const Vector& vec ) {
			return Vector(
				std::roundf( vec.x * 1000.0f ) / 1000.0f,
				std::roundf( vec.y * 1000.0f ) / 1000.0f,
				std::roundf( vec.z * 1000.0f ) / 1000.0f
			);
		};

		return round_eye( a ) == round_eye( b );
	};

	struct C_AimTarget;
	struct C_AimPoint;

	struct C_AimPoint {
		Vector position;

		float damage = 0.0f;
		float hitchance = 0.0f;
		float pointscale = 0.0f;

		int healthRatio = 0;
		int hitboxIndex = 0;
		int hitgroup = 0;

		bool center = false;
		bool penetrated = false;
		bool isLethal = false;
		bool isHead = false;
		bool isBody = false;
		bool is_should_baim = false;
		bool is_should_headaim = false;

		C_AimTarget* target = nullptr;
		Engine::C_LagRecord* record = nullptr;
	};

	struct C_AimTarget {
		std::vector<C_AimPoint> points;
		std::vector<BoundingBox> obb;
		std::vector<CapsuleHitbox> capsules;
		Engine::C_BaseLagRecord backup;
		Engine::C_LagRecord* record = nullptr;
		C_CSPlayer* player = nullptr;
		bool overrideHitscan = false;
		bool preferHead = false;
		bool preferBody = false;
		bool hasLethal = false;
		bool onlyHead = true;
		bool hasCenter = false;
	};

	struct C_PointsArray {
		C_AimPoint* points = nullptr;
		int pointsCount = 0;
	};

	struct C_HitchanceData {
		Vector direction;
		Vector end;
		bool hit = false;
		bool damageIsAccurate = false;
	};

	struct C_HitchanceArray {
		C_AimPoint* point;
		C_HitchanceData* data;
		int dataCount;
	};

	// hitchance

#ifdef DEBUG_HITCHANCE
	static int g_ClipTrace = 0;
	static int g_Intersection = 0;
	static bool g_PerfectAccuracy = false;
#endif

	struct RagebotData {
		// spread cone
		float m_flSpread;
		float m_flInaccuracy;

		Encrypted_t<CUserCmd> m_pCmd = nullptr;
		C_CSPlayer* m_pLocal = nullptr;
		C_WeaponCSBaseGun* m_pWeapon = nullptr;
		Encrypted_t<CCSWeaponInfo>  m_pWeaponInfo = nullptr;
		bool* m_pSendPacket = nullptr;
		C_CSPlayer* m_pLastTarget = nullptr;

		std::vector<C_AimTarget> m_targets;
		std::vector<C_AimPoint> m_aim_points;
		std::vector<C_AimTarget> m_aim_data;

		C_AimTarget* m_pBestTarget;


		Vector m_vecEyePos;

		// last entity iterated in list
		int m_nIterativeTarget = 0;

		// failed hitchance this tick
		bool m_bFailedHitchance = false;

		// no need to autoscope
		bool m_bNoNeededScope = false;

		bool m_bResetCmd = false;
		bool m_bRePredict = false;
		int m_RestoreZoomLevel = INT_MAX;

		bool m_bPredictedScope = false;

		int m_iChokedCommands = -1;
		int m_iDelay = 0;

		bool m_bDelayedHeadAim = false;

		CVariables::RAGE* rbot = nullptr;

		// delay shot
		Vector m_PeekingPosition;
		float m_flLastPeekTime;

		bool m_bDebugGetDamage = false;

		bool m_bEarlyStop = false;

		// hitchance
		static std::vector<std::tuple<float, float, float>> precomputed_seeds;
	};

	static RagebotData _rage_data;

	class C_Ragebot : public Ragebot {
	public:
		// only sanity checks, etc.
		virtual bool Run( Encrypted_t<CUserCmd> cmd, C_CSPlayer* local, bool* sendPacket );

		virtual bool GetBoxOption( mstudiobbox_t* hitbox, mstudiohitboxset_t* hitboxSet, float& ps, bool override_hitscan ) {
			if( !hitbox )
				return false;

			if( !hitboxSet )
				return false;

			if( hitboxSet->pHitbox( HITBOX_PELVIS ) == hitbox ) {
				ps = m_rage_data->rbot->body_point_scale;
				return override_hitscan ? m_rage_data->rbot->bt_hitboxes_pelvis : m_rage_data->rbot->hitboxes_pelvis;
			}

			if( hitboxSet->pHitbox( HITBOX_RIGHT_FOOT ) == hitbox || hitboxSet->pHitbox( HITBOX_LEFT_FOOT ) == hitbox ) {
				ps = m_rage_data->rbot->point_scale;
				return override_hitscan ? m_rage_data->rbot->bt_hitboxes_feets : m_rage_data->rbot->hitboxes_feets;
			}

			switch( hitbox->group ) {
			case Hitgroup_Head:
				ps = m_rage_data->rbot->point_scale;
				return ( override_hitscan ? m_rage_data->rbot->bt_hitboxes_head : m_rage_data->rbot->hitboxes_head ) && !g_Vars.rage.prefer_body.enabled;
				break;
			case Hitgroup_Neck: // just neck
				ps = m_rage_data->rbot->point_scale;
				return  ( override_hitscan ? m_rage_data->rbot->bt_hitboxes_neck : m_rage_data->rbot->hitboxes_neck ) && !g_Vars.rage.prefer_body.enabled;
				break;
			case Hitgroup_Chest:
				ps = m_rage_data->rbot->point_scale;
				return  override_hitscan ? m_rage_data->rbot->bt_hitboxes_chest : m_rage_data->rbot->hitboxes_chest;
				break;
			case Hitgroup_Stomach:
				ps = m_rage_data->rbot->body_point_scale;
				return  override_hitscan ? m_rage_data->rbot->bt_hitboxes_stomach : m_rage_data->rbot->hitboxes_stomach;
				break;
			case Hitgroup_RightLeg:
			case Hitgroup_LeftLeg:
				if( hitboxSet->pHitbox( HITBOX_RIGHT_FOOT ) != hitbox && hitboxSet->pHitbox( HITBOX_LEFT_FOOT ) != hitbox ) {
					ps = m_rage_data->rbot->point_scale;
					return  override_hitscan ? m_rage_data->rbot->bt_hitboxes_legs : m_rage_data->rbot->hitboxes_legs;
				}
				break;
			case Hitgroup_RightArm:
			case Hitgroup_LeftArm:
				ps = m_rage_data->rbot->point_scale;
				return override_hitscan ? m_rage_data->rbot->bt_hitboxes_arms : m_rage_data->rbot->hitboxes_arms;
				break;
			default:
				return false;
				break;
			}
		};

		// should override condition
		virtual bool OverrideHitscan( C_CSPlayer* player, Engine::C_LagRecord* record );

		// return true if rage enabled
		virtual bool SetupRageOptions( );

		virtual void Multipoint( C_CSPlayer* player, Engine::C_LagRecord* record, int side, std::vector<std::pair<Vector, bool>>& points, mstudiobbox_t* hitbox, mstudiohitboxset_t* hitboxSet, float& ps, int hitboxIndex );
	public:
		C_Ragebot( ) : m_rage_data( &_rage_data ) { };
		virtual ~C_Ragebot( ) { };

	private:
		// run aimbot itself
		virtual bool RunInternal( );

		// aim at point
		virtual bool AimAtPoint( C_AimPoint* bestPoint );

		// get theoretical hit chance
		virtual bool Hitchance( C_AimPoint* point, const Vector& start, float chance );
		virtual bool AccuracyBoost( C_AimPoint* point, const Vector& start, float chance );

		__forceinline bool IsPointAccurate( C_AimPoint* point, const Vector& start );

		virtual void AddPoint( C_CSPlayer* player, Engine::C_LagRecord* record, int side, std::vector<std::pair<Vector, bool>>& points, const Vector& point, mstudiobbox_t* hitbox, mstudiohitboxset_t* hitboxSet, bool isMultipoint );

		virtual std::pair<bool, C_AimPoint> RunHitscan( );

		void ScanPoint( C_AimPoint* pPoint );

		// sort records, choose best and calculate damage
		// return true if target is valid
		__forceinline int GeneratePoints( C_CSPlayer* player, std::vector<C_AimTarget>& aim_targets, std::vector<C_AimPoint>& aim_points );
		virtual Engine::C_LagRecord* GetBestLagRecord( C_CSPlayer* player, Engine::C_BaseLagRecord* backup );

		virtual bool IsRecordValid( C_CSPlayer* player, Engine::C_LagRecord* record );

		__forceinline float GetDamage( C_CSPlayer* player, Vector vecPoint, Engine::C_LagRecord* record, int hitboxIdx, bool bCalculatePoint = false );

		__forceinline bool SetupTargets( );

		__forceinline void SelectBestTarget( );

		Encrypted_t<RagebotData> m_rage_data;
	};

	float C_Ragebot::GetDamage( C_CSPlayer* player, Vector vecPoint, Engine::C_LagRecord* record, int hitboxIdx, bool bCalculatePoint ) {
		auto renderable = player->GetClientRenderable( );
		if( !renderable )
			return 1.0f;

		auto model = player->GetModel( );
		if( !model )
			return 1.0f;

		auto hdr = Interfaces::m_pModelInfo->GetStudiomodel( model );
		if( !hdr )
			return 1.0f;

		auto hitboxSet = hdr->pHitboxSet( player->m_nHitboxSet( ) );
		if( !hitboxSet )
			return -1.0f;

		auto hitbox = hitboxSet->pHitbox( hitboxIdx );
		if( !hitbox )
			return -1.0f;

		Autowall::C_FireBulletData fireData;
		fireData.m_bPenetration = m_rage_data->rbot->autowall;

		matrix3x4_t* pBoneMatrix = player->m_CachedBoneData( ).Base( );
		Vector vecPointCalc = pBoneMatrix[ hitboxSet->pHitbox( hitboxIdx )->bone ].at( 3 );
		Vector dir = bCalculatePoint ? vecPointCalc - m_rage_data->m_vecEyePos : vecPoint - m_rage_data->m_vecEyePos;
		dir.Normalize( );

		if( m_rage_data->m_bDebugGetDamage )
			Interfaces::m_pDebugOverlay->AddBoxOverlay( vecPointCalc, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), QAngle( ), 255, 255, 255, 127, 0.1f );

		fireData.m_vecStart = m_rage_data->m_vecEyePos;
		fireData.m_vecPos = vecPoint;
		fireData.m_vecDirection = dir;
		fireData.m_iHitgroup = hitbox->group;

		fireData.m_Player = m_rage_data->m_pLocal;
		fireData.m_TargetPlayer = player;
		fireData.m_WeaponData = m_rage_data->m_pWeaponInfo.Xor( );
		fireData.m_Weapon = m_rage_data->m_pWeapon;

		const float flDamage = Autowall::FireBullets( &fireData );

		return flDamage;
	}

	bool C_Ragebot::Run( Encrypted_t<CUserCmd> cmd, C_CSPlayer* local, bool* sendPacket ) {
		if( !g_Vars.rage.enabled || !g_Vars.rage.key.enabled )
			return false;

		if( !g_Vars.globals.RandomInit ) {
			return false;
		}

		auto weapon = ( C_WeaponCSBaseGun* )local->m_hActiveWeapon( ).Get( );
		if( !weapon ) {
			return false;
		}

		auto weaponInfo = weapon->GetCSWeaponData( );
		if( !weaponInfo.IsValid( ) ) {
			return false;
		}

		// run aim on zeus
		if( weapon->m_iItemDefinitionIndex( ) != WEAPON_ZEUS
			&& ( weaponInfo->m_iWeaponType == WEAPONTYPE_KNIFE || weaponInfo->m_iWeaponType == WEAPONTYPE_GRENADE || weaponInfo->m_iWeaponType == WEAPONTYPE_C4 ) )
			return false;

		if( !SetupRageOptions( ) )
			return false;

		if( !local || local->IsDead( ) ) {
			m_rage_data->m_pLastTarget = nullptr;
			m_rage_data->m_bFailedHitchance = false;
			return false;
		}

		m_rage_data->m_pLocal = local;
		m_rage_data->m_pWeapon = weapon;
		m_rage_data->m_pWeaponInfo = weaponInfo;
		m_rage_data->m_pSendPacket = sendPacket;
		m_rage_data->m_pCmd = cmd;
		m_rage_data->m_bEarlyStop = false;
		g_Vars.globals.OverridingMinDmg = m_rage_data->rbot->min_damage_override && g_Vars.rage.key_dmg_override.enabled;
		g_Vars.globals.OverridingHitscan = m_rage_data->rbot->override_hitscan && g_Vars.rage.override_key.enabled;

		if( weapon->m_iItemDefinitionIndex( ) == WEAPON_REVOLVER ) {
			if( !( m_rage_data->m_pCmd->buttons & IN_RELOAD ) && weapon->m_iClip1( ) ) {
				static float cockTime = 0.f;
				float curtime = local->m_nTickBase( ) * Interfaces::m_pGlobalVars->interval_per_tick;
				m_rage_data->m_pCmd->buttons &= ~IN_ATTACK2;
				if( m_rage_data->m_pLocal->CanShoot( true ) ) {
					if( cockTime <= curtime ) {
						if( weapon->m_flNextSecondaryAttack( ) <= curtime )
							cockTime = curtime + 0.234375f;
						else
							m_rage_data->m_pCmd->buttons |= IN_ATTACK2;
					}
					else
						m_rage_data->m_pCmd->buttons |= IN_ATTACK;
				}
				else {
					cockTime = curtime + 0.234375f;
					m_rage_data->m_pCmd->buttons &= ~IN_ATTACK;
				}
			}
		}
		else if( !this->m_rage_data->m_pLocal->CanShoot( ) ) {
			if( !m_rage_data->m_pWeaponInfo->m_bFullAuto )
				m_rage_data->m_pCmd->buttons &= ~IN_ATTACK;

			if( Interfaces::m_pGlobalVars->curtime < m_rage_data->m_pLocal->m_flNextAttack( )
				|| m_rage_data->m_pWeapon->m_iClip1( ) < 1 )
				return false;
		}

		if( m_rage_data->m_pLastTarget != nullptr && m_rage_data->m_pLastTarget->IsDead( ) ) {
			m_rage_data->m_pLastTarget = nullptr;
		}

		bool ret = RunInternal( );

		return ret;
	}

	bool C_Ragebot::SetupRageOptions( ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) )
			return false;

		auto weapon = ( C_WeaponCSBaseGun* )local->m_hActiveWeapon( ).Get( );
		if( !weapon )
			return false;

		auto weaponInfo = weapon->GetCSWeaponData( );
		if( !weaponInfo.IsValid( ) )
			return false;

		m_rage_data->m_pWeapon = weapon;
		m_rage_data->m_pWeaponInfo = weaponInfo;

		auto id = weapon->m_iItemDefinitionIndex( );
		if( id == WEAPON_ZEUS )
			return false;

		switch( weaponInfo->m_iWeaponType ) {
		case WEAPONTYPE_PISTOL:
			if( id == WEAPON_DEAGLE || id == WEAPON_REVOLVER )
				m_rage_data->rbot = &g_Vars.rage_heavypistols;
			else
				m_rage_data->rbot = &g_Vars.rage_pistols;
			break;
		case WEAPONTYPE_SUBMACHINEGUN:
			m_rage_data->rbot = &g_Vars.rage_smgs;
			break;
		case WEAPONTYPE_RIFLE:
			m_rage_data->rbot = &g_Vars.rage_rifles;
			break;
		case WEAPONTYPE_SHOTGUN:
			m_rage_data->rbot = &g_Vars.rage_shotguns;
			break;
		case WEAPONTYPE_SNIPER_RIFLE:
			if( id == WEAPON_G3SG1 || id == WEAPON_SCAR20 )
				m_rage_data->rbot = &g_Vars.rage_autosnipers;
			else
				m_rage_data->rbot = ( id == WEAPON_AWP ) ? &g_Vars.rage_awp : &g_Vars.rage_scout;
			break;
		case WEAPONTYPE_MACHINEGUN:
			m_rage_data->rbot = &g_Vars.rage_heavys;
			break;
		default:
			m_rage_data->rbot = &g_Vars.rage_default;
			break;
		}

		if( !m_rage_data->rbot )
			return false;

		if( !m_rage_data->rbot->active ) {
			m_rage_data->rbot = &g_Vars.rage_default;
		}

		return m_rage_data->rbot->active;
	}

	bool C_Ragebot::RunInternal( ) {
		auto cmd_backup = *m_rage_data->m_pCmd.Xor( );

		//m_rage_data->m_bResetCmd = true;
		m_rage_data->m_bRePredict = false;
		m_rage_data->m_bPredictedScope = false;
		m_rage_data->m_bNoNeededScope = true;

		m_rage_data->m_flSpread = Engine::Prediction::Instance( )->GetSpread( );
		m_rage_data->m_flInaccuracy = Engine::Prediction::Instance( )->GetInaccuracy( );

		//ILoggerEvent::Get( )->PushEvent( std::to_string( m_rage_data->m_flInaccuracy ), FloatColor::White, true, "debug" );

		auto success = RunHitscan( );
		//if( success.first ) {
		//	m_rage_data->m_bResetCmd = false;
		//}

		const bool bOnLand = !( Engine::Prediction::Instance( ).GetFlags( ) & FL_ONGROUND ) && m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND;

		// if failed at hitchancing or aimbot good
		float feet = 0.f;
		if( ( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SCAR20 || m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_G3SG1 ) && success.second.target && success.second.target->player ) {
			auto dist = ( m_rage_data->m_pLocal->m_vecOrigin( ).Distance( success.second.target->player->m_vecOrigin( ) ) );
			auto meters = dist * 0.0254f;
			feet = round_to_multiple( meters * 3.281f, 5 );
		}

		bool htcFailed = m_rage_data->m_bFailedHitchance;
		if( m_rage_data->m_bFailedHitchance ) {

			if( m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND && !bOnLand ) {
				if( m_rage_data->rbot->autostop_check && !g_Vars.globals.bMoveExploiting ) {
					if( !g_Vars.globals.Fakewalking ) {
						Interfaces::Movement::Get( )->InstantStop( );

						auto RemoveButtons = [ & ] ( int key ) { m_rage_data->m_pCmd->buttons &= ~key; };
						RemoveButtons( IN_MOVERIGHT );
						RemoveButtons( IN_MOVELEFT );
						RemoveButtons( IN_FORWARD );
						RemoveButtons( IN_BACK );
					}

					//m_rage_data->m_bResetCmd = false;
				}
			}

			m_rage_data->m_bFailedHitchance = false;

			m_rage_data->m_bNoNeededScope = false;
		}

		if( feet > 0.f ) {
			if( feet <= 60.f ) {
				m_rage_data->m_bNoNeededScope = true;
			}
			else {
				m_rage_data->m_bNoNeededScope = false;
			}

			if( g_Vars.misc.fakeduck && g_Vars.misc.fakeduck_bind.enabled )
				m_rage_data->m_bNoNeededScope = false;
		}

		if( m_rage_data->rbot->autoscope &&
			m_rage_data->m_pWeaponInfo->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE &&
			m_rage_data->m_pWeapon->m_zoomLevel( ) <= 0 &&
			m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND &&
			!m_rage_data->m_bNoNeededScope ) {
			m_rage_data->m_pCmd->buttons |= IN_ATTACK2;
			m_rage_data->m_pCmd->buttons &= ~IN_ATTACK;
			//m_rage_data->m_pWeapon->m_zoomLevel( ) = 1;
			m_rage_data->m_bPredictedScope = true;
			m_rage_data->m_bRePredict = true;
			//m_rage_data->m_bResetCmd = false;
		}

		auto correction = m_rage_data->m_pLocal->m_aimPunchAngle( ) * g_Vars.weapon_recoil_scale->GetFloat( );

		m_rage_data->m_pCmd->viewangles -= correction;
		m_rage_data->m_pCmd->viewangles.Normalize( );

		if( m_rage_data->rbot->compensate_spread && !g_Vars.misc.anti_untrusted
			&& ( g_Vars.globals.m_iServerType > 7 ) && m_rage_data->m_pLocal->CanShoot( ) && m_rage_data->m_pCmd->buttons & IN_ATTACK ) {
			m_rage_data->m_bResetCmd = false;

			auto weapon_inaccuracy = Engine::Prediction::Instance( )->GetInaccuracy( );
			auto weapon_spread = Engine::Prediction::Instance( )->GetSpread( );

			auto random_seed = m_rage_data->m_pCmd->random_seed & 149;

			auto rand1 = g_Vars.globals.SpreadRandom[ random_seed ].flRand1;
			auto rand_pi1 = g_Vars.globals.SpreadRandom[ random_seed ].flRandPi1;
			auto rand2 = g_Vars.globals.SpreadRandom[ random_seed ].flRand2;
			auto rand_pi2 = g_Vars.globals.SpreadRandom[ random_seed ].flRandPi2;

			int id = m_rage_data->m_pWeapon->m_iItemDefinitionIndex( );
			auto recoil_index = m_rage_data->m_pWeapon->m_flRecoilIndex( );

			if( id == 64 ) {
				if( m_rage_data->m_pCmd->buttons & IN_ATTACK2 ) {
					rand1 = 1.0f - rand1 * rand1;
					rand2 = 1.0f - rand2 * rand2;
				}
			}
			else if( id == 28 && recoil_index < 3.0f ) {
				for( int i = 3; i > recoil_index; i-- ) {
					rand1 *= rand1;
					rand2 *= rand2;
				}

				rand1 = 1.0f - rand1;
				rand2 = 1.0f - rand2;
			}

			auto rand_inaccuracy = rand1 * weapon_inaccuracy;
			auto rand_spread = rand2 * weapon_spread;

			Vector2D spread =
			{
				std::cos( rand_pi1 ) * rand_inaccuracy + std::cos( rand_pi2 ) * rand_spread,
				std::sin( rand_pi1 ) * rand_inaccuracy + std::sin( rand_pi2 ) * rand_spread,
			};

			// 
			// pitch/yaw/roll
			// 

			Vector side, up;
			Vector forward = QAngle::Zero.ToVectors( &side, &up );

			Vector direction = ( forward + ( side * spread.x ) + ( up * spread.y ) );

			QAngle angles_spread = direction.ToEulerAngles( );

			angles_spread.x -= m_rage_data->m_pCmd->viewangles.x;
			angles_spread.Normalize( );

			forward = angles_spread.ToVectorsTranspose( &side, &up );

			angles_spread = ( forward.ToEulerAngles( &up ) );

			angles_spread.y += m_rage_data->m_pCmd->viewangles.y;
			angles_spread.Normalize( );

			m_rage_data->m_pCmd->viewangles = angles_spread;
		}


		if( m_rage_data->m_bResetCmd ) {
			*m_rage_data->m_pCmd.Xor( ) = cmd_backup;
		}
		else {
			if( m_rage_data->m_bRePredict )
				Engine::Prediction::Instance( )->Repredict( );
		}

		return success.first;
	}

	void C_Ragebot::AddPoint( C_CSPlayer* player, Engine::C_LagRecord* record, int side, std::vector<std::pair<Vector, bool>>& points, const Vector& point, mstudiobbox_t* hitbox, mstudiohitboxset_t* hitboxSet, bool isMultipoint ) {
		auto pointTransformed = point;
		if( !hitbox )
			return;

		if( !hitboxSet )
			return;

		points.push_back( std::make_pair( pointTransformed, isMultipoint ) );
	}

	static int ClipRayToHitbox( const Ray_t& ray, mstudiobbox_t* hitbox, matrix3x4_t& matrix, CGameTrace& trace )
	{
		static auto fn = Memory::Scan( XorStr( "client.dll" ), XorStr( "55 8B EC 83 E4 F8 F3 0F 10 42" ) );

		if( !fn || !hitbox )
			return -1;

		trace.fraction = 1.0f;
		trace.startsolid = false;

		return reinterpret_cast < int( __fastcall* )( const Ray_t&, mstudiobbox_t*, matrix3x4_t&, CGameTrace& ) > ( fn )( ray, hitbox, matrix, trace );
	}

	bool C_Ragebot::Hitchance( C_AimPoint* pPoint, const Vector& vecStart, float flChance ) {
		if( flChance <= 0.0f )
			return true;

		if( !pPoint )
			return false;

		if( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SCAR20 || m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_G3SG1 ) {
			if( ( m_rage_data->m_flInaccuracy <= 0.0380f ) && m_rage_data->m_pWeapon->m_zoomLevel( ) == 0 ) {
				pPoint->hitchance = 50.f;
				return true;
			}
		}

		if( ( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SSG08 || m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_REVOLVER ) && ( !m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND ) ) {
			if( ( m_rage_data->m_flInaccuracy < 0.009f ) ) {
				pPoint->hitchance = 75.f;
				return true;
			}
		}

		// performance optimization.
		if( ( g_Vars.globals.m_vecFixedEyePosition - pPoint->position ).Length( ) > m_rage_data->m_pWeaponInfo->m_flWeaponRange )
			return false;

		Vector forward = pPoint->position - vecStart;
		forward.Normalize( );

		Vector right, up;
		forward.GetVectors( right, up );

		auto pRenderable = pPoint->target->player->GetClientRenderable( );
		if( !pRenderable )
			return false;

		auto pModel = pRenderable->GetModel( );
		if( !pModel )
			return false;

		auto pHdr = Interfaces::m_pModelInfo->GetStudiomodel( pModel );
		if( !pHdr )
			return false;

		auto pHitboxSet = pHdr->pHitboxSet( pPoint->target->player->m_nHitboxSet( ) );

		if( !pHitboxSet )
			return false;

		auto pHitbox = pHitboxSet->pHitbox( pPoint->hitboxIndex );

		if( !pHitbox )
			return false;

		matrix3x4_t* pMatrix = pPoint->record->GetBoneMatrix( );
		if( !pMatrix )
			return false;

		const auto maxTraces = 128;
		auto hits = 0;
		CGameTrace tr;
		for( int i = 0; i < maxTraces; ++i ) {
			float flRand1 = g_Vars.globals.SpreadRandom[ i ].flRand1;
			float flRandPi1 = g_Vars.globals.SpreadRandom[ i ].flRandPi1;
			float flRand2 = g_Vars.globals.SpreadRandom[ i ].flRand2;
			float flRandPi2 = g_Vars.globals.SpreadRandom[ i ].flRandPi2;

			float m_flRecoilIndex = m_rage_data->m_pWeapon->m_flRecoilIndex( );
			if( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_REVOLVER ) {
				flRand1 = 1.0f - flRand1 * flRand1;
				flRand2 = 1.0f - flRand2 * flRand2;
			}
			else if( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_NEGEV && m_flRecoilIndex < 3.f ) {
				for( int x = 3; x > m_flRecoilIndex; --x ) {
					flRand1 *= flRand1;
					flRand2 *= flRand2;
				}

				flRand1 = 1.f - flRand1;
				flRand2 = 1.f - flRand2;
			}

			float flRandInaccuracy = flRand1 * m_rage_data->m_flInaccuracy;
			float flRandSpread = flRand2 * m_rage_data->m_flSpread;

			float flRandPi1Cos, flRandPi1Sin;
			DirectX::XMScalarSinCos( &flRandPi1Sin, &flRandPi1Cos, flRandPi1 );

			float flRandPi2Cos, flRandPi2Sin;
			DirectX::XMScalarSinCos( &flRandPi2Sin, &flRandPi2Cos, flRandPi2 );

			float spread_x = flRandPi1Cos * flRandInaccuracy + flRandPi2Cos * flRandSpread;
			float spread_y = flRandPi1Sin * flRandInaccuracy + flRandPi2Sin * flRandSpread;

			Vector direction;
			direction.x = forward.x + ( spread_x * right.x ) + ( spread_y * up.x );
			direction.y = forward.y + ( spread_x * right.y ) + ( spread_y * up.y );
			direction.z = forward.z + ( spread_x * right.z ) + ( spread_y * up.z );

			Vector end = vecStart + direction * m_rage_data->m_pWeaponInfo->m_flWeaponRange;

			CGameTrace trace;
			Ray_t ray;
			ray.Init( m_rage_data->m_vecEyePos, end );

			auto bHit = ClipRayToHitbox( ray, pHitbox, pMatrix[ pHitbox->bone ], trace ) >= 0;
			if( bHit ) {
				hits++;
			}

			// abort if we can no longer reach hitchance.
			if( static_cast< float >( hits + maxTraces - i ) / static_cast< float >( maxTraces ) < flChance ) {
				pPoint->hitchance = 0.f;
				return false;
			}
		}

		float hc{ };
		if( hits ) {
			hc = static_cast< float >( hits ) / static_cast< float >( maxTraces );
		}
		else {
			hc = 0.f;
		}

		pPoint->hitchance = hc * 100.f;

		return hc >= flChance;
	}

	bool C_Ragebot::AccuracyBoost( C_AimPoint* pPoint, const Vector& vecStart, float flChance ) {
		if( !pPoint )
			return false;

		if( flChance <= 0.0f )
			return true;

		if( m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_SCAR20 || m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_G3SG1 ) {
			if( ( m_rage_data->m_flInaccuracy <= 0.0380f ) && m_rage_data->m_pWeapon->m_zoomLevel( ) == 0 ) {
				return true;
			}
		}

		auto pModel = pPoint->target->player->GetClientRenderable( )->GetModel( );
		if( !pModel )
			return false;

		auto pHdr = Interfaces::m_pModelInfo->GetStudiomodel( pModel );
		if( !pHdr )
			return false;

		auto pHitboxSet = pHdr->pHitboxSet( pPoint->target->player->m_nHitboxSet( ) );

		if( !pHitboxSet )
			return false;

		auto pHitbox = pHitboxSet->pHitbox( pPoint->hitboxIndex );

		if( !pHitbox )
			return false;

		int iHits = 0;
		int iAutowalledHits = 0;

		const bool bIsCapsule = pHitbox->m_flRadius != -1.0f;

		Vector vecForward = pPoint->position - vecStart;
		vecForward.Normalize( );

		Vector vecRight, vecUp;
		vecForward.GetVectors( vecRight, vecUp );

		// calculate simple spread
		const float flSpread = m_rage_data->m_flSpread + m_rage_data->m_flInaccuracy;

		// get hitbox radius
		const float flHitboxRadius = pHitbox->m_flRadius;

		// get resolved matrix
		matrix3x4_t* pMatrix = pPoint->record->GetBoneMatrix( );

		// get min and max hitbox vectors
		auto vecMin = pHitbox->bbmin.Transform( pMatrix[ pHitbox->bone ] );
		auto vecMax = pHitbox->bbmax.Transform( pMatrix[ pHitbox->bone ] );

		if( this->m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) == WEAPON_REVOLVER )
			flChance = 0.07f;

		CGameTrace tr;

		// iterate all traces 
		for( int i = 1; i <= 6; i++ ) {
			for( int j = 0; j < 8; ++j ) {
				float flCalculatedSpread = flSpread * float( float( i ) / 6.f );

				// TODO: cache it
				float flDirCos, flDirSin;
				DirectX::XMScalarSinCos( &flDirCos, &flDirSin, DirectX::XM_2PI * float( float( j ) / 8.0f ) );

				// calculate spread
				Vector2D vecSpread;
				vecSpread.x = flDirCos * flCalculatedSpread;
				vecSpread.y = flDirSin * flCalculatedSpread;

				// calculate direction
				Vector vecDirection;
				vecDirection.x = ( vecForward.x + vecSpread.x * vecRight.x + vecSpread.y * vecUp.x );
				vecDirection.y = ( vecForward.y + vecSpread.x * vecRight.y + vecSpread.y * vecUp.y );
				vecDirection.z = ( vecForward.z + vecSpread.x * vecRight.z + vecSpread.y * vecUp.z );

				// calculate end point
				Vector vecEnd = vecStart + vecDirection * m_rage_data->m_pWeaponInfo->m_flWeaponRange;

				//if( !bIsCapsule ) {
				//	Interfaces::m_pEngineTrace->ClipRayToEntity( Ray_t( m_rage_data->m_vecEyePos, vecEnd ), MASK_SHOT, pPoint->target->player, &tr );
				//}

				auto bHit = bIsCapsule ?
					Math::IntersectSegmentToSegment( m_rage_data->m_vecEyePos, vecEnd, vecMin, vecMax, flHitboxRadius ) : Math::IntersectionBoundingBox( m_rage_data->m_vecEyePos, vecEnd, vecMin, vecMax ); //: ( tr.hit_entity == pPoint->target->player && ( tr.hitgroup >= Hitgroup_Head && tr.hitgroup <= Hitgroup_RightLeg ) || tr.hitgroup == Hitgroup_Gear );

				if( bHit ) {
					iHits++;

					Autowall::C_FireBulletData fireData;
					fireData.m_bPenetration = true;

					fireData.m_vecStart = this->m_rage_data->m_vecEyePos;
					fireData.m_vecDirection = vecDirection;
					fireData.m_iHitgroup = convert_hitbox_to_hitgroup( pPoint->hitboxIndex );
					fireData.m_Player = this->m_rage_data->m_pLocal;
					fireData.m_TargetPlayer = pPoint->target->player;
					fireData.m_WeaponData = this->m_rage_data->m_pWeaponInfo.Xor( );
					fireData.m_Weapon = this->m_rage_data->m_pWeapon;

					const float flDamage = Autowall::FireBullets( &fireData );
					if( flDamage >= 1.0f ) {
						if( this->m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_SSG08 && this->m_rage_data->m_pWeapon->m_iItemDefinitionIndex( ) != WEAPON_AWP ) {
							iAutowalledHits++;
							continue;
						}

						int iHealthRatio = int( float( pPoint->target->player->m_iHealth( ) ) / flDamage ) + 1;
						if( iHealthRatio <= pPoint->healthRatio ) {
							iAutowalledHits++;
						}
					}
				}
			}
		}

		constexpr int iAllTraces = 48;

		if( iHits > 0 && iAutowalledHits > 0 ) {
			// calculate hitchance based on hits
			const float newHitchance = float( iHits ) / iAllTraces;

			// we no have good hitchance
			if( newHitchance < flChance )
				return false;

			// calculate hitchance based on autowalled hits
			const float flCalculatedChance = float( iAutowalledHits ) / iAllTraces;

			return flCalculatedChance >= flChance;
		}

		return false;
	}

	bool C_Ragebot::IsPointAccurate( C_AimPoint* point, const Vector& start ) {
		if( !m_rage_data->m_pLocal->CanShoot( ) )
			return false;

		//if( m_rage_data->rbot->shotdelay ) {
		//	float delay = m_rage_data->rbot->shotdelay_amount * 0.01f;
		//	float nextShotTime = this->m_rage_data->m_pWeaponInfo->m_flUnknownFloat0 + TICKS_TO_TIME( LastShotTime );
		//	if( ( ( ( m_rage_data->m_pWeaponInfo->m_flUnknownFloat0 * delay )
		//		+ ( m_rage_data->m_pWeaponInfo->m_flUnknownFloat0 * delay ) ) + nextShotTime ) > TICKS_TO_TIME( Interfaces::m_pGlobalVars->tickcount ) ) {
		//		m_rage_data->m_pCmd->buttons &= ~IN_ATTACK;
		//		return false;
		//	}
		//}

		if( m_rage_data->rbot->delay_shot_on_unducking && m_rage_data->m_pLocal->m_flDuckAmount( ) >= 0.125f ) {
			if( g_Vars.globals.m_flPreviousDuckAmount > m_rage_data->m_pLocal->m_flDuckAmount( ) ) {
				return false;
			}
		}

		auto dist = ( m_rage_data->m_pLocal->m_vecOrigin( ).Distance( point->target->player->m_vecOrigin( ) ) );
		auto meters = dist * 0.0254f;
		float flDistanceInFeet = round_to_multiple( meters * 3.281f, 5 );

		int iHealth = point->target->player->m_iHealth( );
		bool bOnLand = !( Engine::Prediction::Instance( ).GetFlags( ) & FL_ONGROUND ) && m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND;

		auto ShouldHitchance = [ & ] ( ) {
			// nospread enabled
			if( m_rage_data->m_flSpread == 0.0f || m_rage_data->m_flInaccuracy == 0.0f )
				return false;

			const auto weapon_id = m_rage_data->m_pWeapon->m_iItemDefinitionIndex( );
			const auto crouched = m_rage_data->m_pLocal->m_fFlags( ) & FL_DUCKING;
			const auto sniper = m_rage_data->m_pWeaponInfo->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE;
			const auto round_acc = [ ] ( const float accuracy ) { return roundf( accuracy * 1000.f ) / 1000.f; };

			float rounded_acc = round_acc( m_rage_data->m_flInaccuracy );
			float maxSpeed = m_rage_data->m_pLocal->GetMaxSpeed( );

			// no need for hitchance, if we can't increase it anyway.
			if( crouched ) {
				if( rounded_acc == round_acc( sniper ? m_rage_data->m_pWeaponInfo->m_flInaccuracyCrouchAlt : m_rage_data->m_pWeaponInfo->m_flInaccuracyCrouch ) ) {
					return false;
				}
			}
			else {
				if( m_rage_data->m_pLocal->m_fFlags( ) & FL_ONGROUND && !bOnLand ) {

					if( rounded_acc == round_acc( sniper ? m_rage_data->m_pWeaponInfo->m_flInaccuracyStandAlt : m_rage_data->m_pWeaponInfo->m_flInaccuracyStand ) ) {
						return false;
					}
				}
			}

			return true;
		};

		float hitchance = m_rage_data->rbot->hitchance;

		//if( g_TickbaseController.Using( ) ) {
		//	if( hitchance != m_rage_data->rbot->doubletap_hitchance ) {
		//		//ILoggerEvent::Get( )->PushEvent( "used dt hitchance", FloatColor::White, true, "dt" );
		//		hitchance = m_rage_data->rbot->doubletap_hitchance;
		//	}
		//}

		float m_flAccuracyBoostHitchance;
		if( m_rage_data->rbot->accry_boost_on_shot ) {
			/*
			* tmr we are gonna re-work our accry boost seletion.
			  we are gonna add 3 modes
			  -> low
			  -> medium
			  -> high
			  each mode contains a certain amount of hitchance to check if the player behind a wall is accurate enough to fire at.

			  note - vio;
			  float m_flAcrryboostHitchance = 0.f;
			  for each mode on a switch statement
			  set the hitchance to the corrent amount.
			  also detect wat wpn we are holding and choose the best amount.
			  after that is done we should be able to overide our accryboosthitchance to our current hitchance
			  this is done below
			  if( hitchance != m_rage_data->rbot->doubletap_hitchance )
					hitchance = m_rage_data->rbot->doubletap_hitchance;
			*/
			switch( m_rage_data->rbot->accry_boost_on_shot_modes ) {
			case 0: // low
				m_flAccuracyBoostHitchance = 35.f;
				break;
			case 1: // medium
				m_flAccuracyBoostHitchance = 55.f;
				break;
			case 2: // high
				m_flAccuracyBoostHitchance = 75.f;
				break;
			default:
				break;
			}
		}

		// we can hitchance them & check if accry boost is valid.
		if( hitchance > 0.0f && ShouldHitchance( )  ) {
			// we cannot hitchance the player or no valid acrry boost then we failed hitchance.
			if( !Hitchance( point, start, hitchance * 0.01f ) /*|| AccuracyBoost( point, start, m_flAccuracyBoostHitchance * 0.01f ) )*/ || ( ( m_rage_data->m_flInaccuracy + m_rage_data->m_flSpread > ( 0.04f - ( m_rage_data->rbot->doubletap_hitchance / 3000 ) ) && !m_rage_data->m_pWeaponInfo->m_iWeaponType == WEAPONTYPE_SNIPER_RIFLE ) ) ) {
				m_rage_data->m_bFailedHitchance = true;
				return false;
			}
		}

		m_rage_data->m_bFailedHitchance = false;
		return true;
	}

	void C_Ragebot::Multipoint( C_CSPlayer* player, Engine::C_LagRecord* record, int side, std::vector<std::pair<Vector, bool>>& points, mstudiobbox_t* hitbox, mstudiohitboxset_t* hitboxSet, float& pointScale, int hitboxIndex ) {
		auto boneMatrix = record->GetBoneMatrix( );

		if( !hitbox || !boneMatrix )
			return;

		if( !hitboxSet )
			return;

		Vector center = ( hitbox->bbmax + hitbox->bbmin ) * 0.5f;
		Vector centerTrans = center.Transform( boneMatrix[ hitbox->bone ] );

		// the center of feet ain't optimal, we are adding a better point later.
		AddPoint( player, record, side, points,
			centerTrans,
			hitbox, hitboxSet, false
		);

		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local || local->IsDead( ) )
			return;

		if( !m_rage_data->rbot->static_point_scale && m_rage_data->m_pWeapon && hitbox->m_flRadius > 0.0f ) {
			pointScale = 0.91f; // we can go high here because the new multipoint is perfect

			float spreadCone = Engine::Prediction::Instance( )->GetSpread( ) + Engine::Prediction::Instance( )->GetInaccuracy( );
			float dist = centerTrans.Distance( m_rage_data->m_vecEyePos );

			dist /= sinf( DEG2RAD( 90.0f - RAD2DEG( spreadCone ) ) );

			spreadCone = sinf( spreadCone );

			float radiusScaled = ( hitbox->m_flRadius - ( dist * spreadCone ) );
			if( radiusScaled < 0.0f ) {
				radiusScaled *= -1.f;
			}

			float ps = pointScale;
			pointScale = ( radiusScaled / hitbox->m_flRadius );
			pointScale = Math::Clamp( pointScale, 0.0f, ps );
		}

		if( pointScale <= 0.0f )
			return;

		if( hitbox->m_flRadius <= 0.0f ) {
			if( hitboxIndex == HITBOX_RIGHT_FOOT || hitboxIndex == HITBOX_LEFT_FOOT ) {
				float d1 = ( hitbox->bbmin.z - center.z ) * 0.425f;

				if( hitboxIndex == HITBOX_LEFT_FOOT )
					d1 *= -1.f;

				// optimal point for feet
				/*AddPoint( player, record, side, points,
					Vector( center.x, center.y, center.z + d1 ).Transform( boneMatrix[ hitbox->bone ] ),
					hitbox, hitboxSet, true
				);*/

				if( m_rage_data->rbot->mp_hitboxes_feets ) {
					// toe
					AddPoint( player, record, side, points,
						Vector( ( ( hitbox->bbmax.x - center.x ) * pointScale ) + center.x, center.y, center.z ).Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);

					// heel
					AddPoint( player, record, side, points,
						Vector( ( ( hitbox->bbmin.x - center.x ) * pointScale ) + center.x, center.y, center.z ).Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);
				}
			}
		}
		else {
			float r = hitbox->m_flRadius * pointScale;

			if( hitboxIndex == HITBOX_HEAD && !m_rage_data->m_bDelayedHeadAim ) {

				// always adding these (they suck)
				Vector right{ hitbox->bbmax.x, hitbox->bbmax.y, hitbox->bbmax.z + ( hitbox->m_flRadius * 0.5f ) };
				AddPoint( player, record, side, points,
					right.Transform( boneMatrix[ hitbox->bone ] ),
					hitbox, hitboxSet, true
				);

				Vector left{ hitbox->bbmax.x, hitbox->bbmax.y, hitbox->bbmax.z - ( hitbox->m_flRadius * 0.5f ) };
				AddPoint( player, record, side, points,
					left.Transform( boneMatrix[ hitbox->bone ] ),
					hitbox, hitboxSet, true
				);

				//Interfaces::m_pDebugOverlay->AddTextOverlay( front.Transform( boneMatrix[ hitbox->bone ] ), Interfaces::m_pGlobalVars->interval_per_tick * 2, "front" );

				if( m_rage_data->rbot->mp_hitboxes_head ) {
					constexpr float rotation = 0.70710678f;

					// ok, this looks ghetto as shit but we have to clamp these to not have these be off too much
					pointScale = std::clamp<float>( pointScale, 0.1f, 0.95f );
					r = hitbox->m_flRadius * pointScale;

					// top/back 45 deg.
					// this is the best spot to shoot at (when peeking, when RESOLVED XD!)
					AddPoint( player, record, side, points,
						Vector( hitbox->bbmax.x + ( rotation * r ), hitbox->bbmax.y + ( -rotation * r ), hitbox->bbmax.z ).Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);

					//Interfaces::m_pDebugOverlay->AddTextOverlay( Vector( hitbox->bbmax.x + ( rotation * r ), hitbox->bbmax.y + ( -rotation * r ), hitbox->bbmax.z ).Transform( boneMatrix[ hitbox->bone ] ), Interfaces::m_pGlobalVars->interval_per_tick * 2, "front" );

					// ok, this looks ghetto as shit but we have to clamp these to not have these be off too much
					pointScale = std::clamp<float>( pointScale, 0.1f, 0.95f );
					r = hitbox->m_flRadius * pointScale;

					Vector back{ center.x, hitbox->bbmax.y - r, center.z };
					AddPoint( player, record, side, points,
						back.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);
				}
			}

			else if( hitboxIndex == HITBOX_STOMACH || hitboxIndex == HITBOX_PELVIS ) {
				if( m_rage_data->rbot->mp_hitboxes_stomach ) {
					Vector back{ center.x, hitbox->bbmax.y - r, center.z };
					AddPoint( player, record, side, points,
						back.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);

					Vector right{ hitbox->bbmax.x, hitbox->bbmax.y, hitbox->bbmax.z + ( hitbox->m_flRadius * 0.5f ) };
					AddPoint( player, record, side, points,
						right.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);

					Vector left{ hitbox->bbmax.x, hitbox->bbmax.y, hitbox->bbmax.z - ( hitbox->m_flRadius * 0.5f ) };
					AddPoint( player, record, side, points,
						left.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);
				}
			}

			else if( hitboxIndex == HITBOX_LOWER_CHEST || hitboxIndex == HITBOX_CHEST || hitboxIndex == HITBOX_UPPER_CHEST ) {
				if( m_rage_data->rbot->mp_hitboxes_chest ) {
					Vector back{ center.x, hitbox->bbmax.y - r, center.z };
					AddPoint( player, record, side, points,
						back.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);
				}
			}
			else if( hitboxIndex == HITBOX_RIGHT_THIGH || hitboxIndex == HITBOX_LEFT_THIGH ) {
				if( m_rage_data->rbot->mp_hitboxes_legs ) {

					Vector half_bottom{ hitbox->bbmax.x - ( hitbox->m_flRadius * 0.5f ), hitbox->bbmax.y, hitbox->bbmax.z };
					AddPoint( player, record, side, points,
						half_bottom.Transform( boneMatrix[ hitbox->bone ] ),
						hitbox, hitboxSet, true
					);
				}
			}
		}
	}

	bool C_Ragebot::SetupTargets( ) {
		m_rage_data->m_targets.clear( );
		m_rage_data->m_aim_points.clear( );
		m_rage_data->m_aim_data.clear( );
		m_rage_data->m_pBestTarget = nullptr;

		for( int idx = 1; idx <= Interfaces::m_pGlobalVars->maxClients; ++idx ) {
			auto player = C_CSPlayer::GetPlayerByIndex( idx );
			if( !player || player == m_rage_data->m_pLocal || player->IsDead( ) || player->m_bGunGameImmunity( ) || player->IsTeammate( m_rage_data->m_pLocal ) )
				continue;

			if( !player->IsDormant( ) )
				GeneratePoints( player, m_rage_data->m_targets, m_rage_data->m_aim_points );
		}

		if( m_rage_data->m_targets.empty( ) )
			return false;

		if( m_rage_data->m_aim_points.empty( ) ) {

			for( auto& target : m_rage_data->m_targets )
				target.backup.Apply( target.player );

			return false;
		}

		for( auto& target : m_rage_data->m_targets ) {
			std::vector<C_AimPoint> tempPoints;

			for( auto& point : m_rage_data->m_aim_points ) {
				if( point.target->player->EntIndex( ) == target.player->EntIndex( ) )
					tempPoints.emplace_back( point );
			}

			if( !tempPoints.empty( ) ) {
				target.record->Apply( target.player );

				// scan all valid points
				for( size_t i = 0u; i < tempPoints.size( ); ++i )
					ScanPoint( &tempPoints.at( i ) );

				target.backup.Apply( target.player );

				std::vector<C_AimPoint> finalPoints;

				if( !finalPoints.empty( ) )
					finalPoints.clear( );

				int hp = target.player->m_iHealth( );

				for( auto& p : tempPoints ) {
					if( p.damage > 1.0f ) {
						if( !p.isHead ) {
							if( p.damage >= hp ) {
								p.isLethal = true;
								target.hasLethal = true;
							}

							target.onlyHead = false;
						}

						if( p.center ) {
							target.hasCenter = true;
						}

						// is this point valid? fuck yeah, let's push back
						finalPoints.push_back( p );
						continue;
					}
				}

				// don't even bother adding an entry if there are no valid aimpoints
				if( !finalPoints.empty( ) ) {
					target.points = finalPoints;

					// add a valid entry
					m_rage_data->m_aim_data.emplace_back( target );
				}
			}
		}

		m_rage_data->m_aim_points.clear( );

		if( m_rage_data->m_aim_data.empty( ) ) {
			for( auto& target : m_rage_data->m_targets )
				target.backup.Apply( target.player );

			return false;
		}

		SelectBestTarget( );

		if( m_rage_data->m_pBestTarget && !m_rage_data->m_aim_points.empty( ) )
			return true;

		// backup targets
		for( auto& target : m_rage_data->m_targets )
			target.backup.Apply( target.player );

		return false;
	}

	void C_Ragebot::SelectBestTarget( )
	{
		if( m_rage_data->m_aim_data.empty( ) )
			return;

		auto CheckTargets = [ & ] ( C_AimTarget* a, C_AimTarget* b ) -> bool {
			if( !a || !b )
				goto fuck_yeah;

			if( m_rage_data->m_pLastTarget != nullptr && !m_rage_data->m_pLastTarget->IsDead( ) && a->player->EntIndex( ) == m_rage_data->m_pLastTarget->EntIndex( ) )
				return true;

			switch( m_rage_data->rbot->target_selection ) {
			case SELECT_HIGHEST_DAMAGE: {
				float damageFirstTarget, damageSecondTarget;

				for( auto& p : a->points )
					damageFirstTarget += p.damage;

				for( auto& p : b->points )
					damageSecondTarget += p.damage;

				return damageFirstTarget > damageSecondTarget;
				break;
			}
			case SELECT_FOV: {
				float fovToFirstTarget, fovToSecondTarget;

				auto first = a->player->WorldSpaceCenter( );
				auto second = b->player->WorldSpaceCenter( );

				fovToFirstTarget = Math::GetFov( m_rage_data->m_pCmd->viewangles, m_rage_data->m_vecEyePos, first );
				fovToSecondTarget = Math::GetFov( m_rage_data->m_pCmd->viewangles, m_rage_data->m_vecEyePos, second );

				return fovToFirstTarget < fovToSecondTarget;
				break;
			}
			case SELECT_LOWEST_HP:
				return a->player->m_iHealth( ) <= b->player->m_iHealth( ); break;
			case SELECT_LOWEST_DISTANCE:
				return a->player->m_vecOrigin( ).Distance( m_rage_data->m_pLocal->m_vecOrigin( ) ) <= b->player->m_vecOrigin( ).Distance( m_rage_data->m_pLocal->m_vecOrigin( ) ); break;
			case SELECT_LOWEST_PING:
				return ( *Interfaces::m_pPlayerResource.Xor( ) )->GetPlayerPing( a->player->EntIndex( ) ) <= ( *Interfaces::m_pPlayerResource.Xor( ) )->GetPlayerPing( b->player->EntIndex( ) );
				break;
			}
		fuck_yeah:
			// this might not make sense to you, but it actually does.
			return ( a != nullptr || ( a != nullptr && b != nullptr && a == b ) ) ? true : false;
		};

		for( auto& data : m_rage_data->m_aim_data ) {
			// this should never happen, just to be extra safe.
			if( data.points.empty( ) )
				continue;

			if( !m_rage_data->m_pBestTarget ) {
				m_rage_data->m_pBestTarget = &data;
				m_rage_data->m_aim_points = data.points;

				// we only have one entry (target)? let's skip target selection..
				if( m_rage_data->m_aim_data.size( ) == 1 )
					break;
				else
					continue;
			}

			if( m_rage_data->m_pLastTarget != nullptr && !m_rage_data->m_pLastTarget->IsDead( ) && data.player->m_entIndex != m_rage_data->m_pLastTarget->m_entIndex && m_rage_data->m_pLastTarget->m_entIndex <= 64 ) {
				continue;
			}

			if( CheckTargets( &data, m_rage_data->m_pBestTarget ) ) {
				m_rage_data->m_pBestTarget = &data;
				m_rage_data->m_aim_points = data.points;
				continue;
			}
		}

	}

	std::pair<bool, C_AimPoint> C_Ragebot::RunHitscan( ) {
		m_rage_data->m_vecEyePos = g_Vars.globals.m_vecFixedEyePosition;


		if( !SetupTargets( ) )
			return { false, C_AimPoint( ) };


		//for( auto& p : m_rage_data->m_aim_points ) {
		//	Interfaces::m_pDebugOverlay->AddBoxOverlay( p.position, Vector( -0.7, -0.7, -0.7 ), Vector( 0.7, 0.7, 0.7 ), QAngle( ), 0, 255, 255, 255, Interfaces::m_pGlobalVars->interval_per_tick * 2 );
		//}

		C_AimPoint* bestPoint = nullptr;
		for( auto& p : m_rage_data->m_aim_points ) {
			if( p.damage < 1.0f )
				continue;

			// if we got no bestPoint yet, we should always take the first point
			if( !bestPoint ) {
				bestPoint = &p;
				continue;
			}

			// get vars.
			int iHealth = p.target->player->m_iHealth( );
			float flMaxBodyDamage = Autowall::ScaleDamage( p.target->player, m_rage_data->m_pWeaponInfo->m_iWeaponDamage, m_rage_data->m_pWeaponInfo->m_flArmorRatio, Hitgroup_Stomach );
			float flMaxHeadDamage = Autowall::ScaleDamage( p.target->player, m_rage_data->m_pWeaponInfo->m_iWeaponDamage, m_rage_data->m_pWeaponInfo->m_flArmorRatio, Hitgroup_Head );

			if( p.isLethal ) {
				// don't shoot at head if we can shoot body and kill enemy
				if( !p.isBody ) {
					continue; // go to next point
				}

				// we always want this point, due to it being either choosen by bShouldBaim or p.is_should_baim
				if( bestPoint->isHead || ( int ) p.damage > int( bestPoint->damage + 5 ) )
					bestPoint = &p;

				// if this damage is lethal, we actually want to break.
				if( bestPoint->damage >= iHealth ) {
					break;
				}

				// let's continue searching for a better point instead of possibly overriding the current one later on.
				continue;
			}
			else {
				// if damage to the head is higher than hp, prioritize head or safe points
				if( int( flMaxHeadDamage ) > iHealth ) {
					// don't shoot at body if we can shoot head and kill enemy
					if( !p.isHead ) {
						continue; // go to next point
					}

					// possibly good head point?
					if( ( !bestPoint->center && p.target->hasCenter ) || ( !p.target->hasCenter && ( int ) p.damage > ( int ) bestPoint->damage ) )
						bestPoint = &p;

					// basically, if this is lethal, we take it.
					if( bestPoint->damage >= iHealth ) {
						break;
					}
				}
				// we cant do damage to the head, lets procced with bodyaim conditions.
				else {
					// if damage to body higher than hp, prioritize body or safe points
					if( int( flMaxBodyDamage ) >= iHealth ) {
						// don't shoot at head if we can shoot body and kill enemy
						if( !p.isBody ) {
							continue; // go to next point
						}

						// possibly good body point?
						if( ( !bestPoint->center && p.target->hasCenter ) || ( !p.target->hasCenter && ( int ) p.damage > ( int ) bestPoint->damage ) )
							bestPoint = &p;

						// basically, if this is lethal, we take it.
						if( bestPoint->damage >= iHealth ) {
							break;
						}
					}
				}
			}

			auto bestTarget = bestPoint->target;
			if( bestTarget->preferBody ) {
				if( bestPoint->isBody != p.isBody ) {
					bestPoint = bestPoint->isBody ? bestPoint : &p;
					continue;
				}
			}
			else {
				if( bestTarget->preferHead ) {
					if( bestPoint->isHead != p.isHead ) {
						bestPoint = bestPoint->isHead ? bestPoint : &p;
						continue;
					}
				}
			}

			if( ( bestPoint->center && ( int )p.damage > ( int )bestPoint->damage + 5 ) || ( !bestPoint->center && ( int )p.damage > ( int )bestPoint->damage ) ) {
				bestPoint = &p;
				continue;
			}
		}

		bool result = false;
		if( bestPoint ) {
			if( IsPointAccurate( bestPoint, m_rage_data->m_vecEyePos ) ) {
				bool bDidDelayHeadShot = m_rage_data->m_bDelayedHeadAim;
				m_rage_data->m_bDelayedHeadAim = false;
				g_Vars.globals.m_bDelayingShot[ bestPoint->target->record->player->EntIndex( ) ] = false;

				if( AimAtPoint( bestPoint ) ) {
					if( m_rage_data->m_pCmd->buttons & IN_ATTACK ) {
						Encrypted_t<Engine::C_EntityLagData> m_lag_data = Engine::LagCompensation::Get( )->GetLagData( bestPoint->target->player->m_entIndex );
						auto targedt = TIME_TO_TICKS( bestPoint->target->record->m_flSimulationTime + Engine::LagCompensation::Get( )->GetLerp( ) );

						m_rage_data->m_pCmd->tick_count = targedt;

						//printf( "tickcount %i\n", m_rage_data->m_pCmd->tick_count );

						if( m_lag_data.IsValid( ) ) {
							std::stringstream msg;

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
								case HITBOX_PELVIS: // there is no pelvis hitgroup
									result = XorStr( "stomach" ); break;
								default:
									result = XorStr( "-" );
								}

								return result;
							};

#if defined(BETA_MODE) || defined(DEV)
							player_info_t info;
							if( Interfaces::m_pEngine->GetPlayerInfo( bestPoint->target->player->EntIndex( ), &info ) ) {
								int ping = 0;

								auto netchannel = Encrypted_t<INetChannel>( Interfaces::m_pEngine->GetNetChannelInfo( ) );
								if( !netchannel.IsValid( ) )
									ping = 0;
								else
									ping = static_cast< int >( netchannel->GetLatency( FLOW_OUTGOING ) * 1000.0f );

								std::vector<std::string> flags{ };
								if( bestPoint->record->m_bExtrapolated )
									flags.push_back( XorStr( "E" ) );

								if( m_lag_data->m_History.front( ).m_bTeleportDistance )
									flags.push_back( XorStr( "LC" ) );

								if( bestPoint->isLethal )
									flags.push_back( XorStr( "L" ) );

								if( bestPoint->target->record->m_bIsWalking )
									flags.push_back( XorStr( "W" ) );

								if( bestPoint->target->record->m_bIsRunning )
									flags.push_back( XorStr( "R" ) );

								if( bestPoint->target->preferBody )
									flags.push_back( XorStr( "PB" ) );

								if( bestPoint->record->m_bIsSideways )
									flags.push_back( XorStr( "SW" ) );

								if( info.fakeplayer ) // bot
									flags.push_back( XorStr( "B" ) );

								if( bDidDelayHeadShot )
									flags.push_back( XorStr( "DH" ) );

								std::string buffer;
								if( !flags.empty( ) ) {
									for( auto n = 0; n < flags.size( ); ++n ) {
										auto& it = flags.at( n );

										buffer.append( it );
										if( n != flags.size( ) - 1 && flags.size( ) > 1 )
											buffer.append( XorStr( ":" ) );
									}
								}
								else {
									buffer = XorStr( "none" );
								}

								msg << XorStr( "dmg: " ) << int( bestPoint->damage ) << XorStr( " | " );
								msg << XorStr( "hitgroup: " ) << TranslateHitbox( bestPoint->hitboxIndex ).c_str( ) << XorStr( "(" ) << int( bestPoint->pointscale * 100.f ) << XorStr( "%%%%)" ) << XorStr( " | " );
								msg << XorStr( "flick: " ) << int( bestPoint->target->record->m_iResolverMode == 6 ) << XorStr( " | " );
								msg << XorStr( "bt: " ) << TIME_TO_TICKS( m_lag_data->m_History.front( ).m_flSimulationTime - bestPoint->target->record->m_flSimulationTime ) << XorStr( " | " );
								msg << XorStr( "sent: " ) << int( *m_rage_data->m_pSendPacket ) << XorStr( " | " );
								msg << XorStr( "hc: " ) << int( bestPoint->hitchance ) << XorStr( " | " );
								msg << XorStr( "miss: " ) << m_lag_data->m_iMissedShots << XorStr( ":" ) << m_lag_data->m_iMissedShotsLBY << XorStr( ":" ) << bestPoint->target->record->m_iResolverMode << XorStr( " | " );
								msg << XorStr( "flag: " ) << buffer.data( ) << XorStr( " | " );
								msg << XorStr( "player: " ) << FixedStrLength( info.szName ).data( ) << XorStr( " | ");

								ILoggerEvent::Get( )->PushEvent( msg.str( ), FloatColor( 0.8f, 0.8f, 0.8f ), !g_Vars.misc.undercover_log, XorStr( "fired shot " ) );
								ILoggerEvent::Get()->PushEvent(std::to_string(m_lag_data->m_iMissedShots), FloatColor(0.8f, 0.8f, 0.0f), !g_Vars.misc.undercover_log, XorStr(""));
							}
#endif
						}

						m_rage_data->m_pLastTarget = m_rage_data->m_pBestTarget->player;

						Engine::C_ShotInformation::Get( )->CreateSnapshot( bestPoint->target->player, m_rage_data->m_vecEyePos, bestPoint->position, bestPoint->target->record, bestPoint->target->record->m_iResolverMode, bestPoint->hitgroup, bestPoint->hitboxIndex, int( bestPoint->damage ) );

						if( g_Vars.esp.esp_enable ) {
							if( g_Vars.esp.hitmatrix )
								IChams::Get( )->AddHitmatrix( bestPoint->target->player, bestPoint->target->record->GetBoneMatrix( ) );

							//if( g_Vars.esp.hitskeleton )
							//	IEsp::Get( )->AddSkeletonMatrix( bestPoint->target->player, bestPoint->target->record->GetBoneMatrix( ) );
						}

						if( g_Vars.esp.draw_hitboxes ) {
							auto matrix = bestPoint->target->record->GetBoneMatrix( );

							auto hdr = Interfaces::m_pModelInfo->GetStudiomodel( bestPoint->target->player->GetModel( ) );
							if( hdr ) {
								auto hitboxSet = hdr->pHitboxSet( bestPoint->target->player->m_nHitboxSet( ) );
								if( hitboxSet ) {
									for( int i = 0; i < hitboxSet->numhitboxes; ++i ) {
										auto hitbox = hitboxSet->pHitbox( i );
										if( hitbox->m_flRadius <= 0.f )
											continue;

										auto min = hitbox->bbmin.Transform( matrix[ hitbox->bone ] );
										auto max = hitbox->bbmax.Transform( matrix[ hitbox->bone ] );

										Interfaces::m_pDebugOverlay->AddCapsuleOverlay( min, max, hitbox->m_flRadius, g_Vars.esp.hitboxes_color.r * 255, g_Vars.esp.hitboxes_color.g * 255, g_Vars.esp.hitboxes_color.b * 255, g_Vars.esp.hitboxes_color.a * 255,
											Interfaces::m_pCvar->FindVar( XorStr( "sv_showlagcompensation_duration" ) )->GetFloat( ) );
									}
								}
							}
						}

						result = true;
					}
				}
			}
		}

		for( auto& target : m_rage_data->m_targets ) {
			target.backup.Apply( target.player );
		}

		return { result, *bestPoint };
	}

	bool C_Ragebot::OverrideHitscan( C_CSPlayer* player, Engine::C_LagRecord* record ) {
		auto local = C_CSPlayer::GetLocalPlayer( );
		if( !local )
			return false;

		if( m_rage_data->rbot->prefer_body_disable_resolved ) {
			if( record->m_bResolved )
				return false;
		}

		return true;
	}

	void C_Ragebot::ScanPoint( C_AimPoint* pPoint ) {
		if( !pPoint || !pPoint->target || !pPoint->target->player )
			return;

		// no do
		if( pPoint->hitboxIndex == HITBOX_HEAD && m_rage_data->m_bDelayedHeadAim ) {
			return;
		}

		Autowall::C_FireBulletData fireData;
		fireData.m_bPenetration = this->m_rage_data->rbot->autowall;

		auto dir = pPoint->position - this->m_rage_data->m_vecEyePos;
		dir.Normalize( );

		fireData.m_vecStart = this->m_rage_data->m_vecEyePos;
		fireData.m_vecDirection = dir;
		fireData.m_iHitgroup = convert_hitbox_to_hitgroup( pPoint->hitboxIndex );
		fireData.m_Player = this->m_rage_data->m_pLocal;
		fireData.m_TargetPlayer = pPoint->target->player;
		fireData.m_WeaponData = this->m_rage_data->m_pWeaponInfo.Xor( );
		fireData.m_Weapon = this->m_rage_data->m_pWeapon;

		pPoint->damage = Autowall::FireBullets( &fireData );
		pPoint->penetrated = fireData.m_iPenetrationCount < 4;

		int hp = pPoint->target->player->m_iHealth( );
		float mindmg = ( m_rage_data->rbot->min_damage > 100 ? hp + ( m_rage_data->rbot->min_damage - 100 ) : m_rage_data->rbot->min_damage );
		if( m_rage_data->rbot->min_damage_override && g_Vars.rage.key_dmg_override.enabled ) {
			mindmg = m_rage_data->rbot->min_damage_override_amount > 100 ? hp + ( m_rage_data->rbot->min_damage_override_amount - 100 ) : m_rage_data->rbot->min_damage_override_amount;
		}
		else if( !pPoint->penetrated ) {
			mindmg = m_rage_data->rbot->min_damage_visible > 100 ? hp + ( m_rage_data->rbot->min_damage_visible - 100 ) : m_rage_data->rbot->min_damage_visible;
		}

		// if lethal, we still want to shoot
		if( pPoint->damage >= 20 && pPoint->damage >= ( hp + 2 ) )
			mindmg = hp;

		if( ( convert_hitbox_to_hitgroup( pPoint->hitboxIndex ) == Hitgroup_Head ) && fireData.m_iHitgroup != Hitgroup_Head ) {
			pPoint->damage = 0.f;
			return;
		}

		if( pPoint->damage >= mindmg ) {
			pPoint->hitgroup = fireData.m_EnterTrace.hitgroup;
			pPoint->healthRatio = int( float( pPoint->target->player->m_iHealth( ) ) / pPoint->damage ) + 1;

			auto hitboxSet = ( *( studiohdr_t** )pPoint->target->player->m_pStudioHdr( ) )->pHitboxSet( pPoint->target->player->m_nHitboxSet( ) );
			auto hitbox = hitboxSet->pHitbox( pPoint->hitboxIndex );

			pPoint->isHead = pPoint->hitboxIndex == HITBOX_HEAD || pPoint->hitboxIndex == HITBOX_NECK;
			pPoint->isBody = pPoint->hitboxIndex == HITBOX_PELVIS || pPoint->hitboxIndex == HITBOX_STOMACH;

			// nospread enabled
			if( m_rage_data->m_flSpread != 0.0f && m_rage_data->m_flInaccuracy != 0.0f ) {
				auto pHitboxSet = ( *( studiohdr_t** )pPoint->target->player->m_pStudioHdr( ) )->pHitboxSet( pPoint->target->player->m_nHitboxSet( ) );
				auto pHitbox = pHitboxSet->pHitbox( pPoint->hitboxIndex );

				const bool bIsCapsule = pHitbox->m_flRadius != -1.0f;
				const float flRadius = pHitbox->m_flRadius;

				float flSpread = m_rage_data->m_flSpread + m_rage_data->m_flInaccuracy;

				Vector right, up;
				dir.GetVectors( right, up );

				int iHits = 0;

				Vector vecMin = pHitbox->bbmin.Transform( pPoint->target->player->m_CachedBoneData( ).Element( pHitbox->bone ) );
				Vector vecMax = pHitbox->bbmax.Transform( pPoint->target->player->m_CachedBoneData( ).Element( pHitbox->bone ) );

				for( int x = 1; x <= 4; x++ ) {
					for( int j = 0; j < x * 5; ++j ) {
						float flCalculatedSpread = flSpread * float( float( x ) / 4.f );

						float flDirCos, flDirSin;
						DirectX::XMScalarSinCos( &flDirCos, &flDirSin, DirectX::XM_2PI * float( float( j ) / float( x * 5 ) ) );

						// calculate spread vector
						Vector2D vecSpread;
						vecSpread.x = flDirCos * flCalculatedSpread;
						vecSpread.y = flDirSin * flCalculatedSpread;

						// calculate direction
						Vector vecDir;
						vecDir.x = dir.x + ( vecSpread.x * right.x ) + ( vecSpread.y * up.x );
						vecDir.y = dir.y + ( vecSpread.x * right.y ) + ( vecSpread.y * up.y );
						vecDir.z = dir.z + ( vecSpread.x * right.z ) + ( vecSpread.y * up.z );

						// calculate end point
						Vector vecEnd = m_rage_data->m_vecEyePos + vecDir * m_rage_data->m_pWeaponInfo->m_flWeaponRange;

						// use intersection for check hit
						bool bHit = bIsCapsule ?
							Math::IntersectSegmentToSegment( m_rage_data->m_vecEyePos, vecEnd, vecMin, vecMax, flRadius ) :
							Math::IntersectionBoundingBox( m_rage_data->m_vecEyePos, vecEnd, vecMin, vecMax );

						//did we hit
						if( bHit )
							iHits++;
					}
				}

				pPoint->hitchance = float( iHits ) / 0.5f;
			}
			else {
				pPoint->hitchance = 0.f;
			}
		}
		else {
			pPoint->healthRatio = 100;
			pPoint->hitchance = 0.0f;
			pPoint->damage = 0.f;
		}
	}

	int C_Ragebot::GeneratePoints( C_CSPlayer* player, std::vector<C_AimTarget>& aim_targets, std::vector<C_AimPoint>& aim_points ) {
		auto lagData = Engine::LagCompensation::Get( )->GetLagData( player->m_entIndex );
		if( !lagData.IsValid( ) || lagData->m_History.empty( ) || !lagData->m_History.front( ).m_bIsValid )
			return 0;

		player_info_t info;
		if( !Interfaces::m_pEngine->GetPlayerInfo( player->m_entIndex, &info ) )
			return 0;

		auto animState = player->m_PlayerAnimState( );

		if( !animState )
			return 0;

		auto renderable = player->GetClientRenderable( );
		if( !renderable )
			return 0;

		auto model = renderable->GetModel( );
		if( !model )
			return 0;

		auto hdr = Interfaces::m_pModelInfo->GetStudiomodel( model );
		if( !hdr )
			return 0;

		Engine::C_BaseLagRecord backup;
		backup.Setup( player );

		auto record = GetBestLagRecord( player, &backup );
		if( !record || !IsRecordValid( player, record ) ) {
			backup.Apply( player );
			return 0;
		}

		backup.Apply( player );

		auto hitboxSet = hdr->pHitboxSet( player->m_nHitboxSet( ) );

		if( !hitboxSet )
			return 0;

		auto& aim_target = aim_targets.emplace_back( );
		aim_target.player = player;
		aim_target.record = record;
		aim_target.backup = backup;
		aim_target.overrideHitscan = false;
		if( m_rage_data->rbot->override_hitscan ) {
			if( g_Vars.rage.override_key.enabled ) {
				aim_target.overrideHitscan = true;
			}
		}

		aim_target.preferBody = ( ( m_rage_data->rbot->prefer_body || !( record->m_iFlags & FL_ONGROUND ) ) && this->OverrideHitscan( player, record ) );
		aim_target.preferHead = !aim_target.preferBody;

		auto addedPoints = 0;
		for( int i = 0; i < HITBOX_MAX; i++ ) {
			auto hitbox = hitboxSet->pHitbox( i );
			float ps = 0.0f;
			if( !GetBoxOption( hitbox, hitboxSet, ps, aim_target.overrideHitscan ) ) {
				continue;
			}

			bool bDelayLimb = false;
			auto bIsLimb = hitbox->group == Hitgroup_LeftArm || hitbox->group == Hitgroup_RightArm || hitbox->group == Hitgroup_RightLeg || hitbox->group == Hitgroup_LeftLeg;
			if( bIsLimb ) {
				// we're not moving, let's delay the limb shot
				if( m_rage_data->m_pLocal->m_vecVelocity( ).Length2D( ) < 3.25f ) {
					bDelayLimb = true;
				}
				// we're moving, let's not shoot at limbs at all
				else {
					continue;
				}

				if( m_rage_data->rbot->ignorelimbs_ifwalking && record->m_vecVelocity.Length2DSquared( ) > 1.0f ) {
					continue;
				}
			}

			ps *= 0.01f;

			std::vector<std::pair<Vector, bool>> points;
			Multipoint( player, record, 0, points, hitbox, hitboxSet, ps, i );

			if( !points.size( ) )
				continue;

			for( const auto& point : points ) {
				C_AimPoint& p = aim_points.emplace_back( );

				p.position = point.first;
				p.center = !point.second;
				p.target = &aim_target;
				p.record = record;
				p.hitboxIndex = i;

				if( point.second )
					p.pointscale = ps;
				else
					p.pointscale = 0.f;

				++addedPoints;
			}
		}

		return addedPoints;
	}

	Engine::C_LagRecord* C_Ragebot::GetBestLagRecord( C_CSPlayer* player, Engine::C_BaseLagRecord* backup ) {
		auto lagData = Engine::LagCompensation::Get( )->GetLagData( player->m_entIndex );
		if( !lagData.IsValid( ) || lagData->m_History.empty( ) )
			return nullptr;

		auto& record = lagData->m_History.front( );
		if( !record.m_bIsValid ) {
			return nullptr;
		}

		int recordsCount = 0;
		Engine::C_LagRecord* arrRecords[ 64 ] = { nullptr };

		for( auto it = lagData->m_History.begin( ); it != lagData->m_History.end( ); ++it ) {
			if( it->m_bSkipDueToResolver ) {
				continue;
			}

			if( !it->m_bIsValid || !IsRecordValid( player, &*it ) ) {
				continue;
			}

			arrRecords[ recordsCount ] = &*it;
			recordsCount++;

			if( it->m_bTeleportDistance ) {
				break;
			}

			if( recordsCount + 1 >= 64 )
				break;
		}

		if( recordsCount <= 1 ) {
			return &record;
		}

		Engine::C_LagRecord* pBestRecord = nullptr;

		// iterate all valid records
		for( int i = 0; i < recordsCount; i++ ) {
			// get current record
			Engine::C_LagRecord* currentRecord = arrRecords[ i ];

			// if best record null, set best record to current record
			if( !pBestRecord ) {
				pBestRecord = currentRecord;
				continue; // go to next record
			}

			if( pBestRecord->m_bResolved != currentRecord->m_bResolved ) {
				if( !pBestRecord->m_bResolved ) {
					pBestRecord = currentRecord;
					continue;
				}
			}
		}

		if( !pBestRecord ) {
			return &record;
		}

		return pBestRecord;
	}

	bool C_Ragebot::IsRecordValid( C_CSPlayer* player, Engine::C_LagRecord* record ) {
		return Engine::LagCompensation::Get( )->IsRecordOutOfBounds( *record, 0.2f );
	}

	bool C_Ragebot::AimAtPoint( C_AimPoint* bestPoint ) {
		C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal && !pLocal->IsDead( ) )
			return false;

		C_WeaponCSBaseGun* pWeapon = ( C_WeaponCSBaseGun* )pLocal->m_hActiveWeapon( ).Get( );
		if( !pWeapon )
			return false;

		m_rage_data->m_pCmd->buttons &= ~IN_USE;

		// todo: aimstep
		Vector delta = bestPoint->position - m_rage_data->m_vecEyePos;
		delta.Normalize( );

		QAngle aimAngles = delta.ToEulerAngles( );
		aimAngles.Normalize( );

		if( !g_Vars.rage.silent_aim )
			Interfaces::m_pEngine->SetViewAngles( aimAngles );

		m_rage_data->m_pCmd->viewangles = aimAngles;
		m_rage_data->m_pCmd->viewangles.Normalize( );

		g_Vars.globals.CorrectShootPosition = true;
		g_Vars.globals.AimPoint = bestPoint->position;
		g_Vars.globals.ShootPosition = m_rage_data->m_vecEyePos;

		m_rage_data->m_iChokedCommands = -1;
		m_rage_data->m_bFailedHitchance = false;

		if( g_Vars.rage.auto_fire ) {
			if( !g_Vars.globals.Fakewalking )
				*m_rage_data->m_pSendPacket = false;

			m_rage_data->m_pCmd->buttons |= IN_ATTACK;

			g_Vars.globals.m_bAimbotShot = true;
		}

		return true;
	}

	static C_Ragebot instance;
	Encrypted_t<Ragebot> Interfaces::Ragebot::Get( ) {
		return &instance;
	}
}