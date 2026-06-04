/* jni_patch.c -- Fake Java Native Interface for GTA CTW on R36S
 *
 * Ported from TheOfficialFloW/gtactw_vita (MIT License)
 * Adapted for Linux/SDL2/R36S gamepad.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "config.h"
#include "so_util.h"
#include "jni_patch.h"

extern so_module gtactw_mod;
extern char g_data_path[512];

/* Gamepad state updated each frame by poll_input() */
extern SDL_GameController *g_gamepad;
extern int g_gamepad_buttons;
extern float g_gamepad_axis[6]; /* LX LY RX RY L2 R2 */

/* Touch event function pointer (resolved from libCTW.so) */
static int (*AND_TouchEvent)(int action, int report, int x, int y);

/* JNI method IDs */
enum MethodID {
    UNKNOWN = 0,
    INIT_EGL_AND_GLES2,
    SWAP_BUFFERS,
    MAKE_CURRENT,
    UN_MAKE_CURRENT,
    GET_APP_LOCAL_VALUE,
    FILE_GET_ARCHIVE_NAME,
    DELETE_FILE,
    GET_DEVICE_INFO,
    GET_DEVICE_TYPE,
    GET_DEVICE_LOCALE,
    GET_GAMEPAD_TYPE,
    GET_GAMEPAD_BUTTONS,
    GET_GAMEPAD_AXIS,
    GET_GAMEPAD_TRACK,
    GET_SUPPORT_PAUSE_RESUME,
    HAS_APP_LOCAL_VALUE,
    GET_SPECIAL_BUILD_TYPE,
    GET_TOTAL_MEMORY,
    GET_AVAILABLE_MEMORY,
    IS_NETWORK_AVAILABLE,
    IS_WIFI_AVAILABLE,
    IS_TV,
    GET_ANDROID_BUILDINFO,
    FINISH,
    PLAY_MOVIE,
    STOP_MOVIE,
    IS_MOVIE_PLAYING,
    SERVICE_APP_COMMAND,
    SERVICE_APP_COMMAND_INT,
    CONVERT_TO_BITMAP,
    GET_SCREEN_WIDTH_INCHES,
    SET_APP_LOCAL_VALUE,
};

static const struct { const char *name; int id; } method_map[] = {
    { "InitEGLAndGLES2",       INIT_EGL_AND_GLES2       },
    { "swapBuffers",            SWAP_BUFFERS              },
    { "makeCurrent",            MAKE_CURRENT              },
    { "unMakeCurrent",          UN_MAKE_CURRENT           },
    { "getAppLocalValue",       GET_APP_LOCAL_VALUE       },
    { "FileGetArchiveName",     FILE_GET_ARCHIVE_NAME     },
    { "DeleteFile",             DELETE_FILE               },
    { "GetDeviceInfo",          GET_DEVICE_INFO           },
    { "GetDeviceType",          GET_DEVICE_TYPE           },
    { "GetDeviceLocale",        GET_DEVICE_LOCALE         },
    { "GetGamepadType",         GET_GAMEPAD_TYPE          },
    { "GetGamepadButtons",      GET_GAMEPAD_BUTTONS       },
    { "GetGamepadAxis",         GET_GAMEPAD_AXIS          },
    { "GetGamepadTrack",        GET_GAMEPAD_TRACK         },
    { "getSupportPauseResume",  GET_SUPPORT_PAUSE_RESUME  },
    { "hasAppLocalValue",       HAS_APP_LOCAL_VALUE       },
    { "GetSpecialBuildType",    GET_SPECIAL_BUILD_TYPE    },
    { "GetTotalMemory",         GET_TOTAL_MEMORY          },
    { "GetAvailableMemory",     GET_AVAILABLE_MEMORY      },
    { "isNetworkAvailable",     IS_NETWORK_AVAILABLE      },
    { "isWiFiAvailable",        IS_WIFI_AVAILABLE         },
    { "isTV",                   IS_TV                     },
    { "GetAndroidBuildinfo",    GET_ANDROID_BUILDINFO     },
    { "finish",                 FINISH                    },
    { "PlayMovie",              PLAY_MOVIE                },
    { "StopMovie",              STOP_MOVIE                },
    { "IsMoviePlaying",         IS_MOVIE_PLAYING          },
    { "ServiceAppCommand",      SERVICE_APP_COMMAND       },
    { "ServiceAppCommandInt",   SERVICE_APP_COMMAND_INT   },
    { "ConvertToBitmap",        CONVERT_TO_BITMAP         },
    { "GetScreenWidthInches",   GET_SCREEN_WIDTH_INCHES   },
    { "setAppLocalValue",       SET_APP_LOCAL_VALUE       },
};

