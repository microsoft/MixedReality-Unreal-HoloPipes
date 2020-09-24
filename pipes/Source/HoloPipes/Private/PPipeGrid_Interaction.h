#pragma once

#include "HoloPipes.h"
#include "PPipeGrid_Interaction.generated.h"

enum class InteractionMode
{
    // No interaction is enabled
    None,

    // A pipe segment has focus, and might start either a drag or a rotate
    Focus,

    // A pipe segment has been picked up for movement
    Translate,

    // A pipe segment is being rotated with a rotate handle
    Rotate,

    // The toolbox itself has focus
    ToolboxFocus,

    // The toolbox has been picked up for movement
    ToolboxTranslate
};

UENUM(BlueprintType)
enum class EInteractionAxis : uint8
{
    None,
    Yaw,
    Pitch,
    Roll
};

UENUM(BlueprintType)
enum class EInteractionPhase : uint8
{
    None,
    Visible,
    Pending,
    Blocked,
    Ready
};
