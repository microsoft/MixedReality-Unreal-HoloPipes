// Fill out your copyright notice in the Description page of Project Settings.


#include "PPipesGameMode.h"
#include "Engine/world.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "TutorialParser.h"

const float DirtySecondsBeforeSave = 120.0f;
const int CostToSkipLevel = 20;

enum class SaveGameFlags
{
    None = 0x00,

    // Target cursor is enabled
    TargetCursor = 0x01,

    // Pipe Labels are enabled
    PipeLabels = 0x02,

    // We have a stored toolbox coordinate
    ToolboxCoordinate = 0x04,

    // The single shot Show Cursor confirmation has been shown
    ShowCursorConfirmation = 0x08,

    // The single shot Show Labels confirmation has been shown
    ShowLabelsConfirmation = 0x10,

    // The single shot Move Game confirmation has been shown
    MoveGameConfirmation = 0x20,

    // The single shot Recall Game confirmation has been shown
    RecallGameConfirmation = 0x40
};

DEFINE_ENUM_FLAG_OPERATORS(SaveGameFlags)

APPipesGameMode::APPipesGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

	PipeGrid = nullptr;
    Tutorial = nullptr;

	LevelOptions = {};

	GenerateLevelNumber = 0;
	GenerateLevelSolution = false;

    GenerateStraightCost = 10;
    GenerateCornerCost = 11;

    Level = 0;
    Score = 0;

    PercentTarget3Stars = 1.25f;
    PercentTarget2Stars = 1.75f;

    m_applyFlags = false;
    m_saveGameFlags = SaveGameFlags::None;

    SetDefaultRules();
    SetDefaultTutorials();
}

void APPipesGameMode::SetDefaultRules()
{
    LevelRules =
    {

        // PlaySpaceSize
        {
            /* DefaultValue  */ 3,
            /* InitialLevel  */ 35,
            /* InitialValue  */ 4,
            /* IncreaseBy    */ 1,
            /* IncreaseEvery */ 80,
            /* MaxValue      */ 8 // Tops out at 395
        },

        // PipesMax
        {
            /* DefaultValue  */ 2,
            /* InitialLevel  */ 35,
            /* InitialValue  */ 3,
            /* IncreaseBy    */ 1,
            /* IncreaseEvery */ 40,
            /* MaxValue      */ PipeClassCount - 1 // Tops out at 395
        },

        // Junctions
        {
            /* DefaultValue  */ 0,
            /* InitialLevel  */ 5,
            /* InitialValue  */ 1,
            /* IncreaseBy    */ 1,
            /* IncreaseEvery */ 40,
            /* MaxValue      */ 11 // Tops out at 405
        },

        // Blocks
        {
            /* DefaultValue  */ 0,
            /* InitialLevel  */ 15,
            /* InitialValue  */ 2,
            /* IncreaseBy    */ 1,
            /* IncreaseEvery */ 40,
            /* MaxValue      */ 12 // Tops out at 415
        },

        // Fixed
        {
            /* DefaultValue  */ 0,
            /* InitialLevel  */ 25,
            /* InitialValue  */ 1,
            /* IncreaseBy    */ 1,
            /* IncreaseEvery */ 40,
            /* MaxValue      */ 11 // Tops at 425
        },
    };
}

void APPipesGameMode::SetDefaultTutorials()
{
    std::vector<std::shared_ptr<TutorialLevel>> tutorials;

    TutorialParser parser;
    parser.Parse(tutorials);

    for (auto& tutorial : tutorials)
    {
        m_tutorials[tutorial->Level] = tutorial;
    }
}

void APPipesGameMode::StartPlay()
{
	Super::StartPlay();
    m_saveManager.Initialize();

    m_waitingOnImport = true;
    m_announceImportResult = false;
    m_saveManager.LoadGameAsync(false /* userSelectedPath */ );
}

