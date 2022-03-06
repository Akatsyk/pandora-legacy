#pragma once
class IInput
{
public:
	virtual void Init_All( void ) = 0;
	virtual void Shutdown_All( void ) = 0;
	virtual int GetButtonBits( int ) = 0;
	virtual void CreateMove( int sequence_number, float input_sample_frametime, bool active ) = 0;
	virtual void ExtraMouseSample( float frametime, bool active ) = 0;
	virtual bool WriteUsercmdDeltaToBuffer( int nSlot, void* buf, int from, int to, bool isnewcommand ) = 0;
	virtual void EncodeUserCmdToBuffer( void* buf, int slot ) = 0;
	virtual void DecodeUserCmdFromBuffer( void* buf, int slot ) = 0;
	virtual CUserCmd* GetUserCmd( int slot, int sequence_number ) = 0;

	//void* pvftable;
	bool				m_pad_something;
	bool				m_mouse_initialized;
	bool				m_mouse_active;
	bool				pad_something01;
	char				pad_0x08[ 0x2C ];
	void* m_keys;
	char				pad_0x38[ 0x64 ];
	__int32				pad_0x41;
	__int32				pad_0x42;
	bool				m_camera_intercepting_mouse;
	bool				m_fCameraInThirdPerson;
	bool                m_fCameraMovingWithMouse;
	Vector			    m_vecCameraOffset;
	bool                m_fCameraDistanceMove;
	int                 m_nCameraOldX;
	int                 m_nCameraOldY;
	int                 m_nCameraX;
	int                 m_nCameraY;
	bool                m_CameraIsOrthographic;
	Vector              m_angPreviousViewAngles;
	Vector              m_angPreviousViewAnglesTilt;
	float               m_flLastForwardMove;
	int                 m_nClearInputState;
	char                pad_0xE4[ 0x8 ];
	CUserCmd* m_pCommands;
	CVerifiedUserCmd* m_pVerifiedCommands;

	__forceinline int CAM_IsThirdPerson( int slot = -1 ) {
		return Memory::VCall< int( __thiscall* )( decltype( this ), int ) >( this, 32 )( this, slot );
	}

	__forceinline void CAM_ToThirdPerson( ) {
		return Memory::VCall< void( __thiscall* )( decltype( this ) ) >( this, 35 )( this );
	}

	__forceinline void CAM_ToFirstPerson( ) {
		return Memory::VCall< void( __thiscall* )( decltype( this ) ) >( this, 36 )( this );
	}
};
