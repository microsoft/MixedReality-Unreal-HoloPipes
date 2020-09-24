// Fill out your copyright notice in the Description page of Project Settings.


#include "LevelGenerator.h"
#include "PipeRotation.h"
#include <safeint.h>

using namespace msl::utilities;

const LevelGenerator::PipeSegmentTemp LevelGenerator::PipeSegmentTemp::c_Empty = {};

LevelGenerator::LevelGenerator()
{
	SetStatus(GeneratorStatus::Idle);
}

LevelGenerator::~LevelGenerator()
{
	Reset(true /*resetThread*/, false /*resetVirtualAndRealizedLists*/);
}

void LevelGenerator::CancelLevel()
{
	Reset(true /*resetThread*/, true /*resetVirtualAndRealizedLists*/);
}

void LevelGenerator::Reset(bool resetThread, bool resetVirtualAndRealizedLists)
{
	if (resetThread)
	{
		if (m_thread)
		{
			m_abortExecution = true;
			SetStatus(GeneratorStatus::Canceling);

			m_thread->WaitForCompletion();
			delete m_thread;
			m_thread = nullptr;
		}

		m_abortExecution = false;
		SetStatus(GeneratorStatus::Idle);
	}

	if (resetVirtualAndRealizedLists)
	{
		RealizedPipes.clear();
		VirtualPipes.clear();
	}

    m_pipesToBuild.clear();
	m_pipeGrid.clear();

	m_rng.Init(0);

    m_sideMin = 0;
    m_sideMax = 0;
    m_playSpaceSize = 0;
	m_gridSide = 0;
	m_gridSideSquared = 0;
	m_gridSideCubed = 0;
    m_straightCost = 0;
    m_cornerCost = 0;
    m_startCandidates.clear();
    m_endCandidates.clear();
}

bool LevelGenerator::GenerateLevel(const FGenerateOptions& options)
{
	Reset(true /*resetThread*/, true /*resetVirtualAndRealizedLists*/);

    bool success = true;

    if (options.PlaySpaceSize < 3)
    {
        UE_LOG(HoloPipesLog, Error, L"LevelGenerator - PlaySpace size must be 3 or greater, %d was specified", options.PlaySpaceSize);
        success = false;
    }

    if (success && options.MaxNumPipes < 1 || options.MaxNumPipes >= PipeClassCount)
    {
        UE_LOG(HoloPipesLog, Error, L"LevelGenerator - NumPipes must be between 1 and %d (inclusive), %d was specified", (PipeClassCount - 1), options.MaxNumPipes);
        success = false;
    }

    if (success && (options.CornerCost <= 0 || options.StraightCost <= 0))
    {
        UE_LOG(HoloPipesLog, Error, L"LevelGenerator - traversal cost of straight pieces (%f) and corners (%f) must be greater than 0", options.StraightCost, options.CornerCost);
        success = false;
    }

    m_playSpaceSize = options.PlaySpaceSize;
    m_straightCost = options.StraightCost;
    m_cornerCost = options.CornerCost;

    // Starts and ends are generated outside the playspace, so a grid side is actually two longer than
    // the specified option
    m_gridSide = m_playSpaceSize + 2;

    // The back of the place space is in front of the user, stretching forward. The user is centered left/right, and top/bottom
    m_sideMin = -m_gridSide / 2;
    m_sideMax = m_sideMin + m_gridSide - 1;

    if (success)
    {
        if (!SafeMultiply(m_gridSide, m_gridSide, m_gridSideSquared) ||
            !SafeMultiply(m_gridSideSquared, m_gridSide, m_gridSideCubed))
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator - Numeric overflow, unable to compute required memory for PlaySpaceSize %d", options.PlaySpaceSize);
            success = false;
        }
    }

    if (success)
    {
        m_pipeGrid.resize(m_gridSideCubed, PipeSegmentTemp::c_Empty);

        if (m_pipeGrid.size() >= m_gridSideCubed)
        {
            for (int x = m_sideMin; x <= m_sideMax; x++)
            {
                for (int y = m_sideMin; y <= m_sideMax; y++)
                {
                    for (int z = m_sideMin; z <= m_sideMax; z++)
                    {
                        GetSegment(x, y, z).Location = { x, y, z };
                    }
                }
            }
        }
        else
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator - Unable to allocate segment grid (%d elements)", m_gridSideCubed);
            success = false;
        }
    }

    if (success)
    {
        m_startCandidates.reserve(m_gridSideSquared);
        if (m_startCandidates.capacity() < m_gridSideSquared)
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator - Unable to allocate end candidates list (%d elements)", m_gridSideSquared);
            success = false;
        }
    }

    if (success)
    {
        int maxEndCandidates = m_gridSideCubed * 3;
        m_endCandidates.reserve(maxEndCandidates);

        if (m_endCandidates.capacity() < maxEndCandidates)
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator - Unable to allocate end candidates list (%d elements)", maxEndCandidates);
            success = false;
        }
    }

    if (success)
    {
        m_rng.Init(options.Level);

        // Shuffle the available pipe classes so we get a random selection of pipe styling 
        // each level and the class reveals nothing about the order of pipe generation
        // We use one more than the number of requested pipes (up to the maximum classes)
        // to add a little variety
        const int numSourceClasses = std::min(options.MaxNumPipes + 1, PipeClassCount - 1);

        std::vector<int> pipeClasses;
        pipeClasses.reserve(numSourceClasses);
        for (int i = 1; i <= numSourceClasses; i++)
        {
            pipeClasses.push_back(i);
        }

        m_rng.Shuffle(pipeClasses);

        m_pipesToBuild.resize(options.MaxNumPipes);

        if (m_pipesToBuild.size() == options.MaxNumPipes)
        {
            for (int i = 0; i < options.MaxNumPipes; i++)
            {
                auto& pipe = m_pipesToBuild[i];

                pipe.Class = pipeClasses.back();
                pipeClasses.pop_back();

                pipe.Fixed = 0;
                pipe.Junctions = 0;
            }

            for (int i = 0; i < options.MaxJunctions; i++)
            {
                m_pipesToBuild[m_rng.GetInt(0, options.MaxNumPipes)].Junctions++;
            }

            for (int i = 0; i < options.MaxFixed; i++)
            {
                m_pipesToBuild[m_rng.GetInt(0, options.MaxNumPipes)].Fixed++;
            }
        }
        else
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator - Unable to allocate pipe list (%d elements)", options.MaxNumPipes);
            success = false;
        }
    }

    if (success)
    {
        GenerateBlocks(options.MaxBlocks);
    }

    if (success)
    {
        m_thread = FRunnableThread::Create(this, L"GenerateThread");
        success = (m_thread != nullptr);
    }

    return success;
}


