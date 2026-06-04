# Makefile -- GTA CTW R36S port
#
# Target: ARMv7 HF (32-bit ARM, matches the Android armeabi-v7a .so)
#         Cross-compile from x86-64 with arm-linux-gnueabihf-gcc.
#
# Quick start:
#   make setup    # install cross-dev packages (needs sudo)
#   make          # build gtactw_r36
#   make data     # prepare game data directory

CROSS   := arm-linux-gnueabihf-
CC      := $(CROSS)gcc
STRIP   := $(CROSS)strip

# Local armhf sysroot built from extracted .deb packages
SYSROOT := armhf-sysroot/root

CFLAGS  := -march=armv7-a -mfpu=neon -mfloat-abi=hard \
           -O1 -g -fno-omit-frame-pointer \
           -DDEBUG \
           -Wall -Wno-unused-function \
           -I src \
           -I $(SYSROOT)/usr/include \
           -I $(SYSROOT)/usr/include/arm-linux-gnueabihf

LDFLAGS := -L$(SYSROOT)/usr/lib/arm-linux-gnueabihf \
           -lSDL2 -lEGL -lGLESv2 -lopenal -lmpg123 -lz -ldl -lpthread -lm \
           -Wl,-rpath-link,$(SYSROOT)/usr/lib/arm-linux-gnueabihf \
           -Wl,--allow-shlib-undefined

SRCS    := src/main.c \
           src/clock_fix.c \
           src/so_util.c \
           src/jni_patch.c \
           src/openal_patch.c \
           src/opengl_patch.c \
           src/mpg123_patch.c

OBJS    := $(SRCS:.c=.o)
TARGET  := gtactw_r36
PRELOAD := libclock_fix.so

.PHONY: all clean setup data install check-syms

all: $(TARGET) $(PRELOAD)

$(PRELOAD): src/clock_preload.c src/clock_preload.map
	$(CC) -O1 -fPIC -shared -nostdlib \
	  -march=armv7-a -mfpu=neon -mfloat-abi=hard \
	  -Wl,-soname,$(PRELOAD) \
	  -Wl,--version-script=src/clock_preload.map \
	  -o $@ $<

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(PRELOAD)

# Install cross-dev packages for armhf (requires sudo)
setup:
	sudo dpkg --add-architecture armhf
	sudo apt-get update
	sudo apt-get install -y \
	    libsdl2-dev:armhf \
	    libopenal-dev:armhf \
	    libgles2-mesa-dev:armhf \
	    libegl1-mesa-dev:armhf \
	    libz-dev:armhf \
	    gcc-arm-linux-gnueabihf

# Prepare the game data directory
DATA_PATH ?= data

data: $(DATA_PATH)
$(DATA_PATH):
	mkdir -p $@
	cp extracted/lib/armeabi-v7a/libCTW.so $@/
	cp main.4.com.rockstargames.gtactw.obb $@/
	unzip -o gtacw.apk 'assets/*' -d $@/

# Copy binary + data to device (set DEVICE_IP or use SSH)
install: all data
	scp $(TARGET) $(DATA_PATH)/libCTW.so $(DATA_PATH)/main.4.com.rockstargames.gtactw.obb \
	    $(if $(DEVICE_IP),root@$(DEVICE_IP):/opt/gtactw/,$(DATA_PATH)/)

# Print all unresolved symbols from the .so to verify our symbol table
check-syms:
	@echo "=== Undefined symbols in libCTW.so ==="
	@arm-linux-gnueabihf-nm -D extracted/lib/armeabi-v7a/libCTW.so \
	    | awk '/ U /{ print $$3 }' | sed 's/@@.*//' | sort -u
