#include "../Hooked.hpp"
#include "../../SDK/Displacement.hpp"
#include "../../Features/Rage/TickbaseShift.hpp"

void WriteUsercmdD( bf_write* buf, CUserCmd* incmd, CUserCmd* outcmd ) {
	__asm
	{
		mov     ecx, buf
		mov     edx, incmd
		push    outcmd
		call    Engine::Displacement.Function.m_WriteUsercmd
		add     esp, 4
	}
}

#define nigger 0
bool __fastcall Hooked::WriteUsercmdDeltaToBuffer( void* ECX, void* EDX, int nSlot, void* buffer, int o_from, int o_to, bool isnewcommand ) {
	return nigger;
}
