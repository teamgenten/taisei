/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2017, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2017, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "boss.h"
#include "global.h"
#include "stage.h"
#include "stagetext.h"

Boss* create_boss(char *name, char *ani, char *dialog, complex pos) {
	Boss *buf = malloc(sizeof(Boss));
	memset(buf, 0, sizeof(Boss));

	buf->name = malloc(strlen(name) + 1);
	strcpy(buf->name, name);
	buf->pos = pos;

	aniplayer_create(&buf->ani, get_ani(ani));
	if(dialog)
		buf->dialog = get_tex(dialog);

	buf->birthtime = global.frames;
	return buf;
}

void draw_boss_text(Alignment align, float x, float y, const char *text, TTF_Font *fnt, Color clr) {
	Color black = rgb(0, 0, 0);
	Color white = rgb(1, 1, 1);

	parse_color_call(derive_color(black, CLRMASK_A, clr), glColor4f);
	draw_text(align, x+1, y+1, text, fnt);
	parse_color_call(clr, glColor4f);
	draw_text(align, x, y, text, fnt);

	if(clr != white) {
		parse_color_call(white, glColor4f);
	}
}

void spell_opening(Boss *b, int time) {
	complex x0 = VIEWPORT_W/2+I*VIEWPORT_H/3.5;
	float f = clamp((time-40.)/60.,0,1);

	complex x = x0 + (VIEWPORT_W+I*35 - x0) * f*(f+1)*0.5;

	int strw = stringwidth(b->current->name,_fonts.standard);

	glPushMatrix();
	glTranslatef(creal(x),cimag(x),0);
	float scale = f+1.*(1-f)*(1-f)*(1-f);
	glScalef(scale,scale,1);
	glRotatef(360*f,1,1,0);
	glDisable(GL_CULL_FACE);
	draw_boss_text(AL_Right, strw/2*(1-f), 0, b->current->name, _fonts.standard, rgb(1, 1, 1));
	glEnable(GL_CULL_FACE);
	glPopMatrix();

	glUseProgram(0);
}

void draw_extraspell_bg(Boss *boss, int time) {
	// overlay for all extra spells

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(0.2,0.1,0,0.7);
	fill_screen(sin(time) * 0.015, time / 50.0, 1, "stage3/wspellclouds");
	glColor4f(1,1,1,1);
	glBlendEquation(GL_MIN);
	fill_screen(cos(time) * 0.015, time / 70.0, 1, "stage4/kurumibg2");
	fill_screen(sin(time+2.1) * 0.015, time / 30.0, 1, "stage4/kurumibg2");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
}

Color boss_healthbar_color(AttackType atype) {
	switch(atype) {
	default: case AT_Normal:        return rgb(1.00, 1.00, 1.00);
	         case AT_Spellcard:     return rgb(1.00, 0.65, 0.65);
	         case AT_SurvivalSpell: return rgb(0.50, 0.50, 1.00);
	         case AT_ExtraSpell:    return rgb(1.00, 0.30, 0.20);
	}
}

static StageProgress* get_spellstage_progress(Attack *a, StageInfo **out_stginfo, bool write) {
	if(!write || (global.replaymode == REPLAY_RECORD && global.stage->type == STAGE_STORY)) {
		StageInfo *i = stage_get_by_spellcard(a->info, global.diff);
		if(i) {
			StageProgress *p = stage_get_progress_from_info(i, global.diff, write);

			if(out_stginfo) {
				*out_stginfo = i;
			}

			if(p) {
				return p;
			}
		}
#if DEBUG
		else if(a->type == AT_Spellcard || a->type == AT_ExtraSpell) {
			log_warn("FIXME: spellcard '%s' is not available in spell practice mode!", a->name);
		}
#endif
	}

	return NULL;
}

static bool boss_should_skip_attack(Boss *boss, Attack *a) {
	// if we failed any spells on this boss up to this point, skip any extra spells,
	// as well as any attacks associated with them.
	//
	// (for example, the "Generic move" that might have been automatically added by
	// boss_add_attack_from_info. this is what the a->info->type check is for.)

	return boss->failed_spells && (a->type == AT_ExtraSpell || (a->info && a->info->type == AT_ExtraSpell));
}

