/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "replay.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "global.h"

static uint8_t replay_magic_header[] = REPLAY_MAGIC_HEADER;

void replay_init(Replay *rpy) {
	memset(rpy, 0, sizeof(Replay));
	log_debug("Replay at %p initialized for writing", (void*)rpy);
}

ReplayStage* replay_create_stage(Replay *rpy, StageInfo *stage, uint64_t seed, Difficulty diff, Player *plr) {
	ReplayStage *s;

	rpy->stages = (ReplayStage*)realloc(rpy->stages, sizeof(ReplayStage) * (++rpy->numstages));
	s = rpy->stages + rpy->numstages - 1;
	memset(s, 0, sizeof(ReplayStage));

	s->capacity = REPLAY_ALLOC_INITIAL;
	s->events = (ReplayEvent*)malloc(sizeof(ReplayEvent) * s->capacity);

	s->stage = stage->id;
	s->seed	= seed;
	s->diff	= diff;

	s->plr_pos_x = floor(creal(plr->pos));
	s->plr_pos_y = floor(cimag(plr->pos));

	s->plr_points = plr->points;
	s->plr_continues_used = plr->continues_used;
	s->plr_focus = plr->focus;
	s->plr_char	= plr->mode->character->id;
	s->plr_shot	= plr->mode->shot_mode;
	s->plr_lives = plr->lives;
	s->plr_life_fragments = plr->life_fragments;
	s->plr_bombs = plr->bombs;
	s->plr_bomb_fragments = plr->bomb_fragments;
	s->plr_power = plr->power;
	s->plr_inputflags = plr->inputflags;

	log_debug("Created a new stage %p in replay %p", (void*)s, (void*)rpy);
	return s;
}

void replay_stage_sync_player_state(ReplayStage *stg, Player *plr) {
	plr->points = stg->plr_points;
	plr->continues_used = stg->plr_continues_used;
	plr->mode = plrmode_find(stg->plr_char, stg->plr_shot);
	plr->pos = stg->plr_pos_x + I * stg->plr_pos_y;
	plr->focus = stg->plr_focus;
	plr->lives = stg->plr_lives;
	plr->life_fragments = stg->plr_life_fragments;
	plr->bombs = stg->plr_bombs;
	plr->bomb_fragments = stg->plr_bomb_fragments;
	plr->power = stg->plr_power;
	plr->inputflags = stg->plr_inputflags;
}

static void replay_destroy_stage(ReplayStage *stage) {
	free(stage->events);
	memset(stage, 0, sizeof(ReplayStage));
}

void replay_destroy_events(Replay *rpy) {
	if(!rpy) {
		return;
	}

	if(rpy->stages) {
		for(int i = 0; i < rpy->numstages; ++i) {
			ReplayStage *stg = rpy->stages + i;
			free(stg->events);
			stg->events = NULL;
		}
	}
}

void replay_destroy(Replay *rpy) {
	if(!rpy) {
		return;
	}

	if(rpy->stages) {
		for(int i = 0; i < rpy->numstages; ++i) {
			replay_destroy_stage(rpy->stages + i);
		}

		free(rpy->stages);
	}

	free(rpy->playername);

	memset(rpy, 0, sizeof(Replay));
}

void replay_stage_event(ReplayStage *stg, uint32_t frame, uint8_t type, uint16_t value) {
	if(!stg) {
		return;
	}

	ReplayStage *s = stg;
	ReplayEvent *e = s->events + s->numevents++;
	e->frame = frame;
	e->type = type;
	e->value = value;

	if(s->numevents >= s->capacity) {
		log_debug("Replay stage reached its capacity of %d, reallocating", s->capacity);
		s->capacity *= 2;
		s->events = (ReplayEvent*)realloc(s->events, sizeof(ReplayEvent) * s->capacity);
		log_debug("The new capacity is %d", s->capacity);
	}

	if(type == EV_OVER) {
		log_debug("The replay is OVER");
	}
}

static void replay_write_string(SDL_RWops *file, char *str, uint16_t version) {
	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		SDL_WriteU8(file, strlen(str));
	} else {
		SDL_WriteLE16(file, strlen(str));
	}

	SDL_RWwrite(file, str, 1, strlen(str));
}

