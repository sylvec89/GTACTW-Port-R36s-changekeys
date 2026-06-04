/* openal_patch.c -- Replace the embedded Android AudioTrack OpenAL Soft
 *                   with the system OpenAL Soft (ALSA/PipeWire backend).
 *
 * The game has OpenAL Soft statically compiled in with only the Android
 * AudioTrack backend.  We hook every al / alc symbol to redirect to the
 * system libopenal.so which has proper Linux audio backends.
 *
 * We use dlsym rather than linking directly so we handle version differences
 * automatically: if a symbol exists in both the game .so and system OpenAL
 * it gets hooked; if it's missing from either side it's silently skipped.
 */

#include <dlfcn.h>
#include <stdio.h>

#include "so_util.h"
#include "openal_patch.h"

extern so_module gtactw_mod;

static void *libopenal = NULL;

/* Override alcCreateContext to force 44100 Hz */
typedef void *ALCdevice;
typedef void *ALCcontext;
typedef int   ALCint;
#define ALC_FREQUENCY 0x1007

static ALCcontext *(*real_alcCreateContext)(ALCdevice *, const ALCint *) = NULL;

static ALCcontext *alcCreateContextHook(ALCdevice *dev, const ALCint *unused) {
    (void)unused;
    const ALCint attr[] = { ALC_FREQUENCY, 44100, 0 };
    if (real_alcCreateContext)
        return real_alcCreateContext(dev, attr);
    return NULL;
}

/* Hook a symbol: look it up in both the game .so and system OpenAL, patch if found */
static void hook_al(const char *name) {
    uintptr_t game_sym = so_symbol(&gtactw_mod, name);
    if (!game_sym) return;

    void *sys_sym = dlsym(libopenal, name);
    if (!sys_sym) return;

    hook_addr(game_sym, (uintptr_t)sys_sym);
}

/* Full list of AL/ALC symbols the game may export */
static const char *al_symbols[] = {
    "alAuxiliaryEffectSlotf", "alAuxiliaryEffectSlotfv",
    "alAuxiliaryEffectSloti", "alAuxiliaryEffectSlotiv",
    "alBuffer3f", "alBuffer3i", "alBufferData",
    "alBufferSamplesSOFT", "alBufferSubDataSOFT", "alBufferSubSamplesSOFT",
    "alBufferf", "alBufferfv", "alBufferi", "alBufferiv",
    "alDeferUpdatesSOFT",
    "alDeleteAuxiliaryEffectSlots", "alDeleteBuffers",
    "alDeleteEffects", "alDeleteFilters", "alDeleteSources",
    "alDisable", "alDistanceModel", "alDopplerFactor", "alDopplerVelocity",
    "alEffectf", "alEffectfv", "alEffecti", "alEffectiv",
    "alEnable",
    "alFilterf", "alFilterfv", "alFilteri", "alFilteriv",
    "alGenAuxiliaryEffectSlots", "alGenBuffers",
    "alGenEffects", "alGenFilters", "alGenSources",
    "alGetAuxiliaryEffectSlotf", "alGetAuxiliaryEffectSlotfv",
    "alGetAuxiliaryEffectSloti", "alGetAuxiliaryEffectSlotiv",
    "alGetBoolean", "alGetBooleanv",
    "alGetBuffer3f", "alGetBuffer3i",
    "alGetBufferSamplesSOFT",
    "alGetBufferf", "alGetBufferfv", "alGetBufferi", "alGetBufferiv",
    "alGetDouble", "alGetDoublev",
    "alGetEffectf", "alGetEffectfv", "alGetEffecti", "alGetEffectiv",
    "alGetEnumValue", "alGetError",
    "alGetFilterf", "alGetFilterfv", "alGetFilteri", "alGetFilteriv",
    "alGetFloat", "alGetFloatv", "alGetInteger", "alGetIntegerv",
    "alGetListener3f", "alGetListener3i",
    "alGetListenerf", "alGetListenerfv", "alGetListeneri", "alGetListeneriv",
    "alGetProcAddress",
    "alGetSource3dSOFT", "alGetSource3f", "alGetSource3i",
    "alGetSource3i64SOFT", "alGetSourcedSOFT", "alGetSourcedvSOFT",
    "alGetSourcef", "alGetSourcefv", "alGetSourcei",
    "alGetSourcei64SOFT", "alGetSourcei64vSOFT", "alGetSourceiv",
    "alGetString",
    "alIsAuxiliaryEffectSlot", "alIsBuffer",
    "alIsBufferFormatSupportedSOFT",
    "alIsEffect", "alIsEnabled", "alIsExtensionPresent",
    "alIsFilter", "alIsSource",
    "alListener3f", "alListener3i",
    "alListenerf", "alListenerfv", "alListeneri", "alListeneriv",
    "alProcessUpdatesSOFT",
    "alSource3dSOFT", "alSource3f", "alSource3i",
    "alSource3i64SOFT",
    "alSourcePause", "alSourcePausev",
    "alSourcePlay", "alSourcePlayv",
    "alSourceQueueBuffers", "alSourceRewind", "alSourceRewindv",
    "alSourceStop", "alSourceStopv", "alSourceUnqueueBuffers",
    "alSourcedSOFT", "alSourcedvSOFT",
    "alSourcef", "alSourcefv",
    "alSourcei", "alSourcei64SOFT", "alSourcei64vSOFT", "alSourceiv",
    "alSpeedOfSound",
    "alcCaptureCloseDevice", "alcCaptureOpenDevice",
    "alcCaptureSamples", "alcCaptureStart", "alcCaptureStop",
    "alcCloseDevice",
    "alcDestroyContext",
    "alcGetContextsDevice", "alcGetCurrentContext",
    "alcGetEnumValue", "alcGetError",
    "alcGetIntegerv", "alcGetProcAddress", "alcGetString",
    "alcGetThreadContext",
    "alcIsExtensionPresent", "alcIsRenderFormatSupportedSOFT",
    "alcLoopbackOpenDeviceSOFT",
    "alcMakeContextCurrent", "alcOpenDevice",
    "alcProcessContext", "alcRenderSamplesSOFT",
    "alcSetThreadContext", "alcSuspendContext",
    NULL
};

