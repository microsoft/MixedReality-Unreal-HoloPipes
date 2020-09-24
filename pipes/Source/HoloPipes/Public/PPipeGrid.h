// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPipe.h"
#include "PMRPawn.h"
#include "PToolbox.h"
#include <unordered_map>
#include <functional>
#include "PPipeGrid_PlaceWorld.h"
#include "PPipeGrid_GridHands.h"
#include "PPipeGrid_Cursor.h"
#include "PPipeGrid_Interaction.h"
#include "PPipeGrid_Score.h"
#include "PPipeGrid.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGridSolved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGridUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FToolboxMoved);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPipeGrasped, FPipeGridCoordinate, graspLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPipeDropped, FPipeGridCoordinate, dropLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRotateHandleShown, FPipeGridCoordinate, pipeLocation, EInteractionAxis, axis);

enum class AddPipeOptions
{
    None                = 0x00,
    InToolbox           = 0x01, // For internal use only
    Fixed               = 0x02,
    FromToolbox         = 0x04
}; 

DEFINE_ENUM_FLAG_OPERATORS(AddPipeOptions);

struct GridPipe
{
    APPipe* pipe;
    FPipeGridCoordinate currentLocation;
    int currentPipeClass;
};

UCLASS()
class HOLOPIPES_API APPipeGrid : public AActor
{
	GENERATED_BODY()
	
public:

	// Sets default values for this actor's properties
	APPipeGrid();

    UFUNCTION(BlueprintCallable)
	void Clear();

    void SetPawn(APMRPawn* pawn);

    void InitializeToolbox(const std::vector<PipeSegmentGenerated>& pipesInToolbox, int playableGridSize);
    void AddToToolbox(const std::vector<PipeSegmentGenerated>& pipesInToolbox);
    void RemoveToolbox();

    void AddPipes(const std::vector<PipeSegmentGenerated>& pipes, AddPipeOptions options);
	void AddPipe(const PipeSegmentGenerated& pipeToAdd, AddPipeOptions options);

    void GetPipeInfo(FPipeGridCoordinate gridLocation, EPipeType* pipeType, int* pipeClass, PipeDirections* connections);
    void UpdatePipeConnections(const FPipeGridCoordinate& coordinate);

    void GetCurrentPlaced(std::vector<PipeSegmentGenerated>& realizedPipes);
    bool GetAnyPipesPlaced();
    bool GetAnyPlacedPipesDisconnected();
    int32 GetActionablePipesCount();

    void ReturnDisconnectedToToolbox();

    void SetRotateEnabled(bool enabled) { m_rotateEnabled = enabled; }
    void SetDragEnabled(bool enabled) { m_dragEnabled = enabled; }
    void SetToolboxEnabled(bool enabled) { m_toolboxEnabled = enabled; }

    void SetTargetCursorEnabled(bool enabled) { TargetCursorEnabled = enabled; }
    bool GetTargetCursorEnabled() { return TargetCursorEnabled; }

    bool GetToolboxCoordinate(FPipeGridCoordinate* pCoord);
    bool TrySetToolboxCoordinate(const FPipeGridCoordinate& coord);
    bool IsToolboxEmpty();
    bool ToolboxAvailable();
    bool CoordinateInToolbox(const FPipeGridCoordinate& coordinate);
    FPipeGridCoordinate GetDefaultToolboxCoordinate(int playableGridSize);

    void FinalizeLevel();
    
    UFUNCTION(BlueprintCallable)
    void SetInteractionEnabled(bool enabled);

    UFUNCTION(BlueprintCallable)
    bool GetInteractionEnabled() { return m_hands.InteractionEnabled; }

    UFUNCTION(BlueprintCallable)
    void SetLabelsEnabled(bool enabled);

    UFUNCTION(BlueprintCallable)
    bool GetLabelsEnabled();

    UFUNCTION(BlueprintImplementableEvent)
    void OnCursorUpdated(FCursorState state);

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FGridSolved OnGridSolved;

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FGridUpdated OnGridUpdated;

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FToolboxMoved OnToolboxMoved;

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FPipeGrasped OnPipeGrasped;

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FPipeDropped OnPipeDropped;

