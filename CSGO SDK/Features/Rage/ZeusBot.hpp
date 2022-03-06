#pragma once
#include "../../SDK/sdk.hpp"

namespace Interfaces
{
	class __declspec( novtable ) ZeusBot : public NonCopyable {
	public:
		static ZeusBot* Get( );
		virtual void Main( Encrypted_t<CUserCmd> pCmd, bool* sendPacket ) = NULL;
	protected:
		ZeusBot( ) {

		}
		virtual ~ZeusBot( ) {

		}
	};
}
