VERSION ?= $(shell git rev-parse --short=8 HEAD)
WONDERFUL_TOOLCHAIN ?= /opt/wonderful
WF_TARGET = wswan/small
include $(WONDERFUL_TOOLCHAIN)/target/$(WF_TARGET)/makedefs.mk

TARGET ?= generic

OBJDIR := obj/$(TARGET)
ELF_STAGE1 := obj/$(TARGET)_stage1.elf
MKDIRS := $(OBJDIR)
LIBS := -lwsx -lws
CFLAGS := $(WF_ARCH_CFLAGS) -I$(WF_TARGET_DIR)/include -Os -fno-jump-tables -ffunction-sections -DVERSION=\"$(VERSION)\"
LDFLAGS := -T$(WF_LDSCRIPT) $(WF_ARCH_LDFLAGS) -L$(WF_TARGET_DIR)/lib

SRCDIRS := obj/assets src src/$(TARGET)
CSOURCES := $(foreach dir,$(SRCDIRS),$(notdir $(wildcard $(dir)/*.c)))
ASMSOURCES := $(foreach dir,$(SRCDIRS),$(notdir $(wildcard $(dir)/*.s)))
OBJECTS := $(CSOURCES:%.c=$(OBJDIR)/%.o) $(ASMSOURCES:%.s=$(OBJDIR)/%.o)

CFLAGS += -Iobj/assets -Isrc -DTARGET_$(TARGET)

DEPS := $(OBJECTS:.o=.d)
CFLAGS += -MMD -MP

vpath %.c $(SRCDIRS)
vpath %.s $(SRCDIRS)

.PHONY: all clean install

all: CartFriend_$(TARGET).wsc compile_commands.json

CartFriend_$(TARGET).wsc: $(ELF_STAGE1)
	$(BUILDROM) -v -o $@ --output-elf $@.elf $<

$(ELF_STAGE1): $(OBJECTS) | $(OBJDIR)
	$(CC) -r -o $(ELF_STAGE1) $(OBJECTS) $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MJ $(patsubst %.o,%.cc.json,$@) -c -o $@ $<

$(OBJDIR)/%.o: %.s | $(OBJDIR)
	$(CC) -x assembler-with-cpp $(CFLAGS) -MJ $(patsubst %.o,%.cc.json,$@) -c -o $@ $<

$(OBJDIR):
	$(info $(shell mkdir -p $(MKDIRS)))

clean:
	rm -r $(OBJDIR)/*
	rm CartFriend_$(TARGET).wsc CartFriend_$(TARGET).elf

compile_commands.json: $(OBJECTS) | Makefile
	@echo "  MERGE   compile_commands.json"
	$(_V)$(WF)/bin/wf-compile-commands-merge $@ $(patsubst %.o,%.cc.json,$^)

-include $(DEPS)