static bool replay_write_stage_event(ReplayEvent *evt, SDL_RWops *file) {
	SDL_WriteLE32(file, evt->frame);
	SDL_WriteU8(file, evt->type);
	SDL_WriteLE16(file, evt->value);

	return true;
}

static uint32_t replay_calc_stageinfo_checksum(ReplayStage *stg, uint16_t version) {
	uint32_t cs = 0;

	cs += stg->stage;
	cs += stg->seed;
	cs += stg->diff;
	cs += stg->plr_points;
	cs += stg->plr_char;
	cs += stg->plr_shot;
	cs += stg->plr_pos_x;
	cs += stg->plr_pos_y;
	cs += stg->plr_focus;
	cs += stg->plr_power;
	cs += stg->plr_lives;
	cs += stg->plr_life_fragments;
	cs += stg->plr_bombs;
	cs += stg->plr_bomb_fragments;
	cs += stg->plr_inputflags;
	cs += stg->numevents;

	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		cs += stg->plr_continues_used;
		cs += stg->flags;
	}

	log_debug("%08x", cs);
	return cs;
}

static bool replay_write_stage(ReplayStage *stg, SDL_RWops *file, uint16_t version) {
	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		SDL_WriteLE32(file, stg->flags);
	}

	SDL_WriteLE16(file, stg->stage);
	SDL_WriteLE32(file, stg->seed);
	SDL_WriteU8(file, stg->diff);
	SDL_WriteLE32(file, stg->plr_points);

	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		SDL_WriteU8(file, stg->plr_continues_used);
	}

	SDL_WriteU8(file, stg->plr_char);
	SDL_WriteU8(file, stg->plr_shot);
	SDL_WriteLE16(file, stg->plr_pos_x);
	SDL_WriteLE16(file, stg->plr_pos_y);
	SDL_WriteU8(file, stg->plr_focus);
	SDL_WriteLE16(file, stg->plr_power);
	SDL_WriteU8(file, stg->plr_lives);
	SDL_WriteU8(file, stg->plr_life_fragments);
	SDL_WriteU8(file, stg->plr_bombs);
	SDL_WriteU8(file, stg->plr_bomb_fragments);
	SDL_WriteU8(file, stg->plr_inputflags);
	SDL_WriteLE16(file, stg->numevents);
	SDL_WriteLE32(file, 1 + ~replay_calc_stageinfo_checksum(stg, version));

	return true;
}

static void fix_flags(Replay *rpy) {
	rpy->flags |= REPLAY_GFLAG_CLEAR;

	for(int i = 0; i < rpy->numstages; ++i) {
		if(!(rpy->stages[i].flags & REPLAY_SFLAG_CLEAR)) {
			rpy->flags &= ~REPLAY_SFLAG_CLEAR;
			break;
		}
	}
}

