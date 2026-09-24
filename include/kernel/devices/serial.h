#pragma once

#include <devices/io.h>
#include <types.h>

namespace serial {

constexpr ui16 kCom1 = 0x3f8;

void initialize();
void write(char character);
void write(const char* message);

} // namespace serial
