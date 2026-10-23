/* CoreAudio HAL additions for FMOD (Roblox audio & microphone input).
 *
 * Darling's PulseAudio HAL exposes an output device (object 2, also 4 as the
 * system output) and an input device (object 3), but only a few properties.
 * FMOD's CoreAudio output/input asks the device for its streams ('stm#') first
 * and gave up on "unknown property", so Roblox fell back to "NoSound Driver"
 * and could not see or record from any microphone.
 *
 * This answers the missing device and stream properties with the format
 * Darling's stream really uses (44.1 kHz, stereo, 32-bit float, interleaved),
 * provides an AUHAL output/input unit (bus 0 output, bus 1 microphone input),
 * and supports both host FIFO (MACOBLOX_AUDIO_FIFO / MACOBLOX_MIC_FIFO) and
 * ring-buffered IOProc paths. MACOBLOX_TRACE_AUDIO=1 logs the calls.
 *
 * Imports are weak: RobloxCrashHandler loads this library but not CoreAudio. */

typedef int OSStatus;
typedef unsigned int UInt32;
typedef double Float64;
typedef struct { UInt32 selector, scope, element; } PropertyAddress;
typedef struct { UInt32 type, subtype, manufacturer, flags, mask; } ComponentDescription;
typedef struct {
    Float64 sample_rate;
    UInt32 format_id, format_flags, bytes_per_packet, frames_per_packet, bytes_per_frame,
        channels_per_frame, bits_per_channel, reserved;
} StreamDescription;
typedef struct { Float64 minimum, maximum; } ValueRange;
typedef struct { StreamDescription format; ValueRange rates; } RangedDescription;
typedef struct { UInt32 channels, byte_size; void *data; } Buffer;
typedef struct { UInt32 count; Buffer buffers[1]; } BufferList;
typedef void (*ListenerProc)(void);

extern char *getenv(const char *);
extern int snprintf(char *, unsigned long, const char *, ...);
extern long write(int, const void *, unsigned long);
extern long read(int, void *, unsigned long);

#define W __attribute__((weak_import)) extern
W OSStatus AudioObjectGetPropertyData(UInt32, const PropertyAddress *, UInt32, const void *, UInt32 *, void *);
W OSStatus AudioObjectGetPropertyDataSize(UInt32, const PropertyAddress *, UInt32, const void *, UInt32 *);
W unsigned char AudioObjectHasProperty(UInt32, const PropertyAddress *);
W OSStatus AudioObjectSetPropertyData(UInt32, const PropertyAddress *, UInt32, const void *, UInt32, const void *);
W OSStatus AudioObjectIsPropertySettable(UInt32, const PropertyAddress *, unsigned char *);
W OSStatus AudioObjectAddPropertyListener(UInt32, const PropertyAddress *, ListenerProc, void *);
W OSStatus AudioObjectRemovePropertyListener(UInt32, const PropertyAddress *, ListenerProc, void *);
W void *AudioComponentFindNext(void *, const ComponentDescription *);
W OSStatus AudioComponentInstanceNew(void *, void **);
W OSStatus AudioUnitSetProperty(void *, UInt32, UInt32, UInt32, const void *, UInt32);
W OSStatus AudioUnitGetProperty(void *, UInt32, UInt32, UInt32, void *, UInt32 *);
W OSStatus AudioUnitInitialize(void *);
W OSStatus AudioOutputUnitStart(void *);
W OSStatus AudioUnitRender(void *, UInt32 *, const void *, UInt32, UInt32, BufferList *);
W OSStatus AudioDeviceCreateIOProcID(UInt32, void *, void *, void **);
W OSStatus AudioDeviceStart(UInt32, void *);
W OSStatus AudioDeviceStop(UInt32, void *);
W OSStatus AudioDeviceDestroyIOProcID(UInt32, void *);
W OSStatus AudioComponentInstanceDispose(void *);
W OSStatus AudioUnitUninitialize(void *);
W OSStatus AudioOutputUnitStop(void *);
extern void *calloc(unsigned long, unsigned long);
extern void *malloc(unsigned long);
extern void free(void *);
extern unsigned long long mach_absolute_time(void);

#define DYLD_INTERPOSE(_replacement, _replacee) \
    __attribute__((used)) static struct { const void *replacement; const void *replacee; } \
    _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = \
        {(const void *)(unsigned long)&_replacement, (const void *)(unsigned long)&_replacee};

#define FOURCC(a, b, c, d) ((UInt32)(a) << 24 | (UInt32)(b) << 16 | (UInt32)(c) << 8 | (UInt32)(d))
#define SCOPE_GLOBAL FOURCC('g', 'l', 'o', 'b')
#define SCOPE_OUTPUT FOURCC('o', 'u', 't', 'p')
#define SCOPE_INPUT FOURCC('i', 'n', 'p', 't')
#define OUTPUT_STREAM 0x4001u
#define INPUT_STREAM 0x4002u
#define NO_ERROR 0
#define BAD_SIZE FOURCC('!', 's', 'i', 'z')
#define UNSUPPORTED_FORMAT FOURCC('!', 'd', 'a', 't')
#define NOT_HANDLED 0x7fffffff

/* ---------------------------------------------------------------- tracing */

extern int open(const char *, int, ...);
extern int close(int);

static int audio_log_fd = -2;

static int tracing(void) {
    static int value = -1;
    if (value < 0) {
        const char *log_env = getenv("MACOBLOX_AUDIO_LOG");
        const char *trace_env = getenv("MACOBLOX_TRACE_AUDIO");
        const char *active = (log_env && log_env[0] && log_env[0] != '0')
                                 ? log_env
                                 : ((trace_env && trace_env[0] && trace_env[0] != '0') ? trace_env : 0);
        value = active ? 1 : 0;
        if (log_env && (log_env[0] == '/' || log_env[0] == '.')) {
            /* Darwin: O_WRONLY (0x0001) | O_APPEND (0x0008) | O_CREAT (0x0200) = 0x0209 */
            audio_log_fd = open(log_env, 0x0209, 0644);
        }
    }
    return value;
}

static void audio_log_write(const char *line, int length) {
    if (length <= 0)
        return;
    write(2, line, (unsigned long)length);
    if (audio_log_fd >= 0)
        write(audio_log_fd, line, (unsigned long)length);
}

static void code(char out[8], UInt32 value) {
    for (int i = 0; i < 4; i++) {
        char c = (char)(value >> (24 - 8 * i));
        out[i] = c >= 32 && c < 127 ? c : '?';
    }
    out[4] = 0;
}

