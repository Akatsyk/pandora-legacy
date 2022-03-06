#pragma once
#include "SDK/sdk.hpp"

class RecvPropHook {
	RecvProp* m_Prop;
	RecvVarProxyFn m_OriginalFn;
public:
	using Shared = std::shared_ptr<RecvPropHook>;

public:
	RecvPropHook( RecvProp* prop, const RecvVarProxyFn hkProxy ) :
		m_Prop( prop ),
		m_OriginalFn( prop->m_ProxyFn ) {
		Hook( hkProxy );
	}

	auto Unhook( ) {
		m_Prop->m_ProxyFn = m_OriginalFn;

		unhooked = true;
	}

	~RecvPropHook( ) {
		if( this && !unhooked ) // was crashing when unloading hack
			Unhook( );
	}

	auto GetOriginalFunction( ) const -> RecvVarProxyFn {
		return m_OriginalFn;
	}

	auto Hook( const RecvVarProxyFn proxy_fn ) const -> void {
		m_Prop->m_ProxyFn = proxy_fn;
	}
private:
	bool unhooked = false;
};
