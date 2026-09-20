/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/fuzz.h"
#include "libromano/logger.h"

#include <float.h>
#include <math.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <unistd.h>
#define FUZZ_HAS_SIGACTION 1
#else
#define FUZZ_HAS_SIGACTION 0
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */

#define FUZZ_MAX_MINIMIZE_ATTEMPTS 4096

static uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

ROMANO_FORCE_INLINE uint64_t rotl64(const uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

static uint64_t xoshiro256ss(uint64_t* s)
{
    const uint64_t result = rotl64(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);

    return result;
}

void fuzz_source_init_seed(FuzzSource* source, uint64_t seed)
{
    memset(source, 0, sizeof(FuzzSource));

    source->state[0] = splitmix64(&seed);
    source->state[1] = splitmix64(&seed);
    source->state[2] = splitmix64(&seed);
    source->state[3] = splitmix64(&seed);
}

void fuzz_source_init_data(FuzzSource* source, const uint8_t* data, size_t size)
{
    memset(source, 0, sizeof(FuzzSource));

    source->data = data;
    source->size = data != NULL ? size : 0;
    source->from_data = true;
}

bool fuzz_source_exhausted(const FuzzSource* source)
{
    return source->exhausted;
}

static uint64_t source_read_bytes(FuzzSource* source, size_t count)
{
    uint64_t value = 0;
    size_t i;

    for(i = 0; i < count; i++)
    {
        if(source->position >= source->size)
        {
            source->exhausted = true;
            break;
        }

        value |= (uint64_t)source->data[source->position++] << (8 * i);
    }

    return value;
}

uint64_t fuzz_u64(FuzzSource* source)
{
    if(source->from_data)
        return source_read_bytes(source, 8);

    return xoshiro256ss(source->state);
}

uint32_t fuzz_u32(FuzzSource* source)
{
    if(source->from_data)
        return (uint32_t)source_read_bytes(source, 4);

    return (uint32_t)(fuzz_u64(source) >> 32);
}

uint8_t fuzz_u8(FuzzSource* source)
{
    if(source->from_data)
        return (uint8_t)source_read_bytes(source, 1);

    return (uint8_t)(fuzz_u64(source) >> 56);
}

bool fuzz_bool(FuzzSource* source)
{
    return (fuzz_u8(source) & 1) != 0;
}

static size_t bytes_needed(uint64_t span)
{
    size_t n = 1;

    while(n < 8 && (span >> (8 * n)) != 0)
        n++;

    return n;
}

uint64_t fuzz_range(FuzzSource* source, uint64_t low, uint64_t high)
{
    uint64_t span;
    uint64_t range;
    uint64_t threshold;
    uint64_t x;

    if(low >= high)
        return low;

    span = high - low;

    if(source->from_data)
    {
        x = source_read_bytes(source, bytes_needed(span));
        return span == UINT64_MAX ? x : low + x % (span + 1);
    }

    if(span == UINT64_MAX)
        return fuzz_u64(source);

    range = span + 1;
    threshold = (0 - range) % range;

    do
    {
        x = fuzz_u64(source);
    } while(x < threshold);

    return low + x % range;
}

int64_t fuzz_range_i64(FuzzSource* source, int64_t low, int64_t high)
{
    uint64_t span;

    if(low >= high)
        return low;

    span = (uint64_t)high - (uint64_t)low;

    return (int64_t)((uint64_t)low + fuzz_range(source, 0, span));
}

size_t fuzz_index(FuzzSource* source, size_t count)
{
    if(count <= 1)
        return 0;

    return (size_t)fuzz_range(source, 0, (uint64_t)count - 1);
}

bool fuzz_one_in(FuzzSource* source, uint32_t n)
{
    return n <= 1 || fuzz_range(source, 0, n - 1) == 0;
}

double fuzz_f64_unit(FuzzSource* source)
{
    return (double)(fuzz_u64(source) >> 11) * (1.0 / 9007199254740992.0);
}

void fuzz_bytes(FuzzSource* source, void* out, size_t size)
{
    uint8_t* bytes = (uint8_t*)out;
    size_t i = 0;

    if(source->from_data)
    {
        size_t available = source->size - source->position;
        size_t n = size < available ? size : available;

        memcpy(bytes, source->data + source->position, n);
        source->position += n;

        if(n < size)
        {
            memset(bytes + n, 0, size - n);
            source->exhausted = true;
        }

        return;
    }

    while(i + 8 <= size)
    {
        uint64_t x = fuzz_u64(source);
        memcpy(bytes + i, &x, 8);
        i += 8;
    }

    if(i < size)
    {
        uint64_t x = fuzz_u64(source);
        memcpy(bytes + i, &x, size - i);
    }
}

size_t fuzz_size(FuzzSource* source, size_t max)
{
    uint32_t bits = 0;
    uint64_t limit;

    if(max == 0)
        return 0;

    switch(fuzz_range(source, 0, 15))
    {
        case 0:
            return 0;
        case 1:
            return max;
        case 2:
            return max > 1 ? max - 1 : max;
        default:
            break;
    }

    while(bits < 64 && ((uint64_t)max >> bits) != 0)
        bits++;

    bits = (uint32_t)fuzz_range(source, 0, bits);
    limit = bits >= 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;

    if(limit > max)
        limit = max;

    return (size_t)fuzz_range(source, 0, limit);
}

static const uint64_t g_special_u64[] = {
    0ULL, 1ULL, 2ULL, 3ULL, 7ULL, 8ULL, 9ULL, 10ULL, 15ULL, 16ULL, 31ULL, 32ULL, 63ULL, 64ULL,
    99ULL, 100ULL, 127ULL, 128ULL, 255ULL, 256ULL, 999ULL, 1000ULL,
    0x7FFFULL, 0x8000ULL, 0xFFFFULL, 0x10000ULL,
    0x7FFFFFFFULL, 0x80000000ULL, 0xFFFFFFFFULL, 0x100000000ULL,
    9999999999ULL, 10000000000ULL,
    0x001FFFFFFFFFFFFFULL, 0x0020000000000000ULL,
    999999999999999999ULL, 1000000000000000000ULL, 9999999999999999999ULL, 10000000000000000000ULL,
    0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL,
};

#define SPECIAL_U64_COUNT (sizeof(g_special_u64) / sizeof(g_special_u64[0]))

uint64_t fuzz_u64_special(FuzzSource* source)
{
    switch(fuzz_range(source, 0, 7))
    {
        case 0:
        case 1:
        case 2:
            return g_special_u64[fuzz_index(source, SPECIAL_U64_COUNT)];
        case 3:
        {
            uint64_t v = (uint64_t)1 << fuzz_range(source, 0, 63);
            return v + (uint64_t)fuzz_range_i64(source, -2, 2);
        }
        case 4:
            return fuzz_u64(source) >> fuzz_range(source, 0, 63);
        default:
            return fuzz_u64(source);
    }
}

uint32_t fuzz_u32_special(FuzzSource* source)
{
    uint64_t v = fuzz_u64_special(source);
    return v > UINT32_MAX && fuzz_bool(source) ? UINT32_MAX : (uint32_t)v;
}

int64_t fuzz_i64_special(FuzzSource* source)
{
    switch(fuzz_range(source, 0, 7))
    {
        case 0:
            return INT64_MIN;
        case 1:
            return INT64_MAX;
        case 2:
            return (int64_t)fuzz_range_i64(source, -3, 3);
        case 3:
        case 4:
        {
            int64_t v = (int64_t)(fuzz_u64_special(source) & 0x7FFFFFFFFFFFFFFFULL);
            return fuzz_bool(source) ? -v : v;
        }
        default:
            return (int64_t)fuzz_u64(source);
    }
}

int32_t fuzz_i32_special(FuzzSource* source)
{
    switch(fuzz_range(source, 0, 5))
    {
        case 0:
            return INT32_MIN;
        case 1:
            return INT32_MAX;
        case 2:
            return (int32_t)fuzz_range_i64(source, -3, 3);
        default:
            return (int32_t)(uint32_t)fuzz_i64_special(source);
    }
}

static const double g_special_f64[] = {
    0.0, 1.0, -1.0, 0.5, 0.1, 0.25, 0.3, 0.9999999, 9.5, 99.5, 0.05, 0.005, 1.5, 2.5,
    123.456, 1e-7, 1e-300, 1e7, 1e15, 1e16, 1e17, 1e19, 1e20, 1e300,
    9007199254740991.0, 9007199254740992.0, 9223372036854775807.0, 18446744073709551615.0,
    DBL_EPSILON, DBL_MIN, DBL_MAX, -DBL_MAX, 4.9406564584124654e-324,
};

#define SPECIAL_F64_COUNT (sizeof(g_special_f64) / sizeof(g_special_f64[0]))

double fuzz_f64_special(FuzzSource* source)
{
    uint64_t bits;
    double d;

    switch(fuzz_range(source, 0, 11))
    {
        case 0:
            return -0.0;
        case 1:
            return fuzz_bool(source) ? HUGE_VAL : -HUGE_VAL;
        case 2:
            return NAN;
        case 3:
        case 4:
            d = g_special_f64[fuzz_index(source, SPECIAL_F64_COUNT)];
            return fuzz_bool(source) ? -d : d;
        case 5:
            bits = fuzz_u64(source);
            memcpy(&d, &bits, sizeof(double));
            return d;
        case 6:
        case 7:
            d = (double)fuzz_i64_special(source) / pow(10.0, (double)fuzz_range(source, 0, 17));
            return d;
        default:
            d = ldexp(fuzz_f64_unit(source) + 0.5, (int)fuzz_range_i64(source, -64, 64));
            return fuzz_bool(source) ? -d : d;
    }
}

double fuzz_f64_finite(FuzzSource* source)
{
    int attempt;

    for(attempt = 0; attempt < 8; attempt++)
    {
        double d = fuzz_f64_special(source);

        if(isfinite(d))
            return d;
    }

    return 0.0;
}

size_t fuzz_string(FuzzSource* source, char* out, size_t max_size, const char* alphabet)
{
    size_t size = fuzz_size(source, max_size);
    size_t alphabet_size = alphabet != NULL ? strlen(alphabet) : 0;
    size_t i;

    for(i = 0; i < size; i++)
    {
        if(alphabet_size == 0)
            out[i] = (char)(1 + fuzz_range(source, 0, 254));
        else
            out[i] = alphabet[fuzz_index(source, alphabet_size)];
    }

    out[size] = '\0';

    return size;
}

static const uint8_t g_special_bytes[] = {
    0x00, 0x01, 0x7F, 0x80, 0xFF, 0xFE, '0', '9', 'a', 'z', ' ', '\t', '\n', '"', '\'',
    '\\', '/', '{', '}', '[', ']', '(', ')', ',', ':', '.', '-', '+', '*', '?', '|', '^', '$',
};

typedef enum MutationKind {
    Mutation_FlipBit,
    Mutation_RandomByte,
    Mutation_SpecialByte,
    Mutation_AddToByte,
    Mutation_SwapBytes,
    Mutation_Erase,
    Mutation_Truncate,
    Mutation_Insert,
    Mutation_InsertRepeated,
    Mutation_Duplicate,
    Mutation_InsertToken,
    Mutation_OverwriteToken,
    Mutation_Count,
} MutationKind;

static size_t mutation_insert(uint8_t* data, size_t size, size_t capacity, size_t position, const uint8_t* bytes, size_t count)
{
    if(size >= capacity)
        return size;

    if(count > capacity - size)
        count = capacity - size;

    memmove(data + position + count, data + position, size - position);
    memcpy(data + position, bytes, count);

    return size + count;
}

size_t fuzz_mutate(FuzzSource* source,
                   uint8_t* data,
                   size_t size,
                   size_t capacity,
                   const FuzzDictionary* dictionary)
{
    uint8_t chunk[64];
    size_t position;
    size_t count;
    MutationKind kind;
    bool has_tokens = dictionary != NULL && dictionary->count > 0;

    if(size > capacity)
        size = capacity;

    if(capacity == 0)
        return 0;

    kind = (MutationKind)fuzz_range(source, 0, Mutation_Count - 1);

    if((kind == Mutation_InsertToken || kind == Mutation_OverwriteToken) && !has_tokens)
        kind = Mutation_Insert;

    if(size == 0)
        kind = has_tokens && fuzz_bool(source) ? Mutation_InsertToken : Mutation_Insert;

    position = fuzz_index(source, size);

    switch(kind)
    {
        case Mutation_FlipBit:
            data[position] ^= (uint8_t)(1u << fuzz_range(source, 0, 7));
            return size;

        case Mutation_RandomByte:
            data[position] = fuzz_u8(source);
            return size;

        case Mutation_SpecialByte:
            data[position] = g_special_bytes[fuzz_index(source, sizeof(g_special_bytes))];
            return size;

        case Mutation_AddToByte:
            data[position] = (uint8_t)(data[position] + (uint8_t)fuzz_range_i64(source, -8, 8));
            return size;

        case Mutation_SwapBytes:
        {
            size_t other = fuzz_index(source, size);
            uint8_t tmp = data[position];
            data[position] = data[other];
            data[other] = tmp;
            return size;
        }

        case Mutation_Erase:
            count = 1 + fuzz_size(source, size - position - 1);
            memmove(data + position, data + position + count, size - position - count);
            return size - count;

        case Mutation_Truncate:
            return position;

        case Mutation_Insert:
            count = 1 + fuzz_index(source, 8);
            fuzz_bytes(source, chunk, count);
            return mutation_insert(data, size, capacity, fuzz_index(source, size + 1), chunk, count);

        case Mutation_InsertRepeated:
            count = 1 + fuzz_index(source, sizeof(chunk));
            memset(chunk, size > 0 ? data[position] : fuzz_u8(source), count);
            return mutation_insert(data, size, capacity, fuzz_index(source, size + 1), chunk, count);

        case Mutation_Duplicate:
            count = 1 + fuzz_index(source, size - position < sizeof(chunk) ? size - position : sizeof(chunk));
            memcpy(chunk, data + position, count);
            return mutation_insert(data, size, capacity, fuzz_index(source, size + 1), chunk, count);

        case Mutation_InsertToken:
        case Mutation_OverwriteToken:
        {
            const char* token = dictionary->tokens[fuzz_index(source, dictionary->count)];
            size_t token_size = strlen(token);

            if(kind == Mutation_InsertToken || size == 0)
                return mutation_insert(data, size, capacity, fuzz_index(source, size + 1), (const uint8_t*)token, token_size);

            count = token_size < size - position ? token_size : size - position;
            memcpy(data + position, token, count);
            return size;
        }

        default:
            return size;
    }
}

static FuzzRunInfo g_run_info;
static bool g_run_active = false;

const FuzzRunInfo* fuzz_current_run(void)
{
    return g_run_active ? &g_run_info : NULL;
}

static const int g_crash_signals[] = {
    SIGSEGV,
    SIGILL,
    SIGFPE,
    SIGABRT,
#if defined(SIGBUS)
    SIGBUS,
#endif /* defined(SIGBUS) */
};

#define CRASH_SIGNALS_COUNT (sizeof(g_crash_signals) / sizeof(g_crash_signals[0]))

static bool g_crash_handlers_installed = false;

#if FUZZ_HAS_SIGACTION
static struct sigaction g_previous_actions[CRASH_SIGNALS_COUNT];
#else
typedef void (*SignalHandler)(int);
static SignalHandler g_previous_actions[CRASH_SIGNALS_COUNT];
#endif /* FUZZ_HAS_SIGACTION */

static void crash_handlers_restore(void)
{
    size_t i;

    if(!g_crash_handlers_installed)
        return;

    for(i = 0; i < CRASH_SIGNALS_COUNT; i++)
    {
#if FUZZ_HAS_SIGACTION
        sigaction(g_crash_signals[i], &g_previous_actions[i], NULL);
#else
        signal(g_crash_signals[i], g_previous_actions[i]);
#endif /* FUZZ_HAS_SIGACTION */
    }

    g_crash_handlers_installed = false;
}

static void crash_print_info(int sig)
{
    char message[512];
    int size;

    size = snprintf(message,
                    sizeof(message),
                    "\n[FATAL] fuzz '%s': signal %d at iteration %llu, replay with ROMANO_FUZZ_REPLAY=0x%016llx\n",
                    g_run_info.name != NULL ? g_run_info.name : "?",
                    sig,
                    (unsigned long long)g_run_info.iteration,
                    (unsigned long long)g_run_info.iteration_seed);

    if(size <= 0)
        return;

    if((size_t)size >= sizeof(message))
        size = (int)sizeof(message) - 1;

#if FUZZ_HAS_SIGACTION
    if(write(STDERR_FILENO, message, (size_t)size) < 0)
        return;
#else
    fputs(message, stderr);
    fflush(stderr);
#endif /* FUZZ_HAS_SIGACTION */
}

#if FUZZ_HAS_SIGACTION
static void crash_handler(int sig, siginfo_t* info, void* context)
{
    ROMANO_UNUSED(context);

    crash_print_info(sig);
    crash_handlers_restore();

    /* Faults generated by an instruction are raised again when returning, now reaching the previous handler */
    if(info == NULL || info->si_code <= 0 || sig == SIGABRT)
        raise(sig);
}
#else
static void crash_handler(int sig)
{
    crash_print_info(sig);
    crash_handlers_restore();
    raise(sig);
}
#endif /* FUZZ_HAS_SIGACTION */

static void crash_handlers_install(void)
{
    size_t i;

    if(g_crash_handlers_installed)
        return;

    for(i = 0; i < CRASH_SIGNALS_COUNT; i++)
    {
#if FUZZ_HAS_SIGACTION
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_sigaction = crash_handler;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        sigaction(g_crash_signals[i], &action, &g_previous_actions[i]);
#else
        g_previous_actions[i] = signal(g_crash_signals[i], crash_handler);
#endif /* FUZZ_HAS_SIGACTION */
    }

    g_crash_handlers_installed = true;
}

void fuzz_reset(void)
{
    crash_handlers_restore();
    g_run_active = false;
    memset(&g_run_info, 0, sizeof(FuzzRunInfo));
}

void fuzz_options_init(FuzzOptions* options, const char* name)
{
    memset(options, 0, sizeof(FuzzOptions));

    options->name = name;
    options->seed = FUZZ_DEFAULT_SEED;
    options->iterations = FUZZ_DEFAULT_ITERATIONS;
    options->max_input_size = FUZZ_DEFAULT_MAX_INPUT_SIZE;
    options->minimize = true;
    options->catch_crashes = true;
}

void fuzz_report_release(FuzzReport* report)
{
    if(report == NULL)
        return;

    free(report->failing_input);
    memset(report, 0, sizeof(FuzzReport));
}

static bool env_u64(const char* name, uint64_t* value)
{
    const char* env = getenv(name);
    char* end = NULL;
    unsigned long long parsed;

    if(env == NULL || env[0] == '\0')
        return false;

    parsed = strtoull(env, &end, 0);

    if(end == env)
        return false;

    *value = (uint64_t)parsed;

    return true;
}

typedef struct RunSettings {
    FuzzOptions options;
    bool replay;
    uint64_t replay_seed;
} RunSettings;

static void settings_init(RunSettings* settings, const FuzzOptions* options, const char* default_name)
{
    const char* env_seed = getenv("ROMANO_FUZZ_SEED");
    const char* env_seconds = getenv("ROMANO_FUZZ_SECONDS");
    uint64_t value;

    if(options != NULL)
        settings->options = *options;
    else
        fuzz_options_init(&settings->options, default_name);

    if(settings->options.name == NULL)
        settings->options.name = default_name;

    if(settings->options.max_input_size == 0)
        settings->options.max_input_size = FUZZ_DEFAULT_MAX_INPUT_SIZE;

    if(env_seed != NULL && strcmp(env_seed, "random") == 0)
    {
        uint64_t mix = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32) ^ (uint64_t)(uintptr_t)settings;
        settings->options.seed = splitmix64(&mix);
    }
    else if(env_u64("ROMANO_FUZZ_SEED", &value))
    {
        settings->options.seed = value;
    }

    if(env_u64("ROMANO_FUZZ_ITERATIONS", &value))
        settings->options.iterations = value;

    if(env_u64("ROMANO_FUZZ_SCALE", &value) && value > 0)
        settings->options.iterations *= value;

    if(env_seconds != NULL && env_seconds[0] != '\0')
        settings->options.max_seconds = strtod(env_seconds, NULL);

    settings->replay = env_u64("ROMANO_FUZZ_REPLAY", &settings->replay_seed);

    if(settings->replay)
        settings->options.iterations = 1;
}