static void report(const char *function, UInt32 object, const PropertyAddress *address,
                   long status, UInt32 size, int ours) {
    if (!tracing())
        return;
    char selector[8] = "-", scope[8] = "-", line[200];
    if (address) {
        code(selector, address->selector);
        code(scope, address->scope);
    }
    int length = snprintf(line, sizeof line, "[MacOBlox Audio] %s object=%u '%s' scope='%s' -> %ld size=%u%s\n",
                          function, object, selector, scope, status, size, ours ? " (shim)" : "");
    audio_log_write(line, length);
}

/* ------------------------------------------------------ added properties */

static int is_output_device(UInt32 object) { return object == 2 || object == 4; }
static int is_input_device(UInt32 object) { return object == 3; }
static int is_device(UInt32 object) { return is_output_device(object) || is_input_device(object); }
static int is_stream(UInt32 object) { return object == OUTPUT_STREAM || object == INPUT_STREAM; }

static StreamDescription current_format = {
    44100.0, FOURCC('l', 'p', 'c', 'm'), 1 /* float */ | 8 /* packed */, 8, 1, 8, 2, 32, 0};

/* Does the device side of `scope` have a stream on this device? */
static int device_has_stream(UInt32 object, UInt32 scope) {
    if (scope == SCOPE_GLOBAL)
        return 1;
    return (is_output_device(object) && scope == SCOPE_OUTPUT) ||
           (is_input_device(object) && scope == SCOPE_INPUT);
}

static OSStatus put(const void *value, UInt32 size, UInt32 *io_size, void *out) {
    if (out) {
        if (!io_size || *io_size < size)
            return BAD_SIZE;
        const unsigned char *from = value;
        unsigned char *to = out;
        for (UInt32 i = 0; i < size; i++)
            to[i] = from[i];
    }
    if (io_size)
        *io_size = size;
    return NO_ERROR;
}

static OSStatus put_u32(UInt32 value, UInt32 *io_size, void *out) {
    return put(&value, sizeof value, io_size, out);
}

/* MACOBLOX_AUDIO=0 turns the additions off (FMOD then uses "NoSound"). */
static int additions_enabled(void) {
    static int value = -1;
    if (value < 0) {
        const char *text = getenv("MACOBLOX_AUDIO");
        value = text && text[0] == '0' ? 0 : 1;
    }
    return value;
}

/* Returns NOT_HANDLED when Darling should answer. out == 0 means size only. */
static OSStatus added_property(UInt32 object, const PropertyAddress *address, UInt32 *io_size, void *out) {
    if (!additions_enabled())
        return NOT_HANDLED;
    UInt32 selector = address->selector, scope = address->scope;
    /* Fix 1: Answer kAudioHardwarePropertyDevices ('dev#') and default input
     * device ('dIn ') on kAudioObjectSystemObject (object 1) so FMOD's
     * getRecordNumDrivers() enumerates the microphone (object 3) alongside
     * the output device (object 2). */
    if (object == 1) {
        if (selector == FOURCC('d', 'e', 'v', '#')) {
            UInt32 devices[2] = {2, 3};
            return put(devices, sizeof devices, io_size, out);
        }
        if (selector == FOURCC('d', 'I', 'n', ' ')) {
            return put_u32(3, io_size, out);
        }
        if (selector == FOURCC('d', 'O', 'u', 't') || selector == FOURCC('s', 'O', 'u', 't')) {
            return put_u32(2, io_size, out);
        }
    }
    if (is_device(object)) {
        switch (selector) {
        case FOURCC('s', 't', 'm', '#'): { /* streams */
            UInt32 stream = is_output_device(object) ? OUTPUT_STREAM : INPUT_STREAM;
            if (!device_has_stream(object, scope))
                return put(&stream, 0, io_size, out);
            return put_u32(stream, io_size, out);
        }
        case FOURCC('s', 'l', 'a', 'y'): { /* stream configuration */
            BufferList list = {0, {{0, 0, 0}}};
            if (!device_has_stream(object, scope) || scope == SCOPE_GLOBAL)
                return put(&list, 8, io_size, out);
            list.count = 1;
            list.buffers[0].channels = current_format.channels_per_frame;
            return put(&list, sizeof list, io_size, out);
        }
        case FOURCC('s', 'f', 'm', 't'): /* legacy device stream format */
        case FOURCC('p', 'f', 't', ' '):
            if (!device_has_stream(object, scope))
                return NOT_HANDLED;
            return put(&current_format, sizeof current_format, io_size, out);
        case FOURCC('n', 'c', 'h', 'n'): /* legacy channel count */
            if (!device_has_stream(object, scope))
                return put_u32(0, io_size, out);
            return put_u32(current_format.channels_per_frame, io_size, out);
        case FOURCC('n', 's', 'r', 't'): { /* nominal sample rate */
            Float64 rate = current_format.sample_rate;
            return put(&rate, sizeof rate, io_size, out);
        }
        case FOURCC('n', 's', 'r', '#'): { /* available nominal sample rates */
            ValueRange range = {current_format.sample_rate, current_format.sample_rate};
            return put(&range, sizeof range, io_size, out);
        }
        case FOURCC('l', 'i', 'v', 'n'): /* device is alive */
            return put_u32(1, io_size, out);
        case FOURCC('f', 's', 'i', 'z'): { /* buffer frame size, from Darling's byte size */
            UInt32 bytes = 0, size = sizeof bytes;
            PropertyAddress legacy = {FOURCC('b', 's', 'i', 'z'), scope, 0};
            if (AudioObjectGetPropertyData(object, &legacy, 0, 0, &size, &bytes) != NO_ERROR || !bytes)
                bytes = 512 * current_format.bytes_per_frame;
            return put_u32(bytes / current_format.bytes_per_frame, io_size, out);
        }
        case FOURCC('f', 's', 'z', '#'): { /* buffer frame size range */
            ValueRange range = {64, 4096};
            return put(&range, sizeof range, io_size, out);
        }
        case FOURCC('l', 't', 'n', 'c'): /* latency */
        case FOURCC('s', 'a', 'f', 't'): /* safety offset */
            return put_u32(0, io_size, out);
        case FOURCC('t', 'r', 'a', 'n'): /* transport type: built-in */
            return put_u32(FOURCC('b', 'l', 't', 'n'), io_size, out);
        case FOURCC('c', 'l', 'c', 'k'): /* clock domain */
            return put_u32(0, io_size, out);
        }
        return NOT_HANDLED;
    }
    if (is_stream(object)) {
        switch (selector) {
        case FOURCC('s', 'f', 'm', 't'): /* virtual format */
        case FOURCC('p', 'f', 't', ' '): /* physical format */
            return put(&current_format, sizeof current_format, io_size, out);
        case FOURCC('s', 'f', 'm', 'a'): /* available virtual formats */
        case FOURCC('p', 'f', 't', 'a'): { /* available physical formats */
            RangedDescription ranged = {current_format, {current_format.sample_rate, current_format.sample_rate}};
            return put(&ranged, sizeof ranged, io_size, out);
        }
        case FOURCC('s', 'd', 'i', 'r'): /* direction: 0 output, 1 input */
            return put_u32(object == INPUT_STREAM, io_size, out);
        case FOURCC('s', 'c', 'h', 'n'): /* starting channel */
        case FOURCC('s', 'a', 'c', 't'): /* is active */
            return put_u32(1, io_size, out);
        case FOURCC('t', 'e', 'r', 'm'): /* terminal type */
        case FOURCC('l', 't', 'n', 'c'): /* latency */
            return put_u32(0, io_size, out);
        }
        return NOT_HANDLED;
    }
    return NOT_HANDLED;
}

