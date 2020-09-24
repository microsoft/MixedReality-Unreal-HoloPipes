// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PPipeGrid.h"
#include "TutorialTypes.h"
#include <memory>
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "PTutorialInstance.generated.h"

enum class TutorialState
{
	None,
	Initialized,
	Running,
	Paused,
	Stopped
};

UENUM(BlueprintType)
enum class ETutorialButton : uint8
{
	None,
	Next,
	Dismiss,
};

UCLASS()
class HOLOPIPES_API APTutorialInstance : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APTutorialInstance();

	void Initialize(APPipeGrid* grid, std::shared_ptr<TutorialLevel> tutorial);
	void StartTutorial();
	void StopTutorial();
	
	bool GetShouldGenerateLevel();
	int32 GetGridSize();

protected:
	
	UPROPERTY(EditAnywhere, Category = "Classes")
	TSubclassOf<AActor> ArrowClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
	APPipeGrid* PipeGrid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrows")
	TArray<AActor*> Arrows;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrows")
	AActor* ToolboxArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Text;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialog")
	bool Anchored;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialog")
	FVector GridSpaceAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialog")
	FVector WorldSpaceOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Dialog")
	ETutorialButton Button;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialog")
	FTimerHandle MinTimeHandle;

	UFUNCTION(BlueprintImplementableEvent)
	void ShowDialog();

	UFUNCTION(BlueprintImplementableEvent)
	void HideDialog();

	UFUNCTION(BlueprintImplementableEvent)
	void CreatedToolbox();

	UFUNCTION(BlueprintCallable)
	void ButtonPressed();

	UFUNCTION(BlueprintCallable)
	void MoveGamePressed();

	UFUNCTION(BlueprintCallable)
	void MenuShown();

	UFUNCTION(BlueprintCallable)
	void PauseTutorial();

	UFUNCTION(BlueprintCallable)
	void ResumeTutorial();

	UFUNCTION()
	void ToolboxMoved();

	UFUNCTION()
	void HandlePipeGrasped(FPipeGridCoordinate graspLocation);

	UFUNCTION()
	void HandlePipeDropped(FPipeGridCoordinate dropLocation);

	UFUNCTION()
	void HandleRotateHandleShown(FPipeGridCoordinate pipeLocation, EInteractionAxis axis);

	UFUNCTION()
	void MinTimeElapsed();

	void AdvancePage();
	void ShowPage();
	void ClosePage();

	void CreateArrows();
	void ClearArrows();
	void PlaceToolboxArrow();

	void PrepareGoals();
	void ClearGoals();

	TutorialState m_state = TutorialState::None;
	std::shared_ptr<TutorialLevel> m_tutorial;

	int32 m_currentPageIndex = 0;
	TutorialPage* m_currentPage = nullptr;

	bool m_moveGameGoal = false;
	bool m_menuGoal = false;
	bool m_graspedGoal = false;
	bool m_droppedGoal = false;
	bool m_rotateHandleGoal = false;

	bool m_minTimeElapsed = false;
	bool m_minCountSatisifed = false;
	int32 m_handleCount = 0;
};
