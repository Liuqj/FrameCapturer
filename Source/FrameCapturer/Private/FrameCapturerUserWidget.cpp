#include "FrameCapturerPrivatePCH.h"
#include "FrameCapturerUserWidget.h"
#include "FrameRecorder.h"
#include "StackBlur.h"

class FHorizontalGaussianBlurPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FHorizontalGaussianBlurPS, Global, FRAMECAPTURER_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FHorizontalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		InSamplerOffset.Bind(Initializer.ParameterMap, TEXT("InSamplerOffset"));
	}
	FHorizontalGaussianBlurPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, const float SamplerOffset)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, Texture);
		SetShaderValue(RHICmdList, GetPixelShader(), InSamplerOffset, SamplerOffset);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << InSamplerOffset;
		return bShaderHasOutdatedParameters;
	}

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

	FVerticalGaussianBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
		InSamplerOffset.Bind(Initializer.ParameterMap, TEXT("InSamplerOffset"));
	}
	FVerticalGaussianBlurPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture, const float SamplerOffset)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, Texture);
		SetShaderValue(RHICmdList, GetPixelShader(), InSamplerOffset, SamplerOffset);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		Ar << InSamplerOffset;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
	FShaderParameter InSamplerOffset;
};

IMPLEMENT_SHADER_TYPE(, FHorizontalGaussianBlurPS, TEXT("HorizontalGaussianBlurPixelShader"), TEXT("Main"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FVerticalGaussianBlurPS, TEXT("VerticalGaussianBlurPixelShader"), TEXT("Main"), SF_Pixel);

void UFrameCapturerUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SingletonFlag)
	{
		UE_LOG(LogFrameCapturer, Error, TEXT("FrameCapturerUserWidget twice open"));
	}
	SingletonFlag = true;

	HiddenWidget();

	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UFrameCapturerUserWidget::CheckCaptureTimer);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 1 / 30.f, true);

	if (!ManualCapture)
	{
		CaptureFrame();
	}
}

void UFrameCapturerUserWidget::NativeDestruct()
{
	Super::NativeDestruct();
	Image->SetBrush(FSlateBrush());
	DestoryFrameCapturer();
	SingletonFlag = false;
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
}

void UFrameCapturerUserWidget::CheckCaptureTimer()
{
	if (FrameCapturer.IsValid())
	{
		const TArray<FCapturedFrame>& Frames = FrameCapturer->GetCapturedFrames();
		if (Frames.Num() > 0)
		{
			UpdateImage(Frames.Last());
			DestoryFrameCapturer();
		}
		else
		{
			FrameCapturer->CaptureThisFrame(nullptr);
		}
	}
}

void UFrameCapturerUserWidget::CaptureFrame()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (UGameViewportClient* ViewportClient = World->GetGameViewport())
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);
			FrameCapturer = MakeUnique<FFrameCapturer>(ViewportClient->GetGameViewport(), FIntPoint((int32)ViewportSize.X >> DownSampleNum, (int32)ViewportSize.Y >> DownSampleNum), BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU);
			FrameCapturer->StartCapturingFrames();
		}
	}
}

void UFrameCapturerUserWidget::HiddenWidget()
{
	UPanelWidget* Parent = GetParent();
	if (Parent)
	{
		Parent->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UFrameCapturerUserWidget::ShowWidget()
{
	UPanelWidget* Parent = GetParent();
	if (Parent)
	{
		Parent->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrameCapturerUserWidget::UpdateImage(const FCapturedFrame& CapturedFrame)
{
	FillImageBrush(CapturedFrame.BufferSize);

	if (BlurMode == EFrameCapturerUserWidgetBlurMode::StackBlur_CPU)
	{
		UpdateImageStackBlur(CapturedFrame);
	}
	else if(BlurMode == EFrameCapturerUserWidgetBlurMode::GaussianBlur_GPU)
	{
		UpdateImageGaussianBlur(CapturedFrame);
	}
}

void UFrameCapturerUserWidget::UpdateImageStackBlur(const FCapturedFrame& CapturedFrame)
{
	FIntPoint BufferSize = CapturedFrame.BufferSize;
	TArray<FColor> OutData(MoveTemp(CapturedFrame.ColorBuffer));

	StackBlur(OutData, BufferSize.X, BufferSize.Y, BlurKernel, StackBlurParallelCore);

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& Data) {
		FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, BufferSize.X, BufferSize.Y);
		{
			auto Resource = (FTexture2DResource*)(ShareImageTexture2D->Resource);
			RHICmdList.UpdateTexture2D(
				Resource->GetTexture2DRHI(),
				0,
				UpdateRegion,
				sizeof(FColor) * BufferSize.X,
				(uint8*)OutData.GetData()
				);
			ShowWidget();
		}
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&, const TArray<FColor>&)>, InRenderCommand, RenderCommand,
		const TArray<FColor>, Data, MoveTemp(OutData),
		{
			InRenderCommand(RHICmdList, Data);
		});
}

void UFrameCapturerUserWidget::UpdateImageGaussianBlur(const FCapturedFrame& CapturedFrame)
{
	auto Resource = (FTexture2DResource*)(ShareImageTexture2D->Resource);
	auto Texture2DRHIRef = CapturedFrame.ReadbackTexture;
	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList) {
		Resource->TextureRHI = Texture2DRHIRef;
		RHIUpdateTextureReference(ShareImageTexture2D->TextureReference.TextureReferenceRHI, Resource->TextureRHI);
		ShowWidget();
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ResolveCaptureFrameTexture,
		TFunction<void(FRHICommandListImmediate&)>, InRenderCommand, RenderCommand,
		{
			InRenderCommand(RHICmdList);
		});
}

void UFrameCapturerUserWidget::FillImageBrush(const FIntPoint& BufferSize)
{
	if (!ShareImageTexture2D.IsValid() || 
		ShareImageTexture2D->GetSizeX() != BufferSize.X || ShareImageTexture2D->GetSizeY() != BufferSize.Y)
	{
		ShareImageTexture2D = UTexture2D::CreateTransient(BufferSize.X, BufferSize.Y);
		ShareImageTexture2D->bIgnoreStreamingMipBias = true;
		ShareImageTexture2D->SRGB = true;
		ShareImageTexture2D->UpdateResource();
	}

	FSlateBrush Brush;
	Brush.SetResourceObject(ShareImageTexture2D.Get());
	Image->SetBrush(Brush);
}

void UFrameCapturerUserWidget::DestoryFrameCapturer()
{
	if (FrameCapturer.IsValid())
	{
		FrameCapturer.Reset();
	}
}

TWeakObjectPtr<UTexture2D> UFrameCapturerUserWidget::ShareImageTexture2D;

bool UFrameCapturerUserWidget::SingletonFlag = false;
