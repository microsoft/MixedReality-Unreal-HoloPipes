// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PFilterDoubleExponential.generated.h"

struct FilterHistory
{
    FVector rawPosition;
    FVector filteredPosition;
    FVector trend;

    uint32 frameCount;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HOLOPIPES_API UPFilterDoubleExponential : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPFilterDoubleExponential();

    UFUNCTION(BlueprintCallable)
    void Reset();

    UFUNCTION(BlueprintCallable)
    void Update(FVector newRawPosition);

protected:

    // How much smoothign will occur. Will lag when too high.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float Smoothing;

    // How much to correct back from prediction. Can  make things springy.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float Correction;

    // Amount of prediction into the future to use. Can over shoot when too high.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float Prediction;
    
    // Size of the radius, in centimeters, where jitter is removed. Can do too much smoothing when too high
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float JitterRadius;
	
    // Size of the max prediction radius, in centimeters. Can snap back to noisy data when too high.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float DeviationRadius;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Result")
    FVector Current;

private:
    FilterHistory m_history;
};
