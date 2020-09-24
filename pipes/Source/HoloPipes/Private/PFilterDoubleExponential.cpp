// Fill out your copyright notice in the Description page of Project Settings.


#include "PFilterDoubleExponential.h"

// Sets default values for this component's properties
UPFilterDoubleExponential::UPFilterDoubleExponential()
{
	PrimaryComponentTick.bCanEverTick = false;

    Smoothing = 0.5f;
    Correction = 0.0f;
    Prediction = 0.0f;
    JitterRadius = 5.0f;
    DeviationRadius = 5.0f;

}

void UPFilterDoubleExponential::Reset()
{
    Current = FVector::ZeroVector;
    memset(&m_history, 0, sizeof(m_history));
}

void UPFilterDoubleExponential::Update(FVector newRawPosition)
{
    // If joing is invalid, reset the filter
    if (newRawPosition.Equals(FVector::ZeroVector))
    {
        Reset();
    }

    // Check for divide by zero. Use an epsilon of a 10th of a millimeter
    float effectiveJitterRadius = __max(JitterRadius, 0.01f);

    FVector newFilteredPosition;
    FVector newPredictedPosition;
    FVector deltaVector;
    float deltaDistance;
    FVector newTrend;

    // Initial start values
    switch (m_history.frameCount)
    {
        case 0:
        {
            newFilteredPosition = newRawPosition;
            newTrend = FVector::ZeroVector;
            m_history.frameCount++;

            break;
        }

        case 1:
        {
            newFilteredPosition = (newRawPosition + m_history.rawPosition) * 0.5f;
            deltaVector = (newFilteredPosition - m_history.filteredPosition);
            newTrend = (deltaVector * Correction) + (m_history.trend * (1.0f - Correction));
            m_history.frameCount++;

            break;
        }

        default:
        {
            // First apply jitter filter
            deltaVector = newRawPosition - m_history.filteredPosition;
            deltaDistance = deltaVector.Size();

            if (deltaDistance <= effectiveJitterRadius)
            {
                newFilteredPosition = newRawPosition * deltaDistance / effectiveJitterRadius + m_history.filteredPosition * (1.0f - deltaDistance / effectiveJitterRadius);
            }
            else
            {
                newFilteredPosition = newRawPosition;
            }

            // Now the double exponential smoothing filter
            newFilteredPosition = newFilteredPosition * (1.0f - Smoothing) + (m_history.filteredPosition + m_history.trend) * Smoothing;

            deltaVector = newFilteredPosition - m_history.filteredPosition;
            newTrend = deltaVector * Correction + m_history.trend * (1.0f - Correction);
            break;
        }
    }

    // Predict into the future to reduce latency
    newPredictedPosition = newFilteredPosition + newTrend * Prediction;

    // Check that we are not too far away from raw data
    deltaVector = newPredictedPosition - newRawPosition;
    deltaDistance = deltaVector.Size();

    if (deltaDistance > DeviationRadius)
    {
        newPredictedPosition = newPredictedPosition * DeviationRadius / deltaDistance + newRawPosition * (1.0f - DeviationRadius / deltaDistance);
    }

    // Save the data from this frame
    m_history.rawPosition = newRawPosition;
    m_history.filteredPosition = newFilteredPosition;
    m_history.trend = newTrend;

    // Output the data
    Current = newPredictedPosition;

}



