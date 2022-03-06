#include "CVariables.hpp"
#include "../source.hpp"
#include "../Features/Miscellaneous/KitParser.hpp"
#include "CColor.hpp"
std::vector<KeyBind_t*> g_keybinds;

CVariables g_Vars;

FloatColor FloatColor::Black = FloatColor( 0.0f, 0.0f, 0.0f, 1.0f );
FloatColor FloatColor::White = FloatColor( 1.0f, 1.0f, 1.0f, 1.0f );
FloatColor FloatColor::Gray = FloatColor( 0.75f, 0.75f, 0.75f, 1.0f );

FloatColor FloatColor::Lerp( const FloatColor& dst, float t ) const {
   return FloatColor( ( dst.r - r ) * t + r, ( dst.g - g ) * t + g, ( dst.b - b ) * t + b, ( dst.a - a ) * t + a );
}

void FloatColor::SetColor( Color clr ) {
    r = static_cast< float >( clr.r( ) ) / 255.0f;
    g = static_cast< float >( clr.g( ) ) / 255.0f;
    b = static_cast< float >( clr.b( ) ) / 255.0f;
    a = static_cast< float >( clr.a( ) ) / 255.0f;
}

Color FloatColor::ToRegularColor( ) {
    return Color( r * 255.f, g * 255.f, b * 255.f, a * 255.f );
}