    UPROPERTY(BlueprintAssignable, Category = "GridEvents")
    FRotateHandleShown OnRotateHandleShown;

    // For Level Score, lower (fewer moves) is better
    UFUNCTION(BlueprintCallable)
    int32 GetCurrentLevelScore();

    void ClearCurrentLevelScore();

    // ----------------------------------------------
    // Location Helpers (PPipeGrid_LocationHelpers)
    // ----------------------------------------------

    static FVector WorldLocationToTransformLocation(const FVector& worldLocation, const FTransform& toWorldTransform);
    static FVector TransformLocationToWorldLocation(const FVector& transformLocation, const FTransform& toWorldTransform);

    FVector WorldLocationToGridLocation(const FVector& worldLocation);
    FVector GridLocationToWorldLocation(const FVector& gridLocation);

    FPipeGridCoordinate GridLocationToGridCoordinate(const FVector& gridLocation);
    FVector GridCoordinateToGridLocation(const FPipeGridCoordinate& gridCoordinate);

    FTransform WorldTransformToGridTransform(const FTransform& worldTransform);

protected:

    void ChangeInteractionEnabledForHand(GridHand& hand);
    void ClearHand(GridHand& hand);
    void ClearHandDebounce(GridHandState& handState);
    void ClearHandFocusHistory(GridHandFocus& handFocus);

    bool CheckForSolution();
    bool ResolveGridState();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    UFUNCTION()
    void HandlePawnGazeUpdate(FGazeUpdate update);

    UFUNCTION()
    void HandlePawnGestureUpdate(FGestureUpdate update);
    void HandlePawnGestureUpdateForHand(EFingerHand fingerHand, const FFingerGestureUpdate& update, GridHand& hand);
    void HandleNoneGestureUpdate(const FFingerGestureUpdate& update, GridHand& hand);
    void HandleFocusedGestureUpdate(const FFingerGestureUpdate& update, GridHand& hand);
    void UpdateFocus(GridHand& hand);

    UFUNCTION(BlueprintCallable)
    bool ToggleDebugFocus();

    GridPipe* GetActionablePipe(FPipeGridCoordinate gridLocation);

    bool PipeConnected(const FPipeGridCoordinate& pipeCoordinate);
    bool PipeConnected(const GridPipe& pipe, PipeDirections direction);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
	float CellSize;

	UPROPERTY(EditAnywhere, Category = "Grid")
	TSubclassOf<APPipe> PipeClass;

    UPROPERTY(EditAnywhere, Category = "Grid")
    TSubclassOf<APToolbox> ToolboxClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    APToolbox* Toolbox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    APMRPawn* Pawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool TargetCursorEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    float MinGridScale;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid")
    float MaxGridScale;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debounce")
    float ShowRotateHandlesTimeout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debounce")
    float LoseRotateFocusTimeout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debounce")
    float ChangeRotateAxisTimeout;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debounce")
    float FocusGutterRatio;

	std::unordered_map<FPipeGridCoordinate, GridPipe, std::function<size_t(const FPipeGridCoordinate&)>> m_pipesGrid;
    std::list<GridPipe*> m_openList;

    GridHands m_hands;

    bool m_labelsEnabled;

#if !UE_BUILD_SHIPPING
    bool m_debugFocus = false;
#endif

    // ----------------------------------------------
    // Toolbox (PPipeGrid_Toolbox)
    // ----------------------------------------------

    void InitializeToolboxPipes();
    
