#pragma once
#ifndef CATA_SRC_SHOCK_GAUGE_H
#define CATA_SRC_SHOCK_GAUGE_H

#include "calendar.h"

class JsonObject;
class JsonOut;

// Psychological stress gauge. MVP covers one axis (Violence) with paired
// desensitization (passed-roll) and trauma (failed-roll) counters, plus a
// cooldown timestamp that rate-limits how often checks may fire.
//
// The full design incldues four axes (Violence, Helplessness, Isolation,
// Unnatural); this struct leaves room for adding the others later without
// changing the containing Character field.
struct shock_gauge {
    int violence_desensitization = 0;   // 0..5; incremented on passed rolls
    int violence_trauma = 0;            // 0..5; incremented on failed rolls
    time_point last_shock_check =
        calendar::turn_zero; // To avoid spamming checks and entering a death spiral

    // Clamp to [0, 5] but this is arbitrary, the scale can be changed
    void add_violence_desensitization( int n );
    void add_violence_trauma( int n );

    void serialize( JsonOut &jsout ) const;
    void deserialize( const JsonObject &jo );
};

#endif // CATA_SRC_SHOCK_GAUGE_H
