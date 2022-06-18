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
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <stdio.h>
#include <string.h>
#include <macros.h>
#include <module2.h>
#include <systemctrl.h>
#include <systemctrl_private.h>
#include <offsets.h>
#include <functions.h>
#include "imports.h"
#include "modulemanager.h"
#include "nidresolver.h"
#include "plugin.h"
#include "elf.h"
#include "loadercore.h"
#include "cryptography.h"
#include "rebootconfig.h"

// init.prx Text Address
unsigned int sceInitTextAddr = 0;

// Plugin Loader Status
int pluginLoaded = 0;

// Real Executable Check Function Pointer
int (* ProbeExec1)(u8 *buffer, int *check) = NULL;
int (* ProbeExec2)(u8 *buffer, int *check) = NULL;

// init.prx Custom sceKernelStartModule Handler
int (* customStartModule)(int modid, SceSize argsize, void * argp, int * modstatus, SceKernelSMOption * opt) = NULL;

// Executable Check #1
int _ProbeExec1(u8 *buffer, int *check)
{
	// Check Executable (we patched our files with shifted attributes so this works)
	int result = ProbeExec1(buffer, check);
	
	// Grab Executable Magic
	unsigned int magic = *(unsigned int *)(buffer);
	
	// ELF File
	if(magic == 0x464C457F)
	{
		// Recover Attributes (which we shifted before)
		unsigned short realattr = *(unsigned short *)(buffer + check[19]);
		
		// Mask Attributes
		unsigned short attr = realattr & 0x1E00;
		
		// Kernel Module
		if(attr != 0)
		{
			// Fetch OFW-detected Attributes
			unsigned short attr2 = *(u16*)((void*)(check)+0x58);
			
			// OFW Attributes don't match
			if((attr2 & 0x1E00) != attr)
			{
				// Now they do. :)
				*(u16*)((void*)(check)+0x58) = realattr;
			}
		}
		
		// Flip Switch
		if(check[18] == 0) check[18] = 1;
	}
	
	// Return Result
	return result;
}

// Executable Check #2
int _ProbeExec2(u8 *buffer, int *check)
{
	// Check Executable
	int result = ProbeExec2(buffer, check);
	
	// Grab Executable Magic
	unsigned int magic = *(unsigned int *)(buffer);
	
	// Plain Static ELF Executable
	if(magic == 0x464C457F && IsStaticElf(buffer))
	{
		// Fake UMD Apitype (as its the only one that allows Static ELFs... and even that, only as LoadExec Target)
		check[2] = 0x120;
		
		// Invalid Module Info Section
		if(check[19] == 0)
		{
			// Search String Table
			char * strtab = GetStrTab(buffer);
			
			// Found it! :D
			if(strtab != NULL)
			{
				// Cast ELF Header
				Elf32_Ehdr * header = (Elf32_Ehdr *)buffer;
				
				// Section Header Start Pointer
				unsigned char * pData = buffer + header->e_shoff;
				
				// Iterate Section Headers
				int i = 0; for (; i < header->e_shnum; i++)
				{
					// Cast Section Header
					Elf32_Shdr * section = (Elf32_Shdr *)pData;
					
					// Found Module Info Section
					if(strcmp(strtab + section->sh_name, ".rodata.sceModuleInfo") == 0)
					{
						// Fix Section Pointer
						check[19] = section->sh_offset;
						check[22] = 0;
						
						// Stop Search
						break;
					}
					
					// Move to next Section
					pData += header->e_shentsize;
				}
			}
		}
	}
	
	// Return Result
	return result;
}

// Executable File Check
int KernelCheckExecFile(unsigned char * buffer, int * check)
{
	// Patch Executable
	int result = PatchExec1(buffer, check);
	
	// PatchExec1 isn't enough... :(
	if(result != 0)
	{
		// Check Executable
		int checkresult = sceKernelCheckExecFile(buffer, check);
		
		// Grab Executable Magic
		unsigned int magic = *(unsigned int *)(buffer);
		
		// Patch Executable
		result = PatchExec3(buffer, check, magic == 0x464C457F, checkresult);
	}
	
	// Return Result
	return result;
}