/* ── fake_vm / fake_env buffers ─────────────────────────────────────────── */
char fake_vm[0x1000];
char fake_env[0x1000];

static void *natives_ptr = NULL;   /* captured from RegisterNatives */
static int (*game_init_fn)(void *, int, int) = NULL; /* first native's fnPtr */

/* Stub for any JNI vtable slot that hasn't been implemented.
 * Returns 0 and logs which slot was hit (via LR → offset into table). */
static int jni_unimpl(void) {
    uintptr_t lr = (uintptr_t)__builtin_return_address(0);
    uintptr_t base = (uintptr_t)fake_env;
    if (lr >= base && lr < base + sizeof(fake_env))
        fprintf(stderr, "JNI: unimplemented env slot at offset 0x%x\n",
                (unsigned)(lr - base - 4));
    else
        fprintf(stderr, "JNI: unimplemented call (LR=0x%08x)\n", (unsigned)lr);
    return 0;
}

static void fill_table_with_stub(void *table, size_t size) {
    uintptr_t *p = (uintptr_t *)table;
    size_t n = size / sizeof(uintptr_t);
    for (size_t i = 0; i < n; i++)
        p[i] = (uintptr_t)jni_unimpl;
}

/* SDL window/context needed for GL operations */
extern SDL_Window   *g_window;
extern SDL_GLContext g_gl_ctx;

/* ── JNI implementations ────────────────────────────────────────────────── */

static int ret0(void) { return 0; }
static int ret1(void) { return 1; }

static int GetDeviceInfo(void) { return 0; }

static int GetDeviceType(void) {
    return (DEVICE_MEMORY_MB << 6) | (3 << 2) | 0x1;
}

static int GetDeviceLocale(void) {
    /* 0=EN 1=FR 2=DE 3=IT 4=ES 5=JP */
    const char *lang = getenv("LANG");
    if (!lang) return 0;
    if (!strncmp(lang, "fr", 2)) return 1;
    if (!strncmp(lang, "de", 2)) return 2;
    if (!strncmp(lang, "it", 2)) return 3;
    if (!strncmp(lang, "es", 2)) return 4;
    if (!strncmp(lang, "ja", 2)) return 5;
    return 0;
}

static int GetGamepadType(int port) {
    if (port != 0) return -1;
    return 8; /* PS3 controller */
}

static int GetGamepadButtons(int port) {
    if (port != 0) return 0;
    return g_gamepad_buttons;
}

static float GetGamepadAxis(int port, int axis) {
    if (port != 0 || axis < 0 || axis > 5) return 0.0f;
    float v = g_gamepad_axis[axis];
    if (axis < 4 && fabsf(v) < STICK_DEADZONE) return 0.0f;
    return v;
}

/* GetGamepadTrack(port, p1, p2) → int — camera/right-stick track delta.
 * Original game used virtual touchpad; we map right analog stick (axes 2/3).
 * Return format: (dx << 16) | (dy & 0xFFFF), each signed int16 in [-128, 127]. */
