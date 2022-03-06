#pragma once
#include "../../SDK/sdk.hpp"

class __declspec( novtable ) GlowOutline : public NonCopyable {
public:
  static GlowOutline* Get( );
  virtual void Render( ) = 0;
  virtual void Shutdown( ) = 0;

protected:
  GlowOutline( ) { };
  virtual ~GlowOutline( ) { };
};
