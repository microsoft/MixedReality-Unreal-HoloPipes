// Fill out your copyright notice in the Description page of Project Settings.

#include "PPipeGrid.h"
#include "PPipeGrid_Internal.h"

// Sets default values
APPipeGrid::APPipeGrid() : m_pipesGrid(0, FPipeGridCoordinate::HashOf)
{
    PrimaryActorTick.bCanEverTick = true;
    Toolbox = nullptr;
    Pawn = nullptr;

	CellSize = 1.0f;
    TargetCursorEnabled = false;
    MinGridScale = 0.1;
    MaxGridScale = 2.5f;
    m_labelsEnabled = false;

    ChangeRotateAxisTimeout = 0.25f;
    ShowRotateHandlesTimeout = 0.5f;
    LoseRotateFocusTimeout = 0.15f;

    FocusGutterRatio = 1.8f;

    m_hands = {};
}

// Called when the game starts or when spawned
void APPipeGrid::BeginPlay()
{
	Super::BeginPlay();	
}

void APPipeGrid::Clear()
{
    SetInteractionEnabled(false);

	for (auto pipe : m_pipesGrid)
	{
        if (pipe.second.pipe)
        {
            pipe.second.pipe->Destroy();
        }
	}

	m_pipesGrid.clear();

    if (Toolbox)
    {
        Toolbox->Clear();
    }

    ClearCurrentLevelScore();
}

void APPipeGrid::SetPawn(APMRPawn* pawn)
{
    Pawn = pawn;
    if (pawn)
    {
        pawn->OnGestureNotification.AddDynamic(this, &APPipeGrid::HandlePawnGestureUpdate);
        pawn->OnGazeNotification.AddDynamic(this, &APPipeGrid::HandlePawnGazeUpdate);
    }
}

void APPipeGrid::AddPipes(const std::vector<PipeSegmentGenerated>& pipes, AddPipeOptions options)
{
	for (const auto& pipeToAdd : pipes)
	{
		AddPipe(pipeToAdd, options);
	}
}

void APPipeGrid::AddPipe(const PipeSegmentGenerated& pipeToAdd, AddPipeOptions options)
{
    if (m_toolboxEnabled && Toolbox)
    {
        const bool isToolboxLocation = CoordinateInToolbox(pipeToAdd.Location);
        const bool isInToolbox = IsAnyFlagSet(options, AddPipeOptions::InToolbox);
        const bool IsFromToolbox = IsAnyFlagSet(options, AddPipeOptions::FromToolbox);

        if (isToolboxLocation)
        {
            if (IsFromToolbox)
            {
                // We're being asked to pull a pipe out of the toolbox to spawn into a toolbox location.
                // We treat that as a no-op
                return;
            }
            else if (!isInToolbox)
            {
                // We're being asked to spawn a new pipe that overlaps the toolbox. Just add it to the toolbox instead
                Toolbox->AddPipe(pipeToAdd.Type);
                return;
            }
            /*
            else
            {
                We're being asked to spawn the pipe that lives in the toolbox, so do so
            }
            */
        }

        else if (IsFromToolbox)
        {
            if (Toolbox->CountRemaining(pipeToAdd.Type) > 0)
            {
                Toolbox->RemovePipe(pipeToAdd.Type);

                // SpawnInToolbox will just update existing pipes if appopriate, or
                // create new ones if it hasn't been created yet
                SpawnInToolbox(pipeToAdd.Type);
            }
        }
    }

    if (PipeClass)
	{
		APPipe* newPipe = GetWorld()->SpawnActor<APPipe>(PipeClass);

		if (newPipe)
		{
			if (!newPipe->Initialize(pipeToAdd.Type, 
                                     pipeToAdd.PipeClass, 
                                     pipeToAdd.Connections, 
                                     IsAnyFlagSet(options, AddPipeOptions::Fixed), 
                                     IsAnyFlagSet(options, AddPipeOptions::InToolbox)))
			{
				newPipe->Destroy();
			}
			else
			{
                auto it = m_pipesGrid.find(pipeToAdd.Location);
                if (it != m_pipesGrid.end() && it->second.pipe)
                {
                    it->second.pipe->Destroy();
                    it->second.pipe = nullptr;
                }

                GridPipe gridPipe;
                gridPipe.pipe = newPipe;
                gridPipe.currentLocation = pipeToAdd.Location;
                gridPipe.currentPipeClass = pipeToAdd.PipeClass;
                m_pipesGrid[pipeToAdd.Location] = gridPipe;

                newPipe->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                newPipe->SetOwner(this);

                FVector location = GridCoordinateToGridLocation(pipeToAdd.Location);
                newPipe->SetActorRelativeTransform(FTransform(newPipe->GetDesiredRotation(), location));

                UpdatePipeConnections(pipeToAdd.Location);
			}
		}
	}
}

