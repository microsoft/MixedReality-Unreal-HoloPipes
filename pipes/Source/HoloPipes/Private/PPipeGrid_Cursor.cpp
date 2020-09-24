#include "PPipeGrid_Cursor.h"
#include "PPipeGrid_Internal.h"

// We've had some problems on ARM64 with complex structs not being reliably zero initialized when setting
// FStruct foo = {}. We therefore use const Empty structs to reliably initialize to known values

const FSimpleCursorState FSimpleCursorState::Empty =
{
    EInteractionPhase::None,
    FVector::ZeroVector,
    false
};

const FRotateCursorState FRotateCursorState::Empty =
{
    EInteractionPhase::None,
    FVector::ZeroVector,
    EInteractionAxis::None,
    FRotator::ZeroRotator,
    false,
    false
};

const FConnectionsCursorState FConnectionsCursorState::Empty =
{
    EInteractionPhase::None,
    FVector::ZeroVector,
    0,
    false
};

const FCursorStateHand FCursorStateHand::Empty =
{
    FSimpleCursorState::Empty,
    FSimpleCursorState::Empty,
    FRotateCursorState::Empty,
    FConnectionsCursorState::Empty
};

const FCursorState FCursorState::Empty =
{
    FCursorStateHand::Empty,
    FCursorStateHand::Empty,
};

void APPipeGrid::UpdateCursorForHand(GridHandState& hand, FCursorStateHand& newState)
{
    if (TargetCursorEnabled)
    {
        newState.Target.Phase = hand.Target.Phase;
        newState.Target.Location = GridLocationToWorldLocation(hand.Target.Location);
    }

    if (m_hands.InteractionEnabled && m_placeWorldState.Mode == PlaceWorldMode::None)
    {
        switch (hand.InteractionMode)
        {
            case InteractionMode::Focus:
            {
                bool canDrag = CanStartDrag(hand);

                newState.Translate.Location = GridCoordinateToGridLocation(hand.Pipe.StartCoordinate);
                newState.Translate.Phase = canDrag ? EInteractionPhase::Pending : EInteractionPhase::Visible;

                if (m_rotateEnabled && hand.Pipe.Actor && !hand.Pipe.Actor->GetInToolbox())
                {
                    if (hand.Debounce.ShowRotateHandles.GetCurrent())
                    {
                        newState.Rotate.Phase = hand.Debounce.CurrentRotateAxis.GetCurrent() == EInteractionAxis::None ? EInteractionPhase::Visible : EInteractionPhase::Pending;
                        newState.Rotate.Location = hand.Rotate.HandleLocation;
                        newState.Rotate.Axis = hand.Rotate.CurrentAxis;
                        newState.Rotate.AxisRotations = hand.Rotate.AxisRotations;
                        newState.Rotate.SuppressChangeAnimation = hand.Rotate.SuppressChangeAnimation;

                        hand.Rotate.SuppressChangeAnimation = false;
                    }
                    else
                    {
                        newState.Rotate.Phase = EInteractionPhase::Blocked;
                    }
                }

                break;
            }

            case InteractionMode::Translate:
            {
                newState.Translate.Location = GridCoordinateToGridLocation(hand.Target.Coordinate);
                newState.Translate.Phase = CanDropPipeAtCoordinate(hand.Target.Coordinate, hand, nullptr) ? EInteractionPhase::Ready : EInteractionPhase::Blocked;

                newState.Connections.Directions = (int)hand.Pipe.Actor->GetPipeDirections();
                newState.Connections.Location = newState.Translate.Location;
                newState.Connections.Phase = newState.Translate.Phase;
                break;
            }

            case InteractionMode::Rotate:
            {
                newState.Rotate.Phase = EInteractionPhase::Ready;
                newState.Rotate.Location = hand.Rotate.HandleLocation;
                newState.Rotate.Axis = hand.Rotate.CurrentAxis;
                newState.Rotate.AxisRotations = hand.Rotate.AxisRotations;

                newState.Connections.Directions = (int)hand.Pipe.Actor->GetPipeDirections();
                newState.Connections.Location = GridCoordinateToGridLocation(hand.Pipe.StartCoordinate);
                newState.Connections.Phase = EInteractionPhase::Ready;

                newState.Translate.Location = GridCoordinateToGridLocation(hand.Pipe.StartCoordinate);
                newState.Translate.Phase = EInteractionPhase::Visible;
                break;
            }
        }
    }
}

