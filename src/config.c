/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include <string.h>

#include "config.h"
#include "global.h"
#include "version.h"

static bool config_initialized = false;

CONFIGDEFS_EXPORT ConfigEntry configdefs[] = {
	#define CONFIGDEF(type,entryname,default,ufield) {type, entryname, {.ufield = default}, NULL},
	#define CONFIGDEF_KEYBINDING(id,entryname,default) CONFIGDEF(CONFIG_TYPE_KEYBINDING, entryname, default, i)
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) CONFIGDEF(CONFIG_TYPE_GPKEYBINDING, entryname, default, i)
	#define CONFIGDEF_INT(id,entryname,default) CONFIGDEF(CONFIG_TYPE_INT, entryname, default, i)
	#define CONFIGDEF_FLOAT(id,entryname,default) CONFIGDEF(CONFIG_TYPE_FLOAT, entryname, default, f)
	#define CONFIGDEF_STRING(id,entryname,default) CONFIGDEF(CONFIG_TYPE_STRING, entryname, NULL, s)

	CONFIGDEFS
	{0}

	#undef CONFIGDEF
	#undef CONFIGDEF_KEYBINDING
	#undef CONFIGDEF_GPKEYBINDING
	#undef CONFIGDEF_INT
	#undef CONFIGDEF_FLOAT
	#undef CONFIGDEF_STRING
};

typedef struct ConfigEntryList {
	struct ConfigEntryList *next;
	struct ConfigEntryList *prev;
	ConfigEntry entry;
} ConfigEntryList;

static ConfigEntryList *unknowndefs = NULL;

void config_init(void) {
	if(config_initialized) {
		return;
	}

	#define CONFIGDEF_KEYBINDING(id,entryname,default)
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default)
	#define CONFIGDEF_INT(id,entryname,default)
	#define CONFIGDEF_FLOAT(id,entryname,default)
	#define CONFIGDEF_STRING(id,entryname,default) stralloc(&configdefs[CONFIG_##id].val.s, default);

	CONFIGDEFS

	#undef CONFIGDEF_KEYBINDING
	#undef CONFIGDEF_GPKEYBINDING
	#undef CONFIGDEF_INT
	#undef CONFIGDEF_FLOAT
	#undef CONFIGDEF_STRING

	config_initialized = true;
}

static void config_delete_unknown_entries(void);

void config_shutdown(void) {
	for(ConfigEntry *e = configdefs; e->name; ++e) {
		if(e->type == CONFIG_TYPE_STRING) {
			free(e->val.s);
			e->val.s = NULL;
		}
	}

	config_delete_unknown_entries();
}

void config_reset(void) {
	#define CONFIGDEF(id,default,ufield) configdefs[CONFIG_##id].val.ufield = default;
	#define CONFIGDEF_KEYBINDING(id,entryname,default) CONFIGDEF(id, default, i)
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) CONFIGDEF(id, default, i)
	#define CONFIGDEF_INT(id,entryname,default) CONFIGDEF(id, default, i)
	#define CONFIGDEF_FLOAT(id,entryname,default) CONFIGDEF(id, default, f)
	#define CONFIGDEF_STRING(id,entryname,default) stralloc(&configdefs[CONFIG_##id].val.s, default);

	CONFIGDEFS

	#undef CONFIGDEF
	#undef CONFIGDEF_KEYBINDING
	#undef CONFIGDEF_GPKEYBINDING
	#undef CONFIGDEF_INT
	#undef CONFIGDEF_FLOAT
	#undef CONFIGDEF_STRING
}

#ifndef CONFIG_RAWACCESS
ConfigEntry* config_get(ConfigIndex idx) {
	assert(idx >= 0 && idx < CONFIGIDX_NUM);
	return configdefs + idx;
}
#endif

static ConfigEntry* config_find_entry(const char *name) {
	ConfigEntry *e = configdefs;
	do if(!strcmp(e->name, name)) return e; while((++e)->name);
	return NULL;
}

KeyIndex config_key_from_scancode(int scan) {
	for(int i = CONFIG_KEY_FIRST; i <= CONFIG_KEY_LAST; ++i) {
		if(configdefs[i].val.i == scan) {
			return CFGIDX_TO_KEYIDX(i);
		}
	}

	return -1;
}

GamepadKeyIndex config_gamepad_key_from_gamepad_button(int btn) {
	for(int i = CONFIG_GAMEPAD_KEY_FIRST; i <= CONFIG_GAMEPAD_KEY_LAST; ++i) {
		if(configdefs[i].val.i == btn) {
			return CFGIDX_TO_GPKEYIDX(i);
		}
	}

	return -1;
}

KeyIndex config_key_from_gamepad_key(GamepadKeyIndex gpkey) {
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) case GAMEPAD_##id: return id;
	switch(gpkey) {
		GPKEYDEFS
		default: return -1;
	}
	#undef CONFIGDEF_GPKEYBINDING
}

