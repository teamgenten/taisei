/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include <zlib.h>

#include "progress.h"
#include "stage.h"
#include "version.h"

/*

	This module implements a persistent storage of a player's game progress, such as unlocked stages, high-scores etc.

	Basic outline of the progress file structure (little-endian encoding):
		[uint64 magic] [uint32 checksum] [array of commands]

	Where a command is:
		[uint8 command] [uint16 size] [array of {size} bytes, contents depend on {command}]

	This way we can easily extend the structure to store whatever we want, without breaking compatibility.
	This is forwards-compatible as well: any unrecognized command can be skipped when reading, since its size is known.

	The checksum is calculated on the whole array of commands (see progress_checksum() for implementation).
	If the checksum is incorrect, the whole progress file is deemed invalid and ignored.

	All commands are optional and the array may be empty. In that case, the checksum may be omitted as well.

	Currently implemented commands (see also the ProgfileCommand enum in progress.h):

		- PCMD_UNLOCK_STAGES:
			Unlocks one or more stages. Only works for stages with fixed difficulty.

		- PCMD_UNLOCK_STAGES_WITH_DIFFICULTY:
			Unlocks one or more stages, each on a specific difficulty

		- PCMD_HISCORE
			Sets the "Hi-Score" (highest score ever attained in one game session)

		- PCMD_STAGE_PLAYINFO
			Sets the times played and times cleared counters for a list of stage/difficulty combinations

		- PCMD_ENDINGS
			Sets the the number of times an ending was achieved for a list of endings

		- PCMD_GAME_SETTINGS
			Sets the last picked difficulty, character and shot mode

		- PCMD_GAME_VERSION
			Sets the game version this file was last written with
*/

/*

	Now in case you wonder why I decided to do it this way instead of stuffing everything in the config file, here are a couple of reasons:

		- The config module, as of the time of writting, is messy enough and probably needs refactoring.

		- I don't want to mix user preferences with things that are not supposed to be directly edited.

		- I don't want someone to accidentally lose progress after deleting the config file because they wanted to restore the default settings.

		- I want to discourage players from editing this file should they find it. This is why it's not textual and has a checksum.
		  Of course that doesn't actually prevent one from cheating it, but if you know how to do that, you might as well grab the game's source code and make it do whatever you please.
		  As long as this can stop a l33th4XxXxX0r1998 armed with notepad.exe or a hex editor, I'm happy.

*/

GlobalProgress progress;

static uint8_t progress_magic_bytes[] = {
	0x00, 0x67, 0x74, 0x66, 0x6f, 0xe3, 0x83, 0x84
};

static uint32_t progress_checksum(uint8_t *buf, size_t num) {
	return crc32(0xB16B00B5, buf, num);
}

typedef struct UnknownCmd {
	struct UnknownCmd *next;
	struct UnknownCmd *prev;
	uint8_t cmd;
	uint16_t size;
	uint8_t *data;
} UnknownCmd;

static bool progress_read_verify_cmd_size(SDL_RWops *vfile, uint8_t cmd, uint16_t cmdsize, uint16_t expectsize) {
	if(cmdsize == expectsize) {
		return true;
	}

	log_warn("Command %x with bad size %u ignored", cmd, cmdsize);

	if(SDL_RWseek(vfile, cmdsize, RW_SEEK_CUR) < 0) {
		log_warn("SDL_RWseek() failed: %s", SDL_GetError());
	}

	return false;
}

