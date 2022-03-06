#include "../hooked.hpp"
#include "../../Utils/FnvHash.hpp"
#include "../../Features/Game/Prediction.hpp"

void __fastcall Hooked::hkEmitSound( IEngineSound* thisptr, uint32_t, void* filter, int ent_index, int channel, const char* sound_entry, unsigned int sound_entry_hash,
	const char* sample, float volume, float attenuation, int seed, int flags, int pitch, const Vector* origin, const Vector* direction,
	void* vec_origins, bool update_positions, float sound_time, int speaker_entity, int test ) {
	g_Vars.globals.szLastHookCalled = XorStr( "7" );

	auto& prediction = Engine::Prediction::Instance( );

	if( strstr( sound_entry, XorStr( "c4_beep" ) ) != nullptr ) {
		g_Vars.globals.bBombActive = true;
		g_Vars.globals.bBombTicked = true;
	}

	if( prediction.InPrediction( ) ) {
		flags |= 1 << 2;
		goto end;
	}

end:
	oEmitSound( thisptr, filter, ent_index, channel, sound_entry, sound_entry_hash, sample, volume, attenuation, seed, flags,
		pitch, origin, direction, vec_origins, update_positions, sound_time, speaker_entity, test );
}