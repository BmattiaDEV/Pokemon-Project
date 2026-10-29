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

CFLAGS	:=	-Wall -O2 -mword-relocations \
			-fomit-frame-pointer -ffast-math \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# Regole di devkitPro
#---------------------------------------------------------------------------------
include $(DEVKITARM)/base_rules
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Logica interna di Make
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
export BUILDDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES)) $(PICAFILES:.v.pica=.shbin.o)
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES         := $(PICAFILES:.v.pica=_shbin.h) $(addsuffix .h,$(subst .,_,$(BINFILES)))

export LIBDIRS	:=	$(PORTLIBS) $(CTRULIB)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LD	:=	$(CXX)
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

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).3dsx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

# Forza la generazione dei file .h prima di compilare i sorgenti C
$(OFILES_SOURCES) : $(HFILES)

# Regola robusta per Picasso che usa percorsi relativi puliti
%.shbin.o %_shbin.h : %.v.pica
	picasso -o $(notdir $*).shbin $(TOPDIR)/source/render/$(notdir $<)
	bin2s $(notdir $*).shbin > $(notdir $*).s
	$(AS) -o $*.shbin.o $(notdir $*).s
	@echo "extern const u8 $(notdir $*)_shbin[];" > $*_shbin.h
	@echo "extern const u32 $(notdir $*)_shbin_size;" >> $*_shbin.h
	@rm -f $(notdir $*).shbin $(notdir $*).s

-include $(DEPENDS)

endif