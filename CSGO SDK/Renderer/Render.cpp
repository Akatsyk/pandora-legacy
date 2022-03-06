#define NOMINMAX
#include "render.hpp"
#include <algorithm>
#include <mutex>

#include <unordered_map>
#include "../source.hpp"

#include "../Utils/XorStr.hpp"

#include "Textures/titlebar.h"
#include "Textures/weaponicons.h"

#pragma region DirectXRenderer

IDirect3DDevice9* Render::DirectX::device;
IDirect3DStateBlock9* Render::DirectX::state_block;
IDirect3DVertexDeclaration9* Render::DirectX::vert_dec;
IDirect3DVertexShader9* Render::DirectX::vert_shader;
IDirect3DVertexBuffer9* Render::DirectX::vert_buf;

bool Render::DirectX::initialized;
std::vector<font*> Render::DirectX::font_arr;

void Render::DirectX::set_render_states( ) {
	device->SetVertexShader( nullptr );
	device->SetPixelShader( nullptr );
	device->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
	device->SetRenderState( D3DRS_LIGHTING, FALSE );
	device->SetRenderState( D3DRS_FOGENABLE, FALSE );
	device->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	device->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

	device->SetRenderState( D3DRS_ZENABLE, FALSE );
	device->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
	device->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
	device->SetRenderState( D3DRS_STENCILENABLE, FALSE );

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );

	device->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	device->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
	device->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, TRUE );
	device->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	device->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_INVDESTALPHA );
	device->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	device->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_ONE );

	device->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );
	device->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
		D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA );
}

void LoadFontFromResource( char* arr, const size_t size ) {
	DWORD n_fonts;
	AddFontMemResourceEx( arr, size, nullptr, &n_fonts );
}

void Render::DirectX::init( IDirect3DDevice9* dev ) {
	device = dev;

	LoadFontFromResource( menuicons, sizeof( menuicons ) );

	font_arr.push_back( &Fonts::menu );
	font_arr.push_back( &Fonts::menu_bold );
	font_arr.push_back( &Fonts::menu_small );
	font_arr.push_back( &Fonts::menu_pixel );
	font_arr.push_back( &Fonts::menu_icon );

	for( auto font : font_arr ) {
		font->setup( device );
		font->init( );
	}

	Textures::background->setup( device, background_texture, sizeof( background_texture ), 2000, 2000 );

	initialized = true;
}

void Render::DirectX::invalidate( ) {
	for( auto font : font_arr ) {
		font->invalidate( );
	}

	Textures::background->invalidate( );
	Textures::logo->invalidate( );
}

void Render::DirectX::restore( IDirect3DDevice9* dev ) {
	device = dev;

	for( auto font : font_arr ) {
		font->setup( device );
		font->init( );
	}

	Textures::background->setup( device, background_texture, sizeof( background_texture ), 2000, 2000 );
}

void Render::DirectX::set_viewport( D3DVIEWPORT9 vp ) {
	device->SetViewport( &vp );
}

D3DVIEWPORT9 Render::DirectX::get_viewport( ) {
	D3DVIEWPORT9 vp;
	device->GetViewport( &vp );
	return vp;
}

void Render::DirectX::set_scissor_rect( D3DVIEWPORT9 vp ) {
	RECT rec;
	rec.left = static_cast< LONG >( vp.X );
	rec.top = static_cast< LONG >( vp.Y );
	rec.right = static_cast< LONG >( vp.X + vp.Width );
	rec.bottom = static_cast< LONG >( vp.Y + vp.Height );

	device->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );
	device->SetScissorRect( &rec );
}

D3DVIEWPORT9 Render::DirectX::get_scissor_rect( ) {
	RECT rec;
	device->GetScissorRect( &rec );

	D3DVIEWPORT9 vp = { rec.left, rec.top, rec.right, rec.bottom, 0.f, 1.0f };
	return vp;
}

void Render::DirectX::reset_scissor_rect( ) {
	RECT rec;
	rec.left = static_cast< LONG >( 0 );
	rec.top = static_cast< LONG >( 0 );
	rec.right = static_cast< LONG >( Render::GetScreenSize( ).x );
	rec.bottom = static_cast< LONG >( Render::GetScreenSize( ).y );

	device->SetScissorRect( &rec );
	device->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
}

