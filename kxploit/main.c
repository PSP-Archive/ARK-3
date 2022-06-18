#include <sdk.h>
#include "kxploit.h"

int (* _sceUtilitySavedataGetStatus)() = (void*)NULL;
int (* _sceUtilitySavedataInitStart)(SceUtilitySavedataParam *params) = (void*)NULL;
void (* _sceUtilitySavedataUpdate)(int a0) = (void*)NULL;
int (* _sceUtilitySavedataShutdownStart)() = (void*)NULL;

FunctionTable* g_tbl = NULL;

void p5_open_savedata(int mode)
{
	p5_close_savedata();

	SceUtilitySavedataParam dialog;

	memset(&dialog, 0, sizeof(SceUtilitySavedataParam));
	dialog.base.size = sizeof(SceUtilitySavedataParam);

	dialog.base.language = 1;
	dialog.base.buttonSwap = 1;
	dialog.base.graphicsThread = 0x11;
	dialog.base.accessThread = 0x13;
	dialog.base.fontThread = 0x12;
	dialog.base.soundThread = 0x10;

	dialog.mode = mode;

	_sceUtilitySavedataInitStart(&dialog);

	// Wait for the dialog to initialize
	while (_sceUtilitySavedataGetStatus() < 2)
	{
		g_tbl->KernelDelayThread(100);
	}
}

// Runs the savedata dialog loop
void p5_close_savedata()
{

	int running = 1;
	int last_status = -1;

	while(running) 
	{
		int status = _sceUtilitySavedataGetStatus();
		
		if (status != last_status)
		{
			last_status = status;
		}

		switch(status) 
		{
			case PSP_UTILITY_DIALOG_VISIBLE:
				_sceUtilitySavedataUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				_sceUtilitySavedataShutdownStart();
				break;

			case PSP_UTILITY_DIALOG_NONE:
				running = 0;
				break;

			case PSP_UTILITY_DIALOG_FINISHED:
				break;
		}
		g_tbl->KernelDelayThread(100);
	}
}

void initKxploit(KxploitFunctions* kf, FunctionTable* tbl)__attribute__((section(".text.startup")));
void initKxploit(KxploitFunctions* kf, FunctionTable* tbl){
	kf->stubScanner = &stubScanner;
	kf->doExploit = &doExploit;
	kf->executeKernel = &executeKernel;
	kf->repairInstruction = &repairInstruction;
	kf->p5_open_savedata = &p5_open_savedata;
	kf->p5_close_savedata = &p5_close_savedata;
	g_tbl = tbl;
	
	// savedata functions
	_sceUtilitySavedataGetStatus = (void*)g_tbl->RelocSyscall(g_tbl->FindImportUserRam("sceUtility", 0x8874DBE0));
	_sceUtilitySavedataInitStart = (void*)g_tbl->RelocSyscall(g_tbl->FindImportUserRam("sceUtility", 0x50C4CD57));
	_sceUtilitySavedataUpdate = (void*)g_tbl->RelocSyscall(g_tbl->FindImportUserRam("sceUtility", 0xD4B95FFB));
	_sceUtilitySavedataShutdownStart = (void*)g_tbl->RelocSyscall(g_tbl->FindImportUserRam("sceUtility", 0x9790B33C));
}
