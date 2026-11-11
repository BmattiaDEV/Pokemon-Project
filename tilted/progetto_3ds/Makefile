#---------------------------------------------------------------------------------
# Makefile definitivo per 3DS - Corretto
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET, BUILD, SOURCES, DATA, INCLUDES
#---------------------------------------------------------------------------------
TARGET      := $(notdir $(CURDIR))
BUILD       := build
SOURCES     := source source/core source/map source/player source/render
DATA        := data
INCLUDES    := source source/core source/map source/player source/render

#---------------------------------------------------------------------------------
# Opzioni di compilazione
#---------------------------------------------------------------------------------
ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS      := -g -Wall -O2 -mword-relocations \
               -ffunction-sections \
               $(ARCH)

CFLAGS      += $(INCLUDE) -D__3DS__

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS     := -g $(ARCH)
LDFLAGS     = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS        := -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# Librerie
#---------------------------------------------------------------------------------
LIBDIRS     := $(CTRULIB)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

# Includiamo eventuali .bin in data/ e i file binari generati (.t3x e .shbin)
# Texture della mappa DS: ogni data/ds_map/*.png diventa un .t3x (vedi ds_map/LEGGIMI.md)
DS_MAP_TEX  := $(patsubst %.png,%.t3x,$(notdir $(wildcard $(DATA)/ds_map/*.png)))

BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin))) \
               player.t3x flower_overlay.t3x grass_overlay.t3x \
               battle_bg.t3x battle_base_enemy.t3x battle_base_player.t3x battle_opponents.t3x battle_player.t3x \
               vshader.shbin $(DS_MAP_TEX)

#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
    export LD   := $(CC)
else
    export LD   := $(CXX)
endif
#---------------------------------------------------------------------------------

export OFILES_BIN   := $(addsuffix .o,$(BINFILES))
export OFILES_SRC   := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES       := $(OFILES_BIN) $(OFILES_SRC)
export HFILES       := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE      := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                       $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                       -I$(CURDIR)/$(BUILD)

export LIBPATHS     := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@for d in $@/*.d; do \
		if [ -f "$$d" ]; then \
			src=$$(sed -n '1s/^[^:]*: *\([^ \\]*\).*/\1/p' "$$d"); \
			if [ -n "$$src" ] && [ ! -f "$$src" ]; then \
				echo "Percorso del progetto cambiato, pulizia file di build obsoleti..."; \
				rm -f $@/*.d $@/*.o; \
				break; \
			fi; \
		fi; \
	done
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Target principale
#---------------------------------------------------------------------------------
all : $(OUTPUT).3dsx

$(OUTPUT).3dsx  : $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf   : $(OFILES)

# Forziamo i sorgenti C ad aspettare che bin2o abbia generato gli header .h
$(OFILES_SRC)   : $(OFILES_BIN)

#---------------------------------------------------------------------------------
# Regola base per bin2o
#---------------------------------------------------------------------------------
%.bin.o : %.bin
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# Compilazione Shader (picasso)
#---------------------------------------------------------------------------------
vshader.shbin: ../source/render/vshader.vsh
	@echo Compiling $(notdir $<)...
	@$(DEVKITPRO)/tools/bin/picasso -o $@ $<

vshader.shbin.o vshader_shbin.h: vshader.shbin
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# Compilazione Texture Giocatore (tex3ds)
#---------------------------------------------------------------------------------
player.t3x: ../data/player.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

player.t3x.o player_t3x.h: player.t3x
	@echo $(notdir $<)
	@$(bin2o)

flower_overlay.t3x: ../data/flower_overlay.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

flower_overlay.t3x.o flower_overlay_t3x.h: flower_overlay.t3x
	@echo $(notdir $<)
	@$(bin2o)

grass_overlay.t3x: ../data/grass_overlay.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

grass_overlay.t3x.o grass_overlay_t3x.h: grass_overlay.t3x
	@echo $(notdir $<)
	@$(bin2o)

battle_bg.t3x: ../data/battle_bg.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

battle_bg.t3x.o battle_bg_t3x.h: battle_bg.t3x
	@echo $(notdir $<)
	@$(bin2o)

battle_base_enemy.t3x: ../data/battle_base_enemy.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

battle_base_enemy.t3x.o battle_base_enemy_t3x.h: battle_base_enemy.t3x
	@echo $(notdir $<)
	@$(bin2o)

battle_base_player.t3x: ../data/battle_base_player.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

battle_base_player.t3x.o battle_base_player_t3x.h: battle_base_player.t3x
	@echo $(notdir $<)
	@$(bin2o)

battle_opponents.t3x: ../data/battle_opponents.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

battle_opponents.t3x.o battle_opponents_t3x.h: battle_opponents.t3x
	@echo $(notdir $<)
	@$(bin2o)

battle_player.t3x: ../data/battle_player.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

battle_player.t3x.o battle_player_t3x.h: battle_player.t3x
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# Texture della mappa DS (regola generica per ogni PNG in data/ds_map/)
#---------------------------------------------------------------------------------
%.t3x: ../data/ds_map/%.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

%.t3x.o %_t3x.h: %.t3x
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