static void progress_read(SDL_RWops *file) {
	int64_t filesize = SDL_RWsize(file);

	if(filesize < 0) {
		log_warn("SDL_RWsize() failed: %s", SDL_GetError());
		return;
	}

	if(filesize > PROGRESS_MAXFILESIZE) {
		log_warn("Progress file is huge (%"PRIi64" bytes, %i max)", filesize, PROGRESS_MAXFILESIZE);
		return;
	}

	for(int i = 0; i < sizeof(progress_magic_bytes); ++i) {
		if(SDL_ReadU8(file) != progress_magic_bytes[i]) {
			log_warn("Invalid header");
			return;
		}
	}

	if(filesize - SDL_RWtell(file) < 4) {
		return;
	}

	uint32_t checksum_fromfile;
	// no byteswapping here
	SDL_RWread(file, &checksum_fromfile, 4, 1);

	size_t bufsize = filesize - sizeof(progress_magic_bytes) - 4;
	uint8_t *buf = malloc(bufsize);

	if(!SDL_RWread(file, buf, bufsize, 1)) {
		log_warn("SDL_RWread() failed: %s", SDL_GetError());
		free(buf);
		return;
	}

	SDL_RWops *vfile = SDL_RWFromMem(buf, bufsize);
	uint32_t checksum = progress_checksum(buf, bufsize);

	if(checksum != checksum_fromfile) {
		log_warn("Bad checksum: %x != %x", checksum, checksum_fromfile);
		SDL_RWclose(vfile);
		free(buf);
		return;
	}

	TaiseiVersion version_info = { 0 };

	while(SDL_RWtell(vfile) < bufsize) {
		ProgfileCommand cmd = (int8_t)SDL_ReadU8(vfile);
		uint16_t cur = 0;
		uint16_t cmdsize = SDL_ReadLE16(vfile);

		switch(cmd) {
			case PCMD_UNLOCK_STAGES:
				while(cur < cmdsize) {
					StageProgress *p = stage_get_progress(SDL_ReadLE16(vfile), D_Any, true);
					if(p) {
						p->unlocked = true;
					}
					cur += 2;
				}
				break;

			case PCMD_UNLOCK_STAGES_WITH_DIFFICULTY:
				while(cur < cmdsize) {
					uint16_t stg = SDL_ReadLE16(vfile);
					uint8_t dflags = SDL_ReadU8(vfile);
					StageInfo *info = stage_get(stg);

					for(unsigned int diff = D_Easy; diff <= D_Lunatic; ++diff) {
						if(dflags & (unsigned int)pow(2, diff - D_Easy)) {
							StageProgress *p = stage_get_progress_from_info(info, diff, true);
							if(p) {
								p->unlocked = true;
							}
						}
					}

					cur += 3;
				}
				break;

			case PCMD_HISCORE:
				progress.hiscore = SDL_ReadLE32(vfile);
				break;

			case PCMD_STAGE_PLAYINFO:
				while(cur < cmdsize) {
					uint16_t stg = SDL_ReadLE16(vfile); cur += sizeof(uint16_t);
					Difficulty diff = SDL_ReadU8(vfile); cur += sizeof(uint8_t);
					StageProgress *p = stage_get_progress(stg, diff, true);

					// assert(p != NULL);

					uint32_t np = SDL_ReadLE32(vfile); cur += sizeof(uint32_t);
					uint32_t nc = SDL_ReadLE32(vfile); cur += sizeof(uint32_t);

					if(p) {
						p->num_played = np;
						p->num_cleared = nc;
					} else {
						log_warn("Invalid stage %x ignored", stg);
					}
				}
				break;

			case PCMD_ENDINGS:
				while(cur < cmdsize) {
					uint8_t ending = SDL_ReadU8(vfile); cur += sizeof(uint8_t);
					uint32_t num_achieved = SDL_ReadLE32(vfile); cur += sizeof(uint32_t);

					if(ending < NUM_ENDINGS) {
						progress.achieved_endings[ending] = num_achieved;
					} else {
						log_warn("Invalid ending %u ignored", ending);
					}
				}
				break;

			case PCMD_GAME_SETTINGS:
				if(progress_read_verify_cmd_size(vfile, cmd, cmdsize, sizeof(uint8_t) * 3)) {
					progress.game_settings.difficulty = SDL_ReadU8(vfile);
					progress.game_settings.character = SDL_ReadU8(vfile);
					progress.game_settings.shotmode = SDL_ReadU8(vfile);
				}
				break;

			case PCMD_GAME_VERSION:
				if(progress_read_verify_cmd_size(vfile, cmd, cmdsize, TAISEI_VERSION_SIZE)) {
					if(version_info.major > 0) {
						log_warn("Multiple version information entries in progress file");
					}

					size_t read __attribute__((unused)) = taisei_version_read(vfile, &version_info);
					assert(read == TAISEI_VERSION_SIZE);
					char *vstr = taisei_version_tostring(&version_info);
					log_info("Progress file from Taisei v%s", vstr);
					free(vstr);
				}
				break;

			default:
				log_warn("Unknown command %x (%u bytes). Will preserve as-is and not interpret", cmd, cmdsize);

				UnknownCmd *c = (UnknownCmd*)list_append((List**)&progress.unknown, malloc(sizeof(UnknownCmd)));
				c->cmd = cmd;
				c->size = cmdsize;
				c->data = malloc(cmdsize);
				SDL_RWread(vfile, c->data, c->size, 1);

				break;
		}
	}

	if(version_info.major == 0) {
		log_warn("No version information in progress file, it's probably just old (Taisei v1.1, or an early pre-v1.2 development build)");
	} else {
		TaiseiVersion current_version;
		TAISEI_VERSION_GET_CURRENT(&current_version);
		int cmp = taisei_version_compare(&current_version, &version_info, VCMP_TWEAK);

		if(cmp != 0) {
			char *v_prog = taisei_version_tostring(&version_info);
			char *v_game = taisei_version_tostring(&current_version);

			if(cmp > 0) {
				log_info("Progress file will be automatically upgraded from v%s to v%s upon saving", v_prog, v_game);
			} else {
				log_warn("Progress file will be automatically downgraded from v%s to v%s upon saving", v_prog, v_game);
			}

			free(v_prog);
			free(v_game);
		}
	}

	free(buf);
	SDL_RWclose(vfile);
}

