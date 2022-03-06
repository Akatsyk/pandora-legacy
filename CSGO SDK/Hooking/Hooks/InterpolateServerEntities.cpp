#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Features/Game/SetupBones.hpp"
#include "../../Features/Rage/LagCompensation.hpp"

namespace Hooked
{
	void __cdecl InterpolateServerEntities( ) {
		g_Vars.globals.szLastHookCalled = XorStr( "11" );
		if( !g_Vars.globals.RenderIsReady )
			return oInterpolateServerEntities( );

		C_CSPlayer* pLocal = C_CSPlayer::GetLocalPlayer( );
		if( !pLocal || !Interfaces::m_pEngine->IsInGame( ) )
			return oInterpolateServerEntities( );

		oInterpolateServerEntities( );

		auto pStudioHdr = pLocal->m_pStudioHdr( );

		// fix server model origin
		{
			pLocal->SetAbsAngles( QAngle( 0.0f, g_Vars.globals.flRealYaw, 0.0f ) );

			matrix3x4_t matWorldMatrix{ };
			matWorldMatrix.AngleMatrix( QAngle( 0.f, g_Vars.globals.flRealYaw, 0.f ), pLocal->GetAbsOrigin( ) );

			if( pStudioHdr ) {
				uint8_t uBoneComputed[ 0x20 ] = { 0 };
				pLocal->BuildTransformations( pStudioHdr, g_Vars.globals.m_RealBonesPositions, g_Vars.globals.m_RealBonesRotations,
					matWorldMatrix, BONE_USED_BY_ANYTHING, uBoneComputed );
			}

			pLocal->InvalidateBoneCache( );

			auto pBackupBones = pLocal->m_BoneAccessor( ).m_pBones;
			pLocal->m_BoneAccessor( ).m_pBones = pLocal->m_CachedBoneData( ).Base( );

			if( pStudioHdr ) {
				using AttachmentHelper_t = void( __thiscall* )( C_CSPlayer*, CStudioHdr* );
				static AttachmentHelper_t AttachmentHelperFn = ( AttachmentHelper_t )Engine::Displacement.Function.m_AttachmentHelper;
				AttachmentHelperFn( pLocal, pStudioHdr );
			}

			pLocal->m_BoneAccessor( ).m_pBones = pBackupBones;
		}
	}
}