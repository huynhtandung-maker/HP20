#pragma once
#include "config.h"
void portalBegin(Config* config);
bool portalActive();
bool portalSaved();
void portalTick();
void portalClose();
String portalName();
