
#include "PPipeGrid_Internal.h"

bool APPipeGrid::CanStartDrag(GridHandState& handState)
{
    return (m_dragEnabled &&
            handState.Pipe.Actor != nullptr &&
            handState.Rotate.CurrentAxis == EInteractionAxis::None &&
            handState.Pipe.StartCoordinate == handState.Target.Coordinate);
}

bool APPipeGrid::TryHandleDragStart(const FFingerGestureUpdate& update, GridHandState& handState)
{
    bool handled = false;

    if (CanStartDrag(handState))
    {
        GridPipe* pipe = GetActionablePipe(handState.Pipe.StartCoordinate);
        if (pipe && pipe->pipe == handState.Pipe.Actor)
        {
            handState.InteractionMode = InteractionMode::Translate;

            // Clear out the element. It's no longer owned by the grid section
            pipe->pipe = nullptr;

            if (!handState.Pipe.Actor->GetInToolbox())
            {
                // And if it's not in the toolbox, erase the section (we leave it
                // in the toolbox to keep other things from dropping there)
                m_pipesGrid.erase(handState.Pipe.StartCoordinate);
            }

            handState.Drag.AttachStartRotation = WorldTransformToGridTransform(update.Attach).GetRotation();

            handState.Pipe.Actor->FireInteractionEvent(EInteractionEvent::DragStarted);
            handState.Pipe.Actor->SetPipeClass(DefaultPipeClass);
            handState.Pipe.Actor->SetFocusStage(EFocusStage::None);
            handState.Pipe.Actor->SetPipeConnections(PipeDirections::None);
            
            UpdatePipeConnections(handState.Pipe.StartCoordinate);

            // With the pipe piece removed, we need to update all pipe classes (colors)
            // Ignore the return value because even if the grid is currently in a solution state,
            // there's at least one piece (the drag piece) that hasn't been placed
            ResolveGridState();

            OnPipeGrasped.Broadcast(handState.Pipe.StartCoordinate);
            handled = true;
        }
    }

    return handled;
}

void APPipeGrid::HandleTranslateGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState)
{
    switch (update.Progress)
    {
        case EGestureProgress::None:
        case EGestureProgress::Tracked:
        case EGestureProgress::Lost:
            HandleDragLost(handState);
            break;

        default:

        case EGestureProgress::Started:
        case EGestureProgress::Updated:
            {
                if (handState.Pipe.Actor->GetInToolbox())
                {
                    if (handState.Target.Coordinate != handState.Pipe.StartCoordinate)
                    {
                        // The pipe has left the toolbox
                        handState.Pipe.Actor->SetInToolbox(false);

                        EPipeType type = handState.Pipe.Actor->GetPipeType();
                        Toolbox->RemovePipe(type);
                        SpawnInToolbox(type);
                    }
                }

                handState.Pipe.Actor->SetActorRelativeLocation(handState.Target.Location);

                const FQuat targetCurrentRotation = WorldTransformToGridTransform(update.Attach).GetRotation();
                const FQuat rotationDiff = targetCurrentRotation * handState.Drag.AttachStartRotation.Inverse();

                handState.Pipe.Actor->SetActorRelativeRotation(rotationDiff * handState.Pipe.StartRotation);

                FRotator rot = rotationDiff.Rotator();
                rot = SnapRotationTo90DegreeIncrements(rot);
                rot.Normalize();

                PipeDirections newDirections = RotateConnections(handState.Pipe.StartDirections, rot);
                if (newDirections != handState.Pipe.Actor->GetPipeDirections())
                {
                    handState.Pipe.Actor->SetPipeDirections(newDirections);
                }
                break;
            }
    }
}