bool APPipesGameMode::EnsurePipeGrid()
{
    if (!PipeGrid)
    {
        APMRPawn* pawn = nullptr;

        for (TActorIterator<APMRPawn> it(GetWorld()); it; ++it)
        {
            UE_CLOG(pawn, HoloPipesLog, Warning, L"APPipesGameMode - Found more than one APMRPawn");
            pawn = *it;
        }

        UE_CLOG(!pawn, HoloPipesLog, Warning, L"APPipesGameMode - Found no APMRPawn");

        if (!PipeGrid)
        {
            if (PipeGridClass)
            {
                PipeGrid = GetWorld()->SpawnActor<APPipeGrid>(PipeGridClass);
                if (PipeGrid)
                {
                    PipeGrid->SetPawn(pawn);
                    PipeGrid->OnGridSolved.AddDynamic(this, &APPipesGameMode::GridSolved);
                    PipeGrid->OnGridUpdated.AddDynamic(this, &APPipesGameMode::GridUpdated);
                    PipeGrid->OnToolboxMoved.AddDynamic(this, &APPipesGameMode::ToolboxMoved);
                }
            }
        }
    }

    if (PipeGrid)
    {
        if (m_applyFlags)
        {
            if ((m_saveGameFlags & SaveGameFlags::TargetCursor) == SaveGameFlags::TargetCursor)
            {
                PipeGrid->SetTargetCursorEnabled(true);
            }

            if ((m_saveGameFlags & SaveGameFlags::PipeLabels) == SaveGameFlags::PipeLabels)
            {
                PipeGrid->SetLabelsEnabled(true);
            }

            m_applyFlags = false;
        }
    }

    return (PipeGrid != nullptr);
}

void APPipesGameMode::PlaceToolbox()
{
    if (m_haveUserToolboxCoordinate)
    {
        if (!PipeGrid->TrySetToolboxCoordinate(m_userToolboxCoordinate))
        {
            m_haveUserToolboxCoordinate = false;
            SetSaveNeeded();
        }
    }
}

void APPipesGameMode::RecallToolbox()
{
    if (PipeGrid && PipeGrid->ToolboxAvailable())
    {
        FPipeGridCoordinate toolboxCoordinate = PipeGrid->GetDefaultToolboxCoordinate(LevelOptions.PlaySpaceSize);
        while (!PipeGrid->TrySetToolboxCoordinate(toolboxCoordinate))
        {
            toolboxCoordinate.Y++;
        }

        ToolboxMoved();
    }
}

void APPipesGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (m_waitingForGenerator)
    {
        bool waitingForGenerator = false;
        switch (m_generator.GetStatus())
        {
            case GeneratorStatus::Failed:
            {
                HandleGenerateComplete(false);
                break;
            }

            case GeneratorStatus::Complete:
            {
                GenerateTime = GetWorld()->GetTimeSeconds() - m_generateStart;

                EnsurePipeGrid();

                if (PipeGrid)
                {
                    PipeGrid->Clear();

                    std::vector<PipeSegmentGenerated> placeFromToolbox;

                    for (int i = 0; i < m_pipesToPlace.Num(); i++)
                    {
                        FSavedPipe savedPipe = m_pipesToPlace[i];

                        PipeSegmentGenerated newPipe = {};
                        newPipe.Type = savedPipe.PipeType;
                        newPipe.PipeClass = savedPipe.PipeClass;
                        newPipe.Connections = (PipeDirections)savedPipe.PipeConnections;
                        newPipe.Location = savedPipe.PipeCoordinate;

                        placeFromToolbox.push_back(newPipe);
                    }

                    m_pipesToPlace.Reset();

                    if (GenerateLevelSolution)
                    {
                        PipeGrid->InitializeToolbox(std::vector<PipeSegmentGenerated>(), LevelOptions.PlaySpaceSize);
                        PipeGrid->AddPipes(m_generator.VirtualPipes, AddPipeOptions::None);
                    }
                    else
                    {
                        PipeGrid->InitializeToolbox(m_generator.VirtualPipes, LevelOptions.PlaySpaceSize);
                    }

                    PipeGrid->AddPipes(m_generator.RealizedPipes, AddPipeOptions::Fixed);

                    if (placeFromToolbox.size() > 0)
                    {
                        PipeGrid->AddPipes(placeFromToolbox, AddPipeOptions::FromToolbox);
                    }

                    PipeGrid->FinalizeLevel();
                    PlaceToolbox();

                    if (Tutorial)
                    {
                        Tutorial->StartTutorial();
                    }
                }

                if (m_generateRequested)
                {
                    GenerateLevelInternal(GenerateLevelNumber, GenerateLevelSolution);
                    waitingForGenerator = true;
                    m_generateRequested = false;
                }

                HandleGenerateComplete(true);

                break;
            }

            default:
            {
                waitingForGenerator = true;
            }
        }

        m_waitingForGenerator = waitingForGenerator;
    }

    if (!m_waitingForGenerator && m_saveNeeded && (GetWorld()->GetTimeSeconds() - m_saveNeededTime) >= DirtySecondsBeforeSave)
    {
        // We've been dirty long enough to warrant a save
        SaveGame();
    }

    if (m_waitingOnImport)
    {
        ESaveState state = m_saveManager.GetLoadingState();
        if (state != ESaveState::Pending)
        {
            m_waitingOnImport = false;

            if (state == ESaveState::None || state == ESaveState::Complete ||
                (state == ESaveState::Failed && !m_announceImportResult))
            {
                // For import, we generate the saved level on None or Complete. We
                // also generate on Failed if the import wasn't user initiated (ie, initial level 
                // at startup)
                GenerateSavedLevel();
            }

            if (m_announceImportResult)
            {
                // For import, we treat None as Complete
                OnImportComplete(state == ESaveState::None ? ESaveState::Complete : state);
            }
        }
    }

    if (m_waitingOnExport)
    {
        ESaveState state = m_saveManager.GetSavingState();
        if (state != ESaveState::Pending)
        {
            m_waitingOnExport = false;

            if (m_announceExportResult)
            {
                // For export, we treat None as Failed
                OnExportComplete(state == ESaveState::None ? ESaveState::Failed : state);
            }
        }
    }
}

