#include "stdafx.h"
#include "..\xrRender_DXTargets\DX.h"
#include "../xrRender/fbasicvisual.h"
#include "../../xrEngine/xr_object.h"
#include "../../xrEngine/CustomHUD.h"
#include "../../xrEngine/igame_persistent.h"
#include "../../xrEngine/environment.h"
#include "../xrRender/SkeletonCustom.h"
#include "../xrRender/LightTrack.h"
#include "../xrRender/dxRenderDeviceRender.h"
#include "../xrRender/dxWallMarkArray.h"
#include "../xrRender/dxUIShader.h"

#include "../xrRenderDX10/3DFluid/dx103DFluidManager.h"
#include "../xrRender/ShaderResourceTraits.h"

#include "D3DX10Core.h"

CRender										RImplementation;
CRenderMSAA									RMSAA;

//////////////////////////////////////////////////////////////////////////
class CGlow				: public IRender_Glow
{
public:
	bool				bActive;
public:
	CGlow() : bActive(false)		{ }
	virtual void					set_active			(bool b)					{ bActive=b;		}
	virtual bool					get_active			()							{ return bActive;	}
	virtual void					set_position		(const Fvector& P)			{ }
	virtual void					set_direction		(const Fvector& D)			{ }
	virtual void					set_radius			(float R)					{ }
	virtual void					set_texture			(LPCSTR name)				{ }
	virtual void					set_color			(const Fcolor& C)			{ }
	virtual void					set_color			(float r, float g, float b)	{ }
};

bool CRender::is_sun() 
{
    Fcolor sun_color = ((light*)Lights.sun_adapted._get())->color;
    return (ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r, sun_color.g, sun_color.b)>EPS));
}

float		r_dtex_range		= 50.f;
//////////////////////////////////////////////////////////////////////////
ShaderElement*			CRender::rimp_select_sh_dynamic	(dxRender_Visual	*pVisual, float cdist_sq)
{
	int		id	= SE_R2_SHADOW;
	if	(CRender::PHASE_NORMAL == RImplementation.phase)
	{
		id = ((_sqrt(cdist_sq)-pVisual->vis.sphere.R)<r_dtex_range)?SE_R2_NORMAL_HQ:SE_R2_NORMAL_LQ;
	}
	return pVisual->shader->E[id]._get();
}
//////////////////////////////////////////////////////////////////////////
ShaderElement*			CRender::rimp_select_sh_static	(dxRender_Visual	*pVisual, float cdist_sq)
{
	int		id	= SE_R2_SHADOW;
	if	(CRender::PHASE_NORMAL == RImplementation.phase)
	{
		id = ((_sqrt(cdist_sq)-pVisual->vis.sphere.R)<r_dtex_range)?SE_R2_NORMAL_HQ:SE_R2_NORMAL_LQ;
	}
	return pVisual->shader->E[id]._get();
}
static class cl_parallax		: public R_constant_setup		{	virtual void setup	(R_constant* C)
{
	float			h			=	ps_r2_df_parallax_h;
	RCache.set_c	(C,h,-h/2.f,1.f/r_dtex_range,1.f/r_dtex_range);
}}	binder_parallax;

#ifdef DIRECTX11
static class cl_LOD		: public R_constant_setup
{
	virtual void setup	(R_constant* C)
	{
		RCache.LOD.set_LOD(C);
	}
} binder_LOD;
#endif

static class cl_pos_decompress_params		: public R_constant_setup		{	virtual void setup	(R_constant* C)
{
	float VertTan =  -1.0f * tanf( deg2rad(Device.fFOV/2.0f ) );
	float HorzTan =  - VertTan / Device.fASPECT;

	RCache.set_c	( C, HorzTan, VertTan, ( 2.0f * HorzTan )/(float)Device.dwWidth, ( 2.0f * VertTan ) /(float)Device.dwHeight );

}}	binder_pos_decompress_params;

static class cl_pos_decompress_params2		: public R_constant_setup		{	virtual void setup	(R_constant* C)
{
	RCache.set_c	(C,(float)Device.dwWidth, (float)Device.dwHeight, 1.0f/(float)Device.dwWidth, 1.0f/(float)Device.dwHeight );

}}	binder_pos_decompress_params2;

static class cl_water_intensity : public R_constant_setup		
{	
	virtual void setup	(R_constant* C)
	{
		CEnvDescriptor&	E = *g_pGamePersistent->Environment().CurrentEnv;
		float fValue = E.m_fWaterIntensity;
		RCache.set_c	(C, fValue, fValue, fValue, 0);
	}
}	binder_water_intensity;

