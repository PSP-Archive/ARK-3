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

#include "210patch.h"
#include "flashpatch.h"
#include "libs/graphics/graphics.h"

// kermit_peripheral's sub_000007CC clone, called by loadexec + 0x0000299C with a0=8 (was a0=7 for fw <210)
// Returns 0 on success
u64 kermit_flash_load(int cmd)
{
	u8 buf[128];
	u64 resp;
	void *alignedBuf = (void*)ALIGN_64((int)buf + 63);
	k_tbl->KernelDcacheInvalidateRange(alignedBuf, 0x40);
	
	KermitPacket *packet = (KermitPacket *)KERMIT_PACKET((int)alignedBuf);


	u32 argc = 0;
	k_tbl->Kermit_driver_4F75AA05(packet, KERMIT_MODE_PERIPHERAL, cmd, argc, KERMIT_CALLBACK_DISABLE, &resp);

	return resp;
}

int flash_load()
{
	int ret = kermit_flash_load(KERMIT_CMD_ERROR_EXIT);

	// Wait for flash to load
	k_tbl->KernelDelayThread(10000);

	return ret;
}

int flashLoadPatch(int cmd)
{

	int ret = kermit_flash_load(cmd);

	// Custom handling on loadFlash mode, else nothing
	if ( cmd == KERMIT_CMD_ERROR_EXIT || cmd == KERMIT_CMD_ERROR_EXIT_2 )
	{
		int linked;

		// Wait for flash to load
		k_tbl->KernelDelayThread(10000);

		// Patch flash0 Filesystem Driver
		linked = patchFlash0Archive();
		k_tbl->KernelIcacheInvalidateAll();
		k_tbl->KernelDcacheWritebackInvalidateAll();
	}
	
	return ret;
}
