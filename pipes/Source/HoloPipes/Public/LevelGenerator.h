// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include <random>
#include <list>
#include <vector>
#include <set>
#include <algorithm>
#include <HAL/Runnable.h>
#include <HAL/RunnableThread.h>
#include <UObject/ObjectMacros.h>
#include <atomic>
#include "PPipe.h"
#include "LevelGenerator.generated.h"

class RNG
{
public:

	void Init(int seed)
	{
		int seedSeqSeed[] = { seed, ' ', 'L', 'e', 'v', 'e', 'l', ' ', seed + 1, ' ', 's', 'e', 'e', 'd', ' ', seed + 2 };
		std::seed_seq seedSeq(seedSeqSeed, seedSeqSeed + ARRAYSIZE(seedSeqSeed));
		engine.seed(seedSeq);

		// While not strictly necessary, burning through a quick 100 random numbers helps us get to a "more
		// random" sequence of numbers when using mod (the lower bits have more randomess when the engine is
		// "warmed up"
		//
		// Experts disagree on whether this is true, but it's cheap to generate a few numbers and throw them away
		for (int i = 0; i < 100; i++)
		{
			GetInt();
		}
	}

	float GetFloat()
	{
		return (double)engine() / (double)engine.max();
	}

	UINT32 GetInt()
	{
		return engine();
	}

	UINT32 GetInt(UINT32 minInclusive, UINT32 maxExclusive)
	{
		if (maxExclusive <= (minInclusive + 1))
		{
			return minInclusive;
		}

		return (engine() % (maxExclusive - minInclusive)) + minInclusive;
	}

	bool GetBool()
	{
		return GetInt(0, 1) == 1;
	}

	template <class t>
	void Shuffle(std::vector<t>& v)
	{
		std::shuffle(v.begin(), v.end(), engine);
	}


private:

	std::mt19937 engine;
};

enum class GeneratorStatus
{
	Idle,		// The generator hasn't been started
	Generating,	// The generetor is actively generating the last requested level
	Canceling,	// The generator has been asked to stop prematurely
	Failed,		// The generator has finished, but failed to complete a level
	Complete	// The generator has finished a complete level
};

USTRUCT(BlueprintType)
struct FGenerateOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PlaySpaceSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxNumPipes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxJunctions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxFixed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxBlocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StraightCost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CornerCost;
};

/**
 * 
 */
class HOLOPIPES_API LevelGenerator : FRunnable
{
public:

	LevelGenerator();
	virtual ~LevelGenerator();

	bool GenerateLevel(const FGenerateOptions& options);
	void CancelLevel();

	GeneratorStatus GetStatus() { return m_status.load(); }

	std::vector<PipeSegmentGenerated> VirtualPipes;
	std::vector<PipeSegmentGenerated> RealizedPipes;
		
private:

	//
	// FRunnable
	//

	virtual uint32 Run() override;

    struct PipeTemp
    {
        int Class;
        int Junctions;
        int Fixed;
    };

    enum BuildState
    {
        None,
        OpenList,
        ClosedList,
        Committing,
        Committed
    };

	struct PipeSegmentTemp : public PipeSegmentGenerated
	{
		const static PipeSegmentTemp c_Empty;

		bool Fixed = false;
        BuildState State = BuildState::None;

        FPipeGridCoordinate ParentLocation = FPipeGridCoordinate::Zero;
        PipeDirections ParentDirection = PipeDirections::None;

        int PathCost = 0;
        int PredictedCost = 0;

        int TotalCost() const { return PathCost + PredictedCost; }
	};

	void SetStatus(GeneratorStatus status) { m_status.store(status); }

    int ComputePredictedCost(const FPipeGridCoordinate& from, const FPipeGridCoordinate& to, PipeDirections parentToChild, PipeDirections endSide);

	PipeSegmentTemp& GetSegment(const FPipeGridCoordinate& location);
	PipeSegmentTemp& GetSegment(int x, int y, int z);

	void Reset(bool resetThread, bool resetVirtualAndRealizedLists);
    bool FinalizeLevel();

    PipeDirections SideFromCoordinate(const FPipeGridCoordinate& coordinate);
    void BuildStartCandidateList(PipeDirections side);
    void BuildEndCandidateList(PipeDirections half);
    
    bool GenerateBlocks(int count);
	bool GeneratePipe(const PipeTemp& pipe);
    bool GeneratePipe(const PipeTemp& pipe, const FPipeGridCoordinate& startCoordinate, PipeDirections startDirection);
    bool GenerateJunctions(const PipeTemp& pipe);
    bool GenerateJunction(const PipeTemp& pipe);
    bool GenerateFixed(const PipeTemp& pipe);

    bool CompletePipe(const PipeTemp& pipe, const FPipeGridCoordinate& endCoordinate, bool forJunction);

    bool CommitPipe(const PipeTemp& pipe, const FPipeGridCoordinate& end);
    void ResetAStar();
    void RemoveFromOpen(PipeSegmentTemp* victim);
    PipeSegmentTemp* RemoveRandomLeastFromOpen();

    std::vector<PipeTemp> m_pipesToBuild;
    std::vector<PipeSegmentTemp> m_pipeGrid;

    struct PipeSegmentTempCompare
    {
        bool operator() (const PipeSegmentTemp* lhs, const PipeSegmentTemp* rhs) const
        {
            return ((lhs->TotalCost()) < (rhs->TotalCost()));
        }
    };
    std::multiset<PipeSegmentTemp*, PipeSegmentTempCompare> m_openList;
    std::vector<PipeSegmentTemp*> m_closedList;
    std::vector<PipeSegmentTemp*> m_committingList;

	RNG m_rng;

    int m_sideMin = 0;
    int m_sideMax = 0;
    int m_playSpaceSize = 0;
	int m_gridSide = 0; 
	int m_gridSideSquared = 0;
	int m_gridSideCubed = 0;
    int m_straightCost = 0;
    int m_cornerCost = 0;
    std::vector<FPipeGridCoordinate> m_startCandidates;
    std::vector<FPipeGridCoordinate> m_endCandidates;

	std::atomic<GeneratorStatus> m_status;
	volatile bool m_abortExecution = false;

	FRunnableThread* m_thread = nullptr;
};
