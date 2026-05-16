#include "alsa_mixer.h"

int alsa_mixer_init(void)
{
    int ret1, ret2;
    
    //其实我应该用libasound的，但我不会调用所以就用system()了
    ret1 = system("amixer sset 'Left Input Mixer MIC1 Boost' on 2>/dev/null");
    ret2 = system("amixer sset 'MIC1 gain volume' 24 2>/dev/null");

    if (ret1 == 0 && ret2 == 0) {
        ALSA_MIXER_DEBUG("Mic initialized via amixer");
        return 0;
    } else {
        ALSA_MIXER_DEBUG("Failed to set mic gain (ret1=%d, ret2=%d)", ret1, ret2);
        return -1;
    }
}