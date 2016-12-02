#include "SFrameCapturerImage.h"
#include "GaussianBlurShaders.h"
#include "FrameCapturer.h"
#include "StackBlur.h"
#include "FrameCapturerSettings.h"

static FName DefaultTextureParameterName("Texture");

SFrameCapturerImage::SFrameCapturerImage()
{
	bCanTick = true;
}

SFrameCapturerImage::~SFrameCapturerImage()
{
	// Test Leak this pointer
}

void SFrameCapturerImage::Construct(const FArguments& InArgs)
{
	BlurKernel = InArgs._BlurKernel;
	DownSampleNum = InArgs._DownSampleNum;
	BlurMode = InArgs._BlurMode;
	StackBlurParallelCore = InArgs._StackBlurParallelCore;
	GaussianBlurIteratorCount = InArgs._GaussianBlurIteratorCount;
}

void SFrameCapturerImage::StartCaptureFrame(UWorld* InWorld)
{
	World = InWorld;
	WantInitialize = true;
}

int32 SFrameCapturerImage::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	CurrentFrameCapturerSlateCount++;

	if (GetDefault<UFrameCapturerSettings>()->ShowSingleSlateWhenStack == false || CurrentFrameCapturerSlateCount == CacheFrameCapturerSlateCount)
	{
		if (WantInitialize)
		{
			InitBrushAndFrameCapturer(AllottedGeometry);
			WantInitialize = false;
		}

		OnPaintFrameCapter(OutDrawElements, LayerId);

		OnPaintImage(bParentEnabled, InWidgetStyle, OutDrawElements, LayerId, AllottedGeometry, MyClippingRect);
	}
	else
	{
		WantInitialize = true;
		ClearImageBrush();
	}

	return LayerId;
}

void SFrameCapturerImage::OnPaintFrameCapter(FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (FrameCapturer.IsValid())
	{
		// FIXME: 如此处理 Drawer 不合适?
		TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> Drawer = MakeShareable<FFrameCaptureElement>(new FFrameCaptureElement());

		StaticCastSharedPtr<FFrameCaptureElement>(Drawer)->Init(Drawer, FrameCapturer);

		FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, Drawer);
	}
}

void SFrameCapturerImage::OnPaintImage(bool bParentEnabled, const FWidgetStyle &InWidgetStyle, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry &AllottedGeometry, const FSlateRect& MyClippingRect) const
{
	const FSlateBrush* ImageBrush = Image.Get();

	if ((ImageBrush != nullptr) && (ImageBrush->DrawAs != ESlateBrushDrawType::NoDrawType))
	{
		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		if (BlurMode.Get() != EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
		{
			DrawEffects |= ESlateDrawEffect::NoGamma;
		}

		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * ImageBrush->GetTint(InWidgetStyle));

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ImageBrush, MyClippingRect, DrawEffects, FinalColorAndOpacity);
	}
}

void SFrameCapturerImage::InitBrushAndFrameCapturer(const FGeometry &AllottedGeometry) const
{
	// FIXME: if world perpetuation
	if (World.IsValid() && World->IsGameWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D ViewportSize;

			ViewportClient->GetViewportSize(ViewportSize);

			FIntPoint TargetSize((int32)ViewportSize.X >> DownSampleNum.Get(), (int32)ViewportSize.Y >> DownSampleNum.Get());

			FillImageBrush(TargetSize.X, TargetSize.Y);

			bool ReadBack = BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU;

			FVector2D Size = AllottedGeometry.GetDrawSize();
			FIntRect CaptureRect
			{
				(int32)AllottedGeometry.AbsolutePosition.X,
				(int32)AllottedGeometry.AbsolutePosition.Y,
				(int32)(AllottedGeometry.AbsolutePosition.X + Size.X),
				(int32)(AllottedGeometry.AbsolutePosition.Y + Size.Y)
			};

			FrameCapturer = MakeShareable<FFrameCapturer>(new FFrameCapturer(
				ViewportClient->GetGameViewport(),
				CaptureRect,
				TargetSize,
				ReadBack,
				ReadBack ? nullptr : ImageRenderTarget2D->GetItem()->GameThread_GetRenderTargetResource()->GetRenderTargetTexture(),
				PF_B8G8R8A8
				));

			SFrameCapturerImageProxy Proxy
			{
				BlurKernel.Get(),
				DownSampleNum.Get(),
				CaptureFrameCount.Get(),
				BlurMode.Get(),
				StackBlurParallelCore.Get(),
				GaussianBlurIteratorCount.Get(),
				ImageTexture2D,
				ImageRenderTarget2D
			};

			FrameCapturer->PrepareCapturingFrames_MainThread(CaptureFrameCount.Get(),
				[=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& ColorBuffer, const FTexture2DRHIRef& Texture, int32 Width, int32 Height) {
				Proxy.UpdateImage(RHICmdList, ColorBuffer, Texture, Width, Height);
			}
			);
		}
	}
}

