// Fill out your copyright notice in the Description page of Project Settings.


#include "PMRPawn.h"
#include "Engine/world.h"
#include "HoloPipes.h"

// Sets default values
APMRPawn::APMRPawn()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    m_handStates = {};
    VRCamera = nullptr;
}

// Called when the game starts or when spawned
void APMRPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APMRPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


// Called to bind functionality to input
void APMRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void APMRPawn::UpdateFingerState(FFingersUpdate update, FGazeUpdate gaze)
{
    OnGazeNotification.Broadcast(gaze);

    FGestureUpdate notification = {};
    bool sendLeftNotification = HandleFinger(update.Left, m_handStates.Left, notification.Left);
    bool sendRightNotification = HandleFinger(update.Right, m_handStates.Right, notification.Right);

    if (sendLeftNotification || sendRightNotification)
    {
        OnGestureNotification.Broadcast(notification);
    }
}

bool APMRPawn::HandleFinger(const FFingerUpdate& update, HandState& currentState, FFingerGestureUpdate& notification)
{
    bool sendNotification = false;

    if (update.Pose != EFingerPose::None)
    {
        // Keep the old positions if we don't have new ones
        currentState.target = update.Target;
        currentState.attach = update.Attach;
    }

    EGestureProgress lastProgress = currentState.progress;

    switch (lastProgress)
    {
        case EGestureProgress::None:
            HandleNone(update.Pose, currentState);
            break;

        case EGestureProgress::Tracked:
            HandleTracked(update.Pose, currentState);
            break;

        case EGestureProgress::Started:
            HandleStarted(update.Pose, currentState);
            break;

        case EGestureProgress::Updated:
            HandleUpdated(update.Pose, currentState);
            break;

        case EGestureProgress::Lost:
            HandleLost(update.Pose, currentState);
            break;
    }

    // We send all notifications, except when we're in a steady state of "None"
    if (lastProgress != EGestureProgress::None || currentState.progress != EGestureProgress::None)
    {
        notification.Progress = currentState.progress;
        notification.Target = currentState.target;
        notification.Attach = currentState.attach;
        sendNotification = true;
    }

    return sendNotification;
}

void APMRPawn::HandleNone(EFingerPose currentPose, HandState& currentState)
{
    switch (currentPose)
    {
        case EFingerPose::Tracked:
            currentState.progress = EGestureProgress::Tracked;
            break;

        case EFingerPose::Pinched:
            currentState.progress = EGestureProgress::Started;
            break;
    }
}

void APMRPawn::HandleTracked(EFingerPose currentPose, HandState& currentState)
{
    switch (currentPose)
    {
        case EFingerPose::None:
            currentState.progress = EGestureProgress::Lost;
            break;

        case EFingerPose::Pinched:
            currentState.progress = EGestureProgress::Started;
            break;
    }
}

void APMRPawn::HandleStarted(EFingerPose currentPose, HandState& currentState)
{
    switch (currentPose)
    {
        case EFingerPose::None:
        case EFingerPose::Tracked:
            currentState.progress = EGestureProgress::Lost;
            break;

        case EFingerPose::Pinched:
            currentState.progress = EGestureProgress::Updated;
            break;
    }
}

void APMRPawn::HandleUpdated(EFingerPose currentPose, HandState& currentState)
{
    switch (currentPose)
    {
        case EFingerPose::None:
        case EFingerPose::Tracked:
            currentState.progress = EGestureProgress::Lost;
            break;
    }
}

void APMRPawn::HandleLost(EFingerPose currentPose, HandState& currentState)
{
    switch (currentPose)
    {
        case EFingerPose::None:
            currentState.progress = EGestureProgress::None;
            break;

        case EFingerPose::Tracked:
            currentState.progress = EGestureProgress::Tracked;
            break;

        case EFingerPose::Pinched:
            currentState.progress = EGestureProgress::Started;
            break;
    }
}