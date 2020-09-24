
#include "PPipeGrid_Internal.h"

static const EPipeType ValidToolboxTypes[] = { EPipeType::Start, EPipeType::End, EPipeType::Straight, EPipeType::Corner, EPipeType::Junction };
static const EPipeType ActionableToolboxTypes[] = { EPipeType::Straight, EPipeType::Corner, EPipeType::Junction };

bool APPipeGrid::IsToolboxDragging()
{
    return m_hands.Left.State.InteractionMode == InteractionMode::ToolboxTranslate ||
        m_hands.Right.State.InteractionMode == InteractionMode::ToolboxTranslate;
}

bool APPipeGrid::IsToolboxFocused()
{
    return IsToolboxDragging() ||
        m_hands.Left.State.InteractionMode == InteractionMode::ToolboxFocus ||
        m_hands.Right.State.InteractionMode == InteractionMode::ToolboxFocus;
}

bool APPipeGrid::ToolboxAvailable()
{
    return Toolbox && m_toolboxEnabled && !IsToolboxDragging();
}

bool APPipeGrid::IsToolboxEmpty()
{
    bool empty = true;

    if (Toolbox && m_toolboxEnabled)
    {
        FToolboxContents contents = Toolbox->GetContents();
        empty = ((contents.CornerPipes + contents.JunctionPipes + contents.StraightPipes) == 0);
    }

    return empty;
}


bool APPipeGrid::GetToolboxCoordinate(FPipeGridCoordinate* pCoord)
{
    (*pCoord) = FPipeGridCoordinate::Zero;
    bool valid = false;

    if (m_toolboxEnabled && Toolbox && !IsToolboxDragging())
    {
        (*pCoord) = GridLocationToGridCoordinate(WorldLocationToGridLocation(Toolbox->GetActorLocation()));
        valid = true;
    }

    return valid;
}

bool APPipeGrid::TrySetToolboxCoordinate(const FPipeGridCoordinate& coord)
{
    bool success = m_toolboxEnabled && Toolbox && DoesToolboxFitAt(coord);
    if (success)
    {
        LiftToolbox();
        DropToolbox(coord);
    }

    return success;
}

void APPipeGrid::LiftToolbox()
{
    for (auto type : ActionableToolboxTypes)
    {
        // Make sure all of our pipes have been created
        SpawnInToolbox(type);
    }

    for (auto type : ValidToolboxTypes)
    {
        FPipeGridCoordinate victim;
        if (ToolboxCoordinateForType(type, &victim))
        {
            auto it = m_pipesGrid.find(victim);

            if (it != m_pipesGrid.end())
            {
                if (it->second.pipe)
                {
                    it->second.pipe->AttachToComponent(Toolbox->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
                    it->second.pipe = nullptr;
                }

                m_pipesGrid.erase(it);
            }
        }
    }
}

bool APPipeGrid::TryHandleStartToolboxDrag(GridHandState& handState)
{
    FPipeGridCoordinate top;

    if (!IsToolboxDragging() && ToolboxCoordinateForType(EPipeType::Start, &top) && top == handState.Pipe.StartCoordinate)
    {
        handState.Toolbox.StartCoordinate = GridLocationToGridCoordinate(WorldLocationToGridLocation(Toolbox->GetActorLocation()));

        // We always assume the current position of the toolbox is safe (since nothing should currently block it there).
        handState.Toolbox.LastSafeCoordinate = handState.Toolbox.StartCoordinate;
        Toolbox->SetDropTarget(Toolbox->GetActorLocation());

        LiftToolbox();
        
        handState.InteractionMode = InteractionMode::ToolboxTranslate;
        return true;
    }

    return false;
}

void APPipeGrid::HandleToolboxGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState)
{
    switch (update.Progress)
    {
        case EGestureProgress::None:
        case EGestureProgress::Tracked:
        case EGestureProgress::Lost:
            HandleToolboxDragLost(handState);
            break;

        default:

        case EGestureProgress::Started:
        case EGestureProgress::Updated:
        {   
            FVector newLocation = handState.Target.Location;
            newLocation.Z -= (2 * CellSize);
            Toolbox->SetActorRelativeLocation(newLocation);

            FPipeGridCoordinate newCoord = GridLocationToGridCoordinate(newLocation);
            if (!DoesToolboxFitAt(newCoord))
            {
                if (DoesToolboxFitAt(handState.Toolbox.LastSafeCoordinate))
                {
                    newCoord = handState.Toolbox.LastSafeCoordinate;
                }
                else
                {
                    // We don't fit at our new coordinate, and we no longer fit at our last safe coordinate.
                    // Revert to our start coordinate. (If our start coordinate is no longer safe, we deal 
                    // with that differently at drop time)
                    newCoord = handState.Toolbox.StartCoordinate;
                }
            }

            if (newCoord != handState.Toolbox.LastSafeCoordinate)
            {
                handState.Toolbox.LastSafeCoordinate = newCoord;
                Toolbox->SetDropTarget(GridLocationToWorldLocation(GridCoordinateToGridLocation(newCoord)));
            }

            break;
        }
    }
}