void APPipeGrid::HandleDragLost(GridHandState& handState)
{
    if (handState.InteractionMode == InteractionMode::Translate)
    {

        FPipeGridCoordinate dragEnd = handState.Target.Coordinate;
        bool blocked = false;

        bool toolboxDrop = false;

        if (!CanDropPipeAtCoordinate(dragEnd, handState, &toolboxDrop))
        {
            blocked = true;

            if (!CanDropPipeAtCoordinate(handState.Pipe.StartCoordinate, handState, &toolboxDrop))
            {
                ToolboxCoordinateForType(handState.Pipe.Actor->GetPipeType(), &dragEnd);
                toolboxDrop = true;
            }
            else
            {
                dragEnd = handState.Pipe.StartCoordinate;
            }
        }

        APPipe* eventPipe = nullptr;
        if (toolboxDrop)
        {
            if (!handState.Pipe.Actor->GetInToolbox())
            {
                Toolbox->AddPipe(handState.Pipe.Actor->GetPipeType());
            }

            SpawnInToolbox(handState.Pipe.Actor->GetPipeType());

            auto it = m_pipesGrid.find(dragEnd);
            
            if (it != m_pipesGrid.end() && it->second.pipe != nullptr)
            {
                eventPipe = it->second.pipe;
            }

            handState.Pipe.Actor->Destroy();
        }
        else
        {
            auto it = m_pipesGrid.find(dragEnd);
            if (it != m_pipesGrid.end() &&
                it->second.pipe != nullptr &&
                it->second.pipe != handState.Pipe.Actor)
            {
                it->second.pipe->Destroy();
                it->second.pipe = nullptr;
            }

            GridPipe insertedPipe;
            insertedPipe.pipe = handState.Pipe.Actor;
            insertedPipe.currentLocation = dragEnd;
            insertedPipe.currentPipeClass = DefaultPipeClass;
            insertedPipe.pipe->SetInToolbox(false);

            eventPipe = insertedPipe.pipe;

            m_pipesGrid[dragEnd] = insertedPipe;
            handState.Pipe.Actor->SetActorRelativeLocation(GridCoordinateToGridLocation(dragEnd));

            FRotator newRot;
            GetPipeRotation(handState.Pipe.Actor->GetPipeType(), handState.Pipe.Actor->GetPipeDirections(), newRot);
            handState.Pipe.Actor->SetActorRelativeRotation(newRot);

            UpdatePipeConnections(dragEnd);
        }

        if (toolboxDrop || blocked)
        {
            handState.Pipe.Actor = nullptr;
            ClearHandDebounce(handState);
            handState.InteractionMode = InteractionMode::None;
        }
        else
        {
            handState.InteractionMode = InteractionMode::Focus;
            handState.Pipe.StartCoordinate = dragEnd;

            // Stay in rotate handle mode (don't lose focus at end of rotate)
            handState.Debounce.ShouldKeepFocus.Reset(LoseRotateFocusTimeout);
            handState.Debounce.ShouldKeepFocus.SetCurrent(true);

            handState.Debounce.ShowRotateHandles.SetCurrent(true);

            handState.Debounce.CurrentRotateAxis.Reset(ChangeRotateAxisTimeout);
            handState.Debounce.CurrentRotateAxis.SetCurrent(EInteractionAxis::None);
        }

        if (!toolboxDrop)
        {
            // Make sure that all appropriate toolbox pipes exist (there are a few timing issues that can result 
            // in getting a drop before we've updated the pipe appropriately
            EnsureToolboxPipes();
        }

        if (eventPipe != nullptr)
        {
            if (blocked)
            {
                eventPipe->FireInteractionEvent(EInteractionEvent::DragFinishedBlocked);
            }
            else
            {
                eventPipe->FireInteractionEvent(PipeConnected(dragEnd) ? EInteractionEvent::DragFinishedConnected : EInteractionEvent::DragFinished);
            }
        }

        OnPipeDropped.Broadcast(dragEnd);
    }
}

void APPipeGrid::AttachTarget(const FFingerGestureUpdate& update, GridHandState& handState)
{
    if (!handState.Target.Attached)
    {
        handState.Target.Attached = true;
        handState.Target.InAttachSpace = WorldLocationToTransformLocation(update.Target.GetLocation(), update.Attach);
    }
}

void APPipeGrid::DetachTarget(GridHandState& handState)
{
    handState.Target.Attached = false;
    handState.Target.InAttachSpace = FVector::ZeroVector;
}

FVector APPipeGrid::GetEffectiveTarget(const FFingerGestureUpdate& update, GridHandState& handState)
{
    if (handState.Target.Attached)
    {
        return TransformLocationToWorldLocation(handState.Target.InAttachSpace, update.Attach);
    }
    else
    {
        return update.Target.GetLocation();
    }
}

bool APPipeGrid::CanDropPipeAtCoordinate(const FPipeGridCoordinate& coordinate, const GridHandState& hand, bool* intoToolbox)
{
    // For drag, we typically can't drop onto spots that are already occupied
    auto it = m_pipesGrid.find(coordinate);
    bool canDrop = (it == m_pipesGrid.end());
    bool toolboxDrop = false;

    if (!canDrop && hand.Pipe.Actor && !IsToolboxDragging())
    {
        FPipeGridCoordinate toolboxCoordinate;

        canDrop =
            (ToolboxCoordinateForType(EPipeType::Straight, &toolboxCoordinate) && toolboxCoordinate == coordinate) ||
            (ToolboxCoordinateForType(EPipeType::Corner, &toolboxCoordinate) && toolboxCoordinate == coordinate) ||
            (ToolboxCoordinateForType(EPipeType::Junction, &toolboxCoordinate) && toolboxCoordinate == coordinate);

        toolboxDrop = canDrop;
    }

    if (intoToolbox)
    {
        (*intoToolbox) = toolboxDrop;
    }

    return canDrop;
}