static Attack* boss_get_final_attack(Boss *boss) {
	Attack *final;
	for(final = boss->attacks + boss->acount - 1; final >= boss->attacks && boss_should_skip_attack(boss, final); --final);
	return final >= boss->attacks ? final : NULL;
}

static Attack* boss_get_next_attack(Boss *boss) {
	Attack *next;
	for(next = boss->current + 1; next < boss->attacks + boss->acount && boss_should_skip_attack(boss, next); ++next);
	return next < boss->attacks + boss->acount ? next : NULL;
}

static bool boss_attack_is_final(Boss *boss, Attack *a) {
	return boss_get_final_attack(boss) == a;
}

static bool attack_is_over(Attack *a) {
	return a->hp <= 0 && global.frames > a->endtime;
}

static void BossGlow(Projectile *p, int t) {
	AniPlayer *aplr = (AniPlayer*)REF(p->args[2]);
	assert(aplr != NULL);

	Shader *shader = get_shader("silhouette");
	glUseProgram(shader->prog);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glPushMatrix();
	float s = 1.0+t/p->args[0]*0.5;
	glTranslatef(creal(p->pos), cimag(p->pos), 0);
	glScalef(s, s, 1);

	float clr[4];
	parse_color_array(p->color, clr);
	clr[3] = 1.5 - s;

	float fade = 1 - clr[3];
	float deform = 5 - 10 * fade * fade;
	glUniform4fv(uniloc(shader, "color"), 1, clr);
	glUniform1f(uniloc(shader, "deform"), deform);

	aniplayer_play(aplr,0,0);

	glPopMatrix();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(recolor_get_shader()->prog);
}

static int boss_glow_rule(Projectile *p, int t) {
	if(t == EVENT_DEATH) {
		AniPlayer *aplr = REF(p->args[2]);
		free(aplr);
		free_ref(p->args[2]);
		return 1;
	}

	return enemy_flare(p, t);
}

static Projectile* spawn_boss_glow(Boss *boss, Color clr, int timeout) {
	// copy animation state to render the same frame even after the boss has changed its own
	AniPlayer *aplr = aniplayer_create_copy(&boss->ani);

	return PARTICLE(
		// this is in sync with the boss position oscillation
		.pos = boss->pos + 6 * sin(global.frames/25.0) * I,
		.color = clr,
		.rule = boss_glow_rule,
		.draw_rule = BossGlow,
		.args = { timeout, 0, add_ref(aplr), },
		.angle = M_PI * 2 * frand(),
		.size = (1+I)*9000, // ensure it's drawn under everything else
	);
}

static void spawn_particle_effects(Boss *boss) {
	Color glowcolor = boss->glowcolor;
	Color shadowcolor = boss->shadowcolor;

	Attack *cur = boss->current;
	bool is_spell = cur && ATTACK_IS_SPELL(cur->type) && !cur->endtime;
	bool is_extra = cur && cur->type == AT_ExtraSpell && global.frames >= cur->starttime;

	if(!(global.frames % 13) && !is_extra) {
		complex v = cexp(I*global.frames);

		PARTICLE("smoke", v, shadowcolor, enemy_flare,
			.draw_rule = EnemyFlareShrink,
			.args = { 180, 0, add_ref(boss), },
			.angle = M_PI * 2 * frand(),
			.flags = PFLAG_DRAWADD,
		);
	}

	if(!(global.frames % (2 + 2 * is_extra)) && (is_spell || boss_is_dying(boss))) {
		float glowstr = 0.5;
		float a = (1.0 - glowstr) + glowstr * pow(psin(global.frames/15.0), 1.0);
		spawn_boss_glow(boss, multiply_colors(glowcolor, rgb(a, a, a)), 24);
	}
}

void draw_boss_background(Boss *boss) {
	glPushMatrix();
	glTranslatef(creal(boss->pos), cimag(boss->pos), 0);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glRotatef(global.frames*4.0, 0, 0, -1);

	float f = 0.8+0.1*sin(global.frames/8.0);

	if(boss_is_dying(boss)) {
		float t = (global.frames - boss->current->endtime)/(float)BOSS_DEATH_DELAY + 1;
		f -= t*(t-0.7)/max(0.01, 1-t);
	}

	glScalef(f,f,f);
	draw_texture(0, 0, "boss_circle");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPopMatrix();
}

