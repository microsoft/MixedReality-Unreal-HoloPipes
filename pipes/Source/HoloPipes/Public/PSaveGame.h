// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PPipe.h"
#include <memory>

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <windows.h>
#include "SavedGameManager.h"
#include <wrl/client.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#include "PSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FSavedPipe
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    int PipeClass;

    UPROPERTY(VisibleAnywhere)
    EPipeType PipeType;

    UPROPERTY(VisibleAnywhere)
    FPipeGridCoordinate PipeCoordinate;

    UPROPERTY(VisibleAnywhere)
    int PipeConnections;
};

/**
 * 
 */
UCLASS()
class HOLOPIPES_API UPSaveGame : public USaveGame
{
public:
    GENERATED_BODY()

    UPSaveGame();

    UPROPERTY(VisibleAnywhere)
    FString Descriptor;

    UPROPERTY(VisibleAnywhere)
    int32 Version;

    UPROPERTY(VisibleAnywhere)
    int32 CurrentLevel;

    UPROPERTY(VisibleAnywhere)
    TArray<FSavedPipe> PlacedPipes;

    UPROPERTY(VisibleAnywhere)
    uint32 GameFlags;

    UPROPERTY(VisibleAnywhere)
    FPipeGridCoordinate ToolboxCoordinate;

    UPROPERTY(VisibleAnywhere)
    int32 GameScore;

    UPROPERTY(VisibleAnywhere)
    int32 LevelScore;

    UPROPERTY(VisibleAnywhere)
    TArray<uint8> CompletedLevels;

    UPROPERTY(VisibleAnywhere)
    int32 PaidStars;
};

UENUM(BlueprintType)
enum class ESaveState : uint8
{
    None,
    Pending,
    Canceled,
    Complete,
    Failed
};

class SaveManager
{
public:

    // Create a new UPSaveGame with default values
    static UPSaveGame* CreateSavedGame();

    bool Initialize();

    bool CanSaveNow();

    bool CanImportExportNow();

    // Asynchronously saves the provided game. Will fail if a file operation is already in progress
    bool SaveGameAsync(UPSaveGame* pToSave, bool userSelectedPath);

    // Asynchronously loads a game, optionally from a user provided path
    bool LoadGameAsync(bool userSelectedPath);

    // Return the state of any loading operation
    ESaveState GetLoadingState();

    // Return the state of any saving operation
    ESaveState GetSavingState();

    // Returns the game loaded at startup, or a default saved game if
    // no saved game exists. The game is loaded asynchronously; GetLoadedGame
    // returns nullptr until the load is complete
    UPSaveGame* GetLoadedGame();

    // Wait for all pending operations to complete
    void WaitForCompletion();

private:

    Microsoft::WRL::ComPtr<ISavedGameManager> m_spManager;
    Microsoft::WRL::ComPtr<ISavedGame> m_spLoading;
    Microsoft::WRL::ComPtr<ISavedGame> m_spSaving;
    SavedGamePathType m_defaultPath = SavedGamePathType::LocalState;
};
