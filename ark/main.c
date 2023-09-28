/*
 * This file is part of PRO CFW.

 * PRO CFW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * PRO CFW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PRO CFW. If not, see <http://www.gnu.org/licenses/ .
 */

#include "main.h"
#include <loadexec_patch.h>
#include "libs/graphics/graphics.h"
#include "flash_dump.h"
#include "flashpatch.h"
#include "reboot.h"
#include <functions.h>
#include "210patch.h"

char* savepath = (char*)0x08803000;
char* running_ark = "Running ARK in PS? mode\0";

// Sony Reboot Buffer Loader
int (* _LoadReboot)(void *, unsigned int, void *, unsigned int) = NULL;

// LoadExecVSHWithApitype Direct Call
int (* _KernelLoadExecVSHWithApitype)(int, char *, struct SceKernelLoadExecVSHParam *, int) = NULL;

void initKxploitFile()
{
	char k_path[SAVE_PATH_SIZE];
	strcpy(k_path, savepath);
	strcat(k_path, "K.BIN");
	
	SceUID fd = g_tbl->IoOpen(k_path, PSP_O_RDONLY, 0);
	g_tbl->IoRead(fd, (void *)(KXPLOIT_LOADADDR|0x40000000), 0x4000);
	g_tbl->IoClose(fd);
	void (* kEntryPoint)(KxploitFunctions*, FunctionTable*) = (void*)KXPLOIT_LOADADDR;
	kEntryPoint(kxf, g_tbl);
}

// Entry Point
int exploitEntry(char* arg0) __attribute__((section(".text.startup")));
int exploitEntry(char* arg0)
{

	// copy the path of the save
	strcpy(savepath, arg0);
	
	// Clear BSS Segment
	clearBSS();

	// init function table
	getUserFunctions();

	// make PRTSTR available for payloads
	g_tbl->prtstr = (void *)&PRTSTR11;

	// init screen
	initScreen(g_tbl->DisplaySetFrameBuf);

	// Output Exploit Reach Screen
	PRTSTR("Starting");
	
	// read kxploit file into memory and initialize it
	initKxploitFile();
	
	if (kxf->stubScanner() == 0)
	{
		// Corrupt Kernel
		int ret = kxf->doExploit();
		
		if (ret >= 0)
		{
			// Flush Cache
			g_tbl->KernelDcacheWritebackAll();

			// Refresh screen (vram buffer screws it)
			cls();
			
			// Output Loading Screen
			PRTSTR("Loading");
			
			// Trigger Kernel Permission Callback
			kxf->executeKernel((u32)&kernelContentFunction);
		}
		else{
			PRTSTR("Exploit failed");
			_sw(0,0);
		}
	}
	else{
		PRTSTR("Scan failed");
		_sw(0, 0);
	}

	return 0;
}

#if FLASH_DUMP == 0
int bootRecoveryCheck(char* recovery)
{
	//if (IS_VITA_POPS)
		return 1;
	/*
	// Allocate Buffer for Gamepad Data
	SceCtrlData data;
		
	// Set Sampling Cycle
	k_tbl->KernelCtrlSetSamplingCycle(0);
		snapshot
	// Set Sampling Mode (we don't need the analog stick really)
	k_tbl->KernelCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
		
	// Poll 64 Times
	int i = 0; for(; i < 64; i++)
	{
		// Clear Memory
		memset(&data, 0, sizeof(data));
			
		// Poll Data
		k_tbl->KernelCtrlPeekBufferPositive(&data, 1);
			
		// Recovery Mode
		if((data.Buttons & PSP_CTRL_RTRIGGER) == PSP_CTRL_RTRIGGER)
		{
		
			SceIoStat stat;
				
			// Return Recovery Path
			if (k_tbl->KernelIOGetStat(recovery, &stat) >= 0)	return 1;
			else	break;
		}
	}
	
	// Normal Boot
	return 0;
	*/
}

void runMenu(void)
{
	PRTSTR("running menu");

	char path[SAVE_PATH_SIZE];
	memset(path, 0, SAVE_PATH_SIZE);
	strcpy(path, savepath);
	//strcat(path, (IS_VITA_POPS)?"XBOOT.PBP":"VBOOT.PBP");
	strcat(path, "VBOOT.PBP");

	char recoverypath[SAVE_PATH_SIZE];
	memset(recoverypath, 0, SAVE_PATH_SIZE);
	strcpy(recoverypath, savepath);
	strcat(recoverypath, "RECOVERY.PBP");
	
	// Prepare Homebrew Reboot
	char * ebootpath = path;//(bootRecoveryCheck(recoverypath)) ? (recoverypath) : (path);
	struct SceKernelLoadExecVSHParam param;
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.args = strlen(ebootpath) + 1;
	param.argp = ebootpath;
	param.key = "game";

	// Trigger Reboot
	_KernelLoadExecVSHWithApitype(0x141, ebootpath, &param, 0x10000);
}
#endif

