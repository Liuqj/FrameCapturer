#include "FrameCapturerImage.h"
#include "FrameCapturer.h"
#include "StackBlur.h"
#include "GaussianBlurShaders.h"

void UFrameCapturerImage::OnWidgetRebuilt()
{
	Super::OnWidgetRebuilt();

	if (!ManualCapture)
	{
		CaptureFrame();
	}
}

void UFrameCapturerImage::CaptureFrame()
{
	MyFrameCapturerImage->StartCaptureFrame(GetWorld());
}

TSharedRef<SWidget> UFrameCapturerImage::RebuildWidget()
{
	MyFrameCapturerImage = SNew(SFrameCapturerImage);
	return MyFrameCapturerImage.ToSharedRef();
}

void UFrameCapturerImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FSlateColor> ColorAndOpacityBinding = OPTIONAL_BINDING(FSlateColor, ColorAndOpacity);

	TAttribute<const FSlateBrush*> ImageBinding = OPTIONAL_BINDING_CONVERT(FSlateBrush, Brush, const FSlateBrush*, ConvertImage);
	
	if (MyFrameCapturerImage.IsValid())
	{
		MyFrameCapturerImage->SetBlurKernel(BlurKernel);
		MyFrameCapturerImage->SetBlurMode(BlurMode);
		MyFrameCapturerImage->SetDownSampleNum(DownSampleNum);
		MyFrameCapturerImage->SetCaptureFrameCount(CaptureFrameCount);
		MyFrameCapturerImage->SetStackBlurParallelCore(StackBlurParallelCore);
		MyFrameCapturerImage->SetGaussianBlurIteratorCount(GaussianBlurIteratorCount);
		MyFrameCapturerImage->SetImage(ImageBinding);
		MyFrameCapturerImage->SetOptionalEffectBrush(&OptionalEffectBrush);

		MyFrameCapturerImage->SetColorAndOpacity(ColorAndOpacityBinding);
		MyFrameCapturerImage->SetOnMouseButtonDown(BIND_UOBJECT_DELEGATE(FPointerEventHandler, HandleMouseButtonDown));
	}
}

void UFrameCapturerImage::ReleaseSlateResources(bool bReleaseChildren)
{
	MyFrameCapturerImage.Reset();
}