uint32 LevelGenerator::Run()
{
    bool success = false;

	if (!m_abortExecution)
	{
        int generatedPipes = 0;
        for (auto& pipe : m_pipesToBuild)
        {
            if (GeneratePipe(pipe))
            {
                generatedPipes++;
            }
        }

		success = generatedPipes > 0;
	}

	if (success && !m_abortExecution)
	{
		success = FinalizeLevel();
	}

	if (!m_abortExecution)
	{
		Reset(false /* resetThread */, !success /*resetVirtualAndRealizedLists*/);
		SetStatus(success ? GeneratorStatus::Complete : GeneratorStatus::Failed);
	}

	return 0;
}

LevelGenerator::PipeSegmentTemp& LevelGenerator::GetSegment(const FPipeGridCoordinate& location)
{
	return GetSegment(location.X, location.Y, location.Z);
}

LevelGenerator::PipeSegmentTemp& LevelGenerator::GetSegment(int x, int y, int z)
{
	int index = 
		((z - m_sideMin) * m_gridSideSquared) +
		((x - m_sideMin) * m_gridSide) +
		(y - m_sideMin);
	
	if (index >= 0 && index < m_gridSideCubed)
	{
		return m_pipeGrid[index];
	}
	else
	{
		UE_LOG(HoloPipesLog, Warning, L"LevelGenerator - Invalid segment location specified { %d, %d, %d }", x, y, z);
		static PipeSegmentTemp invalidSegment;
		invalidSegment = PipeSegmentTemp::c_Empty;
		return invalidSegment;
	}
}

bool PipeSegmentCompare(const PipeSegmentGenerated& lhs, const PipeSegmentGenerated& rhs)
{
    return ((int)lhs.Type < (int)rhs.Type);
}

