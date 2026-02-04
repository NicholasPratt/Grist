#!/usr/bin/make -f
# Top-level Makefile for Transistor Distortion Plugin

# Default target
all: plugins lv2-ttl

# Build all plugins
plugins:
	$(MAKE) -C plugins/Grist

# Build LV2 TTL generator (needed for LV2 metadata)
dpf/utils/lv2_ttl_generator:
	$(MAKE) -C dpf/utils/lv2-ttl-generator

# Generate LV2 TTL files
lv2-ttl: dpf/utils/lv2_ttl_generator
	@if [ -d bin/Grist.lv2 ]; then \
		cd bin/Grist.lv2 && \
		if [ -f Grist_dsp.so ]; then \
			../../dpf/utils/lv2_ttl_generator ./Grist_dsp.so; \
		elif [ -f Grist.so ]; then \
			../../dpf/utils/lv2_ttl_generator ./Grist.so; \
		fi \
	fi

# Clean build artifacts
clean:
	$(MAKE) -C plugins/Grist clean

# Generate compilation database for IDE support
compdb:
	$(MAKE) -C plugins/Grist compdb

# Install to system plugin directories
install:
	@echo "Installing plugins..."
	@mkdir -p ~/.lv2
	@mkdir -p ~/.vst
	@mkdir -p ~/.vst3
	@mkdir -p ~/.clap
	@if [ -d bin/Grist.lv2 ]; then \
		cp -r bin/Grist.lv2 ~/.lv2/; \
		echo "  LV2 installed to ~/.lv2/"; \
	fi
	@if [ -f bin/Grist-vst.so ]; then \
		cp bin/Grist-vst.so ~/.vst/; \
		echo "  VST2 installed to ~/.vst/"; \
	fi
	@if [ -d bin/Grist.vst3 ]; then \
		cp -r bin/Grist.vst3 ~/.vst3/; \
		echo "  VST3 installed to ~/.vst3/"; \
	fi
	@if [ -f bin/Grist.clap ]; then \
		cp bin/Grist.clap ~/.clap/; \
		echo "  CLAP installed to ~/.clap/"; \
	fi
	@echo "Installation complete!"

# Uninstall plugins
uninstall:
	@echo "Uninstalling plugins..."
	@rm -rf ~/.lv2/Grist.lv2
	@rm -f ~/.vst/Grist-vst.so
	@rm -rf ~/.vst3/Grist.vst3
	@rm -f ~/.clap/Grist.clap
	@echo "Uninstall complete!"

.PHONY: all plugins clean compdb install uninstall lv2-ttl
