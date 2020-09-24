#pragma once

class APPipe;

struct GridScoreHand
{
    const APPipe* Pipe = nullptr;
    int32 Age = 0;
};

struct GridScore
{
    GridScoreHand Hands[2] = {};

    int32 CurrentScore = 0;
};

