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
#include <functions.h>

//FunctionTable extern *g_tbl = NULL;

int (* WlanGetEtherAddr)(unsigned char *destAddr) = NULL;
void (* KernelLibcTime)(int, int) = NULL;

// Load required Libraries for WlanGetEtherAddr
int stubScanner(FunctionTable* tbl)
{

	g_tbl = tbl;

	// Load Modules via sceUtilityLoadModule (if available)
	g_tbl->UtilityLoadModule(PSP_MODULE_NET_COMMON);	// 0x100
	g_tbl->UtilityLoadModule(PSP_MODULE_NET_ADHOC);	// 0x101
	
	KernelLibcTime = (void*)g_tbl->KernelLibcTime;
	
	// Find WlanGetEtherAddr in Memory
	WlanGetEtherAddr = (void*)g_tbl->FindImportUserRam("sceWlanDrv", 0x0C622081);

	return 0;
}

int doExploit(void)
{
	unsigned char *sysmemAddr = (unsigned char *)0x8800F718;

	WlanGetEtherAddr(sysmemAddr);
	WlanGetEtherAddr(sysmemAddr - 2);
	WlanGetEtherAddr(sysmemAddr - 4);
	WlanGetEtherAddr(sysmemAddr - 6);

	return 0;
}

void executeKernel(u32 kernelContentFunction)
{
	KernelLibcTime(0, KERNELIFY(kernelContentFunction));
}

void repairInstruction(void)
{
	// checked in 1.61, 1.67, 1.69 1.80
	_sw(0x0200D821, 0x8800F718 - 4);
	_sw(0x3C038801, 0x8800F718);
	_sw(0x8C654384, 0x8800F718 + 4);
}
