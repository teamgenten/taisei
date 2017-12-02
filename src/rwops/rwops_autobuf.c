/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "rwops_autobuf.h"
#include "rwops_segment.h"

#define BUFFER(rw) ((Buffer*)((rw)->hidden.unknown.data1))

typedef struct Buffer {
    SDL_RWops *memrw;
    void *data;
    void **ptr;
    size_t size;
} Buffer;

static void auto_realloc(Buffer *b, size_t newsize) {
    size_t pos = SDL_RWtell(b->memrw);
    SDL_RWclose(b->memrw);

    b->size = newsize;
    b->data = realloc(b->data, b->size);
    b->memrw = SDL_RWFromMem(b->data, b->size);

    if(b->ptr) {
        *b->ptr = b->data;
    }

    if(pos > 0) {
        SDL_RWseek(b->memrw, pos, RW_SEEK_SET);
    }
}

static int auto_close(SDL_RWops *rw) {
    if(rw) {
        Buffer *b = BUFFER(rw);
        SDL_RWclose(b->memrw);
        free(b->data);
        free(b);
        SDL_FreeRW(rw);
    }

    return 0;
}

static int64_t auto_seek(SDL_RWops *rw, int64_t offset, int whence) {
    return SDL_RWseek(BUFFER(rw)->memrw, offset, whence);
}

static int64_t auto_size(SDL_RWops *rw) {
    // return SDL_RWsize(BUFFER(rw)->memrw);
    return BUFFER(rw)->size;
}

static size_t auto_read(SDL_RWops *rw, void *ptr, size_t size, size_t maxnum) {
    return SDL_RWread(BUFFER(rw)->memrw, ptr, size, maxnum);
}

static size_t auto_write(SDL_RWops *rw, const void *ptr, size_t size, size_t maxnum) {
    Buffer *b = BUFFER(rw);
    size_t newsize = b->size;

    while(size * maxnum > newsize - SDL_RWtell(rw)) {
        newsize *= 2;
    }

    if(newsize > b->size) {
        auto_realloc(b, newsize);
    }

    return SDL_RWwrite(BUFFER(rw)->memrw, ptr, size, maxnum);
}

SDL_RWops* SDL_RWAutoBuffer(void **ptr, size_t initsize) {
    SDL_RWops *rw = SDL_AllocRW();

    if(!rw) {
        return NULL;
    }

    rw->type = SDL_RWOPS_UNKNOWN;
    rw->seek = auto_seek;
    rw->size = auto_size;
    rw->read = auto_read;
    rw->write = auto_write;
    rw->close = auto_close;

    Buffer *b = calloc(1, sizeof(Buffer));

    b->size = initsize;
    b->data = malloc(b->size);
    b->ptr = ptr;
    b->memrw = SDL_RWFromMem(b->data, b->size);

    if(ptr) {
        *ptr = b->data;
    }

    rw->hidden.unknown.data1 = b;

    return rw;
}

SDL_RWops* SDL_RWCopyToBuffer(SDL_RWops *src) {
    uint8_t buf[4096] = {0};
    ssize_t len;
    SDL_RWops *abufrw = SDL_RWAutoBuffer(NULL, 4096);

    while((len = SDL_RWread(src, buf, 1, sizeof(buf))) > 0) {
        SDL_RWwrite(abufrw, buf, 1, len);
    }

    size_t datasize = SDL_RWtell(abufrw);
    SDL_RWseek(abufrw, 0, RW_SEEK_SET);
    abufrw = SDL_RWWrapSegment(abufrw, 0, datasize, true);

    return abufrw;
}
