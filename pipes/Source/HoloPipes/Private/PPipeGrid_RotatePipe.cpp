
#include "PPipeGrid_Internal.h"

bool APPipeGrid::TryHandleRotateStart(GridHandState& handState)
{
    bool handled = false;

    if (m_rotateEnabled && 
        handState.InteractionMode == InteractionMode::Focus && 
        handState.Pipe.Actor != nullptr &&
        !handState.Pipe.Actor->GetInToolbox())
    {
        if (handState.Rotate.CurrentAxis == EInteractionAxis::None)
        {
            ComputeCurrentRotationHandle(handState.Pipe.StartCoordinate, handState, &handState.Rotate.CurrentAxis, &handState.Rotate.HandleLocation);
        }

        if (handState.Rotate.CurrentAxis != EInteractionAxis::None)
        {
            GridPipe* pipe = GetActionablePipe(handState.Pipe.StartCoordinate);
            if (pipe && pipe->pipe == handState.Pipe.Actor)
            {
                handState.InteractionMode = InteractionMode::Rotate;

                handState.Rotate.StartLocation = handState.Target.Location;
                handState.Pipe.Actor->FireInteractionEvent(EInteractionEvent::RotateStarted);
                handled = true;
            }
        }
    }

    return handled;
}
// Expects normalized angles
float ComputeAngleFromPerpendicular(const FVector& a, const FVector& b)
{
    return fabs(90.0f - UKismetMathLibrary::RadiansToDegrees(acosf(FVector::DotProduct(a, b))));
}

void APPipeGrid::ComputeCurrentRotationHandle(const FPipeGridCoordinate& pipeGridCoord, const GridHandState& handState, EInteractionAxis* pHandleAxis, FVector* pHandleLocation)
{
    ComputeCurrentRotationHandle(pipeGridCoord, handState, EInteractionAxis::None, pHandleAxis, pHandleLocation);
}

constexpr float HandleFocusCenterRatio = 1.28f;
constexpr float HandleFocusRadiusRatio = 1.1f;

struct HandleFocusOption
{
    EInteractionAxis Axis;
    FVector CenterOffset;
};

HandleFocusOption HandleFocusOptions[] =
{
    {EInteractionAxis::Yaw,     {1.0f, 1.0f, 0}},
    {EInteractionAxis::Pitch,   {1.0f, 0, 1.0f}},
    {EInteractionAxis::Roll,    {0, 1.0f, 1.0f}},
};

void APPipeGrid::ComputeCurrentRotationHandle(const FPipeGridCoordinate& pipeGridCoord, const GridHandState& handState, EInteractionAxis preferredAxis, EInteractionAxis* pHandleAxis, FVector* pHandleLocation)
{
    EInteractionAxis selectedAxis = EInteractionAxis::None;
    FVector selectedLocation = FVector::ZeroVector;

    if (m_rotateEnabled)
    {
        const FVector gridLocation = GridCoordinateToGridLocation(pipeGridCoord);
        const FVector relativeTargetLocation = handState.Target.Location - gridLocation;
        float radius = CellSize / 2.0f;

        float handleX = relativeTargetLocation.X >= 0 ? radius : -radius;
        float handleY = relativeTargetLocation.Y >= 0 ? radius : -radius;
        float handleZ = relativeTargetLocation.Z >= 0 ? radius : -radius;
        const FVector handleOffsets = { handleX, handleY, handleZ };

        float handleFocusRadius = radius * HandleFocusRadiusRatio;

        float selectedAxisDistSq = 0;

        for (const auto& option : HandleFocusOptions)
        {
            if (preferredAxis == EInteractionAxis::None || preferredAxis == option.Axis)
            {
                const FVector handleRelativeBase = handleOffsets * option.CenterOffset;
                const FVector handleCenter = gridLocation + (HandleFocusCenterRatio * handleRelativeBase);

                bool considerHandle = pipeGridCoord != handState.Target.Coordinate;

                if (!considerHandle)
                {
                    // Hand is within pipe's cell. Only consider a rotate handle if there's strong evidence a user
                    // intends to focus the handle

                    if (FVector::DistSquared(handState.Target.Location, handleCenter) < FVector::DistSquared(handState.Target.Location, gridLocation))
                    {
                        // The hand is closer to the handle than the pipe's center. Is gaze as well?
                        float handleDistanceToGazeSq = 0;
                        float pipeDistanceToGazeSq = 0;
                        considerHandle =
                            FindMinDistanceFromGazeSq(handleCenter, &handleDistanceToGazeSq) &&
                            FindMinDistanceFromGazeSq(gridLocation, &pipeDistanceToGazeSq) &&
                            handleDistanceToGazeSq < pipeDistanceToGazeSq;
                    }

                }

                if (considerHandle)
                {
                    float distToHandleSq = FVector::DistSquared(handleCenter, handState.Target.Location);
                    if (selectedAxis == EInteractionAxis::None || (selectedAxis != EInteractionAxis::None && distToHandleSq < selectedAxisDistSq))
                    {
                        selectedAxis = option.Axis;
                        selectedLocation = handleRelativeBase;
                        selectedAxisDistSq = distToHandleSq;
                    }
                }
            }
        }
    }

    if (pHandleAxis)
    {
        (*pHandleAxis) = selectedAxis;
    }

    if (pHandleLocation)
    {
        (*pHandleLocation) = selectedLocation;
    }

}