bool LevelGenerator::FinalizeLevel()
{
    ResetAStar();

    size_t noneCount = 0;

    for (const auto& segment : m_pipeGrid)
    {
        if (segment.Type == EPipeType::None)
        {
            noneCount++;
        }
        else if (segment.Fixed)
        {
            RealizedPipes.push_back(segment);
        }
        else
        {
            VirtualPipes.push_back(segment);
        }
    }

    if (m_pipeGrid.size() != (noneCount + RealizedPipes.size() + VirtualPipes.size()))
    {
        UE_LOG(HoloPipesLog, Warning, L"LevelGenerator - Unable to build VirtualPipes and RealizedPipes list");
        return false;
    }

    std::sort(VirtualPipes.begin(), VirtualPipes.end(), PipeSegmentCompare);

    return true;
}

bool LevelGenerator::GenerateBlocks(int count)
{
    for (int i = 0; i < count; i += 2)
    {
        // We don't require the GenerateBlocks is even, but we do
        // our best to generate the blocks in pairs when it is
        bool blockPair = (i + 1) < count;

        bool placed = false;

        while (!placed)
        {
            FPipeGridCoordinate offset =
            {
                static_cast<int>(1 + m_rng.GetInt(0, static_cast<UINT32>(m_playSpaceSize))),
                static_cast<int>(1 + m_rng.GetInt(0, static_cast<UINT32>(m_playSpaceSize))),
                static_cast<int>(1 + m_rng.GetInt(0, static_cast<UINT32>(m_playSpaceSize)))
            };

            FPipeGridCoordinate positionA = { m_sideMin + offset.X, m_sideMin + offset.Y, m_sideMin + offset.Z };
            auto& segmentA = GetSegment(positionA);

            bool place = false;

            if (segmentA.Type == EPipeType::None)
            {
                if (!blockPair)
                {
                    place = true;
                }
                else
                {
                    FPipeGridCoordinate positionB = { m_sideMax - offset.X, m_sideMax - offset.Y, m_sideMax - offset.Z };

                    place = (positionA != positionB);

                    if (positionA != positionB)
                    {
                        auto& segmentB = GetSegment(positionB);

                        if (segmentB.Type == EPipeType::None)
                        {
                            place = true;
                            segmentB.Type = EPipeType::Block;
                            segmentB.Fixed = true;
                            segmentB.State = BuildState::Committed;
                        }
                    }
                }
            }

            if (place)
            {
                segmentA.Type = EPipeType::Block;
                segmentA.Fixed = true;
                segmentA.State = BuildState::Committed;

                placed = true;
            }
        }
    }

    return true;
}

PipeDirections LevelGenerator::SideFromCoordinate(const FPipeGridCoordinate& coordinate)
{
    if (coordinate.X == m_sideMin)
    {
        return PipeDirections::Back;
    }
    else if (coordinate.X == m_sideMax)
    {
        return PipeDirections::Front;
    }
    else if (coordinate.Y == m_sideMin)
    {
        return PipeDirections::Left;
    }
    else if (coordinate.Y == m_sideMax)
    {
        return PipeDirections::Right;
    }
    else if (coordinate.Z == m_sideMin)
    {
        return PipeDirections::Bottom;
    }
    else if (coordinate.Z == m_sideMax)
    {
        return PipeDirections::Top;
    }
    else
    {
        return PipeDirections::None;
    }
}

void LevelGenerator::BuildStartCandidateList(PipeDirections side)
{
    m_startCandidates.clear();

    const int fullMin = m_sideMin;
    const int fullMax = m_sideMax;
    const int searchMin = m_sideMin + 1;
    const int searchMax = m_sideMax - 1;

    FPipeGridCoordinate minSearch = { searchMin, searchMin, searchMin };
    FPipeGridCoordinate maxSearch = { searchMax, searchMax, searchMax };

    switch (side)
    {
        case PipeDirections::Front:
            minSearch.X = maxSearch.X = fullMax;
            break;

        case PipeDirections::Left:
            minSearch.Y = maxSearch.Y = fullMin;
            break;

        case PipeDirections::Right:
            minSearch.Y = maxSearch.Y = fullMax;
            break;

        case PipeDirections::Top:
            minSearch.Z = maxSearch.Z = fullMax;
            break;

        case PipeDirections::Bottom:
            minSearch.Z = maxSearch.Z = fullMin;
            break;

        // case PipeDirections::Back: // We don't allow starts on the back side
        // case PipeDirections::None: // We don't allow starts without specifying a side
        default:
            // We don't support Back or None
            return;
    }

    for (int x = minSearch.X; x <= maxSearch.X; x++)
    {
        for (int y = minSearch.Y; y <= maxSearch.Y; y++)
        {
            for (int z = minSearch.Z; z <= maxSearch.Z; z++)
            {
                m_startCandidates.push_back({ x,y,z });
            }
        }
    }

    m_rng.Shuffle(m_startCandidates);
}

