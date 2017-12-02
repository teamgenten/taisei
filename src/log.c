/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include <SDL_bits.h>
#include <SDL_mutex.h>

#ifdef LOG_ENABLE_BACKTRACE
    #include <execinfo.h>
#endif

#include "log.h"
#include "util.h"
#include "list.h"

#ifdef __WINDOWS__
    #define LOG_EOL "\r\n"
#else
    #define LOG_EOL "\n"
#endif

typedef struct Logger {
    struct Logger *next;
    struct Logger *prev;
    SDL_RWops *out;
    unsigned int levels;
} Logger;

static Logger *loggers = NULL;
static unsigned int enabled_log_levels;
static unsigned int backtrace_log_levels;
static SDL_mutex *log_mutex;

// order must much the LogLevel enum after LOG_NONE
static const char *level_prefix_map[] = { "D", "I", "W", "E" };

static const char* level_prefix(LogLevel lvl) {
    int idx = SDL_MostSignificantBitIndex32(lvl);
    assert_nolog(idx >= 0 && idx < sizeof(level_prefix_map) / sizeof(char*));
    return level_prefix_map[idx];
}

static char* format_log_string(LogLevel lvl, const char *funcname, const char *fmt, va_list args, bool is_backtrace) {
    const char *pref = level_prefix(lvl);
    char *msg = vstrfmt(fmt, args);
    char *final = strfmt("%-9d %s: %s(): %s%s", SDL_GetTicks(), pref, funcname, msg, LOG_EOL);
    free(msg);

    // TODO: maybe convert all \n in the message to LOG_EOL

#ifdef DEBUG
    if((backtrace_log_levels & lvl) && !is_backtrace) {
        DebugInfo *debug_info = get_debug_info();
        DebugInfo *debug_meta = get_debug_meta();

        msg = final;
        final = strfmt(
            "%s%s%s"
            "Debug info: %s:%i:%s%s"
            "Debug info set at: %s:%i:%s%s"
            "Note: debug info may not be relevant to this issue%s",
            msg, LOG_EOL, LOG_EOL,
            debug_info->file, debug_info->line, debug_info->func, LOG_EOL,
            debug_meta->file, debug_meta->line, debug_meta->func, LOG_EOL,
            LOG_EOL
        );

        free(msg);
    }
#endif

    return final;
}

noreturn static void log_abort(const char *msg) {
#ifdef LOG_FATAL_MSGBOX
    if(msg) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Taisei error", msg, NULL);
    }
#endif

    // abort() doesn't clean up, but it lets us get a backtrace, which is more useful
    log_shutdown();
    abort();
}

static void log_internal(LogLevel lvl, bool is_backtrace, const char *funcname, const char *fmt, va_list args) {
    assert(fmt[strlen(fmt)-1] != '\n');

    char *str = NULL;
    size_t slen = 0;
    lvl &= enabled_log_levels;

    if(lvl == LOG_NONE) {
        return;
    }

    for(Logger *l = loggers; l; l = l->next) {
        if(l->levels & lvl) {
            if(!str) {
                str = format_log_string(lvl, funcname, fmt, args, is_backtrace);
                slen = strlen(str);
            }

            // log_backtrace locks the mutex by itself, then recursively calls log_internal
            if(is_backtrace) {
                SDL_RWwrite(l->out, str, 1, slen);
            } else {
                SDL_LockMutex(log_mutex);
                SDL_RWwrite(l->out, str, 1, slen);
                SDL_UnlockMutex(log_mutex);
            }
        }
    }

    if(is_backtrace) {
        free(str);
        return;
    }

    if(lvl & backtrace_log_levels) {
        log_backtrace(lvl);
    }

    if(lvl & LOG_FATAL) {
        log_abort(str);
    }

    free(str);
}

static char** get_backtrace(int *num) {
#ifdef LOG_ENABLE_BACKTRACE
    void *ptrs[*num];
    *num = backtrace(ptrs, *num);
    return backtrace_symbols(ptrs, *num);
#else
    char **dummy = malloc(sizeof(char*));
    *num = 1;
    *dummy = "[Backtrace support is not available in this build]";
    return dummy;
#endif
}

void log_backtrace(LogLevel lvl) {
    int num = LOG_BACKTRACE_SIZE;
    char **symbols = get_backtrace(&num);

    SDL_LockMutex(log_mutex);
    _taisei_log(lvl, true, __func__, "*** BACKTRACE ***");

    for(int i = 0; i < num; ++i) {
        _taisei_log(lvl, true, __func__, "> %s", symbols[i]);
    }

    _taisei_log(lvl, true, __func__, "*** END OF BACKTRACE ***");
    SDL_UnlockMutex(log_mutex);

    free(symbols);
}

void _taisei_log(LogLevel lvl, bool is_backtrace, const char *funcname, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(lvl, is_backtrace, funcname, fmt, args);
    va_end(args);
}

noreturn void _taisei_log_fatal(LogLevel lvl, const char *funcname, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(lvl, false, funcname, fmt, args);
    va_end(args);

    // should usually not get here, log_internal will abort earlier if lvl is LOG_FATAL
    // that is unless LOG_FATAL is disabled for some reason
    log_abort(NULL);
}

static void* delete_logger(List **loggers, List *logger, void *arg) {
    Logger *l = (Logger*)logger;

#if HAVE_STDIO_H
    if(l->out->type == SDL_RWOPS_STDFILE) {
        fflush(l->out->hidden.stdio.fp);
    }
#endif

    SDL_RWclose(l->out);
    free(list_unlink(loggers, logger));

    return NULL;
}

void log_init(LogLevel lvls, LogLevel backtrace_lvls) {
    enabled_log_levels = lvls;
    backtrace_log_levels = lvls & backtrace_lvls;
    log_mutex = SDL_CreateMutex();
}

void log_shutdown(void) {
    list_foreach((List**)&loggers, delete_logger, NULL);
    SDL_DestroyMutex(log_mutex);
    log_mutex = NULL;
}

bool log_initialized(void) {
    return log_mutex;
}

void log_add_output(LogLevel levels, SDL_RWops *output) {
    if(!output) {
        return;
    }

    if(!(levels & enabled_log_levels)) {
        SDL_RWclose(output);
        return;
    }

    Logger *l = (Logger*)list_append((List**)&loggers, malloc(sizeof(Logger)));
    l->levels = levels;
    l->out = output;
}

static LogLevel chr2lvl(char c) {
    c = toupper(c);

    for(int i = 0; i < sizeof(level_prefix_map) / sizeof(char*); ++i) {
        if(c == level_prefix_map[i][0]) {
            return (1 << i);
        } else if(c == 'A') {
            return LOG_ALL;
        }
    }

    return 0;
}

LogLevel log_parse_levels(LogLevel lvls, const char *lvlmod) {
    if(!lvlmod) {
        return lvls;
    }

    bool enable = true;

    for(const char *c = lvlmod; *c; ++c) {
        if(*c == '+') {
            enable = true;
        } else if(*c == '-') {
            enable = false;
        } else if(enable) {
            lvls |= chr2lvl(*c);
        } else {
            lvls &= ~chr2lvl(*c);
        }
    }

    return lvls;
}