typedef void (*cmd_preparefunc_t)(size_t*, void**);
typedef void (*cmd_writefunc_t)(SDL_RWops*, void**);

typedef struct cmd_writer_t {
	cmd_preparefunc_t prepare;
	cmd_writefunc_t write;
	void *data;
} cmd_writer_t;

#define CMD_HEADER_SIZE 3

//
//	PCMD_UNLOCK_STAGES
//

static void progress_prepare_cmd_unlock_stages(size_t *bufsize, void **arg) {
	int num_unlocked = 0;

	for(StageInfo *stg = stages; stg->procs; ++stg) {
		StageProgress *p = stage_get_progress_from_info(stg, D_Any, false);
		if(p && p->unlocked) {
			++num_unlocked;
		}
	}

	if(num_unlocked) {
		*bufsize += CMD_HEADER_SIZE;
		*bufsize += num_unlocked * 2;
	}

	*arg = (void*)(intptr_t)num_unlocked;
}

static void progress_write_cmd_unlock_stages(SDL_RWops *vfile, void **arg) {
	int num_unlocked = (intptr_t)*arg;

	if(!num_unlocked) {
		return;
	}

	SDL_WriteU8(vfile, PCMD_UNLOCK_STAGES);
	SDL_WriteLE16(vfile, num_unlocked * 2);

	for(StageInfo *stg = stages; stg->procs; ++stg) {
		StageProgress *p = stage_get_progress_from_info(stg, D_Any, false);
		if(p && p->unlocked) {
			SDL_WriteLE16(vfile, stg->id);
		}
	}
}

//
//	PCMD_UNLOCK_STAGES_WITH_DIFFICULTY
//

static void progress_prepare_cmd_unlock_stages_with_difficulties(size_t *bufsize, void **arg) {
	int num_unlocked = 0;

	for(StageInfo *stg = stages; stg->procs; ++stg) {
		if(stg->difficulty)
			continue;

		bool unlocked = false;

		for(Difficulty diff = D_Easy; diff <= D_Lunatic; ++diff) {
			StageProgress *p = stage_get_progress_from_info(stg, diff, false);
			if(p && p->unlocked) {
				unlocked = true;
			}
		}

		if(unlocked) {
			++num_unlocked;
		}
	}

	if(num_unlocked) {
		*bufsize += CMD_HEADER_SIZE;
		*bufsize += num_unlocked * 3;
	}

	*arg = (void*)(intptr_t)num_unlocked;
}

