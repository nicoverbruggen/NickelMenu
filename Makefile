NH_QT_MAJOR ?= 5
NICKELHOOK ?= NickelHook/
include $(patsubst %/,%,$(NICKELHOOK))/NickelHook.mk

override PKGCONF  += Qt$(NH_QT_MAJOR)Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden
override CPPFLAGS += -DNM_QT_MAJOR=$(NH_QT_MAJOR)
ifeq ($(NH_QT_MAJOR),6)
override SOURCES += src/install.c $(patsubst %/,%,$(NICKELHOOK))/resources.c
override CFLAGS += -Wno-error=unused-result
else
override KOBOROOT += res/doc:$(NM_CONFIG_DIR)/doc
endif

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded src/libnm.so
.PHONY: strip

ifeq ($(NM_UNINSTALL_CONFIGDIR),1)
override CPPFLAGS += -DNM_UNINSTALL_CONFIGDIR
endif

ifeq ($(NM_CONFIG_DIR),)
override NM_CONFIG_DIR := /mnt/onboard/.adds/nm
endif

ifneq ($(NM_CONFIG_DIR),/mnt/onboard/.adds/nm)
$(info -- Warning: NM_CONFIG_DIR is set to a non-default value; this will cause issues with other mods using it!)
endif

override CPPFLAGS += -DNM_CONFIG_DIR='"$(NM_CONFIG_DIR)"' -DNM_CONFIG_DIR_DISP='"$(patsubst /mnt/onboard/%,KOBOeReader/%,$(NM_CONFIG_DIR))"'

include $(patsubst %/,%,$(NICKELHOOK))/NickelHook.mk

ifeq ($(NH_QT_MAJOR),6)
override GENERATED += src/embedded_resources.h
src/install.o: src/embedded_resources.h
ifneq ($(wildcard $(patsubst %/,%,$(NICKELHOOK))/embed_resources.c),)
HOSTCC ?= cc
NH_RESOURCE_TOOL := .cache/embed_resources-$(shell uname -s)-$(shell uname -m)
override GENERATED += $(NH_RESOURCE_TOOL)
$(NH_RESOURCE_TOOL): $(patsubst %/,%,$(NICKELHOOK))/embed_resources.c
	mkdir -p $(@D)
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -O2 $< -o $@
	chmod 755 $@
src/embedded_resources.h: res/doc $(NH_RESOURCE_TOOL)
	$(NH_RESOURCE_TOOL) $@ res/doc
else
# Keep the pinned dependency buildable until its generator update is committed.
src/embedded_resources.h: res/doc $(patsubst %/,%,$(NICKELHOOK))/embed_resources.py
	python3 $(patsubst %/,%,$(NICKELHOOK))/embed_resources.py $@ res/doc
endif
endif
