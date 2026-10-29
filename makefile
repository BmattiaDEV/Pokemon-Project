#---------------------------------------------------------------------------------
# Impostazioni principali del progetto
#---------------------------------------------------------------------------------
TARGET		:=	Motore3D
BUILD		:=	build

SOURCES		:=	source source/core source/render source/world source/camera source/save source/player source/encounter
DATA		:=	data/bin
INCLUDES	:=	include
ROMFS		:=	romfs

#---------------------------------------------------------------------------------
# Opzioni di compilazione
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-Wall -O2 -mword-relocations -fomit-frame-pointer -ffast-math $(ARCH)
CFLAGS	+=	$(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
ASFLAGS		:= -g $(ARCH)
LDFLAGS		= -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS		:= -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# Regole Base
#---------------------------------------------------------------------------------
include $(DEVKITARM)/base_rules
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Logica di configurazione Make
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export BUILDDIR	:=	$(CURDIR)/$(BUILD)

export LIBDIRS	:=	$(PORTLIBS) $(CTRULIB)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)
export LD		:=	$(CXX)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

clean:
	@echo Pulisco i file generati...
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

else

#---------------------------------------------------------------------------------
# Compilazione vera e propria (Dentro la cartella build)
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.c)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.v.pica)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.bin)))

export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES)) $(PICAFILES:.v.pica=.shbin.o)
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES         := $(PICAFILES:.v.pica=_shbin.h) $(addsuffix .h,$(subst .,_,$(BINFILES)))

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).3dsx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

# Regola d'oro: Genera texture e shader PRIMA del codice C
$(OFILES_SOURCES) : $(HFILES)

# -----------------------------------------------------------------------------
# REGOLE MANUALI BLINDATE ANTI-WINDOWS/MSYS
# Senza l'uso di pipe complessi (|) o doppi apici (") che fanno impazzire la bash
# -----------------------------------------------------------------------------

# 1. Da Binario (.bin) a Oggetto (.bin.o) e Header (_bin.h)
%.bin.o %_bin.h : %.bin
	@echo "-> Convertendo l'immagine: $(notdir $<)"
	@cp $< $(notdir $<)
	@bin2s -H $*_bin.h $(notdir $<) > $*.s
	@$(AS) -o $*.bin.o $*.s
	@rm -f $*.s $(notdir $<)

# 2. Da Shader PICA (.v.pica) a Oggetto (.shbin.o) e Header (_shbin.h)
%.shbin.o %_shbin.h : %.v.pica
	@echo "-> Compilando lo Shader: $(notdir $<)"
	@picasso -o $*.shbin $<
	@bin2s -H $*_shbin.h $*.shbin > $*.s
	@$(AS) -o $*.shbin.o $*.s
	@rm -f $*.s $*.shbin

-include $(DEPENDS)

endif