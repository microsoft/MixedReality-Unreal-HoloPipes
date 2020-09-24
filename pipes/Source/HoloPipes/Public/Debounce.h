#pragma once

#include "PPipeGrid_Interaction.h"

#ifndef TIMEOUT_EPSILON 
#define TIMEOUT_EPSILON (1.e-4f)
#endif

class Debounce
{
public:
    void Reset(float timeout = 0)
    {
        m_current = 0;
        m_nextSeen = false;
        m_nextFirstSeen = 0;

        if (timeout > TIMEOUT_EPSILON)
        {
            m_timeout = timeout;
        }
        else
        {
            m_timeout = 0;
        }
    }
    
protected:

    void SetCurrentInternal(int32 value)
    {
        m_current = value;
        m_nextSeen = false;
        m_nextFirstSeen = 0;
    }

    int UpdateCurrentInternal(int32 value, float time)
    {
        if (m_timeout < TIMEOUT_EPSILON || value == m_current)
        {
            // If our timeout is (close to) 0, or we're being set
            // to our current value, just take the value
            SetCurrentInternal(value);
        }
        else if (m_nextSeen && value == m_next)
        {
            // This is the last value we saw. Is it time to switch?
            if ((time - m_nextFirstSeen) >= m_timeout)
            {
                SetCurrentInternal(value);
            }
        }
        else
        {
            // We have a new value, start tracking it
            m_nextSeen = true;
            m_nextFirstSeen = time;
            m_next = value;
        }

        return m_current;
    }

    int32 m_current = 0;
    int32 m_next = 0;

    float m_timeout = 0;
    float m_nextFirstSeen = 0;

    bool m_nextSeen = false;

};

class DebounceInt : public Debounce
{

    void SetCurrent(int32 value)
    {
        SetCurrentInternal(value);
    }

    int32 GetCurrent()
    {
        return m_current;
    }

    int UpdateCurrent(int32 value, float time)
    {
        return UpdateCurrentInternal(value, time);
    }
};

struct DebounceBool : public Debounce
{
    void SetCurrent(bool value)
    {
        SetCurrentInternal(value ? 1 : 0);
    }

    bool GetCurrent()
    {
        return (m_current != 0);
    }

    bool UpdateCurrent(bool value, float time)
    {
        return 0 != UpdateCurrentInternal((value ? 1 : 0), time);
    }

};

struct DebounceAxis : public Debounce
{
    void SetCurrent(EInteractionAxis axis)
    {
        SetCurrentInternal(static_cast<int32>(axis));
    }

    EInteractionAxis GetCurrent()
    {
        return static_cast<EInteractionAxis>(m_current);
    }

    EInteractionAxis UpdateCurrent(EInteractionAxis value, float time)
    {
        return static_cast<EInteractionAxis>(UpdateCurrentInternal(static_cast<int32>(value), time));
    }
};