void APPipeGrid::UpdateCursor()
{
    FCursorState newState = FCursorState::Empty;

    UpdateCursorForHand(m_hands.Left.State, newState.Left);
    UpdateCursorForHand(m_hands.Right.State, newState.Right);

    SendCursorUpdate(newState);
}

bool APPipeGrid::ProcessCursorUpdatesForHand(const FCursorStateHand& newState, FCursorStateHand& state, bool forceDirty)
{
    bool dirty = false;

    const bool targetDirty =
        forceDirty ||
        newState.Target.Phase != state.Target.Phase ||
        !newState.Target.Location.Equals(state.Target.Location);

    const bool translateDirty =
        forceDirty ||
        newState.Translate.Phase != state.Translate.Phase ||
        (FVector::DistSquared(newState.Translate.Location, state.Translate.Location) > ((CellSize / 2) * (CellSize / 2)));

    const bool rotateDirty =
        forceDirty ||
        (newState.Rotate.Phase != state.Rotate.Phase) ||
        (newState.Rotate.Axis != state.Rotate.Axis) ||
        !newState.Rotate.AxisRotations.Equals(state.Rotate.AxisRotations) ||
        (FVector::DistSquared(newState.Rotate.Location, state.Rotate.Location) > ((CellSize / 2) * (CellSize / 2)));

    const bool connectionsDirty =
        forceDirty ||
        newState.Connections.Phase != state.Connections.Phase ||
        (newState.Connections.Directions != state.Connections.Directions) ||
        (FVector::DistSquared(newState.Connections.Location, state.Connections.Location) > ((CellSize / 2) * (CellSize / 2)));

    state = newState;
    state.Target.Dirty = targetDirty;
    state.Translate.Dirty = translateDirty;
    state.Rotate.Dirty = rotateDirty;
    state.Connections.Dirty = connectionsDirty;

    return (targetDirty || translateDirty || rotateDirty || connectionsDirty);
}

void APPipeGrid::UpdateCursorProxy(const FConnectionsCursorState& cursorState, GridHandState& hand)
{
    if (cursorState.Dirty)
    {
        switch (cursorState.Phase)
        {
            case EInteractionPhase::Blocked:
            case EInteractionPhase::Ready:
            {
                if (hand.Pipe.Actor == nullptr)
                {
                    hand.Proxy.Clear();
                }
                else
                {
                    if (hand.Proxy.Actor == nullptr && PipeClass)
                    {
                        APPipe* newPipe = GetWorld()->SpawnActor<APPipe>(PipeClass);

                        if (newPipe)
                        {
                            if (!newPipe->Initialize(hand.Pipe.Actor->GetPipeType(), DefaultPipeClass, hand.Pipe.Actor->GetPipeDirections(), false /* fixed */, false /* inToolbox */, EPipeProxyType::ProxyReady))
                            {
                                newPipe->Destroy();
                            }
                            else
                            {
                                newPipe->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                                hand.Proxy.Actor = newPipe;
                            }
                        }
                    }

                    if (hand.Proxy.Actor)
                    {
                        FRotator rotation;
                        GetPipeRotation(hand.Proxy.Actor->GetPipeType(), (PipeDirections)(cursorState.Directions), rotation);
                        hand.Proxy.Actor->SetActorRelativeTransform(FTransform(rotation, cursorState.Location, hand.Proxy.Actor->GetRootComponent()->GetRelativeTransform().GetScale3D()));
                        hand.Proxy.Actor->SetProxyBlocked(cursorState.Phase == EInteractionPhase::Blocked);
                    }
                }
                break;
            }

            default:
                hand.Proxy.Clear();
                break;
        }
    }
}

void APPipeGrid::SendCursorUpdate(const FCursorState& newState, bool forceDirty)
{
    bool leftDirty = ProcessCursorUpdatesForHand(newState.Left, m_hands.Left.Cursor, forceDirty);
    bool rightDirty = ProcessCursorUpdatesForHand(newState.Right, m_hands.Right.Cursor, forceDirty);

    UpdateCursorProxy(m_hands.Left.Cursor.Connections, m_hands.Left.State);
    UpdateCursorProxy(m_hands.Right.Cursor.Connections, m_hands.Right.State);

    if (leftDirty || rightDirty)
    {
        FCursorState updatedState = { m_hands.Left.Cursor, m_hands.Right.Cursor };
        OnCursorUpdated(updatedState);
    }
}