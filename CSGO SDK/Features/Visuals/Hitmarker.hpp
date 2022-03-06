#pragma once
#include "../../SDK/sdk.hpp"

namespace Hitmarkers {
	// for world hitmarkers
	struct Hitmarkers_t {
		float m_flTime = 0.0f;
		float m_flAlpha = 0.0f;
		float m_flDamage = 0.0f;

		float m_flRandomRotation = 0.0f;
		float m_flRandomEnlargement = 0.0f;

		Color m_uColor = { };

		float m_flPosX = std::numeric_limits<float>::min( );
		float m_flPosY = std::numeric_limits<float>::min( );
		float m_flPosZ = std::numeric_limits<float>::min( );
	};
	extern std::vector<Hitmarkers_t> m_vecWorldHitmarkers;


	inline std::pair<Color, int> m_nLastDamageData;

	void AddWorldHitmarker( float flPosX = -1.f, float flPosY = -1.f, float flPosZ = -1.f );

	// for screen hitmarkers
	inline float m_flMarkerAlpha;
	inline float m_flRandomRotation;
	inline float m_flRandomEnlargement;
	inline bool m_bFirstMarker;
	inline Color m_uMarkerColor;
	void AddScreenHitmarker( Color uColor );

	void RenderWorldHitmarkers( );
	void RenderScreenHitmarkers( );
	void RenderHitmarkers( );
}