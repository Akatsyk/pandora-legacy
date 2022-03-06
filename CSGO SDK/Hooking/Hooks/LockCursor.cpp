#include "../Hooked.hpp"
#include "../../SDK/Classes/Player.hpp"

void __stdcall Hooked::LockCursor( ) {
   g_Vars.globals.szLastHookCalled = XorStr( "16" );

   oLockCursor( (void*)Interfaces::m_pSurface.Xor() );

   auto pLocal = C_CSPlayer::GetLocalPlayer( );

   bool state = true;
   if( !Interfaces::m_pEngine->IsInGame( ) || ( pLocal && pLocal->IsDead( ) ) || GUI::ctx->typing ) {
	   state = !g_Vars.globals.menuOpen;
   }

   Interfaces::m_pInputSystem->EnableInput( state );

   if ( g_Vars.globals.menuOpen )
	  Interfaces::m_pSurface->UnlockCursor( );
}