/* ----------------------------------------------------------- interposers */

static OSStatus h_get(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                      UInt32 *size, void *data) {
    if (address) {
        UInt32 capacity = size ? *size : 0;
        OSStatus status = added_property(object, address, &capacity, data);
        if (status != NOT_HANDLED) {
            if (size) *size = capacity;
            report("GetPropertyData", object, address, status, capacity, 1);
            return status;
        }
    }
    OSStatus status = AudioObjectGetPropertyData(object, address, qsize, qdata, size, data);
    report("GetPropertyData", object, address, status, size ? *size : 0, 0);
    return status;
}
DYLD_INTERPOSE(h_get, AudioObjectGetPropertyData)

static OSStatus h_size(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                       UInt32 *size) {
    if (address) {
        UInt32 capacity = 0;
        OSStatus status = added_property(object, address, &capacity, 0);
        if (status != NOT_HANDLED) {
            if (size) *size = capacity;
            report("GetPropertyDataSize", object, address, status, capacity, 1);
            return status;
        }
    }
    OSStatus status = AudioObjectGetPropertyDataSize(object, address, qsize, qdata, size);
    report("GetPropertyDataSize", object, address, status, size ? *size : 0, 0);
    return status;
}
DYLD_INTERPOSE(h_size, AudioObjectGetPropertyDataSize)

static unsigned char h_has(UInt32 object, const PropertyAddress *address) {
    UInt32 capacity = 0;
    if (address && added_property(object, address, &capacity, 0) != NOT_HANDLED) {
        report("HasProperty", object, address, 1, 0, 1);
        return 1;
    }
    unsigned char has = AudioObjectHasProperty(object, address);
    report("HasProperty", object, address, has, 0, 0);
    return has;
}
DYLD_INTERPOSE(h_has, AudioObjectHasProperty)

static OSStatus h_set(UInt32 object, const PropertyAddress *address, UInt32 qsize, const void *qdata,
                      UInt32 size, const void *data) {
    if (additions_enabled() && address && (is_stream(object) || is_device(object)) &&
        (address->selector == FOURCC('s', 'f', 'm', 't') || address->selector == FOURCC('p', 'f', 't', ' '))) {
        /* Only the format Darling's PulseAudio stream uses is accepted. */
        const StreamDescription *format = data;
        OSStatus status = size == sizeof *format && format->format_id == current_format.format_id &&
                                  format->sample_rate == current_format.sample_rate &&
                                  format->channels_per_frame == current_format.channels_per_frame &&
                                  format->bits_per_channel == 32 && (format->format_flags & 1)
                              ? NO_ERROR
                              : UNSUPPORTED_FORMAT;
        report("SetPropertyData", object, address, status, size, 1);
        return status;
    }
    if (additions_enabled() && address && is_device(object) &&
        address->selector == FOURCC('f', 's', 'i', 'z') && size == 4) {
        UInt32 bytes = *(const UInt32 *)data * current_format.bytes_per_frame;
        PropertyAddress legacy = {FOURCC('b', 's', 'i', 'z'), address->scope, 0};
        OSStatus status = AudioObjectSetPropertyData(object, &legacy, 0, 0, sizeof bytes, &bytes);
        report("SetPropertyData", object, address, status, size, 1);
        return status;
    }
    OSStatus status = AudioObjectSetPropertyData(object, address, qsize, qdata, size, data);
    report("SetPropertyData", object, address, status, size, 0);
    return status;
}
DYLD_INTERPOSE(h_set, AudioObjectSetPropertyData)

static OSStatus h_settable(UInt32 object, const PropertyAddress *address, unsigned char *settable) {
    if (additions_enabled() && address && (((is_stream(object) || is_device(object)) &&
                                           (address->selector == FOURCC('s', 'f', 'm', 't') ||
                                            address->selector == FOURCC('p', 'f', 't', ' '))) ||
                    (is_device(object) && address->selector == FOURCC('f', 's', 'i', 'z')))) {
        if (settable) *settable = 1;
        return NO_ERROR;
    }
    UInt32 capacity = 0;
    if (address && added_property(object, address, &capacity, 0) != NOT_HANDLED) {
        if (settable) *settable = 0;
        return NO_ERROR;
    }
    return AudioObjectIsPropertySettable(object, address, settable);
}
DYLD_INTERPOSE(h_settable, AudioObjectIsPropertySettable)

/* Darling's AudioObjectAddPropertyListener always fails ("unknown
 * property"), and FMOD treats a failed listener registration (default device
 * changes and so on) as a failed init. Darling's devices never change, so
 * listeners are accepted and simply never called. */
static OSStatus h_add_listener(UInt32 object, const PropertyAddress *address, ListenerProc proc, void *data) {
    if (additions_enabled()) {
        report("AddPropertyListener", object, address, NO_ERROR, 0, 1);
        return NO_ERROR;
    }
    return AudioObjectAddPropertyListener(object, address, proc, data);
}
DYLD_INTERPOSE(h_add_listener, AudioObjectAddPropertyListener)

static OSStatus h_remove_listener(UInt32 object, const PropertyAddress *address, ListenerProc proc, void *data) {
    if (additions_enabled())
        return NO_ERROR;
    return AudioObjectRemovePropertyListener(object, address, proc, data);
}
DYLD_INTERPOSE(h_remove_listener, AudioObjectRemovePropertyListener)

