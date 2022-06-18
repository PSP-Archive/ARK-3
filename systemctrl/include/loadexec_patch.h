#ifndef LOADEXEC_PATCH_H
#define LOADEXEC_PATCH_H

#include <sdk.h>
#include <module2.h>
#include <macros.h>
#include <offsets.h>

extern void patchLoadExecCommon(SceModule2* loadexec, u32 LoadReboot);

#endif
