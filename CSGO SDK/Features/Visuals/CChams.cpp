#include "CChams.hpp"
#include "../../source.hpp"
#include <fstream>
#include "../../SDK/Classes/Player.hpp"
#include "../../SDK/Classes/IMaterialSystem.hpp"
#include "../../SDK/CVariables.hpp"
#include "../Rage/LagCompensation.hpp"
#include "../Game/SetupBones.hpp"
#include "../../Hooking/hooked.hpp"
#include "../../SDK/displacement.hpp"
#include "../Rage/ShotInformation.hpp"
#include "../Game/Prediction.hpp"

extern C_AnimationLayer FakeAnimLayers[ 13 ];

#pragma optimize( "", off )

namespace Interfaces
{
	enum ChamsMaterials {
		MATERIAL_OFF = 0,
		MATERIAL_FLAT = 1,
		MATERIAL_REGULAR,
		MATERIAL_GLOW,
		MATERIAL_OUTLINE,
		MATERIAL_SHINY,
	};

	struct C_HitMatrixEntry {
		int ent_index;
		ModelRenderInfo_t info;
		DrawModelState_t state;
		matrix3x4_t pBoneToWorld[ 128 ] = { };
		float time;
		matrix3x4_t model_to_world;
	};

	class CChams : public IChams {
	public:
		void OnDrawModel( void* ECX, IMatRenderContext* MatRenderContext, DrawModelState_t& DrawModelState, ModelRenderInfo_t& RenderInfo, matrix3x4_t* pBoneToWorld ) override;
		void DrawModel( void* ECX, IMatRenderContext* MatRenderContext, DrawModelState_t& DrawModelState, ModelRenderInfo_t& RenderInfo, matrix3x4_t* pBoneToWorld ) override;
		bool GetBacktrackMatrix( C_CSPlayer* player, matrix3x4_t* out );
		void OnPostScreenEffects( ) override;
		bool IsVisibleScan( C_CSPlayer* player );	  virtual void AddHitmatrix( C_CSPlayer* player, matrix3x4_t* bones );

		CChams( );
		~CChams( );

		virtual bool CreateMaterials( ) {
			if( m_bInit )
				return true;

			m_matRegular = Interfaces::m_pMatSystem->FindMaterial( ( "debug/debugambientcube" ), nullptr );
			m_matFlat = Interfaces::m_pMatSystem->FindMaterial( ( "debug/debugdrawflat" ), nullptr );
			m_matGlow = Interfaces::m_pMatSystem->FindMaterial( ( "dev/glow_armsrace" ), nullptr );

			std::ofstream( "csgo\\materials\\pdr_shine.vmt" ) << R"#("VertexLitGeneric"
			{
					"$basetexture" "vgui/white_additive"
					"$ignorez"      "0"
					"$phong"        "1"
					"$BasemapAlphaPhongMask"        "1"
					"$phongexponent" "15"
					"$normalmapalphaenvmask" "1"
					"$envmap"       "env_cubemap"
					"$envmaptint"   "[0.6 0.6 0.6]"
					"$phongboost"   "[0.6 0.6 0.6]"
					"phongfresnelranges"   "[0.5 0.5 1.0]"
					"$nofog"        "1"
					"$model"        "1"
					"$nocull"       "0"
					"$selfillum"    "1"
					"$halflambert"  "1"
					"$znearer"      "0"
					"$flat"         "1"
			}
			)#";

			m_matShiny = Interfaces::m_pMatSystem->FindMaterial( XorStr( "pdr_shine" ), TEXTURE_GROUP_MODEL );

			if( !m_matRegular || m_matRegular == nullptr || m_matRegular->IsErrorMaterial( ) )
				return false;

			if( !m_matFlat || m_matFlat == nullptr || m_matFlat->IsErrorMaterial( ) )
				return false;

			if( !m_matGlow || m_matGlow == nullptr || m_matGlow->IsErrorMaterial( ) )
				return false;

			if( !m_matShiny || m_matShiny == nullptr || m_matShiny->IsErrorMaterial( ) )
				return false;

			m_bInit = true;
			return true;
		}

	private:
		void OverrideMaterial( bool ignoreZ, int type, const FloatColor& rgba, float glow_mod = 0, bool wf = false, const FloatColor& pearlescence_clr = { }, float pearlescence = 1.f, float shine = 1.f );