void APPipesGameMode::HandleGenerateComplete(bool success)
{
    if (success)
    {
        SetRecordedScoreForLevel(Level, 0);
        SaveGame();
    }

    OnGenerateComplete(success);
}

void APPipesGameMode::GenerateSavedLevel()
{
    if (!m_waitingForGenerator)
    {
        int savedLevel = 1;

        UPSaveGame* savedGame = m_saveManager.GetLoadedGame();
        savedLevel = savedGame ? savedGame->CurrentLevel : 1;
        m_saveGameFlags = savedGame ? (SaveGameFlags)savedGame->GameFlags : SaveGameFlags::None;
        m_haveUserToolboxCoordinate = ((m_saveGameFlags & SaveGameFlags::ToolboxCoordinate) == SaveGameFlags::ToolboxCoordinate);
        if (m_haveUserToolboxCoordinate)
        {
            m_userToolboxCoordinate = savedGame->ToolboxCoordinate;
        }

        m_applyFlags = true;

        m_pipesToPlace = std::move(savedGame->PlacedPipes);
        Score = savedGame->GameScore;
        m_paidStars = savedGame->PaidStars;

        CompletedLevels = savedGame->CompletedLevels;
        if (CompletedLevels.Num() < savedLevel)
        {
            CompletedLevels.AddZeroed(savedLevel - CompletedLevels.Num());
        }

        OnScoreChanged();

        GenerateLevelInternal(savedLevel, false /*buildSolution*/);
        m_levelScore = savedGame->LevelScore;

    }
}

void APPipesGameMode::GenerateLevel(int32 newLevel, bool buildSolution)
{
	if (!m_waitingForGenerator)
	{
		GenerateLevelInternal(newLevel, buildSolution);
	}
}