static int ret0_al(void) { return 0; }

void patch_openal(void) {
    libopenal = dlopen("libopenal.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!libopenal) {
        libopenal = dlopen("libopenal.so", RTLD_NOW | RTLD_GLOBAL);
    }
    if (!libopenal) {
        fprintf(stderr, "patch_openal: could not open libopenal: %s\n", dlerror());
        fprintf(stderr, "Audio will be silent.\n");
        return;
    }

    /* Disable the embedded ALSOFT's Android AudioTrack backend so it doesn't
     * try to spawn a JNI thread.  These are internal ALSOFT functions. */
    uintptr_t sym;
    sym = so_symbol(&gtactw_mod, "alc_audiotrack_probe");
    if (sym) { fprintf(stderr, "patch_openal: stubbing alc_audiotrack_probe\n"); hook_addr(sym, (uintptr_t)ret0_al); }
    sym = so_symbol(&gtactw_mod, "alc_audiotrack_init");
    if (sym) { fprintf(stderr, "patch_openal: stubbing alc_audiotrack_init\n");  hook_addr(sym, (uintptr_t)ret0_al); }
    sym = so_symbol(&gtactw_mod, "alc_audiotrack_deinit");
    if (sym) { fprintf(stderr, "patch_openal: stubbing alc_audiotrack_deinit\n");hook_addr(sym, (uintptr_t)ret0_al); }

    /* Hook alcCreateContext specially to force 44100 Hz */
    real_alcCreateContext = dlsym(libopenal, "alcCreateContext");
    uintptr_t game_alcCreateContext = so_symbol(&gtactw_mod, "alcCreateContext");
    if (game_alcCreateContext && real_alcCreateContext)
        hook_addr(game_alcCreateContext, (uintptr_t)alcCreateContextHook);

    /* Hook all other AL/ALC symbols */
    for (int i = 0; al_symbols[i]; i++)
        hook_al(al_symbols[i]);

    fflush(stderr);
}
