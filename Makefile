#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	Quake2CTR
BUILD		:=	build
SOURCES		:=	.
APP_AUTHOR := MasterFeizz
APP_TITLE := Quake2CTR
APP_DESCRIPTION := Port of Quake 2
DATA		:=	ctr/assets/
INCLUDES	:=	.

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS	:=	-g -Wall -O2 -mword-relocations -ffast-math \
			-fomit-frame-pointer -ffunction-sections \
			-fno-short-enums $(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS -DREF_HARD_LINKED -DGAME_HARD_LINKED -DOGG_SUPPORT -DVORBIS_USE_TREMOR

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:=  -lpicaGL -lctru -lm -lvorbisidec -logg

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(DEVKITPRO)/picaGL $(DEVKITPRO)/portlibs/3ds


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

SYSTEM = 	ctr/cd_ctr.o \
			ctr/snddma_ctr.o \
			ctr/sys_ctr.o \
			ctr/in_ctr.o \
			ctr/net_ctr.o \
			ctr/touch_ctr.o \
			ctr/vid_ctr.o \
			ctr/heap_ctr.o \
			ctr/glimp_ctr.o \
			ctr/qgl_ctr.o \
			ctr/glob.o

CLIENT =	client/cl_input.o \
			client/cl_inv.o \
			client/cl_main.o \
			client/cl_cin.o \
			client/cl_ents.o \
			client/cl_fx.o \
			client/cl_parse.o \
			client/cl_pred.o \
			client/cl_scrn.o \
			client/cl_tent.o \
			client/cl_view.o \
			client/menu.o \
			client/console.o \
			client/keys.o \
			client/snd_dma.o \
			client/snd_mem.o \
			client/snd_mix.o \
			client/snd_stream.o \
			client/snd_wavstream.o \
			client/qmenu.o \
			client/cl_newfx.o

REFGL = 	ref_gl/gl_draw.o \
			ref_gl/gl_image.o \
			ref_gl/gl_light.o \
			ref_gl/gl_mesh.o \
			ref_gl/gl_model.o \
			ref_gl/gl_rmain.o \
			ref_gl/gl_rmisc.o \
			ref_gl/gl_rsurf.o \
			ref_gl/gl_warp.o

QCOMMON = 	qcommon/cmd.o \
			qcommon/cmd_auto.o \
			qcommon/cmodel.o \
			qcommon/common.o \
			qcommon/crc.o \
			qcommon/cvar.o \
			qcommon/files.o \
			qcommon/md4.o \
			qcommon/net_chan.o \
			qcommon/pmove.o 

SERVER = 	server/sv_ccmds.o \
			server/sv_ents.o \
			server/sv_game.o \
			server/sv_init.o \
			server/sv_main.o \
			server/sv_send.o \
			server/sv_user.o \
			server/sv_world.o

GAME = 		game/m_tank.o \
			game/p_client.o \
			game/p_hud.o \
			game/p_trail.o \
			game/p_view.o \
			game/p_weapon.o \
			game/q_shared.o \
			game/g_ai.o \
			game/g_chase.o \
			game/g_cmds.o \
			game/g_combat.o \
			game/g_func.o \
			game/g_items.o \
			game/g_main.o \
			game/g_misc.o \
			game/g_monster.o \
			game/g_phys.o \
			game/g_save.o \
			game/g_spawn.o \
			game/g_svcmds.o \
			game/g_target.o \
			game/g_trigger.o \
			game/g_turret.o \
			game/g_utils.o \
			game/g_weapon.o \
			game/m_actor.o \
			game/m_berserk.o \
			game/m_boss2.o \
			game/m_boss3.o \
			game/m_boss31.o \
			game/m_boss32.o \
			game/m_brain.o \
			game/m_chick.o \
			game/m_flash.o \
			game/m_flipper.o \
			game/m_float.o \
			game/m_flyer.o \
			game/m_gladiator.o \
			game/m_gunner.o \
			game/m_hover.o \
			game/m_infantry.o \
			game/m_insane.o \
			game/m_medic.o \
			game/m_move.o \
			game/m_mutant.o \
			game/m_parasite.o \
			game/m_soldier.o \
			game/m_supertank.o


CFILES		:=	$(CLIENT) $(QCOMMON) $(SERVER) $(GAME) $(REFGL) $(SYSTEM)
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	mkdir -p build/client build/qcommon build/server build/ref_gl build/game build/ctr
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh
else
$(OUTPUT).3dsx	:	$(OUTPUT).elf
endif
$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

# WARNING: This is not the right way to do this! TODO: Do it right!
#---------------------------------------------------------------------------------
%.vsh.o	:	%.vsh
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@python $(AEMSTRO)/aemstro_as.py $< ../$(notdir $<).shbin
	@bin2s ../$(notdir $<).shbin | $(PREFIX)as -o $@
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u32" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@rm ../$(notdir $<).shbin

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
