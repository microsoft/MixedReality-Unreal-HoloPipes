// Fill out your copyright notice in the Description page of Project Settings.


#include "PTutorialInstance.h"
#include "Engine/world.h"

constexpr int32 DefaultGridSize = 4;
constexpr float MinAllowedMinTme = 0.5f;

// Sets default values
APTutorialInstance::APTutorialInstance()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PipeGrid = nullptr;
	ToolboxArrow = nullptr;
	Text = FText::FromString(L"");
	Anchored = false;
	GridSpaceAnchor = FVector::ZeroVector;
	WorldSpaceOffset = FVector::ZeroVector;
	Button = ETutorialButton::None;
}

void APTutorialInstance::Initialize(APPipeGrid* grid, std::shared_ptr<TutorialLevel> tutorial)
{
	if (m_state == TutorialState::None && grid && tutorial)
	{
		PipeGrid = grid;
		m_tutorial = tutorial;
		m_currentPage = 0;

		m_state = TutorialState::Initialized;
	}
}

bool APTutorialInstance::GetShouldGenerateLevel()
{
	return m_tutorial->Generate ? true : (m_tutorial->GridSize <= 0);
}

int32 APTutorialInstance::GetGridSize()
{
	return !m_tutorial->Generate ? m_tutorial->GridSize : 0;
}

void APTutorialInstance::StartTutorial()
{
	if (m_state == TutorialState::Initialized)
	{
		PipeGrid->OnToolboxMoved.AddDynamic(this, &APTutorialInstance::ToolboxMoved);
		PipeGrid->OnPipeGrasped.AddDynamic(this, &APTutorialInstance::HandlePipeGrasped);
		PipeGrid->OnPipeDropped.AddDynamic(this, &APTutorialInstance::HandlePipeDropped);
		PipeGrid->OnRotateHandleShown.AddDynamic(this, &APTutorialInstance::HandleRotateHandleShown);
		ShowPage();
	}
}

void APTutorialInstance::StopTutorial()
{
	if (m_state != TutorialState::Stopped)
	{
		ClosePage();
		m_state = TutorialState::Stopped;

		PipeGrid->OnToolboxMoved.RemoveDynamic(this, &APTutorialInstance::ToolboxMoved);
	}
}

void APTutorialInstance::AdvancePage()
{
	if (m_state == TutorialState::Running)
	{
		ClosePage();

		m_currentPageIndex++;
		if (m_currentPageIndex < m_tutorial->Pages.size())
		{
			ShowPage();
		}
		else
		{
			m_state = TutorialState::Stopped;
		}
	}
}

void APTutorialInstance::ShowPage()
{
	if (m_state == TutorialState::Initialized || m_state == TutorialState::Running)
	{
		m_state = TutorialState::Running;

		m_currentPage = &m_tutorial->Pages[m_currentPageIndex];

		PrepareGoals();

		bool toolboxEnabled = (m_currentPage->Disabled & TutorialDisabledFeatures::Toolbox) == TutorialDisabledFeatures::None;

		PipeGrid->SetToolboxEnabled(toolboxEnabled);
		PipeGrid->SetDragEnabled((m_currentPage->Disabled & TutorialDisabledFeatures::Drag) == TutorialDisabledFeatures::None);
		PipeGrid->SetRotateEnabled((m_currentPage->Disabled & TutorialDisabledFeatures::Rotate) == TutorialDisabledFeatures::None);        
		
		if (!toolboxEnabled)
		{
			PipeGrid->RemoveToolbox();
		}
		else
		{
			std::vector<PipeSegmentGenerated> toolboxPipes;
			for (auto& type : m_currentPage->Toolbox)
			{
				toolboxPipes.push_back({ type, 0, PipeDirections::None, {0,0,0} });
			}

			if (toolboxPipes.size() > 0)
			{
				if (!PipeGrid->ToolboxAvailable() || (!GetShouldGenerateLevel() && m_currentPageIndex == 0))
				{
					int32 gridSize = GetGridSize();
					PipeGrid->InitializeToolbox(toolboxPipes, gridSize > 0 ? gridSize : DefaultGridSize);
					CreatedToolbox();
				}
				else
				{
					PipeGrid->AddToToolbox(toolboxPipes);
				}
			}
		}

		bool finalizeLevel = false;
		std::vector<PipeSegmentGenerated> fixedPipes;
		std::vector<PipeSegmentGenerated> placedPipes;

		for (auto& pipe : m_currentPage->Pipes)
		{
			if (pipe.Fixed)
			{
				fixedPipes.push_back({ pipe.Type, pipe.PipeClass, (PipeDirections)pipe.Connections, pipe.Coordinate });
			}
			else
			{
				placedPipes.push_back({ pipe.Type, pipe.PipeClass, (PipeDirections)pipe.Connections, pipe.Coordinate });
			}
		}

		if (fixedPipes.size() > 0)
		{
			PipeGrid->AddPipes(fixedPipes, AddPipeOptions::Fixed);
			finalizeLevel = true;
		}

		if (placedPipes.size() > 0)
		{
			PipeGrid->AddPipes(placedPipes, AddPipeOptions::None);
			finalizeLevel = true;
		}

		if (finalizeLevel)
		{
			PipeGrid->FinalizeLevel();
		}

		CreateArrows();

		Text = FText::FromString(m_currentPage->Text.c_str());

		if (m_currentPage->Anchor.Valid)
		{
			Anchored = true;
			WorldSpaceOffset = m_currentPage->Anchor.Offset;

			// We treat the anchor as the "bottom" of the requested coordinate, putting the center
			// of the dialog between two grid squares. The offset is that meant to raise/lower as
			// appropriate
			FPipeGridCoordinate requested = m_currentPage->Anchor.Coordinate;
			FPipeGridCoordinate below = { requested.X, requested.Y, requested.Z - 1 };

			GridSpaceAnchor = (PipeGrid->GridCoordinateToGridLocation(requested) + PipeGrid->GridCoordinateToGridLocation(below)) / 2;
		}

		ShowDialog();
	}
}

