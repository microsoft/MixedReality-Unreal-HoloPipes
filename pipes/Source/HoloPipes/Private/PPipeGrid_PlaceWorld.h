#pragma once

#include "PMRPawn.h"

enum class PlaceWorldMode
{
    None,
    Pending,
    OneHand,
    TwoHand
};

// In OneHand placement mode, we support translation and keeping the orientation the same with respect to the user
struct PlaceWorldOneHandState
{
    EFingerHand AttachedToHand;
    FTransform GameToAttach;
};

// In TwoHand placement mode, we support full translation, rotation and scale
struct PlaceWorldTwoHandState
{
    FVector StartScale;
    FVector StartGridWorldOffset;
    FVector StartHeadToHand;
    FVector PreviousLeft;
    FVector PreviousRight;
    float StartDistance;
};

struct PlaceWorldState
{
    PlaceWorldMode Mode = PlaceWorldMode::None;

    bool LeftActive;
    bool RightActive;

    FVector LeftPosition;
    FVector RightPosition;

    PlaceWorldOneHandState OneHand;

    PlaceWorldTwoHandState TwoHand;
};
