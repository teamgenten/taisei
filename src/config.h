/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#pragma once
#include "taisei.h"

#include <SDL_keycode.h>
#include <stdbool.h>

#define CONFIG_FILE "storage/config"

/*
	Define these macros, then use CONFIGDEFS to expand them all for all config entries, or KEYDEFS for just keybindings.
	Don't forget to undef them afterwards.
	Remember that the id won't have the CONFIG_ prefix, prepend it yourself if you want a ConfigIndex constant.
*/

// #define CONFIGDEF_KEYBINDING(id,entryname,default)
// #define CONFIGDEF_GPKEYBINDING(id,entryname,default)
// #define CONFIGDEF_INT(id,entryname,default)
// #define CONFIGDEF_FLOAT(id,entryname,default)
// #define CONFIGDEF_STRING(id,entryname,default)

/*
 *	DO NOT REORDER KEYBINDINGS.
 *
 *	The KEY_* constants are used by replays. These are formed from KEYDEFS.
 *	Always add new keys at the end of KEYDEFS, never put them directly into CONFIGDEFS.
 *
 *	KEYDEFS can be safely moved around within CONFIGDEFS, however.
 *	Stuff in GPKEYDEFS is safe to reorder.
 */

#define KEYDEFS \
	CONFIGDEF_KEYBINDING(KEY_UP,				"key_up",				SDL_SCANCODE_UP) \
	CONFIGDEF_KEYBINDING(KEY_DOWN,				"key_down",				SDL_SCANCODE_DOWN) \
	CONFIGDEF_KEYBINDING(KEY_LEFT,				"key_left",				SDL_SCANCODE_LEFT) \
	CONFIGDEF_KEYBINDING(KEY_RIGHT,				"key_right",			SDL_SCANCODE_RIGHT) \
	CONFIGDEF_KEYBINDING(KEY_FOCUS,				"key_focus",			SDL_SCANCODE_LSHIFT) \
	CONFIGDEF_KEYBINDING(KEY_SHOT,				"key_shot",				SDL_SCANCODE_Z) \
	CONFIGDEF_KEYBINDING(KEY_BOMB,				"key_bomb",				SDL_SCANCODE_X) \
	CONFIGDEF_KEYBINDING(KEY_FULLSCREEN,		"key_fullscreen",		SDL_SCANCODE_F11) \
	CONFIGDEF_KEYBINDING(KEY_SCREENSHOT,		"key_screenshot",		SDL_SCANCODE_P) \
	CONFIGDEF_KEYBINDING(KEY_SKIP,				"key_skip",				SDL_SCANCODE_LCTRL) \
	CONFIGDEF_KEYBINDING(KEY_IDDQD,				"key_iddqd",			SDL_SCANCODE_Q) \
	CONFIGDEF_KEYBINDING(KEY_HAHAIWIN,			"key_skipstage",		SDL_SCANCODE_E) \
	CONFIGDEF_KEYBINDING(KEY_PAUSE,				"key_pause",			SDL_SCANCODE_PAUSE) \
	CONFIGDEF_KEYBINDING(KEY_NOBACKGROUND,		"key_nobackground",		SDL_SCANCODE_LALT) \
	CONFIGDEF_KEYBINDING(KEY_POWERUP,			"key_powerup",			SDL_SCANCODE_2) \
	CONFIGDEF_KEYBINDING(KEY_POWERDOWN,			"key_powerdown",		SDL_SCANCODE_1) \
	CONFIGDEF_KEYBINDING(KEY_FPSLIMIT_OFF,		"key_fpslimit_off",		SDL_SCANCODE_RSHIFT) \


#define GPKEYDEFS \
	CONFIGDEF_GPKEYBINDING(KEY_UP, 				"gamepad_key_up", 		SDL_CONTROLLER_BUTTON_DPAD_UP) \
	CONFIGDEF_GPKEYBINDING(KEY_DOWN, 			"gamepad_key_down", 	SDL_CONTROLLER_BUTTON_DPAD_DOWN) \
	CONFIGDEF_GPKEYBINDING(KEY_LEFT, 			"gamepad_key_left", 	SDL_CONTROLLER_BUTTON_DPAD_LEFT) \
	CONFIGDEF_GPKEYBINDING(KEY_RIGHT, 			"gamepad_key_right", 	SDL_CONTROLLER_BUTTON_DPAD_RIGHT) \
	CONFIGDEF_GPKEYBINDING(KEY_FOCUS, 			"gamepad_key_focus", 	SDL_CONTROLLER_BUTTON_X) \
	CONFIGDEF_GPKEYBINDING(KEY_SHOT, 			"gamepad_key_shot", 	SDL_CONTROLLER_BUTTON_A) \
	CONFIGDEF_GPKEYBINDING(KEY_BOMB, 			"gamepad_key_bomb", 	SDL_CONTROLLER_BUTTON_Y) \
	CONFIGDEF_GPKEYBINDING(KEY_SKIP, 			"gamepad_key_skip", 	SDL_CONTROLLER_BUTTON_B) \


