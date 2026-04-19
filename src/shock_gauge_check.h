#pragma once
#ifndef CATA_SRC_SHOCK_GAUGE_CHECK_H
#define CATA_SRC_SHOCK_GAUGE_CHECK_H

#include "calendar.h"
#include "type_id.h"

class Character;

extern const efftype_id effect_violence_desensitization;
extern const efftype_id effect_violence_trauma;

// Shock gauge check free functions. MVP covers one axis (Violence)
// hooked off every melee or ranged attack, rate-limited by the cooldown below.

constexpr time_duration shock_check_cooldown = 2_minutes;

// Violence defense = max of the character's three core combat skills.
// A specialist is just as ready for bloodshed as a generalist.
int compute_violence_defense( const Character &who );

// Runs the per-attack shock check for an event of the given rank.
// Gated by (1) desensitization >= event_rank, and (2) a per-axis
// cooldown. Emits a player message on roll resolution and refreshes
// the display effect.
void trigger_attack_stress_check( Character &who, int event_rank );

// Syncs violence_desensitization / violence_trauma effect intensities
// with the underlying gauge counters. Called after any counter change.
void refresh_shock_gauge_effects( Character &who );

#endif // CATA_SRC_SHOCK_GAUGE_CHECK_H
