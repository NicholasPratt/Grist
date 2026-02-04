#!/usr/bin/make -f
# Makefile for Grist plugin

# Plugin name
NAME = Grist

# Files to build
FILES_DSP = \
	Grist.cpp

FILES_UI = \
	GristUI.cpp

# -----------------------------------------------------------------------------
# Do some magic

DPF_PATH = ../../dpf

# Set build directory to local path
DPF_BUILD_DIR = ../../build/$(NAME)
DPF_TARGET_DIR = ../../bin

# UI type for NanoVG
UI_TYPE = opengl

include $(DPF_PATH)/Makefile.plugins.mk

# Build targets (only when OpenGL is available for UI)
ifeq ($(HAVE_OPENGL),true)
TARGETS += jack
TARGETS += lv2_sep
TARGETS += vst2
TARGETS += vst3
TARGETS += clap
else
$(warning OpenGL not found - building DSP-only plugins without UI)
$(warning Install libgl-dev for UI support: sudo apt install libgl-dev)
# Clear UI files when building without OpenGL
FILES_UI =
TARGETS += lv2
TARGETS += vst2
TARGETS += vst3
TARGETS += clap
endif

all: $(TARGETS)

# -----------------------------------------------------------------------------