void APTutorialInstance::ClosePage()
{
	if (m_state == TutorialState::Running && m_currentPage)
	{
		HideDialog();

		Anchored = false;
		GridSpaceAnchor = FVector::ZeroVector;
		WorldSpaceOffset = FVector::ZeroVector;
		Button = ETutorialButton::None;

		ClearArrows();

		PipeGrid->SetToolboxEnabled(true);
		PipeGrid->SetDragEnabled(true);
		PipeGrid->SetRotateEnabled(true);

		ClearGoals();

		m_currentPage = nullptr;
	}
}

void APTutorialInstance::PauseTutorial()
{
	if (m_state == TutorialState::Running)
	{
		ClosePage();
		m_state = TutorialState::Paused;
	}
}

void APTutorialInstance::ResumeTutorial()
{
	if (m_state == TutorialState::Paused)
	{
		m_state = TutorialState::Running;
		ShowPage();
	}
}

void APTutorialInstance::CreateArrows()
{
	if (ArrowClass)
	{
		for (const auto& arrow : m_currentPage->Arrows)
		{
			bool isToolboxArrow = (arrow.Type == TutorialArrowType::Toolbox);
			bool createArrow = !isToolboxArrow || !ToolboxArrow;

			if (createArrow)
			{
				AActor* newArrow = GetWorld()->SpawnActor<AActor>(ArrowClass);
				Arrows.Add(newArrow);

				newArrow->AttachToActor(PipeGrid, FAttachmentTransformRules::KeepRelativeTransform);

				if (isToolboxArrow)
				{
					ToolboxArrow = newArrow;
					PlaceToolboxArrow();
				}
				else
				{
					newArrow->SetActorRelativeLocation(PipeGrid->GridCoordinateToGridLocation(arrow.Coordinate));
				}
			}
		}
	}
}

void APTutorialInstance::ClearArrows()
{
	for (auto arrow : Arrows)
	{
		arrow->Destroy();
	}

	Arrows.Empty();
	ToolboxArrow = nullptr;
}

void APTutorialInstance::PlaceToolboxArrow()
{
	if (ToolboxArrow)
	{
		FPipeGridCoordinate toolboxCoordinate;
		if (PipeGrid->GetToolboxCoordinate(&toolboxCoordinate))
		{
			// Adjust so the arrow is above the toolbox's handle
			toolboxCoordinate.Z += 3;
			ToolboxArrow->SetActorRelativeLocation(PipeGrid->GridCoordinateToGridLocation(toolboxCoordinate));
		}
	}
}

void APTutorialInstance::ToolboxMoved()
{
	PlaceToolboxArrow();
}

