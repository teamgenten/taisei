/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#pragma once
#include "taisei.h"

#ifdef DEBUG
	#define PLR_DPS_STATS
#endif

#include "util.h"
#include "enemy.h"
#include "gamepad.h"
#include "aniplayer.h"
#include "resource/animation.h"

enum {
	PLR_MAX_POWER = 400,
	PLR_MAX_LIVES = 9,
	PLR_MAX_BOMBS = 9,

	PLR_MAX_LIFE_FRAGMENTS = 5,
	PLR_MAX_BOMB_FRAGMENTS = 5,

	PLR_START_LIVES = 2,
	PLR_START_BOMBS = 3,

	PLR_SCORE_PER_LIFE_FRAG = 55000,
	PLR_SCORE_PER_BOMB_FRAG = 22000,

	PLR_SPELLPRACTICE_POWER = 200,

	PLR_STGPRACTICE_POWER = 200,
	PLR_STGPRACTICE_LIVES = 4,
	PLR_STGPRACTICE_BOMBS = 4,

	PLR_MIN_BORDER_DIST = 16,
};

typedef enum {
	// do not reorder these or you'll break replays

	INFLAG_UP = 1,
	INFLAG_DOWN = 2,
	INFLAG_LEFT = 4,
	INFLAG_RIGHT = 8,
	INFLAG_FOCUS = 16,
	INFLAG_SHOT = 32,
	INFLAG_SKIP = 64,
} PlrInputFlag;

enum {
	INFLAGS_MOVE = INFLAG_UP | INFLAG_DOWN | INFLAG_LEFT | INFLAG_RIGHT
};

typedef struct {
	complex pos;
	complex deathpos;
	short focus;

	int graze;
	unsigned int points;

	int lives;
	int bombs;
	int life_fragments;
	int bomb_fragments;
	short power;
	int continues_used;

	int continuetime;
	int recovery;
	int deathtime;
	int respawntime;
	int bombcanceltime;
	int bombcanceldelay;

	struct PlayerMode *mode;
	AniPlayer ani;
	Enemy *slaves;

	int inputflags;
	bool gamepadmove;
	complex lastmovedir;
	int axis_ud;
	int axis_lr;

	bool iddqd;

#ifdef PLR_DPS_STATS
	int total_dmg;
#endif
} Player;

// this is used by both player and replay code
enum {
	EV_PRESS,
	EV_RELEASE,
	EV_OVER, // replay-only
	EV_AXIS_LR,
	EV_AXIS_UD,
	EV_CHECK_DESYNC, // replay-only
	EV_FPS, // replay-only
	EV_INFLAGS,
	EV_CONTINUE,
};

// This is called first before we even enter stage_loop.
// It's also called right before syncing player state from a replay stage struct, if a replay is being watched or recorded, before every stage.
// The entire state is reset here, and defaults for story mode are set.
void player_init(Player *plr);

// This is called early in stage_loop, before creating or reading replay stage data.
// State that is not supposed to be preserved between stages is reset here, and any plrmode-specific resources are preloaded.
void player_stage_pre_init(Player *plr);

// This is called right before the stage's begin proc. After that, the actual game loop starts.
void player_stage_post_init(Player *plr);

// Yes, that's 3 different initialization functions right here.

void player_draw(Player*);
void player_logic(Player*);
bool player_should_shoot(Player *plr, bool extra);

bool player_set_power(Player *plr, short npow);

void player_move(Player*, complex delta);

void player_realdeath(Player*);
void player_death(Player*);
void player_graze(Player*, complex, int);

void player_event(Player *plr, uint8_t type, uint16_t value, bool *out_useful, bool *out_cheat);
bool player_event_with_replay(Player *plr, uint8_t type, uint16_t value);
void player_applymovement(Player* plr);
void player_fix_input(Player *plr);

void player_add_life_fragments(Player *plr, int frags);
void player_add_bomb_fragments(Player *plr, int frags);
void player_add_lives(Player *plr, int lives);
void player_add_bombs(Player *plr, int bombs);
void player_add_points(Player *plr, unsigned int points);

void player_cancel_bomb(Player *plr, int delay);
int player_get_bomb_progress(Player *plr, double *out_speed);
int player_run_bomb_logic(Player *plr, void *ent, complex *argptr, int (*callback)(void *ent, int t, double speed));

void player_preload(void);
