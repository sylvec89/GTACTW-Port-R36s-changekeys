# GitHub Actions / Ubuntu ARMHF build

CROSS   := arm-linux-gnueabihf-
CC      := $(CROSS)gcc
STRIP   := $(CROSS)strip

CFLAGS := \
    -march=armv7-a \
    -mfpu=neon \
    -mfloat-abi=hard \
    -O2 \
    -fPIC \
    -Wall \
    -I/usr/include/SDL2 \
    -D_REENTRANT \
    -Isrc

LDFLAGS := \
    -lSDL2 \
    -lEGL \
    -lGLESv2 \
    -lopenal \
    -lmpg123 \
    -lz \
    -ldl \
    -lpthread \
    -lm

SRCS := \
    src/main.c \
    src/clock_fix.c \
    src/so_util.c \
    src/jni_patch.c \
    src/openal_patch.c \
    src/opengl_patch.c \
    src/mpg123_patch.c

OBJS := $(SRCS:.c=.o)

TARGET := gtactw_r36
PRELOAD := libclock_fix.so

all: $(TARGET) $(PRELOAD)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(PRELOAD): src/clock_preload.c
	$(CC) -shared -fPIC -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(PRELOAD)