bool replay_write(Replay *rpy, SDL_RWops *file, uint16_t version) {
	uint16_t base_version = (version & ~REPLAY_VERSION_COMPRESSION_BIT);
	bool compression = (version & REPLAY_VERSION_COMPRESSION_BIT);
	int i, j;

	SDL_RWwrite(file, replay_magic_header, sizeof(replay_magic_header), 1);
	SDL_WriteLE16(file, version);

	if(base_version >= REPLAY_STRUCT_VERSION_TS102000_REV0) {
		TaiseiVersion v;
		TAISEI_VERSION_GET_CURRENT(&v);

		if(taisei_version_write(file, &v) != TAISEI_VERSION_SIZE) {
			log_warn("Failed to write game version: %s", SDL_GetError());
			return false;
		}
	}

	void *buf;
	SDL_RWops *abuf = NULL;
	SDL_RWops *vfile = file;

	if(compression) {
		abuf = SDL_RWAutoBuffer(&buf, 64);
		vfile = SDL_RWWrapZWriter(abuf, REPLAY_COMPRESSION_CHUNK_SIZE, false);
	}

	replay_write_string(vfile, config_get_str(CONFIG_PLAYERNAME), base_version);
	fix_flags(rpy);

	if(base_version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		SDL_WriteLE32(vfile, rpy->flags);
	}

	SDL_WriteLE16(vfile, rpy->numstages);

	for(i = 0; i < rpy->numstages; ++i) {
		if(!replay_write_stage(rpy->stages + i, vfile, base_version)) {
			if(compression) {
				SDL_RWclose(vfile);
				SDL_RWclose(abuf);
			}

			return false;
		}
	}

	if(compression) {
		SDL_RWclose(vfile);
		SDL_WriteLE32(file, SDL_RWtell(file) + SDL_RWtell(abuf) + 4);
		SDL_RWwrite(file, buf, SDL_RWtell(abuf), 1);
		SDL_RWclose(abuf);
		vfile = SDL_RWWrapZWriter(file, REPLAY_COMPRESSION_CHUNK_SIZE, false);
	}

	for(i = 0; i < rpy->numstages; ++i) {
		ReplayStage *stg = rpy->stages + i;
		for(j = 0; j < stg->numevents; ++j) {
			if(!replay_write_stage_event(stg->events + j, vfile)) {
				if(compression) {
					SDL_RWclose(vfile);
				}

				return false;
			}
		}
	}

	if(compression) {
		SDL_RWclose(vfile);
	}

	// useless byte to simplify the premature EOF check, can be anything
	SDL_WriteU8(file, REPLAY_USELESS_BYTE);

	return true;
}

#ifdef REPLAY_LOAD_DEBUG
#define PRINTPROP(prop,fmt) log_debug(#prop " = %" # fmt " [%"PRIi64" / %"PRIi64"]", prop, SDL_RWtell(file), filesize)
#else
#define PRINTPROP(prop,fmt) (void)(prop)
#endif

#define CHECKPROP(prop,fmt) PRINTPROP(prop,fmt); if(filesize > 0 && SDL_RWtell(file) == filesize) { log_warn("%s: Premature EOF", source); return false; }

static void replay_read_string(SDL_RWops *file, char **ptr, uint16_t version) {
	size_t len;

	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		len = SDL_ReadU8(file);
	} else {
		len = SDL_ReadLE16(file);
	}

	*ptr = malloc(len + 1);
	memset(*ptr, 0, len + 1);

	SDL_RWread(file, *ptr, 1, len);
}

static bool replay_read_header(Replay *rpy, SDL_RWops *file, int64_t filesize, size_t *ofs, const char *source) {
	uint8_t header[sizeof(replay_magic_header)];
	(*ofs) += sizeof(header);

	SDL_RWread(file, header, sizeof(header), 1);

	if(memcmp(header, replay_magic_header, sizeof(header))) {
		log_warn("%s: Incorrect header", source);
		return false;
	}

	CHECKPROP(rpy->version = SDL_ReadLE16(file), u);
	(*ofs) += 2;

	uint16_t base_version = (rpy->version & ~REPLAY_VERSION_COMPRESSION_BIT);
	bool compression = (rpy->version & REPLAY_VERSION_COMPRESSION_BIT);
	bool gamev_assumed = false;

	switch(base_version) {
		case REPLAY_STRUCT_VERSION_TS101000: {
			// legacy format with no versioning, assume v1.1
			TAISEI_VERSION_SET(&rpy->game_version, 1, 1, 0, 0);
			gamev_assumed = true;
			break;
		}

		case REPLAY_STRUCT_VERSION_TS102000_REV0:
		case REPLAY_STRUCT_VERSION_TS102000_REV1:
		{
			if(taisei_version_read(file, &rpy->game_version) != TAISEI_VERSION_SIZE) {
				log_warn("%s: Failed to read game version", source);
				return false;
			}

			(*ofs) += TAISEI_VERSION_SIZE;
			break;
		}

		default: {
			log_warn("%s: Unknown struct version %u", source, base_version);
			return false;
		}
	}

	char *gamev = taisei_version_tostring(&rpy->game_version);
	log_info("Struct version %u (%scompressed), game version %s%s",
		base_version, compression ? "" : "un", gamev, gamev_assumed ? " (assumed)" : "");
	free(gamev);

	if(compression) {
		CHECKPROP(rpy->fileoffset = SDL_ReadLE32(file), u);
		(*ofs) += 4;
	}

	return true;
}

