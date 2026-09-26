#pragma once

#include <types.h>

inline char* int2str(ui64 src) {
    static char buffer[21];
    ui8 length = 0;

    do {
        buffer[length++] = static_cast<char>('0' + src % 10);
        src /= 10;
    } while (src != 0);

    for (ui8 left = 0, right = length - 1; left < right; ++left, --right) {
        char digit = buffer[left];
        buffer[left] = buffer[right];
        buffer[right] = digit;
    }
    buffer[length] = '\0';
    return buffer;
}
