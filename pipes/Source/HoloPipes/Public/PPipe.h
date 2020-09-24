// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "HoloPipes.h"
#include "GameFramework/Actor.h"
#include "PPipe.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPipePropertyChanged);


UENUM(BlueprintType)
enum class EPipeType : uint8
{
	None =		0x00 UMETA(DisplayName = "None"),
	Start =		0x01 UMETA(DisplayName = "Start"),
	End =		0x02 UMETA(DisplayName = "End"),
	Straight =	0x04 UMETA(DisplayName = "Straight"),
	Corner =	0x08 UMETA(DisplayName = "Corner"),
	Junction =	0x10 UMETA(DisplayName = "Junction"),
    Block =     0x20 UMETA(DisplayName = "Block")
};

DEFINE_ENUM_FLAG_OPERATORS(EPipeType);

UENUM(BlueprintType)
enum class EInteractionEvent : uint8
{
    DragStarted,
    DragFinished,
    DragFinishedBlocked,
    DragFinishedConnected,
    RotateStarted,
    RotateFinished,
    RotateFinishedConnected,
};

UENUM(BlueprintType)
enum class EPipeProxyType : uint8
{
    None,
    ProxyReady,
    ProxyBlocked
};

UENUM(BlueprintType)
enum class EFocusStage : uint8
{
    None,
    HandOnly,
    HandAndGaze,
    HandAndGazeAndDistance,
};


USTRUCT(BlueprintType)
struct FPipeGridCoordinate
{
	GENERATED_BODY()

	static const int AxisBitWidth = 21; // Value range 0-2,097,151
	static const int AxisMax = ((1 << (AxisBitWidth - 1)) - 1); // 1,058,575
	static const int AxisMin = -(1 << (AxisBitWidth - 1)); // -1,058,576

    static const FPipeGridCoordinate Zero;
    static const FPipeGridCoordinate Min;
    static const FPipeGridCoordinate Max;

	static size_t HashOf(const FPipeGridCoordinate& loc);

    static float Distance(const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);
    static float DistanceSq(const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);

    float DistanceTo(const FPipeGridCoordinate& other) const;
    float DistanceToSq(const FPipeGridCoordinate& other) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Z;
};

bool operator == (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);
bool operator != (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);
FPipeGridCoordinate& operator += (FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);
FPipeGridCoordinate operator + (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs);
FPipeGridCoordinate operator * (int lhs, const FPipeGridCoordinate& rhs);
FPipeGridCoordinate operator * (const FPipeGridCoordinate& lhs, int rhs);

// For some reason, Unreal decided the C++ values for Bitflags should indicate
// which bits are set, not what the value is. This means that Right, with a value
// of 0, should produce an int with 1 << 0, or 0x01.
UENUM(BlueprintType, Meta = (BitFlags))
enum class EPipeDirections : uint8
{
    Right,  // 1 << 0, 0x01
    Front,  // 1 << 1, 0x02
    Left,   // 1 << 2, 0x04
    Back,   // 1 << 3, 0x08
    Top,    // 1 << 4, 0x10
    Bottom, // 1 << 5, 0x20
};

// C++ implementation of Pipe Directions, for use in code
enum class PipeDirections
{
	None =		0x00,
	Right =		0x01,
	Front =		0x02,
	Left =		0x04,
	Back =		0x08,
	Top =		0x10,
	Bottom =	0x20
};

DEFINE_ENUM_FLAG_OPERATORS(PipeDirections);

constexpr int PipeClassCount = 13;
constexpr int DefaultPipeClass = 0;

struct PipeSegmentGenerated
{
    EPipeType Type = EPipeType::None;
	int PipeClass = DefaultPipeClass;
    PipeDirections Connections = PipeDirections::None;
	FPipeGridCoordinate Location = FPipeGridCoordinate::Zero;
};

UCLASS()
class HOLOPIPES_API APPipe : public AActor
{
	GENERATED_BODY()
	
public:	

    static FPipeGridCoordinate PipeDirectionToLocationAdjustment(PipeDirections connection);
    static PipeDirections LocationAdjustmentToPipeDirection(const FPipeGridCoordinate& adjustment);
    static PipeDirections LocationAdjustmentToPipeDirection(const FVector& adjustment);
    static PipeDirections InvertPipeDirection(PipeDirections connection);	
    
    static const PipeDirections ValidDirections[];
    static const int ValidDirectionsCount;

	// Sets default values for this actor's properties
	APPipe();

	bool Initialize(EPipeType newType, int PipeClass, PipeDirections connections, bool fixed, bool inToolbox = false, EPipeProxyType proxyType = EPipeProxyType::None);

	EPipeType GetPipeType() const;

	bool SetPipeClass(int newClass);
	int GetPipeClass() const;

	bool GetPipeFixed() const;
    void SetPipeFixed(bool fixed);

    PipeDirections GetPipeDirections() const;
    void SetPipeDirections(PipeDirections newDirections);

    PipeDirections GetPipeConnections() const;
    void SetPipeConnections(PipeDirections connections);

    void SetProxyBlocked(bool blocked);

    void SetInToolbox(bool inToolbox);
    bool GetInToolbox();

    void SetShowLabel(bool showLabel);
    bool GetShowLabel();

    void SetFocusStage(EFocusStage stage);
    EFocusStage GetFocusStage();

    FRotator GetDesiredRotation() const;


    void FireInteractionEvent(EInteractionEvent interactionEvent) { OnInteractionEvent(interactionEvent); }

protected:

    UFUNCTION(BlueprintImplementableEvent)
    void OnPipeClassChanged(int newClass);

    UFUNCTION(BlueprintImplementableEvent)
    void OnPipeInitializationComplete();

    UFUNCTION(BlueprintImplementableEvent)
    void OnInteractionEvent(EInteractionEvent InteractionEvent);

    UFUNCTION(BlueprintImplementableEvent)
    void OnProxyTypeChanged(EPipeProxyType proxyType);

    UFUNCTION(BlueprintImplementableEvent)
    void OnShowLabelChanged(bool show);

    UFUNCTION(BlueprintImplementableEvent)
    void OnFixedChanged();

    UFUNCTION(BlueprintImplementableEvent)
    void OnFocusStageChanged(EFocusStage stage);

	// The type of pipe piece represented by this instance, which is invariant over
	// its lifetime
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
	EPipeType PipeType;

	// The class of the pipe piece, which can change over time
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
	int PipeClass;

	// Whether this pipe piece is locked in place
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
	bool PipeFixed;

    // Whether this pipe piece is currently in the toolbox
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    bool InToolbox;

    // What kind of proxy this piece is
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    EPipeProxyType ProxyType;

    // Whether this pipe piece is locked in place
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    bool ShowLabel;

    // Whether this pipe piece is currently gazed
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipe")
    EFocusStage FocusStage;

    // Get the current directions in which this pipe is connected
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Meta = (Bitmask, BitmaskEnum = "EPipeDirections"), Category = "Pipe")
    int32 PipeConnections;
    
    PipeDirections m_directions;

private:

	bool m_initialized;
};
