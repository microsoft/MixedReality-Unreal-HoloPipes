// Fill out your copyright notice in the Description page of Project Settings.

#include "PSaveGame.h"
#include "Kismet/GameplayStatics.h"

PCWSTR SaveFileName = L"pipes.sav";
PCWSTR ExportFileName = L"HoloPipes Saved Game|pipes.sav";
PCWSTR SaveDescriptor = L"HoloPipes";

/*
Version History
===============

Version 1
---------
Original save huerstics

Version 2
---------
Introduced smarter level generation, including requiring starts/ends be on different sides

Version 3
---------
Started using A* to compute more intelligent paths between start and end.
Added ToolboxCoordinate
Added Game Flags
Added Score

Version 4
---------
Updated generation rules

*/

const INT32 SaveVersion_Current = 4;

UPSaveGame::UPSaveGame()
{
    Descriptor = SaveDescriptor;
    Version = SaveVersion_Current;
    CurrentLevel = 1;
    GameFlags = 0;
    ToolboxCoordinate = FPipeGridCoordinate::Zero;
    GameScore = 0;
    LevelScore = 0;
    PaidStars = 0;
}

UPSaveGame* SaveManager::CreateSavedGame()
{
    return Cast<UPSaveGame>(UGameplayStatics::CreateSaveGameObject(UPSaveGame::StaticClass()));
}

bool SaveManager::Initialize()
{
    bool initialized = m_spManager ? true : false;
    if (!initialized)
    {
        PCWSTR dllPath = SGM_DLL_RELPATH;
        void* dllHandle = FPlatformProcess::GetDllHandle(dllPath);
        if (dllHandle)
        {
            initialized = SUCCEEDED(CreateSavedGameManager(m_spManager.ReleaseAndGetAddressOf()));
        }

        if (initialized)
        {
#if WITH_EDITOR
            // We can't save to LocalState when we're not a UWP, so use the documents folder instead.
            // This is done primarily to enable saved games during development
            m_defaultPath = SavedGamePathType::Documents;
#endif
        }
    }

    return initialized;
}

bool SaveManager::CanSaveNow()
{
    return (m_spManager && GetLoadingState() != ESaveState::Pending && GetSavingState() != ESaveState::Pending);
}


bool SaveManager::CanImportExportNow()
{
#if WITH_EDITOR
    return false;
#else
    return CanSaveNow();
#endif
}

ESaveState SaveManager::GetLoadingState()
{
    if (m_spLoading)
    {
        switch (m_spLoading->GetState())
        {
            case SavedGameState::Pending:
                return ESaveState::Pending;

            case SavedGameState::Failed:
                return ESaveState::Failed;

            case SavedGameState::Complete:
                return ESaveState::Complete;

            case SavedGameState::Canceled:
                return ESaveState::Canceled;
        }
    }

    return ESaveState::None;
}

ESaveState SaveManager::GetSavingState()
{
    if (m_spSaving)
    {
        switch (m_spSaving->GetState())
        {
            case SavedGameState::Pending:
                return ESaveState::Pending;

            case SavedGameState::Failed:
                return ESaveState::Failed;

            case SavedGameState::Complete:
                return ESaveState::Complete;

            case SavedGameState::Canceled:
                return ESaveState::Canceled;
        }
    }

    return ESaveState::None;
}

bool SaveManager::LoadGameAsync(bool userSelectedPath)
{
    bool loadStarted = false;

    if ((userSelectedPath && CanImportExportNow()) || (!userSelectedPath && CanSaveNow()))
    {
        loadStarted =
            SUCCEEDED(m_spManager->LoadGame(userSelectedPath ? ExportFileName : SaveFileName,
                                            userSelectedPath ? SavedGamePathType::UserSelected : m_defaultPath,
                                            m_spLoading.ReleaseAndGetAddressOf()));
    }

    return loadStarted;
}