GamepadKeyIndex config_gamepad_key_from_key(KeyIndex key) {
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) case id: return GAMEPAD_##id;
	switch(key) {
		GPKEYDEFS
		default: return -1;
	}
	#undef CONFIGDEF_GPKEYBINDING
}

KeyIndex config_key_from_gamepad_button(int btn) {
	return config_key_from_gamepad_key(config_gamepad_key_from_gamepad_button(btn));
}

static void config_set_val(ConfigIndex idx, ConfigValue v) {
	ConfigEntry *e = config_get(idx);
	ConfigCallback callback = e->callback;
	bool difference = true;

	switch(e->type) {
		case CONFIG_TYPE_INT:
		case CONFIG_TYPE_KEYBINDING:
		case CONFIG_TYPE_GPKEYBINDING:
			difference = e->val.i != v.i;
			break;

		case CONFIG_TYPE_FLOAT:
			difference = e->val.f != v.f;
			break;

		case CONFIG_TYPE_STRING:
			difference = strcmp(e->val.s, v.s);
			break;
	}

	if(!difference) {
		return;
	}

	if(callback) {
		e->callback = NULL;
		callback(idx, v);
		e->callback = callback;
		return;
	}

#define PRINTVAL(t) log_debug("%s:" #t " = %" #t, e->name, e->val.t);

	switch(e->type) {
		case CONFIG_TYPE_INT:
		case CONFIG_TYPE_KEYBINDING:
		case CONFIG_TYPE_GPKEYBINDING:
			e->val.i = v.i;
			PRINTVAL(i)
			break;

		case CONFIG_TYPE_FLOAT:
			e->val.f = v.f;
			PRINTVAL(f)
			break;

		case CONFIG_TYPE_STRING:
			stralloc(&e->val.s, v.s);
			PRINTVAL(s)
			break;
	}

#undef PRINTVAL
}

int config_set_int(ConfigIndex idx, int val) {
	ConfigValue v = {.i = val};
	config_set_val(idx, v);
	return config_get_int(idx);
}

double config_set_float(ConfigIndex idx, double val) {
	ConfigValue v = {.f = val};
	config_set_val(idx, v);
	return config_get_float(idx);
}

char* config_set_str(ConfigIndex idx, const char *val) {
	ConfigValue v = {.s = (char*)val};
	config_set_val(idx, v);
	return config_get_str(idx);
}

void config_set_callback(ConfigIndex idx, ConfigCallback callback) {
	ConfigEntry *e = config_get(idx);
	assert(e->callback == NULL);
	e->callback = callback;
}

static ConfigEntry* config_get_unknown_entry(const char *name) {
	ConfigEntry *e;
	ConfigEntryList *l;

	for(l = unknowndefs; l; l = l->next) {
		if(!strcmp(name, l->entry.name)) {
			return &l->entry;
		}
	}

	l = (ConfigEntryList*)list_push((List**)&unknowndefs, malloc(sizeof(ConfigEntryList)));
	e = &l->entry;
	memset(e, 0, sizeof(ConfigEntry));
	stralloc(&e->name, name);
	e->type = CONFIG_TYPE_STRING;

	return e;
}

static void config_set_unknown(const char *name, const char *val) {
	stralloc(&config_get_unknown_entry(name)->val.s, val);
}

static void* config_delete_unknown_entry(List **list, List *lentry, void *arg) {
	ConfigEntry *e = &((ConfigEntryList*)lentry)->entry;

	free(e->name);
	free(e->val.s);
	free(list_unlink(list, lentry));

	return NULL;
}

static void config_delete_unknown_entries(void) {
	list_foreach((List**)&unknowndefs, config_delete_unknown_entry, NULL);
}

void config_save(void) {
	SDL_RWops *out = vfs_open(CONFIG_FILE, VFS_MODE_WRITE);
	ConfigEntry *e = configdefs;

	if(!out) {
		log_warn("VFS error: %s", vfs_get_error());
		return;
	}

	SDL_RWprintf(out, "# Generated by %s %s\n", TAISEI_VERSION_FULL, TAISEI_VERSION_BUILD_TYPE);

	do switch(e->type) {
		case CONFIG_TYPE_INT:
			SDL_RWprintf(out, "%s = %i\n", e->name, e->val.i);
			break;

		case CONFIG_TYPE_KEYBINDING:
			SDL_RWprintf(out, "%s = %s\n", e->name, SDL_GetScancodeName(e->val.i));
			break;

		case CONFIG_TYPE_GPKEYBINDING:
			SDL_RWprintf(out, "%s = %s\n", e->name, SDL_GameControllerGetStringForButton(e->val.i));
			break;

		case CONFIG_TYPE_STRING:
			SDL_RWprintf(out, "%s = %s\n", e->name, e->val.s);
			break;

		case CONFIG_TYPE_FLOAT:
			SDL_RWprintf(out, "%s = %f\n", e->name, e->val.f);
			break;
	} while((++e)->name);

	if(unknowndefs) {
		SDL_RWprintf(out, "# The following options were not recognized by taisei\n");

		for(ConfigEntryList *l = unknowndefs; l; l = l->next) {
			e = &l->entry;
			SDL_RWprintf(out, "%s = %s\n", e->name, e->val.s);
		}
	}

	SDL_RWclose(out);

	char *sp = vfs_repr(CONFIG_FILE, true);
	log_info("Saved config '%s'", sp);
	free(sp);
}