static bool replay_read_meta(Replay *rpy, SDL_RWops *file, int64_t filesize, const char *source) {
	uint16_t version = rpy->version & ~REPLAY_VERSION_COMPRESSION_BIT;

	replay_read_string(file, &rpy->playername, version);
	PRINTPROP(rpy->playername, s);

	if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
		CHECKPROP(rpy->flags = SDL_ReadLE32(file), u);
	}

	CHECKPROP(rpy->numstages = SDL_ReadLE16(file), u);

	if(!rpy->numstages) {
		log_warn("%s: No stages in replay", source);
		return false;
	}

	rpy->stages = malloc(sizeof(ReplayStage) * rpy->numstages);
	memset(rpy->stages, 0, sizeof(ReplayStage) * rpy->numstages);

	for(int i = 0; i < rpy->numstages; ++i) {
		ReplayStage *stg = rpy->stages + i;

		if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
			CHECKPROP(stg->flags = SDL_ReadLE32(file), u);
		}

		CHECKPROP(stg->stage = SDL_ReadLE16(file), u);
		CHECKPROP(stg->seed = SDL_ReadLE32(file), u);
		CHECKPROP(stg->diff = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_points = SDL_ReadLE32(file), u);

		if(version >= REPLAY_STRUCT_VERSION_TS102000_REV1) {
			CHECKPROP(stg->plr_continues_used = SDL_ReadU8(file), u);
		}

		CHECKPROP(stg->plr_char = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_shot = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_pos_x = SDL_ReadLE16(file), u);
		CHECKPROP(stg->plr_pos_y = SDL_ReadLE16(file), u);
		CHECKPROP(stg->plr_focus = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_power = SDL_ReadLE16(file), u);
		CHECKPROP(stg->plr_lives = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_life_fragments = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_bombs = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_bomb_fragments = SDL_ReadU8(file), u);
		CHECKPROP(stg->plr_inputflags = SDL_ReadU8(file), u);
		CHECKPROP(stg->numevents = SDL_ReadLE16(file), u);

		if(replay_calc_stageinfo_checksum(stg, version) + SDL_ReadLE32(file)) {
			log_warn("%s: Stageinfo is corrupt", source);
			return false;
		}
	}

	return true;
}

static bool replay_read_events(Replay *rpy, SDL_RWops *file, int64_t filesize, const char *source) {
	for(int i = 0; i < rpy->numstages; ++i) {
		ReplayStage *stg = rpy->stages + i;

		if(!stg->numevents) {
			log_warn("%s: No events in stage", source);
			return false;
		}

		stg->events = malloc(sizeof(ReplayEvent) * stg->numevents);
		memset(stg->events, 0, sizeof(ReplayEvent) * stg->numevents);

		for(int j = 0; j < stg->numevents; ++j) {
			ReplayEvent *evt = stg->events + j;

			CHECKPROP(evt->frame = SDL_ReadLE32(file), u);
			CHECKPROP(evt->type = SDL_ReadU8(file), u);
			CHECKPROP(evt->value = SDL_ReadLE16(file), u);
		}
	}

	return true;
}