void APPipesGameMode::GenerateLevelInternal(int32 newLevel, bool buildSolution)
{
    Level = std::max(newLevel, 1);
    GenerateLevelNumber = Level;
    bool hasSeed = false;
    int32 seed = 0;

    StopTutorial();

    m_waitingForGenerator = true;

    m_levelScore = 0;
    if (PipeGrid)
    {
        PipeGrid->ClearCurrentLevelScore();
    }

    auto it = m_tutorials.find(Level);
    if (it != m_tutorials.end())
    {

        EnsurePipeGrid();

        if (PipeGrid && TutorialClass)
        {
            Tutorial = GetWorld()->SpawnActor<APTutorialInstance>(TutorialClass);
            if (Tutorial)
            {
                Tutorial->Initialize(PipeGrid, it->second);

                m_waitingForGenerator = Tutorial->GetShouldGenerateLevel();

                if (m_waitingForGenerator)
                {
                    hasSeed = it->second->HasSeed;
                    seed = hasSeed ? it->second->Seed : 0;
                }
                else
                {
                    // m_pipesToPlace is ignored for Tutorial levels with a specified starting state.
                    // Clear it out so it doesn't pollute a later level.
                    m_pipesToPlace.Empty();

                    PipeGrid->Clear();

                    GenerateTime = 0;
                    Tutorial->StartTutorial();

                    if (PipeGrid->ToolboxAvailable())
                    {
                        PlaceToolbox();
                    }

                    LevelOptions = {};
                    LevelOptions.Level = Level;
                    LevelOptions.PlaySpaceSize = Tutorial->GetGridSize();

                    HandleGenerateComplete(true);
                }
            }
        }
    }

    if (m_waitingForGenerator)
    {

        // Either this isn't a tutorial, or it's just text over
        // the generated level. Kick off the generation now.

        if (PipeGrid)
        {
            PipeGrid->SetToolboxEnabled(true);
            PipeGrid->SetDragEnabled(true);
            PipeGrid->SetRotateEnabled(true);
        }

        LevelOptions = {};
        BuildOptionsForLevel(Level, LevelOptions);

        if (hasSeed)
        {
            // We build the LevelOptions for the actual Level (to get appropriate options), and then
            // use the seed to change the layout of the generated level
            LevelOptions.Level = seed;
        }

        GenerateLevelSolution = buildSolution;

        m_generateStart = GetWorld()->GetTimeSeconds();
        m_generator.GenerateLevel(LevelOptions);
    }
}

#if WITH_EDITOR
void APPipesGameMode::PostEditChangeProperty(FPropertyChangedEvent& propertyChangedEvent)
{
	Super::PostEditChangeProperty(propertyChangedEvent);

	if (propertyChangedEvent.Property != nullptr)
	{
		FName propertyName = propertyChangedEvent.Property->GetFName();

		if (propertyName == GET_MEMBER_NAME_CHECKED(APPipesGameMode, GenerateLevelNumber) ||
			propertyName == GET_MEMBER_NAME_CHECKED(APPipesGameMode, GenerateLevelSolution) ||
            propertyName == GET_MEMBER_NAME_CHECKED(APPipesGameMode, GenerateStraightCost) ||
            propertyName == GET_MEMBER_NAME_CHECKED(APPipesGameMode, GenerateCornerCost))
		{
            if (!m_waitingForGenerator)
            {
                GenerateLevel(GenerateLevelNumber, GenerateLevelSolution);
            }
            else
            {
                m_generateRequested = true;
            }
		}
	}
}
#endif

void APPipesGameMode::BuildOptionsForLevel(uint32 level, FGenerateOptions& options)
{
	options = {};

    options.Level = level;
    options.PlaySpaceSize = LevelRules.PlaySpaceSize.Compute(level);
    options.MaxNumPipes = LevelRules.PipesMax.Compute(level);
    options.MaxJunctions = LevelRules.JunctionsMax.Compute(level);
    options.MaxFixed = LevelRules.FixedMax.Compute(level);
    options.MaxBlocks = LevelRules.BlocksMax.Compute(level);

    options.StraightCost = GenerateStraightCost;
    options.CornerCost = GenerateCornerCost;
}

bool APPipesGameMode::CanSweepLevel()
{
    return PipeGrid && PipeGrid->GetAnyPlacedPipesDisconnected() && PipeGrid->ToolboxAvailable();
}

void APPipesGameMode::SweepLevel()
{
    if (PipeGrid)
    {
        PipeGrid->ReturnDisconnectedToToolbox();
    }
}