void draw_boss(Boss *boss) {
	float red = 0.5*exp(-0.5*(global.frames-boss->lastdamageframe));
	if(red > 1)
		red = 0;

	float boss_alpha = 1;

	if(boss_is_dying(boss)) {
		float t = (global.frames - boss->current->endtime)/(float)BOSS_DEATH_DELAY + 1;
		boss_alpha = (1 - t) + 0.3;
	}

	glColor4f(1,1-red,1-red/2,boss_alpha);
	aniplayer_play(&boss->ani,creal(boss->pos), cimag(boss->pos) + 6*sin(global.frames/25.0));
	glColor4f(1,1,1,1);

	if(boss->current->type == AT_Move && global.frames - boss->current->starttime > 0 && boss_attack_is_final(boss, boss->current))
		return;

	draw_boss_text(AL_Left, 10, 20, boss->name, _fonts.standard, rgb(1, 1, 1));

	if(!boss->current)
		return;

	if(ATTACK_IS_SPELL(boss->current->type))
		spell_opening(boss, global.frames - boss->current->starttime);

	if(boss->current->type != AT_Move) {
		char buf[16];
		float remaining = max(0, (boss->current->timeout - global.frames + boss->current->starttime)/(float)FPS);
		Color textclr;

		if(remaining < 6) {
			textclr = rgb(1.0, 0.2, 0.2);
		} else if(remaining < 11) {
			textclr = rgb(1.0, 1.0, 0.2);
		} else {
			textclr = rgb(1.0, 1.0, 1.0);
		}

		snprintf(buf, sizeof(buf),  "%.2f", remaining);
		draw_boss_text(AL_Center, VIEWPORT_W - 24, 10, buf, _fonts.standard, textclr);

		StageProgress *p = get_spellstage_progress(boss->current, NULL, false);
		if(p) {
			float a = clamp((global.frames - boss->current->starttime - 60) / 60.0, 0, 1);
			snprintf(buf, sizeof(buf), "%u / %u", p->num_cleared, p->num_played);
			draw_boss_text(AL_Right,
				VIEWPORT_W + stringwidth(buf, _fonts.small) * pow(1 - a, 2),
				35 + stringheight(buf, _fonts.small),
				buf, _fonts.small, rgba(1, 1, 1, a)
			);
		}

		int maxhpspan = 0;
		int hpspan = 0;
		int nextspell, prevspell;
		for(nextspell = 0; nextspell < boss->acount - 1; nextspell++) {
			if(boss_should_skip_attack(boss,&boss->attacks[nextspell]))
				continue;
			int t = boss->attacks[nextspell].type;
			if(!attack_is_over(&boss->attacks[nextspell]) && t == AT_Spellcard)
				break;
		}


		for(prevspell = nextspell; prevspell > 0; prevspell--) {
			if(boss_should_skip_attack(boss,&boss->attacks[prevspell]))
				continue;

			int t = boss->attacks[prevspell].type;
			if(attack_is_over(&boss->attacks[prevspell]) && t == AT_Spellcard)
				break;
			maxhpspan += boss->attacks[prevspell].maxhp;
			hpspan += boss->attacks[prevspell].hp;
		}

		if(!maxhpspan) {
			return;
		}

		glDisable(GL_TEXTURE_2D);
		glPushMatrix();
		glTranslatef(10,2,0);
		glScalef((VIEWPORT_W-60)/(float)maxhpspan,1,1);

		// background shadow
		glPushMatrix();
		glColor4f(0,0,0,0.65);
		glScalef(hpspan+2, 4, 1);
		glTranslatef(0.5, 0.5, 0);
		draw_quad();
		glPopMatrix();

		// actual health bar
		for(int i = nextspell; i > prevspell; i--) {
			if(boss_should_skip_attack(boss,&boss->attacks[i]))
				continue;

			parse_color_call(boss_healthbar_color(boss->attacks[i].type), glColor4f);

			glPushMatrix();
			glScalef(boss->attacks[i].hp, 2, 1);
			glTranslatef(+0.5, 0.5, 0);
			draw_quad();
			glPopMatrix();
			glTranslatef(boss->attacks[i].hp, 0, 0);
		}

		glPopMatrix();
		glEnable(GL_TEXTURE_2D);

		// remaining spells
		glColor4f(1,1,1,0.7);

		int x = 0;
		for(int i = boss->acount-1; i > nextspell; i--)
			if(boss->attacks[i].type == AT_Spellcard)
				draw_texture_with_size(x += 22, 40, 20, 20, "star");

		glColor3f(1,1,1);
	}
}

