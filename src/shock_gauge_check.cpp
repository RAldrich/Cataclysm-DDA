#include "shock_gauge_check.h"

#include <algorithm>

#include "calendar.h"
#include "character.h"
#include "messages.h"
#include "rng.h"
#include "shock_gauge.h"
#include "skill.h"
#include "translations.h"

static const skill_id skill_gun( "gun" );
static const skill_id skill_melee( "melee" );
static const skill_id skill_unarmed( "unarmed" );

const efftype_id effect_violence_desensitization( "violence_desensitization" );
const efftype_id effect_violence_trauma( "violence_trauma" );

// Your skill for dealing with the stress of inflicting violence is
// the best of your general combat skills.
int compute_violence_defense( const Character &who )
{
    const float best = std::max( { who.get_skill_level( skill_melee ),
                                   who.get_skill_level( skill_unarmed ),
                                   who.get_skill_level( skill_gun ) } );
    return static_cast<int>( best );
}

void trigger_attack_stress_check( Character &who, int event_rank )
{
    shock_gauge &gauge = who.get_shock_gauge();

    // If you're desensitized to this degree of stress, you don't have to make a
    // check, you've already dealt with this shit (or worse).
    if( gauge.violence_desensitization >= event_rank ) {
        return;
    }

    // Per-axis cooldown: checks fire at most once every shock_check_cooldown.
    if( gauge.last_shock_check != calendar::turn_zero &&
        calendar::turn - gauge.last_shock_check < shock_check_cooldown ) {
        return;
    }

    gauge.last_shock_check = calendar::turn;

    const int defense = compute_violence_defense( who );

    // This is completely open to tuning. d4/6/8/10/12 vs defense seemed like a fine place to start.
    const int difficulty_die = event_rank * 2 + 2;
    const bool passed = rng( 0, difficulty_die ) <= defense;

    if( passed ) {
        gauge.add_violence_desensitization( 1 );
        if( who.is_avatar() ) {
            add_msg( m_good, _( "You steel yourself." ) );
        }
    } else {
        gauge.add_violence_trauma( 1 );
        if( who.is_avatar() ) {
            add_msg( m_bad, _( "The violence shakes you." ) );
        }
    }

    refresh_shock_gauge_effects( who );
}

static void sync_gauge_effect( Character &who, const efftype_id &id, int notches )
{
    if( notches > 0 ) {
        // Permanent effect (duration ignored); force intensity to the notch
        // count and override any existing application.
        who.add_effect( id, 1_turns, false, notches, true );
    } else if( who.has_effect( id ) ) {
        who.remove_effect( id );
    }
}

void refresh_shock_gauge_effects( Character &who )
{
    const shock_gauge &gauge = who.get_shock_gauge();
    sync_gauge_effect( who, effect_violence_desensitization,
                       gauge.violence_desensitization );
    sync_gauge_effect( who, effect_violence_trauma, gauge.violence_trauma );
}
