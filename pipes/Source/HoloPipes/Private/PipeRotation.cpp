#include "PipeRotation.h"

struct PipeRotationEntry
{
	EPipeType RotationValidFor;
	FRotator Rotation;
};

const PipeRotationEntry ConnectionsToRotations[] =
{
	// 0 - None
	{ EPipeType::Block, {0.0f, 0.0f, 0.0f} },

	// 1 - Right
	{ EPipeType::Start | EPipeType::End, {0.0f, 90.0f, 0.0f} },

	// 2 - Front
	{ EPipeType::Start | EPipeType::End, {0.0f, 0.0f, 0.0f} },

	// 3 - Front, Right
	{ EPipeType::Corner, {0.0f, 0.0f, 90.0f} },

	// 4 - Left
	{ EPipeType::Start | EPipeType::End, {0.0f, -90.0f, 0.0f} },

	// 5 - Left, Right
	{ EPipeType::Straight, {0.0f, 90.0f, 0.0f} },

	// 6 - Left, Front
	{ EPipeType::Corner, {0.0f, -90.0f, 90.0f} },

	// 7 - Left, Front, Right
	{ EPipeType::Junction, {0.0f, -90.0f, 90.0f} },

	// 8 - Back
	{ EPipeType::Start | EPipeType::End, {0.0f, 180.0f, 0.0f} },

	// 9 - Back, Right
	{ EPipeType::Corner, {0.0f, 90.0f, 90.0f} },

	// 10 - Back, Front
	{ EPipeType::Straight, {0.0f, 0.0f, 0.0f} },

	// 11 - Back, Front, Right
	{ EPipeType::Junction, {0.0f, 0.0f, 90.0f} },

	// 12 - Back, Left
	{ EPipeType::Corner, {0.0f, 180.0f, 90.0f} },

	// 13 - Back, Left, Right
	{ EPipeType::Junction, {0.0f, 90.0f, 90.0f} },

	// 14 - Back, Left, Front
	{ EPipeType::Junction, {0.0f, 0.0f, -90.0f} },

	// 15 - Back, Left, Front, Right
	{ },

	// 16 - Top
	{ EPipeType::Start | EPipeType::End, {90.0f, 0.0f, 0.0f} },

	// 17 - Top, Right
	{ EPipeType::Corner, {0.0f, 90.0f, 0.0f} },

	// 18 - Top, Front
	{ EPipeType::Corner, {0.0f, 0.0f, 0.0f} },

	// 19 - Top, Front, Right
	{ },

	// 20 - Top, Left
	{ EPipeType::Corner, {0.0f, -90.0f, 0.0f} },

	// 21 - Top, Left, Right
	{ EPipeType::Junction, {0.0f, 90.0f, 0.0f} },

	// 22 - Top, Left, Front
	{ },

	// 23 - Top, Left, Front, Right
	{ },

	// 24 - Top, Back
	{ EPipeType::Corner, {0.0f, 180.0f, 0.0f} },

	// 25 - Top, Back, Right
	{ },

	// 26 - Top, Back, Front
	{ EPipeType::Junction, {0.0f, 0.0f, 0.0f} },

	// 27 - Top, Back, Front, Right
	{ },

	// 28 - Top, Back, Left
	{ },

	// 29 - Top, Back, Left, Right
	{ },

	// 30 - Top, Back, Left, Front
	{ },

	// 31 - Top, Back, Left, Front, Right
	{ },

	// 32 - Bottom,
	{ EPipeType::Start | EPipeType::End, {-90.0f, 180.0f, 0.0f} },

	// 33 - Bottom, Right
	{ EPipeType::Corner, {180.0f, -90.0f, 0.0f} },

	// 34 - Bottom, Front
	{ EPipeType::Corner, {180.0f, 180.0f, 0.0f} },

	// 35 - Bottom, Front, Right
	{ },

	// 36 - Bottom, Left
	{ EPipeType::Corner, {180.0f, 90.0f, 0.0f} },

	// 37 - Bottom, Left, Right
	{ EPipeType::Junction, {180.0f, -90.0f, 0.0f} },

	// 38 - Bottom, Left, Front
	{ },

	// 39 - Bottom, Left, Front, Right
	{ },

	// 40 - Bottom, Back
	{ EPipeType::Corner, {180.0f, 0.0f, 0.0f} },

	// 41 - Bottom, Back, Right
	{ },

	// 42 - Bottom, Back, Front
	{ EPipeType::Junction, {0.0f, 0.0f, 180.0f} },

	// 43 - Bottom, Back, Front, Right
	{ },

	// 44 - Bottom, Back, Left
	{ },

	// 45 - Bottom, Back, Left, Right
	{ },

	// 46 - Bottom, Back, Left, Front
	{ },

	// 47 - Bottom, Back, Left, Front, Right
	{ },

	// 48 - Bottom, Top
	{ EPipeType::Straight, {90.0f, 0.0f, 0.0f} },

	// 49 - Bottom, Top, Right
	{ EPipeType::Junction, {90.0f, -90.0f, 0.0f} },

	// 50 - Bottom, Top, Front
	{ EPipeType::Junction, {-90.0f, 0.0f, 0.0f} },

	// 51 - Bottom, Top, Front, Right
	{ },

	// 52 - Bottom, Top, Left
	{ EPipeType::Junction, {90.0f, 90.0f, 0.0f} },

	// 53 - Bottom, Top, Left, Right
	{ },

	// 54 - Bottom, Top, Left, Front
	{ },

	// 55 - Bottom, Top, Left, Front, Right
	{ },

	// 56 - Bottom, Top, Back
	{ EPipeType::Junction, {90.0f, 0.0f, 0.0f} },

	// 57 - Bottom, Top, Back, Right
	{ },

	// 58 - Bottom, Top, Back, Front
	{ },

	// 59 - Bottom, Top, Back, Front, Right
	{ },

	// 60 - Bottom, Top, Back, Left
	{ },

	// 61 - Bottom, Top, Back, Left, Right
	{ },

	// 62 - Bottom, Top, Back, Left, Front
	{ },

	// 63 - Bottom, Top, Back, Left, Front, Right
	{ },
};

