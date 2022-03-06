#pragma once
#include "../../SDK/sdk.hpp"

class __declspec( novtable ) ILoggerEvent : NonCopyable {
public:
  static Encrypted_t<ILoggerEvent> Get( );
  virtual void Main( ) = NULL;
  virtual void PushEvent( std::string msg, FloatColor clr, bool visualise = true, std::string prefix = "" ) = NULL;
protected:
  ILoggerEvent( ) { };
  virtual ~ILoggerEvent( ) { };
};
