# GTA: Chinatown Wars — R36S port

A native Linux port of the Android version of Grand Theft Auto Chinatown Wars, 
targeting the R36S handheld (Rockchip RK3326, Cortex-A35, Mali-400)
running dArkOS — and tested working on R36S clones with Mali-Bifrost G31.

The port loads the original Android `libCTW.so` directly inside a small Linux
host binary that emulates just enough of Android's runtime (JNI vtable,
bionic pthread ABI, Android logging) to make the game think it's still on
Android. Audio (OpenAL) and graphics (GLES 2.0) calls are bridged to the
system libraries with soft-float → hard-float ABI thunks.

---

## Status

Fully working:
- All rendering programs (world, lit geometry, animated sprites, HUD/minimap)
- Sound effects + radio / music streaming (mpg123 + OpenAL)
- Gamepad input
- Touchscreen (if the device supports it - Tested on a RG353V)
- Save games

Graphics quality enabled:
- 4× MSAA
- Trilinear mipmaps + anisotropic filtering (up to 16×)
- BGRA → RGBA texture swizzle for Mali-400 compatibility

---

## What you need to supply

These files come from the legitimate Android version of the game (Google Play
or your own sideload). They are NOT in this repo for copyright reasons.

- `main.4.com.rockstargames.gtactw.obb` — ~865 MB game data
- `gtacw.apk` — Android package V1.04! the assets folder inside it contains the
  text strings GXT.obb.mp3, legal screens, and font support files that
  the game needs

On first launch on-device, the launcher script extracts both archives and
deletes them to free the SD card space.

---

## Building from source

### 1. Cross-compile toolchain (one-time, on a Linux host)

```bash
sudo dpkg --add-architecture armhf
sudo apt-get update
sudo apt-get install -y \
    gcc-arm-linux-gnueabihf \
    libsdl2-dev:armhf \
    libopenal-dev:armhf \
    libgles2-mesa-dev:armhf \
    libegl1-mesa-dev:armhf \
    libmpg123-dev:armhf \
    zlib1g-dev:armhf
```

The Makefile expects headers/libs at `armhf-sysroot/root/usr/...`. Either
symlink it to your install root, or unpack the `.deb` files into that
directory. (Auto-bootstrap of the sysroot is on the to-do list.)

### 2. Build

```bash
make           # produces ./gtactw_r36 and ./libclock_fix.so
```

That's it. Both binaries are ARMv7-A hard-float ELFs targeting the device.

### 3. Extract `libCTW.so` from the APK

```bash
unzip -j gtacw.apk lib/armeabi-v7a/libCTW.so
```

You'll end up with `libCTW.so` next to your `gtactw_r36`.

---

## Installing on the device

1. Copy these four files into `/roms/ports/gtactw/` on the device:
   - `gtactw_r36`
   - `libclock_fix.so`
   - `libCTW.so`
   And  - `gtactw.sh` to `/roms/ports/` so emulationstation picks it up, 
   You can rename it if you like.

2. Copy the user-supplied files into the same directory:
   - `main.4.com.rockstargames.gtactw.obb`
   - `gtacw.apk`

3. Launch the port. On first run, `gtactw.sh` will:
   - Detect the OBB and APK
   - Extract both with on-screen per-file progress
   - Verify the extraction produced `ROM.WAD` and `GXT.obb.mp3`
   - Delete the source archives to reclaim ~838 MB
   - Launch the game

The log lives at `/roms/ports/gtactw/gtactw.log` for postmortem debugging.

---

## Hardware / OS target

- **CPU**: ARM Cortex-A35 (ARMv8) running in ARMv7-A 32-bit mode, NEON, hard-float
- **GPU**: Mali-400 MP2 (Utgard) on original R36S, or Mali-Bifrost G31 on clones — both work
- **Display**: 640×480
- **OS**: dArkOS (Linux 6.x, glibc 2.36+, SDL2, ALSA, OpenAL Soft, mpg123)
- **Audio**: ALSA only (no PulseAudio/PipeWire on the device)

---

## Acknowledgements

This port draws on prior reverse-engineering work from the GTA:CTW Vita port
@TheOfficialFloW/gtactw_vita for understanding the JNI sequence, NV thread spawn hook,
and OS_Thread layout.

The game itself is © Rockstar Games. This repository contains only the port
glue and Linux host code — no game assets are bundled.
