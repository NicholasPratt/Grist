#!/usr/bin/make -f
# Top-level Makefile for Grist

# Default target
# v1: build CLAP only (no LV2 TTL step)
all: plugins

# Build all plugins
plugins:
	$(MAKE) -C plugins/Grist

# (LV2 TTL generation removed for v1; CLAP-only)

# Clean build artifacts
clean:
	$(MAKE) -C plugins/Grist clean

# Generate compilation database for IDE support
compdb:
	$(MAKE) -C plugins/Grist compdb

# Install to system plugin directories
install:
	@echo "Installing plugins..."
	@mkdir -p ~/.clap
	@if [ -f bin/Grist.clap ]; then \
		cp bin/Grist.clap ~/.clap/; \
		echo "  CLAP installed to ~/.clap/"; \
	fi
	@echo "Installation complete!"

# Uninstall plugins
uninstall:
	@echo "Uninstalling plugins..."
	@rm -f ~/.clap/Grist.clap
	@echo "Uninstall complete!"

.PHONY: all plugins clean compdb install uninstall