static void progress_write_cmd_unlock_stages_with_difficulties(SDL_RWops *vfile, void **arg) {
	int num_unlocked = (intptr_t)*arg;

	if(!num_unlocked) {
		return;
	}

	SDL_WriteU8(vfile, PCMD_UNLOCK_STAGES_WITH_DIFFICULTY);
	SDL_WriteLE16(vfile, num_unlocked * 3);

	for(StageInfo *stg = stages; stg->procs; ++stg) {
		if(stg->difficulty)
			continue;

		uint8_t dflags = 0;

		for(Difficulty diff = D_Easy; diff <= D_Lunatic; ++diff) {
			StageProgress *p = stage_get_progress_from_info(stg, diff, false);
			if(p && p->unlocked) {
				dflags |= (unsigned int)pow(2, diff - D_Easy);
			}
		}

		if(dflags) {
			SDL_WriteLE16(vfile, stg->id);
			SDL_WriteU8(vfile, dflags);
		}
	}
}

//
//	PCMD_HISCORE
//

static void progress_prepare_cmd_hiscore(size_t *bufsize, void **arg) {
	*bufsize += CMD_HEADER_SIZE + sizeof(uint32_t);
}

static void progress_write_cmd_hiscore(SDL_RWops *vfile, void **arg) {
	SDL_WriteU8(vfile, PCMD_HISCORE);
	SDL_WriteLE16(vfile, sizeof(uint32_t));
	SDL_WriteLE32(vfile, progress.hiscore);
}

//
//	PCMD_STAGE_PLAYINFO
//

struct cmd_stage_playinfo_data_elem {
	struct cmd_stage_playinfo_data_elem *next;
	struct cmd_stage_playinfo_data_elem *prev;
	uint16_t stage;
	uint8_t diff;
	uint32_t num_played;
	uint32_t num_cleared;
};

struct cmd_stage_playinfo_data {
	size_t size;
	struct cmd_stage_playinfo_data_elem *elems;
};

static void progress_prepare_cmd_stage_playinfo(size_t *bufsize, void **arg) {
	struct cmd_stage_playinfo_data *data = malloc(sizeof(struct cmd_stage_playinfo_data));
	memset(data, 0, sizeof(struct cmd_stage_playinfo_data));

	for(StageInfo *stg = stages; stg->procs; ++stg) {
		Difficulty d_first, d_last;

		if(stg->difficulty == D_Any) {
			d_first = D_Easy;
			d_last = D_Lunatic;
		} else {
			d_first = d_last = D_Any;
		}

		for(Difficulty d = d_first; d <= d_last; ++d) {
			StageProgress *p = stage_get_progress_from_info(stg, d, false);

			if(p && (p->num_played || p->num_cleared)) {
				struct cmd_stage_playinfo_data_elem *e = (
					(struct cmd_stage_playinfo_data_elem*)
					list_push(
						(List**)&data->elems,
						malloc(sizeof(struct cmd_stage_playinfo_data_elem))
					)
				);

				e->stage = stg->id;					data->size += sizeof(uint16_t);
				e->diff = d;						data->size += sizeof(uint8_t);
				e->num_played = p->num_played;		data->size += sizeof(uint32_t);
				e->num_cleared = p->num_cleared;	data->size += sizeof(uint32_t);
			}
		}
	}

	*arg = data;

	if(data->size) {
		*bufsize += CMD_HEADER_SIZE + data->size;
	}
}

static void progress_write_cmd_stage_playinfo(SDL_RWops *vfile, void **arg) {
	struct cmd_stage_playinfo_data *data = *arg;

	if(!data->size) {
		goto cleanup;
	}

	SDL_WriteU8(vfile, PCMD_STAGE_PLAYINFO);
	SDL_WriteLE16(vfile, data->size);

	for(struct cmd_stage_playinfo_data_elem *e = data->elems; e; e = e->next) {
		SDL_WriteLE16(vfile, e->stage);
		SDL_WriteU8(vfile, e->diff);
		SDL_WriteLE32(vfile, e->num_played);
		SDL_WriteLE32(vfile, e->num_cleared);
	}

cleanup:
	list_free_all((List**)&data->elems);
	free(data);
}

