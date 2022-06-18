#include <pspsdk.h>
#include <pspdebug.h>
#include <pspiofilemgr.h>
#include <pspctrl.h>
#include <pspinit.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <psputility.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("ARK-3", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(4096);

#include "ark_imports.h"

#define H_LOADADDR 0x08D20000

int SysMemUserForUser_91DE343C(void* unk);

// ARK's linkless loader must be able to find this string
static const char* loadpath = "ms0:/PSP/SAVEDATA/ARK_01234/h.bin";

// ARK.BIN requires these imports
void loadARKImports(){
	IoOpen = &sceIoOpen;
	IoRead = &sceIoRead;
	IoClose = &sceIoClose;
	IoWrite = &sceIoWrite;
	
	KernelLibcTime = &sceKernelLibcTime;
	KernelDcacheWritebackAll = &sceKernelDcacheWritebackAll;
	DisplaySetFrameBuf = &sceDisplaySetFrameBuf;

	KernelCreateThread = &sceKernelCreateThread;
	KernelDelayThread = &sceKernelDelayThread;
	KernelStartThread = &sceKernelStartThread;
	KernelWaitThreadEnd = &sceKernelWaitThreadEnd;

	KernelDeleteVpl = &sceKernelDeleteVpl;
	KernelDeleteFpl = &sceKernelDeleteFpl;
	
	UtilityLoadModule = &sceUtilityLoadModule;
	UtilityUnloadModule = &sceUtilityUnloadModule;
	UtilityLoadNetModule = &sceUtilityLoadNetModule;
	UtilityUnloadNetModule = &sceUtilityUnloadNetModule;
	
	_SysMemUserForUser_91DE343C = &SysMemUserForUser_91DE343C;
	KernelFreePartitionMemory = &sceKernelFreePartitionMemory;
}

int main(){
	loadARKImports();
	
	SceUID fd = sceIoOpen(loadpath, PSP_O_RDONLY, 0);
	sceIoRead(fd, (void *)(H_LOADADDR|0x40000000), 0x4000);
	sceIoClose(fd);
	
	void (* hEntryPoint)() = (void*)H_LOADADDR;
	hEntryPoint();
	
	return 0;
}
