#pragma once
#include "../../SDK/sdk.hpp"

namespace Interfaces
{
  class __declspec( novtable ) KnifeBot : public NonCopyable {
  public:
	 static KnifeBot* Get( );
	 virtual void Main(  Encrypted_t<CUserCmd> pCmd, bool* sendPacket) = NULL;
  protected:
	 KnifeBot( ) {

	 }
	 virtual ~KnifeBot( ) {

	 }
  };
}