struct vertice {
	float x, y, z, rhw;
	DWORD c;
};

void Render::DirectX::rect( Vector2D pos, Vector2D size, Color c ) {
	vertice verts[ 5 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, &verts, 20 );
}

void Render::DirectX::rect_fill( Vector2D pos, Vector2D size, Color c ) {
	vertice verts[ 4 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &verts, 20 );
}

void Render::DirectX::triangle( Vector2D pos1, Vector2D pos2, Vector2D pos3, Color c ) {
	vertice verts[ 4 ] = {
		{ int( pos1.x ), int( pos1.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos2.x ), int( pos2.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos3.x ), int( pos3.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos1.x ), int( pos1.y ), 0.01f, 0.01f, c.ToARGB( ) },
	};

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, TRUE );
	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_LINESTRIP, 3, &verts, 20 );
	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
}

void Render::DirectX::triangle_filled( Vector2D pos1, Vector2D pos2, Vector2D pos3, Color c ) {
	vertice verts[ 3 ] = {
		{ int( pos1.x ), int( pos1.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos2.x ), int( pos2.y ), 0.01f, 0.01f, c.ToARGB( ) },
		{ int( pos3.x ), int( pos3.y ), 0.01f, 0.01f, c.ToARGB( ) },
	};

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, TRUE );
	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 1, &verts, 20 );
	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
}