bool replay_read(Replay *rpy, SDL_RWops *file, ReplayReadMode mode, const char *source) {
	int64_t filesize; // must be signed
	SDL_RWops *vfile = file;

	if(!source) {
		source = "<unknown>";
	}

	if(!(mode & REPLAY_READ_ALL) ) {
		log_fatal("%s: Called with invalid read mode %x", source, mode);
	}

	mode &= REPLAY_READ_ALL;
	filesize = SDL_RWsize(file);

	if(filesize < 0) {
		log_warn("%s: SDL_RWsize() failed: %s", source, SDL_GetError());
	}

	if(mode & REPLAY_READ_META) {
		memset(rpy, 0, sizeof(Replay));

		if(filesize > 0 && filesize <= sizeof(replay_magic_header) + 2) {
			log_warn("%s: Replay file is too short (%"PRIi64")", source, filesize);
			return false;
		}

		size_t ofs = 0;

		if(!replay_read_header(rpy, file, filesize, &ofs, source)) {
			return false;
		}

		bool compression = false;

		if(rpy->version & REPLAY_VERSION_COMPRESSION_BIT) {
			if(rpy->fileoffset < SDL_RWtell(file)) {
				log_warn("%s: Invalid offset %"PRIi32"", source, rpy->fileoffset);
				return false;
			}

			vfile = SDL_RWWrapZReader(SDL_RWWrapSegment(file, ofs, rpy->fileoffset, false),
									  REPLAY_COMPRESSION_CHUNK_SIZE, true);
			filesize = -1;
			compression = true;
		}

		if(!replay_read_meta(rpy, vfile, filesize, source)) {
			if(compression) {
				SDL_RWclose(vfile);
			}

			return false;
		}

		if(compression) {
			SDL_RWclose(vfile);
			vfile = file;
		} else {
			rpy->fileoffset = SDL_RWtell(file);
		}
	}

	if(mode & REPLAY_READ_EVENTS) {
		if(!(mode & REPLAY_READ_META)) {
			if(!rpy->fileoffset) {
				log_fatal("%s: Tried to read events before reading metadata", source);
			}

			for(int i = 0; i < rpy->numstages; ++i) {
				if(rpy->stages[i].events) {
					log_warn("%s: BUG: Reading events into a replay that already had events, call replay_destroy_events() if this is intended", source);
					replay_destroy_events(rpy);
					break;
				}
			}

			if(SDL_RWseek(file, rpy->fileoffset, RW_SEEK_SET) < 0) {
				log_warn("%s: SDL_RWseek() failed: %s", source, SDL_GetError());
				return false;
			}
		}

		bool compression = false;

		if(rpy->version & REPLAY_VERSION_COMPRESSION_BIT) {
			vfile = SDL_RWWrapZReader(file, REPLAY_COMPRESSION_CHUNK_SIZE, false);
			filesize = -1;
			compression = true;
		}

		if(!replay_read_events(rpy, vfile, filesize, source)) {
			if(compression) {
				SDL_RWclose(vfile);
			}

			replay_destroy_events(rpy);
			return false;
		}

		if(compression) {
			SDL_RWclose(vfile);
		}

		// useless byte to simplify the premature EOF check, can be anything
		SDL_ReadU8(file);
	}

	return true;
}

#undef CHECKPROP
#undef PRINTPROP

static char* replay_getpath(const char *name, bool ext) {
	return ext ?	strfmt("storage/replays/%s.%s", name, REPLAY_EXTENSION) :
					strfmt("storage/replays/%s", 	name);
}

bool replay_save(Replay *rpy, const char *name) {
	char *p = replay_getpath(name, !strendswith(name, REPLAY_EXTENSION));
	char *sp = vfs_repr(p, true);
	log_info("Saving %s", sp);
	free(sp);

	SDL_RWops *file = vfs_open(p, VFS_MODE_WRITE);
	free(p);

	if(!file) {
		log_warn("VFS error: %s", vfs_get_error());
		return false;
	}

	bool result = replay_write(rpy, file, REPLAY_STRUCT_VERSION_WRITE);
	SDL_RWclose(file);
	return result;
}

static const char* replay_mode_string(ReplayReadMode mode) {
	if((mode & REPLAY_READ_ALL) == REPLAY_READ_ALL) {
		return "full";
	}

	if(mode & REPLAY_READ_META) {
		return "meta";
	}

	if(mode & REPLAY_READ_EVENTS) {
		return "events";
	}

	log_fatal("Bad mode %i", mode);
}

bool replay_load(Replay *rpy, const char *name, ReplayReadMode mode) {
	char *p = replay_getpath(name, !strendswith(name, REPLAY_EXTENSION));
	char *sp = vfs_repr(p, true);
	log_info("Loading %s (%s)", sp, replay_mode_string(mode));

	SDL_RWops *file = vfs_open(p, VFS_MODE_READ);
	free(p);

	if(!file) {
		log_warn("VFS error: %s", vfs_get_error());
		free(sp);
		return false;
	}

	bool result = replay_read(rpy, file, mode, sp);

	if(!result) {
		replay_destroy(rpy);
	}

	free(sp);
	SDL_RWclose(file);
	return result;
}