void APPipeGrid::HandleRotateGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState)
{
    switch (update.Progress)
    {
        case EGestureProgress::None:
        case EGestureProgress::Tracked:
        case EGestureProgress::Lost:
            HandleRotateLost(handState);
            break;

        case EGestureProgress::Started:
        case EGestureProgress::Updated:
            HandleRotateUpdate(handState, handState.Target.Location);
            break;
    }
}

// A compiler bug currently chokes on SSE code inside FVector::Normalize when it's optimized. Feedback filed with
// VS. For now, turn off optimization of ComputeCandidateRotation when compiling for non-HoloLens (HoloLens uses
// NEON instructions on ARM, which does not appear to suffer from the same bug)
#if !PLATFORM_HOLOLENS
#pragma optimize("", off)
#endif // !PLATFORM_HOLOLENS

float APPipeGrid::ComputeCandidateRotation(const FVector& base, const FVector& current, const FVector& mask)
{
    float angle = 0;

    FVector baseProjected = base * mask;
    FVector currentProjected = current * mask;

    // If we can't normalize, than our vectors are too short to give us
    // meaningful numbers answers
    if (baseProjected.Normalize() && currentProjected.Normalize())
    {
        float dotProduct = FVector::DotProduct(baseProjected, currentProjected);
        float angleR = UKismetMathLibrary::Acos(dotProduct);
        angle = UKismetMathLibrary::RadiansToDegrees(angleR);

        // Dot product gives us the angle between our two normalized vectors, but not the direction (sign) of that angle.
        // Within a tenth of a degree of parallel to base, we don't care about sign, since it isn't noticeable if we have it wrong.
        // Beyond that, we do the cross product to find the perpendicular axis, and take its sign. That works because we projected the 
        // vectors onto a plane which is perpendicular to one of the X, Y or Z axes.
        if (angle > 0.1f && angle < 179.9f)
        {
            FVector signVector = FVector::CrossProduct(baseProjected, currentProjected);
            signVector.Normalize();
            if (signVector.X < -0.9f || signVector.Y < -0.9f || signVector.Z < -0.9f)
            {
                angle = -angle;
            }
        }
    }

    return angle;
}

#if !PLATFORM_HOLOLENS
#pragma optimize("", on)
#endif // !PLATFORM_HOLOLENS

