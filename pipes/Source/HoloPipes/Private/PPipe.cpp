// Fill out your copyright notice in the Description page of Project Settings.

#include "PPipe.h"

#include "PipeRotation.h"
#include "PPipeGrid.h"

const PipeDirections APPipe::ValidDirections[] = {
    PipeDirections::Right,
    PipeDirections::Back,
    PipeDirections::Left,
    PipeDirections::Front,
    PipeDirections::Top,
    PipeDirections::Bottom };

const int APPipe::ValidDirectionsCount = ARRAYSIZE(ValidDirections);

const FPipeGridCoordinate FPipeGridCoordinate::Zero = { 0,0,0 };
const FPipeGridCoordinate FPipeGridCoordinate::Min = { FPipeGridCoordinate::AxisMin, FPipeGridCoordinate::AxisMin, FPipeGridCoordinate::AxisMin };
const FPipeGridCoordinate FPipeGridCoordinate::Max = { FPipeGridCoordinate::AxisMax, FPipeGridCoordinate::AxisMax, FPipeGridCoordinate::AxisMax };

FPipeGridCoordinate APPipe::PipeDirectionToLocationAdjustment(PipeDirections connection)
{
    switch (connection)
    {
        case PipeDirections::None:
            return { 0,0,0 };

        case PipeDirections::Front:
            return { 1,0,0 };

        case PipeDirections::Back:
            return { -1,0,0 };

        case PipeDirections::Left:
            return { 0,-1,0 };

        case PipeDirections::Right:
            return { 0,1,0 };

        case PipeDirections::Top:
            return { 0,0,1 };

        case PipeDirections::Bottom:
            return { 0,0,-1 };

        default:
            UE_LOG(HoloPipesLog, Warning, L"APPipe::PipeDirectionToLocationAdjustment invalid direction specified (%d)", static_cast<int>(connection));
            return { 0,0,0 };
    }
}

PipeDirections APPipe::LocationAdjustmentToPipeDirection(const FVector& adjustment)
{
    FPipeGridCoordinate rounded = { (int)roundf(adjustment.X), (int)roundf(adjustment.Y), (int)roundf(adjustment.Z) };
    return LocationAdjustmentToPipeDirection(rounded);
}

PipeDirections APPipe::LocationAdjustmentToPipeDirection(const FPipeGridCoordinate& adjustment)
{
    int absx = abs(adjustment.X);
    int absy = abs(adjustment.Y);
    int absz = abs(adjustment.Z);

    if (absx > absy && absx > absz)
    {
        return adjustment.X >= 0 ? PipeDirections::Front : PipeDirections::Back;
    }
    else if (absy > absx && absy > absz)
    {
        return adjustment.Y >= 0 ? PipeDirections::Right : PipeDirections::Left;
    }
    else if (absz > absx && absz > absy)
    {
        return adjustment.Z >= 0 ? PipeDirections::Top : PipeDirections::Bottom;
    }
    else
    {
        return PipeDirections::None;
    }
}

PipeDirections APPipe::InvertPipeDirection(PipeDirections connection)
{
    switch (connection)
    {
        case PipeDirections::None:
            return PipeDirections::None;

        case PipeDirections::Front:
            return PipeDirections::Back;

        case PipeDirections::Back:
            return PipeDirections::Front;

        case PipeDirections::Left:
            return PipeDirections::Right;

        case PipeDirections::Right:
            return PipeDirections::Left;

        case PipeDirections::Top:
            return PipeDirections::Bottom;

        case PipeDirections::Bottom:
            return PipeDirections::Top;

        default:
            UE_LOG(HoloPipesLog, Warning, L"APPipe::InvertPipeDirection invalid direction specified (%d)", static_cast<int>(connection));
            return PipeDirections::None;
    }
}

bool operator == (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
	return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
}

bool operator != (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
    return lhs.X != rhs.X || lhs.Y != rhs.Y || lhs.Z != rhs.Z;
}

FPipeGridCoordinate& operator += (FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
    lhs = lhs + rhs;
    return lhs;
}

FPipeGridCoordinate operator + (const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
    return { lhs.X + rhs.X, lhs.Y + rhs.Y, lhs.Z + rhs.Z };
}

FPipeGridCoordinate operator * (int lhs, const FPipeGridCoordinate& rhs)
{
    return { lhs * rhs.X, lhs * rhs.Y, lhs * rhs.Z };
}

FPipeGridCoordinate operator * (const FPipeGridCoordinate& lhs, int rhs)
{
    return rhs * lhs;
}

size_t FPipeGridCoordinate::HashOf(const FPipeGridCoordinate& loc)
{
	if (loc.X < AxisMin || loc.X > AxisMax ||
		loc.Y < AxisMin || loc.Y > AxisMax ||
		loc.Z < AxisMin || loc.Y > AxisMax)
	{
		UE_LOG(HoloPipesLog, Error, L"FPipeGridCoordinate - Attempting to hash invalid location {%d, %d, %d}", loc.X, loc.Y, loc.Z);
	}
	
	return
		((static_cast<size_t>(loc.X - AxisMin) << AxisBitWidth) << AxisBitWidth) |
		(static_cast<size_t>(loc.Y - AxisMin) << AxisBitWidth) |
		static_cast<size_t>(loc.Y - AxisMin);
}

