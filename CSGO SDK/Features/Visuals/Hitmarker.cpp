#include "Hitmarker.hpp"
#include "../../SDK/Classes/player.hpp"
#include <chrono>
#include "../../SDK/CVariables.hpp"
#include "../../Renderer/Render.hpp"

std::vector<Hitmarkers::Hitmarkers_t> Hitmarkers::m_vecWorldHitmarkers{ };

void Hitmarkers::AddScreenHitmarker( Color uColor ) {
	m_uMarkerColor = uColor;
	m_flMarkerAlpha = 255.f;
}

void Hitmarkers::AddWorldHitmarker( float flPosX, float flPosY, float flPosZ ) {
	if( !Interfaces::m_pPrediction->GetUnpredictedGlobals( ) )
		return;

	Hitmarkers_t info;

	info.m_flAlpha = 1.0f;

	info.m_flPosX = flPosX;
	info.m_flPosY = flPosY;
	info.m_flPosZ = flPosZ;
	info.m_flTime = Interfaces::m_pPrediction->GetUnpredictedGlobals( )->curtime;

	m_vecWorldHitmarkers.push_back( info );
}

void Hitmarkers::RenderWorldHitmarkers( ) {
	if( !Interfaces::m_pPrediction->GetUnpredictedGlobals( ) )
		return;

	if( !Hitmarkers::m_vecWorldHitmarkers.size( ) ) {
		return;
	}

	for( size_t i{ }; i < Hitmarkers::m_vecWorldHitmarkers.size( ); ++i ) {
		Hitmarkers::Hitmarkers_t& info = Hitmarkers::m_vecWorldHitmarkers[ i ];
		// bool bLastHitmarker = Hitmarkers::m_vecWorldHitmarkers.begin( ) + 1 == Hitmarkers::m_vecWorldHitmarkers.end( );

		// If the delta between the current time and hurt time is larger than 0.5 seconds then we should erase
		if( Interfaces::m_pPrediction->GetUnpredictedGlobals( )->curtime - info.m_flTime > 0.5f ) {
			info.m_flAlpha -= ( 1.0f / 1.0f ) * Interfaces::m_pGlobalVars->frametime;
			info.m_flAlpha = std::clamp<float>( info.m_flAlpha, 0.0f, 1.0f );
		}

		Vector2D vecPos;
		if( WorldToScreen( Vector( info.m_flPosX, info.m_flPosY, info.m_flPosZ ), vecPos ) ) {
			constexpr int iLineSize{ 8 };

			if( info.m_flAlpha > 0.0f && g_Vars.esp.visualize_hitmarker_world ) {
				auto DrawHitmarker = [ & ] ( Vector2D pos, Color clr ) {
					Render::DirectX::line(
						Vector2D( static_cast< int >( pos.x - iLineSize ), static_cast< int >( pos.y - iLineSize ) ),
						Vector2D( static_cast< int >( pos.x - ( iLineSize / 2 ) ), static_cast< int >( pos.y - ( iLineSize / 2 ) ) ),
						clr.OverrideAlpha( 255 * info.m_flAlpha, true ) );

					Render::DirectX::line(
						Vector2D( static_cast< int >( pos.x - iLineSize ), static_cast< int >( pos.y + iLineSize ) ),
						Vector2D( static_cast< int >( pos.x - ( iLineSize / 2 ) ), static_cast< int >( pos.y + ( iLineSize / 2 ) ) ),
						clr.OverrideAlpha( 255 * info.m_flAlpha, true ) );

					Render::DirectX::line(
						Vector2D( static_cast< int >( pos.x + iLineSize ), static_cast< int >( pos.y + iLineSize ) ),
						Vector2D( static_cast< int >( pos.x + ( iLineSize / 2 ) ), static_cast< int >( pos.y + ( iLineSize / 2 ) ) ),
						clr.OverrideAlpha( 255 * info.m_flAlpha, true ) );

					Render::DirectX::line(
						Vector2D( static_cast< int >( pos.x + iLineSize ), static_cast< int >( pos.y - iLineSize ) ),
						Vector2D( static_cast< int >( pos.x + ( iLineSize / 2 ) ), static_cast< int >( pos.y - ( iLineSize / 2 ) ) ),
						clr.OverrideAlpha( 255 * info.m_flAlpha, true ) );
				};

				DrawHitmarker( vecPos - Vector2D( 1, 0 ), Color::Palette_t::Black( ).OverrideAlpha( 125 ) );
				DrawHitmarker( vecPos - Vector2D( 0, 1 ), Color::Palette_t::Black( ).OverrideAlpha( 125 ) );
				DrawHitmarker( vecPos + Vector2D( 1, 0 ), Color::Palette_t::Black( ).OverrideAlpha( 125 ) );
				DrawHitmarker( vecPos + Vector2D( 0, 1 ), Color::Palette_t::Black( ).OverrideAlpha( 125 ) );

				DrawHitmarker( vecPos, Color::Palette_t::White( ) );
			}
		}

		if( Interfaces::m_pPrediction->GetUnpredictedGlobals( )->curtime - info.m_flTime > 2.5f ) {
			//printf( "cleared hit at %i (expired)\n", i );
			Hitmarkers::m_vecWorldHitmarkers.erase( Hitmarkers::m_vecWorldHitmarkers.begin( ) + i );
		}
	}
}

