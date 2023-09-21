# Number of Compilation Threads
OPT=-j8

ARKROOT = $(shell pwd)
PROVITA ?= $(shell pwd)


PYTHON = $(shell which python3)
USE_FLASH0_ARK ?= 1
SAVE ?= -1
K ?= vita360
FLASH_DUMP ?= 0

export ARKROOT DEBUG PROVITA K FLASH_DUMP
export POPSMAN_VERSION POPS_VERSION

SUBDIRS = libs systemctrl ISODrivers/galaxy stargate exitgame menu/provsh menu/arkMenu menu/xMenu popcorn peops ISODrivers/inferno rebootbuffer ark kxploit loader contrib/PC/btcnf
.PHONY: subdirs $(SUBDIRS) cleanobj clean cleanobj distclean copy-bin mkdir-dist encrypt-prx

all: subdirs mkdir-dist copy-bin encrypt-prx

copy-bin: loader/h.bin kxploit/k.bin contrib/PC/btcnf/pspbtinf.bin contrib/PC/btcnf/pspbtxnf.bin contrib/PC/btcnf/pspbtnnf.bin contrib/PC/btcnf/pspbtmnf.bin contrib/PSP/fake.cso menu/provsh/EBOOT.PBP menu/arkMenu/EBOOT.PBP menu/xMenu/EBOOT.PBP
	$(Q)cp loader/h.bin dist/H.BIN
	$(Q)cp ark/ark.bin dist/ARK.BIN
	$(Q)cp kxploit/k.bin dist/K.BIN
	$(Q)cp contrib/PC/btcnf/pspbtinf.bin dist/PSPBTINF.BIN
	$(Q)cp contrib/PC/btcnf/pspbtxnf.bin dist/PSPBTXNF.BIN
	$(Q)cp contrib/PC/btcnf/pspbtnnf.bin dist/PSPBTNNF.BIN
	$(Q)cp contrib/PC/btcnf/pspbtmnf.bin dist/PSPBTMNF.BIN
	$(Q)cp contrib/PSP/fake.cso dist/FAKECSO0.BIN
	$(Q)cp contrib/PSP/qsplink.prx dist/QSPLINK.PRX
	$(Q)cp menu/provsh/EBOOT.PBP dist/RECOVERY.PBP
	$(Q)cp menu/arkMenu/EBOOT.PBP dist/VBOOT.PBP
	$(Q)cp menu/arkMenu/DATA.PKG dist/DATA.PKG
	$(Q)cp menu/xMenu/EBOOT.PBP dist/XBOOT.PBP

encrypt-prx: \
	dist/SYSCTRL0.BIN dist/GALAXY00.BIN dist/INFERNO0.BIN dist/STARGATE.BIN dist/EXITGAME.BIN dist/POPCORN0.BIN dist/PEOPS.PRX dist/MARCH330.BIN\
	dist/POPSMAN0.BIN dist/POPS.PRX dist/PSPVMC00.BIN dist/MEDIASYN.BIN dist/MODULEMR.BIN dist/NPDRM.PRX dist/NP966000.BIN dist/H.BIN
ifeq ($(USE_FLASH0_ARK), 1)
	$(Q)$(PYTHON) contrib/PC/pack/pack.py -p dist/FLASH0.ARK contrib/PC/pack/packlist.txt
else
	$(Q)-rm -f dist/FLASH0.ARK
endif
# in the end always destroy tmp release key cache
	$(Q)-rm -f $(tmpReleaseKey)


# Only clean non-library code
cleanobj:
	$(Q)$(MAKE) clean CLEANOBJ=1

distclean clean:
ifndef CLEANOBJ
	$(Q)$(MAKE) $@ -C libs
endif
	$(Q)$(MAKE) $@ -C rebootbuffer
	$(Q)$(MAKE) $@ -C ark
	$(Q)$(MAKE) $@ -C kxploit
	$(Q)$(MAKE) $@ -C loader
	$(Q)$(MAKE) $@ -C systemctrl
	$(Q)$(MAKE) $@ -C ISODrivers/galaxy
	$(Q)$(MAKE) $@ -C stargate
	$(Q)$(MAKE) $@ -C exitgame
	$(Q)$(MAKE) $@ -C menu/provsh
	$(Q)$(MAKE) $@ -C menu/arkMenu
	$(Q)$(MAKE) $@ -C menu/xMenu
	$(Q)$(MAKE) $@ -C popcorn
	$(Q)$(MAKE) $@ -C peops
	$(Q)$(MAKE) $@ -C ISODrivers/inferno
	$(Q)-rm -rf dist *~ | true
	$(Q)-rm -f contrib/PC/btcnf/pspbtinf.bin
	$(Q)-rm -f contrib/PC/btcnf/pspbtmnf.bin
	$(Q)-rm -f contrib/PC/btcnf/pspbtnnf.bin
	$(Q)-rm -f contrib/PC/btcnf/pspbtxnf.bin
	$(Q)$(PYTHON) cleandeps.py

subdirs: $(SUBDIRS)

$(filter-out libs, $(SUBDIRS)): libs
	$(Q)$(MAKE) $(OPT) -C $@

libs:
	$(Q)$(MAKE) $(OPT) -C $@

arkmenu: libs
	$(Q)$(MAKE) $@ -C menu/arkMenu

xmenu: libs
	$(Q)$(MAKE) $@ -C menu/xMenu

ark: rebootbuffer

loader: ark

mkdir-dist:
	$(Q)mkdir dist | true

-include $(PROVITA)/.config
include $(PROVITA)/common/make/check.mak
include $(PROVITA)/common/make/quiet.mak
include $(PROVITA)/common/make/mod_enc.mak
