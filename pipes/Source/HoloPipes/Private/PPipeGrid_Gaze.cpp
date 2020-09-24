
#include "PPipeGrid_Internal.h"

void APPipeGrid::UpdateGaze(const FGazeUpdate& worldSpaceGaze)
{
    m_gaze.Origin = WorldLocationToGridLocation(worldSpaceGaze.Origin);
    m_gaze.Direction = GetActorTransform().Inverse().TransformVectorNoScale(worldSpaceGaze.Direction);
}

bool APPipeGrid::GazeIntersectsCoordinate(const FPipeGridCoordinate& coordinate, FVector* position)
{
    const float halfCell = CellSize / 2.0f;
    const FVector halfCellVector = FVector(halfCell);
    const FVector coordinatePosition = GridCoordinateToGridLocation(coordinate);

    return GazeIntersectsAABB(coordinatePosition - halfCellVector, coordinatePosition + halfCellVector, position);
}

constexpr int VectorDimCount = 3;

enum class Quadrant
{
    Right,
    Left,
    Middle
};

bool APPipeGrid::GazeIntersectsAABB(const FVector& minAABB, const FVector& maxAABB, FVector* position)
{
    bool intersects = false;
    FVector candidatePosition = { 0,0,0 };

    // Adapted from "Fast Ray-Box Intersection" by Andrew Woo - "Graphics Gems", Academic Press, 1990
    bool inside = true;
    Quadrant quadrant[VectorDimCount] = {};
    float candidatePlane[VectorDimCount] = {};

    // Find candidate planes
    for (int i = 0; i < VectorDimCount; i++)
    {
        if (m_gaze.Origin[i] < minAABB[i])
        {
            quadrant[i] = Quadrant::Left;
            candidatePlane[i] = minAABB[i];
            inside = false;
        }
        else if (m_gaze.Origin[i] > maxAABB[i])
        {
            quadrant[i] = Quadrant::Right;
            candidatePlane[i] = maxAABB[i];
            inside = false;
        }
        else
        {
            quadrant[i] = Quadrant::Middle;
            candidatePlane[i] = 0;
        }
    }

    if (inside)
    {
        // Short circuit if ray cast from inside box (we know it intersets). This
        // produces an inaccurate intersection point, so revisit if that becomes
        // an issue
        candidatePosition = m_gaze.Origin;
        intersects = true;
    }
    else
    {
        // Calculate T distances to candidate planes
        float maxT[VectorDimCount] = {};
        for (int i = 0; i < VectorDimCount; i++)
        {
            if (quadrant[i] != Quadrant::Middle && m_gaze.Direction[i] != 0)
            {
                maxT[i] = (candidatePlane[i] - m_gaze.Origin[i]) / m_gaze.Direction[i];
            }
            else
            {
                maxT[i] = -1.0f;
            }
        }

        // Get the largest of the maxT's for the final choice of intersection
        int maxPlane = 0;
        for (int i = 1; i < VectorDimCount; i++)
        {
            if (maxT[maxPlane] < maxT[i])
            {
                maxPlane = i;
            }
        }

        // Check final candidate actually inside box
        if (maxT[maxPlane] >= 0)
        {
            intersects = true;
            for (int i = 0; intersects && i < VectorDimCount; i++)
            {
                if (maxPlane == i)
                {
                    candidatePosition[i] = candidatePlane[i];
                }
                else
                {
                    float pos = m_gaze.Origin[i] + (maxT[maxPlane] * m_gaze.Direction[i]);;
                    if (pos < minAABB[i] || pos > maxAABB[i])
                    {
                        intersects = false;
                    }
                    else
                    {
                        candidatePosition[i];
                    }
                }
            }
        }
        
    }

    (*position) = intersects ? candidatePosition : FVector(0, 0, 0);
    return intersects;

}

bool APPipeGrid::FindMinDistanceFromGaze(const FVector& position, float* distance)
{
    float distanceSq = 0;
    if (FindMinDistanceFromGazeSq(position, &distanceSq))
    {
        (*distance) = sqrt(distanceSq);
        return true;
    }

    return false;
}

bool APPipeGrid::FindMinDistanceFromGazeSq(const FVector& position, float* distance)
{
    const FVector originToPosition = (position - m_gaze.Origin);
    const float originToPositionLengthSq = originToPosition.SizeSquared();

    // Compute distance along gaze to point closest to position
    float closestDistanceAlongGaze = FVector::DotProduct(originToPosition, m_gaze.Direction);

    // A negative distance means the position is behind us, and therefore not interesting
    if (closestDistanceAlongGaze > 0)
    {
        (*distance) = FVector::DistSquared(m_gaze.Origin + (m_gaze.Direction * closestDistanceAlongGaze), position);
        return true;
    }


    return false;
}

void APPipeGrid::MarkPipesForGaze(MarkPipesForGazeMode mode)
{
    for (auto& entry : m_pipesGrid)
    {
        if (entry.second.pipe && !entry.second.pipe->GetPipeFixed() && !RotateEngaged(entry.first))
        {
            if (mode == MarkPipesForGazeMode::ClearAll)
            {
                entry.second.pipe->SetFocusStage(EFocusStage::None);
            }
            else
            {
                FVector position;
                entry.second.pipe->SetFocusStage(GazeIntersectsCoordinate(entry.first, &position) ? EFocusStage::HandAndGaze : EFocusStage::None);
            }
        }
    }

    
}
