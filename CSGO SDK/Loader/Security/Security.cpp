#include "Security.hpp"
#include <Windows.h>
#include <TlHelp32.h>
#include <thread>

CProtection g_protection;

/* hey btw, here we could also implement heartbeat ! */
void TestThread( ) {
	int count = 0;

	// I hope this can't cause any false-positives, I only noticed 1 which was caused by nvidia, LOL!
	do {
		if( !g_protection.safety_check( ) ) {
			++count;
		}

		std::this_thread::sleep_for( std::chrono::seconds( 3 ) );
	} while( count < 3 );

	//HWND null = NULL;
	//LI_FN( MessageBoxA )( null, XorStr( "hhhhhhhhhh" ), g_protection.error_string.c_str( ), 0 );

	LI_FN( exit )( 69 );
}

void CProtection::Run( ) {
	//std::thread ther( TestThread );
	//ther.detach( );
}