#define CONFIGDEFS \
	 /* @version must be on top. don't change its default value here, it does nothing. */ \
	CONFIGDEF_INT		(VERSION,					"@version",								0) \
	\
	CONFIGDEF_STRING	(PLAYERNAME, 				"playername", 							"Player") \
	CONFIGDEF_INT		(FULLSCREEN, 				"fullscreen", 							0) \
	CONFIGDEF_INT		(FULLSCREEN_DESKTOP,		"fullscreen_desktop_mode",				1) \
	CONFIGDEF_INT		(VID_WIDTH, 				"vid_width", 							RESX) \
	CONFIGDEF_INT		(VID_HEIGHT, 				"vid_height", 							RESY) \
	CONFIGDEF_INT		(VID_RESIZABLE,				"vid_resizable",						0) \
	CONFIGDEF_INT		(VSYNC, 					"vsync", 								0) \
	CONFIGDEF_INT		(MIXER_CHUNKSIZE,			"mixer_chunksize",						1024) \
	CONFIGDEF_FLOAT		(SFX_VOLUME, 				"sfx_volume", 							1.0) \
	CONFIGDEF_FLOAT		(BGM_VOLUME, 				"bgm_volume", 							1.0) \
	CONFIGDEF_INT		(NO_STAGEBG, 				"disable_stagebg", 						0) \
	CONFIGDEF_INT		(SAVE_RPY, 					"save_rpy", 							2) \
	CONFIGDEF_INT		(SPELLSTAGE_AUTORESTART,	"spellpractice_restart_on_fail",		0) \
	CONFIGDEF_FLOAT		(TEXT_QUALITY,				"text_quality",							1.0) \
	CONFIGDEF_FLOAT		(FG_QUALITY,				"fg_quality",							1.0) \
	CONFIGDEF_FLOAT		(BG_QUALITY,				"bg_quality",							1.0) \
	CONFIGDEF_INT		(SHOT_INVERTED,				"shot_inverted",						0) \
	CONFIGDEF_INT		(FOCUS_LOSS_PAUSE,			"focus_loss_pause",						1) \
	KEYDEFS \
	CONFIGDEF_INT		(GAMEPAD_ENABLED, 			"gamepad_enabled", 						0) \
	CONFIGDEF_STRING	(GAMEPAD_DEVICE, 			"gamepad_device", 						"default") \
	CONFIGDEF_INT		(GAMEPAD_AXIS_UD, 			"gamepad_axis_ud", 						1) \
	CONFIGDEF_INT		(GAMEPAD_AXIS_LR, 			"gamepad_axis_lr", 						0) \
	CONFIGDEF_INT		(GAMEPAD_AXIS_FREE, 		"gamepad_axis_free", 					1) \
	CONFIGDEF_FLOAT		(GAMEPAD_AXIS_UD_SENS, 		"gamepad_axis_ud_free_sensitivity", 	1.0) \
	CONFIGDEF_FLOAT		(GAMEPAD_AXIS_LR_SENS, 		"gamepad_axis_lr_free_sensitivity", 	1.0) \
	CONFIGDEF_FLOAT		(GAMEPAD_AXIS_DEADZONE, 	"gamepad_axis_deadzone", 				0.0) \
	GPKEYDEFS \


typedef enum ConfigIndex {
	#define CONFIGDEF(id) CONFIG_##id,
	#define CONFIGDEF_KEYBINDING(id,entryname,default) CONFIGDEF(id)
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) CONFIGDEF(GAMEPAD_##id)
	#define CONFIGDEF_INT(id,entryname,default) CONFIGDEF(id)
	#define CONFIGDEF_FLOAT(id,entryname,default) CONFIGDEF(id)
	#define CONFIGDEF_STRING(id,entryname,default) CONFIGDEF(id)

	CONFIGDEFS
	CONFIGIDX_NUM

	#undef CONFIGDEF
	#undef CONFIGDEF_KEYBINDING
	#undef CONFIGDEF_GPKEYBINDING
	#undef CONFIGDEF_INT
	#undef CONFIGDEF_FLOAT
	#undef CONFIGDEF_STRING
} ConfigIndex;