void APPipeGrid::FinalizeLevel()
{
    // Make sure all the colors/labels are correctly up to date
    ResolveGridState();
}

void APPipeGrid::GetCurrentPlaced(std::vector<PipeSegmentGenerated>& realizedPipes)
{
    for (auto& pipeEntry : m_pipesGrid)
    {
        if (pipeEntry.second.pipe && !pipeEntry.second.pipe->GetPipeFixed() && !pipeEntry.second.pipe->GetInToolbox())
        {
            PipeSegmentGenerated segment = {};
            segment.Type = pipeEntry.second.pipe->GetPipeType();
            segment.PipeClass = pipeEntry.second.pipe->GetPipeClass();
            segment.Location = pipeEntry.first;
            segment.Connections = pipeEntry.second.pipe->GetPipeDirections();

            realizedPipes.push_back(segment);
        }
    }
}

bool APPipeGrid::GetAnyPipesPlaced()
{
    for (auto& pipeEntry : m_pipesGrid)
    {
        if (pipeEntry.second.pipe && !pipeEntry.second.pipe->GetPipeFixed() && !pipeEntry.second.pipe->GetInToolbox())
        {
            return true;
        }
    }

    return false;
}

bool APPipeGrid::GetAnyPlacedPipesDisconnected()
{
    for (auto& pipeEntry : m_pipesGrid)
    {
        if (pipeEntry.second.pipe && !pipeEntry.second.pipe->GetPipeFixed() && !pipeEntry.second.pipe->GetInToolbox() && pipeEntry.second.pipe->GetPipeClass() == DefaultPipeClass)
        {
            return true;
        }
    }

    return false;
}

int32 APPipeGrid::GetActionablePipesCount()
{
    int actionablePipes = 0;
    for (auto& pipeEntry : m_pipesGrid)
    {
        if (pipeEntry.second.pipe && !pipeEntry.second.pipe->GetPipeFixed())
        {
            actionablePipes++;
        }
    }

    return actionablePipes;
}

GridPipe* APPipeGrid::GetActionablePipe(FPipeGridCoordinate gridLocation)
{
    if (!RotateEngaged(gridLocation))
    {
        auto it = m_pipesGrid.find(gridLocation);

        if (it != m_pipesGrid.end() &&
            it->second.pipe &&
            !it->second.pipe->GetPipeFixed())
        {
            return &(it->second);
        }
    }
    
    return nullptr;
}

void APPipeGrid::UpdatePipeConnections(const FPipeGridCoordinate& coordinate)
{
    auto itTarget = m_pipesGrid.find(coordinate);
    bool haveTarget = ((itTarget != m_pipesGrid.end()) && itTarget->second.pipe && !itTarget->second.pipe->GetInToolbox());

    PipeDirections targetDirections = haveTarget ? itTarget->second.pipe->GetPipeDirections() : PipeDirections::None;
    PipeDirections targetConnections = PipeDirections::None;

    for (int i = 0; i < APPipe::ValidDirectionsCount; i++)
    {
        PipeDirections targetCandidate = APPipe::ValidDirections[i];
        PipeDirections neighborCandidate = APPipe::InvertPipeDirection(targetCandidate);

        bool targetCanConnect = IsAnyFlagSet(targetDirections, targetCandidate);
        bool connected = false;

        auto itNeighbor = m_pipesGrid.find(coordinate + APPipe::PipeDirectionToLocationAdjustment(targetCandidate));
        bool haveNeighbor = ((itNeighbor != m_pipesGrid.end()) && itNeighbor->second.pipe && !itNeighbor->second.pipe->GetInToolbox());

        if (haveNeighbor)
        {
            PipeDirections neighborDirections = itNeighbor->second.pipe->GetPipeDirections();
            connected = targetCanConnect && IsAnyFlagSet(neighborDirections, neighborCandidate);

            PipeDirections neighborConnections = itNeighbor->second.pipe->GetPipeConnections();
            itNeighbor->second.pipe->SetPipeConnections(connected ? (neighborConnections | neighborCandidate) : (neighborConnections & ~neighborCandidate));
        }

        if (connected)
        {
            targetConnections |= targetCandidate;
        }
    }

    if (haveTarget)
    {
        itTarget->second.pipe->SetPipeConnections(targetConnections);
    }
}

