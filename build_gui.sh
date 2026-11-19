#!/bin/bash
gcc -Wall -O2 -Wextra -Igui gui/*.c -o gui.exe $(pkg-config --cflags --libs gtk4)