void Hitmarkers::RenderScreenHitmarkers( ) {
	static Vector2D vCenter = Render::GetScreenSize( ) * 0.5f;
	static Vector2D vDrawCenter = vCenter;
	if( m_flMarkerAlpha == 255.f ) {
		if( !m_bFirstMarker ) {
			RandomSeed( Interfaces::m_pGlobalVars->framecount );
			m_flRandomRotation = RandomFloat( -15.f, 15.f );
			m_flRandomEnlargement = RandomFloat( -2.f, 2.f );
			vDrawCenter.x += RandomFloat( -2.f, 2.f );
			vDrawCenter.y += RandomFloat( -2.f, 2.f );
		}
		else {
			m_flRandomRotation = 0.f;
			m_flRandomEnlargement = 0.f;
			vDrawCenter = vCenter;
			m_bFirstMarker = false;
		}
	}

	constexpr float flFadeFactor = 1.0f / 0.2f;
	float flFadeIncrement = ( flFadeFactor * Interfaces::m_pGlobalVars->framecount );
	m_flMarkerAlpha -= flFadeFactor;
	m_flRandomEnlargement -= m_flMarkerAlpha / 1020;

	if( m_flMarkerAlpha <= 0 ) {
		m_bFirstMarker = true;
		m_flMarkerAlpha = 0;
		return;
	}

	auto DrawAngularLine = [ ] ( int x, int y, float rad, float length, float gap, Color uColor ) -> void {
		const float flRadians = DEG2RAD( rad );
		Render::DirectX::line(
			Vector2D( ( int )round( x + ( sin( flRadians ) * length ) ), ( int )round( y + ( cos( flRadians ) * length ) ) ),
			Vector2D( ( int )round( x + ( sin( flRadians ) * gap ) ), ( int )round( y + ( cos( flRadians ) * gap ) ) ),
			uColor );
	};

	for( size_t i = 0; i < 4; i++ ) {
		DrawAngularLine(
			vDrawCenter.x,
			vDrawCenter.y,
			45.f + m_flRandomRotation + ( 90.f * i ),
			18.f + m_flRandomEnlargement,
			24.f + m_flRandomEnlargement,
			Color( m_uMarkerColor.r( ), m_uMarkerColor.g( ), m_uMarkerColor.b( ), m_flMarkerAlpha ) );
	}
}

void Hitmarkers::RenderHitmarkers( ) {
	if( g_Vars.esp.visualize_hitmarker_world || g_Vars.esp.visualize_damage ) {
		RenderWorldHitmarkers( );
	}

	if( g_Vars.esp.vizualize_hitmarker ) {
		RenderScreenHitmarkers( );
	}
}