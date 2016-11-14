#pragma once

#include "SImage.h"
#include "BlurModeEnum.h"
#include "FrameCaptureTexturePool.h"

struct SFrameCapturerImageProxy
{
	int32 BlurKernel;
	int32 DownSampleNum;
	int32 CaptureFrameCount;
	EFrameCapturerUserWidgetBlurMode BlurMode;
	int32 StackBlurParallelCore;
	int32 GaussianBlurIteratorCount;
	TRefCountPtr<FFrameCapturePooledTexture2DItem> ImageTexture2D;
	TRefCountPtr<FFrameCapturePooledRenderTarget2DItem> ImageRenderTarget2D;

	void UpdateImage(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const;
	void UpdateImageStackBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const;
	void UpdateImageGaussianBlur(FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) const;
};
