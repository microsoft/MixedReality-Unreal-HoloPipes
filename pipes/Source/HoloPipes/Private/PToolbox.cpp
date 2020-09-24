// Fill out your copyright notice in the Description page of Project Settings.


#include "PToolbox.h"
#include "PipeRotation.h"

// Sets default values
APToolbox::APToolbox()
{
    PrimaryActorTick.bCanEverTick = true;
    Focus = EToolboxFocus::None;
}

// Called when the game starts or when spawned
void APToolbox::BeginPlay()
{
	Super::BeginPlay();
	
}

void APToolbox::Clear()
{
    Contents = { 0,0,0 };

    if (m_initializationComplete)
    {
        OnToolboxContentsChanged();
    }
}

void APToolbox::Initialize(const std::vector<PipeSegmentGenerated>& pipeSegments)
{
    Contents = { 0,0,0 };

    for (const auto& segment : pipeSegments)
    {
        switch (segment.Type)
        {
            case EPipeType::Straight:
                Contents.StraightPipes++;
                break;

            case EPipeType::Corner:
                Contents.CornerPipes++;
                break;

            case EPipeType::Junction:
                Contents.JunctionPipes++;
                break;
        }
    }

    m_initializationComplete = true;
	OnToolboxInitializationComplete();
}

void APToolbox::RemovePipe(EPipeType toRemove)
{
    switch (toRemove)
    {
        case EPipeType::Straight:
            if (Contents.StraightPipes > 0)
            {
                Contents.StraightPipes--;
                OnToolboxContentsChanged();
            }
            break;

        case EPipeType::Corner:
            if (Contents.CornerPipes > 0)
            {
                Contents.CornerPipes--;
                OnToolboxContentsChanged();
            }
            break;

        case EPipeType::Junction:
            if (Contents.JunctionPipes> 0)
            {
                Contents.JunctionPipes--;
                OnToolboxContentsChanged();
            }
            break;
    }
}


void APToolbox::AddPipe(EPipeType toAdd)
{
    switch (toAdd)
    {
        case EPipeType::Straight:
            Contents.StraightPipes++;
            break;

        case EPipeType::Corner:
            Contents.CornerPipes++;
            break;

        case EPipeType::Junction:
            Contents.JunctionPipes++;
            break;
    }

    OnToolboxContentsChanged();
}

int APToolbox::CountRemaining(EPipeType type)
{
    switch (type)
    {
        case EPipeType::Straight:
            return Contents.StraightPipes;

        case EPipeType::Corner:
            return Contents.CornerPipes;

        case EPipeType::Junction:
            return Contents.JunctionPipes;

        default:
            return 0;
    }
}

void APToolbox::SetFocus(EToolboxFocus newFocus)
{
    if (Focus != newFocus)
    {
        Focus = newFocus;
        OnToolboxFocusChanged();
    }
}

void APToolbox::SetDropTarget(FVector dropTarget)
{
    DropTarget = dropTarget;
    OnDropTargetChanged();
}

