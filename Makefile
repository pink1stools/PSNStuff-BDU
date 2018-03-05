# -------------------------------------------------------------------
# gamePKG Makefile [by CaptainCPS-X, 2012-2013]
# -------------------------------------------------------------------
APP_VER			=1.00
DATE 			=$(shell date +"%Y%m%d")

PSN_PKG_NPDRM 	= bin/psn_package_npdrm

SELF_APP_VER	=0001000000000000
CONTENT_ID		=BDUPS3-PSNBDU123_00-0000000000000000
MAKE_SELF_NPDRM = scetool.exe \
	--sce-type=SELF \
	--compress-data=TRUE \
	--skip-sections=FALSE \
	--self-ctrl-flags=4000000000000000000000000000000000000000000000000000000000000002 \
	--key-revision=04 \
	--self-auth-id=1010000001000003 \
	--self-app-version=$(SELF_APP_VER) \
	--self-add-shdrs=TRUE \
	--self-vendor-id=01000002 \
	--self-type=NPDRM \
	--self-fw-version=0003004000000000 \
	--np-license-type=FREE \
	--np-content-id=$(CONTENT_ID) \
	--np-app-type=EXEC \
	--np-real-fname="EBOOT.BIN" \
	--encrypt

MAKE_RELOAD_FSELF = scetool.exe \
	--sce-type=SELF \
	--compress-data=TRUE \
	--skip-sections=TRUE \
	--self-ctrl-flags=4000000000000000000000000000000000000000000000000000000000000002 \
	--key-revision=04 \
	--self-auth-id=1010000001000003 \
	--self-app-version=$(SELF_APP_VER) \
	--self-vendor-id=01000002 \
	--self-type=APP \
	--self-fw-version=0003004000000000 \
	--encrypt
	
# -------------------------------------------------------------------

CELL_MK_DIR 	= $(CELL_SDK)/samples/mk
include $(CELL_MK_DIR)/sdk.makedef.mk

PPU_TARGET		= EBOOT.ELF
SRC_EXTENSIONS	= c cpp cc
PPU_SRCS		= $(wildcard $(patsubst %,src/*.%,$(SRC_EXTENSIONS)))
PPU_INCDIRS		+= -I$(CELL_FW_DIR)/include -Iinclude
PPU_OPTIMIZE_LV	= -O2
PPU_CPPFLAGS	+=  -DPSGL
PPU_CXXSTDFLAGS	+= -fno-exceptions
PPU_LIBS		+=  $(CELL_FW_DIR)/libfw.a
PPU_LDLIBDIR	+=  -L$(CELL_FW_DIR) -L$(PPU_PSGL_LIBDIR)
PPU_LDLIBS		+=  -lPSGL -lPSGLU -lPSGLFX -lm -lresc_stub -lgcm_cmd -lgcm_sys_stub -lnet_stub -lio_stub -lsysutil_stub -lsysmodule_stub -lfs_stub -ldbgfont -lfont_stub -lfontFT_stub -lfreetype_stub

include $(CELL_MK_DIR)/sdk.target.mk

# -------------------------------------------------------------------

gamePKG: $(PPU_TARGET)
	@$(PPU_STRIP) -s $(PPU_TARGET) -o $(OBJS_DIR)/$(PPU_TARGET)
	@cd bin/ && $(MAKE_RELOAD_FSELF) ../$(OBJS_DIR)/$(PPU_TARGET) ../release/PS3_GAME/USRDIR/RELOAD.SELF && cd ../
	@rm $(PPU_TARGET)
	@cd bin/ && $(MAKE_SELF_NPDRM) ../$(OBJS_DIR)/$(PPU_TARGET) ../release/PS3_GAME/USRDIR/EBOOT.BIN && cd ../
	@$(PSN_PKG_NPDRM) release/package.conf release/PS3_GAME/
	@mv *.pkg release/PSNStuff_BDU_$(APP_VER)_[$(DATE)].pkg	

# -------------------------------------------------------------------
