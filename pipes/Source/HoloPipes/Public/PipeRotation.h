#pragma once

#include "PPipe.h"

bool GetPipeRotation(EPipeType type, PipeDirections connections, FRotator& rotator);
PipeDirections RotateConnections(PipeDirections connections, FRotator rotator);

float SnapAngleTo90DegreeIncrements(float a);
FRotator SnapRotationTo90DegreeIncrements(FRotator rotation);
