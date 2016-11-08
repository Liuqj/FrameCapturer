#pragma once

#include "BlurModeEnum.generated.h"
UENUM()
enum class EFrameCapturerUserWidgetBlurMode : uint8
{
	StackBlur_CPU,
	GaussianBlur_GPU,
};
