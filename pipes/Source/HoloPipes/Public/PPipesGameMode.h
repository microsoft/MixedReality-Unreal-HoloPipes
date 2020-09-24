// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PPipe.h"
#include "PPipeGrid.h"
#include "PTutorialInstance.h"
#include "LevelGenerator.h"
#include "PSaveGame.h"
#include "TutorialTypes.h"
#include "map"
#include "PPipesGameMode.generated.h"

enum class SaveGameFlags;

UENUM(BlueprintType)
enum class ESingleShotState : uint8
{
    None,
    ShowCursorConfirmation,
    ShowLabelsConfirmation,
    MoveGameConfirmation,
    RecallGameConfirmation
};

USTRUCT(BlueprintType)
struct FGeneratorRule
{
    GENERATED_BODY()

    // Static value used before InitialLevel
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DefaultValue;

    // First level we start dynamically computing value
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InitialLevel;

    // Value used as of InitialLevel
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InitialValue;

    // The value we increase by
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 IncreaseBy;

    // How many levels we maintain the same value
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 IncreaseEvery;

    // The static maximum value
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxValue;

    int32 Compute(int32 level)
	{
        int32 computed = DefaultValue;

        if (level >= InitialLevel)
        {
            computed = InitialValue;
            if (computed < MaxValue)
            {
                computed = InitialValue + (IncreaseBy * ((level - InitialLevel)) / IncreaseEvery);
                computed = std::min(std::max(InitialValue, computed), MaxValue);
            }

        }

		return computed;
	}
};

USTRUCT(BlueprintType)
struct FGeneratorRules
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneratorRule PlaySpaceSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneratorRule PipesMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneratorRule JunctionsMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneratorRule BlocksMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneratorRule FixedMax;
};

/**
 * 
 */
UCLASS()
class HOLOPIPES_API APPipesGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:

	APPipesGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent);
#endif

protected:

	void BuildOptionsForLevel(uint32 level, FGenerateOptions& options);

    void SetDefaultRules();
    void SetDefaultTutorials();

    bool EnsurePipeGrid();

    UPSaveGame* BuildSaveGame();
    void SaveGame();

    void PlaceToolbox();

    void SetSaveNeeded();

    void HandleGenerateComplete(bool success);

    void GenerateSavedLevel();

	UPROPERTY(EditAnywhere, Category = "Classes")
	TSubclassOf<APPipeGrid> PipeGridClass;

    UPROPERTY(EditAnywhere, Category = "Classes")
    TSubclassOf<APTutorialInstance> TutorialClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
    int32 GenerateLevelNumber;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	bool GenerateLevelSolution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
    int32 GenerateStraightCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
    int32 GenerateCornerCost;

    UPROPERTY(EditAnywhere, Category = "Generator")
    FGeneratorRules LevelRules;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedLevel")
    int32 Level;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedLevel")
	FGenerateOptions LevelOptions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedLevel")
    APTutorialInstance* Tutorial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeneratedLevel")
    float GenerateTime;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    APPipeGrid* PipeGrid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    int32 Score;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    TArray<uint8> CompletedLevels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
    float PercentTarget3Stars;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
    float PercentTarget2Stars;
    	
	UFUNCTION(BlueprintCallable)
	void GenerateLevel(int32 newLevel, bool buildSolution);
	void GenerateLevelInternal(int32 newLevel, bool buildSolution);

    UFUNCTION(BlueprintCallable)
    bool CanSweepLevel();

    UFUNCTION(BlueprintCallable)
    void SweepLevel();

    UFUNCTION(BlueprintCallable)
    bool CanImportExportGame();

    UFUNCTION(BlueprintCallable)
    void ExportGame();

    UFUNCTION(BlueprintCallable)
    void ImportGame();

    UFUNCTION(BlueprintCallable)
    bool GetSingleShotAndSet(ESingleShotState state);

    UFUNCTION(BlueprintCallable)
    void FlushGameStateSync();

    UFUNCTION(BlueprintCallable)
    void RecallToolbox();

    UFUNCTION(BlueprintCallable)
    void StopTutorial();

    UFUNCTION(BlueprintCallable)
    bool CanSkipLevel();

    UFUNCTION(BlueprintCallable)
    void SkipLevel();

	UFUNCTION(BlueprintImplementableEvent)
	void OnGenerateComplete(bool success);
    
    // Indicates that the levels has been solved and that indicated number of points should be
    // celebrated with fanfare
    UFUNCTION(BlueprintImplementableEvent)
    void OnLevelSolved(int32 newPoints);

    // Indicates that the score should be changed without fanfare (such as when loading a saved game)
    UFUNCTION(BlueprintImplementableEvent)
    void OnScoreChanged();

    UFUNCTION(BlueprintImplementableEvent)
    void OnImportComplete(ESaveState state);

    UFUNCTION(BlueprintImplementableEvent)
    void OnExportComplete(ESaveState state);

    UFUNCTION()
    void GridSolved();

    UFUNCTION()
    void GridUpdated();

    UFUNCTION()
    void ToolboxMoved();

	LevelGenerator m_generator;
	bool m_waitingForGenerator = false;
    TArray<FSavedPipe> m_pipesToPlace;

    bool m_saveNeeded = false;
    float m_saveNeededTime = 0;

    float m_generateStart = 0;

    SaveManager m_saveManager;

    std::map<int32, std::shared_ptr<TutorialLevel>> m_tutorials;

    bool m_applyFlags;
    SaveGameFlags m_saveGameFlags;

    bool m_generateRequested = false;
   
    bool m_haveUserToolboxCoordinate = false;
    FPipeGridCoordinate m_userToolboxCoordinate = FPipeGridCoordinate::Zero;

    int32 m_levelScore = 0;
    int32 m_paidStars = 0;

    bool m_waitingOnImport = false;
    bool m_announceImportResult = false;

    bool m_waitingOnExport = false;
    bool m_announceExportResult = false;

    int GetRecordedScoreForLevel(int32 level);
    void SetRecordedScoreForLevel(int32 level, int score);
};