void APPipeGrid::ComputeCandidateRotations(GridHandState& handState, const FVector& gridLocation, float* yaw, float* pitch, float* roll)
{
    if (yaw)
    {
        (*yaw) = 0;
    }

    if (pitch)
    {
        (*pitch) = 0;
    }

    if (roll)
    {
        (*roll) = 0;
    }

    FVector root = handState.Pipe.Actor ? WorldLocationToGridLocation(handState.Pipe.Actor->GetActorLocation()) : FVector::ZeroVector;

    // We don't normalize base because we don't want to lose precision. We will normalize the direction specific bases instead
    FVector base = (handState.Rotate.StartLocation - root);
    FVector current = (gridLocation - root);

    if (yaw)
    {
        // Yaw is the angle in the xy plane
        (*yaw) = ComputeCandidateRotation(base, current, { 1, 1, 0 });
    }

    if (pitch)
    {
        // Pitch is the angle in the xz plane
        (*pitch) = -ComputeCandidateRotation(base, current, { 1, 0, 1 });
    }

    if (roll)
    {
        // Roll is the angle in the yz plane
        (*roll) = -ComputeCandidateRotation(base, current, { 0, 1, 1 });
    }
}

void APPipeGrid::HandleRotateUpdate(GridHandState& handState, const FVector& gridLocation)
{
    EInteractionAxis currentAxis = handState.Rotate.CurrentAxis;
    handState.Rotate.AxisRotations = FRotator::ZeroRotator;

    ComputeCandidateRotations(handState,
                              gridLocation,
                              currentAxis == EInteractionAxis::Yaw ? &handState.Rotate.AxisRotations.Yaw : nullptr,
                              currentAxis == EInteractionAxis::Pitch ? &handState.Rotate.AxisRotations.Pitch : nullptr,
                              currentAxis == EInteractionAxis::Roll ? &handState.Rotate.AxisRotations.Roll : nullptr);

    PipeDirections newDirections = RotateConnections(handState.Pipe.StartDirections, handState.Rotate.AxisRotations);
    handState.Pipe.Actor->SetActorRelativeRotation((handState.Rotate.AxisRotations.Quaternion() * handState.Pipe.StartRotation).Rotator());

    if (newDirections != handState.Pipe.Actor->GetPipeDirections())
    {
        handState.Pipe.Actor->SetPipeDirections(newDirections);
        UpdatePipeConnections(handState.Pipe.StartCoordinate);
        ResolveGridState();
    }
}

void APPipeGrid::HandleRotateLost(GridHandState& handState)
{
    if (handState.InteractionMode == InteractionMode::Rotate)
    {
        handState.InteractionMode = InteractionMode::Focus;
        handState.Rotate.AxisRotations = FRotator::ZeroRotator;

        // Stay in rotate handle mode (don't lose focus at end of rotate)
        handState.Debounce.ShouldKeepFocus.Reset(LoseRotateFocusTimeout);
        handState.Debounce.ShouldKeepFocus.SetCurrent(true);

        handState.Debounce.ShowRotateHandles.SetCurrent(true);

        FRotator newRot;
        GetPipeRotation(handState.Pipe.Actor->GetPipeType(), handState.Pipe.Actor->GetPipeDirections(), newRot);
        handState.Pipe.Actor->SetActorRelativeRotation(newRot);

        ComputeCurrentRotationHandle(handState.Pipe.StartCoordinate, handState, handState.Rotate.CurrentAxis, &handState.Rotate.CurrentAxis, &handState.Rotate.HandleLocation);
        handState.Debounce.CurrentRotateAxis.Reset(ChangeRotateAxisTimeout);
        handState.Debounce.CurrentRotateAxis.SetCurrent(handState.Rotate.CurrentAxis);

        handState.Rotate.SuppressChangeAnimation = true;

        if (handState.Pipe.Actor)
        {
            handState.Pipe.Actor->FireInteractionEvent(PipeConnected(handState.Pipe.StartCoordinate) ? EInteractionEvent::RotateFinishedConnected : EInteractionEvent::RotateFinished);
        }

        OnPipeDropped.Broadcast(handState.Pipe.StartCoordinate);
    }
}

bool APPipeGrid::RotateEngaged(const FPipeGridCoordinate& location)
{
    return (m_hands.Left.State.InteractionMode == InteractionMode::Rotate && m_hands.Left.State.Pipe.StartCoordinate == location) ||
        (m_hands.Right.State.InteractionMode == InteractionMode::Rotate && m_hands.Right.State.Pipe.StartCoordinate == location);
}

bool APPipeGrid::ShowingRotateHandles(GridHandState& handState)
{
    return m_rotateEnabled && handState.Debounce.ShowRotateHandles.GetCurrent();
}