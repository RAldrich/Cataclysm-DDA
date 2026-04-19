#include "shock_gauge.h"

#include <algorithm>

#include "calendar.h"
#include "json.h"

static constexpr int shock_gauge_max = 5;

static int clamp_shock( int value )
{
    return std::clamp( value, 0, shock_gauge_max );
}

void shock_gauge::add_violence_desensitization( int n )
{
    violence_desensitization = clamp_shock( violence_desensitization + n );
}

void shock_gauge::add_violence_trauma( int n )
{
    violence_trauma = clamp_shock( violence_trauma + n );
}

void shock_gauge::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member( "violence_desensitization", violence_desensitization );
    jsout.member( "violence_trauma", violence_trauma );
    jsout.member( "last_shock_check", last_shock_check );
    jsout.end_object();
}

void shock_gauge::deserialize( const JsonObject &jo )
{
    jo.read( "violence_desensitization", violence_desensitization );
    jo.read( "violence_trauma", violence_trauma );
    jo.read( "last_shock_check", last_shock_check );
}
