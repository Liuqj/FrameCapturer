#include "FrameCapturerPrivatePCH.h"
#include "FrameCapturerUserWidget.h"
#include "FrameRecorder.h"
#include "StackBlur.h"

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
			FrameCapturer = MakeUnique<FFrameCapturer>(ViewportClient->GetGameViewport(), FIntPoint(ViewportSize.X, ViewportSize.Y));
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
	FillImageBrush(CapturedFrame);

	FIntPoint BufferSize = CapturedFrame.BufferSize;
	FIntRect TexRect(0, 0, BufferSize.X >> DownSampleNum, BufferSize.Y >> DownSampleNum);
	uint32 Size = TexRect.Width() * TexRect.Height();
	TArray<FColor> OutData;
	OutData.AddUninitialized(Size);
	for (int32 Y = 0; Y < TexRect.Height(); Y++)
	{
		for (int32 X = 0; X < TexRect.Width(); X++)
		{
			OutData[Y * TexRect.Width() + X] = CapturedFrame.ColorBuffer[(Y << DownSampleNum) * BufferSize.X + (X << DownSampleNum)];
		}
	}

	static int32 MaxCore = 4;
	StackBlur(OutData, TexRect.Width(), TexRect.Height(), BlurKernel, MaxCore);

	auto RenderCommand = [=](FRHICommandListImmediate& RHICmdList, const TArray<FColor>& Data) {
		FUpdateTextureRegion2D UpdateRegion(0, 0, 0, 0, TexRect.Width(), TexRect.Height());
		{
			auto Resource = (FTexture2DResource*)(ShareImageTexture2D->Resource);
			RHICmdList.UpdateTexture2D(
				Resource->GetTexture2DRHI(),
				0,
				UpdateRegion,
				sizeof(FColor) * TexRect.Width(),
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

void UFrameCapturerUserWidget::FillImageBrush(const FCapturedFrame &CapturedFrame)
{
	FIntPoint BufferSize = CapturedFrame.BufferSize;
	FIntRect TexRect(0, 0, BufferSize.X >> DownSampleNum, BufferSize.Y >> DownSampleNum);
	if (!ShareImageTexture2D.IsValid() ||
		ShareImageTexture2D->GetSizeX() != TexRect.Width() || ShareImageTexture2D->GetSizeY() != TexRect.Height())
	{
		ShareImageTexture2D = UTexture2D::CreateTransient(TexRect.Width(), TexRect.Height());
		ShareImageTexture2D->UpdateResource();
	}
	Image->SetBrushFromTexture(ShareImageTexture2D.Get());
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