    bool TryHandleStartToolboxDrag(GridHandState& handState); 
    void HandleToolboxGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState);
    void HandleToolboxDragLost(GridHandState& handState);

    bool IsToolboxDragging();
    bool IsToolboxFocused();
    bool DoesToolboxFitAt(FPipeGridCoordinate root);

    void LiftToolbox();
    void DropToolbox(FPipeGridCoordinate root);

    void EnsureToolboxPipes();
    void SpawnInToolbox(EPipeType toolboxPipe);
    bool ToolboxCoordinateForType(EPipeType type, FPipeGridCoordinate* pCoord);
    bool ToolboxCoordinateForType(EPipeType type, FPipeGridCoordinate root, FPipeGridCoordinate* pCoord);

    bool m_toolboxEnabled = true;
    
    // ----------------------------------------------
    // Translate Pipe (PipeGrid_TranslatePipe)
    // ----------------------------------------------

    void HandleTranslateGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState);
    void HandleDragLost(GridHandState& handState);
    bool TryHandleDragStart(const FFingerGestureUpdate& update, GridHandState& handState);
    bool CanStartDrag(GridHandState& handState);

    void AttachTarget(const FFingerGestureUpdate& update, GridHandState& handState);
    void DetachTarget(GridHandState& handState);
    FVector GetEffectiveTarget(const FFingerGestureUpdate& update, GridHandState& handState);

    bool CanDropPipeAtCoordinate(const FPipeGridCoordinate& coordinate, const GridHandState& handState, bool* intoToolbox);

    bool m_dragEnabled = true;

    // ----------------------------------------------
    // Rotate Pipe (PPipeGrid_RotatePipe)
    // ----------------------------------------------

    void HandleRotateGestureUpdate(const FFingerGestureUpdate& update, GridHandState& handState);
    void HandleRotateUpdate(GridHandState& handState, const FVector& gridLocation);
    bool TryHandleRotateStart(GridHandState& handState); 
    void HandleRotateLost(GridHandState& handState);
    void ComputeCurrentRotationHandle(const FPipeGridCoordinate& pipeGridCoord, const GridHandState& handState, EInteractionAxis* pHandleAxis, FVector* pHandleLocation);
    void ComputeCurrentRotationHandle(const FPipeGridCoordinate& pipeGridCoord, const GridHandState& handState, EInteractionAxis preferrdAxis, EInteractionAxis* pHandleAxis, FVector* pHandleLocation);

    void ComputeCandidateRotations(GridHandState& handState, const FVector& gridLocation, float* yaw, float* pitch, float* roll);
    float ComputeCandidateRotation(const FVector& base, const FVector& current, const FVector& mask); 

    bool RotateEngaged(const FPipeGridCoordinate& location);
    bool ShowingRotateHandles(GridHandState& handState);

    bool m_rotateEnabled = true;

    // ----------------------------------------------
    // Cursor (PPipeGrid_Cursor)
    // ----------------------------------------------

    void UpdateCursor();
    void UpdateCursorForHand(GridHandState& handState, FCursorStateHand& cursorState);
    void SendCursorUpdate(const FCursorState& newState, bool forceDirty = false);
    bool ProcessCursorUpdatesForHand(const FCursorStateHand& newState, FCursorStateHand& state, bool forceDirty);
    void UpdateCursorProxy(const FConnectionsCursorState& cursorState, GridHandState& hand);
    
    // ----------------------------------------------
    // Place World (PPipeGrid_PlaceWorld)
    // ----------------------------------------------

    UFUNCTION(BlueprintCallable)
    void EnablePlaceWorld(bool enabled);


    UFUNCTION(BlueprintCallable)
    bool GetPlaceWorldEnabled();

    void HandlePlaceWorldGestureUpdate(const FGestureUpdate& update);

    PlaceWorldState m_placeWorldState;

    // ----------------------------------------------
    // Score (PPipeGrid_Score)
    // ----------------------------------------------

    void ScoreHandGrasp(EFingerHand hand, const APPipe* graspedPipe);
    void ScoreHandRelease(EFingerHand hand, const APPipe* releasedPipe);

    GridScore m_score = {};

    // ----------------------------------------------
    // Gaze (PPipeGrid_Gaze)
    // ----------------------------------------------

    enum class MarkPipesForGazeMode
    {
        ClearAll,
        MarkGazed,
    };

    void UpdateGaze(const FGazeUpdate& worldSpaceGaze);
    bool GazeIntersectsCoordinate(const FPipeGridCoordinate& coordinate, FVector* position);
    bool GazeIntersectsAABB(const FVector& minAABB, const FVector& maxAABB, FVector* position);
    bool FindMinDistanceFromGaze(const FVector& position, float* distance);
    bool FindMinDistanceFromGazeSq(const FVector& position, float* distance);
    void MarkPipesForGaze(MarkPipesForGazeMode mode);

    FGazeUpdate m_gaze;
};