void SFrameCapturerImage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SImage::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (FrameCapturer.IsValid())
	{
		if (FrameCapturer->GetRemainFrameCount() == 0)
		{
			FrameCapturer.Reset();
		}
	}
}

void SFrameCapturerImage::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MaterialDynamic);
}

UWorld* SFrameCapturerImage::GetWorld()
{
	return World.Get();
}

void SFrameCapturerImage::FillImageBrush(int32 Width, int32 Height) const
{
	FSlateBrush* NoConstImage = (FSlateBrush*)Image.Get();
	NoConstImage->Tiling = ESlateBrushTileType::NoTile;
	if (BlurMode.Get() == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		ImageTexture2D = FFrameCaptureTexturePool::Get().GetTexture2D(Width, Height);

		UObject* ResourceObject = OptionalEffectBrush->GetResourceObject();
		if (ResourceObject)
		{
			if (UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject))
			{
				MaterialDynamic = UMaterialInstanceDynamic::Create(Material, GetTransientPackage());

				MaterialDynamic->SetTextureParameterValue(DefaultTextureParameterName, ImageTexture2D->GetItem());

				NoConstImage->SetResourceObject(MaterialDynamic);
			}
		}
		else
		{
			NoConstImage->SetResourceObject(ImageTexture2D->GetItem());
		}
	}
	else
	{
		ImageRenderTarget2D = FFrameCaptureTexturePool::Get().GetRenderTarget2D(Width, Height);

		UObject* ResourceObject = OptionalEffectBrush->GetResourceObject();
		if (ResourceObject)
		{
			if (UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject))
			{
				MaterialDynamic = UMaterialInstanceDynamic::Create(Material, GetTransientPackage());

				MaterialDynamic->SetTextureParameterValue(DefaultTextureParameterName, ImageRenderTarget2D->GetItem());

				NoConstImage->SetResourceObject(MaterialDynamic);
			}
		}
		else
		{
			NoConstImage->SetResourceObject(ImageRenderTarget2D->GetItem());
		}
	}
	FlushRenderingCommands();
}

void SFrameCapturerImage::ClearImageBrush() const
{
	FSlateBrush* NoConstImage = (FSlateBrush*)Image.Get();
	NoConstImage->SetResourceObject(nullptr);
}

void SFrameCapturerImage::SetBlurKernel(int32 InBlurKernel)
{
	BlurKernel = InBlurKernel;
}

void SFrameCapturerImage::SetDownSampleNum(int32 InDownSampleNum)
{
	DownSampleNum = InDownSampleNum;
}

void SFrameCapturerImage::SetCaptureFrameCount(int32 InCaptureFrameCount)
{
	CaptureFrameCount = InCaptureFrameCount;
}

void SFrameCapturerImage::SetBlurMode(EFrameCapturerUserWidgetBlurMode InMode)
{
	BlurMode = InMode;
}

void SFrameCapturerImage::SetStackBlurParallelCore(int32 InStackBlurParallelCore)
{
	StackBlurParallelCore = InStackBlurParallelCore;
}

void SFrameCapturerImage::SetGaussianBlurIteratorCount(int32 InGaussianBlurIteratorCount)
{
	GaussianBlurIteratorCount = InGaussianBlurIteratorCount;
}

void SFrameCapturerImage::SetOptionalEffectBrush(FSlateBrush* InBrush)
{
	OptionalEffectBrush = InBrush;
}

int32 SFrameCapturerImage::CacheFrameCapturerSlateCount = 0;

int32 SFrameCapturerImage::CurrentFrameCapturerSlateCount = 0;
