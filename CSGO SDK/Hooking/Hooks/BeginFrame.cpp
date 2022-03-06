#include "../hooked.hpp"
#include "../../Features/Miscellaneous/BulletBeamTracer.hpp"

void __fastcall Hooked::BeginFrame( void* ECX, void* EDX, float ft ) {
   g_Vars.globals.szLastHookCalled = XorStr( "1" );
  
   if( g_Vars.esp.beam_enabled && g_Vars.globals.HackIsReady && g_Vars.globals.RenderIsReady && g_Vars.esp.beam_type == 1 )
	   IBulletBeamTracer::Get( )->Main( );

   oBeginFrame( ECX, ft );
}