void APPipeGrid::GetPipeInfo(FPipeGridCoordinate gridLocation, EPipeType* pipeType, int* pipeClass, PipeDirections* connections)
{
    auto it = m_pipesGrid.find(gridLocation);
    if (it != m_pipesGrid.end() && it->second.pipe)
    {
        (*pipeType) = it->second.pipe->GetPipeType();
        (*pipeClass) = it->second.pipe->GetPipeClass();
        (*connections) = it->second.pipe->GetPipeDirections();
    }
    else
    {
        (*pipeType) = EPipeType::None;
        (*pipeClass) = DefaultPipeClass;
        (*connections) = PipeDirections::None;
    }
}

void APPipeGrid::SetInteractionEnabled(bool enabled)
{
    if (enabled != m_hands.InteractionEnabled)
    {
        ChangeInteractionEnabledForHand(m_hands.Left);
        ChangeInteractionEnabledForHand(m_hands.Right);

        SendCursorUpdate(FCursorState::Empty, true);

        m_hands.InteractionEnabled = enabled;

        if (Toolbox)
        {
            Toolbox->SetFocus(EToolboxFocus::None);
        }
    }
}

void APPipeGrid::ChangeInteractionEnabledForHand(GridHand& hand)
{
    HandleDragLost(hand.State);
    HandleToolboxDragLost(hand.State);

    ClearHand(hand);
}

void APPipeGrid::ClearHand(GridHand& hand)
{
    hand.State.Proxy.Clear();
    hand.State = {};
    ClearHandDebounce(hand.State);

#if !UE_BUILD_SHIPPING
    ClearHandFocusHistory(hand.Focus);
#endif
}

void APPipeGrid::ClearHandFocusHistory(GridHandFocus& handFocus)
{
    for (auto& pipe : handFocus.HandOnly)
    {
        pipe->SetFocusStage(EFocusStage::None);
    }
    handFocus.HandOnly.clear();

    for (auto& pipe : handFocus.HandAndGaze)
    {
        pipe->SetFocusStage(EFocusStage::None);
    }
    handFocus.HandAndGaze.clear();
}

void APPipeGrid::ClearHandDebounce(GridHandState& handState)
{
    handState.Debounce.ShouldKeepFocus.Reset();
    handState.Debounce.CurrentRotateAxis.Reset();
    handState.Debounce.ShowRotateHandles.Reset(ShowRotateHandlesTimeout);
}


void APPipeGrid::HandlePawnGazeUpdate(FGazeUpdate update)
{
    UpdateGaze(update);
}

void APPipeGrid::HandlePawnGestureUpdate(FGestureUpdate update)
{
    if (m_placeWorldState.Mode != PlaceWorldMode::None)
    {
        HandlePlaceWorldGestureUpdate(update);
    }

#if !UE_BUILD_SHIPPING
    ClearHandFocusHistory(m_hands.Left.Focus);
    ClearHandFocusHistory(m_hands.Right.Focus);
#endif

    HandlePawnGestureUpdateForHand(EFingerHand::Left, update.Left, m_hands.Left);
    HandlePawnGestureUpdateForHand(EFingerHand::Right, update.Right, m_hands.Right);

    if (Toolbox)
    {
        if (IsToolboxDragging())
        {
            Toolbox->SetFocus(EToolboxFocus::Carried);
        }
        else if (IsToolboxFocused())
        {
            Toolbox->SetFocus(EToolboxFocus::Focused);
        }
        else
        {
            Toolbox->SetFocus(EToolboxFocus::None);
        }
    }

    UpdateCursor();
}