static uint64_t iteration_seed(const RunSettings* settings, uint64_t iteration)
{
    uint64_t state;

    if(settings->replay)
        return settings->replay_seed;

    state = settings->options.seed ^ (iteration * 0xD1B54A32D192ED03ULL);

    return splitmix64(&state);
}

static double seconds_since(clock_t start)
{
    return (double)(clock() - start) / (double)CLOCKS_PER_SEC;
}

static void run_begin(const RunSettings* settings)
{
    g_run_info.name = settings->options.name;
    g_run_info.iteration = 0;
    g_run_info.iteration_seed = 0;
    g_run_info.minimizing = false;
    g_run_active = true;

    if(settings->options.catch_crashes)
        crash_handlers_install();

    if(settings->replay)
        logger_log_info("fuzz '%s': replaying iteration seed 0x%016llx",
                        settings->options.name,
                        (unsigned long long)settings->replay_seed);
    else
        logger_log_debug("fuzz '%s': seed 0x%016llx, %llu iterations",
                         settings->options.name,
                         (unsigned long long)settings->options.seed,
                         (unsigned long long)settings->options.iterations);
}

static void run_end(const RunSettings* settings, FuzzReport* report)
{
    fuzz_reset();

    if(report->failed)
    {
        logger_log_error("fuzz '%s': failure at iteration %llu (seed 0x%016llx)",
                         settings->options.name,
                         (unsigned long long)report->failing_iteration,
                         (unsigned long long)report->seed);
        logger_log_error("fuzz '%s': replay with ROMANO_FUZZ_REPLAY=0x%016llx",
                         settings->options.name,
                         (unsigned long long)report->failing_seed);
    }
    else
    {
        logger_log_debug("fuzz '%s': %llu iterations passed in %.3fs",
                         settings->options.name,
                         (unsigned long long)report->iterations,
                         report->elapsed_seconds);
    }
}

