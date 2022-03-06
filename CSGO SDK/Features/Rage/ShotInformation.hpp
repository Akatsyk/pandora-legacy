#pragma once
#include "LagCompensation.hpp"
#include <vector>
#include <deque>

namespace Engine
{
	struct ShotSnapshot {
		int playerIdx;
		int outSequence;
		bool correctEyePos;
		bool correctSequence;
		C_CSPlayer* player;
		Engine::C_LagRecord resolve_record;
		Vector eye_pos;
		float time;
		bool doubleTap;

		// onepaste
		Vector AimPoint;

		int Hitgroup;
		int Hitbox;
		int ResolverType;

		int m_nSelectedDamage;
	};

	struct PlayerHurt_t {
		int damage;
		int hitgroup;
		C_CSPlayer* player;
		int playerIdx;
	};

	struct WeaponFire_t {
		std::vector< Vector > impacts;
		std::vector< PlayerHurt_t > damage;
		std::deque< ShotSnapshot >::iterator snapshot;
		bool didDraw = false;
	};

	class C_ShotInformation {
	public:
		static Encrypted_t<C_ShotInformation> Get();

		void Start();
		void ProcessEvents();

		void EventCallback(IGameEvent* gameEvent, uint32_t hash);
		void CreateSnapshot(C_CSPlayer* player, const Vector& shootPosition, const Vector& aimPoint, Engine::C_LagRecord* record, int resolverSide, int hitgroup, int hitbox, int nDamage, bool doubleTap = false);
		void CorrectSnapshots(bool is_sending_packet);

		std::deque< ShotSnapshot > m_Shapshots;
		std::vector< WeaponFire_t > m_Weaponfire;

		bool m_GetEvents = false;
	};
}