#include "led_ctrl.h"
#include <fcntl.h>
#include <unistd.h>

void led_ctrl(int ctrl) {
    if (ctrl != 0 && ctrl != 1) return;
    int fd = open("/dev/led_ctrl", O_WRONLY);
    if (fd >= 0) {
        char val = (ctrl == 1) ? '1' : '0';
        write(fd, &val, 1);
        close(fd);
    }
}