static bool budget_exceeded(const RunSettings* settings, clock_t start)
{
    return settings->options.max_seconds > 0.0 && seconds_since(start) > settings->options.max_seconds;
}

bool fuzz_run_property(const FuzzOptions* options,
                       FuzzPropertyFunc property,
                       void* user_data,
                       FuzzReport* report)
{
    RunSettings settings;
    FuzzReport local_report;
    FuzzSource source;
    clock_t start = clock();
    uint64_t i;
    bool passed;

    if(report == NULL)
        report = &local_report;

    memset(report, 0, sizeof(FuzzReport));
    settings_init(&settings, options, "property");
    report->seed = settings.options.seed;

    run_begin(&settings);

    for(i = 0; i < settings.options.iterations; i++)
    {
        if(i > 0 && budget_exceeded(&settings, start))
            break;

        g_run_info.iteration = i;
        g_run_info.iteration_seed = iteration_seed(&settings, i);

        fuzz_source_init_seed(&source, g_run_info.iteration_seed);

        report->iterations++;

        if(!property(&source, user_data))
        {
            report->failed = true;
            report->failing_iteration = i;
            report->failing_seed = g_run_info.iteration_seed;
            break;
        }
    }

    report->elapsed_seconds = seconds_since(start);

    run_end(&settings, report);

    passed = !report->failed;

    if(report == &local_report)
        fuzz_report_release(report);

    return passed;
}