#define INTOF(s)   ((int)strtol(s, NULL, 10))
#define FLOATOF(s) ((float)strtod(s, NULL))

typedef struct ConfigParseState {
	int first_entry;
} ConfigParseState;

static void config_set(const char *key, const char *val, void *data) {
	ConfigEntry *e = config_find_entry(key);
	ConfigParseState *state = data;

	if(!e) {
		log_warn("Unknown setting '%s'", key);
		config_set_unknown(key, val);

		if(state->first_entry < 0) {
			state->first_entry = CONFIGIDX_NUM;
		}

		return;
	}

	if(state->first_entry < 0) {
		state->first_entry = (intptr_t)(e - configdefs);
	}

	switch(e->type) {
		case CONFIG_TYPE_INT:
			e->val.i = INTOF(val);
			break;

		case CONFIG_TYPE_KEYBINDING: {
			SDL_Scancode scan = SDL_GetScancodeFromName(val);

			if(scan == SDL_SCANCODE_UNKNOWN) {
				log_warn("Unknown key '%s'", val);
			} else {
				e->val.i = scan;
			}

			break;
		}

		case CONFIG_TYPE_GPKEYBINDING: {
			SDL_GameControllerButton btn = SDL_GameControllerGetButtonFromString(val);

			if(btn == SDL_CONTROLLER_BUTTON_INVALID) {
				log_warn("Unknown gamepad key '%s'", val);
			} else {
				e->val.i = btn;
			}

			break;
		}

		case CONFIG_TYPE_STRING:
			stralloc(&e->val.s, val);
			break;

		case CONFIG_TYPE_FLOAT:
			e->val.f = FLOATOF(val);
			break;
	}
}

#undef INTOF
#undef FLOATOF

typedef void (*ConfigUpgradeFunc)(void);

static void config_upgrade_1(void) {
	// disable vsync by default
	config_set_int(CONFIG_VSYNC, 0);

	// this version also changes meaning of the vsync value
	// previously it was: 0 = on,  1 = off, 2 = adaptive, because lachs0r doesn't know how my absolutely genius options menu works.
	//         now it is: 0 = off, 1 = on,  2 = adaptive, as it should be.
}

static ConfigUpgradeFunc config_upgrades[] = {
	/*
		To bump the config version and add an upgrade state, simply append an upgrade function to this array.
		NEVER reorder these entries, remove them, etc, ONLY append.

		If a stage is no longer needed (nothing useful needs to be done, e.g. the later stages would override it),
		then replace the function pointer with NULL.
	*/

	config_upgrade_1,
};

static void config_apply_upgrades(int start) {
	int num_upgrades = sizeof(config_upgrades) / sizeof(ConfigUpgradeFunc);

	if(start > num_upgrades) {
		log_warn("Config seems to be from a newer version of Taisei (config version %i, ours is %i)", start, num_upgrades);
		return;
	}

	for(int upgrade = start; upgrade < num_upgrades; ++upgrade) {
		if(config_upgrades[upgrade]) {
			log_info("Upgrading config to version %i", upgrade + 1);
			config_upgrades[upgrade]();
		} else {
			log_debug("Upgrade to version %i is a no-op", upgrade + 1);
		}
	}
}

void config_load(void) {
	config_init();

	char *config_path = vfs_repr(CONFIG_FILE, true);
	SDL_RWops *config_file = vfs_open(CONFIG_FILE, VFS_MODE_READ);

	if(config_file) {
		log_info("Loading configuration from %s", config_path);

		ConfigParseState *state = malloc(sizeof(ConfigParseState));
		state->first_entry = -1;

		if(!parse_keyvalue_stream_cb(config_file, config_set, state)) {
			log_warn("Errors occured while parsing the configuration file");
		}

		if(state->first_entry != CONFIG_VERSION) {
			// config file was likely written by an old version of taisei that is unaware of the upgrade mechanism.
			config_set_int(CONFIG_VERSION, 0);
		}

		free(state);
		SDL_RWclose(config_file);

		config_apply_upgrades(config_get_int(CONFIG_VERSION));
	} else {
		VFSInfo i = vfs_query(CONFIG_FILE);

		if(i.error) {
			log_warn("VFS error: %s", vfs_get_error());
		} else if(i.exists) {
			log_warn("Config file %s is not readable", config_path);
		}
	}

	free(config_path);

	// set config version to the latest
	config_set_int(CONFIG_VERSION, sizeof(config_upgrades) / sizeof(ConfigUpgradeFunc));
}
