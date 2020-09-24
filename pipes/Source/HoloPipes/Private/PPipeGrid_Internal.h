#pragma once

#include "PPipeGrid.h"
#include "PipeRotation.h"
#include "Engine/world.h"
#include "Kismet/KismetMathLibrary.h"


namespace
{
    template <class T>
    bool IsAnyFlagSet(T flags, T flag)
    {
        return ((flags & flag) != (T)0);
    }
}