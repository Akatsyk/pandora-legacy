#include "SetupBones.hpp"

#include "../../SDK/Displacement.hpp"
#include "../../Hooking/Hooked.hpp"
#include "../../SDK/Classes/weapon.hpp"

#include "../../SDK/Valve/CBaseHandle.hpp"

// ......
inline CHandle<C_BaseEntity> m_hRagdoll( C_CSPlayer* entity ) {
	return *( CHandle<C_BaseEntity>* )( ( int32_t )entity + Engine::Displacement.DT_CSPlayer.m_hRagdoll );
}

BoneSetup g_BoneSetup;

bool BoneSetup::SetupBonesRebuild( C_CSPlayer* entity, matrix3x4_t* pBoneMatrix, int nBoneCount, int boneMask, float time, int flags ) {
	if( *( int* )( uintptr_t( entity ) + Engine::Displacement.DT_BaseViewModel.m_nSequence ) == -1 ) {
		return false;
	}

	if( boneMask == -1 ) {
		boneMask = entity->m_iPrevBoneMask( );
	}

	boneMask = boneMask | 0x80000;

	// If we're setting up LOD N, we have set up all lower LODs also
	// because lower LODs always use subsets of the bones of higher LODs.
	int nLOD = 0;
	int nMask = BONE_USED_BY_VERTEX_LOD0;
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 ) {
		if( boneMask & nMask )
			break;
	}
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 ) {
		boneMask |= nMask;
	}

	auto model_bone_counter = **( unsigned long** )( Engine::Displacement.C_BaseAnimating.InvalidateBoneCache + 0x000A );

	CBoneAccessor backup_bone_accessor = entity->m_BoneAccessor( );
	CBoneAccessor* bone_accessor = &entity->m_BoneAccessor( );
	if( !bone_accessor )
		return false;

	if( entity->m_iMostRecentModelBoneCounter( ) != model_bone_counter || ( flags & BoneSetupFlags::ForceInvalidateBoneCache ) ) {
		if( FLT_MAX >= entity->m_flLastBoneSetupTime( ) || time < entity->m_flLastBoneSetupTime( ) ) {
			bone_accessor->m_ReadableBones = 0;
			bone_accessor->m_WritableBones = 0;
			entity->m_flLastBoneSetupTime( ) = ( time );
		}

		entity->m_iPrevBoneMask( ) = entity->m_iAccumulatedBoneMask( );
		entity->m_iAccumulatedBoneMask( ) = 0;

		auto hdr = entity->m_pStudioHdr( );
		if( hdr ) { // profiler stuff
			( ( CStudioHdrEx* )hdr )->m_nPerfAnimatedBones = 0;
			( ( CStudioHdrEx* )hdr )->m_nPerfUsedBones = 0;
			( ( CStudioHdrEx* )hdr )->m_nPerfAnimationLayers = 0;
		}
	}

	// Keep track of everything asked for over the entire frame
	// But not those things asked for during bone setup
	entity->m_iAccumulatedBoneMask( ) |= boneMask;

	// fix enemy poses getting raped when going out of pvs
	entity->m_iOcclusionFramecount( ) = 0;
	entity->m_iOcclusionFlags( ) = 0;

	// Make sure that we know that we've already calculated some bone stuff this time around.
	entity->m_iMostRecentModelBoneCounter( ) = model_bone_counter;

	bool bReturnCustomMatrix = ( flags & BoneSetupFlags::UseCustomOutput ) && pBoneMatrix;
	CStudioHdr* hdr = entity->m_pStudioHdr( );
	if( !hdr ) {
		return false;
	}

	// Setup our transform based on render angles and origin.
	Vector origin = ( flags & BoneSetupFlags::UseInterpolatedOrigin ) ? entity->GetAbsOrigin( ) : entity->m_vecOrigin( );
	QAngle angles = entity->GetAbsAngles( );

	alignas( 16 ) matrix3x4_t parentTransform;
	parentTransform.AngleMatrix( angles, origin );

	boneMask |= entity->m_iPrevBoneMask( );

	if( bReturnCustomMatrix ) {
		bone_accessor->m_pBones = pBoneMatrix;
	}

	// Allow access to the bones we're setting up so we don't get asserts in here.
	int oldReadableBones = bone_accessor->GetReadableBones( );
	int oldWritableBones = bone_accessor->GetWritableBones( );
	int newWritableBones = oldReadableBones | boneMask;
	bone_accessor->SetWritableBones( newWritableBones );
	bone_accessor->SetReadableBones( newWritableBones );

	if( !( hdr->_m_pStudioHdr->flags & 0x00000010 ) ) {
		entity->m_fEffects( ) |= EF_NOINTERP;

		entity->m_iEFlags( ) |= EFL_SETTING_UP_BONES;

		entity->m_pIk( ) = nullptr;
		entity->m_EntClientFlags |= 2; // ENTCLIENTFLAGS_DONTUSEIK

		alignas( 16 ) Vector pos[ 128 ];
		alignas( 16 ) Quaternion q[ 128 ];
		uint8_t computed[ 0x100 ];

		entity->StandardBlendingRules( hdr, pos, q, time, boneMask );

		std::memset( computed, 0, 0x100 );
		entity->BuildTransformations( hdr, pos, q, parentTransform, boneMask, computed );

		entity->m_iEFlags( ) &= ~EFL_SETTING_UP_BONES;

		// entity->ControlMouth( hdr );

		if( !bReturnCustomMatrix /*&& !bSkipAnimFrame*/ ) {
			memcpy( entity->m_vecBonePos( ), &pos[ 0 ], sizeof( Vector ) * hdr->_m_pStudioHdr->numbones );
			memcpy( entity->m_quatBoneRot( ), &q[ 0 ], sizeof( Quaternion ) * hdr->_m_pStudioHdr->numbones );
		}
	}
	else {
		parentTransform = bone_accessor->m_pBones[ 0 ];
	}

	if( /*boneMask & BONE_USED_BY_ATTACHMENT*/ flags & BoneSetupFlags::AttachmentHelper ) {
		using AttachmentHelperFn = void( __thiscall* )( C_BaseEntity*, CStudioHdr* );
		( ( AttachmentHelperFn )Engine::Displacement.Function.m_AttachmentHelper )( entity, hdr );
	}

	// don't override bone cache if we're just generating a standalone matrix
	if( bReturnCustomMatrix ) {
		*bone_accessor = backup_bone_accessor;

		return true;
	}

	return true;
}
bool BoneSetup::BuildBones( C_CSPlayer* entity, int mask, int flags ) {
	// no need to restore this
	entity->m_bIsJiggleBonesEnabled( ) = false;

	// setup bones :-)
	return SetupBonesRebuild( entity, nullptr, -1, mask, Interfaces::m_pGlobalVars->curtime, flags );
}