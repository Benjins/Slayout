@echo off

cl /Od /Zi src/win32_main.cpp /Feslayout_viewer kernel32.lib Gdi32.lib user32.lib