static class cl_tree_amplitude_intensity : public R_constant_setup
{
    virtual void setup(R_constant* C)
    {
        CEnvDescriptor&	E = *g_pGamePersistent->Environment().CurrentEnv;
        float fValue = E.m_fTreeAmplitudeIntensity;
        RCache.set_c(C, fValue, fValue, fValue, 0);
    }
} binder_tree_amplitude_intensity;

static class cl_sun_shafts_intensity : public R_constant_setup		
{	
	virtual void setup	(R_constant* C)
	{
		CEnvDescriptor&	E = *g_pGamePersistent->Environment().CurrentEnv;
		float fValue = E.m_fSunShaftsIntensity;
		RCache.set_c	(C, fValue, fValue, fValue, 0);
	}
}	binder_sun_shafts_intensity;

static class cl_alpha_ref	: public R_constant_setup 
{	
	virtual void setup (R_constant* C) 
	{ 
		StateManager.BindAlphaRef(C);
	}
} binder_alpha_ref;

extern float debug_fog_height; extern float debug_fog_density; extern float debug_fog_max_dist; extern float debug_fog_min_dist;
static class cl_fog_shaders : public R_constant_setup
{
	virtual void setup(R_constant* C)
	{
		CEnvDescriptor& E = *g_pGamePersistent->Environment().CurrentEnv;
		float fValue1 = E.m_fFogShaders_height;
		float fValue2 = E.m_fFogShaders_density;
		float fValue3 = E.m_fFogShaders_max_dist;
		float fValue4 = E.m_fFogShaders_min_dist;
		RCache.set_c(C, fValue1 + debug_fog_height, fValue2 + debug_fog_density, fValue3 + debug_fog_max_dist, fValue4 + debug_fog_min_dist);
	}
} binder_fog_shaders;

static class cl_volumetric_fog_param : public R_constant_setup
{
	virtual void setup(R_constant* C)
	{
		RCache.set_c(C, ps_render_volumetric_fog, 0, 0, 0);
	}
}	binder_volumetric_fog;