void APPipesGameMode::GridSolved()
{
    int32 effectiveLevelScore = m_levelScore;
    m_levelScore = 0;

    int32 levelScoreTarget = 0;

    if (PipeGrid)
    {
        effectiveLevelScore += PipeGrid->GetCurrentLevelScore();
        PipeGrid->ClearCurrentLevelScore();

        // We never offer more than 1 star when generating a solution
        if (!GenerateLevelSolution)
        {
            levelScoreTarget = PipeGrid->GetActionablePipesCount();
        }
    }
    
    int target2Stars = 0;
    if (PercentTarget2Stars > 0)
    {
        target2Stars = FMath::RoundToInt((float)levelScoreTarget * PercentTarget2Stars);
    }

    int target3Stars = 0;
    if (PercentTarget3Stars < PercentTarget2Stars && PercentTarget3Stars > 0)
    {
        target3Stars = FMath::RoundToInt((float)levelScoreTarget * PercentTarget3Stars);
    }

    int newPoints = 1;
    if (effectiveLevelScore <= target3Stars)
    {
        if (PipeGrid && !PipeGrid->IsToolboxEmpty())
        {
            // We award a bonus star if the level is finished with a score of 3 AND
            // the level was solved with pieces left over
            newPoints = 4;
        }
        else
        {
            newPoints = 3;
        }
    }
    else if (effectiveLevelScore <= target2Stars)
    {
        newPoints = 2;
    }

    int oldScore = GetRecordedScoreForLevel(Level);
    if (newPoints > oldScore)
    {
        Score += (newPoints - oldScore);
        SetRecordedScoreForLevel(Level, newPoints);
    }

    StopTutorial();

    OnLevelSolved(newPoints);
}

void APPipesGameMode::GridUpdated()
{
    SetSaveNeeded();
}

void APPipesGameMode::SetSaveNeeded()
{
    if (!m_saveNeeded)
    {
        m_saveNeeded = true;
        m_saveNeededTime = GetWorld()->GetTimeSeconds();
    }
}

void APPipesGameMode::ToolboxMoved()
{
    m_haveUserToolboxCoordinate = PipeGrid->GetToolboxCoordinate(&m_userToolboxCoordinate);
    SetSaveNeeded();
}

UPSaveGame* APPipesGameMode::BuildSaveGame()
{
    UPSaveGame* newSaveGame = SaveManager::CreateSavedGame();
    newSaveGame->CurrentLevel = Level;

    if (PipeGrid)
    {
        TArray<FSavedPipe> pipesToSave;

        std::vector<PipeSegmentGenerated> placed;
        PipeGrid->GetCurrentPlaced(placed);

        for (auto& pipe : placed)
        {
            FSavedPipe savedPipe = {};
            savedPipe.PipeType = pipe.Type;
            savedPipe.PipeClass = pipe.PipeClass;
            savedPipe.PipeConnections = (int)pipe.Connections;
            savedPipe.PipeCoordinate = pipe.Location;

            pipesToSave.Add(savedPipe);
        }

        if (pipesToSave.Num() > 0)
        {
            newSaveGame->PlacedPipes = std::move(pipesToSave);
        }
    }

    SaveGameFlags flags = m_saveGameFlags;

    newSaveGame->GameScore = Score;
    newSaveGame->LevelScore = m_levelScore;
    newSaveGame->PaidStars = m_paidStars;

    // Ignore the previous TargetCursor and PipeLabels state. Take the most up to date state,
    // if we have it
    flags &= ~(SaveGameFlags::TargetCursor | SaveGameFlags::PipeLabels);
    if (PipeGrid)
    {
        if (PipeGrid->GetTargetCursorEnabled())
        {
            flags |= SaveGameFlags::TargetCursor;
        }
        if (PipeGrid->GetLabelsEnabled())
        {
            flags |= SaveGameFlags::PipeLabels;
        }

        newSaveGame->LevelScore += PipeGrid->GetCurrentLevelScore();
    }

    // Ignore the previous toolbox coordinate state, taking the latest instead
    flags &= ~(SaveGameFlags::ToolboxCoordinate);
    if (m_haveUserToolboxCoordinate)
    {
        flags |= SaveGameFlags::ToolboxCoordinate;
        newSaveGame->ToolboxCoordinate = m_userToolboxCoordinate;
    }

    newSaveGame->GameFlags = (uint32)(flags);

    newSaveGame->CompletedLevels = CompletedLevels;

    return newSaveGame;
}