void determineGlobalConfig(){

	// copy savepath
	memset(ark_config->savepath, 0, SAVE_PATH_SIZE);
	strcpy(ark_config->savepath, savepath);
	
	// determine if in Vita POPS (PS1 exploits) mode
	ark_config->is_vita_pops = (char)(k_tbl->KernelFindModuleByName("pspvmc_Library") != NULL);
	setIsVitaPops((int)(ark_config->is_vita_pops));
	
	// detect if can make folder in PSP/GAME
	if (!(ark_config->is_vita_pops)){
		k_tbl->KernelIOMkdir("ms0:/PSP/GAME/ARKTESTFOLDER", 0777);
		ark_config->can_install_game = k_tbl->KernelIORmdir("ms0:/PSP/GAME/ARKTESTFOLDER") == 0;
	}
	
	// determine if can write eboot.pbp
	int test = k_tbl->KernelIOOpen("ms0:/EBOOT.PBP.", PSP_O_CREAT|PSP_O_TRUNC|PSP_O_WRONLY, 0777);
	k_tbl->KernelIOClose(test);
	ark_config->can_write_eboot = k_tbl->KernelIORemove("ms0:/EBOOT.PBP.");
	
	// find game ID
	if (ark_config->is_vita_pops){
		*((u8*)GAMEID) = 0;
	}
	else{
		SceUID fd = k_tbl->KernelIOOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0777);
		u8 skip;
	
		k_tbl->KernelIORead(fd, GAMEID, 4);
		k_tbl->KernelIORead(fd, &skip, 1);
		k_tbl->KernelIORead(fd, GAMEID+4, 5);
	
		k_tbl->KernelIOClose(fd);
	}
}

// Kernel Permission Function
void kernelContentFunction(void)
{
	// Switch to Kernel Permission Level
	setK1Kernel();

	getKernelFunctions();

	//kxf->repairInstruction();
	
	// determine global configuration that affects how ARK behaves
	determineGlobalConfig();
	
	#if FLASH_DUMP == 1
	initKernelThread();
	return;
	#else
	running_ark[17] = (IS_VITA_POPS)? 'X':'P';
	PRTSTR(running_ark);

	// Find LoadExec Module
	SceModule2 * loadexec = k_tbl->KernelFindModuleByName("sceLoadExec");
	
	// Find Reboot Loader Function
	_LoadReboot = (void *)loadexec->text_addr;
	
	// Find LoadExec Functions
	_KernelLoadExecVSHWithApitype = (void *)findFirstJALForFunction("sceLoadExec", "LoadExecForKernel", 0xD8320A28);
	
	// make the common loadexec patches
	patchLoadExecCommon(loadexec, (u32)LoadReboot);

	if (NEWER_FIRMWARE){
		// Redirect KERMIT_CMD_ERROR_EXIT loadFlash function
		u32 knownnids[2] = { 0x3943440D, 0x0648E1A3 /* 3.3X */ };
		u32 swaddress = 0;
		int i;
		for (i = 0; i < 2; i++)
		{
			swaddress = findFirstJALForFunction("sceKermitPeripheral_Driver", "sceKermitPeripheral_driver", knownnids[i]);
			if (swaddress != 0)
				break;
		}
		_sw(JUMP(flashLoadPatch), swaddress);
		_sw(NOP, swaddress+4);
	}
	else{
		// Patch flash0 Filesystem Driver
		if(patchFlash0Archive() < 0)
		{
			return;
		}
	}

	// Invalidate Cache
	k_tbl->KernelIcacheInvalidateAll();
	k_tbl->KernelDcacheWritebackInvalidateAll();

	runMenu();
	#endif
}

// Fake K1 Kernel Setting
void setK1Kernel(void)
{
	// Set K1 to Kernel Value
	__asm__ (
		"nop\n"
		"lui $k1,0x0\n"
	);
}

// Clear BSS Segment of Payload
void clearBSS(void)
{
	// BSS Start and End Address from Linkfile
	extern char __bss_start, __bss_end;
	
	// Clear Memory
	memset(&__bss_start, 0, &__bss_end - &__bss_start);
}