//////////////////////////////////////////////////////////////////////////
// Just two static storage
void					CRender::create					()
{
	Device.seqFrame.Add	(this,REG_PRIORITY_HIGH+0x12345678);
	
	m_skinning			= -1;
	m_MSAASample		= -1;

	// hardware
	o.smapsize			= ps_r2_smapsize;
	o.mrt				= (DEVICE_HW::CRYRAY_RENDER::HW.Caps.raster.dwMRT_count >= 3);
	o.mrtmixdepth		= (DEVICE_HW::CRYRAY_RENDER::HW.Caps.raster.b_MRT_mixdepth);

	o.nullrt = false;
	
	if (o.nullrt)		
	{
		Msg				("* NULLRT supported");
		if (0)
		{
			u32 device_id	= DEVICE_HW::CRYRAY_RENDER::HW.Caps.id_device;
			bool disable_nullrt = false;
			switch (device_id)	
			{
			case 0x190:
			case 0x191:
			case 0x192:
			case 0x193:
			case 0x194:
			case 0x197:
			case 0x19D:
			case 0x19E:{
				disable_nullrt = true;	//G80
				break;
					   }
			case 0x400:
			case 0x401:
			case 0x402:
			case 0x403:
			case 0x404:
			case 0x405:
			case 0x40E:
			case 0x40F:{
				disable_nullrt = true;	//G84
				break;
					   }
			case 0x420:
			case 0x421:
			case 0x422:
			case 0x423:
			case 0x424:
			case 0x42D:
			case 0x42E:
			case 0x42F:{
				disable_nullrt = true;	// G86
				break;
					   }
			}
			if (disable_nullrt)	o.nullrt=false;
		};
		if (o.nullrt)	Msg				("* ...and used");
	};

	// SMAP / DST
	o.HW_smap_FETCH4	= FALSE;
	o.HW_smap			= true;
	o.HW_smap_PCF		= o.HW_smap		;
	if (o.HW_smap)		
	{
		//	For ATI it's much faster on DX10 to use D32F format
		if (DEVICE_HW::CRYRAY_RENDER::HW.Caps.id_vendor==0x1002)
			o.HW_smap_FORMAT	= D3DFMT_D32F_LOCKABLE;
		else
			o.HW_smap_FORMAT	= D3DFMT_D24X8;
		Msg				("* HWDST/PCF supported and used");
	}

	//	DX10 disabled
	o.fp16_filter		= true;
	o.fp16_blend		= true;

	// search for ATI formats
	if (!o.HW_smap && (0==strstr(Core.Params,"-nodf24")) )		
	{
		o.HW_smap		= DEVICE_HW::CRYRAY_RENDER::HW.support	((D3DFORMAT)(MAKEFOURCC('D','F','2','4')),	D3DRTYPE_TEXTURE,D3DUSAGE_DEPTHSTENCIL);
		if (o.HW_smap)	{
			o.HW_smap_FORMAT= MAKEFOURCC	('D','F','2','4');
			o.HW_smap_PCF	= FALSE			;
			o.HW_smap_FETCH4= TRUE			;
		}
		Msg				("* DF24/F4 supported and used [%X]", o.HW_smap_FORMAT);
	}

	// emulate ATI-R4xx series
	if (strstr(Core.Params,"-r4xx"))	
	{
		o.mrtmixdepth	= FALSE;
		o.HW_smap		= FALSE;
		o.HW_smap_PCF	= FALSE;
		o.fp16_filter	= FALSE;
		o.fp16_blend	= FALSE;
	}

	VERIFY2				(o.mrt && (DEVICE_HW::CRYRAY_RENDER::HW.Caps.raster.dwInstructions>=256),"Hardware doesn't meet minimum feature-level");
	if (o.mrtmixdepth)		o.albedo_wo		= FALSE	;
	else if (o.fp16_blend)	o.albedo_wo		= FALSE	;
	else					o.albedo_wo		= TRUE	;

	// nvstencil on NV40 and up
	o.nvstencil			= FALSE;
	if (strstr(Core.Params,"-nonvs"))		o.nvstencil	= FALSE;

	// nv-dbt
	o.nvdbt				= false;
	if (o.nvdbt)		Msg	("* NV-DBT supported and used");

	// gloss
	char*	g			= strstr(Core.Params,"-gloss ");
	o.forcegloss		= g?	TRUE	:FALSE	;
	if (g)				
		o.forcegloss_v		= float	(atoi	(g+xr_strlen("-gloss ")))/255.f;

	// options
	o.bug				= (strstr(Core.Params,"-bug"))?			TRUE	:FALSE	;
	o.sunfilter			= (strstr(Core.Params,"-sunfilter"))?	TRUE	:FALSE	;
	o.advancedpp		= FullRenderingFunctionality;
	if (o.advancedpp)
		Msg("# [CryRay Engine RenderINFO]: The full functionality of the renderer is active!");

	o.volumetricfog		= ps_r2_ls_flags.test(R3FLAG_VOLUMETRIC_SMOKE);
	o.sjitter			= (strstr(Core.Params,"-sjitter"))?		TRUE	:FALSE	;
	o.depth16			= (strstr(Core.Params,"-depth16"))?		TRUE	:FALSE	;
	o.noshadows			= (strstr(Core.Params,"-noshadows"))?	TRUE	:FALSE	;
	o.Tshadows			= (strstr(Core.Params,"-tsh"))?			TRUE	:FALSE	;
	o.mblur				= ps_r__common_flags.test(RFLAGDX10_MOTION_BLUR);
	o.distortion_enabled= (strstr(Core.Params,"-nodistort"))?	FALSE	:TRUE	;
	o.distortion		= o.distortion_enabled;
	o.disasm			= (strstr(Core.Params,"-disasm"))?		TRUE	:FALSE	;
	o.forceskinw		= (strstr(Core.Params,"-skinw"))?		TRUE	:FALSE	;

	o.no_ram_textures = ps_r__common_flags.test(RFLAG_NO_RAM_TEXTURES);
	if (o.no_ram_textures)
		Msg("# [CryRay Engine RenderINFO]: Managed textures disabled");

	o.ssao_blur_on		= p_engine_flags32.test(R2FLAGEXT_SSAO_BLUR) && (ps_r_ssao != 0);
	o.ssao_opt_data		= p_engine_flags32.test(R2FLAGEXT_SSAO_OPT_DATA) && (ps_r_ssao != 0);
	o.ssao_half_data	= p_engine_flags32.test(R2FLAGEXT_SSAO_HALF_DATA) && o.ssao_opt_data && (ps_r_ssao != 0);
	o.ssao_hdao			= p_engine_flags32.test(R2FLAGEXT_SSAO_HDAO) && (ps_r_ssao != 0);
	o.ssao_hbao			= !o.ssao_hdao && p_engine_flags32.test(R2FLAGEXT_SSAO_HBAO) && (ps_r_ssao != 0);
	o.ssao_ssdo			= !o.ssao_hdao && !o.ssao_hbao && p_engine_flags32.test(R2FLAGEXT_SSAO_SSDO) && (ps_r_ssao != 0);

	Msg("# [CryRay Engine RenderINFO]: Information about MSAA with xrEngine, selected token: %i", ps_r3_msaa);
	Msg("# [CryRay Engine RenderINFO]: Information about MSAA A-Test with xrEngine, selected token: %i", ps_r3_msaa_atest);
	Msg("# [CryRay Engine RenderINFO]: Information about 'Sun Quality' with xrEngine, selected token: %i", ps_r_sun_quality);

	//	TODO: fix hbao shader to allow to perform per-subsample effect!
	// LV da da idi nahuy suka 
#ifdef DIRECTX11
	if (o.ssao_hbao)
	{
		o.ssao_opt_data = true;
		if (o.ssao_opt_data == true)
			Msg("# [CryRay Engine RenderINFO]: SSAO->HBAO (opt_data) ON");
	}
	else if (o.ssao_hdao)
	{
		o.ssao_opt_data = false;
		if (o.ssao_opt_data == false)
			Msg("# [CryRay Engine RenderINFO]: SSAO->HDAO (opt_data) OFF");
	}

	//OldSerpskiStalker
	o.dx11		= renderer_value == 2 || renderer_value == 3;
	o.dx11		= o.dx11 && ( DEVICE_HW::CRYRAY_RENDER::HW.FeatureLevel >= D3D_FEATURE_LEVEL_11_0 );
#else
	if (o.ssao_hdao)
	{
		o.ssao_opt_data = false;
		if (o.ssao_opt_data == false)
			Msg("# [CryRay Engine RenderINFO]: SSAO->HDAO (opt_data) OFF");
	}
	else if (o.ssao_hbao)
	{
		o.ssao_opt_data = true;
		if (o.ssao_opt_data == true)
			Msg("# [CryRay Engine RenderINFO]: SSAO->HBAO (opt_data) ON");
	}

	o.dx10_1 = ps_r2_ls_flags.test((u32)R3FLAG_USE_DX10_1);
	if (o.dx10_1)
	{
		EnvCryRay.used_dx10_1 = 1;
		Msg("# [CryRay Engine RenderINFO]: used DX10.1: %i", EnvCryRay.used_dx10_1);
	}
	o.dx10_1 = o.dx10_1 && ( DEVICE_HW::CRYRAY_RENDER::HW.pDevice1 != 0 );
#endif
	RMSAA._opt.dx10_msaa			= !!ps_r3_msaa;
	RMSAA._opt.dx10_msaa_samples    = (1 << ps_r3_msaa);

	//-' OldSerpskiStalker. Реворк загрузки механизма MSAA
	RMSAA._opt.full_rendering_msaa	= (RMSAA._opt.dx10_msaa && o.dx10_1) || (DEVICE_HW::CRYRAY_RENDER::HW.FeatureLevel >= D3D_FEATURE_LEVEL_11_0);
	RMSAA._opt.dx10_msaa_alphatest = 0;

	if (RMSAA._opt.dx10_msaa)
	{
		if (RMSAA._opt.dx10_msaa)
			Msg("# [CryRay Engine RenderINFO]: RMSAA._opt.dx10_msaa ON");

		if (RMSAA._opt.full_rendering_msaa)
		{
			if (RMSAA._opt.full_rendering_msaa)
				Msg("# [CryRay Engine RenderINFO]: RMSAA._opt.full_rendering_msaa ON");

			if (ps_r3_msaa_atest == 1)
			{
				RMSAA._opt.dx10_msaa_alphatest = MSAA_ATEST_DX10_1_ATOC;
				Msg("# [CryRay Engine RenderINFO]: MSAA_ATEST_DX10_1_ATOC ON");
			}
			else if (ps_r3_msaa_atest == 2)
			{
				RMSAA._opt.dx10_msaa_alphatest = MSAA_ATEST_DX10_1_NATIVE;
				Msg("# [CryRay Engine RenderINFO]: MSAA_ATEST_DX10_1_NATIVE ON");
			}
		}
		else
		{
			if (ps_r3_msaa_atest == 1)
			{
				RMSAA._opt.dx10_msaa_alphatest = MSAA_ATEST_DX10_0_ATOC;
				Msg("# [CryRay Engine RenderINFO]: MSAA_ATEST_DX10_0_ATOC ON");
			}
		}
	}

	o.dx10_minmax_sm = ps_r3_minmax_sm;
	o.dx10_minmax_sm_screenarea_threshold = 1600*1200;
#ifdef DIRECTX11
	o.dx11_enable_tessellation = DEVICE_HW::CRYRAY_RENDER::HW.FeatureLevel>=D3D_FEATURE_LEVEL_11_0 && ps_r2_ls_flags_ext.test(R2FLAGEXT_ENABLE_TESSELLATION);
#endif
	if (o.dx10_minmax_sm==MMSM_AUTODETECT)
	{
		o.dx10_minmax_sm = MMSM_OFF;

		//	AMD device
		if (DEVICE_HW::CRYRAY_RENDER::HW.Caps.id_vendor==0x1002)
		{
			if (ps_r_sun_quality>=3)
				o.dx10_minmax_sm=MMSM_AUTO;
			else if (ps_r_sun_shafts>=2)
			{
				o.dx10_minmax_sm=MMSM_AUTODETECT;
				//	Check resolution in runtime in use_minmax_sm_this_frame
				o.dx10_minmax_sm_screenarea_threshold = 1600*1200;
			}
		}

		//	NVidia boards
		if (DEVICE_HW::CRYRAY_RENDER::HW.Caps.id_vendor==0x10DE)
		{
			if ((ps_r_sun_shafts>=2))
			{
				o.dx10_minmax_sm=MMSM_AUTODETECT;
				//	Check resolution in runtime in use_minmax_sm_this_frame
				o.dx10_minmax_sm_screenarea_threshold = 1280*1024;
			}
		}
	}

	// constants
	CResourceManager* RM = dxRenderDeviceRender::Instance().Resources;
	RM->RegisterConstantSetup("parallax", &binder_parallax);
	RM->RegisterConstantSetup("water_intensity", &binder_water_intensity);
	RM->RegisterConstantSetup("sun_shafts_intensity", &binder_sun_shafts_intensity);
	RM->RegisterConstantSetup("m_AlphaRef", &binder_alpha_ref);
	RM->RegisterConstantSetup("pos_decompression_params", &binder_pos_decompress_params);
	RM->RegisterConstantSetup("pos_decompression_params2", &binder_pos_decompress_params2);
#ifdef DIRECTX11
	RM->RegisterConstantSetup("triLOD", &binder_LOD);
#endif
	RM->RegisterConstantSetup("fog_shaders_values", &binder_fog_shaders);
	RM->RegisterConstantSetup("volumetric_fog", &binder_volumetric_fog);

	c_lmaterial					= "L_material";
	c_sbase						= "s_base";

	m_bMakeAsyncSS				= false;

	Target						= xr_new<CRenderTarget>		();	// Main target

	Models						= xr_new<CModelPool>		();
	PSLibrary.OnCreate			();
//	HWOCC.occq_create			(occq_size);

	rmNormal					();
	marker						= 0;
	D3D_QUERY_DESC			qdesc;
	qdesc.MiscFlags				= 0;
	qdesc.Query					= D3D_QUERY_EVENT;
	ZeroMemory(q_sync_point, sizeof(q_sync_point));
	
	for (u32 i=0; i<DEVICE_HW::CRYRAY_RENDER::HW.Caps.iGPUNum; ++i)
		R_CHK(DEVICE_HW::CRYRAY_RENDER::HW.pRenderDevice->CreateQuery(&qdesc,&q_sync_point[i]));
#ifdef DIRECTX11
	DEVICE_HW::CRYRAY_RENDER::HW.pRenderContext->End(q_sync_point[0]);
#else
	q_sync_point[0]->End();
#endif
	::PortalTraverser.initialize();
	FluidManager.Initialize( 70, 70, 70 );
	FluidManager.SetScreenSize(Device.dwWidth, Device.dwHeight);
}

