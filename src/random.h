/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#pragma once
#include "taisei.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct RandomState {
	uint32_t w;
	uint32_t z;
    bool locked;
} RandomState;

int tsrand_test(void);

void tsrand_init(RandomState *rnd, uint32_t seed);
void tsrand_switch(RandomState *rnd);
void tsrand_seed_p(RandomState *rnd, uint32_t seed);
uint32_t tsrand_p(RandomState *rnd);

void tsrand_seed(uint32_t seed);
uint32_t tsrand(void);

void tsrand_lock(RandomState *rnd);
void tsrand_unlock(RandomState *rnd);

double frand(void);
double nfrand(void);

void __tsrand_fill_p(RandomState *rnd, int amount, const char *file, unsigned int line);
void __tsrand_fill(int amount, const char *file, unsigned int line);
uint32_t __tsrand_a(int idx, const char *file, unsigned int line);
double __afrand(int idx, const char *file, unsigned int line);
double __anfrand(int idx, const char *file, unsigned int line);

#define tsrand_fill_p(rnd,amount) __tsrand_fill_p(rnd, amount, __FILE__, __LINE__)
#define tsrand_fill(amount) __tsrand_fill(amount, __FILE__, __LINE__)
#define tsrand_a(idx) __tsrand_a(idx, __FILE__, __LINE__)
#define afrand(idx) __afrand(idx, __FILE__, __LINE__)
#define anfrand(idx) __anfrand(idx, __FILE__, __LINE__)

#define TSRAND_MAX INT32_MAX

#define TSRAND_ARRAY_LIMIT 64
#define srand USE_tsrand_seed_INSTEAD_OF_srand
#define rand USE_tsrand_INSTEAD_OF_rand

/*
 *	These have to be distinct 16-bit constants k, for which both k*2^16-1 and k*2^15-1 are prime numbers
 */

#define TSRAND_W_COEFF 30963
#define TSRAND_W_SEED_COEFF 19164
#define TSRAND_Z_COEFF 29379
#define TSRAND_Z_SEED_COEFF 31083
