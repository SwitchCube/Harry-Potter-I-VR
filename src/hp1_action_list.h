#pragma once
// Explicit application/manifest contract, checked before build and VR launch.
struct Hp1ActionSpec {const char* key;const char* type;};
static constexpr Hp1ActionSpec kHp1Actions[]={
    {"move","vector2"},
    {"turn","vector2"},
    {"cast","boolean"},
    {"jump","boolean"},
    {"menu","boolean"},
    {"confirm","boolean"},
    {"recenter","boolean"},
    {"right_grip","pose"},
    {"right_hand","pose"},
    {"left_grip","pose"},
    {"show_health","boolean"},
};
