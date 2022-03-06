#include "BulletBeamTracer.hpp"
#include "../../source.hpp"
#include "../../SDK/CVariables.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../Rage/ShotInformation.hpp"

class CBulletBeamTracer : public IBulletBeamTracer {
public:
	void Main( ) override;
	void PushBeamInfo( BulletImpactInfo beam_info ) override;
private:
	void DrawBeam( );
	std::vector<BulletImpactInfo> bulletImpactInfo;
};

Encrypted_t<IBulletBeamTracer> IBulletBeamTracer::Get( ) {
	static CBulletBeamTracer instance;
	return &instance;
}

void CBulletBeamTracer::Main( ) {
	DrawBeam( );
}

void CBulletBeamTracer::PushBeamInfo( BulletImpactInfo beam_info ) {
	bulletImpactInfo.emplace_back( beam_info );
}

const unsigned short INVALID_STRING_INDEX = ( unsigned short )-1;
bool PrecacheModel( const char* szModelName )
{
	INetworkStringTable* m_pModelPrecacheTable = Interfaces::g_pClientStringTableContainer->FindTable( "modelprecache" );

	if( m_pModelPrecacheTable )
	{
		Interfaces::m_pModelInfo->FindOrLoadModel( szModelName );
		int idx = m_pModelPrecacheTable->AddString( false, szModelName );
		if( idx == INVALID_STRING_INDEX )
			return false;
	}
	return true;
}

void CBulletBeamTracer::DrawBeam( ) {
	C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer( );
	if( !LocalPlayer || LocalPlayer->IsDead( ) )
		return;

	float time = Interfaces::m_pGlobalVars->curtime;
	bool       is_final_impact;

	for( size_t i{ }; i < bulletImpactInfo.size( ) && !bulletImpactInfo.empty( ); ++i ) {
		auto& tr = bulletImpactInfo[ i ];

		if( tr.m_nIndex == LocalPlayer->EntIndex( ) ) {
			// is this the final impact?
			// last impact in the vector, it's the final impact.
			if( i == ( bulletImpactInfo.size( ) - 1 ) )
				is_final_impact = true;

			// the current impact's tickbase is different than the next, it's the final impact.
			else if( ( i + 1 ) < bulletImpactInfo.size( ) && tr.m_nTickbase != bulletImpactInfo.operator[ ]( i + 1 ).m_nTickbase )
				is_final_impact = true;

			else
				is_final_impact = false;

			if( !is_final_impact ) {
				bulletImpactInfo.erase( bulletImpactInfo.begin( ) + i );
			}
		}

		float delta = time - tr.m_flExpTime;
		if( delta > 1.0f )
			bulletImpactInfo.erase( bulletImpactInfo.begin( ) + i );
	}

	if( !bulletImpactInfo.empty( ) ) {
		for( auto& it : bulletImpactInfo ) {
			float delta = time - it.m_flExpTime;
			Color col = it.m_nIndex == LocalPlayer->EntIndex( ) ? Color::HSBtoRGB( delta, 1.0f, 1.0f ) : Color( 255, 15, 46 );
			col.SetAlpha( 1.0f - delta * 255 );
			Vector2D w2s_start, w2s_end;
			bool a = WorldToScreen( it.m_vecStartPos, w2s_start );
			bool b = WorldToScreen( it.m_vecHitPos, w2s_end );

			switch( g_Vars.esp.beam_type ) {
			case 0:
				if( a && b )
					Render::Engine::Line( w2s_start, w2s_end, col );
				break;
			case 1:
				if( !PrecacheModel( XorStr( "materials/sprites/laserbeam.vmt" ) ) ) {
					break;
				}

				BeamInfo_t beam_info;

				beam_info.m_nType = 0;
				beam_info.m_pszModelName = XorStr( "materials/sprites/laserbeam.vmt" );
				beam_info.m_nModelIndex = Interfaces::m_pModelInfo->GetModelIndex( XorStr( "materials/sprites/laserbeam.vmt" ) );
				beam_info.m_flHaloScale = 0.0f;
				beam_info.m_flLife = 0.09f; //0.09
				beam_info.m_flWidth = .6f;
				beam_info.m_flEndWidth = .75f;
				beam_info.m_flFadeLength = 3.0f;
				beam_info.m_flAmplitude = 0.f;
				beam_info.m_flBrightness = ( col.a( ) - 255.f ) * 0.8f;
				beam_info.m_flSpeed = 1.f;
				beam_info.m_nStartFrame = 1;
				beam_info.m_flFrameRate = 60;
				beam_info.m_flRed = col.r( );
				beam_info.m_flGreen = col.g( );
				beam_info.m_flBlue = col.b( );
				beam_info.m_nSegments = 4;
				beam_info.m_bRenderable = true;
				beam_info.m_nFlags = 0;

				beam_info.m_vecStart = it.m_vecStartPos;
				beam_info.m_vecEnd = it.m_vecHitPos;

				Beam_t* beam = Interfaces::m_pRenderBeams->CreateBeamPoints( beam_info );

				if( beam ) {
					Interfaces::m_pRenderBeams->DrawBeam( beam );
				}

				break;
			}
		}
	}
}