void APTutorialInstance::PrepareGoals()
{
	bool advancePage = false;

	for (auto& goal : m_currentPage->Goals)
	{
		switch (goal.Type)
		{
			case TutorialGoalType::Next:
				Button = ETutorialButton::Next;
				break;

			case TutorialGoalType::Dismiss:
				Button = ETutorialButton::Dismiss;
				break;

			case TutorialGoalType::Move:
				m_moveGameGoal = true;
				break;

			case TutorialGoalType::Menu:
				m_menuGoal = true;
				break;

			case TutorialGoalType::Grab:
			case TutorialGoalType::GrabFromToolbox:
				m_graspedGoal = true;
				break;

			case TutorialGoalType::Place:
			{
				// Check if the goal is already satisfied

				EPipeType type;
				int pipeClass;
				PipeDirections connections;

				PipeGrid->GetPipeInfo(goal.Pipe.Coordinate, &type, &pipeClass, &connections);
				if (type == goal.Pipe.Type && connections == goal.Pipe.Connections)
				{
					advancePage = true;
				}

				m_droppedGoal = true;
				break;
			}

			case TutorialGoalType::Handle:
				m_rotateHandleGoal = true;
				m_minTimeElapsed = (goal.MinTime < MinAllowedMinTme);
				m_minCountSatisifed = (goal.MinCount == 0);
				break;
		}
	}

	if (advancePage)
	{
		AdvancePage();
	}
}

void APTutorialInstance::ClearGoals()
{
	Button = ETutorialButton::None;
	m_moveGameGoal = false;
	m_menuGoal = false;
	m_graspedGoal = false;
	m_droppedGoal = false;
	m_rotateHandleGoal = false;

	m_handleCount = 0;
	m_minTimeElapsed = false;
	m_minCountSatisifed = false;

	if (MinTimeHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(MinTimeHandle);
	}
}

void APTutorialInstance::ButtonPressed()
{
	if (Button != ETutorialButton::None)
	{
		AdvancePage();
	}
}

void APTutorialInstance::MoveGamePressed()
{
	if (m_moveGameGoal)
	{
		AdvancePage();
	}
}

void APTutorialInstance::MenuShown()
{
	if (m_menuGoal)
	{
		AdvancePage();
	}
}

void APTutorialInstance::HandlePipeGrasped(FPipeGridCoordinate graspLocation)
{
	if (m_graspedGoal)
	{
		for (auto& goal : m_currentPage->Goals)
		{
			if (goal.Type == TutorialGoalType::Grab && graspLocation == goal.Pipe.Coordinate)
			{
				AdvancePage();
				return;
			}
			else if (goal.Type == TutorialGoalType::GrabFromToolbox && PipeGrid->CoordinateInToolbox(graspLocation))
			{
				AdvancePage();
				return;
			}
		}
	}
}

void APTutorialInstance::HandlePipeDropped(FPipeGridCoordinate dropLocation)
{
	if (m_droppedGoal)
	{
		for (auto& goal : m_currentPage->Goals)
		{
			if (goal.Type == TutorialGoalType::Place && dropLocation == goal.Pipe.Coordinate)
			{
				EPipeType type;
				int pipeClass;
				PipeDirections connections;

				PipeGrid->GetPipeInfo(dropLocation, &type, &pipeClass, &connections);
				if (type == goal.Pipe.Type && connections == goal.Pipe.Connections)
				{
					AdvancePage();
					return;
				}
			}
		}
	}
}

void APTutorialInstance::HandleRotateHandleShown(FPipeGridCoordinate pipeLocation, EInteractionAxis axis)
{
	if (m_rotateHandleGoal)
	{
		for (auto& goal : m_currentPage->Goals)
		{
			if (goal.Type == TutorialGoalType::Handle && pipeLocation == goal.Pipe.Coordinate)
			{
				if (axis == EInteractionAxis::None)
				{
					// We lost the rotate handle
					if (!m_minTimeElapsed && MinTimeHandle.IsValid())
					{
						GetWorld()->GetTimerManager().ClearTimer(MinTimeHandle);
					}
					return;
				}
				else
				{
					m_handleCount++;
					m_minCountSatisifed = (m_minCountSatisifed || (m_handleCount >= goal.MinCount));

					if (m_minCountSatisifed && m_minTimeElapsed)
					{
						AdvancePage();
					}
					else
					{
						if (!m_minTimeElapsed && !MinTimeHandle.IsValid())
						{
							GetWorld()->GetTimerManager().SetTimer(MinTimeHandle, this, &APTutorialInstance::MinTimeElapsed, goal.MinTime);
						}
					}
				}

				return;
			}
		}
	}
}

void APTutorialInstance::MinTimeElapsed()
{
	if (m_rotateHandleGoal)
	{
		m_minTimeElapsed = true;
		if (m_minCountSatisifed)
		{
			AdvancePage();
		}
	}
}