void APPipeGrid::DropToolbox(FPipeGridCoordinate root)
{
    const FVector newLocation = GridCoordinateToGridLocation(root);
    Toolbox->SetActorRelativeLocation(newLocation);

    TArray<AActor*> children;
    Toolbox->GetAttachedActors(children);

    for (auto child : children)
    {
        if (child)
        {
            APPipe* childPipe = Cast<APPipe>(child);
            if (childPipe)
            {
                FVector gridLocation = WorldLocationToGridLocation(childPipe->GetActorLocation());

                GridPipe gridPipe;
                gridPipe.pipe = childPipe;
                gridPipe.currentLocation = GridLocationToGridCoordinate(gridLocation);
                gridPipe.currentPipeClass = childPipe->GetPipeClass();
                m_pipesGrid[gridPipe.currentLocation] = gridPipe;

                childPipe->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

                childPipe->SetActorRelativeLocation(gridLocation);
            }
        }
    }

    InitializeToolboxPipes();
}

void APPipeGrid::HandleToolboxDragLost(GridHandState& handState)
{
    if (handState.InteractionMode == InteractionMode::ToolboxTranslate)
    {
        handState.InteractionMode = InteractionMode::None;

        FPipeGridCoordinate dropCoordinate = handState.Toolbox.LastSafeCoordinate;
        if (!DoesToolboxFitAt(dropCoordinate))
        {
            dropCoordinate = handState.Toolbox.StartCoordinate;
        }

        // Return any pipes at the drop location back to the toolbox. We should
        // only be dropping someplace with pipes if its our start coordinate,
        // which means that it should be safe to put any pipes that were dropped
        // there while we were dragging back into the toolbox.
        for (auto type : ValidToolboxTypes)
        {
            FPipeGridCoordinate victim;
            if (ToolboxCoordinateForType(type, dropCoordinate, &victim))
            {
                auto it = m_pipesGrid.find(victim);

                if (it != m_pipesGrid.end())
                {
                    if (it->second.pipe)
                    {
                        EPipeType victimType = it->second.pipe->GetPipeType();
                        switch (victimType)
                        {
                            case EPipeType::Straight:
                            case EPipeType::Corner:
                            case EPipeType::Junction:
                                Toolbox->AddPipe(victimType);
                                break;

                            default:
                                UE_LOG(HoloPipesLog, Error, L"APPipeGrid::HandleToolboxDragLost - Unexpected victim pipe type (%d) while dropping", (int)victimType);
                                break;
                        }

                        it->second.pipe->Destroy();
                        it->second.pipe = nullptr;
                    }

                    m_pipesGrid.erase(it);
                }
            }
        }

        DropToolbox(dropCoordinate);

        if (dropCoordinate != handState.Toolbox.StartCoordinate)
        {
            OnToolboxMoved.Broadcast();
        }
    }
}

void APPipeGrid::EnsureToolboxPipes()
{
    SpawnInToolbox(EPipeType::Straight);
    SpawnInToolbox(EPipeType::Corner);
    SpawnInToolbox(EPipeType::Junction);
}

