#pragma once
#include "config.h"
struct CloudJob { char host[128], token[129], ca[6000], payload[256]; };
bool cloudBegin();
bool cloudSubmit(const Config& config, const String& payload);
bool cloudResult(int& result);
