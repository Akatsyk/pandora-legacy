#pragma once
#include "../../SDK/sdk.hpp"
#include "../../SDK/Classes/Player.hpp"

enum BoneSetupFlags {
	None = 0,
	UseInterpolatedOrigin = ( 1 << 0 ),
	UseCustomOutput = ( 1 << 1 ),
	ForceInvalidateBoneCache = ( 1 << 2 ),
	AttachmentHelper = ( 1 << 3 ),
};

class BoneSetup {
public:
	bool SetupBonesRebuild( C_CSPlayer* entity, matrix3x4_t* pBoneMatrix, int nBoneCount, int boneMask, float time, int flags );
	bool BuildBones( C_CSPlayer* entity, int boneMask, int flags );
};

extern BoneSetup g_BoneSetup;