#include "EventLogger.hpp"
#include "../../source.hpp"
#include "../../Renderer/Render.hpp"

class NotifyText {
public:
	std::string m_text;
	Color		m_color;
	float		m_time;

public:
	__forceinline NotifyText( const std::string& text, Color color, float time ) : m_text{ text }, m_color{ color }, m_time{ time } {}
};

std::deque< std::shared_ptr< NotifyText > > m_notify_text;

class CLogger : public ILoggerEvent {
public:
	void Main( ) override;

	void PushEvent( std::string msg, FloatColor clr, bool visualise = true, std::string prefix = "" ) override;

private:
};

Encrypted_t<ILoggerEvent> ILoggerEvent::Get( ) {
	static CLogger instance;
	return &instance;
}

void CLogger::Main( ) {
	int		x{ 8 }, y{ 5 }, size{ Render::Engine::hud.m_size.m_height + 1 };
	Color	color;
	float	left;

	// update lifetimes.
	for( size_t i{ }; i < m_notify_text.size( ); ++i ) {
		auto notify = m_notify_text[ i ];

		notify->m_time -= Interfaces::m_pGlobalVars->frametime;

		if( notify->m_time <= 0.f ) {
			m_notify_text.erase( m_notify_text.begin( ) + i );
			continue;
		}
	}

	// we have nothing to draw.
	if( m_notify_text.empty( ) )
		return;

	// iterate entries.
	for( size_t i{ }; i < m_notify_text.size( ); ++i ) {
		auto notify = m_notify_text[ i ];

		if( notify->m_text.find( XorStr( "%%%%" ) ) != std::string::npos )
			notify->m_text.erase( notify->m_text.find( XorStr( "%" ) ), 3 );

		left = notify->m_time;
		color = notify->m_color;

		if( left < .5f ) {
			float f = left;
			f = Math::Clamp( f, 0.f, .5f );

			f /= .5f;

			color.RGBA[ 3 ] = ( int )( f * 255.f );

			if( i == 0 && f < 0.2f )
				y -= size * ( 1.f - f / 0.2f );
		}

		else
			color.RGBA[ 3 ] = 255;

		Render::Engine::hud.string( x, y, color, notify->m_text );
		y += size;
	}

	// clear more than 10 entries.
	if( m_notify_text.size( ) > 10 )
		m_notify_text.pop_front( );
}

void CLogger::PushEvent( std::string msg, FloatColor clr, bool visualise, std::string prefix ) {
	if( visualise ) {
		m_notify_text.push_back( std::make_shared< NotifyText >( std::string( XorStr( "" ) ).append( prefix.data( ) ).append( XorStr( "" ) ).append( msg ), clr.ToRegularColor( ), 8.f ) );
	}

	Interfaces::m_pCvar->ConsoleColorPrintf( g_Vars.menu.ascent.ToRegularColor( ), XorStr( "[ vader.tech ] " ) );
	if( !prefix.empty( ) ) {
		Interfaces::m_pCvar->ConsoleColorPrintf( g_Vars.menu.ascent.ToRegularColor( ), std::string( XorStr( "" ) ).append( prefix.data( ) ).append( XorStr( "" ) ).data( ) );
	}

	Interfaces::m_pCvar->ConsoleColorPrintf( clr.ToRegularColor( ), std::string( msg + XorStr( "\n" ) ).c_str( ) );
}
