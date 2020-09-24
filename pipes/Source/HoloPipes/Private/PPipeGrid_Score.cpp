
#include "PPipeGrid_Score.h"
#include "PPipeGrid_Internal.h"

static constexpr int32 MaxScorePipeAge = 1;

// The Level Score should increase with each manipulation of a unique pipe, As long a
// either hand keeps manipulating the last manipulated piece, the score should not
// increase.
// 
// This is complicated by the fact that we can use two hands simultaneously. We don't
// want the score to increase if we're using the last pipe recently manipulated by either
// hand, but also don't want an infrequently used hand to pick up a pipe used "a while
// ago," for free after the frequently used hand has manipulated multiple pipe pieces.
//
// We accomplish this by keeping track of the last pipe manipulated by both hands, but
// also tracking the "Age" of the pipe (or how many times hand B has dropped a pipe since
// hand A last did), and clearing out the pipe when it gets too old

void APPipeGrid::ScoreHandGrasp(EFingerHand hand, const APPipe* graspedPipe)
{
    // Picking up a pipe is free as long as it was the last pipe picked up by either 
    // hand. Otherwise, it increments the score
    if (graspedPipe)
    {
        if (graspedPipe != m_score.Hands[0].Pipe && graspedPipe != m_score.Hands[1].Pipe)
        {
            m_score.CurrentScore++;
        }
    }
}

void APPipeGrid::ScoreHandRelease(EFingerHand hand, const APPipe* releasedPipe)
{
    if (releasedPipe)
    {
        int currentHandNum = (int)hand;
        auto& currentHand = m_score.Hands[currentHandNum];
        auto& otherHand = m_score.Hands[(currentHandNum + 1) % 2];

        currentHand.Age = 0;
        currentHand.Pipe = releasedPipe;

        if (otherHand.Pipe != nullptr)
        {
            otherHand.Age++;
            if (otherHand.Age > MaxScorePipeAge)
            {
                otherHand.Age = 0;
                otherHand.Pipe = nullptr;
            }
        }
    }
}

int32 APPipeGrid::GetCurrentLevelScore()
{
    return m_score.CurrentScore;
}

void APPipeGrid::ClearCurrentLevelScore()
{
    m_score = {};
}