void boss_rule_extra(Boss *boss, float alpha) {
	if(global.frames % 5) {
		return;
	}

	int cnt = 10 * max(1, alpha);
	alpha = min(2, alpha);
	int lt = 1;

	if(alpha == 0) {
		lt += 2;
		alpha = 1 * frand();
	}

	for(int i = 0; i < cnt; ++i) {
		float a = i*2*M_PI/cnt + global.frames / 100.0;
		complex dir = cexp(I*(a+global.frames/50.0));
		complex vel = dir * 3;
		float v = max(0, alpha - 1);
		float psina = psin(a);

		PARTICLE(
			.texture = (frand() < v*0.3 || lt > 1) ? "stain" : "arc",
			.pos = boss->pos + dir * (100 + 50 * psin(alpha*global.frames/10.0+2*i)) * alpha,
			.color = rgb(
				1.0 - 0.5 * psina *    v,
				0.5 + 0.2 * psina * (1-v),
				0.5 + 0.5 * psina *    v
			),
			.rule = timeout_linear,
			.draw_rule = GrowFade,
			.flags = PFLAG_DRAWADD,
			.args = { 30*lt, vel * (1 - 2 * !(global.frames % 10)), 2.5 },
		);
	}
}

bool boss_is_dying(Boss *boss) {
	return boss->current && boss->current->endtime && boss->current->type != AT_Move && boss_attack_is_final(boss, boss->current);
}

bool boss_is_fleeing(Boss *boss) {
	return boss->current && boss->current->type == AT_Move && boss_attack_is_final(boss, boss->current);
}

bool boss_is_vulnerable(Boss *boss) {
	return boss->current && boss->current->type != AT_Move && boss->current->type != AT_SurvivalSpell && boss->current->starttime < global.frames && !boss->current->finished;
}

bool boss_damage(Boss *boss, int dmg) {
	if(!boss || !boss->current || !boss_is_vulnerable(boss))
		return false;
	if(dmg > 0 && global.frames-boss->lastdamageframe > 2)
		boss->lastdamageframe = global.frames;
	boss->current->hp -= dmg;
	return true;
}

static void boss_give_spell_bonus(Boss *boss, Attack *a, Player *plr) {
	bool fail = a->failtime, extra = a->type == AT_ExtraSpell;

	const char *title = extra ?
						(fail ? "Extra Spell failed..." : "Extra Spell cleared!"):
						(fail ?       "Spell failed..." :       "Spell cleared!");

	int time_left = max(0, a->starttime + a->timeout - global.frames);

	double sv = a->scorevalue;

	int clear_bonus = 0.5 * sv * !fail;
	int time_bonus = sv * (time_left / (double)a->timeout);
	int endurance_bonus = 0;
	int surv_bonus = 0;

	if(fail) {
		time_bonus /= 4;
		endurance_bonus = sv * 0.1 * (max(0, a->failtime - a->starttime) / (double)a->timeout);
	} else if(a->type == AT_SurvivalSpell) {
		surv_bonus = sv * (1.0 + 0.02 * (a->timeout / (double)FPS));
	}

	int total = time_bonus + surv_bonus + endurance_bonus + clear_bonus;
	float diff_bonus = 0.6 + 0.2 * global.diff;
	total *= diff_bonus;

	char diff_bonus_text[6];
	snprintf(diff_bonus_text, sizeof(diff_bonus_text), "x%.2f", diff_bonus);

	player_add_points(plr, total);

	StageTextTable tbl;
	stagetext_begin_table(&tbl, title, rgb(1, 1, 1), rgb(1, 1, 1), VIEWPORT_W/2, 0,
		ATTACK_END_DELAY_SPELL * 2, ATTACK_END_DELAY_SPELL / 2, ATTACK_END_DELAY_SPELL);
	stagetext_table_add_numeric_nonzero(&tbl, "Clear bonus", clear_bonus);
	stagetext_table_add_numeric_nonzero(&tbl, "Time bonus", time_bonus);
	stagetext_table_add_numeric_nonzero(&tbl, "Survival bonus", surv_bonus);
	stagetext_table_add_numeric_nonzero(&tbl, "Endurance bonus", endurance_bonus);
	stagetext_table_add_separator(&tbl);
	stagetext_table_add(&tbl, "Diff. multiplier", diff_bonus_text);
	stagetext_table_add_numeric(&tbl, "Total", total);
	stagetext_end_table(&tbl);

	play_sound("spellend");

	if(!fail) {
		play_sound("spellclear");
	}
}

