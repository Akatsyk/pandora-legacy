#include "../Hooked.hpp"
#include "../../Features/Miscellaneous/Movement.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../Utils/InputSys.hpp"
#include "../../Features/Miscellaneous/VisibilityOptimization.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../SDK/Classes/PropManager.hpp"

void __fastcall Hooked::OverrideView( void* ECX, int EDX, CViewSetup* vsView ) {
	g_Vars.globals.szLastHookCalled = XorStr( "18" );
	C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );

	bool bOk = g_Vars.globals.RenderIsReady && vsView && local && Interfaces::m_pEngine->IsInGame( );

	if( bOk ) {
		IGrenadePrediction::Get( )->View( );
		if( !local->IsDead( ) ) {
			auto weapon = ( C_WeaponCSBaseGun* )( local->m_hActiveWeapon( ).Get( ) );
			if( weapon ) {
				auto weapon_data = weapon->GetCSWeaponData( );
				if( weapon_data.IsValid( ) ) {
					if( local->m_bIsScoped( ) ) {
						if( g_Vars.esp.remove_scope_zoom ) {
							if( weapon->m_zoomLevel( ) == 2 ) {
								vsView->fov = 45.0f;
							}
							else {
								vsView->fov = g_Vars.esp.world_fov;
							}
						}
					}
					else {
						vsView->fov = g_Vars.esp.world_fov;
					}
				}
			}
		}

		Interfaces::Movement::Get( )->ThirdPerson( );
	}

	oOverrideView( ECX, vsView );
}