#define CONFIGIDX_FIRST 0
#define CONFIGIDX_LAST (CONFIGIDX_NUM - 1)

typedef enum KeyIndex {
	#define CONFIGDEF_KEYBINDING(id,entryname,default) id,

	KEYDEFS
	KEYIDX_NUM

	#undef CONFIGDEF_KEYBINDING
} KeyIndex;

#define KEYIDX_FIRST 0
#define KEYIDX_LAST (KEYIDX_NUM - 1)
#define CFGIDX_TO_KEYIDX(idx) (idx - CONFIG_KEY_FIRST + KEYIDX_FIRST)
#define KEYIDX_TO_CFGIDX(idx) (idx + CONFIG_KEY_FIRST - KEYIDX_FIRST)
#define CONFIG_KEY_FIRST CONFIG_KEY_UP
#define CONFIG_KEY_LAST (CONFIG_KEY_FIRST + KEYIDX_LAST - KEYIDX_FIRST)

typedef enum GamepadKeyIndex {
	#define CONFIGDEF_GPKEYBINDING(id,entryname,default) GAMEPAD_##id,

	GPKEYDEFS
	GAMEPAD_KEYIDX_NUM

	#undef CONFIGDEF_GPKEYBINDING
} GamepadKeyIndex;

#define GAMEPAD_KEYIDX_FIRST 0
#define GAMEPAD_KEYIDX_LAST (GAMEPAD_KEYIDX_NUM - 1)
#define CFGIDX_TO_GPKEYIDX(idx) (idx - CONFIG_GAMEPAD_KEY_FIRST + GAMEPAD_KEYIDX_FIRST)
#define GPKEYIDX_TO_CFGIDX(idx) (idx + CONFIG_GAMEPAD_KEY_FIRST - GAMEPAD_KEYIDX_FIRST)
#define CONFIG_GAMEPAD_KEY_FIRST CONFIG_GAMEPAD_KEY_UP
#define CONFIG_GAMEPAD_KEY_LAST (CONFIG_GAMEPAD_KEY_FIRST + GAMEPAD_KEYIDX_LAST - GAMEPAD_KEYIDX_FIRST)

typedef enum ConfigEntryType {
	CONFIG_TYPE_INT,
	CONFIG_TYPE_STRING,
	CONFIG_TYPE_KEYBINDING,
	CONFIG_TYPE_GPKEYBINDING,
	CONFIG_TYPE_FLOAT
} ConfigEntryType;

typedef union ConfigValue {
	int i;
	double f;
	char *s;
} ConfigValue;

typedef void (*ConfigCallback)(ConfigIndex, ConfigValue);

typedef struct ConfigEntry {
	ConfigEntryType type;
	char *name;
	ConfigValue val;
	ConfigCallback callback;
} ConfigEntry;

#define CONFIG_LOAD_BUFSIZE 256

KeyIndex config_key_from_scancode(int scan);
GamepadKeyIndex config_gamepad_key_from_gamepad_button(int btn);
KeyIndex config_key_from_gamepad_key(GamepadKeyIndex gpkey);
GamepadKeyIndex config_gamepad_key_from_key(KeyIndex key);
KeyIndex config_key_from_gamepad_button(int btn);

void config_reset(void);
void config_init(void);
void config_shutdown(void);
void config_load(void);
void config_save(void);
void config_set_callback(ConfigIndex idx, ConfigCallback callback);

#ifndef DEBUG
	#define CONFIG_RAWACCESS
#endif

#ifdef CONFIG_RAWACCESS
	#define CONFIGDEFS_EXPORT
	extern ConfigEntry configdefs[];
	#define config_get(idx) (configdefs + (idx))
#else
	#define CONFIGDEFS_EXPORT static
	ConfigEntry* config_get(ConfigIndex idx);
#endif

#define config_get_int(idx) (config_get(idx)->val.i)
#define config_get_float(idx) (config_get(idx)->val.f)
#define config_get_str(idx) (config_get(idx)->val.s)

int config_set_int(ConfigIndex idx, int val);
double config_set_float(ConfigIndex idx, double val);
char* config_set_str(ConfigIndex idx, const char *val);
