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
#include "encoder.hh"
#include <algorithm>
#include <cstring>

sf_count_t enc_get_filelen(void* userdata)
{
    encoder* enc = static_cast<encoder*>(userdata);
    return enc->data.size();
}

sf_count_t enc_seek(sf_count_t offset, int whence, void* userdata)
{
    encoder* enc = static_cast<encoder*>(userdata);
    switch(whence)
    {
    case SEEK_CUR:
        enc->pos += offset;
        break;
    case SEEK_SET:
        enc->pos = offset;
        break;
    case SEEK_END:
        enc->pos = enc->data.size() + offset;
        break;
    default:
        break;
    }
    return enc->pos;
}

sf_count_t enc_read(void* ptr, sf_count_t count, void* userdata)
{
    encoder* enc = static_cast<encoder*>(userdata);
    unsigned actual = std::min(enc->data.size() - enc->pos, (size_t)count);
    memcpy(ptr, enc->data.data() + enc->pos, actual);
    enc->pos += actual;
    return actual;
}

sf_count_t enc_write(const void* ptr, sf_count_t count, void* userdata)
{
    encoder* enc = static_cast<encoder*>(userdata);
    unsigned min_len = enc->pos + count;
    if(enc->data.size() < min_len) enc->data.resize(min_len);
    memcpy(enc->data.data() + enc->pos, ptr, count);
    enc->pos += count;
    return count;
}

sf_count_t enc_tell(void* userdata)
{
    encoder* enc = static_cast<encoder*>(userdata);
    return enc->pos;
}

encoder::encoder(uint64_t samplerate, format fmt, double quality)
: fmt(fmt), pos(0)
{
    static constexpr int sf_formats[] = {
        SF_FORMAT_WAV,
        SF_FORMAT_OGG,
        SF_FORMAT_FLAC
    };

    io.get_filelen = enc_get_filelen;
    io.seek = enc_seek;
    io.read = enc_read;
    io.write = enc_write;
    io.tell = enc_tell;

    SF_INFO info;
    memset(&info, 0, sizeof(info));
    info.samplerate = samplerate;
    info.channels = 1;
    info.format = sf_formats[(int)fmt];

    switch(fmt)
    {
    case WAV:
        if(quality >= 90) info.format |= SF_FORMAT_PCM_32;
        else if(quality >= 50) info.format |= SF_FORMAT_PCM_24;
        else info.format |= SF_FORMAT_PCM_16;
        break;
    case FLAC:
        if(quality >= 50) info.format |= SF_FORMAT_PCM_24;
        else if(quality >= 10) info.format |= SF_FORMAT_PCM_16;
        else info.format |= SF_FORMAT_PCM_S8;
        break;
    case OGG:
        info.format |= SF_FORMAT_VORBIS;
        break;
    }

    file = sf_open_virtual(&io, SFM_WRITE, &info, this);

    if(fmt == OGG || fmt == FLAC)
    {
        quality /= 100.0;
        sf_command(
            file,
            SFC_SET_VBR_ENCODING_QUALITY,
            &quality,
            sizeof(quality)
        );
    }
    sf_set_string(file, SF_STR_TITLE, "CaféFM Recording");
    sf_set_string(file, SF_STR_SOFTWARE, "CaféFM");
}

encoder::~encoder()
{
    finish();
}

size_t encoder::write(int32_t* samples, size_t count)
{
    auto r = sf_writef_int(file, samples, count); 
    return r;
}

void encoder::finish()
{
    if(file)
    {
        sf_close(file);
        file = nullptr;
    }
}

encoder::format encoder::get_format() const
{
    return fmt;
}

size_t encoder::get_data_size() const
{
    return data.size();
}

const uint8_t* encoder::get_data() const
{
    return data.data();
}
