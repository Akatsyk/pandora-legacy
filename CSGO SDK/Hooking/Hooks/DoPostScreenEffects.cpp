#include "../Hooked.hpp"
#include "../../Features/Visuals/Glow.hpp"
#include "../../Features/Visuals/CChams.hpp"
#include "../../SDK/CVariables.hpp"

namespace Hooked
{
	int __stdcall DoPostScreenEffects(int a1) {
		g_Vars.globals.szLastHookCalled = XorStr("5");
		if (g_Vars.globals.HackIsReady && g_Vars.globals.RenderIsReady)
			GlowOutline::Get()->Render();

		Interfaces::IChams::Get()->OnPostScreenEffects();

		g_Vars.globals.m_bInPostScreenEffects = true;
		auto result = oDoPostScreenEffects(Interfaces::m_pClientMode.Xor(), a1);
		g_Vars.globals.m_bInPostScreenEffects = false;

		return result;
	}
}
