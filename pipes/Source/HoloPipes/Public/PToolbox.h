// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPipe.h"
#include "LevelGenerator.h"
#include <vector>
#include "PToolbox.generated.h"


UENUM(BlueprintType)
enum class EToolboxFocus : uint8
{
    None,
    Focused,
    Carried,
};

USTRUCT(BlueprintType)
struct FToolboxContents
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int StraightPipes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int CornerPipes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int JunctionPipes;
};

UCLASS()
class HOLOPIPES_API APToolbox : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APToolbox();

	void Initialize(const std::vector<PipeSegmentGenerated>& pipeSegments);
	void Clear();

    FToolboxContents GetContents() { return Contents; }

    void RemovePipe(EPipeType toRemove);
    void AddPipe(EPipeType toAdd);
    int CountRemaining(EPipeType type);

    void SetFocus(EToolboxFocus newFocus);
    void SetDropTarget(FVector dropTarget);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FToolboxContents Contents;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EToolboxFocus Focus;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector DropTarget;

	UFUNCTION(BlueprintImplementableEvent)
	void OnToolboxInitializationComplete();

    UFUNCTION(BlueprintImplementableEvent)
    void OnToolboxContentsChanged();

    UFUNCTION(BlueprintImplementableEvent)
    void OnToolboxFocusChanged();

    UFUNCTION(BlueprintImplementableEvent)
    void OnDropTargetChanged();

    bool m_initializationComplete = false;

};
