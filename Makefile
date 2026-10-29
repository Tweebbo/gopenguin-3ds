.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error DEVKITARM is not set. Use ./build.sh)
endif
include $(DEVKITARM)/3ds_rules
TARGET := gopenguin-3ds-demo
BUILD := obj
SOURCES := source source/transport vendor/qrcodegen
INCLUDES := source
ROMFS := romfs
ifeq ($(LAN_TEST),1)
TARGET := gopenguin-lan-test
BUILD := obj-lan
ROMFS := build/lan-romfs
endif
ifeq ($(GPU_TEST),1)
TARGET := gopenguin-gpu-test
BUILD := obj-gpu-test
SOURCES := source tests
ROMFS := build/gpu-romfs
endif
ifeq ($(DEBUG_GAMES),1)
TARGET := gopenguin-debug
BUILD := obj-debug
ROMFS := build/debug-romfs
endif
APP_TITLE := GoPenguin
APP_DESCRIPTION := Native island client
APP_AUTHOR := GoPenguin
ifeq ($(DEBUG_GAMES),1)
APP_TITLE := GoPenguin Debug
APP_DESCRIPTION := Local minigame testing
endif
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
GOPENGUIN_VERSION ?= $(shell cat $(if $(TOPDIR),$(TOPDIR),$(CURDIR))/VERSION)
CFLAGS := -g -Wall -Wextra -Werror -O2 -D__3DS__ -DGAME_THINICE_READY -DGOPENGUIN_VERSION=\"$(GOPENGUIN_VERSION)\" -mword-relocations -ffunction-sections -std=gnu11 $(ARCH) $(INCLUDE)
ifeq ($(LAN_TEST),1)
CFLAGS += -DGP_LAN_ONLY
CFLAGS += -DGP_UPDATE_URL=\"http://10.0.2.2:18891/latest.json\"
endif
ifeq ($(GPU_TEST),1)
CFLAGS += -DSWL_GPU_FRAME_QUADS=128
endif
ifeq ($(DEBUG_GAMES),1)
CFLAGS += -DGP_DEBUG_GAMES -DGP_LAN_ONLY
endif
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcitro2d -lcitro3d -ljansson -lmbedtls -lmbedx509 -lmbedcrypto -lz -lctru -lm
LIBDIRS := $(PORTLIBS) $(CTRULIB)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/out/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export LD := $(CC)
export OFILES := main.o demo.o island.o swl.o swl_gpu.o swl_shader.shbin.o common.o penguin_net.o world_state.o settings.o conn.o ws.o certs.o garment.o asset_cache.o wear.o client_data.o profile.o inventory_ui.o overlay.o hash.o package.o movie_meta.o movie_bank.o social.o igloo.o igloo_geometry.o sprite.o gp_audio.o puffles.o newspaper.o membership.o qrcodegen.o
export OFILES += $(notdir $(patsubst %.c,%.o,$(wildcard $(CURDIR)/source/game_*.c)))
export OFILES += $(notdir $(patsubst %.c,%.o,$(wildcard $(CURDIR)/source/ui_*.c)))
export OFILES += lobby.o
export OFILES += room_tube.o
export OFILES += room_npc.o
export OFILES += epf_access.o
export OFILES += pcm_stream.o
export OFILES += offline_shop.o
export OFILES += gp_update.o update_manifest.o download.o
export OFILES += chat_keyboard.o
ifeq ($(GPU_TEST),1)
export OFILES := gpu_test.o swl.o swl_gpu.o swl_shader.shbin.o
endif
export INCLUDE := -I$(CURDIR)/source -I$(CURDIR)/vendor/qrcodegen $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON := $(TOPDIR)/assets/icon.png
export _3DSXDEPS := $(OUTPUT).smdh $(wildcard $(CURDIR)/$(ROMFS)/gfx/*) $(wildcard $(CURDIR)/$(ROMFS)/scenes/*) $(wildcard $(CURDIR)/$(ROMFS)/data/*) $(wildcard $(CURDIR)/$(ROMFS)/*.bin) $(wildcard $(CURDIR)/$(ROMFS)/games/*) $(wildcard $(CURDIR)/$(ROMFS)/audio/*.json) $(wildcard $(CURDIR)/$(ROMFS)/audio/music/*/*.wav) $(wildcard $(CURDIR)/$(ROMFS)/audio/sfx/*/*.wav)
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)
export _3DSXDEPS += $(wildcard $(CURDIR)/$(ROMFS)/books/*.gpk) $(wildcard $(CURDIR)/$(ROMFS)/audio/music/*/*.gpz) $(wildcard $(CURDIR)/$(ROMFS)/audio/sfx/*/*.gpz)
ifeq ($(DEBUG_GAMES),1)
export _3DSXDEPS += $(wildcard $(CURDIR)/$(ROMFS)/games/debug-card-preview/*.gpk)
endif
export _3DSXDEPS += $(wildcard $(CURDIR)/$(ROMFS)/games/card-preview/*.gpk)
export _3DSXDEPS += $(wildcard $(CURDIR)/$(ROMFS)/npcs/*.nav)
.PHONY: all
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $(BUILD) out
else
.DEFAULT_GOAL := $(OUTPUT).3dsx
$(OFILES): $(TOPDIR)/Makefile $(TOPDIR)/VERSION
swl_gpu.o: swl_shader_shbin.h
swl_shader.shbin: $(TOPDIR)/source/swl_shader.v.pica
	picasso -o $@ $<
swl_shader.shbin.o swl_shader_shbin.h: swl_shader.shbin
	$(bin2o)
$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)
-include $(OFILES:.o=.d)
endif
