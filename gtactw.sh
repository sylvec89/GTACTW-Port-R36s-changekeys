#!/bin/bash
# GTA: Chinatown Wars launcher for R36S / ArkOS (PortMaster-compatible)

CURR_TTY="/dev/tty1"
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

# PortMaster integration
for pmdir in "/opt/system/Tools/PortMaster" "/opt/tools/PortMaster" \
             "$XDG_DATA_HOME/PortMaster" "/roms/ports/PortMaster"; do
  if [ -f "$pmdir/control.txt" ]; then
    source "$pmdir/control.txt"
    source "$pmdir/device_info.txt" 2>/dev/null || true
    get_controls 2>/dev/null || true
    break
  fi
done

# Fallbacks when running outside PortMaster (direct terminal / SSH)
ESUDO="${ESUDO:-}"
sdl_controllerconfig="${sdl_controllerconfig:-}"
pm_finish() { :; }

GAMEDIR="/roms/ports/gtactw"
cd "$GAMEDIR"

# Truncate the log (we'll append to it later, via tee).
> "$GAMEDIR/gtactw.log"

# ── TTY ownership & EmulationStation shutdown ──────────────────────────────
# We do this FIRST so install messages can render on the framebuffer console.
# We do NOT unbind vtconsole here — keeping it bound is what makes /dev/tty1
# writes show up on the screen. vtconsole gets unbound just before the game
# launches (KMS needs the framebuffer).
$ESUDO chmod 666 $CURR_TTY            2>/dev/null
$ESUDO chmod 666 /dev/uinput          2>/dev/null
$ESUDO chmod 666 /dev/dri/card0       2>/dev/null
$ESUDO chmod 666 /dev/dri/renderD128  2>/dev/null

$ESUDO pkill -TERM emulationstation 2>/dev/null || true
sleep 1
$ESUDO pkill -9    emulationstation 2>/dev/null || true

printf "\033c"        > $CURR_TTY    # clear
printf "\e[?25l"      > $CURR_TTY    # hide cursor