void					CRender::destroy				()
{
	m_bMakeAsyncSS				= false;
	FluidManager.Destroy();
	::PortalTraverser.destroy	();
	for (u32 i=0; i<DEVICE_HW::CRYRAY_RENDER::HW.Caps.iGPUNum; ++i)
		_RELEASE				(q_sync_point[i]);
	
	HWOCC.occq_destroy			();
	xr_delete					(Models);
	xr_delete					(Target);
	PSLibrary.OnDestroy			();
	Device.seqFrame.Remove		(this);
	r_dsgraph_destroy			();
}

void CRender::reset_begin()
{
	// Update incremental shadowmap-visibility solver
	// BUG-ID: 10646
	{
		u32 it=0;
		for (it=0; it<Lights_LastFrame.size(); it++)	{
			if (0==Lights_LastFrame[it])	continue	;
			try {
				Lights_LastFrame[it]->svis.resetoccq ()	;
			} catch (...)
			{
				Msg	("! Failed to flush-OCCq on light [%d] %X",it,*(u32*)(&Lights_LastFrame[it]));
			}
		}
		Lights_LastFrame.clear	();
	}

    //AVO: let's reload details while changed details options on vid_restart
    if (b_loaded && ((dm_current_size != dm_size) || (ps_r__Detail_density != ps_current_detail_density)))
    {
        Details->Unload();
        xr_delete(Details);
    }
    //-AVO

	xr_delete					(Target);
	HWOCC.occq_destroy			();
	for (u32 i=0; i<DEVICE_HW::CRYRAY_RENDER::HW.Caps.iGPUNum; ++i)
		_RELEASE				(q_sync_point[i]);
}