static int GetGamepadTrack(int port, int p1, int p2) {
    if (port != 0) return 0;
    float rx = g_gamepad_axis[2];
    float ry = g_gamepad_axis[3];
    if (fabsf(rx) < STICK_DEADZONE) rx = 0.0f;
    if (fabsf(ry) < STICK_DEADZONE) ry = 0.0f;
    int dx = (int)(rx * 128.0f);
    int dy = (int)(ry * 128.0f);
    return (dx << 16) | (dy & 0xFFFF);
}

static int GetSupportPauseResume(void) { return 1; }

static int HasAppLocalValue(char *key) {
    return (key && strcmp(key, "STORAGE_ROOT") == 0) ? 1 : 0;
}

static int GetSpecialBuildType(void) { return 0; }

static int GetTotalMemory(void) { return DEVICE_MEMORY_MB; }

static int GetAvailableMemory(void) { return DEVICE_MEMORY_MB / 2; }

static char *GetAndroidBuildinfo(int type) {
    (void)type;
    return "Linux;R36S;1.0";
}

static int InitEGLAndGLES2(void) {
    extern SDL_Window *g_window;
    extern SDL_GLContext g_gl_ctx;
    fprintf(stderr, "InitEGLAndGLES2: SDL_GL_MakeCurrent window=%p ctx=%p\n",
            (void*)g_window, (void*)g_gl_ctx);
    fflush(stderr);
    if (g_window && g_gl_ctx)
        SDL_GL_MakeCurrent(g_window, g_gl_ctx);
    return 1;
}

static int swapBuffers(void) {
    SDL_GL_SwapWindow(g_window);
    return 1;
}

static int makeCurrent(void) {
    extern SDL_Window *g_window;
    extern SDL_GLContext g_gl_ctx;
    if (g_window && g_gl_ctx)
        SDL_GL_MakeCurrent(g_window, g_gl_ctx);
    return 1;
}

static int unMakeCurrent(void) {
    SDL_GL_MakeCurrent(g_window, NULL);
    return 1;
}

static char *getAppLocalValue(char *key) {
    if (strcmp(key, "STORAGE_ROOT") == 0)
        return g_data_path;
    return NULL;
}

static char *FileGetArchiveName(int type) {
    switch (type) {
    case 0: return OBB_RELPATH;  /* this version uses 0-indexed OBBs */
    case 1: return OBB_RELPATH;  /* keep 1-indexed for safety */
    default: return NULL;
    }
}

static int DeleteFile(char *file) {
    char path[640];
    snprintf(path, sizeof(path), "%s/%s", g_data_path, file);
    return remove(path) == 0 ? 1 : 0;
}

/* ── JNI vtable callbacks ────────────────────────────────────────────────── */

static int CallBooleanMethodV(void *env, void *obj, int id, uintptr_t *args) {
    (void)env; (void)obj;
    switch (id) {
    case INIT_EGL_AND_GLES2:      return InitEGLAndGLES2();
    case SWAP_BUFFERS:             return swapBuffers();
    case MAKE_CURRENT:             return makeCurrent();
    case UN_MAKE_CURRENT:          return unMakeCurrent();
    case DELETE_FILE:              return DeleteFile((char *)args[0]);
    case GET_SUPPORT_PAUSE_RESUME: return GetSupportPauseResume();
    case HAS_APP_LOCAL_VALUE:      return HasAppLocalValue((char *)args[0]);
    case IS_NETWORK_AVAILABLE:     return 0;
    case IS_WIFI_AVAILABLE:        return 0;
    case IS_TV:                    return 0;
    case IS_MOVIE_PLAYING:         return 0;
    case SERVICE_APP_COMMAND:      return 1;  /* pretend success */
    case SERVICE_APP_COMMAND_INT:  return 1;  /* pretend success */
    case CONVERT_TO_BITMAP:        return 0;  /* no bitmap conversion */
    case SET_APP_LOCAL_VALUE:      return 1;
    default:                       return 0;
    }
}

