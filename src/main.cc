#include <cstdio>
#include "fm.hh"

#define COUNT 32
int main()
{
    static_fm_synth<OSC_TRIANGLE, OSC_SQUARE> fm;
    fm.set_frequency(0, 1, 32);
    fm.set_period(1, 0.5);
    fm.set_amplitude(1, 0.5);
    int32_t* buffer = new int32_t[COUNT];
    fm.synthesize(buffer, COUNT);
    for(int i = 0; i < COUNT; ++i)
    {
        printf("%f\n", buffer[i]/((double)(1l<<31)));
    }
    delete [] buffer;
    return 0;
}
