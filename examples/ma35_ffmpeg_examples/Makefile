# Flexible Makefile for FFmpeg AMA examples
# Supports both system installation and custom FFmpeg builds
#
# Usage:
#   make                                    # Use default system FFmpeg
#   make PREFIX=/path/to/ffmpeg            # Use custom FFmpeg installation
#   make PREFIX=/tmp/ffbuild2              # Use specific build directory

# Default installation prefix (system FFmpeg location)
PREFIX ?= /opt/amd/ama/ma35

# VPI installation path (can be different from FFmpeg)
# Default to system location, but allow override
VPI_PATH ?= /opt/amd/ama/ma35

# Determine if we're using system or custom FFmpeg
ifeq ($(PREFIX),/opt/amd/ama/ma35)
    # System installation mode
    FFMPEG_PKG_CONFIG_PATH = $(PREFIX)/lib/pkgconfig
    FFMPEG_INCLUDE_PATH = $(PREFIX)/include
    FFMPEG_LIB_PATH = $(PREFIX)/lib
else
    # Custom build mode
    # Check common locations for pkg-config files in custom builds
    ifneq ($(wildcard $(PREFIX)/lib/pkgconfig),)
        FFMPEG_PKG_CONFIG_PATH = $(PREFIX)/lib/pkgconfig
    else ifneq ($(wildcard $(PREFIX)/lib64/pkgconfig),)
        FFMPEG_PKG_CONFIG_PATH = $(PREFIX)/lib64/pkgconfig
    else ifneq ($(wildcard $(PREFIX)/pkgconfig),)
        FFMPEG_PKG_CONFIG_PATH = $(PREFIX)/pkgconfig
    else
        # Fallback for builds without pkg-config
        FFMPEG_PKG_CONFIG_PATH = 
    endif
    
    # Set include and lib paths
    FFMPEG_INCLUDE_PATH = $(PREFIX)/include
    ifneq ($(wildcard $(PREFIX)/lib),)
        FFMPEG_LIB_PATH = $(PREFIX)/lib
    else ifneq ($(wildcard $(PREFIX)/lib64),)
        FFMPEG_LIB_PATH = $(PREFIX)/lib64
    else
        FFMPEG_LIB_PATH = $(PREFIX)
    endif
endif

# VPI paths
VPI_INCLUDE_PATH = $(VPI_PATH)/include/vpi
VPI_LIB_PATH = $(VPI_PATH)/lib

# FFmpeg libraries to link
FFMPEG_LIBS = libavdevice   \
              libavformat   \
              libavfilter   \
              libavcodec    \
              libswresample \
              libswscale    \
              libavutil

# Compiler flags
CFLAGS = -Wall -g

# Add include paths
CFLAGS += -I$(PREFIX)
CFLAGS += -I$(FFMPEG_INCLUDE_PATH)
CFLAGS += -I$(VPI_INCLUDE_PATH)

# Try to use pkg-config if available
ifneq ($(FFMPEG_PKG_CONFIG_PATH),)
    PKG_CONFIG_FLAGS := $(shell PKG_CONFIG_PATH=$(FFMPEG_PKG_CONFIG_PATH) pkg-config --cflags $(FFMPEG_LIBS) 2>/dev/null)
    ifneq ($(PKG_CONFIG_FLAGS),)
        CFLAGS += $(PKG_CONFIG_FLAGS)
    endif
endif

# Linker flags
LDFLAGS = -L$(FFMPEG_LIB_PATH) -Wl,-rpath,$(FFMPEG_LIB_PATH)
LDFLAGS += -L$(VPI_LIB_PATH) -Wl,-rpath,$(VPI_LIB_PATH)

# Libraries to link
LDLIBS = 

# Try to use pkg-config for libraries if available
ifneq ($(FFMPEG_PKG_CONFIG_PATH),)
    PKG_CONFIG_LIBS := $(shell PKG_CONFIG_PATH=$(FFMPEG_PKG_CONFIG_PATH) pkg-config --libs $(FFMPEG_LIBS) 2>/dev/null)
    ifneq ($(PKG_CONFIG_LIBS),)
        LDLIBS += $(PKG_CONFIG_LIBS)
    else
        # Fallback to manual library specification
        LDLIBS += -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil
    endif
else
    # No pkg-config, specify libraries manually
    LDLIBS += -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil
endif

# Add VPI and XRM libraries
LDLIBS += -lvpi -lxrm

# Add system libraries
LDLIBS += -lpthread -lm -ldl

# Example programs to build
EXAMPLES = ama_hwdownload_ama           \
           ama_hwframe_transfer_data    \
           ama_long_term

# Object files
OBJS = $(addsuffix .o,$(EXAMPLES))

# Print configuration (for debugging)
.PHONY: show-config
show-config:
	@echo "=== Build Configuration ==="
	@echo "PREFIX:                $(PREFIX)"
	@echo "VPI_PATH:              $(VPI_PATH)"
	@echo "FFMPEG_PKG_CONFIG_PATH: $(FFMPEG_PKG_CONFIG_PATH)"
	@echo "FFMPEG_INCLUDE_PATH:   $(FFMPEG_INCLUDE_PATH)"
	@echo "FFMPEG_LIB_PATH:       $(FFMPEG_LIB_PATH)"
	@echo "VPI_INCLUDE_PATH:      $(VPI_INCLUDE_PATH)"
	@echo "VPI_LIB_PATH:          $(VPI_LIB_PATH)"
	@echo "==========================="
	@echo "CFLAGS:  $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "LDLIBS:  $(LDLIBS)"
	@echo "==========================="

# Default target
.PHONY: all
all: $(EXAMPLES)

# Build rules
%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean targets
.PHONY: clean-test
clean-test:
	$(RM) test*.pgm test.h264 test.mp2 test.sw test.mpg *.log

.PHONY: clean
clean: clean-test
	$(RM) $(EXAMPLES) $(OBJS)

.PHONY: distclean
distclean: clean
	$(RM) *~ core

# Help target
.PHONY: help
help:
	@echo "FFmpeg AMA Examples Makefile"
	@echo ""
	@echo "Usage:"
	@echo "  make                    # Build with default system FFmpeg (/opt/amd/ama/ma35)"
	@echo "  make PREFIX=/path       # Build with custom FFmpeg installation"
	@echo "  make show-config        # Display current build configuration"
	@echo "  make clean              # Remove built files"
	@echo "  make help               # Show this help message"
	@echo ""
	@echo "Environment Variables:"
	@echo "  PREFIX       - FFmpeg installation directory (default: /opt/amd/ama/ma35)"
	@echo "  VPI_PATH     - VPI library installation directory (default: /opt/amd/ama/ma35)"
	@echo ""
	@echo "Examples:"
	@echo "  make PREFIX=/home/user/ffmpeg_build"
	@echo "  make PREFIX=/tmp/ffbuild2 VPI_PATH=/opt/amd/ama/ma35"
	@echo ""
	@echo "The Makefile will automatically:"
	@echo "  - Find pkg-config files in PREFIX/lib/pkgconfig or PREFIX/lib64/pkgconfig"
	@echo "  - Set include paths to PREFIX/include"
	@echo "  - Set library paths and RPATH for runtime linking"
	@echo "  - Link against VPI libraries from VPI_PATH"