/* pcs("aapcs"): game calls via soft-float vtable; float return must go in r0 */
__attribute__((pcs("aapcs")))
static float CallFloatMethodV(void *env, void *obj, int id, uintptr_t *args) {
    (void)env; (void)obj;
    switch (id) {
    case GET_GAMEPAD_AXIS:          return GetGamepadAxis((int)args[0], (int)args[1]);
    case GET_SCREEN_WIDTH_INCHES:   return 4.5f;  /* approx 4.5-inch screen */
    default:                        return 0.0f;
    }
}

static int CallIntMethodV(void *env, void *obj, int id, uintptr_t *args) {
    (void)env; (void)obj;
    switch (id) {
    case GET_GAMEPAD_TYPE:       return GetGamepadType((int)args[0]);
    case GET_GAMEPAD_BUTTONS:    return GetGamepadButtons((int)args[0]);
    case GET_GAMEPAD_TRACK:      return GetGamepadTrack((int)args[0], (int)args[1], (int)args[2]);
    case GET_DEVICE_INFO:        return GetDeviceInfo();
    case GET_DEVICE_TYPE:        return GetDeviceType();
    case GET_DEVICE_LOCALE:      return GetDeviceLocale();
    case GET_SPECIAL_BUILD_TYPE: return GetSpecialBuildType();
    case GET_TOTAL_MEMORY:       return GetTotalMemory();
    case GET_AVAILABLE_MEMORY:   return GetAvailableMemory();
    default:                     return 0;
    }
}

static void *CallObjectMethodV(void *env, void *obj, int id, uintptr_t *args) {
    (void)env; (void)obj;
    switch (id) {
    case GET_APP_LOCAL_VALUE:   return getAppLocalValue((char *)args[0]);
    case FILE_GET_ARCHIVE_NAME: return (void *)FileGetArchiveName((int)args[0]);
    case GET_ANDROID_BUILDINFO: return GetAndroidBuildinfo((int)args[0]);
    default:                    return NULL;
    }
}

static void CallVoidMethodV(void *env, void *obj, int id, uintptr_t *args) {
    (void)env; (void)obj; (void)id; (void)args;
}

static int GetMethodID(void *env, void *cls, const char *name, const char *sig) {
    (void)env; (void)cls;
    for (int i = 0; i < (int)(sizeof(method_map)/sizeof(method_map[0])); i++) {
        if (strcmp(name, method_map[i].name) == 0)
            return method_map[i].id;
    }
    return UNKNOWN;
}

static void RegisterNatives(void *env, int r1, void *r2) {
    (void)env; (void)r1;
    natives_ptr = r2;
    /* Extract the init entry point now while the JNINativeMethod array is still on JNI_OnLoad's stack */
    game_init_fn = *(void **)(r2 + 8);
}

static void *NewGlobalRef(void) { return (void *)0x42424242; }

static char *NewStringUTF(void *env, char *bytes) {
    (void)env;
    return bytes;
}

static char *GetStringUTFChars(void *env, char *str, int *isCopy) {
    (void)env;
    if (isCopy) *isCopy = 0;
    return str;
}

void *NVThreadGetCurrentJNIEnv(void) {
    return fake_env;
}