void APPipeGrid::HandlePawnGestureUpdateForHand(EFingerHand fingerHand, const FFingerGestureUpdate& update, GridHand& hand)
{
    hand.State.Target.Location = WorldLocationToGridLocation(GetEffectiveTarget(update, hand.State));
    hand.State.Target.Coordinate = GridLocationToGridCoordinate(hand.State.Target.Location);

    if (m_placeWorldState.Mode == PlaceWorldMode::None)
    {
        InteractionMode previousInteractionMode = hand.State.InteractionMode;
        const APPipe* previousPipe = hand.State.Pipe.Actor;

        if (m_hands.InteractionEnabled)
        {
            switch (previousInteractionMode)
            {
                case InteractionMode::None:
                    HandleNoneGestureUpdate(update, hand);
                    break;

                case InteractionMode::Focus:
                case InteractionMode::ToolboxFocus:
                    HandleFocusedGestureUpdate(update, hand);
                    break;

                case InteractionMode::Translate:
                    HandleTranslateGestureUpdate(update, hand.State);
                    break;

                case InteractionMode::Rotate:
                    HandleRotateGestureUpdate(update, hand.State);
                    break;

                case InteractionMode::ToolboxTranslate:
                    HandleToolboxGestureUpdate(update, hand.State);
                    break;
            }
        }

        if (m_hands.InteractionEnabled &&
            previousInteractionMode != hand.State.InteractionMode)
        {
            if (previousInteractionMode == InteractionMode::Rotate || previousInteractionMode == InteractionMode::Translate)
            {
                // We have finished an interaction. Update score appropriately
                ScoreHandRelease(fingerHand, previousPipe);
                
                // Also resolve the grid (updating colors as necessary, and checking for a solution).
                if (!CheckForSolution())
                {
                    // We're not in a solution state, but announce that the grid has changed.
                    OnGridUpdated.Broadcast();
                }
            }
            else if (hand.State.InteractionMode == InteractionMode::Rotate || hand.State.InteractionMode == InteractionMode::Translate)
            {
                // We have started an interaction. Update score appropriately
                ScoreHandGrasp(fingerHand, hand.State.Pipe.Actor);
            }
        }
    }

    switch (update.Progress)
    {
        case EGestureProgress::None:
            hand.State.Target.Phase = EInteractionPhase::None;
            DetachTarget(hand.State);
            break;

        case EGestureProgress::Tracked:
            hand.State.Target.Phase = EInteractionPhase::Pending;
            DetachTarget(hand.State);
            break;

        case EGestureProgress::Started:
        case EGestureProgress::Updated:
            hand.State.Target.Phase = EInteractionPhase::Ready;
            AttachTarget(update, hand.State);
            break;

        case EGestureProgress::Lost:
            hand.State.Target.Phase = EInteractionPhase::Pending;
            DetachTarget(hand.State);
            break;
    }
}

