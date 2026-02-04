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

# Build targets
# v1: CLAP only (fast iteration)
TARGETS += clap

# If OpenGL is missing, we still build CLAP (DSP-only); UI will be disabled.
ifeq ($(HAVE_OPENGL),false)
$(warning OpenGL not found - building CLAP DSP-only (no UI))
$(warning Install libgl-dev for UI support: sudo apt install libgl-dev)
FILES_UI =
endif

all: $(TARGETS)

# -----------------------------------------------------------------------------
