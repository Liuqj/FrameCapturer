#include "FrameCaptureElement.h"
#include "SFrameCapturerImage.h"
#include "GaussianBlurShaders.h"
#include "FrameCapturer.h"
#include "StackBlur.h"
#include "FrameCapturerSettings.h"

FFrameCaptureElement::FFrameCaptureElement()
{
}

FFrameCaptureElement::~FFrameCaptureElement()
{

}

void FFrameCaptureElement::Init(TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> InHoldThis, TSharedPtr<FFrameCapturer> InFrameCapturer)
{
	HoldThis = InHoldThis;
	FrameCapturer = InFrameCapturer;
}

void FFrameCaptureElement::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	FTexture2DRHIRef& RT = *(FTexture2DRHIRef*)InWindowBackBuffer;

	auto FrameCapturerPtr = FrameCapturer.Pin();
	if (FrameCapturerPtr.IsValid())
	{
		FrameCapturerPtr->TryCapturingThisFrames_RenderThread(RHICmdList, RT);
	}

	SetRenderTarget(RHICmdList, RT, FTextureRHIRef());

	HoldThis.Reset();
}