/* ------------------------------------------------ output & input AUHAL unit */

/* Darling's AUHAL only accepts the canonical non-interleaved AudioUnit format
 * (the AUBase check), while FMOD asks for interleaved float and then gave up
 * (kAudioUnitErr_FormatNotSupported). Instead of it, FMOD gets this small
 * HAL output/input unit:
 *  - Bus 0 (output): accepts FMOD's format and render callback (property 23),
 *    renders on an 8 MB stack thread into MACOBLOX_AUDIO_FIFO or a ring buffer
 *    fed to Darling's output IOProc.
 *  - Bus 1 (microphone input): accepts EnableIO on scope=1 (Input), Element=1,
 *    input notify callback (property 2005), and client format on scope=2
 *    (Output), Element=1. Reads captured PCM on an 8 MB stack thread from
 *    MACOBLOX_MIC_FIFO (or Darling's input IOProc on device 3) into an input
 *    ring buffer, invokes the input callback on that safe thread, and serves
 *    samples from AudioUnitRender(). */

typedef OSStatus (*RenderProc)(void *, UInt32 *, const void *, UInt32, UInt32, BufferList *);
typedef struct {
    Float64 sample_time;
    unsigned long long host_time;
    Float64 rate_scalar;
    unsigned long long word_clock;
    unsigned char smpte[24];
    UInt32 flags, reserved;
} TimeStamp;

#define UNIT_MAGIC 0x4d4f4241u /* 'MOBA' */
typedef struct {
    UInt32 magic;
    UInt32 device;
    int output_enabled;
    int input_enabled;
    int running;
    int initialized;
    StreamDescription format;
    StreamDescription input_format;
    RenderProc render;
    void *render_context;
    RenderProc input_callback;
    void *input_context;
    void *ioproc;
    UInt32 max_frames;
    Float64 sample_time;
    Float64 input_sample_time;
    float *scratch;
    UInt32 scratch_frames;
    /* Output ring (FMOD -> host/Darling) */
    unsigned char *ring;
    unsigned long ring_bytes;
    volatile unsigned long ring_written, ring_read;
    /* Input ring (host/Darling mic -> FMOD AudioUnitRender) */
    unsigned char *in_ring;
    unsigned long in_ring_bytes;
    volatile unsigned long in_ring_written, in_ring_read;
    volatile int producing;
    void *thread;
    void *in_thread;
} OutputUnit;

#define INVALID_PROPERTY (-10879)
#define PROPERTY_NOT_WRITABLE (-10865)

static void *hal_component;

static OutputUnit *as_unit(void *instance) {
    OutputUnit *unit = instance;
    return unit && unit->magic == UNIT_MAGIC ? unit : 0;
}

#define RENDER_FRAMES 512
#define RING_FRAMES 8192
extern int pthread_create(void **, const void *, void *(*)(void *), void *);
extern int pthread_join(void *, void **);
extern int pthread_attr_init(void *);
extern int pthread_attr_setstacksize(void *, unsigned long);
extern int usleep(unsigned int);

static UInt32 stream_frame_bytes(const StreamDescription *fmt) {
    UInt32 channels = fmt->channels_per_frame ? fmt->channels_per_frame : 2;
    UInt32 sample_bytes = fmt->bits_per_channel / 8 ? fmt->bits_per_channel / 8 : 4;
    return channels * sample_bytes;
}

static UInt32 unit_frame_bytes(OutputUnit *unit) {
    return stream_frame_bytes(&unit->format);
}

static UInt32 unit_input_frame_bytes(OutputUnit *unit) {
    return stream_frame_bytes(&unit->input_format);
}

/* Render `frames` frames into `out` as interleaved samples. */
static void unit_render(OutputUnit *unit, unsigned char *out, UInt32 frames) {
    UInt32 channels = unit->format.channels_per_frame ? unit->format.channels_per_frame : 2;
    UInt32 sample_bytes = unit->format.bits_per_channel / 8 ? unit->format.bits_per_channel / 8 : 4;
    UInt32 frame_bytes = channels * sample_bytes;
    int planar = (unit->format.format_flags & 0x20) != 0; /* kAudioFormatFlagIsNonInterleaved */
    TimeStamp stamp = {unit->sample_time, mach_absolute_time(), 1.0, 0, {0}, 3 /* sample+host time */, 0};
    UInt32 flags = 0;
    OSStatus status = -1;
    if (unit->render && !planar) {
        BufferList list = {1, {{channels, frames * frame_bytes, out}}};
        status = unit->render(unit->render_context, &flags, &stamp, 0, frames, &list);
    } else if (unit->render && unit->scratch) {
        struct { UInt32 count; Buffer buffers[8]; } list = {0};
        list.count = channels > 8 ? 8 : channels;
        for (UInt32 c = 0; c < list.count; c++) {
            list.buffers[c].channels = 1;
            list.buffers[c].byte_size = frames * sample_bytes;
            list.buffers[c].data = (unsigned char *)unit->scratch + (unsigned long)c * frames * sample_bytes;
        }
        status = unit->render(unit->render_context, &flags, &stamp, 0, frames, (BufferList *)&list);
        for (UInt32 f = 0; f < frames; f++)
            for (UInt32 c = 0; c < list.count; c++)
                for (UInt32 b = 0; b < sample_bytes; b++)
                    out[(f * channels + c) * sample_bytes + b] =
                        ((unsigned char *)list.buffers[c].data)[f * sample_bytes + b];
    }
    if (status != NO_ERROR)
        for (UInt32 i = 0; i < frames * frame_bytes; i++) out[i] = 0;
    unit->sample_time += frames;
}

static void *unit_render_thread(void *context) {
    OutputUnit *unit = context;
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = malloc(chunk);
    if (!block)
        return 0;
    while (unit->producing) {
        unsigned long used = unit->ring_written - unit->ring_read;
        if (unit->ring_bytes - used < chunk) {
            usleep(2000);
            continue;
        }
        unit_render(unit, block, RENDER_FRAMES);
        unsigned long position = unit->ring_written % unit->ring_bytes;
        for (unsigned long i = 0; i < chunk; i++)
            unit->ring[(position + i) % unit->ring_bytes] = block[i];
        __sync_synchronize();
        unit->ring_written += chunk;
    }
    free(block);
    return 0;
}

extern int *__error(void);
extern int pthread_sigmask(int, const unsigned int *, unsigned int *);
#define FIFO_AHEAD_FRAMES 2600