//
//	PCMD_ENDINGS
//

static void progress_prepare_cmd_endings(size_t *bufsize, void **arg) {
	int n = 0;
	*arg = 0;

	for(int i = 0; i < NUM_ENDINGS; ++i) {
		if(progress.achieved_endings[i]) {
			++n;
		}
	}

	if(n) {
		uint16_t sz = n * (sizeof(uint8_t) + sizeof(uint32_t));
		*arg = (void*)(uintptr_t)sz;
		*bufsize += CMD_HEADER_SIZE + sz;
	}
}

static void progress_write_cmd_endings(SDL_RWops *vfile, void **arg) {
	uint16_t sz = (uintptr_t)*arg;

	if(!sz) {
		return;
	}

	SDL_WriteU8(vfile, PCMD_ENDINGS);
	SDL_WriteLE16(vfile, sz);

	for(int i = 0; i < NUM_ENDINGS; ++i) {
		if(progress.achieved_endings[i]) {
			SDL_WriteU8(vfile, i);
			SDL_WriteLE32(vfile, progress.achieved_endings[i]);
		}
	}
}

//
//	PCMD_GAME_SETTINGS
//

static void progress_prepare_cmd_game_settings(size_t *bufsize, void **arg) {
	*bufsize += CMD_HEADER_SIZE + sizeof(uint8_t) * 3;
}

static void progress_write_cmd_game_settings(SDL_RWops *vfile, void **arg) {
	SDL_WriteU8(vfile, PCMD_GAME_SETTINGS);
	SDL_WriteLE16(vfile, sizeof(uint8_t) * 3);
	SDL_WriteU8(vfile, progress.game_settings.difficulty);
	SDL_WriteU8(vfile, progress.game_settings.character);
	SDL_WriteU8(vfile, progress.game_settings.shotmode);
}

//
//	PCMD_GAME_VERSION
//

static void progress_prepare_cmd_game_version(size_t *bufsize, void **arg) {
	*bufsize += CMD_HEADER_SIZE + TAISEI_VERSION_SIZE;
}

static void progress_write_cmd_game_version(SDL_RWops *vfile, void **arg) {
	SDL_WriteU8(vfile, PCMD_GAME_VERSION);
	SDL_WriteLE16(vfile, TAISEI_VERSION_SIZE);

	TaiseiVersion v;
	TAISEI_VERSION_GET_CURRENT(&v);

	// the buffer consistency check should fail later if there are any errors here
	taisei_version_write(vfile, &v);
}

//
//	Copy unhandled commands from the original file
//

static void progress_prepare_cmd_unknown(size_t *bufsize, void **arg) {
	for(UnknownCmd *c = progress.unknown; c; c = c->next) {
		*bufsize += CMD_HEADER_SIZE + c->size;
	}
}

static void progress_write_cmd_unknown(SDL_RWops *vfile, void **arg) {
	for(UnknownCmd *c = progress.unknown; c; c = c->next) {
		SDL_WriteU8(vfile, c->cmd);
		SDL_WriteLE16(vfile, c->size);
		SDL_RWwrite(vfile, c->data, c->size, 1);
	}
}

//
//	Test
//

__attribute__((unused)) static void progress_prepare_cmd_test(size_t *bufsize, void **arg) {
	*bufsize += CMD_HEADER_SIZE + sizeof(uint32_t);
}

__attribute__((unused)) static void progress_write_cmd_test(SDL_RWops *vfile, void **arg) {
	SDL_WriteU8(vfile, 0x7f);
	SDL_WriteLE16(vfile, sizeof(uint32_t));
	SDL_WriteLE32(vfile, 0xdeadbeef);
}

