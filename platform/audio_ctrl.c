#include "audio_ctrl.h"

void audio_enable(bool b) {
    if(b) system("echo 1 > /dev/spk_crtl");
    else system("echo 0 > /dev/spk_crtl");
}