void APPipeGrid::UpdateFocus(GridHand& hand)
{
    float now = GetWorld()->GetTimeSeconds();

    const FPipeGridCoordinate previousCoord = hand.State.Pipe.StartCoordinate;
    APPipe* previousPipe = hand.State.Pipe.Actor;

    if (previousPipe != nullptr)
    {
        GridPipe* previousGridPipe = GetActionablePipe(previousCoord);
        if (!previousGridPipe || previousGridPipe->pipe != previousPipe)
        {
            // We did have a pipe, but it no longer exists at that location
            previousPipe = nullptr;
        }
    }

    FPipeGridCoordinate toolboxHandleCoord = FPipeGridCoordinate::Zero;
    const bool haveToolboxHandle = ToolboxCoordinateForType(EPipeType::Start, &toolboxHandleCoord);
    const bool toolboxHandleFocused = haveToolboxHandle && hand.State.Target.Coordinate == toolboxHandleCoord;

    FPipeGridCoordinate currentCoord = FPipeGridCoordinate::Zero;
    APPipe* currentPipe = nullptr;

    float currentDistSq = 0;

    const FVector aabbCornerOffset = FVector(CellSize * FocusGutterRatio) / 2.0f;

    // Consider all pipes near the hand to pick the best "point in time" candidate for focus
    for (int xOffset = -1; xOffset <= 1; xOffset++)
    {
        for (int yOffset = -1; yOffset <= 1; yOffset++)
        {
            for (int zOffset = -1; zOffset <= 1; zOffset++)
            {
                FPipeGridCoordinate candidateCoord = hand.State.Target.Coordinate + FPipeGridCoordinate{ xOffset, yOffset, zOffset };
                const GridPipe* candidateGridPipe = GetActionablePipe(candidateCoord);

                if (candidateGridPipe && candidateGridPipe->pipe)
                {
                    // We have a pipe

                    const FVector candidateLocation = GridCoordinateToGridLocation(candidateCoord);
                    FVector intersection;

                    if (!candidateGridPipe->pipe->GetInToolbox() && !GazeIntersectsAABB(candidateLocation - aabbCornerOffset, candidateLocation + aabbCornerOffset, &intersection))
                    {
                        // The pipe isn't in the toolbox and doesn't have gaze nearby. Ignore it for focus.
                        // (We don't care about gaze in the toolbox because it is common to interact with the toolbox
                        // without looking directly at it).
#if !UE_BUILD_SHIPPING
                        if (m_debugFocus)
                        {
                            
                            // Ignore it as a focus candidate (still record it for debug visualizations)
                            candidateGridPipe->pipe->SetFocusStage(EFocusStage::HandOnly);
                            hand.Focus.HandOnly.push_back(candidateGridPipe->pipe);
                        }
#endif
                    }
                    else
                    {

#if !UE_BUILD_SHIPPING
                        if (m_debugFocus)
                        {
                            // The pipe does have gaze nearby, so consider it as a candidate
                            hand.Focus.HandAndGaze.push_back(candidateGridPipe->pipe);
                        }
#endif

                        float candidateDistSq = FVector::DistSquared(hand.State.Target.Location, GridCoordinateToGridLocation(candidateCoord));
                        bool takeCandidate = (currentPipe == nullptr || candidateDistSq < currentDistSq);

                        if (takeCandidate)
                        {
                            // This new pipe is currently the best candidate, so take it (replacing any previous
                            // best candidate)

#if !UE_BUILD_SHIPPING
                            if (m_debugFocus && currentPipe != nullptr)
                            {
                                // Mark it for visualization (as long as we're not downgrading it)
                                if (currentPipe->GetFocusStage() != EFocusStage::HandAndGazeAndDistance)
                                {
                                    currentPipe->SetFocusStage(EFocusStage::HandAndGaze);
                                }
                            }
#endif

                            currentPipe = candidateGridPipe->pipe;
                            currentCoord = candidateCoord;
                            currentDistSq = candidateDistSq;
                        }

#if !UE_BUILD_SHIPPING
                        else if (m_debugFocus)
                        {
                            // This new pipe isn't the best candidate, so we'll only mark it for
                            // visualization purposes (but don't downgrade it)

                            if (candidateGridPipe->pipe->GetFocusStage() != EFocusStage::HandAndGazeAndDistance)
                            {
                                candidateGridPipe->pipe->SetFocusStage(EFocusStage::HandAndGaze);
                            }
                        }
#endif
                    }
                }
            }
        }
    }

    if (currentPipe)
    {

#if !UE_BUILD_SHIPPING

        if (m_debugFocus)
        {
            // We haven't marked the candidate pipe yet, so mark it for visualization
            currentPipe->SetFocusStage(EFocusStage::HandAndGazeAndDistance);
        }
#endif

        if (toolboxHandleFocused)
        {
            // Clear out the current pipe if the toolbox handle is focused
            currentPipe = nullptr;
        }
    }

    bool pipeChanged = (currentPipe != previousPipe);
    bool shouldKeepFocus = true;
    bool usePrevious = false;

    if (pipeChanged)
    {
        if (previousPipe)
        {
            // The neighborhood is defined as the pipe itself and its direct neighbors
            const bool targetInPreviousNeighborhood = (hand.State.Target.Coordinate.DistanceToSq(previousCoord) <= 3.0f);

            // We have a previous pipe. We should only consider using it if we're still in its same neighborhood
            if (targetInPreviousNeighborhood)
            {
                // If we're focusing another pipe, or we're focusing the toolbox handle, we need to consider 
                // whether the previous pipe should hold on to focus a little longer (ie, if it has ever shown rotate handles 
                // and our focus has only recently changed to something else)
                if (currentPipe || toolboxHandleFocused)
                {
                    // Focus isn't sticky in the toolbox
                    if (!previousPipe->GetInToolbox())
                    {
                        // We want to keep focus if both fingers and gaze are still pretty close to the previous pipe
                        const FVector previousLocation = GridCoordinateToGridLocation(previousCoord);
                        const FVector minAABB = previousLocation - aabbCornerOffset;
                        const FVector maxAABB = previousLocation + aabbCornerOffset;

                        FVector intersection;
                        if (GazeIntersectsAABB(minAABB, maxAABB, &intersection))
                        {
                            usePrevious = true;
                            for (int i = 0; usePrevious && i < 3; i++)
                            {
                                usePrevious = (hand.State.Target.Location[i] > minAABB[i] && hand.State.Target.Location[i] < maxAABB[i]);
                            }
                        }

                        // If we're not close enough to automatically keep focus, keep focus if we haven't timed out yet
                        if (!usePrevious && hand.State.Debounce.ShouldKeepFocus.UpdateCurrent(false, now))
                        {
                            usePrevious = true;
                            shouldKeepFocus = false;
                        }
                    }
                }
                else
                {
                    usePrevious = true;
                }
            }
        }
    }

    if (usePrevious)
    {
        currentPipe = previousPipe;
        currentCoord = previousCoord;
        pipeChanged = false;
    }

    bool wasShowingRotateHandles = ShowingRotateHandles(hand.State);
    EInteractionAxis previousRotateHandle = wasShowingRotateHandles ? hand.State.Rotate.CurrentAxis : EInteractionAxis::None;

    if (currentPipe)
    {
        // We've got a current pipe, so update our state
        hand.State.InteractionMode = InteractionMode::Focus;
        hand.State.Pipe.Actor = currentPipe;
        hand.State.Pipe.StartCoordinate = currentCoord;
        hand.State.Pipe.StartDirections = hand.State.Pipe.Actor->GetPipeDirections();
        hand.State.Pipe.StartRotation = hand.State.Pipe.Actor->GetDesiredRotation().Quaternion();

        EInteractionAxis nextHandleAxis = EInteractionAxis::None;
        FVector nextHandleLocation = FVector::ZeroVector;

        if (pipeChanged)
        {
            hand.State.Debounce.ShouldKeepFocus.Reset();
            hand.State.Debounce.ShouldKeepFocus.SetCurrent(true);

            hand.State.Debounce.CurrentRotateAxis.Reset();
            hand.State.Debounce.CurrentRotateAxis.SetCurrent(EInteractionAxis::None);

            hand.State.Debounce.ShowRotateHandles.SetCurrent(false);
        }
        else
        {
            if (hand.State.Debounce.ShowRotateHandles.UpdateCurrent(m_rotateEnabled && !currentPipe->GetInToolbox(), now))
            {
                ComputeCurrentRotationHandle(currentCoord, hand.State, &nextHandleAxis, &nextHandleLocation);

                if (!wasShowingRotateHandles)
                {
                    hand.State.Debounce.ShouldKeepFocus.Reset(LoseRotateFocusTimeout);
                    hand.State.Debounce.ShouldKeepFocus.SetCurrent(true);

                    hand.State.Debounce.CurrentRotateAxis.Reset(ChangeRotateAxisTimeout);
                    hand.State.Debounce.CurrentRotateAxis.SetCurrent(nextHandleAxis);
                }
            }

            if (shouldKeepFocus)
            {
                hand.State.Debounce.ShouldKeepFocus.SetCurrent(true);
            }
        }

        if (nextHandleAxis == hand.State.Debounce.CurrentRotateAxis.UpdateCurrent(nextHandleAxis, now))
        {
            hand.State.Rotate.CurrentAxis = nextHandleAxis;
            hand.State.Rotate.HandleLocation = nextHandleLocation;
        }
    }
    else
    {
        // We don't have a current pipe, so clear our state
        hand.State.Pipe = {};
        
        ClearHandDebounce(hand.State);
        hand.State.Rotate.CurrentAxis = EInteractionAxis::None;
        hand.State.Rotate.HandleLocation = FVector::ZeroVector;

        // And check if we're focusing the toolbox

        if (toolboxHandleFocused && !IsToolboxDragging())
        {
            hand.State.InteractionMode = InteractionMode::ToolboxFocus;
            hand.State.Pipe.StartCoordinate = toolboxHandleCoord;
        }
        else
        {
            hand.State.InteractionMode = InteractionMode::None;
        }
    }

    bool isShowingRotateHandles = ShowingRotateHandles(hand.State);
    EInteractionAxis currentRotateHandle = isShowingRotateHandles ? hand.State.Rotate.CurrentAxis : EInteractionAxis::None;

    if (currentRotateHandle != previousRotateHandle)
    {
        OnRotateHandleShown.Broadcast(currentRotateHandle == EInteractionAxis::None ? currentCoord : previousCoord, currentRotateHandle);
    }
}