static void *unit_fifo_thread(void *context) {
    OutputUnit *unit = context;
    const char *path = getenv("MACOBLOX_AUDIO_FIFO");
    /* A reader that went away must give EPIPE here, not kill the game. */
    unsigned int block_pipe = 1u << (13 - 1); /* SIGPIPE */
    pthread_sigmask(1 /* SIG_BLOCK */, &block_pipe, 0);
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = malloc(chunk);
    int fd = -1;
    unsigned long long start = mach_absolute_time();
    unsigned long long frames_written = 0;
    Float64 rate = unit->format.sample_rate > 0 ? unit->format.sample_rate : 44100.0;
    while (block && unit->producing) {
        if (fd < 0) {
            fd = open(path, 1 /* O_WRONLY */);
            if (fd < 0) {
                usleep(200000);
                continue;
            }
            start = mach_absolute_time();
            frames_written = 0;
        }
        double elapsed = (double)(mach_absolute_time() - start) / 1e9;
        double ahead = (double)frames_written - elapsed * rate;
        if (ahead > FIFO_AHEAD_FRAMES) {
            usleep(3000);
            continue;
        }
        if (ahead < -rate / 4) { /* fell far behind (stall): restart the clock */
            start = mach_absolute_time();
            frames_written = 0;
        }
        unit_render(unit, block, RENDER_FRAMES);
        unsigned long done = 0;
        while (done < chunk) {
            long written = write(fd, block + done, chunk - done);
            if (written <= 0) {
                if (written < 0 && *__error() == 4 /* EINTR */)
                    continue;
                close(fd);
                fd = -1;
                break;
            }
            done += (unsigned long)written;
        }
        frames_written += RENDER_FRAMES;
    }
    if (fd >= 0)
        close(fd);
    free(block);
    return 0;
}

/* Microphone capture thread: reads captured PCM from MACOBLOX_MIC_FIFO (or
 * waits for Darling's input IOProc to push into in_ring) on an 8 MB stack
 * thread and notifies FMOD's input_callback so FMOD pulls samples via
 * AudioUnitRender() without ever running on Darling's tiny GCD thread. */
static void *unit_mic_thread(void *context) {
    OutputUnit *unit = context;
    const char *fifo_path = getenv("MACOBLOX_MIC_FIFO");
    UInt32 frame_bytes = unit_input_frame_bytes(unit);
    unsigned long chunk = (unsigned long)RENDER_FRAMES * frame_bytes;
    unsigned char *block = fifo_path ? malloc(chunk) : 0;
    int fd = -1;
    unsigned long notified_bytes = 0;

    while (unit->producing) {
        if (fifo_path && block) {
            if (fd < 0) {
                fd = open(fifo_path, 0 /* O_RDONLY */ | 0x0004 /* O_NONBLOCK on Darwin */);
                if (fd < 0) {
                    usleep(50000);
                    continue;
                }
            }
            long got = read(fd, block, chunk);
            if (got <= 0) {
                int err = *__error();
                if (got < 0 && (err == 35 /* EAGAIN */ || err == 4 /* EINTR */)) {
                    usleep(4000);
                    continue;
                }
                close(fd);
                fd = -1;
                usleep(50000);
                continue;
            }
            unsigned long valid = ((unsigned long)got / frame_bytes) * frame_bytes;
            if (valid > 0 && unit->in_ring && unit->in_ring_bytes) {
                unsigned long used = unit->in_ring_written - unit->in_ring_read;
                if (used + valid > unit->in_ring_bytes)
                    unit->in_ring_read = unit->in_ring_written + valid - unit->in_ring_bytes;
                unsigned long pos = unit->in_ring_written % unit->in_ring_bytes;
                for (unsigned long i = 0; i < valid; i++)
                    unit->in_ring[(pos + i) % unit->in_ring_bytes] = block[i];
                __sync_synchronize();
                unit->in_ring_written += valid;
            }
        } else {
            usleep(4000);
        }

        __sync_synchronize();
        while (unit->producing && (unit->in_ring_written - notified_bytes) >= chunk) {
            notified_bytes += chunk;
            if (unit->input_callback) {
                TimeStamp stamp = {unit->input_sample_time, mach_absolute_time(), 1.0, 0, {0}, 3, 0};
                UInt32 flags = 0;
                unit->input_callback(unit->input_context, &flags, &stamp, 1, RENDER_FRAMES, 0);
            }
            unit->input_sample_time += RENDER_FRAMES;
        }
    }
    if (fd >= 0)
        close(fd);
    free(block);
    return 0;
}

static OSStatus unit_ioproc(UInt32 device, const void *now, const BufferList *input, const void *input_time,
                            BufferList *output, const void *output_time, void *context) {
    (void)device; (void)now; (void)input_time; (void)output_time;
    OutputUnit *unit = context;
    /* Copy microphone input from Darling's input IOProc into in_ring. */
    if (unit->input_enabled && input && input->count && input->buffers[0].data &&
        unit->in_ring && unit->in_ring_bytes) {
        const Buffer *in_buf = &input->buffers[0];
        UInt32 in_fb = unit_input_frame_bytes(unit);
        unsigned long got = (in_buf->byte_size / in_fb) * in_fb;
        const unsigned char *src = in_buf->data;
        unsigned long used = unit->in_ring_written - unit->in_ring_read;
        if (used + got > unit->in_ring_bytes)
            unit->in_ring_read = unit->in_ring_written + got - unit->in_ring_bytes;
        unsigned long pos = unit->in_ring_written % unit->in_ring_bytes;
        for (unsigned long i = 0; i < got; i++)
            unit->in_ring[(pos + i) % unit->in_ring_bytes] = src[i];
        __sync_synchronize();
        unit->in_ring_written += got;
    }
    if (!output || !output->count || !unit->output_enabled)
        return NO_ERROR;
    Buffer *buffer = &output->buffers[0];
    UInt32 frame_bytes = unit_frame_bytes(unit);
    unsigned long wanted = (buffer->byte_size / frame_bytes) * frame_bytes;
    buffer->byte_size = (UInt32)wanted;
    unsigned char *bytes = buffer->data;
    __sync_synchronize();
    unsigned long available = unit->ring_written - unit->ring_read;
    unsigned long copied = available < wanted ? available : wanted;
    unsigned long position = unit->ring_read % unit->ring_bytes;
    for (unsigned long i = 0; i < copied; i++)
        bytes[i] = unit->ring[(position + i) % unit->ring_bytes];
    for (unsigned long i = copied; i < wanted; i++)
        bytes[i] = 0; /* underrun: silence */
    unit->ring_read += copied;
    return NO_ERROR; /* an error or 0 bytes would pause Darling's stream */
}

