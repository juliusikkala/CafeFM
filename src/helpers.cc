#include "helpers.hh"

namespace
{
    int div_round_down(int a, int b)
    {
        return a/b + ((a%b) >> 31);
    }
}

int find_string_arg(
    const char* str,
    const char* const* strings,
    unsigned string_count
){
    for(unsigned i = 0; i < string_count; ++i)
        if(strcmp(str, strings[i]) == 0) return i;
    return -1;
}

std::string generate_semitone_name(int semitone)
{
    int octave_number = 5+div_round_down(semitone-3, 12);
    const char* names[12] = {
        "A", "A♯/B♭", "B", "C", "C♯/D♭", "D",
        "D♯/E♭", "E", "F", "F♯/G♭", "G", "G♯/A♭"
    };
    std::string name = names[(semitone%12+12)%12] + std::to_string(octave_number);
    return name;
}

std::vector<std::string> generate_note_list(
    int min_semitone, int max_semitone
){
    std::vector<std::string> res;
    for(; min_semitone < max_semitone; ++min_semitone)
        res.push_back(generate_semitone_name(min_semitone));
    return res;
}