bool SaveManager::SaveGameAsync(UPSaveGame* pToSave, bool userSelectedPath)
{
    bool saveStarted = false;

    if ((userSelectedPath && CanImportExportNow()) || (!userSelectedPath && CanSaveNow()))
    {
        TArray<byte> bytes;
        if (UGameplayStatics::SaveGameToMemory(pToSave, bytes))
        {
            saveStarted =
                SUCCEEDED(m_spManager->SaveGame(userSelectedPath ? ExportFileName : SaveFileName,
                                                userSelectedPath ? SavedGamePathType::UserSelected : m_defaultPath,
                                                bytes.GetData(),
                                                static_cast<UINT32>(bytes.Num()),
                                                m_spSaving.ReleaseAndGetAddressOf()));
        }
    }

    return saveStarted;
}

UPSaveGame* SaveManager::GetLoadedGame()
{
    UPSaveGame* retval = nullptr;

    if (GetLoadingState() == ESaveState::Complete)
    {
        if (m_spLoading && m_spLoading->GetState() == SavedGameState::Complete)
        {
            const byte* bytes = nullptr;
            UINT32 count = 0;

            if (SUCCEEDED(m_spLoading->GetBytes(&bytes, &count)) && bytes != nullptr && count > 0)
            {
                TArray<byte> byteArray(bytes, count);
                UPSaveGame* temp = Cast<UPSaveGame>(UGameplayStatics::LoadGameFromMemory(byteArray));

                if (temp && temp->Descriptor == SaveDescriptor)
                {
                    if (temp->Version != SaveVersion_Current)
                    {
                        temp->PlacedPipes.Empty();
                    }

                    if (temp->CurrentLevel > 1 && temp->GameScore == 0 && temp->PaidStars == 0)
                    {
                        // If we don't have a saved score, assume two stars per level
                        temp->GameScore = (2 * (temp->CurrentLevel - 1));
                    }

                    if (temp->LevelScore == 0 && temp->PlacedPipes.Num() > 0)
                    {
                        // If we don't have a saved level score, but we have placed pipes, assume one point
                        // per placed pipe
                        temp->LevelScore = temp->PlacedPipes.Num();
                    }

                    if (temp->CurrentLevel > temp->CompletedLevels.Num())
                    {
                        temp->CompletedLevels.AddZeroed(temp->CurrentLevel - temp->CompletedLevels.Num());
                    }

                    int storedScore = 0;
                    for (const auto& completedLevel : temp->CompletedLevels)
                    {
                        storedScore += completedLevel;
                    }

                    storedScore -= temp->PaidStars;
                    storedScore = __max(0, storedScore);

                    // Assume the greater of our stored scores is correct
                    if (storedScore > temp->GameScore)
                    {
                        // GameScore is too small, so just take the new total
                        temp->GameScore = storedScore;
                    }
                    else if (storedScore < temp->GameScore)
                    {
                        // Our completed levels scores is too small, so do a best effort to make them match
                        int diff = (temp->GameScore - storedScore);

                        for (int minStars = 1; minStars <= 4 && diff > 0; minStars++)
                        {
                            for (int i = 0; i < temp->CompletedLevels.Num() - 1 && diff > 0; i++)
                            {
                                if (temp->CompletedLevels[i] < minStars)
                                {
                                    temp->CompletedLevels[i]++;
                                    diff--;
                                }
                            }
                        }

                        if (diff > 0)
                        {
                            // With our completed levels, the GameScore doesn't make any sense, so reduce the game score
                            temp->GameScore -= diff;
                        }
                    }
                }

                retval = temp;
            }
        }
    }

    if (retval == nullptr)
    {
        retval = CreateSavedGame();
    }

    return retval;
}

void SaveManager::WaitForCompletion()
{
    if (m_spLoading && m_spLoading->GetState() == SavedGameState::Pending)
    {
        m_spLoading->WaitForCompletion();
    }

    if (m_spSaving && m_spSaving->GetState() == SavedGameState::Pending)
    {
        m_spSaving->WaitForCompletion();
    }
}