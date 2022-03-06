#pragma once
#include <string>
#include <sstream>

#include "../SDK/sdk.hpp"
#include "../SDK/Valve/vector4d.hpp"

#include "Wrapper/font/font.h"
#include "Wrapper/sprite/sprite.h"

#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib, "d3dx9.lib")

using TextureID = void*;
using ColorU32 = uint32_t;
using Rect2D = Vector4D;
using FontHandle = std::size_t;

struct IDirect3DDevice9;

enum text_flags : int {
	CENTER_X = ( 1 << 0 ),
	CENTER_Y = ( 1 << 1 ),
	ALIGN_RIGHT = ( 1 << 2 ),
	ALIGN_BOTTOM = ( 1 << 3 ),
	DROP_SHADOW = ( 1 << 4 ),
	OUTLINED = ( 1 << 5 ),
};

enum : uint32_t {
	FONT_VERDANA = 0,
	FONT_MENU_BOLD = FONT_VERDANA,
	FONT_MENU = FONT_VERDANA,
	FONT_CSGO_ICONS,
	FONT_VERDANA_30_BOLD,
	FONT_VERDANA_25_REGULAR,
	FONT_VISITOR,
	FONT_PORTER,
	FONT_CSGO_ICONS2,
	FONT_VERDANA_40_BOLD,
};

namespace Render {
	namespace DirectX {
		extern IDirect3DDevice9* device;
		extern IDirect3DStateBlock9* state_block;
		extern IDirect3DVertexDeclaration9* vert_dec;
		extern IDirect3DVertexShader9* vert_shader;
		extern IDirect3DVertexBuffer9* vert_buf;
		extern std::vector<font*> font_arr;

		namespace Fonts {
			inline font menu( XorStr( "Verdana" ), 12, 500, true );
			inline font menu_small( XorStr( "Small Fonts" ), 9, 500 );
			inline font menu_pixel( XorStr( "Small Fonts" ), 9, 500 );
			inline font menu_bold( XorStr( "Verdana" ), 12, 700, true );
			inline font menu_icon( XorStr( "fonteditor" ), 50, 500, true );
		}

		namespace Textures {
			inline c_sprite* background = new c_sprite( );
			inline c_sprite* logo = new c_sprite( );
		}

		extern bool initialized;

		void set_render_states( );

		void init( IDirect3DDevice9* p_device );

		void invalidate( );

		void restore( IDirect3DDevice9* p_device );

		void set_viewport( D3DVIEWPORT9 vp );
		D3DVIEWPORT9 get_viewport( );

		void set_scissor_rect( D3DVIEWPORT9 vp );
		D3DVIEWPORT9 get_scissor_rect( );
		void reset_scissor_rect( );

		// drawing functions

		void rect( Vector2D pos, Vector2D size, Color c );
		void rect_fill( Vector2D pos, Vector2D size, Color c );;

		void triangle( Vector2D pos1, Vector2D pos2, Vector2D pos3, Color c );
		void triangle_filled( Vector2D pos1, Vector2D pos2, Vector2D pos3, Color c );

		void polygon_gradient( Vector2D pos1, Vector2D pos2, Vector2D pos3, Vector2D pos4, Color c_a, Color c_b );

		void gradient_v( Vector2D pos, Vector2D size, Color c_a, Color c_b );
		void gradient_h( Vector2D pos, Vector2D size, Color c_a, Color c_b );

		void gradient_multi_color_filled( Vector2D pos, Vector2D size, Color c_a, Color c_b, Color c_c, Color c_d );
		void gradient_multi_color( Vector2D pos, Vector2D size, Color c_a, Color c_b, Color c_c, Color c_d );

		void line( Vector2D a, Vector2D b, Color c );

		void circle_filled( Vector2D center, float radius, int rotate, int type, bool smoothing, int resolution, Color c );
		void arc( Vector2D center, float radius, float multiplier, Color c, bool antialias = true );
		void circle_gradient( Vector2D center, float radius, int rotate, int type, int resolution, Color c, Color c1 );

		void begin( );
		void end( );
	}

	namespace Engine {
		struct FontSize_t {
			int m_width;
			int m_height;
		};

		enum StringFlags_t {
			ALIGN_LEFT = 0,
			ALIGN_RIGHT,
			ALIGN_CENTER
		};

		class Font {
		public:
			HFont      m_handle;
			FontSize_t m_size;

		public:
			__forceinline Font( ) : m_handle{ }, m_size{ } {};

			Font( const std::string& name, int s, int w, int flags );
			Font( HFont font );

			void string( int x, int y, Color color, const std::string& text, StringFlags_t flags = ALIGN_LEFT );
			//void string( int x, int y, Color color, const std::stringstream& text, StringFlags_t flags = ALIGN_LEFT );
			void wstring( int x, int y, Color color, const std::wstring& text, StringFlags_t flags = ALIGN_LEFT );
			FontSize_t size( const std::string& text );
			FontSize_t wsize( const std::wstring& text );
		};

		extern Font esp;
		extern Font pixel;
		extern Font pixel_reg;
		extern Font console;
		extern Font hud;
		extern Font segoe;
		extern Font cs;
		extern Font cs_large;
		extern Font damage;
		extern Font icon;
		extern Font indi;

		extern int m_width;
		extern int m_height;

		void Initialise( );
		bool WorldToScreen( const Vector& world, Vector2D& screen );
		void Line( Vector2D v0, Vector2D v1, Color color );
		void Polygon( int count, Vertex_t* vertices, const Color& col );
		void FilledTriangle( const Vector2D& pos1, const Vector2D& pos2, const Vector2D& pos3, const Color& col );
		void WorldCircle( Vector origin, float radius, Color color, Color colorFill = { } );
		void Line( int x0, int y0, int x1, int y1, Color color );
		void Rect( int x, int y, int w, int h, Color color );
		void RectFilled( int x, int y, int w, int h, Color color );
		void RectFilled( Vector2D pos, Vector2D size, Color color );
		void RectOutlined( int x, int y, int w, int h, Color color, Color color2 );
		void CircleFilled( int x, int y, float radius, int segments, Color color );
		void Gradient( int x, int y, int w, int h, Color color, Color color2, bool horizontal = false );
	}

	Vector2D GetScreenSize( );
};

