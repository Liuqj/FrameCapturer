#include "FrameCapturerPrivatePCH.h"
#include "GaussianBlurShaders.h"

#if ExperimentalGPUBlur

IMPLEMENT_SHADER_TYPE(, FHorizontalGaussianBlurPS, TEXT("HorizontalGaussianBlurPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FVerticalGaussianBlurPS, TEXT("VerticalGaussianBlurPixelShader"), TEXT("Main"), SF_Pixel);

FHorizontalGaussianBlurPS::FHorizontalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	InSamplerOffset.Bind(Initializer.ParameterMap, TEXT("InSamplerOffset"));
}

void FHorizontalGaussianBlurPS::SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI, const float SamplerOffset)
{
	SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
	SetShaderValue(RHICmdList, GetPixelShader(), InSamplerOffset, SamplerOffset);
}

bool FHorizontalGaussianBlurPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << InSamplerOffset;
	return bShaderHasOutdatedParameters;
}

FVerticalGaussianBlurPS::FVerticalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:FGlobalShader(Initializer)
{
	InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
	InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	InSamplerOffset.Bind(Initializer.ParameterMap, TEXT("InSamplerOffset"));
}

void FVerticalGaussianBlurPS::SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI, const float SamplerOffset)
{
	SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
	SetShaderValue(RHICmdList, GetPixelShader(), InSamplerOffset, SamplerOffset);
}

bool FVerticalGaussianBlurPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
	Ar << InTexture;
	Ar << InTextureSampler;
	Ar << InSamplerOffset;
	return bShaderHasOutdatedParameters;
}
#endif

void DrawGaussianBlur(FRHICommandList& RHICmdList, const FTexture2DRHIRef& InOutTexture, const FTexture2DRHIRef& TempTarget, int32 Width, int32 Height, float Kernal)
{
#if ExperimentalGPUBlur
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);
	
	{
		SetRenderTarget(RHICmdList, TempTarget, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, Width, Height, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

		TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FHorizontalGaussianBlurPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), InOutTexture, Kernal / Width);

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			Width,
			Height,
			0, 0,
			1, 1,
			FIntPoint(Width, Height),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}

	{
		SetRenderTarget(RHICmdList, InOutTexture, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, Width, Height, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;

		TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FVerticalGaussianBlurPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), TempTarget, Kernal / Height);

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			Width,
			Height,
			0, 0,
			1, 1,
			FIntPoint(Width, Height),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}
#endif
}