void APPipeGrid::SpawnInToolbox(EPipeType toolboxPipe)
{
    if (Toolbox)
    {
        bool fixed = (Toolbox->CountRemaining(toolboxPipe) == 0);
        bool found = false;

        if (IsToolboxDragging())
        {
            // If we're dragging, all the pipes are attached to the toolbox
            TArray<AActor*> children;
            Toolbox->GetAttachedActors(children);

            for (auto child : children)
            {
                if (!found && child)
                {
                    APPipe* childPipe = Cast<APPipe>(child);
                    if (childPipe)
                    {
                        if (childPipe->GetPipeType() == toolboxPipe)
                        {
                            childPipe->SetPipeFixed(fixed);
                            found = true;
                        }
                    }
                }
            }

            if (!found)
            {
                UE_LOG(HoloPipesLog, Error, L"APPipeGrid::SpawnInToolbox - While dragging, failed to find an attached %d", (int)toolboxPipe);
            }
        }
        else
        {
            // We're not dragging, see if the pipe already exists in the grid
            FPipeGridCoordinate existingPipeCoordinate;
            if (ToolboxCoordinateForType(toolboxPipe, &existingPipeCoordinate))
            {
                auto it = m_pipesGrid.find(existingPipeCoordinate);
                if (it != m_pipesGrid.end() && it->second.pipe)
                {
                    it->second.pipe->SetPipeFixed(fixed);
                    found = true;
                }
            }

            if (!found)
            {
                PipeSegmentGenerated newToolboxPipe = {};
                newToolboxPipe.PipeClass = DefaultPipeClass;
                newToolboxPipe.Type = toolboxPipe;
                ToolboxCoordinateForType(toolboxPipe, &newToolboxPipe.Location);

                switch (toolboxPipe)
                {
                    case EPipeType::Straight:
                        newToolboxPipe.Connections = PipeDirections::Left | PipeDirections::Right;
                        break;

                    case EPipeType::Corner:
                        newToolboxPipe.Connections = PipeDirections::Top | PipeDirections::Right;
                        break;

                    case EPipeType::Junction:
                        newToolboxPipe.Connections = PipeDirections::Left | PipeDirections::Right | PipeDirections::Top;
                        break;
                }

                AddPipeOptions options = AddPipeOptions::InToolbox;
                if (fixed)
                {
                    options |= AddPipeOptions::Fixed;
                }

                AddPipe(newToolboxPipe, options);
            }

        }
    }
}

bool APPipeGrid::DoesToolboxFitAt(FPipeGridCoordinate root)
{
    for (auto type : ValidToolboxTypes)
    {
        bool typeFits = false;

        FPipeGridCoordinate coord;
        if (ToolboxCoordinateForType(type, root, &coord))
        {
            auto it = m_pipesGrid.find(coord);
            typeFits = ((it == m_pipesGrid.end()) || (it->second.pipe == nullptr) || (it->second.pipe->GetInToolbox()));
        }
        
        if (!typeFits)
        {
            return false;
        }
    }

    return true;
}

bool APPipeGrid::ToolboxCoordinateForType(EPipeType type, FPipeGridCoordinate* pCoord)
{
    if (m_toolboxEnabled && Toolbox && !IsToolboxDragging())
    {
        return ToolboxCoordinateForType(
            type,
            GridLocationToGridCoordinate(WorldLocationToGridLocation(Toolbox->GetActorLocation())),
            pCoord);
    }

    return false;
}

bool APPipeGrid::ToolboxCoordinateForType(EPipeType type, FPipeGridCoordinate root, FPipeGridCoordinate* pCoord)
{
    (*pCoord) = root;
    bool valid = true;

    switch (type)
    {
        case EPipeType::Start:
            // Start is used to represent the top section of the toolbox
            pCoord->Z += 2;
            break;

        case EPipeType::Straight:
            pCoord->Z += 1;
            break;

        case EPipeType::Corner:
            //pCoord.Z = location.Z;
            break;

        case EPipeType::Junction:
            pCoord->Z -= 1;
            break;

        case EPipeType::End:
            // End is used to represent the bottom section of the toolbox
            pCoord->Z -= 2;
            break;

        default:
            valid = false;
            break;
    }

    return valid;
}

