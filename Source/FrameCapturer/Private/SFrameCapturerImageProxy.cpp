#include "FrameCapturerPrivatePCH.h"
#include "SFrameCapturerImage.h"
#include "GaussianBlurShaders.h"
#include "FrameCapturer.h"
#include "StackBlur.h"
#include "FrameCapturerSettings.h"

void SFrameCapturerImageProxy::UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const
{
	if (BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		UpdateImageStackBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
	else
	{
		UpdateImageGaussianBlur(RHICmdList, ColorBuffer, Texture, Width, Height);
	}
}

void SFrameCapturerImageProxy::UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const
{
	StackBlur(ColorBuffer, Width, Height, BlurKernel, StackBlurParallelCore);

	FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, Width, Height);
	{
		auto Resource = (FTexture2DResource*)(ImageTexture2D->GetItem()->Resource);

		RHICmdList.UpdateTexture2D(
			Resource->GetTexture2DRHI(),
			0,
			UpdateRegion,
			sizeof(FColor) * Width,
			(uint8*)ColorBuffer.GetData()
			);
	}
}

void SFrameCapturerImageProxy::UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const
{
	static const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
		FIntPoint(Width, Height),
		PF_B8G8R8A8,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_RenderTargetable,
		false);

	TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
	RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("FrameRecorderGaussianTemp"));
	check(ResampleTexturePooledRenderTarget);

	for (int32 i = 0; i < GaussianBlurIteratorCount; i++)
	{
		auto TargetableTexture = ResampleTexturePooledRenderTarget->GetRenderTargetItem().TargetableTexture;
		DrawGaussianBlur(RHICmdList, Texture, (FTexture2DRHIRef&)TargetableTexture, Width, Height, (BlurKernel >> DownSampleNum) / 3.0);
	}
	SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());
}