bool replay_load_syspath(Replay *rpy, const char *path, ReplayReadMode mode) {
	log_info("Loading %s (%s)", path, replay_mode_string(mode));
	SDL_RWops *file;

#ifndef __WINDOWS__
	if(!strcmp(path, "-"))
		file = SDL_RWFromFP(stdin,false);
	else
		file = SDL_RWFromFile(path, "rb");
#else
	file = SDL_RWFromFile(path, "rb");
#endif

	if(!file) {
		log_warn("SDL_RWFromFile() failed: %s", SDL_GetError());
		return false;
	}

	bool result = replay_read(rpy, file, mode, path);

	if(!result) {
		replay_destroy(rpy);
	}

	SDL_RWclose(file);
	return result;
}

void replay_copy(Replay *dst, Replay *src, bool steal_events) {
	int i;

	replay_destroy(dst);
	memcpy(dst, src, sizeof(Replay));

	dst->playername = (char*)malloc(strlen(src->playername)+1);
	strcpy(dst->playername, src->playername);

	dst->stages = (ReplayStage*)malloc(sizeof(ReplayStage) * src->numstages);
	memcpy(dst->stages, src->stages, sizeof(ReplayStage) * src->numstages);

	for(i = 0; i < src->numstages; ++i) {
		ReplayStage *s, *d;
		s = src->stages + i;
		d = dst->stages + i;

		if(steal_events) {
			s->events = NULL;
		} else {
			d->capacity = s->numevents;
			d->events = (ReplayEvent*)malloc(sizeof(ReplayEvent) * d->capacity);
			memcpy(d->events, s->events, sizeof(ReplayEvent) * d->capacity);
		}
	}
}

void replay_stage_check_desync(ReplayStage *stg, int time, uint16_t check, ReplayMode mode) {
	if(!stg || time % (FPS * 5)) {
		return;
	}

	if(mode == REPLAY_PLAY) {
		if(stg->desync_check && stg->desync_check != check) {
			log_warn("Replay desync detected! %u != %u", stg->desync_check, check);
			stg->desynced = true;
		} else {
			log_debug("%u OK", check);
		}
	}
#ifdef REPLAY_WRITE_DESYNC_CHECKS
	else {
		// log_debug("%u", check);
		replay_stage_event(stg, time, EV_CHECK_DESYNC, (int16_t)check);
	}
#endif
}

int replay_find_stage_idx(Replay *rpy, uint8_t stageid) {
	assert(rpy != NULL);
	assert(rpy->stages != NULL);

	for(int i = 0; i < rpy->numstages; ++i) {
		if(rpy->stages[i].stage == stageid) {
			return i;
		}
	}

	log_warn("Stage %x was not found in the replay", stageid);
	return -1;
}

void replay_play(Replay *rpy, int firstidx) {
	if(rpy != &global.replay) {
		replay_copy(&global.replay, rpy, true);
	}

	if(firstidx >= global.replay.numstages || firstidx < 0) {
		log_warn("No stage #%i in the replay", firstidx);
		return;
	}

	global.replaymode = REPLAY_PLAY;

	for(int i = firstidx; i < global.replay.numstages; ++i) {
		ReplayStage *rstg = global.replay_stage = global.replay.stages+i;
		StageInfo *gstg = stage_get(rstg->stage);

		if(!gstg) {
			log_warn("Invalid stage %x in replay at %i skipped.", rstg->stage, i);
			continue;
		}

		global.plr.mode = plrmode_find(rstg->plr_char, rstg->plr_shot);
		stage_loop(gstg);

		if(global.game_over == GAMEOVER_ABORT) {
			break;
		}

		if(global.game_over == GAMEOVER_RESTART) {
			--i;
		}

		global.game_over = 0;
	}

	global.game_over = 0;
	global.replaymode = REPLAY_RECORD;
	replay_destroy(&global.replay);
	global.replay_stage = NULL;
	free_resources(false);
}
