#pragma once
#include "../../SDK/sdk.hpp"

class __declspec( novtable ) IBulletBeamTracer : public NonCopyable {
public:
  struct BulletImpactInfo
  {
	 float m_flExpTime;
	 Vector m_vecStartPos;
	 Vector m_vecHitPos;
	 Color m_cColor;
	 int m_nIndex;
	 int m_nTickbase;
	 bool ignore[64];
  };

  static Encrypted_t<IBulletBeamTracer> Get( );
  virtual void Main( ) = NULL;
  virtual void PushBeamInfo( BulletImpactInfo beam_info ) = NULL;
protected:
  IBulletBeamTracer( ) { };
  virtual ~IBulletBeamTracer( ) { };
};
