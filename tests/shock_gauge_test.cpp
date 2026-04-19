#include <sstream>
#include <string>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "character.h"
#include "json.h"
#include "json_loader.h"
#include "player_helpers.h"
#include "rng.h"
#include "shock_gauge.h"
#include "shock_gauge_check.h"
#include "skill.h"

static const skill_id skill_gun_id( "gun" );
static const skill_id skill_melee_id( "melee" );
static const skill_id skill_unarmed_id( "unarmed" );

TEST_CASE( "shock_gauge_default_state", "[shock_gauge]" )
{
    shock_gauge g;
    CHECK( g.violence_desensitization == 0 );
    CHECK( g.violence_trauma == 0 );
    CHECK( g.last_shock_check == calendar::turn_zero );
}

TEST_CASE( "shock_gauge_desensitization_clamps_at_five", "[shock_gauge]" )
{
    shock_gauge g;
    g.add_violence_desensitization( 1 );
    CHECK( g.violence_desensitization == 1 );
    g.add_violence_desensitization( 10 );
    CHECK( g.violence_desensitization == 5 );
    g.add_violence_desensitization( -2 );
    CHECK( g.violence_desensitization == 3 );
    g.add_violence_desensitization( -100 );
    CHECK( g.violence_desensitization == 0 );
}

TEST_CASE( "shock_gauge_trauma_clamps_at_five", "[shock_gauge]" )
{
    shock_gauge g;
    g.add_violence_trauma( 1 );
    CHECK( g.violence_trauma == 1 );
    g.add_violence_trauma( 10 );
    CHECK( g.violence_trauma == 5 );
    g.add_violence_trauma( -3 );
    CHECK( g.violence_trauma == 2 );
    g.add_violence_trauma( -100 );
    CHECK( g.violence_trauma == 0 );
}

TEST_CASE( "shock_gauge_json_round_trip", "[shock_gauge]" )
{
    shock_gauge written;
    written.add_violence_desensitization( 2 );
    written.add_violence_trauma( 4 );
    written.last_shock_check = calendar::turn_zero + 150_turns;

    std::ostringstream os;
    JsonOut jsout( os );
    jsout.write( written );

    JsonValue jsin = json_loader::from_string( os.str() );
    shock_gauge read;
    REQUIRE( jsin.read( read ) );

    CHECK( read.violence_desensitization == 2 );
    CHECK( read.violence_trauma == 4 );
    CHECK( read.last_shock_check == written.last_shock_check );
}

TEST_CASE( "shock_gauge_missing_key_tolerance", "[shock_gauge]" )
{
    // Loading from an empty object must leave defaults intact — this is the
    // backward-compat path for old saves that pre-date the shock_gauge field.
    JsonValue jsin = json_loader::from_string( "{}" );
    shock_gauge read;
    read.add_violence_desensitization( 3 );
    read.add_violence_trauma( 5 );
    // Deserialize from empty object; pre-set values must stay untouched.
    REQUIRE( jsin.read( read ) );
    CHECK( read.violence_desensitization == 3 );
    CHECK( read.violence_trauma == 5 );
}

// -----------------------------------------------------------------------------
// Phase 2: per-attack stress check
// -----------------------------------------------------------------------------

// Helper: reset avatar state and put all three combat skills at a known level.
static Character &prepare_dummy_for_check( int melee, int unarmed, int gun )
{
    clear_avatar();
    Character &guy = get_player_character();
    guy.set_skill_level( skill_melee_id, melee );
    guy.set_skill_level( skill_unarmed_id, unarmed );
    guy.set_skill_level( skill_gun_id, gun );
    guy.get_shock_gauge() = shock_gauge{};
    calendar::turn = calendar::turn_zero + 1_days;
    return guy;
}

// Seed rng so that the next `rng(0, 6)` draw is strictly greater than zero —
// i.e. a defense-0 character will fail the violence check. Deterministic:
// iterates seeds starting at 200000 and runs through the same sequence on
// every invocation. (Small seeds are pathological for std::minstd_rand0 on
// this range: 16807 = 7^5, so the first draw of `rng(0, 6)` is always 0 for
// seeds <= ~127773 where the multiply doesn't yet wrap modulo 2^31-1.)
static void seed_for_violence_fail()
{
    for( unsigned s = 200000; s < 200256; ++s ) {
        rng_set_engine_seed( s );
        if( rng( 0, 6 ) > 0 ) {
            rng_set_engine_seed( s );
            return;
        }
    }
    FAIL( "no seed in [200000,200256) produced a non-zero rng(0, 6)" );
}

TEST_CASE( "shock_gauge_compute_violence_defense_is_max_of_combat_skills",
           "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 6, 0 );
    CHECK( compute_violence_defense( guy ) == 6 );

    guy.set_skill_level( skill_melee_id, 4 );
    guy.set_skill_level( skill_unarmed_id, 0 );
    guy.set_skill_level( skill_gun_id, 2 );
    CHECK( compute_violence_defense( guy ) == 4 );

    guy.set_skill_level( skill_melee_id, 0 );
    guy.set_skill_level( skill_unarmed_id, 0 );
    guy.set_skill_level( skill_gun_id, 0 );
    CHECK( compute_violence_defense( guy ) == 0 );
}