static void unit_stop_producer(OutputUnit *unit) {
    if (unit->producing) {
        unit->producing = 0;
        if (unit->thread) {
            pthread_join(unit->thread, 0);
            unit->thread = 0;
        }
        if (unit->in_thread) {
            pthread_join(unit->in_thread, 0);
            unit->in_thread = 0;
        }
    }
}

static void unit_stop(OutputUnit *unit) {
    unit_stop_producer(unit);
    if (unit->running && unit->ioproc)
        AudioDeviceStop(unit->device, unit->ioproc);
    unit->running = 0;
    if (unit->ioproc) {
        AudioDeviceDestroyIOProcID(unit->device, unit->ioproc);
        unit->ioproc = 0;
    }
}

static void *t_find(void *after, const ComponentDescription *description) {
    void *component = AudioComponentFindNext(after, description);
    if (description && description->type == FOURCC('a', 'u', 'o', 'u') &&
        (description->subtype == FOURCC('a', 'h', 'a', 'l') ||
         description->subtype == FOURCC('d', 'e', 'f', ' ')))
        hal_component = component;
    if (tracing() && description) {
        char type[8], subtype[8], line[160];
        code(type, description->type);
        code(subtype, description->subtype);
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioComponentFindNext '%s'/'%s' -> %p\n",
                              type, subtype, component);
        audio_log_write(line, length);
    }
    return component;
}
DYLD_INTERPOSE(t_find, AudioComponentFindNext)

static OSStatus t_new(void *component, void **instance) {
    if (additions_enabled() && component && component == hal_component && instance) {
        OutputUnit *unit = calloc(1, sizeof *unit);
        if (unit) {
            unit->magic = UNIT_MAGIC;
            unit->device = 2;
            unit->output_enabled = 1;
            unit->input_enabled = 0;
            unit->format = current_format;
            unit->input_format = current_format;
            unit->max_frames = 4096;
            *instance = unit;
            report("AudioComponentInstanceNew (shim output/input unit)", 0, 0, 0, 0, 1);
            return NO_ERROR;
        }
    }
    OSStatus status = AudioComponentInstanceNew(component, instance);
    report("AudioComponentInstanceNew", 0, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_new, AudioComponentInstanceNew)

static OSStatus t_dispose(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop(unit);
        unit->magic = 0;
        free(unit->scratch);
        free(unit->ring);
        free(unit->in_ring);
        free(unit);
        return NO_ERROR;
    }
    return AudioComponentInstanceDispose(instance);
}
DYLD_INTERPOSE(t_dispose, AudioComponentInstanceDispose)

static void report_format(const char *what, const StreamDescription *format) {
    if (!tracing() || !format)
        return;
    char id[8], line[220];
    code(id, format->format_id);
    int length = snprintf(line, sizeof line,
                          "[MacOBlox Audio]   %s format '%s' rate=%.0f flags=0x%x bytes/packet=%u frames/packet=%u "
                          "bytes/frame=%u channels=%u bits=%u\n",
                          what, id, format->sample_rate, format->format_flags, format->bytes_per_packet,
                          format->frames_per_packet, format->bytes_per_frame, format->channels_per_frame,
                          format->bits_per_channel);
    audio_log_write(line, length);
}

static OSStatus unit_set(OutputUnit *unit, UInt32 id, UInt32 scope, UInt32 element, const void *data, UInt32 size) {
    switch (id) {
    case 2003: /* kAudioOutputUnitProperty_EnableIO */
        if (size < 4) return BAD_SIZE;
        if (scope == 2 && element == 0)
            unit->output_enabled = *(const UInt32 *)data != 0;
        else if (scope == 1 && element == 1) {
            unit->input_enabled = *(const UInt32 *)data != 0;
            if (tracing()) {
                char line[128];
                int len = snprintf(line, sizeof line,
                                   "[MacOBlox Audio Mic] EnableIO bus=1 (Input) input_enabled=%d\n",
                                   unit->input_enabled);
                audio_log_write(line, len);
            }
        }
        return NO_ERROR;
    case 2000: /* kAudioOutputUnitProperty_CurrentDevice */
        if (size < 4) return BAD_SIZE;
        unit->device = *(const UInt32 *)data;
        return NO_ERROR;
    case 8: { /* kAudioUnitProperty_StreamFormat */
        if (size != sizeof(StreamDescription)) return BAD_SIZE;
        const StreamDescription *format = data;
        report_format("unit set", format);
        if (format->format_id != FOURCC('l', 'p', 'c', 'm'))
            return -10868; /* kAudioUnitErr_FormatNotSupported */
        if (scope == 1 && element == 0) /* what the client renders to output */
            unit->format = *format;
        else if (scope == 2 && element == 1) /* what the client reads from mic input */
            unit->input_format = *format;
        return NO_ERROR;
    }
    case 23: /* kAudioUnitProperty_SetRenderCallback */
        if (size < 2 * sizeof(void *)) return BAD_SIZE;
        unit->render = ((RenderProc const *)data)[0];
        unit->render_context = ((void *const *)data)[1];
        return NO_ERROR;
    case 2005: /* kAudioOutputUnitProperty_SetInputCallback */
        if (size < 2 * sizeof(void *)) return BAD_SIZE;
        unit->input_callback = ((RenderProc const *)data)[0];
        unit->input_context = ((void *const *)data)[1];
        if (tracing()) {
            char line[140];
            int len = snprintf(line, sizeof line,
                               "[MacOBlox Audio Mic] SetInputCallback (2005) registered proc=%p ctx=%p\n",
                               (void *)(unsigned long)unit->input_callback, unit->input_context);
            audio_log_write(line, len);
        }
        return NO_ERROR;
    case 14: /* kAudioUnitProperty_MaximumFramesPerSlice */
        if (size >= 4) unit->max_frames = *(const UInt32 *)data;
        return NO_ERROR;
    }
    return NO_ERROR; /* accept and ignore anything else */
}