static void progress_write(SDL_RWops *file) {
	size_t bufsize = 0;
	SDL_RWwrite(file, progress_magic_bytes, 1, sizeof(progress_magic_bytes));

	cmd_writer_t cmdtable[] = {
		{progress_prepare_cmd_game_version, progress_write_cmd_game_version, NULL},
		{progress_prepare_cmd_unlock_stages, progress_write_cmd_unlock_stages, NULL},
		{progress_prepare_cmd_unlock_stages_with_difficulties, progress_write_cmd_unlock_stages_with_difficulties, NULL},
		{progress_prepare_cmd_hiscore, progress_write_cmd_hiscore, NULL},
		{progress_prepare_cmd_stage_playinfo, progress_write_cmd_stage_playinfo, NULL},
		{progress_prepare_cmd_endings, progress_write_cmd_endings, NULL},
		{progress_prepare_cmd_game_settings, progress_write_cmd_game_settings, NULL},
		// {progress_prepare_cmd_test, progress_write_cmd_test, NULL},
		{progress_prepare_cmd_unknown, progress_write_cmd_unknown, NULL},
		{NULL}
	};

	for(cmd_writer_t *w = cmdtable; w->prepare; ++w) {
		size_t oldsize __attribute__((unused)) = bufsize;
		w->prepare(&bufsize, &w->data);
		log_debug("prepare %i: %i", (int)(w - cmdtable), (int)(bufsize - oldsize));
	}

	if(!bufsize) {
		return;
	}

	uint8_t *buf = malloc(bufsize);
	memset(buf, 0x7f, bufsize);
	SDL_RWops *vfile = SDL_RWFromMem(buf, bufsize);

	for(cmd_writer_t *w = cmdtable; w->prepare; ++w) {
		size_t oldpos __attribute__((unused)) = SDL_RWtell(vfile);
		w->write(vfile, &w->data);
		log_debug("write %i: %i", (int)(w - cmdtable), (int)(SDL_RWtell(vfile) - oldpos));
	}

	if(SDL_RWtell(vfile) != bufsize) {
		free(buf);
		log_fatal("Buffer is inconsistent");
		return;
	}

	uint32_t cs = progress_checksum(buf, bufsize);
	// no byteswapping here
	SDL_RWwrite(file, &cs, 4, 1);

	if(!SDL_RWwrite(file, buf, bufsize, 1)) {
		log_fatal("SDL_RWwrite() failed: %s", SDL_GetError());
		return;
	}

	free(buf);
	SDL_RWclose(vfile);
}

#ifdef PROGRESS_UNLOCK_ALL
static void progress_unlock_all(void) {
	StageInfo *stg;

	for(stg = stages; stg->procs; ++stg) {
		for(Difficulty diff = D_Any; diff <= D_Lunatic; ++diff) {
			StageProgress *p = stage_get_progress_from_info(stg, diff, true);
			if(p) {
				p->unlocked = true;
			}
		}
	}
}
#endif

void progress_load(void) {
	memset(&progress, 0, sizeof(GlobalProgress));

#ifdef PROGRESS_UNLOCK_ALL
	progress_unlock_all();
	progress_save();
#endif

	SDL_RWops *file = vfs_open(PROGRESS_FILE, VFS_MODE_READ);

	if(!file) {
		log_warn("Couldn't open the progress file: %s", vfs_get_error());
		return;
	}

	progress_read(file);
	SDL_RWclose(file);
}

void progress_save(void) {
	SDL_RWops *file = vfs_open(PROGRESS_FILE, VFS_MODE_WRITE);

	if(!file) {
		log_warn("Couldn't open the progress file: %s", vfs_get_error());
		return;
	}

	progress_write(file);
	SDL_RWclose(file);
}

static void* delete_unknown_cmd(List **dest, List *elem, void *arg) {
	UnknownCmd *cmd = (UnknownCmd*)elem;
	free(cmd->data);
	free(list_unlink(dest, elem));
	return NULL;
}

void progress_unload(void) {
	list_foreach((void**)&progress.unknown, delete_unknown_cmd, NULL);
}