void CRender::reset_end()
{
	D3D_QUERY_DESC			qdesc;
	qdesc.MiscFlags				= 0;
	qdesc.Query					= D3D_QUERY_EVENT;
	for (u32 i=0; i<DEVICE_HW::CRYRAY_RENDER::HW.Caps.iGPUNum; ++i)
		R_CHK(DEVICE_HW::CRYRAY_RENDER::HW.pRenderDevice->CreateQuery(&qdesc,&q_sync_point[i]));
	//	Prevent error on first get data
#ifdef DIRECTX11
	DEVICE_HW::CRYRAY_RENDER::HW.pRenderContext->End(q_sync_point[0]);
#else
	q_sync_point[0]->End();
#endif
//	HWOCC.occq_create			(occq_size);

	Target						=	xr_new<CRenderTarget>	();

    //AVO: let's reload details while changed details options on vid_restart
    if (b_loaded && ((dm_current_size != dm_size) || (ps_r__Detail_density != ps_current_detail_density)))
    {
        Details = xr_new<CDetailManager>();
        Details->Load();
    }
    //-AVO

	FluidManager.SetScreenSize(Device.dwWidth, Device.dwHeight);

	// Set this flag true to skip the first render frame,
	// that some data is not ready in the first frame (for example device camera position)
	m_bFirstFrameAfterReset		= true;
}

