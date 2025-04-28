# Purpose

This repository contains a proof-of-concept implementation of reliable firmware update process.
The functionality of the bootloader-application pair in this repository mainly relies on source code in submodule FwUpdateLibs.

# application
STM32CubeMX generated project for NUCLEO-F439ZI board.
application/Core/Src/keystore.c - Public key module for accessing generated keys.
application/Core/Src/metadata.c - Application firmware metadata.
application/Core/Src/updateserver.c - Firmware update server using UDP via LwIP.

# bootloader
STM32CubeMX generated project for NUCLEO-F439ZI board.
bootloader/Core/Src/app_status.c - Application binary status information
bootloader/Core/Src/installer.c  - Firmware installer

# License for files not provided by STM32CubeMx or submodules:
MIT License

Copyright (c) 2025 Mikael Penttinen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

THIS LICENSE IS ALSO INCLUDED IN ALL APPROPRIATE FILES