void APPipesGameMode::SaveGame()
{
    if (m_saveManager.CanSaveNow())
    {
        UPSaveGame* newSaveGame = BuildSaveGame();

        if (m_saveManager.SaveGameAsync(newSaveGame, false /* userSelectedPath */))
        {
            m_saveNeeded = false;

            m_waitingOnExport = true;
            m_announceExportResult = false;
        }
        else
        {
            SetSaveNeeded();
        }
    }
    else
    {
        SetSaveNeeded();
    }
}

bool APPipesGameMode::GetSingleShotAndSet(ESingleShotState state)
{
    SaveGameFlags singleShotFlag = SaveGameFlags::None;

    switch (state)
    {
        case ESingleShotState::ShowCursorConfirmation:
            singleShotFlag = SaveGameFlags::ShowCursorConfirmation;
            break;
        case ESingleShotState::ShowLabelsConfirmation:
            singleShotFlag = SaveGameFlags::ShowLabelsConfirmation;
            break;
        case ESingleShotState::MoveGameConfirmation:
            singleShotFlag = SaveGameFlags::MoveGameConfirmation;
            break;
        case ESingleShotState::RecallGameConfirmation:
            singleShotFlag = SaveGameFlags::RecallGameConfirmation;
            break;
    }

    bool wasSet = ((m_saveGameFlags & singleShotFlag) != SaveGameFlags::None);
    m_saveGameFlags |= singleShotFlag;

    if (singleShotFlag != SaveGameFlags::None && !wasSet)
    {
        SetSaveNeeded();
    }

    return wasSet;
}

void APPipesGameMode::FlushGameStateSync()
{
    // Block on pending save operations
    m_saveManager.WaitForCompletion();

    if (m_saveNeeded)
    {
        // And if we're dirty, schedule another save operation (and wait on it)
        SaveGame();
        m_saveManager.WaitForCompletion();
    }
}

bool APPipesGameMode::CanImportExportGame()
{
    return m_saveManager.CanImportExportNow();
}

void APPipesGameMode::ExportGame()
{
    if (CanImportExportGame())
    {
        UPSaveGame* newSaveGame = BuildSaveGame();
        if (m_saveManager.SaveGameAsync(newSaveGame, true /* userSelectedPath */))
        {
            m_waitingOnExport = true;
            m_announceExportResult = true;
        }
        else
        {
            OnExportComplete(ESaveState::Failed);
        }
    }
}

void APPipesGameMode::ImportGame()
{
    if (CanImportExportGame())
    {
        if (m_saveManager.LoadGameAsync(true /* userSelectedPath */))
        {
            m_waitingOnImport = true;
            m_announceImportResult = true;
        }
        else
        {
            OnImportComplete(ESaveState::Failed);
        }
    }
}

int APPipesGameMode::GetRecordedScoreForLevel(int32 level)
{
    int score = 0;
    if (level > 0)
    {
        const int32 entry = level - 1;
        if (entry < CompletedLevels.Num())
        {
            score = static_cast<int>(CompletedLevels[entry]);
        }
    }

    return score;
}

void APPipesGameMode::SetRecordedScoreForLevel(int32 level, int score)
{
    uint8 newScore = static_cast<uint8>(score);

    if (level > 0)
    {
        if (level >= CompletedLevels.Num())
        {
            CompletedLevels.AddZeroed(level - CompletedLevels.Num());
        }

        const int32 entry = level - 1;
        if (CompletedLevels[entry] < newScore)
        {
            CompletedLevels[entry] = newScore;
        }
    }
}

void APPipesGameMode::StopTutorial()
{
    if (Tutorial != nullptr)
    {
        Tutorial->StopTutorial();
        Tutorial->Destroy();
        Tutorial = nullptr;
    }
}

bool APPipesGameMode::CanSkipLevel()
{
    return Score >= CostToSkipLevel;
}

void APPipesGameMode::SkipLevel()
{
    if (Score >= CostToSkipLevel)
    {
        Score -= CostToSkipLevel;
        m_paidStars += CostToSkipLevel;
        CompletedLevels.AddZeroed();
        SetSaveNeeded();
    }
}