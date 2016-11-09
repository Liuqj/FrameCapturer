#pragma once

#include "BlurModeEnum.generated.h"
UENUM()
enum class EFrameCapturerUserWidgetBlurMode : uint8
{
	StackBlur_CPU,
#if ExperimentalGPUBlur
	GaussianBlur_GPU,
#endif
};
