#pragma once

#include "SImage.h"
#include "BlurModeEnum.h"
#include "FrameCaptureTexturePool.h"
#include "SFrameCapturerImageProxy.h"
#include "FrameCapturer.h"

class FFrameCaptureElement : public ICustomSlateElement
{
public:
	FFrameCaptureElement();
	~FFrameCaptureElement();
	void Init(TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> InHoldThis, TSharedPtr<FFrameCapturer> InFrameCapturer);
private:
	virtual void DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer) override;

private:
	TWeakPtr<FFrameCapturer> FrameCapturer;
	SFrameCapturerImageProxy ProxyData;
	// FIXME ��ô����Σ�գ���� CallBack DrawRenderThread ʧ�ܾͻ�й¶
	TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> HoldThis;
};
