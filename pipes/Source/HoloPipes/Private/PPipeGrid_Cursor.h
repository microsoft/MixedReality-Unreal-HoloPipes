#pragma once

#include "PPipeGrid_Interaction.h"
#include "PPipeGrid_Cursor.generated.h"

USTRUCT(BlueprintType)
struct FSimpleCursorState
{
    static const FSimpleCursorState Empty;

    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInteractionPhase Phase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Dirty;
};

USTRUCT(BlueprintType)
struct FConnectionsCursorState
{
    static const FConnectionsCursorState Empty;

    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInteractionPhase Phase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (Bitmask, BitmaskEnum = "EPipeDirections"))
    int32 Directions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Dirty;
};

USTRUCT(BlueprintType)
struct FRotateCursorState
{
    static const FRotateCursorState Empty;

    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInteractionPhase Phase;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInteractionAxis Axis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator AxisRotations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool Dirty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool SuppressChangeAnimation;
};

USTRUCT(BlueprintType)
struct FCursorStateHand
{
    static const FCursorStateHand Empty;

    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSimpleCursorState Target;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSimpleCursorState Translate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotateCursorState Rotate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FConnectionsCursorState Connections;
};

USTRUCT(BlueprintType)
struct FCursorState
{
    static const FCursorState Empty;

    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCursorStateHand Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCursorStateHand Right;
};
