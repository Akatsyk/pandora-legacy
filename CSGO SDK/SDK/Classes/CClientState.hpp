#pragma once

class ClientClass;
struct CEventInfo {
public:
	enum {
		EVENT_INDEX_BITS = 8,
		EVENT_DATA_LEN_BITS = 11,
		MAX_EVENT_DATA = 192,  // ( 1<<8 bits == 256, but only using 192 below )
	};

	// 0 implies not in use
	short					m_class_id;
	float					m_fire_delay;
	const void* m_send_table;
	const ClientClass* m_client_class;
	int						m_bits;
	unsigned char* m_data;
	int						m_flags;
	char pad_18[ 0x18 ];
	CEventInfo* m_next;
};


class CClientState
{
public:
	int& m_nDeltaTick( );
	int& m_nLastOutgoingCommand( );
	int& m_nChokedCommands( );
	int& m_nLastCommandAck( );
	int& m_nMaxClients( );
	bool& m_bIsHLTV( );
	CEventInfo* m_pEvents( );
};
