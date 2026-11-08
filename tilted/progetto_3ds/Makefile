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
SOURCES     := source source/core source/map source/world source/render
DATA        := data
INCLUDES    := source source/core source/map source/world source/render

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

# Escludiamo .csv e .png e aggiungiamo i file binari generati
BINFILES    := $(foreach dir,$(DATA),$(notdir $(filter-out %.csv %.png,$(wildcard $(dir)/*.*)))) \
               tileset.t3x vshader.shbin

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
# Compilazione Texture (tex3ds) - Usa outdoor.png presente in data/
#---------------------------------------------------------------------------------
tileset.t3x: ../data/outdoor.png
	@echo Converting $(notdir $<) to $@...
	@$(DEVKITPRO)/tools/bin/tex3ds -f rgba8888 -z auto $< -o $@

tileset.t3x.o tileset_t3x.h: tileset.t3x
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------