bool GetPipeRotation(EPipeType type, PipeDirections connections, FRotator& rotator)
{
	bool valid = false;
	rotator = FRotator::ZeroRotator;

	int rotationIndex = (int)connections;
	if (rotationIndex >= ARRAYSIZE(ConnectionsToRotations) ||
		(ConnectionsToRotations[rotationIndex].RotationValidFor & type) != type)
	{
		UE_LOG(HoloPipesLog, Error, L"GetPipeRotation invalid connections (%d) specified for pipe type (%d)", rotationIndex, type);
	}
	else
	{
		rotator = ConnectionsToRotations[rotationIndex].Rotation;
		valid = true;
	}

	return valid;
}

float SnapAngleTo90DegreeIncrements(float a)
{
    return 90.0f * roundf(a / 90.0f);
}

FRotator SnapRotationTo90DegreeIncrements(FRotator rotation)
{
    rotation.Yaw = SnapAngleTo90DegreeIncrements(rotation.Yaw);
    rotation.Pitch = SnapAngleTo90DegreeIncrements(rotation.Pitch);
    rotation.Roll= SnapAngleTo90DegreeIncrements(rotation.Roll);
    return rotation;
}

PipeDirections RotateConnections(PipeDirections connections, FRotator rotator)
{
    rotator = SnapRotationTo90DegreeIncrements(rotator);

#if WITH_EDITOR
    int connectionsCount = 0;
#endif

    PipeDirections newConnections = PipeDirections::None;

    for (int i = 0; i < APPipe::ValidDirectionsCount; i++)
    {
        if ((APPipe::ValidDirections[i] & connections) != PipeDirections::None)
        {
            const FPipeGridCoordinate axis = APPipe::PipeDirectionToLocationAdjustment(APPipe::ValidDirections[i]);
            const FVector axisVector = { (float)axis.X, (float)axis.Y, (float)axis.Z };

            FVector newAxisVector = rotator.RotateVector(axisVector);
            newConnections |= APPipe::LocationAdjustmentToPipeDirection(newAxisVector);

#if WITH_EDITOR
            connectionsCount++;
#endif
        }
    }


#if WITH_EDITOR

    // In the editor, as a sanity check, make sure we have the same number of connections both before and after the rotation

    int newConnectionsCount = 0;

    for (int i = 0; i < APPipe::ValidDirectionsCount; i++)
    {
        if ((APPipe::ValidDirections[i] & connections) != PipeDirections::None)
        {
            newConnectionsCount++;
        }
    }

    if (newConnectionsCount != connectionsCount)
    {
        UE_LOG(HoloPipesLog, Error, L"PipeRotation - Rotation %08X with %d connections rotated to %08X with %d connections", (int)connections, connectionsCount, newConnections, newConnectionsCount);
    }

#endif

    return newConnections;
}