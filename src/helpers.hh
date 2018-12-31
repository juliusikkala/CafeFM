#ifndef CAFEFM_HELPERS_HH
#define CAFEFM_HELPERS_HH
#include <cstring>
#include <vector>
#include <utility>
#include <string>

inline double lerp(double a, double b, double t) { return (1 - t) * a + t * b; }

int find_string_arg(
    const char* str,
    const char* const* strings,
    unsigned string_count
);

std::string generate_semitone_name(int semitone);
std::vector<std::string> generate_note_list(
    int min_semitone, int max_semitone
);

#endif
