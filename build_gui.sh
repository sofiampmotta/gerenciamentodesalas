#!/bin/bash
gcc -Wall -Igui gui/*.c -o gui.exe $(pkg-config --cflags --libs gtk4)
