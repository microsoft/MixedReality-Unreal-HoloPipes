#pragma once

#include "PPipe.h"
#include "PMRPawn.h"
#include "PPipeGrid_Interaction.h"
#include "PPipeGrid_Cursor.h"
#include "Debounce.h"

struct GridHandDrag
{
    FQuat AttachStartRotation = FQuat();
};

struct GridHandRotate
{
    FVector StartLocation = FVector::ZeroVector;
    FVector HandleLocation = FVector::ZeroVector;
    FRotator AxisRotations = FRotator::ZeroRotator;
    EInteractionAxis CurrentAxis = EInteractionAxis::None;
    bool SuppressChangeAnimation = false;
};

struct GridHandTarget
{
    EInteractionPhase Phase = EInteractionPhase::None;
    FVector Location = FVector::ZeroVector;
    FPipeGridCoordinate Coordinate = { 0,0,0 };

    FVector InAttachSpace = FVector::ZeroVector;
    bool Attached = false;
};

struct GridHandPipe
{
    APPipe* Actor = nullptr;
    FPipeGridCoordinate StartCoordinate = { 0,0,0 };
    PipeDirections StartDirections = PipeDirections::None;
    FQuat StartRotation = FQuat();
};

struct GridHandProxy
{
    APPipe* Actor = nullptr;

    void Clear()
    {
        if (Actor != nullptr)
        {
            Actor->Destroy();
            Actor = nullptr;
        }
    }
};

struct GridHandToolbox
{
    FPipeGridCoordinate StartCoordinate = { 0,0,0 };
    FPipeGridCoordinate LastSafeCoordinate = { 0,0,0 };
};

struct GridHandDebounce
{
    DebounceBool ShowRotateHandles;
    DebounceBool ShouldKeepFocus;
    DebounceAxis CurrentRotateAxis;
};

struct GridHandState
{
    InteractionMode InteractionMode = InteractionMode::None;

    GridHandDrag Drag;
    GridHandRotate Rotate;
    GridHandTarget Target;
    GridHandPipe Pipe;
    GridHandToolbox Toolbox;
    GridHandProxy Proxy;
    GridHandDebounce Debounce;
};

struct GridHandFocus
{
    std::vector<APPipe*> HandOnly;
    std::vector<APPipe*> HandAndGaze;
};

struct GridHand
{
    GridHandState State;
    FCursorStateHand Cursor;

#if !UE_BUILD_SHIPPING
    GridHandFocus Focus;
#endif
};

struct GridHands
{
    bool InteractionEnabled = false;

    GridHand Left;
    GridHand Right;
};