void Render::DirectX::polygon_gradient( Vector2D pos1, Vector2D pos2, Vector2D pos3, Vector2D pos4, Color c_a, Color c_b ) {
	vertice verts[ 4 ] = {
		{ int( pos1.x ), int( pos1.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos2.x ), int( pos2.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos3.x ), int( pos3.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
		{ int( pos4.x ), int( pos4.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &verts, 20 );
}

void Render::DirectX::gradient_v( Vector2D pos, Vector2D size, Color c_a, Color c_b ) {
	vertice verts[ 4 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_b.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &verts, 20 );
}

void Render::DirectX::gradient_multi_color( Vector2D pos, Vector2D size, Color c_a, Color c_b, Color c_c, Color c_d ) {
	vertice verts[ 5 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_c.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_d.ToARGB( ) },
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, &verts, 20 );
}

void Render::DirectX::gradient_multi_color_filled( Vector2D pos, Vector2D size, Color c_a, Color c_b, Color c_c, Color c_d ) {
	vertice verts[ 4 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_c.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_d.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &verts, 20 );
}

void Render::DirectX::gradient_h( Vector2D pos, Vector2D size, Color c_a, Color c_b ) {
	vertice verts[ 4 ] = {
		{ int( pos.x ), int( pos.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y ), 0.01f, 0.01f, c_b.ToARGB( ) },
		{ int( pos.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_a.ToARGB( ) },
		{ int( pos.x + size.x ), int( pos.y + size.y ), 0.01f, 0.01f, c_b.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &verts, 20 );
}

void Render::DirectX::line( Vector2D a, Vector2D b, Color c ) {
	vertice verts[ 2 ] = {
		{ ( int )a.x, ( int )a.y, 0.01f, 0.01f, c.ToARGB( ) },
		{ ( int )b.x, ( int )b.y, 0.01f, 0.01f, c.ToARGB( ) }
	};

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_LINELIST, 1, &verts, 20 );
}

void Render::DirectX::arc( Vector2D center, float radius, float multiplier, Color c, bool antialias ) {
	const auto d3d_col = c.ToARGB( );
	const float pi = D3DX_PI * multiplier;
	const auto step_angle = pi / 180;
	const auto segments = 64;
	const auto verts = ( vertice* )_malloca( sizeof( vertice ) * ( segments + 2 ) );

	if( verts == nullptr )
		return;

	for( auto i = 0; i < segments + 2; i++ ) {
		verts[ i ].x = ( center.x - radius * std::cos( i * ( 2 * pi / segments ) ) );
		verts[ i ].y = ( center.y - radius * std::sin( i * ( 2 * pi / segments ) ) );
		verts[ i ].z = 0;
		verts[ i ].rhw = 1;
		verts[ i ].c = d3d_col;
	}

	for( auto i = 0; i < segments + 2; i++ ) {
		verts[ i ].x = center.x + std::cos( step_angle ) * ( verts[ i ].x - center.x ) - std::sin( step_angle ) * ( verts[ i ].y - center.y );
		verts[ i ].y = center.y + std::sin( step_angle ) * ( verts[ i ].x - center.x ) + std::cos( step_angle ) * ( verts[ i ].y - center.y );
	}

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, antialias );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, antialias );

	device->SetTexture( 0, nullptr );
	device->DrawPrimitiveUP( D3DPT_LINESTRIP, segments, verts, 20 );

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
}

void Render::DirectX::circle_filled( Vector2D center, float radius, int rotate, int type, bool smoothing, int resolution, Color c ) {
	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, TRUE );

	std::vector<vertice> circle( resolution + 2 );
	float angle = rotate * D3DX_PI / 180;
	float pi;

	if( type == 0 ) pi = D3DX_PI;        // Full circle
	if( type == 1 ) pi = D3DX_PI / 2;      // 1/2 circle
	if( type == 2 ) pi = D3DX_PI / 4;   // 1/4 circle

	circle[ 0 ].x = center.x;
	circle[ 0 ].y = center.y;
	circle[ 0 ].z = 0;
	circle[ 0 ].rhw = 1;
	circle[ 0 ].c = c.ToARGB( );

	for( int i = 1; i < resolution + 2; i++ ) {
		circle[ i ].x = ( float )( center.x - radius * cos( pi * ( ( i - 1 ) / ( resolution / 2.0f ) ) ) );
		circle[ i ].y = ( float )( center.y - radius * sin( pi * ( ( i - 1 ) / ( resolution / 2.0f ) ) ) );
		circle[ i ].z = 0;
		circle[ i ].rhw = 1;
		circle[ i ].c = c.ToARGB( );
	}

	// Rotate matrix
	int _res = resolution + 2;

	// Want smoke? blow me Nigga
	for( int i = 0; i < _res; i++ ) {
		circle[ i ].x = center.x + cos( angle ) * ( circle[ i ].x - center.x ) - sin( angle ) * ( circle[ i ].y - center.y );
		circle[ i ].y = center.y + sin( angle ) * ( circle[ i ].x - center.x ) - cos( angle ) * ( circle[ i ].y - center.y );
	}

	device->CreateVertexBuffer( ( resolution + 2 ) * sizeof( vertice ), D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &vert_buf, NULL );

	void* pVertices;
	vert_buf->Lock( 0, ( resolution + 2 ) * sizeof( vertice ), ( void** )&pVertices, 0 );
	memcpy( pVertices, &circle[ 0 ], ( resolution + 2 ) * sizeof( vertice ) );
	vert_buf->Unlock( );

	device->SetTexture( 0, NULL );
	device->SetPixelShader( NULL );
	device->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	device->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	device->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

	device->SetStreamSource( 0, vert_buf, 0, sizeof( vertice ) );
	device->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
	device->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, resolution );

	if( vert_buf != NULL )
		vert_buf->Release( );

	device->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
	device->SetRenderState( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
}

void Render::DirectX::circle_gradient( Vector2D center, float radius, int rotate, int type, int resolution, Color color, Color color2 ) {
	LPDIRECT3DVERTEXBUFFER9 g_pVB2;

	std::vector<vertice> circle( resolution + 2 );

	float angle = rotate * D3DX_PI / 180, pi = D3DX_PI;

	circle[ 0 ].x = center.x;
	circle[ 0 ].y = center.y;
	circle[ 0 ].z = 0;
	circle[ 0 ].rhw = 1;
	circle[ 0 ].c = color2.ToARGB( );

	for( int i = 1; i < resolution + 2; i++ )
	{
		circle[ i ].x = ( float )( center.x - radius * cos( pi * ( ( i - 1 ) / ( resolution / 2.0f ) ) ) );
		circle[ i ].y = ( float )( center.y - radius * sin( pi * ( ( i - 1 ) / ( resolution / 2.0f ) ) ) );
		circle[ i ].z = 0;
		circle[ i ].rhw = 1;
		circle[ i ].c = color.ToARGB( );
	}

	// Rotate matrix 
	int _res = resolution + 2;
	for( int i = 0; i < _res; i++ )
	{
		circle[ i ].x = center.x + cos( angle ) * ( circle[ i ].x - center.x ) - sin( angle ) * ( circle[ i ].y - center.y );
		circle[ i ].y = center.y + sin( angle ) * ( circle[ i ].x - center.x ) + cos( angle ) * ( circle[ i ].y - center.y );
	}

	device->CreateVertexBuffer( ( resolution + 2 ) * sizeof( vertice ), D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &g_pVB2, NULL );

	VOID* pVertices;
	g_pVB2->Lock( 0, ( resolution + 2 ) * sizeof( vertice ), ( void** )&pVertices, 0 );
	memcpy( pVertices, &circle[ 0 ], ( resolution + 2 ) * sizeof( vertice ) );
	g_pVB2->Unlock( );

	device->SetTexture( 0, NULL );
	device->SetPixelShader( NULL );
	device->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	device->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	device->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

	device->SetStreamSource( 0, g_pVB2, 0, sizeof( vertice ) );
	device->SetFVF( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
	device->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, resolution );
	if( g_pVB2 != NULL )
		g_pVB2->Release( );
}

void Render::DirectX::begin( ) {
	device->CreateStateBlock( D3DSBT_PIXELSTATE, &state_block );

	state_block->Capture( );
	device->GetVertexDeclaration( &vert_dec );
	device->GetVertexShader( &vert_shader );

	set_render_states( );
}

void Render::DirectX::end( ) {
	state_block->Apply( );
	state_block->Release( );

	device->SetVertexDeclaration( vert_dec );
	device->SetVertexShader( vert_shader );
}
#pragma endregion

#pragma region EngineRender

// ctor.
Render::Engine::Font::Font( const std::string& name, int s, int w, int flags ) {
	m_handle = Interfaces::m_pSurface->CreateFont_( );
	Interfaces::m_pSurface->SetFontGlyphSet( m_handle, name.data( ), s, w, 0, 0, flags );
	m_size = size( XorStr( "A" ) );
}

// ctor.
Render::Engine::Font::Font( HFont font ) {
	m_handle = font;
	m_size = size( XorStr( "A" ) );
}

void Render::Engine::Font::string( int x, int y, Color color, const std::string& text, StringFlags_t flags /*= Render::DirectX::ALIGN_LEFT */ ) {
	wstring( x, y, color, Math::MultiByteToWide( text ), flags );
}

//void Render::Engine::Font::string( int x, int y, Color color, const std::stringstream& text, StringFlags_t flags /*= Render::DirectX::ALIGN_LEFT */ ) {
//	wstring( x, y, color, Math::MultiByteToWide( text.str( ) ), flags );
//}

void Render::Engine::Font::wstring( int x, int y, Color color, const std::wstring& text, StringFlags_t flags /*= Render::DirectX::ALIGN_LEFT */ ) {
	int w, h;

	Interfaces::m_pSurface->GetTextSize( m_handle, text.c_str( ), w, h );
	Interfaces::m_pSurface->DrawSetTextFont( m_handle );
	Interfaces::m_pSurface->DrawSetTextColor( color );

	if( flags & Render::Engine::ALIGN_RIGHT )
		x -= w;

	if( flags & Render::Engine::ALIGN_CENTER )
		x -= w / 2;

	Interfaces::m_pSurface->DrawSetTextPos( x, y );
	Interfaces::m_pSurface->DrawPrintText( text.c_str( ), ( int )text.size( ) );
}

Render::Engine::FontSize_t Render::Engine::Font::size( const std::string& text ) {
	return wsize( Math::MultiByteToWide( text ) );
}

Render::Engine::FontSize_t Render::Engine::Font::wsize( const std::wstring& text ) {
	FontSize_t res;
	Interfaces::m_pSurface->GetTextSize( m_handle, text.data( ), res.m_width, res.m_height );
	return res;
}

namespace Render::Engine {
	Font esp;
	Font pixel;
	Font pixel_reg;
	Font console;
	Font hud;
	Font segoe;
	Font cs;
	Font cs_large;
	Font damage;
	Font icon;
	Font indi;

	int m_width;
	int m_height;
}

void Render::Engine::Initialise( ) {
	LoadFontFromResource( weaponicons, sizeof( weaponicons ) );

	esp = Font( XorStr( "Tahoma" ), 11, FW_NORMAL, FONTFLAG_DROPSHADOW );
	segoe = Font( XorStr( "Segoe UI" ), 13, FW_NORMAL, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW | FONTFLAG_DROPSHADOW );
	pixel = Font( XorStr( "04B03" ), 8, FW_NORMAL, FONTFLAG_OUTLINE );
	pixel_reg = Font( XorStr( "Small Fonts" ), 8, FW_NORMAL, FONTFLAG_OUTLINE );
	console = Font( XorStr( "Lucida Console" ), 10, FW_DONTCARE, FONTFLAG_DROPSHADOW );
	hud = Font( XorStr( "Tahoma" ), 13, FW_NORMAL, FONTFLAG_DROPSHADOW );
	cs = Font( XorStr( "WeaponIcons" ), 14, FW_NORMAL, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW );
	cs_large = Font( XorStr( "WeaponIcons" ), 22, FW_NORMAL, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW );
	damage = Font( XorStr( "Segoe UI" ), 26, FW_THIN, FONTFLAG_ANTIALIAS | FONTFLAG_DROPSHADOW );
	indi = Font( XorStr( "Tahoma" ), 15, FW_BOLD, FONTFLAG_ANTIALIAS );

	Interfaces::m_pEngine->GetScreenSize( m_width, m_height );
}

bool Render::Engine::WorldToScreen( const Vector& world, Vector2D& screen ) {
	float w;
	static ptrdiff_t ptrViewMatrix;
	if( !ptrViewMatrix ) {
		ptrViewMatrix = static_cast< ptrdiff_t >( Memory::Scan( XorStr( "client.dll" ), XorStr( "0F 10 05 ? ? ? ? 8D 85 ? ? ? ? B9" ) ) );
		ptrViewMatrix += 0x3;
		ptrViewMatrix = *reinterpret_cast< uintptr_t* >( ptrViewMatrix );
		ptrViewMatrix += 176;
	}

	const VMatrix& matrix = *( VMatrix* )ptrViewMatrix;

	// check if it's in view first.
	// note - dex; w is below 0 when world position is around -90 / +90 from the player's camera on the y axis.
	w = matrix[ 3 ][ 0 ] * world.x + matrix[ 3 ][ 1 ] * world.y + matrix[ 3 ][ 2 ] * world.z + matrix[ 3 ][ 3 ];
	if( w < 0.001f )
		return false;

	// calculate x and y.
	screen.x = matrix[ 0 ][ 0 ] * world.x + matrix[ 0 ][ 1 ] * world.y + matrix[ 0 ][ 2 ] * world.z + matrix[ 0 ][ 3 ];
	screen.y = matrix[ 1 ][ 0 ] * world.x + matrix[ 1 ][ 1 ] * world.y + matrix[ 1 ][ 2 ] * world.z + matrix[ 1 ][ 3 ];

	screen /= w;

	// calculate screen position.
	screen.x = ( m_width / 2 ) + ( screen.x * m_width ) / 2;
	screen.y = ( m_height / 2 ) - ( screen.y * m_height ) / 2;

	return true;
}

void Render::Engine::Line( Vector2D v0, Vector2D v1, Color color ) {
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawLine( v0.x, v0.y, v1.x, v1.y );
}

void Render::Engine::Polygon( int count, Vertex_t* vertices, const Color& col ) {
	static int texture_id;

	if( !Interfaces::m_pSurface->IsTextureIDValid( texture_id ) )
		texture_id = Interfaces::m_pSurface->CreateNewTextureID( );

	Interfaces::m_pSurface->DrawSetColor( col.RGBA[ 0 ], col.RGBA[ 1 ], col.RGBA[ 2 ], col.RGBA[ 3 ] );
	Interfaces::m_pSurface->DrawSetTexture( texture_id );
	Interfaces::m_pSurface->DrawTexturedPolygon( count, vertices );
}

void Render::Engine::FilledTriangle( const Vector2D& pos1, const Vector2D& pos2, const Vector2D& pos3, const Color& col ) {
	static Vertex_t triangle_vert[ 3 ];

	triangle_vert[ 0 ].Init( pos1 );
	triangle_vert[ 1 ].Init( pos2 );
	triangle_vert[ 2 ].Init( pos3 );

	Polygon( 3, triangle_vert, col );
}

void Render::Engine::WorldCircle( Vector origin, float radius, Color color, Color colorFill ) {
	std::vector<Vector> Points3D;
	float step = static_cast< float >( M_PI ) * 2.0f / 256;

	for( float a = 0; a < ( M_PI * 2.0f ); a += step ) {
		Vector start( radius * cosf( a ) + origin.x, radius * sinf( a ) + origin.y, origin.z );
		Vector end( radius * cosf( a + step ) + origin.x, radius * sinf( a + step ) + origin.y, origin.z );

		Vector2D out, out1, pos3d;

		if( Render::Engine::WorldToScreen( end, out1 ) && Render::Engine::WorldToScreen( start, out ) ) {
			if( colorFill.a( ) && Render::Engine::WorldToScreen( origin, pos3d ) ) {
				FilledTriangle( out, out1, pos3d, colorFill );
			}

			Line( out, out1, color );
		}
	}
}

void Render::Engine::Line( int x0, int y0, int x1, int y1, Color color ) {
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawLine( x0, y0, x1, y1 );
}

void Render::Engine::Rect( int x, int y, int w, int h, Color color ) {
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawOutlinedRect( x, y, x + w, y + h );
}

void Render::Engine::RectFilled( int x, int y, int w, int h, Color color ) {
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawFilledRect( x, y, x + w, y + h );
}

void Render::Engine::RectFilled( Vector2D pos, Vector2D size, Color color ) {
	Render::Engine::RectFilled( pos.x, pos.y, size.x, size.y, color );
}

void Render::Engine::RectOutlined( int x, int y, int w, int h, Color color, Color color2 ) {
	Rect( x, y, w, h, color );
	Rect( x - 1, y - 1, w + 2, h + 2, color2 );
	Rect( x + 1, y + 1, w - 2, h - 2, color2 );
}

// thanks nitro
void Render::Engine::CircleFilled( int x, int y, float radius, int segments, Color color )
{
	static int texture = Interfaces::m_pSurface->CreateNewTextureID( true );

	Interfaces::m_pSurface->DrawSetTextureRGBA( texture, Color::White( ).RGBA, 1, 1 );
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawSetTexture( texture );

	std::vector< Vertex_t > vertices{ };

	float step = ( M_PI * 2.f ) / segments;
	for( float i{ 0.f }; i < ( M_PI * 2.f ); i += step )
		vertices.emplace_back( Vector2D{ x + ( radius * std::cos( i ) ), y + ( radius * std::sin( i ) ) } );

	Interfaces::m_pSurface->DrawTexturedPolygon( vertices.size( ), vertices.data( ) );

	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawOutlinedCircle( x, y, radius, segments );
}

void Render::Engine::Gradient( int x, int y, int w, int h, Color color, Color color2, bool horizontal )
{
	Interfaces::m_pSurface->DrawSetColor( color );
	Interfaces::m_pSurface->DrawFilledRectFade( x, y, x + w, y + h, color.a( ), color.a( ), horizontal );

	Interfaces::m_pSurface->DrawSetColor( color2 );
	Interfaces::m_pSurface->DrawFilledRectFade( x, y, x + w, y + h, color2.a( ), color2.a( ), horizontal );
}

#pragma endregion

Vector2D Render::GetScreenSize( ) {
	return Vector2D( Render::Engine::m_width, Render::Engine::m_height );
}