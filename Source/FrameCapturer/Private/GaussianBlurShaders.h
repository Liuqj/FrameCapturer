#pragma once

#if ExperimentalGPUBlur

class FHorizontalGaussianBlurPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FHorizontalGaussianBlurPS, Global, FRAMECAPTURER_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FHorizontalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FHorizontalGaussianBlurPS() {}

	void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI, const float SamplerOffset);

	virtual bool Serialize(FArchive& Ar) override;

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter InSamplerOffset;
};

class FVerticalGaussianBlurPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FVerticalGaussianBlurPS, Global, FRAMECAPTURER_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FVerticalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);

	FVerticalGaussianBlurPS() {}

	void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI, const float SamplerOffset);
	virtual bool Serialize(FArchive& Ar) override;
private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter InSamplerOffset;
};
#endif
// TODO : Maybe try FCanvas::DrawItem and Material
void DrawGaussianBlur(FRHICommandList& RHICmdList, const FTexture2DRHIRef& InOutTexture, const FTexture2DRHIRef& TempTarget, int32 Width, int32 Height, float Kernal);

