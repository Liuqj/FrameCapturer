#pragma once

#include "FrameCapturerSettings.generated.h"

UCLASS(config = Game, defaultconfig)
class UFrameCapturerSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category = "FrameCapturer", meta = (DisplayName = "ShowSingleSlateWhenStack(Experimental)"))
	bool ShowSingleSlateWhenStack = true;
};
