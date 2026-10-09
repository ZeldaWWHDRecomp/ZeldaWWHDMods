#pragma once
#include <cstdint>
namespace dragon::song {
void start();
void tick(unsigned);
void stop();
// Returns the original buffer unchanged when disabled or inactive.
const int16_t* mix(const int16_t*, int frames);
}