// Init Start Module Hook
int InitKernelStartModule(int modid, SceSize argsize, void * argp, int * modstatus, SceKernelSMOption * opt)
{
	int err;
	SceModule2* mod = (SceModule2*) sceKernelFindModuleByUID(modid);

	// Custom Handler registered
	if(customStartModule != NULL)
	{
		// Forward to Handler
		int result = customStartModule(modid, argsize, argp, modstatus, opt);
		
		// Positive Result
		if(result >= 0) return result;
	}
	
	// Plugins not yet loaded
	if(!pluginLoaded)
	{
		// sceMediaSync not yet loaded... too early to load plugins.
		if(sceKernelFindModuleByName("sceMediaSync") == NULL)
		{
			goto out;
		}

		// Load Plugins
		LoadPlugins();
		
		// Remember it
		pluginLoaded = 1;
	}

out:
	// Passthrough
	err = sceKernelStartModule(modid, argsize, argp, modstatus, opt);

#ifdef DEBUG
	// Log module error message
	printk("mod: 0x%08X, modname: %s, modid: 0x%08X, modstatus: 0x%08X, ret: 0x%08X\n",
		sceKernelFindModuleByUID(modid), (mod!=NULL)?mod->modname:"NULL", modid, *modstatus, err);
	if (err < 0)
	{
		char *modname;
		
		if (mod != NULL)
		{
			modname = mod->modname;
		}
		else
		{
			modname = NULL;
		}

		if (modstatus != NULL)
		{
			printk("%s: modname %s, modstatus 0x%08X -> 0x%08X\r\n", __func__, modname, *modstatus, err);
		}
		else
		{
			printk("%s: modname %s -> 0x%08X\r\n", __func__, modname, err);
		}
	}
#endif

	return err;
}

// sceKernelStartModule Hook
int patch_sceKernelStartModule_in_bootstart(int (* bootstart)(SceSize, void *), void * argp)
{
	
	u32 StartModule = JUMP(FindFunction("sceModuleManager", "ModuleMgrForUser", 0x50F0C1EC));
	
	u32 addr = (u32)bootstart;
	for (;; addr+=4){
		if (_lw(addr) == StartModule)
			break;
	}
	
	// Replace Stub
	_sw(JUMP(InitKernelStartModule), addr);
	_sw(NOP, addr + 4);
	
	// Passthrough
	return bootstart(4, argp);
}

// Patch Loader Core Module
void patchLoaderCore(void)
{

	// Find Module
	SceModule2 * mod = (SceModule2 *)sceKernelFindModuleByName("sceLoaderCore");
	
	// Fetch Text Address
	u32 addr = mod->text_addr;
	u32 topaddr = mod->text_addr+mod->text_size;
	
	// override the checkExec reference in the module globals
	u32 checkExec = sctrlHENFindFunction("sceLoaderCore", "LoadCoreForKernel", 0xD3353EC4);
	u32 ref = findRefInGlobals("LoadCoreForKernel", checkExec, checkExec);
	_sw((unsigned int)KernelCheckExecFile, ref);

	// Fix memlmd_EF73E85B Calls that we broke intentionally in Reboot Buffer
	// Not doing this will keep them pointing into Reboot Buffer... which gets unloaded...
	// It would crash...
	typedef struct loadCoreBackup{
		u32 data;
		u32* addrs[2];
	}loadCoreBackup;
	
	loadCoreBackup* backup = (loadCoreBackup*)0x08D20000;
	*(backup->addrs[0]) = backup->data;
	*(backup->addrs[1]) = backup->data;
	
	// start the dynamic patching
	for (; addr<topaddr; addr+=4){
		u32 data = _lw(addr);
		
		if (data == JAL(checkExec)){
			// Hook sceKernelCheckExecFile
			_sw(JAL(KernelCheckExecFile), addr);
		}
		else if (data == 0x02E0F809 && _lw(addr+4) == 0x24040004){
			// Hook sceInit StartModule Call
			_sw(JAL(patch_sceKernelStartModule_in_bootstart), addr);
			// Move Real Bootstart into Argument #1
			_sw(0x02E02021, addr+4);
		}
		else{
			switch (data){
			case 0x30ABFFFF:	ProbeExec1 = (void *)addr-0x100;	break;		// Executable Check Function #1
			case 0x01E63823:	ProbeExec2 = (void *)addr-0x78;		break;		// Executable Check Function #2
			case 0x30894000: 	_sw(0x3C090000, addr);				break;		// Allow Syscalls
			case 0x14A0FFCB:	_sh(0x1000, addr + 2);				break;		// Remove POPS Check
			case 0x14C0FFDF:	_sw(NOP, addr);						break;		// Remove Invalid PRX Type (0x80020148) Check
			}
		}
	}
	
	// Patch Relocation Type 7 to 0 (this makes more homebrews load)
	addr = ref; // addr = mod->text_addr would also work, we generally just want it to be pointing at the code
	while (strcmp((char*)addr, "sceSystemModule")) addr++; // scan for this string, reloc_type comes a few fixed bytes after
	_sw(_lw(addr+0x7C), addr+0x98);
	
	// Hook Executable Checks
	for (addr=mod->text_addr; addr<topaddr; addr+=4){
		if (_lw(addr) == JAL(ProbeExec1))
			_sw(JAL(_ProbeExec1), addr);
		else if (_lw(addr) == JAL(ProbeExec2))
			_sw (JAL(_ProbeExec2), addr);
	}
	
	// Setup NID Resolver
	setupNidResolver(mod->text_addr);
}

