#include "PoGame.h"

SPoGameState NPoGameBehavior::init(SPoGameResources &outResources, SPoGameSettings const &settings)
{
    return SPoGameState();
}

void NPoGameBehavior::cleanup(SPoGameResources &inOutResources, SPoGameState &inOutState, SPoGameSettings const &settings)
{



    inOutResources = {};
    inOutState = {};
}
