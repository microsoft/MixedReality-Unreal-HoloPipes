
#include "PPipeGrid_Internal.h"



FVector APPipeGrid::WorldLocationToTransformLocation(const FVector& worldLocation, const FTransform& toWorldTransform)
{
    return toWorldTransform.Inverse().TransformPosition(worldLocation);
}

FVector APPipeGrid::TransformLocationToWorldLocation(const FVector& transformLocation, const FTransform& toWorldTransform)
{
    return toWorldTransform.TransformPosition(transformLocation);
}

FVector APPipeGrid::WorldLocationToGridLocation(const FVector& worldLocation)
{
    return WorldLocationToTransformLocation(worldLocation, GetActorTransform());
}

FTransform APPipeGrid::WorldTransformToGridTransform(const FTransform& worldTransform)
{
    const FTransform gridToWorld = GetActorTransform();
    const FTransform worldToGrid = gridToWorld.Inverse();
    return worldTransform * worldToGrid;
}

FVector APPipeGrid::GridLocationToWorldLocation(const FVector& gridLocation)
{
    return TransformLocationToWorldLocation(gridLocation, GetActorTransform());
}

FPipeGridCoordinate APPipeGrid::GridLocationToGridCoordinate(const FVector& gridLocation)
{
    return
    {
        (int)round(gridLocation.X / CellSize),
        (int)round(gridLocation.Y / CellSize),
        (int)round(gridLocation.Z / CellSize)
    };
}

FVector APPipeGrid::GridCoordinateToGridLocation(const FPipeGridCoordinate& gridCoordinate)
{
    return {
        CellSize * gridCoordinate.X,
        CellSize * gridCoordinate.Y,
        CellSize * gridCoordinate.Z
    };
}