static int attack_end_delay(Boss *boss) {
	if(boss_attack_is_final(boss, boss->current)) {
		return BOSS_DEATH_DELAY;
	}

	int delay = 0;

	switch(boss->current->type) {
		case AT_Spellcard:  	delay = ATTACK_END_DELAY_SPELL;	break;
		case AT_SurvivalSpell:	delay = ATTACK_END_DELAY_SURV;	break;
		case AT_ExtraSpell: 	delay = ATTACK_END_DELAY_EXTRA;	break;
		case AT_Move:       	delay = ATTACK_END_DELAY_MOVE;	break;
		default:            	delay = ATTACK_END_DELAY;		break;
	}

	if(delay) {
		Attack *next = boss_get_next_attack(boss);

		if(next && next->type == AT_ExtraSpell) {
			delay += ATTACK_END_DELAY_PRE_EXTRA;
		}
	}

	return delay;
}

void boss_finish_current_attack(Boss *boss) {
	AttackType t = boss->current->type;

	boss->current->hp = 0;
	boss->current->finished = true;
	boss->current->rule(boss, EVENT_DEATH);

	aniplayer_reset(&boss->ani);

	if(t != AT_Move) {
		stage_clear_hazards(true);
	}

	if(ATTACK_IS_SPELL(t)) {
		boss_give_spell_bonus(boss, boss->current, &global.plr);

		if(!boss->current->failtime) {
			StageProgress *p = get_spellstage_progress(boss->current, NULL, true);

			if(p) {
				++p->num_cleared;
			}
		} else if(boss->current->type != AT_ExtraSpell) {
			boss->failed_spells++;
		}

		if(t != AT_SurvivalSpell) {
			double i_base = 6.0, i_pwr = 1.0, i_pts = 1.0;

			if(t == AT_ExtraSpell) {
				i_pwr *= 1.25;
				i_pts *= 2.0;
			}

			if(!boss->current->failtime) {
				i_base *= 2.0;
			}

			spawn_items(boss->pos,
				Power, (int)(i_base * i_pwr),
				Point, (int)(i_base * i_pts),
			NULL);
		}
	}

	boss->current->endtime = global.frames + attack_end_delay(boss);
}

