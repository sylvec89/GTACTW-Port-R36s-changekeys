#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_W    640
#define SCREEN_H    480

/* Path to extracted game data on the device.
 * FileGetArchiveName(1) returns a filename relative to this + "/Android/obb/<pkg>/". */
#define DATA_PATH   "/roms/ports/gtactw"
#define SO_PATH     DATA_PATH "/libCTW.so"

/* OBB lives at DATA_PATH/<OBB_RELPATH> */
#define OBB_RELPATH "/main.4.com.rockstargames.gtactw.obb"

/* Device memory in MB (used for GetDeviceType JNI call) */
#define DEVICE_MEMORY_MB 256

/* Deadzone for analog sticks (0.0–1.0) */
#define STICK_DEADZONE 0.25f

/* #define DEBUG */

#endif /* CONFIG_H */