static OSStatus unit_get(OutputUnit *unit, UInt32 id, UInt32 scope, UInt32 element, void *data, UInt32 *size) {
    switch (id) {
    case 8: /* StreamFormat */
        if (element == 1)
            return put(scope == 2 ? &unit->input_format : &current_format, sizeof unit->input_format, size, data);
        return put(&unit->format, sizeof unit->format, size, data);
    case 2000:
        return put_u32(unit->device, size, data);
    case 2003:
        if (scope == 1 && element == 1)
            return put_u32((UInt32)unit->input_enabled, size, data);
        return put_u32(scope == 2 ? (UInt32)unit->output_enabled : 0, size, data);
    case 2001: /* IsRunning */
        return put_u32((UInt32)unit->running, size, data);
    case 2006: /* HasIO */
        if (scope == 1 && element == 1)
            return put_u32(1, size, data);
        return put_u32(scope == 2 ? 1 : 0, size, data);
    case 14:
        return put_u32(unit->max_frames, size, data);
    case 12: { /* Latency */
        Float64 zero = 0;
        return put(&zero, sizeof zero, size, data);
    }
    }
    return INVALID_PROPERTY;
}

static OSStatus t_unit_set(void *instance, UInt32 id, UInt32 scope, UInt32 element, const void *data, UInt32 size) {
    OutputUnit *unit = as_unit(instance);
    OSStatus status = unit ? unit_set(unit, id, scope, element, data, size)
                           : AudioUnitSetProperty(instance, id, scope, element, data, size);
    if (tracing()) {
        char line[160];
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioUnitSetProperty id=%u scope=%u element=%u -> %d%s\n",
                              id, scope, element, status, unit ? " (shim)" : "");
        audio_log_write(line, length);
    }
    return status;
}
DYLD_INTERPOSE(t_unit_set, AudioUnitSetProperty)

static OSStatus t_unit_get(void *instance, UInt32 id, UInt32 scope, UInt32 element, void *data, UInt32 *size) {
    OutputUnit *unit = as_unit(instance);
    OSStatus status = unit ? unit_get(unit, id, scope, element, data, size)
                           : AudioUnitGetProperty(instance, id, scope, element, data, size);
    if (tracing()) {
        char line[160];
        int length = snprintf(line, sizeof line, "[MacOBlox Audio] AudioUnitGetProperty id=%u scope=%u element=%u -> %d%s\n",
                              id, scope, element, status, unit ? " (shim)" : "");
        audio_log_write(line, length);
    }
    return status;
}
DYLD_INTERPOSE(t_unit_get, AudioUnitGetProperty)

static volatile unsigned long mic_render_calls;

static OSStatus t_unit_render(void *instance, UInt32 *action_flags, const void *time_stamp,
                              UInt32 bus_number, UInt32 number_frames, BufferList *data) {
    OutputUnit *unit = as_unit(instance);
    if (!unit) {
        return AudioUnitRender ? AudioUnitRender(instance, action_flags, time_stamp, bus_number, number_frames, data)
                               : -10879;
    }
    if (!data || !data->count)
        return NO_ERROR;
    if (bus_number == 1) {
        UInt32 channels = unit->input_format.channels_per_frame ? unit->input_format.channels_per_frame : 2;
        UInt32 sample_bytes = unit->input_format.bits_per_channel / 8 ? unit->input_format.bits_per_channel / 8 : 4;
        UInt32 frame_bytes = channels * sample_bytes;
        unsigned long wanted = (unsigned long)number_frames * frame_bytes;
        __sync_synchronize();
        unsigned long available = (unit->in_ring && unit->in_ring_bytes)
                                      ? (unit->in_ring_written - unit->in_ring_read)
                                      : 0;
        unsigned long copied = available < wanted ? available : wanted;
        unsigned long pos = unit->in_ring_bytes ? (unit->in_ring_read % unit->in_ring_bytes) : 0;
        unsigned long non_zero_bytes = 0;
        for (unsigned long i = 0; i < copied; i++) {
            if (unit->in_ring[(pos + i) % unit->in_ring_bytes] != 0)
                non_zero_bytes++;
        }
        int planar = (unit->input_format.format_flags & 0x20) != 0;
        if (!planar || data->count == 1) {
            unsigned char *dst = data->buffers[0].data;
            data->buffers[0].byte_size = (UInt32)wanted;
            if (dst) {
                for (unsigned long i = 0; i < copied; i++)
                    dst[i] = unit->in_ring[(pos + i) % unit->in_ring_bytes];
                for (unsigned long i = copied; i < wanted; i++)
                    dst[i] = 0;
            }
        } else {
            UInt32 copied_frames = (UInt32)(copied / frame_bytes);
            for (UInt32 c = 0; c < data->count; c++) {
                unsigned char *dst = data->buffers[c].data;
                data->buffers[c].byte_size = number_frames * sample_bytes;
                if (!dst) continue;
                for (UInt32 f = 0; f < number_frames; f++) {
                    for (UInt32 b = 0; b < sample_bytes; b++) {
                        if (f < copied_frames && c < channels) {
                            unsigned long idx = (pos + ((unsigned long)f * channels + c) * sample_bytes + b) %
                                                unit->in_ring_bytes;
                            dst[f * sample_bytes + b] = unit->in_ring[idx];
                        } else {
                            dst[f * sample_bytes + b] = 0;
                        }
                    }
                }
            }
        }
        unit->in_ring_read += copied;
        if (tracing()) {
            unsigned long call_no = __sync_add_and_fetch(&mic_render_calls, 1);
            if (call_no <= 10 || call_no % 100 == 0) {
                char line[200];
                int len = snprintf(line, sizeof line,
                                   "[MacOBlox Audio Mic] AudioUnitRender(bus=1) #%lu frames=%u copied=%lu/%lu non_zero_bytes=%lu planar=%d\n",
                                   call_no, number_frames, copied, wanted, non_zero_bytes, planar);
                audio_log_write(line, len);
            }
        }
        return NO_ERROR;
    }
    return NO_ERROR;
}
DYLD_INTERPOSE(t_unit_render, AudioUnitRender)

static OSStatus t_init(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit->initialized = 1;
        return NO_ERROR;
    }
    OSStatus status = AudioUnitInitialize(instance);
    report("AudioUnitInitialize", 0, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_init, AudioUnitInitialize)

static OSStatus t_uninit(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop(unit);
        unit->initialized = 0;
        return NO_ERROR;
    }
    return AudioUnitUninitialize(instance);
}
DYLD_INTERPOSE(t_uninit, AudioUnitUninitialize)