void CRender::OnFrame()
{
	Models->DeleteQueue			();
	if (ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC))	{
		// MT-details (@front)
		Device.seqParallel.insert	(Device.seqParallel.begin(),
			fastdelegate::FastDelegate0<>(Details,&CDetailManager::MT_CALC));

		// MT-HOM (@front)
		Device.seqParallel.insert	(Device.seqParallel.begin(),
			fastdelegate::FastDelegate0<>(&HOM,&CHOM::MT_RENDER));
	}
}

// Implementation
IRender_ObjectSpecific*	CRender::ros_create				(IRenderable* parent)				{ return xr_new<CROS_impl>();			}
void					CRender::ros_destroy			(IRender_ObjectSpecific* &p)		{ xr_delete(p);							}
IRenderVisual*			CRender::model_Create			(LPCSTR name, IReader* data)		{ return Models->Create(name,data);		}
IRenderVisual*			CRender::model_CreateChild		(LPCSTR name, IReader* data)		{ return Models->CreateChild(name,data);}
IRenderVisual*			CRender::model_Duplicate		(IRenderVisual* V)					{ return Models->Instance_Duplicate((dxRender_Visual*)V);	}

void					CRender::model_Delete			(IRenderVisual* &V, BOOL bDiscard)	
{ 
	dxRender_Visual* pVisual = (dxRender_Visual*)V;
	Models->Delete(pVisual, bDiscard);
	V = 0;
}

IRender_DetailModel*	CRender::model_CreateDM			(IReader*	F)
{
	CDetail*	D		= xr_new<CDetail> ();
	D->Load				(F);
	return D;
}

void					CRender::model_Delete			(IRender_DetailModel* & F)
{
	if (F)
	{
		CDetail*	D	= (CDetail*)F;
		D->Unload		();
		xr_delete		(D);
		F				= NULL;
	}
}

IRenderVisual*			CRender::model_CreatePE			(LPCSTR name)	
{ 
	PS::CPEDef*	SE			= PSLibrary.FindPED	(name);		R_ASSERT3(SE,"Particle effect doesn't exist",name);
	return					Models->CreatePE	(SE);
}

IRenderVisual*			CRender::model_CreateParticles	(LPCSTR name)	
{ 
	PS::CPEDef*	SE			= PSLibrary.FindPED	(name);
	if (SE) return			Models->CreatePE	(SE);
	else{
		PS::CPGDef*	SG		= PSLibrary.FindPGD	(name);		R_ASSERT3(SG,"Particle effect or group doesn't exist",name);
		return				Models->CreatePG	(SG);
	}
}