void LevelGenerator::BuildEndCandidateList(PipeDirections half)
{
    m_endCandidates.clear();

    const int fullMin = m_sideMin;
    const int fullMax = m_sideMax;
    const int minOfUpperHalf = (m_gridSide % 2);
    const int maxOfLowerHalf = -1;

    // We don't allow ends on the back side, so add one to the X min
    FPipeGridCoordinate minSearch = { fullMin + 1, fullMin, fullMin };
    FPipeGridCoordinate maxSearch = { fullMax, fullMax, fullMax };

    switch (half)
    {
        case PipeDirections::Front:
            minSearch.X = minOfUpperHalf;
            break;

        case PipeDirections::Back:
            maxSearch.X = maxOfLowerHalf;
            break;

        case PipeDirections::Left:
            maxSearch.Y = maxOfLowerHalf;
            break;

        case PipeDirections::Right:
            minSearch.Y = minOfUpperHalf;
            break;

        case PipeDirections::Top:
            minSearch.Z = minOfUpperHalf;
            break;

        case PipeDirections::Bottom:
            maxSearch.Z = maxOfLowerHalf;
            break;

        // case PipeDirections::None: // We don't allow ends without specifying a half
        default:
            return;
    }

    for (int x = minSearch.X; x <= maxSearch.X; x++)
    {
        for (int y = minSearch.Y; y <= maxSearch.Y; y++)
        {
            for (int z = minSearch.Z; z <= maxSearch.Z; z++)
            {
                int edgeCount =
                    ((x == m_sideMin || x == m_sideMax) ? 1 : 0) +
                    ((y == m_sideMin || y == m_sideMax) ? 1 : 0) +
                    ((z == m_sideMin || z == m_sideMax) ? 1 : 0);

                if (edgeCount == 1)
                {
                    m_endCandidates.push_back({ x, y, z });
                }

            }
        }
    }

    m_rng.Shuffle(m_endCandidates);
}

