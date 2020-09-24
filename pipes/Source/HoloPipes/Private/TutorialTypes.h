#pragma once

#include <list>
#include <vector>
#include <string>

enum class TutorialDisabledFeatures
{
    None        = 0x00,
    Drag        = 0x01,
    Rotate      = 0x02,
    Toolbox     = 0x04
};

DEFINE_ENUM_FLAG_OPERATORS(TutorialDisabledFeatures);

enum class TutorialGoalType
{
    None,
    Next,
    Dismiss,
    Move,
    Menu,
    Grab,
    Place,
    Handle,
    GrabFromToolbox
};

enum class TutorialArrowType
{
    Toolbox,
    Coordinate
};

struct TutorialPipe
{
    EPipeType Type = EPipeType::None;
    int PipeClass = DefaultPipeClass;
    PipeDirections Connections = PipeDirections::None;
    FPipeGridCoordinate Coordinate = FPipeGridCoordinate::Zero;
    bool Fixed = false;
};

struct TutorialGoal
{
    TutorialGoalType Type = TutorialGoalType::None;
    TutorialPipe Pipe = {};
    float MinTime = 0;
    int32 MinCount = 0;
};

struct TutorialPageAnchor
{
    bool Valid = false;
    FPipeGridCoordinate Coordinate = FPipeGridCoordinate::Zero;
    FVector Offset = FVector::ZeroVector;
};

struct TutorialArrow
{
    TutorialArrowType Type = TutorialArrowType::Toolbox;
    FPipeGridCoordinate Coordinate = FPipeGridCoordinate::Zero;
};

struct TutorialPage
{
    TutorialPageAnchor Anchor = {};
    std::wstring Text = L"";
    TutorialDisabledFeatures Disabled = TutorialDisabledFeatures::None;

    std::vector<TutorialPipe> Pipes;
    std::vector<EPipeType> Toolbox;
    std::vector<TutorialArrow> Arrows;
    std::vector<TutorialGoal> Goals;
};

struct TutorialLevel
{
    int32 Level = 0;
    int32 GridSize = 0;

    bool HasSeed = false;
    int32 Seed = 0;

    bool Generate = false;

    std::vector<TutorialPage> Pages;
}; 