void					CRender::models_Prefetch		()					{ Models->Prefetch	();}
void					CRender::models_Clear			(BOOL b_complete)	{ Models->ClearPool	(b_complete);}

ref_shader				CRender::getShader				(int id)			{ VERIFY(id<int(Shaders.size()));	return Shaders[id];	}
IRender_Portal*			CRender::getPortal				(int id)			{ VERIFY(id<int(Portals.size()));	return Portals[id];	}
IRender_Sector*			CRender::getSector				(int id)			{ VERIFY(id<int(Sectors.size()));	return Sectors[id];	}
IRender_Sector*			CRender::getSectorActive		()					{ return pLastSector;									}
IRenderVisual*			CRender::getVisual				(int id)			{ VERIFY(id<int(Visuals.size()));	return Visuals[id];	}
D3DVERTEXELEMENT9*		CRender::getVB_Format			(int id, BOOL	_alt)	{ 
	if (_alt)	{ VERIFY(id<int(xDC.size()));	return xDC[id].begin();	}
	else		{ VERIFY(id<int(nDC.size()));	return nDC[id].begin(); }
}
ID3DVertexBuffer*	CRender::getVB					(int id, BOOL	_alt)	{
	if (_alt)	{ VERIFY(id<int(xVB.size()));	return xVB[id];		}
	else		{ VERIFY(id<int(nVB.size()));	return nVB[id];		}
}
ID3DIndexBuffer*	CRender::getIB					(int id, BOOL	_alt)	{ 
	if (_alt)	{ VERIFY(id<int(xIB.size()));	return xIB[id];		}
	else		{ VERIFY(id<int(nIB.size()));	return nIB[id];		}
}
FSlideWindowItem*		CRender::getSWI					(int id)			{ VERIFY(id<int(SWIs.size()));		return &SWIs[id];	}
IRender_Target*			CRender::getTarget				()					{ return Target;										}

IRender_Light*			CRender::light_create			()					{ return Lights.Create();								}
IRender_Glow*			CRender::glow_create			()					{ return xr_new<CGlow>();								}

void					CRender::flush					()					{ r_dsgraph_render_graph	(0);						}

BOOL					CRender::occ_visible			(vis_data& P)		{ return HOM.visible(P);								}
BOOL					CRender::occ_visible			(sPoly& P)			{ return HOM.visible(P);								}
BOOL					CRender::occ_visible			(Fbox& P)			{ return HOM.visible(P);								}

void					CRender::add_Visual				(IRenderVisual*		V )	{ add_leafs_Dynamic((dxRender_Visual*)V, V->_ignore_optimization);								}
void					CRender::add_Geometry			(IRenderVisual*		V )	{ add_Static((dxRender_Visual*)V,View->getMask());					}
void					CRender::add_StaticWallmark		(ref_shader& S, const Fvector& P, float s, CDB::TRI* T, Fvector* verts)
{
	if (T->suppress_wm)	return;
	VERIFY2							(_valid(P) && _valid(s) && T && verts && (s>EPS_L), "Invalid static wallmark params");
	Wallmarks->AddStaticWallmark	(T,verts,P,&*S,s);
}

void CRender::add_StaticWallmark			(IWallMarkArray *pArray, const Fvector& P, float s, CDB::TRI* T, Fvector* V)
{
	dxWallMarkArray *pWMA = (dxWallMarkArray *)pArray;
	ref_shader *pShader = pWMA->dxGenerateWallmark();
	if (pShader) add_StaticWallmark		(*pShader, P, s, T, V);
}

void CRender::add_StaticWallmark			(const wm_shader& S, const Fvector& P, float s, CDB::TRI* T, Fvector* V)
{
	dxUIShader* pShader = (dxUIShader*)&*S;
	add_StaticWallmark		(pShader->hShader, P, s, T, V);
}

void					CRender::clear_static_wallmarks	()
{
	Wallmarks->clear				();
}

void					CRender::add_SkeletonWallmark	(intrusive_ptr<CSkeletonWallmark> wm)
{
	Wallmarks->AddSkeletonWallmark				(wm);
}

void					CRender::add_SkeletonWallmark	(const Fmatrix* xf, CKinematics* obj, ref_shader& sh, const Fvector& start, const Fvector& dir, float size)
{
	Wallmarks->AddSkeletonWallmark				(xf, obj, sh, start, dir, size);
}

