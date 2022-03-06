#include "AutoBuy.hpp"
#include "../../source.hpp"
#include "../../SDK/Classes/player.hpp"
#include "../../SDK/Classes/weapon.hpp"

class CAutoBuy : public IAutoBuy {
public:
	void Main( ) override;
private:
	void Buy( const char* name );
};

Encrypted_t<IAutoBuy> IAutoBuy::Get( ) {
	static CAutoBuy instance;
	return &instance;
}

void CAutoBuy::Main( ) {
	if( !g_Vars.misc.autobuy_enabled )
		return;

	C_CSPlayer* LocalPlayer = C_CSPlayer::GetLocalPlayer( );

	if( !LocalPlayer )
		return;

	int iTeam = LocalPlayer->m_iTeamNum( );

	if( g_Vars.misc.autobuy_first_weapon > 0 ) {
		switch( g_Vars.misc.autobuy_first_weapon ) {
		case 1: // scar20 / g3sg1
		{
			if( iTeam == TEAM_TT ) {
				Buy( XorStr( "buy g3sg1" ) );
			}
			else if( iTeam == TEAM_CT ) {
				Buy( XorStr( "buy scar20" ) );
			}

			break;
		}
		case 2: // ssg08 
			Buy( XorStr( "buy ssg08" ) ); break;
		case 3: // awp
			Buy( XorStr( "buy awp" ) ); break;
		default:
			break;
		}
	}

	if( g_Vars.misc.autobuy_second_weapon > 0 ) {
		switch( g_Vars.misc.autobuy_second_weapon ) {
		case 1: // elite
			Buy( XorStr( "buy elite" ) ); break;
		case 2: // deagle / revolver
			Buy( XorStr( "buy deagle" ) ); break;
		default:
			break;
		}
	}

	if( g_Vars.misc.autobuy_armor ) {
		Buy( XorStr( "buy vest" ) );
		Buy( XorStr( "buy vesthelm" ) );
	}

	if( g_Vars.misc.autobuy_hegrenade )
		Buy( XorStr( "buy hegrenade" ) );
	if( g_Vars.misc.autobuy_molotovgrenade ) {
		if( iTeam == TEAM_TT )
			Buy( XorStr( "buy molotov" ) );
		else if( iTeam == TEAM_CT )
			Buy( XorStr( "buy incgrenade" ) );
	}
	if( g_Vars.misc.autobuy_smokegreanade )
		Buy( XorStr( "buy smokegrenade" ) );

	if( g_Vars.misc.autobuy_flashbang )
		Buy( XorStr( "buy flashbang" ) );

	if( g_Vars.misc.autobuy_zeus )
		Buy( XorStr( "buy taser" ) );

	if( g_Vars.misc.autobuy_defusekit )
		Buy( XorStr( "buy defuser" ) );

	if( g_Vars.misc.autobuy_decoy )
		Buy( XorStr( "buy decoy" ) );
}

void CAutoBuy::Buy( const char* name ) {
	Interfaces::m_pEngine->ClientCmd( name );
}