void CVariables::Create( ) {
   // add weapons
   m_skin_changer.SetName( XorStr( "skin_changer" ) );
   for ( const auto& value : weapon_skins ) {
	  auto idx = m_skin_changer.AddEntry( );
	  auto entry = m_skin_changer[ idx ];
	  entry->m_definition_index = value.id;
	  entry->SetName( value.name );
   }

   this->AddChild( &m_skin_changer );

   sv_accelerate = Interfaces::m_pCvar->FindVar( XorStr( "sv_accelerate" ) );
   sv_airaccelerate = Interfaces::m_pCvar->FindVar( XorStr( "sv_airaccelerate" ) );
   sv_gravity = Interfaces::m_pCvar->FindVar( XorStr( "sv_gravity" ) );
   sv_jump_impulse = Interfaces::m_pCvar->FindVar( XorStr( "sv_jump_impulse" ) );
   sv_penetration_type = Interfaces::m_pCvar->FindVar( XorStr( "sv_penetration_type" ) );

   m_yaw = Interfaces::m_pCvar->FindVar( XorStr( "m_yaw" ) );
   m_pitch = Interfaces::m_pCvar->FindVar( XorStr( "m_pitch" ) );
   sensitivity = Interfaces::m_pCvar->FindVar( XorStr( "sensitivity" ) );

   cl_sidespeed = Interfaces::m_pCvar->FindVar( XorStr( "cl_sidespeed" ) );
   cl_forwardspeed = Interfaces::m_pCvar->FindVar( XorStr( "cl_forwardspeed" ) );
   cl_upspeed = Interfaces::m_pCvar->FindVar( XorStr( "cl_upspeed" ) );
   cl_extrapolate = Interfaces::m_pCvar->FindVar( XorStr( "cl_extrapolate" ) );

   sv_noclipspeed = Interfaces::m_pCvar->FindVar( XorStr( "sv_noclipspeed" ) );

   weapon_recoil_scale = Interfaces::m_pCvar->FindVar( XorStr( "weapon_recoil_scale" ) );
   view_recoil_tracking = Interfaces::m_pCvar->FindVar( XorStr( "view_recoil_tracking" ) );

   r_jiggle_bones = Interfaces::m_pCvar->FindVar( XorStr( "r_jiggle_bones" ) );

   mp_friendlyfire = Interfaces::m_pCvar->FindVar( XorStr( "mp_friendlyfire" ) );

   sv_maxunlag = Interfaces::m_pCvar->FindVar( XorStr( "sv_maxunlag" ) );
   sv_minupdaterate = Interfaces::m_pCvar->FindVar( XorStr( "sv_minupdaterate" ) );
   sv_maxupdaterate = Interfaces::m_pCvar->FindVar( XorStr( "sv_maxupdaterate" ) );

   sv_client_min_interp_ratio = Interfaces::m_pCvar->FindVar( XorStr( "sv_client_min_interp_ratio" ) );
   sv_client_max_interp_ratio = Interfaces::m_pCvar->FindVar( XorStr( "sv_client_max_interp_ratio" ) );

   cl_interp_ratio = Interfaces::m_pCvar->FindVar( XorStr( "cl_interp_ratio" ) );
   cl_interp = Interfaces::m_pCvar->FindVar( XorStr( "cl_interp" ) );
   cl_updaterate = Interfaces::m_pCvar->FindVar( XorStr( "cl_updaterate" ) );

   game_type = Interfaces::m_pCvar->FindVar( XorStr( "game_type" ) );
   game_mode = Interfaces::m_pCvar->FindVar( XorStr( "game_mode" ) );

   ff_damage_bullet_penetration = Interfaces::m_pCvar->FindVar( XorStr( "ff_damage_bullet_penetration" ) );
   ff_damage_reduction_bullets = Interfaces::m_pCvar->FindVar( XorStr( "ff_damage_reduction_bullets" ) );

   mp_damage_scale_ct_head = Interfaces::m_pCvar->FindVar( XorStr( "mp_damage_scale_ct_head" ) );
   mp_damage_scale_t_head = Interfaces::m_pCvar->FindVar( XorStr( "mp_damage_scale_t_head" ) );
   mp_damage_scale_ct_body = Interfaces::m_pCvar->FindVar( XorStr( "mp_damage_scale_ct_body" ) );
   mp_damage_scale_t_body = Interfaces::m_pCvar->FindVar( XorStr( "mp_damage_scale_t_body" ) );

   viewmodel_fov = Interfaces::m_pCvar->FindVar( XorStr( "viewmodel_fov" ) );
   viewmodel_offset_x = Interfaces::m_pCvar->FindVar( XorStr( "viewmodel_offset_x" ) );
   viewmodel_offset_y = Interfaces::m_pCvar->FindVar( XorStr( "viewmodel_offset_y" ) );
   viewmodel_offset_z = Interfaces::m_pCvar->FindVar( XorStr( "viewmodel_offset_z" ) );

   mat_ambient_light_r = Interfaces::m_pCvar->FindVar( XorStr( "mat_ambient_light_r" ) );
   mat_ambient_light_g = Interfaces::m_pCvar->FindVar( XorStr( "mat_ambient_light_g" ) );
   mat_ambient_light_b = Interfaces::m_pCvar->FindVar( XorStr( "mat_ambient_light_b" ) );

   sv_show_impacts = Interfaces::m_pCvar->FindVar( XorStr( "sv_showimpacts" ) );

   molotov_throw_detonate_time = Interfaces::m_pCvar->FindVar( XorStr( "molotov_throw_detonate_time" ) );
   weapon_molotov_maxdetonateslope = Interfaces::m_pCvar->FindVar( XorStr( "weapon_molotov_maxdetonateslope" ) );
   net_client_steamdatagram_enable_override = Interfaces::m_pCvar->FindVar( XorStr( "net_client_steamdatagram_enable_override" ) );
   mm_dedicated_search_maxping = Interfaces::m_pCvar->FindVar( XorStr( "mm_dedicated_search_maxping" ) );

   cl_csm_shadows = Interfaces::m_pCvar->FindVar( XorStr( "cl_csm_shadows" ) );

   r_drawmodelstatsoverlay = Interfaces::m_pCvar->FindVar(XorStr("r_drawmodelstatsoverlay"));
   host_limitlocal = Interfaces::m_pCvar->FindVar(XorStr("host_limitlocal"));

   sv_clockcorrection_msecs = Interfaces::m_pCvar->FindVar( XorStr( "sv_clockcorrection_msecs" ) );
   sv_max_usercmd_future_ticks = Interfaces::m_pCvar->FindVar( XorStr( "sv_max_usercmd_future_ticks" ) );
   sv_maxusrcmdprocessticks = Interfaces::m_pCvar->FindVar( XorStr( "sv_maxusrcmdprocessticks" ) );
   crosshair = Interfaces::m_pCvar->FindVar( XorStr( "crosshair" ) );
   engine_no_focus_sleep = Interfaces::m_pCvar->FindVar( XorStr( "engine_no_focus_sleep" ) );

   r_3dsky = Interfaces::m_pCvar->FindVar( XorStr( "r_3dsky" ) );
   r_RainRadius = Interfaces::m_pCvar->FindVar( XorStr( "r_RainRadius" ) );
   r_rainalpha = Interfaces::m_pCvar->FindVar( XorStr( "r_rainalpha" ) );
   /*developer = Interfaces::m_pCvar->FindVar( XorStr( "developer" ) );
   con_enable = Interfaces::m_pCvar->FindVar( XorStr( "con_enable" ) );
   con_filter_enable = Interfaces::m_pCvar->FindVar( XorStr( "con_filter_enable" ) );
   con_filter_text = Interfaces::m_pCvar->FindVar( XorStr( "con_filter_text" ) );
   con_filter_text_out = Interfaces::m_pCvar->FindVar( XorStr( "con_filter_text_out" ) );
   contimes = Interfaces::m_pCvar->FindVar( XorStr( "contimes" ) );*/
}