# ── First-run installer ────────────────────────────────────────────────────
# Sentinels: ROM.WAD (from the OBB) + GXT.obb.mp3 (from the APK assets).
# If either is missing, run install.
if [ ! -f "$GAMEDIR/ROM.WAD" ] || [ ! -f "$GAMEDIR/GXT.obb.mp3" ]; then
    # All install output goes to BOTH the log file (for diagnosis) AND
    # /dev/tty1 (so the user sees progress on screen).
    # Process substitution `>(...)` keeps us in the same shell, so `exit 1`
    # inside the block terminates the whole script.
    exec > >(tee -a "$GAMEDIR/gtactw.log" > $CURR_TTY) 2>&1

    OBB_FILE=$(ls "$GAMEDIR"/*.obb 2>/dev/null | head -1)
    APK_FILE=$(ls "$GAMEDIR"/*.apk 2>/dev/null | head -1)

    if [ -z "$OBB_FILE" ] || [ -z "$APK_FILE" ]; then
        echo ""
        echo "============================================================"
        echo "  GTA: Chinatown Wars — INSTALLATION INCOMPLETE"
        echo "============================================================"
        echo ""
        echo "First-run setup needs BOTH of these files in:"
        echo "  $GAMEDIR/"
        echo ""
        echo "  - main.4.com.rockstargames.gtactw.obb  (game data, ~865MB)"
        echo "  - gtacw.apk                            (text/font/legal assets)"
        echo ""
        echo "Status:"
        if [ -n "$OBB_FILE" ]; then
            echo "  [OK]      OBB found: $(basename "$OBB_FILE")"
        else
            echo "  [MISSING] No *.obb file found"
        fi
        if [ -n "$APK_FILE" ]; then
            echo "  [OK]      APK found: $(basename "$APK_FILE")"
        else
            echo "  [MISSING] No *.apk file found"
        fi
        echo ""
        echo "Copy the missing file(s) to the folder above and relaunch."
        echo "Returning to menu in 20 seconds..."
        sleep 20
        $ESUDO systemctl start emulationstation 2>/dev/null || true
        pm_finish
        exit 1
    fi

    echo ""
    echo "============================================================"
    echo "  First-run installation — extracting game data"
    echo "  This will take a few minutes. DO NOT POWER OFF."
    echo "============================================================"
    echo ""

    # Stream `unzip` output and print one "  [N/total] filename" line per file.
    extract_with_progress() {
        local archive="$1" dest="$2" filter="$3" extra_flags="$4"
        local total count=0 name
        total=$(unzip -Z1 "$archive" ${filter:+"$filter"} 2>/dev/null | wc -l)
        [ "$total" -eq 0 ] && total=1
        echo "  archive   : $(basename "$archive")"
        echo "  file count: $total"
        echo "  ----"
        unzip -o $extra_flags "$archive" ${filter:+"$filter"} -d "$dest" 2>&1 | \
        while IFS= read -r line; do
            case "$line" in
                # unzip uses one or two leading spaces depending on whether
                # the entry is stored or deflated — match both.
                *inflating:*|*extracting:*|*creating:*)
                    count=$((count + 1))
                    name="${line##*: }"
                    name="${name%%[[:space:]]*}"
                    printf "  [%3d/%3d] %s\n" "$count" "$total" "$(basename "$name")"
                    ;;
                error:*|*"cannot find"*|*"End-of-central-directory"*|unzip:*)
                    printf "  !! %s\n" "$line"
                    ;;
            esac
        done
        echo "  ----"
        return 0
    }

    echo "[1/2] Extracting OBB"
    echo ""
    extract_with_progress "$OBB_FILE" "$GAMEDIR/" "" ""
    if [ ! -f "$GAMEDIR/ROM.WAD" ]; then
        echo ""
        echo "============================================================"
        echo "  ERROR: OBB extraction did not produce ROM.WAD"
        echo "  The .obb file may be corrupt or the wrong version."
        echo "============================================================"
        sleep 15
        $ESUDO systemctl start emulationstation 2>/dev/null || true
        pm_finish
        exit 1
    fi
    # Success — reclaim the ~865MB the OBB was taking up
    echo "  removing $(basename "$OBB_FILE") ($(du -h "$OBB_FILE" | cut -f1))"
    rm -f "$OBB_FILE"

    echo ""
    echo "[2/2] Extracting APK assets"
    echo ""
    # -j flattens the 'assets/' prefix so files land directly in $GAMEDIR
    extract_with_progress "$APK_FILE" "$GAMEDIR/" 'assets/*' "-j"
    if [ ! -f "$GAMEDIR/GXT.obb.mp3" ]; then
        echo ""
        echo "============================================================"
        echo "  ERROR: APK extraction did not produce GXT.obb.mp3"
        echo "  The .apk may be the wrong version (need original Rockstar APK)."
        echo "============================================================"
        sleep 15
        $ESUDO systemctl start emulationstation 2>/dev/null || true
        pm_finish
        exit 1
    fi
    # Success — reclaim the APK space too
    echo "  removing $(basename "$APK_FILE") ($(du -h "$APK_FILE" | cut -f1))"
    rm -f "$APK_FILE"

    echo ""
    echo "============================================================"
    echo "  Installation complete. Launching game..."
    echo "============================================================"
    sleep 2
fi
# ───────────────────────────────────────────────────────────────────────────

# From here on (game run): log only, no more TTY output (game owns the FB).
exec > >(tee -a "$GAMEDIR/gtactw.log") 2>&1

# Release VT console framebuffer so KMS/DRM is free for the game
echo 0 | $ESUDO tee /sys/class/vtconsole/vtcon0/bind > /dev/null 2>&1 || true
echo 0 | $ESUDO tee /sys/class/vtconsole/vtcon1/bind > /dev/null 2>&1 || true

# Video
export SDL_VIDEODRIVER=kmsdrm
export SDL_VIDEO_GL_DRIVER=libGLESv2.so
export SDL_VIDEO_EGL_DRIVER=libEGL.so

# Audio — ALSA only on ArkOS (no PulseAudio/PipeWire)
export SDL_AUDIODRIVER=alsa
export AUDIODEV=default
export ALSOFT_DRIVERS=alsa

# Controller
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# Libraries
export LD_LIBRARY_PATH="/usr/lib/arm-linux-gnueabihf:$LD_LIBRARY_PATH"

# Preload our versioned __clock_gettime64 override before libc's broken vDSO path
export LD_PRELOAD="$GAMEDIR/libclock_fix.so"

./gtactw_r36

# Restore VT console and restart frontend
echo 1 | $ESUDO tee /sys/class/vtconsole/vtcon0/bind > /dev/null 2>&1 || true
echo 1 | $ESUDO tee /sys/class/vtconsole/vtcon1/bind > /dev/null 2>&1 || true
printf "\033c" > $CURR_TTY
printf "\e[?25h" > $CURR_TTY

$ESUDO systemctl start emulationstation 2>/dev/null || \
  $ESUDO systemctl restart oga_events 2>/dev/null || true

pm_finish
