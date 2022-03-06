#pragma once
#include "../../SDK/sdk.hpp"

namespace Interfaces
{
  class __declspec( novtable ) Miscellaneous : public NonCopyable {
  public:
	 static Miscellaneous* Get( );
	 virtual void Main( ) = NULL;

  protected:
	 Miscellaneous( ) { };
	 virtual ~Miscellaneous( ) {
	 }
  };
}
