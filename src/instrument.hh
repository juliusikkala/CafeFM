#ifndef CAFEFM_INSTRUMENT_HH
#define CAFEFM_INSTRUMENT_HH
#include <vector>
#include <cstdint>

struct envelope
{
    void set_volume(
        double peak_volume,
        double sustain_volume,
        int64_t denom = 65536
    );

    void set_curve(
        double attack_length,
        double decay_length,
        double release_length,
        uint64_t samplerate
    );

    envelope convert(
        uint64_t cur_samplerate,
        uint64_t new_samplerate
    ) const;

    int64_t peak_volume_num;
    int64_t sustain_volume_num; // Set to 0 for no sustain
    int64_t volume_denom;

    uint64_t attack_length;
    uint64_t decay_length;
    uint64_t release_length;
};

class instrument
{
public:
    instrument(uint64_t samplerate);
    virtual ~instrument();

    using voice_id = unsigned;

    void set_tuning(double base_frequency = 440.0);
    double get_tuning() const;

    uint64_t get_samplerate() const;

    voice_id press_voice(int semitone);
    void press_voice(voice_id id, int semitone);
    void release_voice(voice_id id);

    void set_polyphony(unsigned n = 16);
    unsigned get_polyphony() const;

    void set_envelope(const envelope& adsr);
    envelope get_envelope() const;

    void set_volume(double volume);
    void set_max_safe_volume();
    double get_volume() const;

    void copy_state(const instrument& other);

    virtual void synthesize(int32_t* samples, unsigned sample_count) = 0;

protected:
    struct voice
    {
        bool enabled;
        bool pressed;
        uint64_t press_timer;
        uint64_t release_timer;
        int semitone;
    };

    double get_frequency(voice_id id) const;
    // If this is too slow, consider generating a table from the envelope
    void get_voice_volume(voice_id id, int64_t& num, int64_t& denom);
    void step_voices();

    virtual void refresh_voice(voice_id id) = 0;
    void refresh_all_voices();
    virtual void handle_polyphony(unsigned n) = 0;

private:
    std::vector<voice> voices;
    envelope adsr;
    double base_frequency;
    double volume;
    uint64_t samplerate;
};

#endif