void APPipeGrid::HandleNoneGestureUpdate(const FFingerGestureUpdate& update, GridHand& hand)
{
    GridHandTarget oldTarget = hand.State.Target;
    ClearHand(hand);

    hand.State.Target = oldTarget;
    hand.State.InteractionMode = InteractionMode::None;

    switch (update.Progress)
    {
        case EGestureProgress::Tracked:
        case EGestureProgress::Started:
        case EGestureProgress::Updated:
            UpdateFocus(hand);
            break;
    }
}

void APPipeGrid::HandleFocusedGestureUpdate(const FFingerGestureUpdate& update, GridHand& hand)
{
    switch (update.Progress)
    {
        case EGestureProgress::None:
        case EGestureProgress::Lost:
            hand.State.InteractionMode = InteractionMode::None;
            break;

        case EGestureProgress::Tracked:
            UpdateFocus(hand);
            break;

        case EGestureProgress::Started:
        {
            bool handled = false;
            if (hand.State.InteractionMode == InteractionMode::ToolboxFocus)
            {
                handled = TryHandleStartToolboxDrag(hand.State);
            }

            if (!handled)
            {
                handled = TryHandleDragStart(update, hand.State);
            }

            if (!handled)
            {
                handled = TryHandleRotateStart(hand.State);
                if (handled)
                {
                    hand.State.Debounce.ShowRotateHandles.SetCurrent(true);
                }
            }


            break;
        }

        case EGestureProgress::Updated:

            UpdateFocus(hand);
            TryHandleDragStart(update, hand.State);

            // We don't allow rotation if we enter a pipe's focus area already pinched
            hand.State.Debounce.ShowRotateHandles.SetCurrent(false);

            break;
    }
}