void process_boss(Boss **pboss) {
	Boss *boss = *pboss;

	aniplayer_update(&boss->ani);

	if(boss->global_rule) {
		boss->global_rule(boss, global.frames - boss->birthtime);
	}

	spawn_particle_effects(boss);

	if(!boss->current || global.dialog) {
		return;
	}

	int time = global.frames - boss->current->starttime;
	bool extra = boss->current->type == AT_ExtraSpell;
	bool over = boss->current->finished && global.frames >= boss->current->endtime;

	if(!boss->current->endtime) {
		int remaining = boss->current->timeout - time;

		if(boss->current->type != AT_Move && remaining <= 11*FPS && remaining > 0 && !(time % FPS)) {
			play_sound(remaining <= 6*FPS ? "timeout2" : "timeout1");
		}

		boss->current->rule(boss, time);
	}

	if(extra) {
		float base = 0.2;
		float ampl = 0.2;
		float s = sin(time / 90.0 + M_PI*1.2);

		if(boss->current->endtime) {
			float p = (boss->current->endtime - global.frames)/(float)ATTACK_END_DELAY_EXTRA;
			float a = max((base + ampl * s) * p * 0.5, 5 * pow(1 - p, 3));
			if(a < 2) {
				global.shake_view = 3 * a;
				boss_rule_extra(boss, a);
				if(a > 1) {
					boss_rule_extra(boss, a * 0.5);
					if(a > 1.3) {
						global.shake_view = 5 * a;
						if(a > 1.7)
							global.shake_view += 2 * a;
						boss_rule_extra(boss, 0);
						boss_rule_extra(boss, 0.1);
					}
				}
			} else {
				global.shake_view_fade = 0.15;
			}
		} else if(time < 0) {
			boss_rule_extra(boss, 1+time/(float)ATTACK_START_DELAY_EXTRA);
		} else {
			float o = min(0, -5 + time/30.0);
			float q = (time <= 150? 1 - pow(time/250.0, 2) : min(1, time/60.0));

			boss_rule_extra(boss, max(1-time/300.0, base + ampl * s) * q);
			if(o) {
				boss_rule_extra(boss, max(1-time/300.0, base + ampl * s) - o);
				if(!global.shake_view) {
					global.shake_view = 5;
					global.shake_view_fade = 0.9;
				} else if(o > -0.05) {
					global.shake_view = 10;
					global.shake_view_fade = 0.5;
				}
			}
		}
	}

	bool timedout = time > boss->current->timeout;

	if((boss->current->type != AT_Move && boss->current->hp <= 0) || timedout) {
		if(!boss->current->endtime) {
			if(timedout && boss->current->type != AT_SurvivalSpell) {
				boss->current->failtime = global.frames;
			}

			boss_finish_current_attack(boss);
		} else if(boss->current->type != AT_Move || !boss_attack_is_final(boss, boss->current)) {
			// XXX: do we actually need to call this for AT_Move attacks at all?
			//      it should be harmless, but probably unnecessary.
			//      i'll be conservative and leave it in for now.
			stage_clear_hazards(true);
		}
	}

	if(boss_is_dying(boss)) {
		float t = (global.frames - boss->current->endtime)/(float)BOSS_DEATH_DELAY + 1;
		tsrand_fill(6);

		PARTICLE(
			.texture = "petal",
			.pos = boss->pos,
			.rule = asymptotic,
			.draw_rule = Petal,
			.color = rgba(0.1+sin(10*t),0.1+cos(10*t),0.5,t),
			.args = {
				sign(anfrand(5))*(3+t*5*afrand(0))*cexp(I*M_PI*8*t),
				5+I,
				afrand(2) + afrand(3)*I,
				afrand(4) + 360.0*I*afrand(1)
			},
			.flags = PFLAG_DRAWADD,
		);

		if(!extra) {
			if(t == 1) {
				global.shake_view_fade = 0.2;
			} else {
				global.shake_view = 5 * (t + t*t + t*t*t);
			}
		}

		if(t == 1) {
			for(int i = 0; i < 10; ++i) {
				spawn_boss_glow(boss, boss->glowcolor, 60 + 20 * i);
			}

			for(int i = 0; i < 256; i++) {
				tsrand_fill(3);
				PARTICLE("flare", boss->pos, 0, timeout_linear, .draw_rule = Fade,
					.args = {
						60 + 10 * afrand(2),
						(3+afrand(0)*10)*cexp(I*tsrand_a(1))
					},
				);
			}

			PARTICLE("blast", boss->pos, 0, timeout, { 60, 3 }, .draw_rule = GrowFade);
			PARTICLE("blast", boss->pos, 0, timeout, { 70, 2.5 }, .draw_rule = GrowFade);
		}

		play_sound_ex("bossdeath", BOSS_DEATH_DELAY * 2, false);
	}

	if(over) {
		if(global.stage->type == STAGE_SPELL && boss->current->type != AT_Move && boss->current->failtime) {
			stage_gameover();
		}

		for(;;) {
			boss->current++;

			if(boss->current - boss->attacks >= boss->acount) {
				// no more attacks, die
				boss->current = NULL;
				boss_death(pboss);
				break;
			}

			if(boss_should_skip_attack(boss, boss->current)) {
				continue;
			}

			boss_start_attack(boss, boss->current);
			break;
		}
	}
}

void boss_death(Boss **boss) {
	Attack *a = boss_get_final_attack(*boss);
	bool fleed = false;

	if(!a) {
		// XXX: why does this happen?
		log_debug("FIXME: boss had no final attacK?");
	} else {
		fleed = a->type == AT_Move;
	}

	if((*boss)->acount && !fleed) {
		petal_explosion(35, (*boss)->pos);
	}

	if(!fleed) {
		stage_clear_hazards(true);
	}

	free_boss(*boss);
	*boss = NULL;
}

static void free_attack(Attack *a) {
	free(a->name);
}

