#include "WeatherController.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/Player.hpp"
#include "../../Renderer/Render.hpp"

namespace Engine
{
	class C_WeatherController : public WeatherController {
	public:
		C_WeatherController( ) { }
		virtual ~C_WeatherController( ) { }

		virtual void ResetWeather( );
		virtual void UpdateWeather( ); // call on overrideview
	};

	WeatherController* WeatherController::Get( ) {
		static C_WeatherController instance;
		return &instance;
	}

	void C_WeatherController::ResetWeather( ) {
		if( !g_Vars.globals.bCreatedRain ) {
			return;
		}


		for( int i = 0; i <= Interfaces::m_pEntList->GetHighestEntityIndex( ); i++ ) {
			C_BaseEntity* pEntity = ( C_BaseEntity* )Interfaces::m_pEntList->GetClientEntity( i );
			if( !pEntity )
				continue;

			const ClientClass* pClientClass = pEntity->GetClientClass( );
			if( !pClientClass )
				continue;

			if( pClientClass->m_ClassID == ClassId_t::CPrecipitation ) {
				if( pEntity->GetClientNetworkable( ) )
					pEntity->GetClientNetworkable( )->Release( );
			}
		}
	}

	void C_WeatherController::UpdateWeather( ) {
		if( !g_Vars.esp.weather ) {
			return;
		}

		if( g_Vars.globals.bCreatedRain ) {
			return;
		}

		static ClientClass* pPrecipitation = nullptr;
		if( !pPrecipitation ) {
			for( auto pClientClass = Interfaces::m_pClient->GetAllClasses( ); pClientClass && !pPrecipitation; pClientClass = pClientClass->m_pNext ) {
				if( pClientClass->m_ClassID == ClassId_t::CPrecipitation ) {
					pPrecipitation = pClientClass;
				}
			}
		}

		if( pPrecipitation && pPrecipitation->m_pCreateFn ) {
			IClientNetworkable* pRainNetworkable = ( ( IClientNetworkable * ( * )( int, int ) )pPrecipitation->m_pCreateFn )( MAX_EDICTS - 1, 0 );
			if( !pRainNetworkable ) {
				return;
			}

			IClientUnknown* pRainUnknown = ( ( IClientRenderable* )pRainNetworkable )->GetIClientUnknown( );
			if( !pRainUnknown ) {
				return;
			}

			C_BaseEntity* pRainEnt = pRainUnknown->GetBaseEntity( );
			if( !pRainEnt ) {
				return;
			}

			if( !pRainEnt->GetClientNetworkable( ) ) {
				return;
			}

			pRainNetworkable->PreDataUpdate( 0 );
			pRainNetworkable->OnPreDataChanged( 0 );

			// null da callbacks
			if( g_Vars.r_RainRadius->fnChangeCallback.m_Size != 0 )
				g_Vars.r_RainRadius->fnChangeCallback.m_Size = 0;

			// limit the render distance of da rain
			if( g_Vars.r_RainRadius->GetFloat( ) != 1000.f )
				g_Vars.r_RainRadius->SetValueFloat( 1000.f );

			// only PRECIPITATION_TYPE_RAIN and PRECIPITATION_TYPE_SNOW work..?
			pRainEnt->m_nPrecipType( ) = PrecipitationType_t::PRECIPITATION_TYPE_SNOW;
			pRainEnt->OBBMins( ) = Vector( -32768.0f, -32768.0f, -32768.0f );
			pRainEnt->OBBMaxs( ) = Vector( 32768.0f, 32768.0f, 32768.0f );

			pRainEnt->GetClientNetworkable( )->OnDataChanged( 0 );
			pRainEnt->GetClientNetworkable( )->PostDataUpdate( 0 );

			g_Vars.globals.bCreatedRain = true;
		}
	}
}