static size_t generate_input(FuzzSource* source, const RunSettings* settings, uint8_t* buffer)
{
    const FuzzOptions* options = &settings->options;
    size_t size = 0;
    size_t mutations;
    size_t i;

    if(options->corpus_count > 0 && !fuzz_one_in(source, 8))
    {
        const FuzzCorpusEntry* entry = &options->corpus[fuzz_index(source, options->corpus_count)];

        size = entry->size < options->max_input_size ? entry->size : options->max_input_size;
        memcpy(buffer, entry->data, size);
    }
    else if(fuzz_bool(source))
    {
        size = fuzz_size(source, options->max_input_size);
        fuzz_bytes(source, buffer, size);
    }

    mutations = 1 + fuzz_size(source, 15);

    for(i = 0; i < mutations; i++)
        size = fuzz_mutate(source, buffer, size, options->max_input_size, options->dictionary);

    return size;
}

static size_t minimize_input(FuzzInputFunc target, void* user_data, uint8_t* input, size_t size)
{
    uint8_t* candidate = (uint8_t*)malloc(size > 0 ? size : 1);
    size_t chunk = size / 2;
    size_t attempts = 0;

    if(candidate == NULL)
        return size;

    g_run_info.minimizing = true;

    while(chunk > 0 && attempts < FUZZ_MAX_MINIMIZE_ATTEMPTS)
    {
        size_t position = 0;

        while(position + chunk <= size && attempts < FUZZ_MAX_MINIMIZE_ATTEMPTS)
        {
            memcpy(candidate, input, position);
            memcpy(candidate + position, input + position + chunk, size - position - chunk);

            attempts++;

            if(!target(candidate, size - chunk, user_data))
            {
                size -= chunk;
                memcpy(input, candidate, size);
            }
            else
            {
                position += chunk;
            }
        }

        chunk /= 2;
    }

    g_run_info.minimizing = false;

    free(candidate);

    return size;
}