static OSStatus t_start(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (!unit) {
        OSStatus status = AudioOutputUnitStart(instance);
        report("AudioOutputUnitStart", 0, 0, status, 0, 0);
        return status;
    }
    if (unit->running || (!unit->output_enabled && !unit->input_enabled))
        return NO_ERROR;

    unsigned char attributes[64] = {0}; /* pthread_attr_t is 64 bytes on Darwin x86_64 */
    pthread_attr_init(attributes);
    pthread_attr_setstacksize(attributes, 8u << 20);
    unit->producing = 1;

    if (unit->input_enabled) {
        UInt32 in_fb = unit_input_frame_bytes(unit);
        if (!unit->in_ring) {
            unit->in_ring_bytes = (unsigned long)RING_FRAMES * in_fb;
            unit->in_ring = calloc(1, unit->in_ring_bytes);
        }
        unit->in_ring_written = unit->in_ring_read = 0;
        if (pthread_create(&unit->in_thread, attributes, unit_mic_thread, unit) != 0) {
            unit->in_thread = 0;
        }
        if (tracing()) {
            const char *fifo_env = getenv("MACOBLOX_MIC_FIFO");
            char line[200];
            int len = snprintf(line, sizeof line,
                               "[MacOBlox Audio Mic] Started mic capture thread: device=%u fifo=%s ring_bytes=%lu\n",
                               unit->device, fifo_env ? fifo_env : "(Darling HAL device 3)", unit->in_ring_bytes);
            audio_log_write(line, len);
        }
    }

    int out_uses_fifo = (unit->output_enabled && getenv("MACOBLOX_AUDIO_FIFO") != 0);
    int in_uses_fifo = (unit->input_enabled && getenv("MACOBLOX_MIC_FIFO") != 0);

    if (out_uses_fifo) {
        if ((unit->format.format_flags & 0x20) && !unit->scratch)
            unit->scratch = malloc((unsigned long)RENDER_FRAMES * unit_frame_bytes(unit));
        if (pthread_create(&unit->thread, attributes, unit_fifo_thread, unit) != 0) {
            unit_stop_producer(unit);
            return -1;
        }
        report("unit: start (host FIFO)", 0, 0, 0, 0, 1);
    }

    if (( !unit->output_enabled || out_uses_fifo ) && ( !unit->input_enabled || in_uses_fifo )) {
        unit->running = 1;
        return NO_ERROR;
    }

    /* Darling's device takes the stream format through the legacy
     * kAudioDevicePropertyStreamFormat ('sfmt') and wants it interleaved;
     * a non-interleaved client format is interleaved in the IOProc. */
    int need_hal_out = (unit->output_enabled && !out_uses_fifo);
    StreamDescription device_format = need_hal_out ? unit->format : unit->input_format;
    if (device_format.format_flags & 0x20) {
        device_format.format_flags &= ~0x20u;
        device_format.bytes_per_frame = device_format.channels_per_frame * (device_format.bits_per_channel / 8);
        device_format.bytes_per_packet = device_format.bytes_per_frame;
    }
    if (need_hal_out) {
        UInt32 frame_bytes = unit_frame_bytes(unit);
        if (!unit->ring) {
            unit->ring_bytes = (unsigned long)RING_FRAMES * frame_bytes;
            unit->ring = calloc(1, unit->ring_bytes);
        }
        if ((unit->format.format_flags & 0x20) && !unit->scratch)
            unit->scratch = malloc((unsigned long)RENDER_FRAMES * frame_bytes);
        if (!unit->ring) {
            unit_stop_producer(unit);
            return -1;
        }
        unit->ring_written = unit->ring_read = 0;
        if (pthread_create(&unit->thread, attributes, unit_render_thread, unit) != 0) {
            unit_stop_producer(unit);
            return -1;
        }
    }
    if (!need_hal_out && unit->input_enabled && unit->device == 2)
        unit->device = 3;
    UInt32 fmt_scope = is_input_device(unit->device) ? SCOPE_INPUT : SCOPE_OUTPUT;
    PropertyAddress format_address = {FOURCC('s', 'f', 'm', 't'), fmt_scope, 0};
    OSStatus status = AudioObjectSetPropertyData(unit->device, &format_address, 0, 0,
                                                 sizeof device_format, &device_format);
    report("unit: device format", unit->device, &format_address, status, 0, 1);
    status = AudioDeviceCreateIOProcID(unit->device, (void *)unit_ioproc, unit, &unit->ioproc);
    if (status == NO_ERROR)
        status = AudioDeviceStart(unit->device, unit->ioproc);
    unit->running = status == NO_ERROR;
    report("unit: start", unit->device, 0, status, 0, 1);
    return status;
}
DYLD_INTERPOSE(t_start, AudioOutputUnitStart)

static OSStatus t_stop(void *instance) {
    OutputUnit *unit = as_unit(instance);
    if (unit) {
        unit_stop_producer(unit);
        if (unit->running && unit->ioproc)
            AudioDeviceStop(unit->device, unit->ioproc);
        unit->running = 0;
        return NO_ERROR;
    }
    return AudioOutputUnitStop(instance);
}
DYLD_INTERPOSE(t_stop, AudioOutputUnitStop)

static OSStatus t_ioproc(UInt32 device, void *proc, void *data, void **id) {
    OSStatus status = AudioDeviceCreateIOProcID(device, proc, data, id);
    report("AudioDeviceCreateIOProcID", device, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_ioproc, AudioDeviceCreateIOProcID)

static OSStatus t_dstart(UInt32 device, void *proc) {
    OSStatus status = AudioDeviceStart(device, proc);
    report("AudioDeviceStart", device, 0, status, 0, 0);
    return status;
}
DYLD_INTERPOSE(t_dstart, AudioDeviceStart)

/* Host time: Darling's AudioGetCurrentHostTime is a stub (logs "STUB" and
 * returns nothing useful), and FMOD times its mixing with it. Darling's
 * mach_absolute_time counts nanoseconds, so host time is nanoseconds too. */
W unsigned long long AudioGetCurrentHostTime(void);
W Float64 AudioGetHostClockFrequency(void);
W unsigned long long AudioConvertNanosToHostTime(unsigned long long);

static unsigned long long h_host_time(void) {
    return mach_absolute_time();
}
DYLD_INTERPOSE(h_host_time, AudioGetCurrentHostTime)

static Float64 h_host_frequency(void) {
    return 1000000000.0;
}
DYLD_INTERPOSE(h_host_frequency, AudioGetHostClockFrequency)

static unsigned long long h_nanos_to_host(unsigned long long nanos) {
    return nanos;
}
DYLD_INTERPOSE(h_nanos_to_host, AudioConvertNanosToHostTime)
