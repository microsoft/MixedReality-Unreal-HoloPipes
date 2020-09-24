// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PMRPawn.generated.h"

UENUM(BlueprintType)
enum class EFingerHand : uint8
{
    Left = 0,
    Right = 1,
    Count = 2
};

UENUM(BlueprintType)
enum class EFingerPose : uint8
{
	None,
	Tracked,
	Pinched
};

UENUM(BlueprintType)
enum class EGestureProgress : uint8
{
    // We don't see the finger at all
    None,

    // We see the finger, but it's not involved in a gesture
    Tracked,

    // The gesture has started
    Started,

    // The gesture has been updated
    Updated,

    // The gesture has been lost
    Lost,
};

struct HandState
{
    EGestureProgress progress;
    FTransform target;
    FTransform attach;
};

struct HandStates
{
    HandState Left;
    HandState Right;
};

USTRUCT(BlueprintType)
struct FFingerUpdate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFingerPose Pose;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform Target;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform Attach;
};

USTRUCT(BlueprintType)
struct FFingersUpdate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFingerUpdate Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFingerUpdate Right;
};

USTRUCT(BlueprintType)
struct FFingerGestureUpdate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGestureProgress Progress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform Target;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform Attach;
};

USTRUCT(BlueprintType)
struct FGestureUpdate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFingerGestureUpdate Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FFingerGestureUpdate Right;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGesture, FGestureUpdate, update);

USTRUCT(BlueprintType)
struct FGazeUpdate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Origin;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Direction;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGaze, FGazeUpdate, update);

UCLASS()
class HOLOPIPES_API APMRPawn : public APawn
{
    GENERATED_BODY()

public:
    // Sets default values for this pawn's properties
    APMRPawn();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY(BlueprintAssignable, Category = "GestureEvents")
    FGesture OnGestureNotification;

    UPROPERTY(BlueprintAssignable, Category = "GestureEvents")
    FGaze OnGazeNotification;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USceneComponent* VRCamera;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable)
    void UpdateFingerState(FFingersUpdate update, FGazeUpdate gaze);

    bool HandleFinger(const FFingerUpdate& update, HandState& currentState, FFingerGestureUpdate& notification);

    void HandleNone(EFingerPose currentPose, HandState& currentState);
    void HandleTracked(EFingerPose currentPose, HandState& currentState);
    void HandleStarted(EFingerPose currentPose, HandState& currentState);
    void HandleUpdated(EFingerPose currentPose, HandState& currentState);
    void HandleLost(EFingerPose currentPose, HandState& currentState);

    HandStates m_handStates;
};