void free_boss(Boss *boss) {
	del_ref(boss);

	for(int i = 0; i < boss->acount; i++)
		free_attack(&boss->attacks[i]);

	aniplayer_free(&boss->ani);
	free(boss->name);
	free(boss->attacks);
	free(boss);
}

void boss_start_attack(Boss *b, Attack *a) {
	log_debug("%s", a->name);

	StageInfo *i;
	StageProgress *p = get_spellstage_progress(a, &i, true);

	if(p) {
		++p->num_played;

		if(!p->unlocked && !global.plr.continues_used) {
			log_info("Spellcard unlocked! %s: %s", i->title, i->subtitle);
			p->unlocked = true;
		}
	}

	a->starttime = global.frames + (a->type == AT_ExtraSpell? ATTACK_START_DELAY_EXTRA : ATTACK_START_DELAY);
	a->rule(b, EVENT_BIRTH);
	if(ATTACK_IS_SPELL(a->type)) {
		play_sound(a->type == AT_ExtraSpell ? "charge_extra" : "charge_generic");

		for(int i = 0; i < 10+5*(a->type == AT_ExtraSpell); i++) {
			tsrand_fill(4);

			PARTICLE(
				.texture = "stain",
				.pos = VIEWPORT_W/2 + VIEWPORT_W/4*anfrand(0)+I*VIEWPORT_H/2+I*anfrand(1)*30,
				.color = rgb(0.2,0.3,0.4),
				.rule = timeout_linear,
				.draw_rule = GrowFade,
				.args = { 50, sign(anfrand(2))*10*(1+afrand(3)) },
				.flags = PFLAG_DRAWADD,
			);
		}

		// schedule a bomb cancellation for when the spell actually starts
		// we don't want an ongoing bomb to immediately ruin the spell bonus
		player_cancel_bomb(&global.plr, a->starttime - global.frames);
	}

	stage_clear_hazards(true);
}

Attack* boss_add_attack(Boss *boss, AttackType type, char *name, float timeout, int hp, BossRule rule, BossRule draw_rule) {
	boss->attacks = realloc(boss->attacks, sizeof(Attack)*(++boss->acount));
	Attack *a = &boss->attacks[boss->acount-1];
	memset(a, 0, sizeof(Attack));

	boss->current = &boss->attacks[0];

	a->type = type;
	a->name = strdup(name);
	a->timeout = timeout * FPS;

	a->maxhp = hp;
	a->hp = hp;

	a->rule = rule;
	a->draw_rule = draw_rule;

	a->starttime = global.frames;

	// FIXME: figure out a better value/formula, i pulled this out of my ass
	a->scorevalue = 2000.0 + hp * 0.6;

	if(a->type == AT_ExtraSpell) {
		a->scorevalue *= 1.25;
	}

	return a;
}

void boss_generic_move(Boss *b, int time) {
	if(b->current->info->pos_dest != BOSS_NOMOVE) {
		GO_TO(b, b->current->info->pos_dest, 0.1)
	}
}

Attack* boss_add_attack_from_info(Boss *boss, AttackInfo *info, char move) {
	if(move) {
		boss_add_attack(boss, AT_Move, "Generic Move", 1, 0, boss_generic_move, NULL)->info = info;
	}

	Attack *a = boss_add_attack(boss, info->type, info->name, info->timeout, info->hp, info->rule, info->draw_rule);
	a->info = info;
	return a;
}

void boss_preload(void) {
	preload_resources(RES_SFX, RESF_OPTIONAL,
		"charge_generic",
		"spellend",
		"spellclear",
		"timeout1",
		"timeout2",
		"bossdeath",
	NULL);

	preload_resources(RES_TEXTURE, RESF_DEFAULT,
		"boss_spellcircle0",
		"boss_circle",
		"boss_indicator",
		"part/boss_shadow",
		"part/arc",
	NULL);

	preload_resources(RES_SHADER, RESF_DEFAULT,
		"boss_zoom",
		"spellcard_intro",
		"spellcard_outro",
		"spellcard_walloftext",
		"silhouette",
	NULL);

	StageInfo *s = global.stage;

	if(s->type != STAGE_SPELL || s->spell->type == AT_ExtraSpell) {
		preload_resources(RES_TEXTURE, RESF_DEFAULT,
			"stage3/wspellclouds",
			"stage4/kurumibg2",
		NULL);
	}
}
