#pragma once
#pragma once
#include "../../SDK/sdk.hpp"

class C_Window {
public:
	Vector2D pos;
	Vector2D size;
	Vector2D mouse_pos;
	int id;

	C_Window( ) { }
	C_Window( Vector2D _pos, Vector2D _size, int _id )
		: pos( _pos ), size( _size ), id( _id ) {
	}

	bool IsInBox( Vector2D m_MousePos, Vector2D box_pos, Vector2D box_size ) {
		return (
			m_MousePos.x > box_pos.x &&
			m_MousePos.y > box_pos.y &&
			m_MousePos.x < box_pos.x + box_size.x &&
			m_MousePos.y < box_pos.y + box_size.y
			);
	}

	void Drag( ) {
		auto current_mouse_pos = InputSys::Get( )->GetMousePosition( );
		if( g_Vars.globals.menuOpen && !GUI::ctx->dragging
			&& InputSys::Get( )->IsKeyDown( VirtualKeys::LeftButton )
			&& ( IsInBox( current_mouse_pos, pos, size ) || IsInBox( mouse_pos, pos, size ) ) ) {
			pos += current_mouse_pos - mouse_pos;

			switch( id ) {
			case 0:
				g_Vars.esp.keybind_window_x = pos.x;
				g_Vars.esp.keybind_window_y = pos.y;
				g_Vars.globals.m_bDraggingKeyBind = true;
				break;
			case 1:
				g_Vars.esp.spec_window_x = pos.x;
				g_Vars.esp.spec_window_y = pos.y;
				g_Vars.globals.m_bDraggingSpecList = true;
				break;
			}
		}
		else { 
			switch( id ) {
			case 0:
				g_Vars.globals.m_bDraggingKeyBind = false;
				break;
			case 1:
				g_Vars.globals.m_bDraggingSpecList = false;
				break;
			}
		}

		switch( id ) {
		case 0:
			pos.x = g_Vars.esp.keybind_window_x;
			pos.y = g_Vars.esp.keybind_window_y;
			break;
		case 1:
			pos.x = g_Vars.esp.spec_window_x;
			pos.y = g_Vars.esp.spec_window_y;
			break;
		}

		mouse_pos = InputSys::Get( )->GetMousePosition( );
	}
};


class __declspec( novtable ) IEsp : public NonCopyable {
public:
   static Encrypted_t<IEsp> Get( );
   virtual void DrawAntiAimIndicator( ) = NULL;
   virtual void Main( ) = NULL;
   virtual void SetAlpha( int idx ) = NULL;
   virtual float GetAlpha( int idx ) = NULL;
   virtual void AddSkeletonMatrix( C_CSPlayer* player, matrix3x4_t* bones ) = NULL;
protected:
   IEsp( ) { };
   virtual ~IEsp( ) {
   }
};
