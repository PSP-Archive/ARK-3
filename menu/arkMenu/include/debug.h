#ifndef DEBUG_H
#define DEBUG_H

#include <cstdio>
#include <cstring>

#include "gfx.h"

using namespace std;

#define debugScreen pspDebugScreenPrintf
void debugFile(const char* text);

#endif