bool fuzz_run_input(const FuzzOptions* options,
                    FuzzInputFunc target,
                    void* user_data,
                    FuzzReport* report)
{
    RunSettings settings;
    FuzzReport local_report;
    FuzzSource source;
    uint8_t* buffer;
    clock_t start = clock();
    uint64_t i;
    bool passed;

    if(report == NULL)
        report = &local_report;

    memset(report, 0, sizeof(FuzzReport));
    settings_init(&settings, options, "input");
    report->seed = settings.options.seed;

    buffer = (uint8_t*)malloc(settings.options.max_input_size);

    if(buffer == NULL)
    {
        logger_log_error("fuzz '%s': cannot allocate the input buffer", settings.options.name);
        report->failed = true;
        return false;
    }

    run_begin(&settings);

    for(i = 0; i < settings.options.iterations; i++)
    {
        size_t size;

        if(i > 0 && budget_exceeded(&settings, start))
            break;

        g_run_info.iteration = i;
        g_run_info.iteration_seed = iteration_seed(&settings, i);

        fuzz_source_init_seed(&source, g_run_info.iteration_seed);
        size = generate_input(&source, &settings, buffer);

        report->iterations++;

        if(!target(buffer, size, user_data))
        {
            report->failed = true;
            report->failing_iteration = i;
            report->failing_seed = g_run_info.iteration_seed;

            if(settings.options.minimize)
                size = minimize_input(target, user_data, buffer, size);

            report->failing_input = (uint8_t*)malloc(size > 0 ? size : 1);

            if(report->failing_input != NULL)
            {
                memcpy(report->failing_input, buffer, size);
                report->failing_input_size = size;
            }

            break;
        }
    }

    report->elapsed_seconds = seconds_since(start);

    if(report->failed && report->failing_input != NULL)
    {
        logger_log_error("fuzz '%s': failing input (%zu bytes%s):",
                         settings.options.name,
                         report->failing_input_size,
                         settings.options.minimize ? ", minimized" : "");
        fuzz_log_input(report->failing_input, report->failing_input_size, 256);
    }

    run_end(&settings, report);

    free(buffer);

    passed = !report->failed;

    if(report == &local_report)
        fuzz_report_release(report);

    return passed;
}

void fuzz_log_input(const uint8_t* data, size_t size, size_t max_bytes)
{
    static const char hex[] = "0123456789abcdef";
    char line[16 * 3 + 2 + 16 + 1];
    size_t shown = size < max_bytes ? size : max_bytes;
    size_t offset;

    for(offset = 0; offset < shown; offset += 16)
    {
        size_t count = shown - offset < 16 ? shown - offset : 16;
        char* p = line;
        size_t i;

        for(i = 0; i < 16; i++)
        {
            if(i < count)
            {
                *p++ = hex[data[offset + i] >> 4];
                *p++ = hex[data[offset + i] & 0xF];
            }
            else
            {
                *p++ = ' ';
                *p++ = ' ';
            }

            *p++ = ' ';
        }

        *p++ = ' ';

        for(i = 0; i < count; i++)
            *p++ = data[offset + i] >= 0x20 && data[offset + i] < 0x7F ? (char)data[offset + i] : '.';

        *p = '\0';

        logger_log_error("  %04zx  %s", offset, line);
    }

    if(shown < size)
        logger_log_error("  ... (%zu more bytes)", size - shown);
}