bool APPipeGrid::PipeConnected(const GridPipe& pipe, PipeDirections direction)
{
    bool connected = false;

    PipeDirections connections = pipe.pipe->GetPipeDirections();
    if (IsAnyFlagSet(direction, connections))
    {
        // This direction represents an outbound connection, see if there's a neighbor
        FPipeGridCoordinate neighborLocation = pipe.currentLocation + APPipe::PipeDirectionToLocationAdjustment(direction);
        auto itFind = m_pipesGrid.find(neighborLocation);

        if (itFind != m_pipesGrid.end())
        {
            if (itFind->second.pipe && !itFind->second.pipe->GetInToolbox())
            {
                // There's a pipe at that location (and it's not in the toolbox), see if it connects back to us
                if (IsAnyFlagSet(itFind->second.pipe->GetPipeDirections(), APPipe::InvertPipeDirection(direction)))
                {
                    // It does! We are connected
                    connected = true;
                }
            }
        }
    }

    return connected;
}

bool APPipeGrid::PipeConnected(const FPipeGridCoordinate& pipeCoordinate)
{
    bool connected = false;
    auto itTarget = m_pipesGrid.find(pipeCoordinate);

    if (itTarget != m_pipesGrid.end())
    {
        GridPipe& gridPipe = itTarget->second;

        if (gridPipe.pipe && !gridPipe.pipe->GetInToolbox())
        {
            connected = (gridPipe.pipe->GetPipeClass() != DefaultPipeClass);

            if (!connected)
            {
                // Pipes can be connected and still be the default pipe class if no 
                // connected pipe is ultiamtely connected to a start or end      

                for (int i = 0; !connected && i < APPipe::ValidDirectionsCount; i++)
                {
                    PipeDirections currentConnection = APPipe::ValidDirections[i];
                    connected = PipeConnected(gridPipe, currentConnection);
                }
            }
        }
    }

    return connected;
}

bool APPipeGrid::CheckForSolution()
{
    bool solution = ResolveGridState();

    if (solution)
    {
        // The grid is in a solution state (ie, no disconnected pipes, no open pipe connections, 
        // all connected pipes are in the same class, and no pipes left in the toolbox).
        // Announce the solution.
        OnGridSolved.Broadcast();
    }
    
    return solution;
}

