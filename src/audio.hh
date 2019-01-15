/*
    Copyright 2018-2019 Julius Ikkala

    This file is part of CafeFM.

    CafeFM is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CafeFM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CafeFM.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CAFE_AUDIO_HH
#define CAFE_AUDIO_HH
#include "portaudio.h"
#include "instrument.hh"
#include "encoder.hh"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <thread>

class audio_output
{
public:
    explicit audio_output(uint64_t samplerate = 44100);
    ~audio_output();

    void open(
        double target_latency = 0.030,
        int system_index = -1,
        int device_index = -1
    );
    void close();
    void start();
    void stop();
    // This must be called only when audio output is stopped!
    void set_instrument(instrument& i);

    void start_recording(
        encoder::format fmt = encoder::WAV,
        double quality = 90,
        double max_recording_length = 30*60 // In seconds
    );
    void stop_recording();
    void abort_encoding();
    bool is_recording() const;
    bool is_encoding() const;
    void get_encoding_progress(uint64_t& num, uint64_t& denom) const;
    // This will throw if encoding hasn't run or was aborted.
    const encoder& get_encoder() const;

    enum loop_state
    {
        LOOP_UNUSED = 0,
        LOOP_MUTED,
        LOOP_PLAYING,
        LOOP_RECORDING
    };

    void reset_loops(size_t max_count = 8, double max_loop_length = 30);
    unsigned get_loop_count() const;
    void set_loop_bpm(double bpm = 120);
    double get_loop_bpm() const;
    // Used for visualizing BPM.
    double get_loop_beat_index() const;
    void set_loop_volume(unsigned loop_index, double volume);
    void record_loop(unsigned loop_index);
    void finish_loop(unsigned loop_index);
    void play_loop(unsigned loop_index, bool play = true);
    void clear_loop(unsigned loop_index);
    void clear_all_loops();
    loop_state get_loop_state(unsigned loop_index) const;
    // Length is in beats according to BPM.
    double get_loop_length(unsigned loop_index) const;

    uint64_t get_samplerate() const;

    static std::vector<const char*> get_available_systems();
    static std::vector<const char*> get_available_devices(
        int system_index
    );
    static std::vector<uint64_t> get_available_samplerates(
        int system_index, int device_index, double target_latency = 0.030
    );

private:
    friend class encoder;

    void open_stream(
        double target_latency,
        int system_index,
        int device_index,
        PaStreamCallback* callback,
        void* userdata
    );

    void handle_recording();
    void handle_encoding();

    static int stream_callback(
        const void* input,
        void* output,
        unsigned long framecount,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* data
    );

    uint64_t samplerate;
    instrument* ins;
    PaStream *stream;

    std::atomic_bool record, encode;

    struct
    {
        std::vector<int32_t> data;
        std::atomic_uint64_t head;
        uint64_t tail; // Tail is only used in one thread, so no atomic needed.
    } ring_buffer;

    mutable std::mutex recording_mutex;
    std::condition_variable recording_cv;

    std::vector<int32_t> raw_recording;
    unsigned encode_head;
    size_t total_recorded_samples;
    size_t max_recording_samples;

    std::unique_ptr<encoder> enc;
    std::unique_ptr<std::thread> recording_thread;

    struct loop
    {
        loop_state state;

        uint64_t volume_num, volume_denom;
        uint64_t start_t;
        uint64_t length;
        int64_t record_stop_timer;
        size_t sample_count;
        int32_t* samples;
    };
    uint64_t beat_length;
    uint64_t loop_t;
    std::vector<int32_t> loop_samples;
    std::vector<loop> loops;
};

#endif