bool APPipeGrid::CoordinateInToolbox(const FPipeGridCoordinate& coordinate)
{
    for (EPipeType type : ValidToolboxTypes)
    {
        FPipeGridCoordinate toolboxCoordinate;
        if (ToolboxCoordinateForType(type, &toolboxCoordinate) && toolboxCoordinate == coordinate)
        {
            return true;
        }
    }

    return false;
}

void APPipeGrid::InitializeToolbox(const std::vector<PipeSegmentGenerated>& pipesInToolbox, int playableGridSize)
{
    if (m_toolboxEnabled)
    {
        if (!Toolbox && ToolboxClass)
        {
            Toolbox = GetWorld()->SpawnActor<APToolbox>(ToolboxClass);

            if (Toolbox)
            {
                Toolbox->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
            }
        }

        if (Toolbox)
        {
            Toolbox->Clear();
            Toolbox->Initialize(pipesInToolbox);

            Toolbox->SetActorRelativeLocation(GridCoordinateToGridLocation(GetDefaultToolboxCoordinate(playableGridSize)));
            Toolbox->SetActorRelativeScale3D({ 1,1,1 });

            InitializeToolboxPipes();
        }
    }
}

FPipeGridCoordinate APPipeGrid::GetDefaultToolboxCoordinate(int playableGridSize)
{
    int gridSide = playableGridSize + 2;
    int gridMin = -(gridSide / 2);
    int gridMax = gridMin + gridSide;
    return { gridMin + 1, gridMax + 1, -1 };
}

void APPipeGrid::AddToToolbox(const std::vector<PipeSegmentGenerated>& pipesInToolbox)
{
    if (m_toolboxEnabled && Toolbox)
    {
        for (const auto& pipe : pipesInToolbox)
        {
            Toolbox->AddPipe(pipe.Type);
            if (Toolbox->CountRemaining(pipe.Type) == 1)
            {
                // We just went from 0 to 1, so respawn the pipe
                SpawnInToolbox(pipe.Type);
            }
        }
    }
}

void APPipeGrid::RemoveToolbox()
{
    if (Toolbox)
    {
        Toolbox->Destroy();
        Toolbox = nullptr;
    }
}

void APPipeGrid::InitializeToolboxPipes()
{
    EnsureToolboxPipes();

    GridPipe reservedAboveToolbox = {};
    reservedAboveToolbox.pipe = nullptr;
    reservedAboveToolbox.currentPipeClass = DefaultPipeClass;

    GridPipe reservedBelowToolbox = reservedAboveToolbox;

    ToolboxCoordinateForType(EPipeType::Start, &reservedAboveToolbox.currentLocation);
    m_pipesGrid[reservedAboveToolbox.currentLocation] = reservedAboveToolbox;

    ToolboxCoordinateForType(EPipeType::End, &reservedBelowToolbox.currentLocation);
    m_pipesGrid[reservedBelowToolbox.currentLocation] = reservedBelowToolbox;
}

void APPipeGrid::ReturnDisconnectedToToolbox()
{
    if (Toolbox)
    {
        std::vector<FPipeGridCoordinate> victims;

        auto it = m_pipesGrid.begin();
        while (it != m_pipesGrid.end())
        {
            if (it->second.pipe && 
                !it->second.pipe->GetPipeFixed() &&
                !it->second.pipe->GetInToolbox() && 
                it->second.pipe->GetPipeClass() == DefaultPipeClass)
            {
                victims.push_back(it->first);

                EPipeType type = it->second.pipe->GetPipeType();
                Toolbox->AddPipe(type);

                if (Toolbox->CountRemaining(type) == 1)
                {
                    // We just went from 0 to 1, so respawn the pipe
                    SpawnInToolbox(type);
                }
            }

            it++;
        }

        // Now destroy all placed victims
        auto itVictim = victims.begin();
        while (itVictim != victims.end())
        {
            auto itVictimInGrid = m_pipesGrid.find(*itVictim);
            itVictimInGrid->second.pipe->Destroy();
            m_pipesGrid.erase(itVictimInGrid);

            itVictim++;
        }
    }

    // Since we've sweeped disconnected pipes into the toolbox, it's theoretically possible
    // we could have landed in a solution state
    CheckForSolution();
}