bool APPipeGrid::ResolveGridState()
{
    int openConnections = 0;
    int disconnectedPipes = 0;

    // Any pipe we're currently dragging is disconnected by definition
    if (m_hands.Left.State.InteractionMode == InteractionMode::Translate && m_hands.Left.State.Pipe.Actor != nullptr)
    {
        disconnectedPipes++;
    }
    if (m_hands.Right.State.InteractionMode == InteractionMode::Translate && m_hands.Right.State.Pipe.Actor != nullptr)
    {
        disconnectedPipes++;
    }
    
    // Step 1: Setup
    // We go through all GridPipes, adding fixed ones to
    // the open list (with their current PipeClass), and marking all
    // others as the default pipe class
    auto itSetup = m_pipesGrid.begin();
    while (itSetup != m_pipesGrid.end())
    {
        if (itSetup->second.pipe && !itSetup->second.pipe->GetInToolbox() && itSetup->second.pipe->GetPipeType() != EPipeType::Block)
        {
            if (itSetup->second.pipe->GetPipeFixed())
            {
                m_openList.push_back(&(itSetup->second));
            }
            else
            {
                itSetup->second.currentPipeClass = DefaultPipeClass;
            }
        }

        itSetup++;
    }

    // Step 2: Walk
    // As long as the open list isn't empty, pick off the first item. 
    // Iterate that item's outbound connections and:
    //   * If that connection has a connected neighbor:
    //     * If the neighbor is still in the Default class, add it to this piece's class and stick in in the open list
    //     * If the neighbor is already in the same class, ignore it
    //     * If the neighbor is in a different class, treat it as an open connection and increment the open connections count
    //   * If that connection has no connected neighbor, increment the open connections count
    while (!m_openList.empty())
    {
        GridPipe* gridPipe = m_openList.front();
        m_openList.pop_front();

        PipeDirections connections = gridPipe->pipe->GetPipeDirections();
        bool shouldShowLabel = false;

        for (int i = 0; i < APPipe::ValidDirectionsCount; i++)
        {
            PipeDirections currentConnection = APPipe::ValidDirections[i];
            if (IsAnyFlagSet(connections, currentConnection))
            {
                // This direction represents an outbound connection, see if there's a neighbor
                FPipeGridCoordinate neighborLocation = gridPipe->currentLocation + APPipe::PipeDirectionToLocationAdjustment(currentConnection);

                auto itFind = m_pipesGrid.find(neighborLocation);

                if (itFind == m_pipesGrid.end() || !itFind->second.pipe)
                {
                    // There's no pipe at that location, so it's an open connection
                    openConnections++;
                    shouldShowLabel = true;
                }

                else if (!itFind->second.pipe || itFind->second.pipe->GetInToolbox())
                {
                    // The neighbor is null or is in the toolbox, so it's disconnected by definition
                    openConnections++;
                    shouldShowLabel = true;
                }

                // Ignore pipes that are already in our class
                else if (itFind->second.currentPipeClass == gridPipe->currentPipeClass)
                {
                    // The neighbor is in our class. Make sure it has a connection back to us
                    if (!IsAnyFlagSet(itFind->second.pipe->GetPipeDirections(), APPipe::InvertPipeDirection(currentConnection)))
                    {
                        // It doesn't, so this is an open connection
                        openConnections++;
                        shouldShowLabel = true;
                    }
                }
                else
                {
                    // The neighbor isn't in our class
                    if (itFind->second.currentPipeClass != DefaultPipeClass)
                    {
                        // The neighbor is already in a class, so it counts as an open connection
                        openConnections++;
                        shouldShowLabel = true;
                    }
                    else
                    {
                        // The neighbor pipe can be added to our class. Make sure it has a connection back to us
                        if (!IsAnyFlagSet(itFind->second.pipe->GetPipeDirections(), APPipe::InvertPipeDirection(currentConnection)))
                        {
                            // It doesn't, so this is an open connection
                            openConnections++;
                            shouldShowLabel = true;
                        }
                        else
                        {
                            // It does! Add it to our class and add it to the open list
                            itFind->second.currentPipeClass = gridPipe->currentPipeClass;
                            m_openList.push_back(&itFind->second);
                        }
                    }
                }
            }
        }

        gridPipe->pipe->SetShowLabel(shouldShowLabel && m_labelsEnabled);
    }

    // Step 3: Cleanup
    // Count how many pipes are disconnected from starts/ends, and update 
    // the class of all pipes that have changed during resolution
    auto itCleanup = m_pipesGrid.begin();
    while (itCleanup != m_pipesGrid.end())
    {
        if (itCleanup->second.pipe)
        {
            if (itCleanup->second.currentPipeClass == DefaultPipeClass)
            {
                itCleanup->second.pipe->SetShowLabel(false);

                if (!itCleanup->second.pipe->GetInToolbox() && itCleanup->second.pipe->GetPipeType() != EPipeType::Block)
                {
                    disconnectedPipes++;
                }
            }

            if (itCleanup->second.currentPipeClass != itCleanup->second.pipe->GetPipeClass())
            {
                itCleanup->second.pipe->SetPipeClass(itCleanup->second.currentPipeClass);
            }
        }

        itCleanup++;
    }

    bool solution = (openConnections == 0 && disconnectedPipes == 0);

    // Step 4:
    // Reutrn whether we're in a solution state; that means all pipe connections are connected,
    // all placed pipes are in a non-default class, and all pipe pieces are in the same class as their 
    // connected neighbors
    return solution;
}

void APPipeGrid::SetLabelsEnabled(bool enabled)
{
    if (m_labelsEnabled != enabled)
    {
        m_labelsEnabled = enabled;
        ResolveGridState();
    }
}

bool APPipeGrid::GetLabelsEnabled()
{
    return m_labelsEnabled;
}


bool APPipeGrid::ToggleDebugFocus()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    m_debugFocus = !m_debugFocus;
    return true;
#endif
}