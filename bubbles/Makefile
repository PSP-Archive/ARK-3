all:
	$(Q)make $@ -C bubble_loader/sysmem $(OPT)
	$(Q)make $@ -C  bubble_loader $(OPT)
	$(Q)sign_np -elf bubble_loader/ARK3.elf EBOOT.BIN 00
	$(Q)mv EBOOT.BIN disc0/psp_game/sysdir/EBOOT.BIN
	$(Q)cp bubble_loader/PARAM.SFO disc0/psp_game/param.sfo
	# TODO: create script to generate ISO from disc0

clean:
	$(Q)make $@ -C bubble_loader/sysmem $(OPT)
	$(Q)make $@ -C  bubble_loader $(OPT)
	$(Q)rm EBOOT.BIN
