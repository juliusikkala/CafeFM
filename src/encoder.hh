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
#ifndef CAFE_ENCODER_HH
#define CAFE_ENCODER_HH
#include <cstdint>
#include <cstddef>
#include <vector>
#include <sndfile.h>

class encoder
{
friend sf_count_t enc_get_filelen(void*);
friend sf_count_t enc_seek(sf_count_t, int, void*);
friend sf_count_t enc_read(void*, sf_count_t, void*);
friend sf_count_t enc_write(const void*, sf_count_t, void*);
friend sf_count_t enc_tell(void*);

public:
    enum format
    {
        WAV = 0,
        OGG,
        FLAC
    };
    static constexpr const char* const format_strings[] = {
        "WAV",
        "OGG",
        "FLAC"
    };

    encoder(
        uint64_t samplerate,
        format fmt,
        double quality
    );
    ~encoder();

    size_t write(int32_t* samples, size_t count);
    void finish();
    format get_format() const;
    size_t get_data_size() const;
    const uint8_t* get_data() const;

private:
    format fmt;
    std::vector<uint8_t> data;
    size_t pos;
    SF_VIRTUAL_IO io;
    SNDFILE* file;
};

#endif