TEST_CASE( "shock_gauge_trigger_desensitizes_on_guaranteed_pass", "[shock_gauge]" )
{
    // Defense 6 satisfies rng(0, 6) <= 6 for every draw, so no seed is needed.
    Character &guy = prepare_dummy_for_check( 0, 6, 0 );

    trigger_attack_stress_check( guy, 2 );

    CHECK( guy.get_shock_gauge().violence_desensitization == 1 );
    CHECK( guy.get_shock_gauge().violence_trauma == 0 );
    CHECK( guy.get_shock_gauge().last_shock_check == calendar::turn );
}

TEST_CASE( "shock_gauge_trigger_traumatizes_on_seeded_fail", "[shock_gauge]" )
{
    // Defense 0 vs rng(0, 6). We need a seed whose first draw is > 0.
    // The helper walks seeds deterministically to find one.
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    seed_for_violence_fail();

    trigger_attack_stress_check( guy, 2 );

    CHECK( guy.get_shock_gauge().violence_trauma == 1 );
    CHECK( guy.get_shock_gauge().violence_desensitization == 0 );
    CHECK( guy.get_shock_gauge().last_shock_check == calendar::turn );
}

TEST_CASE( "shock_gauge_cooldown_blocks_rapid_checks", "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    seed_for_violence_fail();
    trigger_attack_stress_check( guy, 2 );
    REQUIRE( guy.get_shock_gauge().violence_trauma == 1 );
    const time_point first_stamp = guy.get_shock_gauge().last_shock_check;

    // Advance well under the 2-minute cooldown. Second check must be a no-op.
    calendar::turn = first_stamp + 1_minutes;
    trigger_attack_stress_check( guy, 2 );
    CHECK( guy.get_shock_gauge().violence_trauma == 1 );
    CHECK( guy.get_shock_gauge().last_shock_check == first_stamp );

    // Advance past the cooldown; next check is permitted to fire.
    calendar::turn = first_stamp + 2_minutes + 1_seconds;
    seed_for_violence_fail();
    trigger_attack_stress_check( guy, 2 );
    CHECK( guy.get_shock_gauge().violence_trauma == 2 );
    CHECK( guy.get_shock_gauge().last_shock_check == calendar::turn );
}

TEST_CASE( "shock_gauge_desensitization_gate_short_circuits_check",
           "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    guy.get_shock_gauge().add_violence_desensitization( 2 );
    const time_point stamp_before = guy.get_shock_gauge().last_shock_check;
    // Push calendar well past any cooldown so the only remaining gate is
    // the desensitization short-circuit.
    calendar::turn += 1_days;

    trigger_attack_stress_check( guy, 2 );

    CHECK( guy.get_shock_gauge().violence_desensitization == 2 );
    CHECK( guy.get_shock_gauge().violence_trauma == 0 );
    // Gate short-circuit must not touch the cooldown stamp either.
    CHECK( guy.get_shock_gauge().last_shock_check == stamp_before );
}

TEST_CASE( "shock_gauge_trauma_counter_clamps_at_five", "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    guy.get_shock_gauge().add_violence_trauma( 5 );

    seed_for_violence_fail();
    trigger_attack_stress_check( guy, 2 );

    CHECK( guy.get_shock_gauge().violence_trauma == 5 );
}

// -----------------------------------------------------------------------------
// Phase 3: display effects mirror gauge state
// -----------------------------------------------------------------------------

TEST_CASE( "shock_gauge_fresh_character_has_no_display_effects", "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    CHECK_FALSE( guy.has_effect( effect_violence_desensitization ) );
    CHECK_FALSE( guy.has_effect( effect_violence_trauma ) );
}

TEST_CASE( "shock_gauge_pass_applies_desensitization_effect_at_intensity_1",
           "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 6, 0 );

    trigger_attack_stress_check( guy, 2 );

    REQUIRE( guy.has_effect( effect_violence_desensitization ) );
    CHECK( guy.get_effect_int( effect_violence_desensitization ) == 1 );
    CHECK_FALSE( guy.has_effect( effect_violence_trauma ) );
}

TEST_CASE( "shock_gauge_fail_applies_trauma_effect_at_intensity_1",
           "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    seed_for_violence_fail();

    trigger_attack_stress_check( guy, 2 );

    REQUIRE( guy.has_effect( effect_violence_trauma ) );
    CHECK( guy.get_effect_int( effect_violence_trauma ) == 1 );
    CHECK_FALSE( guy.has_effect( effect_violence_desensitization ) );
}

TEST_CASE( "shock_gauge_refresh_mirrors_gauge_counter", "[shock_gauge]" )
{
    Character &guy = prepare_dummy_for_check( 0, 0, 0 );
    guy.get_shock_gauge().add_violence_trauma( 3 );

    refresh_shock_gauge_effects( guy );

    REQUIRE( guy.has_effect( effect_violence_trauma ) );
    CHECK( guy.get_effect_int( effect_violence_trauma ) == 3 );

    // Clearing the gauge must remove the effect.
    guy.get_shock_gauge() = shock_gauge{};
    refresh_shock_gauge_effects( guy );
    CHECK_FALSE( guy.has_effect( effect_violence_trauma ) );
}