void					CRender::add_SkeletonWallmark	(const Fmatrix* xf, IKinematics* obj, IWallMarkArray *pArray, const Fvector& start, const Fvector& dir, float size)
{
	dxWallMarkArray *pWMA = (dxWallMarkArray *)pArray;
	ref_shader *pShader = pWMA->dxGenerateWallmark();
	if (pShader) add_SkeletonWallmark(xf, (CKinematics*)obj, *pShader, start, dir, size);
}

void CRender::add_Occluder			(Fbox2&	bb_screenspace	)
{
	HOM.occlude			(bb_screenspace);
}

void					CRender::set_Object				(IRenderable*	O )	
{ 
	val_pObject				= O;
}

void					CRender::rmNear				()
{
	IRender_Target* T	=	getTarget	();
#ifdef DIRECTX11
	D3D_VIEWPORT VP		=	{0,0,(float)T->get_width(),(float)T->get_height(),0,0.02f };
	
	DEVICE_HW::CRYRAY_RENDER::HW.pRenderContext->RSSetViewports(1, &VP);
#else
	D3D_VIEWPORT VP		=	{0,0,T->get_width(),T->get_height(),0,0.02f };

	DEVICE_HW::CRYRAY_RENDER::HW.pRenderDevice->RSSetViewports(1, &VP);
#endif
}

void					CRender::rmFar()
{
	IRender_Target* T = getTarget();
#ifdef DIRECTX11
	D3D_VIEWPORT VP = { 0,0,(float)T->get_width(),(float)T->get_height(),0.99999f,1.f };

	DEVICE_HW::CRYRAY_RENDER::HW.pRenderContext->RSSetViewports(1, &VP);
#else
	D3D_VIEWPORT VP = { 0,0,T->get_width(),T->get_height(),0.99999f,1.f };

	DEVICE_HW::CRYRAY_RENDER::HW.pRenderDevice->RSSetViewports(1, &VP);
#endif
}

void					CRender::rmNormal			()
{
	IRender_Target* T	=	getTarget	();
#ifdef DIRECTX11
	D3D_VIEWPORT VP		= {0,0,(float)T->get_width(),(float)T->get_height(),0,1.f };

	DEVICE_HW::CRYRAY_RENDER::HW.pRenderContext->RSSetViewports(1, &VP);
#else
	D3D_VIEWPORT VP		= {0,0,T->get_width(),T->get_height(),0,1.f };

	DEVICE_HW::CRYRAY_RENDER::HW.pRenderDevice->RSSetViewports(1, &VP);
#endif
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CRender::CRender()
:m_bFirstFrameAfterReset(false)
{
	init_cacades();
}

CRender::~CRender()
{
	for (FSlideWindowItem it : SWIs)
	{
		xr_free(it.sw);
		it.sw = nullptr;
		it.count = 0;
	}
	SWIs.clear();
}

#include "../../xrEngine/GameFont.h"
void	CRender::Statistics	(CGameFont* _F)
{
	CGameFont&	F	= *_F;
	F.OutNext	(" **** LT:%2d,LV:%2d **** ",stats.l_total,stats.l_visible		);	stats.l_visible = 0;
	F.OutNext	("    S(%2d)   | (%2d)NS   ",stats.l_shadowed,stats.l_unshadowed);
	F.OutNext	("smap use[%2d], merge[%2d], finalclip[%2d]",stats.s_used,stats.s_merged-stats.s_used,stats.s_finalclip);
	stats.s_used = 0; stats.s_merged = 0; stats.s_finalclip = 0;
	F.OutSkip	();
	F.OutNext	(" **** Occ-Q(%03.1f) **** ",100.f*f32(stats.o_culled)/f32(stats.o_queries?stats.o_queries:1));
	F.OutNext	(" total  : %2d",	stats.o_queries	);	stats.o_queries = 0;
	F.OutNext	(" culled : %2d",	stats.o_culled	);	stats.o_culled	= 0;
	F.OutSkip	();
	u32	ict		= stats.ic_total + stats.ic_culled;
	F.OutNext	(" **** iCULL(%03.1f) **** ",100.f*f32(stats.ic_culled)/f32(ict?ict:1));
	F.OutNext	(" visible: %2d",	stats.ic_total	);	stats.ic_total	= 0;
	F.OutNext	(" culled : %2d",	stats.ic_culled	);	stats.ic_culled	= 0;
#ifdef DEBUG
	HOM.stats	();
#endif
}

#ifdef DIRECTX11
void CRender::addShaderOption(const char* name, const char* value)
{
	D3D_SHADER_MACRO macro = {name, value};
	m_ShaderOptions.push_back(macro);
}
#endif