// Levels are randomly generated based on hueristics. We can't be completely random since that would 
// lead to overlapping pipe segments (which isn't valid). Instead, we pick a set of preferred "starting"
// values at random and then systemtically explore the space until we find a set of values that produce
// a valid pipe
bool LevelGenerator::GeneratePipe(const PipeTemp& pipe)
{
    // Pick the first side we'll explore for a pipe start
    int startSide = m_rng.GetInt(0, APPipe::ValidDirectionsCount);

    // Pick the starting coordinate in the play space of that chosen side
    int startA = m_rng.GetInt(0, m_playSpaceSize);
    int startB = m_rng.GetInt(0, m_playSpaceSize);

    for (int sideSearch = 0; !m_abortExecution && sideSearch < APPipe::ValidDirectionsCount; sideSearch++)
    {
        PipeDirections sideDirection = APPipe::ValidDirections[(sideSearch + startSide) % APPipe::ValidDirectionsCount];

        // We don't allow starts and ends on the back side, because they directly block the player
        if (sideDirection != PipeDirections::Back)
        {
            BuildStartCandidateList(sideDirection);

            PipeDirections pipeDirection = APPipe::InvertPipeDirection(sideDirection);

            for (const auto& startCandidateLocation : m_startCandidates)
            {
                PipeSegmentTemp& startCandidate = GetSegment(startCandidateLocation);

                if (startCandidate.Type == EPipeType::None)
                {
                    if (GeneratePipe(pipe, startCandidateLocation, pipeDirection))
                    {
                        // The chosen start worked, so we're done. 
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

int LevelGenerator::ComputePredictedCost(const FPipeGridCoordinate& from, const FPipeGridCoordinate& to, PipeDirections parentToChild, PipeDirections endSide)
{
    const int xdiff = abs(from.X - to.X);
    const int ydiff = abs(from.Y - to.Y);
    const int zdiff = abs(from.Z - to.Z);
    const int differences = (xdiff == 0 ? 0 : 1) + (ydiff == 0 ? 0 : 1) + (zdiff == 0 ? 0 : 1);

    // If a corner will be required, add the difference between a straight piece and a corner piece
    const int cornerModifier = (differences > 1 || parentToChild != endSide) ? (m_cornerCost - m_straightCost) : 0;

    return  ((xdiff + ydiff + zdiff) * m_straightCost) + cornerModifier;
}

bool LevelGenerator::GeneratePipe(const PipeTemp& pipe, const FPipeGridCoordinate& startCoordinate, PipeDirections startDirection)
{
    BuildEndCandidateList(startDirection);

    bool builtPipe = false;

    FPipeGridCoordinate firstOnPathCoordinate = (startCoordinate + APPipe::PipeDirectionToLocationAdjustment(startDirection));
    auto& firstOnPathSegment = GetSegment(firstOnPathCoordinate);

    // A previously committed pipe can't be overwritten
    bool success = (firstOnPathSegment.State != BuildState::Committed);

    while (success && !builtPipe && m_endCandidates.size() > 0)
    {
        FPipeGridCoordinate endCoordinate = m_endCandidates.back();
        m_endCandidates.pop_back();
                
        // We want to avoid fully straight pipes, so make certain that at least two
        // coordinate components are different between start and end
        int differenceCount =
            ((startCoordinate.X == endCoordinate.X) ? 0 : 1) +
            ((startCoordinate.Y == endCoordinate.Y) ? 0 : 1) +
            ((startCoordinate.Z == endCoordinate.Z) ? 0 : 1);

        if (differenceCount > 1)
        {
            auto& endSegment = GetSegment(endCoordinate);

            // Make sure the end isn't already in use
            if (endSegment.State == BuildState::None)
            {
                ResetAStar();

                auto& startSegment = GetSegment(startCoordinate);
                startSegment.Type = EPipeType::Start;
                startSegment.PipeClass = pipe.Class;
                startSegment.Connections = startDirection;
                startSegment.Fixed = true;
                startSegment.PathCost = 0;
                startSegment.PredictedCost = ComputePredictedCost(startCoordinate, endCoordinate, startDirection, SideFromCoordinate(endCoordinate));
                startSegment.State = BuildState::OpenList;

                m_openList.insert(&startSegment);

                if (CompletePipe(pipe, endCoordinate, false))
                {
                    builtPipe = true;
                    success = CommitPipe(pipe, endCoordinate);
                }
            }
        }
    }

    success = (success && builtPipe);

    if (success)
    {
        GenerateJunctions(pipe);
        GenerateFixed(pipe);
    }

    return success;
}

bool LevelGenerator::CompletePipe(const PipeTemp& pipe, const FPipeGridCoordinate& endCoordinate, bool forJunction)
{
    // Implementation of A*. 
    // Assumption: We get called with the open list prepopulated with our start state
    // Each iteration, if we haven't found a path to end yet, we pick off a node from the
    // open list which has the shortest traversed path and remaining (manhattan distance) 
    // path. We add that node to the closed list, and add all its neighbors to the open list.
    // Rinse and repeat until we've found a path or determine there isn't one

    PipeDirections endDirection = SideFromCoordinate(endCoordinate);

    while (m_openList.size() > 0)
    {
        PipeSegmentTemp* selected = RemoveRandomLeastFromOpen();
        if (selected == nullptr)
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CompletePipe - No node pulled off a non-empty open list");
            return false;
        }

        EPipeType validNeighborFilter = EPipeType::None;
        bool consider = true;

        if (selected->State == BuildState::OpenList)
        {
            selected->State = BuildState::ClosedList;
            m_closedList.push_back(selected);

            if (selected->Location == endCoordinate)
            {
                // We've reached the end. We're done
                selected->Type = EPipeType::End;
                selected->Fixed = true;
                return true;
            }
        }
        else if (selected->State == BuildState::Committed && forJunction)
        {
            if (selected->Type == EPipeType::Straight || selected->Type == EPipeType::Corner)
            {
                // We're creating branch of an existing pipe
                validNeighborFilter = selected->Type;
            }
            else
            {
                UE_LOG(HoloPipesLog, Warning, L"LevelGenerator::CompletePipe - Unexpected committed pipe in open list (type %d)", selected->Type);
                consider = false;
            }
        }
        else
        {
            UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CompletePipe - Unexpected pipe in open list (type %d, state %d)", selected->Type, selected->State);
            consider = false;
        }

        if (consider)
        {
            // And consider all filtered neighbors for addition to the open list
            for (int i = 0; i < APPipe::ValidDirectionsCount; i++)
            {
                switch (validNeighborFilter)
                {
                    case EPipeType::None:
                        // If we don't have a filter, consider every neighbor which isn't our parent
                        consider = APPipe::ValidDirections[i] != selected->ParentDirection;
                        break;

                    case EPipeType::Straight:
                        // We have a straight filter, which means we can consider every direction
                        // that isn't already a connection for the pipe
                        consider = forJunction && ((APPipe::ValidDirections[i] & selected->Connections) == PipeDirections::None);
                        break;

                    case EPipeType::Corner:
                        // For corner pieces, the only valid connections are those opposite an existing connection
                        // (That's the only way to make a T junction, which is the only kind we support)
                        consider = forJunction && ((APPipe::InvertPipeDirection(APPipe::ValidDirections[i]) & selected->Connections) != PipeDirections::None);
                        break;

                    default:
                        consider = false;
                }

                if (consider)
                {
                    FPipeGridCoordinate neighborCoordinate = selected->Location + APPipe::PipeDirectionToLocationAdjustment(APPipe::ValidDirections[i]);

                        // Make sure the coordinate is still valid (either the end coordinate, or in the field of play)
                    if (neighborCoordinate == endCoordinate ||
                        (neighborCoordinate.X > m_sideMin&& neighborCoordinate.X < m_sideMax &&
                         neighborCoordinate.Y > m_sideMin&& neighborCoordinate.Y < m_sideMax &&
                         neighborCoordinate.Z > m_sideMin&& neighborCoordinate.Z < m_sideMax))
                    {
                        auto& neighbor = GetSegment(neighborCoordinate);

                        PipeDirections parentDirection = APPipe::InvertPipeDirection(APPipe::ValidDirections[i]);

                        // If our parent is a start, we consider that a straight piece. If it's a committed piece, that means we'll build a junction which is a straight piece.
                        // Otherwise, its a straight piece if the direction to our parent is the same as the direciton to its parent
                        bool parentStraight = selected->Type == EPipeType::Start || selected->State == BuildState::Committed || parentDirection == selected->ParentDirection;
                        int pathCost = selected->PathCost + (parentStraight ? m_straightCost : m_cornerCost);
                        int predictedCost = ComputePredictedCost(neighborCoordinate, endCoordinate, APPipe::ValidDirections[i], endDirection);

                        if (neighbor.State == BuildState::None)
                        {
                            // This neighbor hasn't been added to the open list, so add it now
                            neighbor.PipeClass = pipe.Class;
                            neighbor.ParentLocation = selected->Location;
                            neighbor.ParentDirection = parentDirection;
                            neighbor.PathCost = pathCost;
                            neighbor.PredictedCost = predictedCost;
                            neighbor.State = BuildState::OpenList;
                            m_openList.insert(&neighbor);
                        }
                        else if (neighbor.State == BuildState::OpenList && ((pathCost + predictedCost) < neighbor.TotalCost()))
                        {
                            // Remove from the open list first so that we don't break the sort
                            RemoveFromOpen(&neighbor);

                            // Update the neighbor's path state
                            neighbor.PathCost = pathCost;
                            neighbor.PredictedCost = predictedCost;
                            neighbor.ParentLocation = selected->Location;
                            neighbor.ParentDirection = parentDirection;

                            // And add it back to the open list
                            m_openList.insert(&neighbor);
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool LevelGenerator::GenerateJunctions(const PipeTemp& pipe)
{
    bool success = true;
    int generated = 0;

    while (success && generated < pipe.Junctions && m_endCandidates.size() > 0)
    {
        success = GenerateJunction(pipe);
        if (success)
        {
            generated++;
        }

    }

    return (generated == pipe.Junctions);
}


bool LevelGenerator::GenerateJunction(const PipeTemp& pipe)
{
    ResetAStar();    

    bool success = true;
    bool builtJunction = false;
    
    while (success && !builtJunction && m_endCandidates.size() > 0)
    {
        FPipeGridCoordinate endCoordinate = m_endCandidates.back();
        m_endCandidates.pop_back();

        auto& endSegment = GetSegment(endCoordinate);
        PipeDirections endDirection = SideFromCoordinate(endCoordinate);

            // Make sure the end isn't already in use
        if (endSegment.State == BuildState::None)
        {
            ResetAStar();

            // Find all straight and corner pieces in the pipe. Add their valid neighbors (4 max for straight
            // and 2 max for corners) to the open list and then complete the pipe
            for (auto& segment : m_pipeGrid)
            {
                if (segment.State == BuildState::Committed && segment.PipeClass == pipe.Class &&
                    (segment.Type == EPipeType::Straight || segment.Type == EPipeType::Corner))
                {
                    segment.PathCost = 0;
                    segment.PredictedCost = ComputePredictedCost(segment.Location, endCoordinate, endDirection, endDirection);
                    m_openList.insert(&segment);
                }
            }

            if (CompletePipe(pipe, endCoordinate, true))
            {
                builtJunction = true;
                success = CommitPipe(pipe, endCoordinate);
            }
        }
    }

    success = (success && builtJunction);

    return success && builtJunction;
}

bool LevelGenerator::GenerateFixed(const PipeTemp& pipe)
{
    if (pipe.Fixed > 0)
    {
        std::vector<PipeSegmentTemp*> candidates;
        for (auto& segment : m_pipeGrid)
        {
            if (segment.Type != EPipeType::None &&
                segment.PipeClass == pipe.Class &&
                !segment.Fixed)
            {
                candidates.push_back(&segment);
            }
        }

        m_rng.Shuffle(candidates);

        int generated = 0;
        for (auto segment : candidates)
        {
            bool valid = !(segment->Fixed);

            for (int i = 0; i < APPipe::ValidDirectionsCount && valid; i++)
            {
                PipeDirections direction = APPipe::ValidDirections[i];
                if ((segment->Connections & direction) == direction)
                {
                    const FPipeGridCoordinate neighborCoord = segment->Location + APPipe::PipeDirectionToLocationAdjustment(direction);
                    PipeSegmentTemp& neighbor = GetSegment(neighborCoord);
                    valid = !neighbor.Fixed;
                }
            }

            if (valid)
            {
                segment->Fixed = true;
                generated++;

                if (generated >= pipe.Fixed)
                {
                    return true;
                }
            }
        }
    }

    return (pipe.Fixed == 0);
}

bool LevelGenerator::CommitPipe(const PipeTemp& pipe, const FPipeGridCoordinate& end)
{
    bool success = true;
    m_committingList.clear();

    auto& endSegment = GetSegment(end);
    if (endSegment.Type != EPipeType::End)
    {
        UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Called with a pipe that is not an end");
        success = false;
    }

    // We commit in two stages:
    // 1) Walk the parent chain back from the provided end, looking for a committed start, straight or corner to connect. Along the way, we add each segment to the committing list
    // 2) After successfully walking from end to start, mark the entire pipe comitted
    // We do things this way to avoid partially committing a pipe in an error state. Any such error state is a bug which should be investigated
    if (success)
    {
        PipeDirections fromChildDirection = PipeDirections::None;
        FPipeGridCoordinate childLocation = end;

        bool complete = false;

        while (success && !complete)
        {
            FPipeGridCoordinate newCoordinate = childLocation + APPipe::PipeDirectionToLocationAdjustment(fromChildDirection);
            if (newCoordinate.X < m_sideMin || newCoordinate.X > m_sideMax ||
                newCoordinate.Y < m_sideMin || newCoordinate.Y > m_sideMax ||
                newCoordinate.Z < m_sideMin || newCoordinate.Z > m_sideMax)
            {
                UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - ran off end of grid walking parent chain");
                success = false;
            }
            else
            {
                auto& current = GetSegment(newCoordinate);
                PipeDirections childDirection = APPipe::InvertPipeDirection(fromChildDirection);
                int currentClass = current.PipeClass;

                if (pipe.Class != currentClass)
                {
                    UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Current is of class %d, pipe is of class %d", currentClass, pipe.Class);
                    success = false;
                }
                else
                {
                    switch (current.State)
                    {
                        case BuildState::None:
                        case BuildState::OpenList:
                        {
                            UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - found a segment in state %d walking parent chain", (int)current.State);
                            success = false;
                            break;
                        }

                        case BuildState::ClosedList:
                        {
                            switch (current.Type)
                            {
                                case EPipeType::Start:
                                    if (current.Connections != childDirection)
                                    {
                                        UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Start's child lies in direction %d, but expected %d", (int)childDirection, (int)current.Connections);
                                        success = false;
                                    }
                                    else
                                    {
                                        complete = true;
                                    }
                                    break;

                                case EPipeType::End:
                                    current.Connections = current.ParentDirection;
                                    break;

                                case EPipeType::None:
                                    current.Type = (fromChildDirection == current.ParentDirection ? EPipeType::Straight : EPipeType::Corner);
                                    current.Connections = (current.ParentDirection | childDirection);
                                    break;

                                default:
                                    UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Found unexpected closed pipe type walking parent chain (%d)", current.Type);
                                    success = false;
                                    break;
                            }

                            if (success)
                            {
                                current.State = BuildState::Committing;
                                m_committingList.push_back(&current);
                            }

                            break;
                        }

                        case BuildState::Committing:
                        {
                            UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Found a pipe in the committing state while walking parent chain");
                            success = false;
                        }

                        case BuildState::Committed:
                        {
                            complete = true;
                            switch (current.Type)
                            {
                                case EPipeType::Straight:
                                case EPipeType::Corner:
                                {
                                    PipeDirections newConnections = (current.Connections | childDirection);
                                    FRotator newRotation;
                                    if (!GetPipeRotation(EPipeType::Junction, newConnections, newRotation))
                                    {
                                        UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - Adding connection %d resulted in invalid junction %d", (int)childDirection, (int)current.Connections);
                                        success = false;
                                    }
                                    else
                                    {
                                        current.Type = EPipeType::Junction;
                                        current.Connections = newConnections;
                                    }

                                    break;
                                }

                                default:
                                {
                                    UE_LOG(HoloPipesLog, Error, L"LevelGenerator::CommitPipe - found a committed segment of type %d walking parent chain", (int)current.Type);
                                    success = false;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }

                if (success && !complete)
                {
                    fromChildDirection = current.ParentDirection;
                    childLocation = current.Location;
                }
            }
        }
    }

    if (success)
    {
        for (auto segment : m_committingList)
        {
            segment->State = BuildState::Committed;
        }
    }

    m_committingList.clear();

    return success;
}

void LevelGenerator::RemoveFromOpen(PipeSegmentTemp* victim)
{
    auto range = m_openList.equal_range(victim);
    auto it = range.first;
    bool removed = false;

    while (!removed && it != range.second)
    {
        if ((*it)->Location == victim->Location)
        {
            m_openList.erase(it);
            removed = true;
        }
        else
        {
            it++;
        }
    }

    if (!removed)
    {
        UE_LOG(HoloPipesLog, Warning, L"LevelGenerator::RemoveFromOpen - failed to find victim in open list");
    }
}

LevelGenerator::PipeSegmentTemp* LevelGenerator::RemoveRandomLeastFromOpen()
{
    PipeSegmentTemp* victim = nullptr;

    if (m_openList.size() > 0)
    {
        auto range = m_openList.equal_range(*m_openList.begin());

        auto itWalk = range.first;
        int count = 0;

        while (itWalk != range.second && itWalk != m_openList.end())
        {
            count++;
            itWalk++;
        }

        if (count > 0)
        {
            int selectedIndex = (count == 1 ? 0 : m_rng.GetInt(0, count));

            itWalk = range.first;
            for (int i = 0; i < selectedIndex; i++)
            {
                itWalk++;
            }

            victim = (*itWalk);
            m_openList.erase(itWalk);
        }
    }

    return victim;
}

void LevelGenerator::ResetAStar()
{
    for (auto pipe : m_openList)
    {
        if (pipe->State != BuildState::Committed)
        {
            const FPipeGridCoordinate location = pipe->Location;
            (*pipe) = PipeSegmentTemp::c_Empty;
            pipe->Location = location;
        }
    }

    m_openList.clear();

    for (auto pipe : m_closedList)
    {
        if (pipe->State != BuildState::Committed)
        {
            const FPipeGridCoordinate location = pipe->Location;
            (*pipe) = PipeSegmentTemp::c_Empty;
            pipe->Location = location;
        }
    }

    m_closedList.clear();
}
