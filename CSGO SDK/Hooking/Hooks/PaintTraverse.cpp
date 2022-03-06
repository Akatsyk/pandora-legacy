#include "../Hooked.hpp"
#include "../../Features/Rage/LagCompensation.hpp"
#include "../../SDK/Classes/weapon.hpp"
#include "../../SDK/Valve/CBaseHandle.hpp"
#include "../../Renderer/Render.hpp"
#include "../../Features/Visuals/ESP.hpp"
#include "../../Features/Visuals/GrenadePrediction.hpp"
#include "../../Features/Visuals/EventLogger.hpp"
#include "../../Features/Visuals/ExtendedEsp.hpp"
#include "../../Features/Miscellaneous/BulletBeamTracer.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

//#define TICKBASE_DEBUG 

namespace Hooked
{
	void PrintOnInject( ) {
		if( g_Vars.globals.c_login.empty( ) || strlen( g_Vars.globals.user_info.username ) < 2 )
			return;

		using FnL = void( __cdecl* )( Color const&, char const*, ... );
		static FnL MsgCol = reinterpret_cast< FnL >( GetProcAddress( GetModuleHandleA( XorStr( "tier0.dll" ) ), XorStr( "?ConColorMsg@@YAXABVColor@@PBDZZ" ) ) );

		Color accent = g_Vars.menu.ascent.ToRegularColor( );
		static int days_remaining = g_Vars.globals.user_info.sub_expiration / ( 60 * 60 * 24 );
		//static int days_remaining = 335;

		int display_number = 0;
		std::string display_text = { };
		static bool lifetime = false;
		if( days_remaining < 0 ) {
			// u mad?
			exit( 69 );
			return;
		}

		static int aaa = -1;
		if( aaa == -1 ) {
			Interfaces::m_pEngine->ClientCmd_Unrestricted( XorStr( "clear" ) );
			aaa = GetTickCount( );
		}

		if( GetTickCount( ) - aaa > 500 && aaa != -1 ) {
			static bool done = false;

			if( !done ) {
				MsgCol( Color( 255, 255, 255, 255 ), XorStr( "Welcome to vader.tech.\n" ) );
				done = true;
			}
		}
	}

	void __fastcall PaintTraverse( void* ecx, void* edx, unsigned int vguiPanel, bool forceRepaint, bool allowForce ) {
		g_Vars.globals.szLastHookCalled = XorStr( "21" );
		std::string szPanelName = Interfaces::m_pPanel->GetName( vguiPanel );

		if( !strcmp( XorStr( "HudZoom" ), szPanelName.data( ) ) && g_Vars.esp.remove_post_proccesing )
			return;

		oPaintTraverse( ecx, vguiPanel, forceRepaint, allowForce );

		if( !szPanelName.empty( ) && !szPanelName.compare( XorStr( "MatSystemTopPanel" ) ) ) {
			ILoggerEvent::Get( )->Main( );
			IEsp::Get( )->DrawAntiAimIndicator( );

			IGrenadePrediction::Get( )->Paint( );
			IExtendedEsp::Get( )->Start( );

			IEsp::Get( )->Main( );
			IExtendedEsp::Get( )->Finish( );

			if( g_Vars.esp.beam_enabled && g_Vars.globals.HackIsReady && g_Vars.globals.RenderIsReady && g_Vars.esp.beam_type == 0 )
				IBulletBeamTracer::Get( )->Main( );

			PrintOnInject( );

		}
	}
}
