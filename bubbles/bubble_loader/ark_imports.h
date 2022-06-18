#ifndef ARK_IMPORTS_H
#define ARK_IMPORTS_H

int (* IoOpen)(char *, int, int);
int (* IoRead)(int, void *, int);
int (* IoWrite)(int, void *, int);
int (* IoClose)(int);
void (* KernelLibcTime)(int);
SceUID (* KernelCreateThread)(const char *name, SceKernelThreadEntry entry, int initPriority, int stackSize, SceUInt attr, SceKernelThreadOptParam *option);
int (* KernelStartThread)(SceUID thid, SceSize arglen, void *argp);
void (* KernelDelayThread)(uint32_t);
int (* KernelWaitThreadEnd)(SceUID thid, SceUInt *timeout);
int (* UtilityLoadModule)(int);
int (* UtilityUnloadModule)(int);
int (* UtilityLoadNetModule)(int);
int (* UtilityUnloadNetModule)(int);
int (* _SysMemUserForUser_91DE343C)( void *unk );
int (* KernelFreePartitionMemory)(int);
int (* KernelDeleteVpl)(int);
int (* KernelDeleteFpl)(int);
int (* DisplaySetFrameBuf)(void *topaddr, int bufferwidth, int pixelformat, int sync);
void (* KernelDcacheWritebackAll)(void);
int (* _sceUtilitySavedataGetStatus)();
int (* _sceUtilitySavedataInitStart)(void *params);
void (* _sceUtilitySavedataUpdate)(int a0);
int (* _sceUtilitySavedataShutdownStart)();

#endif
