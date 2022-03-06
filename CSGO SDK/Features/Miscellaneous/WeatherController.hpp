#pragma once
#include "../../SDK/sdk.hpp"

namespace Engine {
	class __declspec( novtable ) WeatherController : public NonCopyable {
	public:
		static WeatherController* Get( );
		virtual void ResetWeather( ) = 0;
		virtual void UpdateWeather( ) = 0;
	};
}