static int GetEnv(void *vm, void **env, int version) {
    (void)vm; (void)version;

    fill_table_with_stub(fake_env, sizeof(fake_env));
    *(uintptr_t *)(fake_env + 0x00)  = (uintptr_t)fake_env;
    *(uintptr_t *)(fake_env + 0x18)  = (uintptr_t)ret0;            /* FindClass */
    *(uintptr_t *)(fake_env + 0x44)  = (uintptr_t)ret0;            /* ExceptionClear */
    *(uintptr_t *)(fake_env + 0x54)  = (uintptr_t)NewGlobalRef;
    *(uintptr_t *)(fake_env + 0x5C)  = (uintptr_t)ret0;            /* DeleteLocalRef */
    *(uintptr_t *)(fake_env + 0x84)  = (uintptr_t)GetMethodID;
    *(uintptr_t *)(fake_env + 0x8C)  = (uintptr_t)CallObjectMethodV;
    *(uintptr_t *)(fake_env + 0x98)  = (uintptr_t)CallBooleanMethodV;
    *(uintptr_t *)(fake_env + 0xC8)  = (uintptr_t)CallIntMethodV;
    *(uintptr_t *)(fake_env + 0xE0)  = (uintptr_t)CallFloatMethodV;
    *(uintptr_t *)(fake_env + 0xF8)  = (uintptr_t)CallVoidMethodV;
    *(uintptr_t *)(fake_env + 0x178) = (uintptr_t)ret0;
    *(uintptr_t *)(fake_env + 0x1C4) = (uintptr_t)ret0;
    *(uintptr_t *)(fake_env + 0x1CC) = (uintptr_t)ret0;
    *(uintptr_t *)(fake_env + 0x240) = (uintptr_t)ret0;            /* keyboard */
    *(uintptr_t *)(fake_env + 0x29C) = (uintptr_t)NewStringUTF;
    *(uintptr_t *)(fake_env + 0x2A4) = (uintptr_t)GetStringUTFChars;
    *(uintptr_t *)(fake_env + 0x2A8) = (uintptr_t)ret0;            /* ReleaseStringUTFChars */
    *(uintptr_t *)(fake_env + 0x35C) = (uintptr_t)RegisterNatives;

    *env = fake_env;
    return 0;
}

void jni_init(void) {
    fill_table_with_stub(fake_vm, sizeof(fake_vm));
    *(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm;
    *(uintptr_t *)(fake_vm + 0x18) = (uintptr_t)GetEnv;
}

void jni_load(void) {
    /* Un-pause the game (it starts paused by default) */
    int *paused = (int *)so_symbol(&gtactw_mod, "IsAndroidPaused");
    if (paused) *paused = 0;

    /* JNI_OnLoad registers natives and stores them in natives_ptr */
    int (*JNI_OnLoad)(void *vm, void *reserved) =
        (void *)so_symbol(&gtactw_mod, "JNI_OnLoad");
    if (!JNI_OnLoad) {
        fprintf(stderr, "jni_load: JNI_OnLoad not found\n");
        return;
    }
    fprintf(stderr, "jni_load: calling JNI_OnLoad\n"); fflush(stderr);
    JNI_OnLoad(fake_vm, NULL);
    fprintf(stderr, "jni_load: JNI_OnLoad returned\n"); fflush(stderr);

    if (!natives_ptr) {
        fprintf(stderr, "jni_load: RegisterNatives was not called — no entry point\n");
        return;
    }
    fprintf(stderr, "jni_load: natives_ptr=%p init_fn=%p\n", natives_ptr, (void*)game_init_fn); fflush(stderr);

    /* Release the GL context from the main thread so the game's rendering
     * thread can make it current (eglMakeCurrent fails with EGL_BAD_ACCESS
     * if the context is still current on another thread). */
    SDL_GL_MakeCurrent(g_window, NULL);

    game_init_fn(fake_env, 0, 1);
    fprintf(stderr, "jni_load: game_init_fn returned — threads spawned, main waiting\n");
    fflush(stderr);
    /* NVEventAppInit returned — game loop runs in spawned threads. Keep
     * the main thread alive until game exits. */
    for (;;) pause();
}

/* ── Touch event sender (called from ProcessEvents) ──────────────────── */

void jni_resolve_touch(void) {
    AND_TouchEvent = (void *)so_symbol(&gtactw_mod, "_Z14AND_TouchEventiiii");
}

void send_touch_event(int action, int slot, int x, int y) {
    if (AND_TouchEvent)
        AND_TouchEvent(action, slot, x, y);
}
