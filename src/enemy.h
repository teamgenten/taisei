/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#pragma once
#include "taisei.h"

#include "util.h"
#include "projectile.h"

#ifdef DEBUG
	#define ENEMY_DEBUG
#endif

typedef struct Enemy Enemy;
typedef int (*EnemyLogicRule)(struct Enemy*, int t);
typedef void (*EnemyVisualRule)(struct Enemy*, int t, bool render);

enum {
	ENEMY_IMMUNE = -9000,
	ENEMY_BOMB = -9001
};

struct Enemy {
	Enemy *next;
	Enemy *prev;

	complex pos;
	complex pos0;

	long birthtime;

	int dir;
	bool moving;

	EnemyLogicRule logic_rule;
	EnemyVisualRule visual_rule;

	int hp;

	complex args[RULE_ARGC];
	float alpha;

#ifdef ENEMY_DEBUG
	DebugInfo debug;
#endif
};

#define create_enemy4c(p,h,d,l,a1,a2,a3,a4) create_enemy_p(&global.enemies,p,h,d,l,a1,a2,a3,a4)
#define create_enemy3c(p,h,d,l,a1,a2,a3) create_enemy_p(&global.enemies,p,h,d,l,a1,a2,a3,0)
#define create_enemy2c(p,h,d,l,a1,a2) create_enemy_p(&global.enemies,p,h,d,l,a1,a2,0,0)
#define create_enemy1c(p,h,d,l,a1) create_enemy_p(&global.enemies,p,h,d,l,a1,0,0,0)

Enemy *create_enemy_p(Enemy **enemies, complex pos, int hp, EnemyVisualRule draw_rule, EnemyLogicRule logic_rule,
				    complex a1, complex a2, complex a3, complex a4);
#ifdef ENEMY_DEBUG
	Enemy* _enemy_attach_dbginfo(Enemy *p, DebugInfo *dbg);
	#define create_enemy_p(...) _enemy_attach_dbginfo(create_enemy_p(__VA_ARGS__), _DEBUG_INFO_PTR_)
#endif

void delete_enemy(Enemy **enemies, Enemy* enemy);
void draw_enemies(Enemy *enemies);
void delete_enemies(Enemy **enemies);

void process_enemies(Enemy **enemies);

void killall(Enemy *enemies);

void Fairy(Enemy*, int t, bool render);
void Swirl(Enemy*, int t, bool render);
void BigFairy(Enemy*, int t, bool render);

int enemy_flare(Projectile *p, int t);
void EnemyFlareShrink(Projectile *p, int t);

void enemies_preload(void);
