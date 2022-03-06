#include "ExtendedBactrack.hpp"
#include "../../source.hpp"
#include "LagCompensation.hpp"
#include <algorithm>
// vader.tech invite https://discord.gg/GV6JNW3nze
class ExtendedBacktrack : public IExtendenBacktrack {
public:
	void SetSuitableInSequence( INetChannel* channel ) override;
	float CalculatePing( INetChannel* channel ) override;
	void FlipState( INetChannel* channel ) override;
private:
	bool m_bIsFlipedState{ };
};

IExtendenBacktrack* IExtendenBacktrack::Get( ) {
	static ExtendedBacktrack instance;
	return &instance;
}

void ExtendedBacktrack::SetSuitableInSequence( INetChannel* channel ) {
	if( m_bIsFlipedState ) {
		m_bIsFlipedState = false;
		return;
	}

	const auto spike = TIME_TO_TICKS( CalculatePing( channel ) );
	if( channel->m_nInSequenceNr > spike )
		channel->m_nInSequenceNr -= spike;
}

float ExtendedBacktrack::CalculatePing( INetChannel* channel ) {
	auto wanted_ping = 0.f;

	if( g_Vars.misc.extended_backtrack )
		wanted_ping = 200.f / 1000.f;
	//else if ( g_Vars.rage.enabled )
	   //wanted_ping = 200.f / 1000.f - Engine::LagCompensation::Get( )->GetLerp( );
	else
		return 0.f;

	return std::max( 0.f, wanted_ping - channel->GetLatency( FLOW_OUTGOING ) );
}

void ExtendedBacktrack::FlipState( INetChannel* channel ) {
	static auto last_reliable_state = -1;

	if( channel->m_nInReliableState != last_reliable_state )
		m_bIsFlipedState = true;

	last_reliable_state = channel->m_nInReliableState;
}