float FPipeGridCoordinate::Distance(const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
    return FMath::Sqrt(FPipeGridCoordinate::DistanceSq(lhs, rhs));
}

float FPipeGridCoordinate::DistanceSq(const FPipeGridCoordinate& lhs, const FPipeGridCoordinate& rhs)
{
    return static_cast<float>(FMath::Square(rhs.X - lhs.X) + FMath::Square(rhs.Y - lhs.Y) + FMath::Square(rhs.Z - lhs.Z));
}

float FPipeGridCoordinate::DistanceTo(const FPipeGridCoordinate& other) const
{
    return Distance(*this, other);
}

float FPipeGridCoordinate::DistanceToSq(const FPipeGridCoordinate& other) const
{
    return DistanceSq(*this, other);
}

// Sets default values
APPipe::APPipe()
{
	PipeType = EPipeType::None;
	PipeClass = DefaultPipeClass;
	m_initialized = false;
    m_directions = PipeDirections::None;

    ShowLabel = false;
}

bool APPipe::Initialize(EPipeType newType, int newClass, PipeDirections directions, bool fixed, bool inToolbox, EPipeProxyType proxyType)
{
	bool success = true;

	if (m_initialized)
	{
		UE_LOG(HoloPipesLog, Warning, L"APPipe::Initialize already called");
	}
	else
	{
		switch (newType)
		{
		case EPipeType::Start:
		case EPipeType::End:
		case EPipeType::Straight:
		case EPipeType::Corner:
		case EPipeType::Junction:
        case EPipeType::Block:
			PipeType = newType;
			break;

		default:
			UE_LOG(HoloPipesLog, Error, L"APPipe::Initialize invalid pipe type (%d) specified", newType);
			success = false;
		}

		if (success)
		{
			success = SetPipeClass(newClass);
		}

		if (success)
		{
            FocusStage = EFocusStage::None;
			PipeFixed = fixed;
            SetPipeDirections(directions);
            SetPipeConnections(PipeDirections::None);
            ProxyType = proxyType;
            InToolbox = inToolbox;
			m_initialized = true;
			OnPipeInitializationComplete();
		}
	}

	return success;
}

EPipeType APPipe::GetPipeType() const
{
	return PipeType;
}

bool APPipe::SetPipeClass(int newClass)
{
	bool success = true;

	if (newClass != PipeClass)
	{
		if (PipeClass < DefaultPipeClass || PipeClass >= PipeClassCount)
		{
			UE_LOG(HoloPipesLog, Error, L"APPipe::SetPipeClass invalid pipe class (%d) specified", newClass);
			success = false;
		}
		else
		{
			PipeClass = newClass;

			if (m_initialized)
			{
				OnPipeClassChanged(newClass);
			}
		}
	}

	return success;
}

int APPipe::GetPipeClass() const
{
	return PipeClass;
}

bool APPipe::GetPipeFixed() const
{
	return PipeFixed;
}

void APPipe::SetPipeFixed(bool fixed)
{
    if (fixed != PipeFixed)
    {
        PipeFixed = fixed;
        if (m_initialized)
        {
            OnFixedChanged();
        }
    }
}

PipeDirections APPipe::GetPipeDirections() const
{
    return m_directions;
}

void APPipe::SetPipeDirections(PipeDirections newDirections)
{
    m_directions = newDirections;
}

PipeDirections APPipe::GetPipeConnections() const
{
    return static_cast<PipeDirections>(PipeConnections);
}

void APPipe::SetPipeConnections(PipeDirections connections)
{
    PipeConnections = static_cast<int32>(connections);
}

FRotator APPipe::GetDesiredRotation() const
{
    FRotator desiredRotation = FRotator::ZeroRotator;
    if (GetPipeRotation(GetPipeType(), GetPipeDirections(), desiredRotation))
    {
        return desiredRotation;
    }
    else
    {
        return FRotator::ZeroRotator;
    }
}

void APPipe::SetProxyBlocked(bool blocked)
{
    if (ProxyType != EPipeProxyType::None)
    {
        EPipeProxyType newType = blocked ? EPipeProxyType::ProxyBlocked : EPipeProxyType::ProxyReady;

        if (newType != ProxyType)
        {
            ProxyType = newType;

            if (m_initialized)
            {
                OnProxyTypeChanged(newType);
            }
        }
    }
}

void APPipe::SetInToolbox(bool inToolbox)
{
    InToolbox = inToolbox;
}

bool APPipe::GetInToolbox()
{
    return InToolbox;
}

void APPipe::SetShowLabel(bool showLabel)
{
    if (ShowLabel != showLabel)
    {
        ShowLabel = showLabel;
        if (m_initialized)
        {
            OnShowLabelChanged(showLabel);
        }
    }
}

bool APPipe::GetShowLabel()
{
    return ShowLabel;
}

void APPipe::SetFocusStage(EFocusStage stage)
{
    if (stage != FocusStage)
    {
        FocusStage = stage;
        if (m_initialized)
        {
            OnFocusStageChanged(FocusStage);
        }
    }
}

EFocusStage APPipe::GetFocusStage()
{
    return FocusStage;
}