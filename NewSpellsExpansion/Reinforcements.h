#pragma once

class hero;

namespace Era
{
   struct TEvent;
}

extern bool reinforcementNativeReady;

bool initializeNativeReinforcements();
bool installNativeReinforcementLifecycleHandlers();
void shutdownNativeReinforcements();
int castNativeReinforcements(hero* Hero, int schoolLevel);
void resetNativeReinforcementState();
void __stdcall saveNativeReinforcementState(Era::TEvent* Event);
void __stdcall loadNativeReinforcementState(Era::TEvent* Event);