		bool m_bInit = false;
		IMaterial* m_matFlat = nullptr;
		IMaterial* m_matRegular = nullptr;
		IMaterial* m_matGlow = nullptr;
		IMaterial* m_matShiny = nullptr;

		std::vector<C_HitMatrixEntry> m_Hitmatrix;
	};

	Encrypted_t<IChams> IChams::Get( ) {
		static CChams instance;
		return &instance;
	}

	CChams::CChams( ) {

	}

	CChams::~CChams( ) {

	}

	void CChams::OnPostScreenEffects( ) {
		auto pLocal = C_CSPlayer::GetLocalPlayer( );

		if( !g_Vars.globals.HackIsReady || !Interfaces::m_pEngine->IsConnected( ) || !Interfaces::m_pEngine->IsInGame( ) || !pLocal ) {
			m_Hitmatrix.clear( );
			return;
		}

		if( m_Hitmatrix.empty( ) )
			return;

		if( !Interfaces::m_pStudioRender.IsValid( ) )
			return;

		auto ctx = Interfaces::m_pMatSystem->GetRenderContext( );

		if( !ctx )
			return;

		auto DrawModelRebuild = [ & ] ( C_HitMatrixEntry it ) -> void {
			if( !g_Vars.r_drawmodelstatsoverlay )
				return;

			DrawModelResults_t results;
			DrawModelInfo_t info;
			ColorMeshInfo_t* pColorMeshes = NULL;
			info.m_bStaticLighting = false;
			info.m_pStudioHdr = it.state.m_pStudioHdr;
			info.m_pHardwareData = it.state.m_pStudioHWData;
			info.m_Skin = it.info.skin;
			info.m_Body = it.info.body;
			info.m_HitboxSet = it.info.hitboxset;
			info.m_pClientEntity = ( IClientRenderable* )it.state.m_pRenderable;
			info.m_Lod = it.state.m_lod;
			info.m_pColorMeshes = pColorMeshes;

			bool bShadowDepth = ( it.info.flags & STUDIO_SHADOWDEPTHTEXTURE ) != 0;

			// Don't do decals if shadow depth mapping...
			info.m_Decals = bShadowDepth ? STUDIORENDER_DECAL_INVALID : it.state.m_decals;

			// Sets up flexes
			float* pFlexWeights = NULL;
			float* pFlexDelayedWeights = NULL;

			int overlayVal = g_Vars.r_drawmodelstatsoverlay->GetInt( );
			int drawFlags = it.state.m_drawFlags;

			if( bShadowDepth ) {
				drawFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
				drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
			}

			if( overlayVal && !bShadowDepth ) {
				drawFlags |= STUDIORENDER_DRAW_GET_PERF_STATS;
			}

			Interfaces::m_pStudioRender->DrawModel( &results, &info, it.pBoneToWorld, pFlexWeights, pFlexDelayedWeights, it.info.origin, drawFlags );
			Interfaces::m_pStudioRender->m_pForcedMaterial = nullptr;
			Interfaces::m_pStudioRender->m_nForcedMaterialType = 0;
		};

		auto it = m_Hitmatrix.begin( );
		while( it != m_Hitmatrix.end( ) ) {
			if( !( &it->state ) || !it->state.m_pModelToWorld || !it->state.m_pRenderable || !it->state.m_pStudioHdr || !it->state.m_pStudioHWData ||
				!it->info.pRenderable || !it->info.pModelToWorld || !it->info.pModel ) {
				++it;
				continue;
			}

			auto alpha = 1.0f;
			auto delta = Interfaces::m_pGlobalVars->realtime - it->time;
			if( delta > 0.0f ) {
				alpha -= delta;
				if( delta > 1.0f ) {
					it = m_Hitmatrix.erase( it );
					continue;
				}
			}

			auto color = g_Vars.esp.hitmatrix_color;
			color.a *= alpha;

			OverrideMaterial( true, 4, color );

			DrawModelRebuild( *it );

			if( g_Vars.esp.chams_hitmatrix_outline ) {
				OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_hitmatrix_outline_color, g_Vars.esp.chams_hitmatrix_outline_value );
				DrawModelRebuild( *it );
			}

			++it;
		}
	}

	void CChams::AddHitmatrix( C_CSPlayer* player, matrix3x4_t* bones ) {
		if( !player || !bones )
			return;

		auto& hit = m_Hitmatrix.emplace_back( );

		std::memcpy( hit.pBoneToWorld, bones, player->m_CachedBoneData( ).Count( ) * sizeof( matrix3x4_t ) );
		hit.time = Interfaces::m_pGlobalVars->realtime + g_Vars.esp.hitmatrix_time;

		static int m_nSkin = SDK::Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_nSkin" ) );
		static int m_nBody = SDK::Memory::FindInDataMap( player->GetPredDescMap( ), XorStr( "m_nBody" ) );

		hit.info.origin = player->GetAbsOrigin( );
		hit.info.angles = player->GetAbsAngles( );

		auto renderable = player->GetClientRenderable( );

		if( !renderable )
			return;

		auto model = player->GetModel( );

		if( !model )
			return;

		auto hdr = *( studiohdr_t** )( player->m_pStudioHdr( ) );

		if( !hdr )
			return;

		hit.state.m_pStudioHdr = hdr;
		hit.state.m_pStudioHWData = Interfaces::m_pMDLCache->GetHardwareData( model->studio );
		hit.state.m_pRenderable = renderable;
		hit.state.m_drawFlags = 0;

		hit.info.pRenderable = renderable;
		hit.info.pModel = model;
		hit.info.pLightingOffset = nullptr;
		hit.info.pLightingOrigin = nullptr;
		hit.info.hitboxset = player->m_nHitboxSet( );
		hit.info.skin = *( int* )( uintptr_t( player ) + m_nSkin );
		hit.info.body = *( int* )( uintptr_t( player ) + m_nBody );
		hit.info.entity_index = player->m_entIndex;
		hit.info.instance = Memory::VCall<ModelInstanceHandle_t( __thiscall* )( void* ) >( renderable, 30u )( renderable );
		hit.info.flags = 0x1;

		hit.info.pModelToWorld = &hit.model_to_world;
		hit.state.m_pModelToWorld = &hit.model_to_world;

		hit.model_to_world.AngleMatrix( hit.info.angles, hit.info.origin );
	}

	void CChams::OverrideMaterial( bool ignoreZ, int type, const FloatColor& rgba, float glow_mod, bool wf, const FloatColor& pearlescence_clr, float pearlescence, float shine ) {
		IMaterial* material = nullptr;

		switch( type ) {
		case MATERIAL_OFF: break;
		case MATERIAL_FLAT:
			material = m_matFlat; break;
		case MATERIAL_REGULAR:
			material = m_matRegular; break;
		case MATERIAL_GLOW:
		case MATERIAL_OUTLINE:
			material = m_matGlow; break;
		case MATERIAL_SHINY:
			material = m_matShiny; break;
		}

		if( !material ) {
			Interfaces::m_pStudioRender->m_pForcedMaterial = nullptr;
			Interfaces::m_pStudioRender->m_nForcedMaterialType = 0;
			return;
		}

		// apparently we have to do this, otherwise SetMaterialVarFlag can cause crashes (I crashed once here when loading a different cfg)
		material->IncrementReferenceCount( );

		material->SetMaterialVarFlag( MATERIAL_VAR_IGNOREZ, ignoreZ );
		material->SetMaterialVarFlag( MATERIAL_VAR_WIREFRAME, wf );

		if( type == MATERIAL_GLOW ) {
			auto tint = material->FindVar( XorStr( "$envmaptint" ), nullptr );
			if( tint )
				tint->SetVecValue( rgba.r,
					rgba.g,
					rgba.b );

			auto alpha = material->FindVar( XorStr( "$alpha" ), nullptr );
			if( alpha )
				alpha->SetFloatValue( rgba.a );

			auto envmap = material->FindVar( XorStr( "$envmapfresnelminmaxexp" ), nullptr );
			if( envmap )
				envmap->SetVecValue( 0.f, 1.f, glow_mod );
		}
		else if( type == MATERIAL_OUTLINE ) {
			auto tint = material->FindVar( XorStr( "$envmaptint" ), nullptr );
			if( tint )
				tint->SetVecValue( rgba.r,
					rgba.g,
					rgba.b );

			auto alpha = material->FindVar( XorStr( "$alpha" ), nullptr );
			if( alpha )
				alpha->SetFloatValue( rgba.a );

			auto envmap = material->FindVar( XorStr( "$envmapfresnelminmaxexp" ), nullptr );
			if( envmap )
				envmap->SetVecValue( 0.f, 1.f, 8.0f );
		}
		else if( type == MATERIAL_SHINY ) {
			material->AlphaModulate( rgba.a );
			material->ColorModulate( rgba.r, rgba.g, rgba.b );
			
			auto tint = material->FindVar( XorStr( "$envmaptint" ), nullptr );
			if( tint )
				tint->SetVecValue( pearlescence_clr.r * ( pearlescence / 100.f ),
					pearlescence_clr.g * ( pearlescence / 100.f ),
					pearlescence_clr.b * ( pearlescence / 100.f ) );

			auto envmap = material->FindVar( XorStr( "$phongboost" ), nullptr );
			if( envmap )
				envmap->SetVecValue( shine / 100.f, shine / 100.f, shine / 100.f );
		}
		else {
			material->AlphaModulate(
				rgba.a );

			material->ColorModulate(
				rgba.r,
				rgba.g,
				rgba.b );
		}

		Interfaces::m_pStudioRender->m_pForcedMaterial = material;
		Interfaces::m_pStudioRender->m_nForcedMaterialType = 0;
	}

	void CChams::DrawModel( void* ECX, IMatRenderContext* MatRenderContext, DrawModelState_t& DrawModelState, ModelRenderInfo_t& RenderInfo, matrix3x4_t* pBoneToWorld ) {
		if( !Interfaces::m_pStudioRender.IsValid( ) )
			goto end;

		if( !CreateMaterials( ) )
			goto end;

		//if( !g_Vars.esp.chams_enabled )
		//	goto end_func;

		C_CSPlayer* local = C_CSPlayer::GetLocalPlayer( );
		if( !local || !Interfaces::m_pEngine->IsInGame( ) )
			goto end;

		static float pulse_alpha = 0.f;
		static bool change_alpha = false;

		if( pulse_alpha <= 0.f )
			change_alpha = true;
		else if( pulse_alpha >= 255.f )
			change_alpha = false;

		pulse_alpha = change_alpha ? pulse_alpha + 0.05f : pulse_alpha - 0.05f;

		// STUDIO_SHADOWDEPTHTEXTURE(used for shadows)
		if( RenderInfo.flags & 0x40000000
			|| !RenderInfo.pRenderable
			|| !RenderInfo.pRenderable->GetIClientUnknown( ) ) {
			goto end;
		}

		// already have forced material ( glow outline ) 
		if( g_Vars.globals.m_bInPostScreenEffects
			|| !pBoneToWorld
			|| !RenderInfo.pModel
			//|| RenderInfo.flags == 0x40100001 
			) {
			goto end;
		}

		auto entity = ( C_CSPlayer* )( RenderInfo.pRenderable->GetIClientUnknown( )->GetBaseEntity( ) );
		if( !entity )
			goto end;

		auto client_class = entity->GetClientClass( );
		if( !client_class )
			goto end;

		if( !client_class->m_ClassID )
			goto end;

		if( client_class->m_ClassID == ClassId_t::CBaseAnimating ) {
			if( g_Vars.esp.remove_sleeves && strstr( DrawModelState.m_pStudioHdr->szName, XorStr( "sleeve" ) ) != nullptr )
				return;
		}

		auto InvalidateMaterial = [ & ] ( ) -> void {
			Interfaces::m_pStudioRender->m_pForcedMaterial = nullptr;
			Interfaces::m_pStudioRender->m_nForcedMaterialType = 0;
		};

		if( g_Vars.esp.chams_attachments ) {
			if( client_class->m_ClassID == ClassId_t::CBaseWeaponWorldModel || client_class->m_ClassID == ClassId_t::CBreakableProp ) {
				if( !entity )
					goto end;

				// ???
				if( entity->m_nModelIndex( ) >= 67000000 )
					goto end;

				if( client_class->m_ClassID == ClassId_t::CBreakableProp ) {
					if( !entity->moveparent( ).IsValid( ) )
						goto end;
				}

				if( client_class->m_ClassID == ClassId_t::CBaseWeaponWorldModel ) {
					if( !entity->m_hCombatWeaponParent( ).IsValid( ) )
						goto end;
				}

				C_BaseEntity* owner = nullptr;
				if( client_class->m_ClassID == ClassId_t::CBaseWeaponWorldModel )
					owner = ( C_CSPlayer* )( ( ( C_CSPlayer* )entity->m_hCombatWeaponParent( ).Get( ) )->m_hOwnerEntity( ).Get( ) );
				else
					owner = ( C_CSPlayer* )entity->moveparent( ).Get( );

				if( owner ) {
					if( g_Vars.esp.chams_attachments ) {
						if( owner->EntIndex( ) != local->EntIndex( ) )
							goto end;
					}
				}

				int material = g_Vars.esp.attachments_chams_mat;

				OverrideMaterial( false, material, g_Vars.esp.attachments_chams_color, 0.f );
				Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

				if( g_Vars.esp.chams_attachments_outline ) {
					OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_attachments_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_attachments_outline_value ) * 0.2f, 1.f, 20.f ), g_Vars.esp.chams_attachments_outline_wireframe );
					Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
				}

				InvalidateMaterial( );
				return;
			}
		}

		if( client_class->m_ClassID == ClassId_t::CPredictedViewModel ) {
			if( !g_Vars.esp.chams_weapon )
				goto end;

			int material = g_Vars.esp.weapon_chams_mat;

			OverrideMaterial( false, material, g_Vars.esp.weapon_chams_color, 0.f );
			Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

			if( g_Vars.esp.chams_weapon_outline ) {
				OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_weapon_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_weapon_outline_value ) * 0.2f, 1.f, 20.f ), g_Vars.esp.chams_weapon_outline_wireframe );
				Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
			}

			InvalidateMaterial( );
			return;
		}
		else if( client_class->m_ClassID == ClassId_t::CBaseAnimating ) {
			if( !g_Vars.esp.chams_hands )
				goto end;

			int material = g_Vars.esp.hands_chams_mat;


			OverrideMaterial( false, material, g_Vars.esp.hands_chams_color, 0.f );
			Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

			if( g_Vars.esp.chams_hands_outline ) {
				OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_hands_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_hands_outline_value ) * 0.2f, 1.f, 20.f ), g_Vars.esp.chams_hands_outline_wireframe );
				Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
			}

			InvalidateMaterial( );
			return;
		}
		else if( entity && entity->IsPlayer( ) && !entity->IsDormant( ) && !entity->IsDead( ) && entity->m_entIndex >= 1 && entity->m_entIndex <= Interfaces::m_pGlobalVars->maxClients || entity->GetClientClass( )->m_ClassID == ClassId_t::CCSRagdoll ) {
			bool is_local_player = false, is_enemy = false, is_teammate = false;
			if( entity->EntIndex( ) == local->EntIndex( ) )
				is_local_player = true;
			else if( entity->m_iTeamNum( ) != local->m_iTeamNum( ) )
				is_enemy = true;
			else
				is_teammate = true;

			if( entity == local ) {
				if( g_Vars.esp.blur_in_scoped && Interfaces::m_pInput->CAM_IsThirdPerson( ) ) {
					if( local && !local->IsDead( ) && local->m_bIsScoped( ) ) {
						Interfaces::m_pRenderView->SetBlend( g_Vars.esp.blur_in_scoped_value / 100.f );
					}
				}
			}

			bool ragdoll = entity->GetClientClass( )->m_ClassID == ClassId_t::CCSRagdoll;

			auto vis = is_teammate ? g_Vars.esp.team_chams_color_vis : g_Vars.esp.enemy_chams_color_vis;
			auto xqz = is_teammate ? g_Vars.esp.team_chams_color_xqz : g_Vars.esp.enemy_chams_color_xqz;
			bool should_xqz = is_teammate ? g_Vars.esp.team_chams_xqz : g_Vars.esp.enemy_chams_xqz;
			bool should_vis = is_teammate ? g_Vars.esp.team_chams_vis : g_Vars.esp.enemy_chams_vis;
			if( is_local_player ) {
				//set local player ghost chams
				static auto g_GameRules = *( uintptr_t** )( Engine::Displacement.Data.m_GameRules );
				bool invalid = g_GameRules && *( bool* )( *( uintptr_t* )g_GameRules + 0x20 ) || ( entity->m_fFlags( ) & ( 1 << 6 ) );

				//set local player chams
				if( g_Vars.esp.chams_local ) {
					OverrideMaterial( false, g_Vars.esp.chams_local_mat, g_Vars.esp.chams_local_color, 0.f, false,
						g_Vars.esp.chams_local_pearlescence_color, g_Vars.esp.chams_local_pearlescence, g_Vars.esp.chams_local_shine );
					
					Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

					if( g_Vars.esp.chams_local_outline ) {
						OverrideMaterial( false, MATERIAL_GLOW, g_Vars.esp.chams_local_outline_color,
							std::clamp<float>( ( 100.0f - g_Vars.esp.chams_local_outline_value ) * 0.2f, 1.f, 20.f ), g_Vars.esp.chams_local_outline_wireframe );

						Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
					}
				}
				else {
					OverrideMaterial( false, 0, vis );
					Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
				}

				InvalidateMaterial( );
				return;
			}
			else if( is_enemy ) {
				auto data = Engine::LagCompensation::Get( )->GetLagData( entity->m_entIndex );
				if( data.IsValid( ) ) {
					if( g_Vars.esp.chams_history ) {
						// start from begin
						matrix3x4_t out[ 128 ];
						if( CChams::GetBacktrackMatrix( entity, out ) ) {
							OverrideMaterial( true, g_Vars.esp.chams_history_mat, g_Vars.esp.chams_history_color );
							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, out );

							if( g_Vars.esp.chams_history_outline ) {
								OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_history_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_history_outline_value ) * 0.2f, 1.f, 20.f ) );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, out );
							}

							InvalidateMaterial( );
						}
					}
				}
				//set enemy chams
				if( g_Vars.esp.chams_enemy ) {
					if( !ragdoll ) {
						int material = g_Vars.esp.enemy_chams_mat;

						if( should_xqz ) {
							OverrideMaterial( true, material, xqz, 0.f, false,
								g_Vars.esp.chams_enemy_pearlescence_color, g_Vars.esp.chams_enemy_pearlescence, g_Vars.esp.chams_enemy_shine );
							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

							InvalidateMaterial( );
						}

						if( should_vis ) {
							OverrideMaterial( false, material, vis, 0.f, false,
								g_Vars.esp.chams_enemy_pearlescence_color, g_Vars.esp.chams_enemy_pearlescence, g_Vars.esp.chams_enemy_shine );
							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

							InvalidateMaterial( );
						}

						if( g_Vars.esp.chams_enemy_outline ) {
							OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_enemy_outline_color,
								std::clamp<float>( ( 100.0f - g_Vars.esp.chams_enemy_outline_value ) * 0.2f, 1.f, 20.f ), g_Vars.esp.chams_enemy_outline_wireframe );

							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );

							InvalidateMaterial( );
						}
					}
				}
				else {
					if( !ragdoll ) {
						InvalidateMaterial( );
						Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
					}
				}

				InvalidateMaterial( );
				return;
			}
			else if( is_teammate ) {
				//set teammate chams
				/*if( g_Vars.esp.chams_death_teammate ) {
					if( ragdoll ) {
						int material = g_Vars.esp.team_chams_death_mat;
						if( g_Vars.esp.chams_teammate_death_pulse ) {
							if( should_xqz_death ) {
								FloatColor clr = xqz_death;
								clr.a = pulse_alpha / 255.f;
								OverrideMaterial( true, material, clr );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}

							if( should_vis_death ) {
								FloatColor clr = vis_death;
								clr.a = pulse_alpha / 255.f;
								OverrideMaterial( false, material, clr );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}
						}
						else {
							if( should_xqz_death ) {
								OverrideMaterial( true, material, xqz_death );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}

							if( should_vis_death ) {
								OverrideMaterial( false, material, vis_death );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}
						}

						if( g_Vars.esp.chams_teammate_death_outline ) {
							OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_teammate_death_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_teammate_death_outline_value ) * 0.2f, 1.f, 20.f ) );
							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
						}
					}
				}
				else {
					if( ragdoll ) {
						InvalidateMaterial( );
						Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
					}
				}*/

				/*//set teammate chams
				if( g_Vars.esp.chams_teammate ) {
					if( !ragdoll ) {
						int material = g_Vars.esp.team_chams_mat;
						if( g_Vars.esp.chams_teammate_pulse ) {
							if( should_xqz ) {
								FloatColor clr = xqz;
								clr.a = pulse_alpha;
								OverrideMaterial( true, material, clr );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}

							if( should_vis ) {
								FloatColor clr = vis;
								clr.a = pulse_alpha;
								OverrideMaterial( false, material, clr );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}
						}
						else {
							if( should_xqz ) {
								OverrideMaterial( true, material, xqz );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}

							if( should_vis ) {
								OverrideMaterial( false, material, vis );
								Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
							}
						}

						if( g_Vars.esp.chams_teammate_outline ) {
							OverrideMaterial( true, MATERIAL_GLOW, g_Vars.esp.chams_teammate_outline_color, std::clamp<float>( ( 100.0f - g_Vars.esp.chams_teammate_outline_value ) * 0.2f, 1.f, 20.f ) );
							Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
						}
					}
				}
				else {
					if( !ragdoll ) {
						InvalidateMaterial( );
						Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
					}
				}*/

				if( !ragdoll ) {
					InvalidateMaterial( );
					Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
				}
				return;
			}
		}
	end:
		local = C_CSPlayer::GetLocalPlayer( );

		if( !Interfaces::m_pEngine->IsInGame( ) || !Interfaces::m_pEngine->IsConnected( ) || !local )
			return;
		else {
			Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
			return;
		}

	end_func:
		Hooked::oDrawModelExecute( ECX, MatRenderContext, DrawModelState, RenderInfo, pBoneToWorld );
		return;

	}

	void CChams::OnDrawModel( void* ECX, IMatRenderContext* MatRenderContext, DrawModelState_t& DrawModelState, ModelRenderInfo_t& RenderInfo, matrix3x4_t* pBoneToWorld ) {

	}

	bool CChams::GetBacktrackMatrix( C_CSPlayer* entity, matrix3x4_t* out ) {
		if( !entity )
			return false;

		auto data = Engine::LagCompensation::Get( )->GetLagData( entity->m_entIndex );
		if( data.IsValid( ) ) {
			// start from begin
			for( auto it = data->m_History.begin( ); it != data->m_History.end( ); ++it ) {
				if( it->player != entity )
					break;

				std::pair< Engine::C_LagRecord*, Engine::C_LagRecord* > last;
				if( !Engine::LagCompensation::Get( )->IsRecordOutOfBounds( *it, 0.2f, -1, false ) && it + 1 != data->m_History.end( ) && Engine::LagCompensation::Get( )->IsRecordOutOfBounds( *( it + 1 ), 0.2f, -1, false ) )
					last = std::make_pair( &*( it + 1 ), &*it );

				if( !last.first || !last.second )
					continue;

				if( !Interfaces::m_pPrediction->GetUnpredictedGlobals( ) )
					continue;

				const auto& FirstInvalid = last.first;
				const auto& LastInvalid = last.second;

				if( !LastInvalid || !FirstInvalid )
					continue;

				if( LastInvalid->m_flSimulationTime - FirstInvalid->m_flSimulationTime > 0.5f )
					continue;

				if( ( FirstInvalid->m_vecOrigin - entity->m_vecOrigin( ) ).Length( ) < 2.5f )
					return false;

				const auto NextOrigin = LastInvalid->m_vecOrigin;
				const auto curtime = Interfaces::m_pPrediction->GetUnpredictedGlobals( )->curtime;

				auto flDelta = 1.f - ( curtime - LastInvalid->m_flInterpolateTime ) / ( LastInvalid->m_flSimulationTime - FirstInvalid->m_flSimulationTime );
				if( flDelta < 0.f || flDelta > 1.f )
					LastInvalid->m_flInterpolateTime = curtime;

				flDelta = 1.f - ( curtime - LastInvalid->m_flInterpolateTime ) / ( LastInvalid->m_flSimulationTime - FirstInvalid->m_flSimulationTime );

				const auto lerp = Math::Interpolate( NextOrigin, FirstInvalid->m_vecOrigin, std::clamp( flDelta, 0.f, 1.f ) );

				matrix3x4_t ret[ 128 ];
				memcpy( ret, FirstInvalid->m_BoneMatrix, sizeof( matrix3x4_t[ 128 ] ) );

				for( size_t i{ }; i < 128; ++i ) {
					const auto matrix_delta = Math::MatrixGetOrigin( FirstInvalid->m_BoneMatrix[ i ] ) - FirstInvalid->m_vecOrigin;
					Math::MatrixSetOrigin( matrix_delta + lerp, ret[ i ] );
				}

				memcpy( out, ret, sizeof( matrix3x4_t[ 128 ] ) );
				return true;
			}
		}
	}
}

#pragma optimize( "", on )