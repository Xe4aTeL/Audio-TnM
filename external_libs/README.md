This folder should contain precompiled libraries:
- ffmpeg
- libsodium
- whisper.cpp
- wxWidgets

The structure is:
```text
external_libs\
├──ffmpeg\
│   ├──bin\
│   │   └──(*.lib | *.dll)\
│   ├──include\
│   ├──lib\
│   ├──share\
├──libsodium\
│   ├──include\
│   ├──x64/Debug/v145/dynamic\
├──whisper-cpp\
│   ├──bin\
│   ├──include\
│   ├──lib\
├──wxWidgets\
│   ├──bin\
│   ├──build\
│   ├──include\
│   ├──lib\
│   │   └──(vc_x64_dll/(mswu/ | mswud/ | *u.lib | *ud.lib | *u.dll | *ud.dll))\
```
All of the libraries are dynamically linked.

Before compiling FFMPEG, make sure to compile [libmp3lame](https://lame.sourceforge.io/) and [libopus](https://opus-codec.org/) as stated in main.cpp
to be able to pass all the tags needed. For this project, ffmpeg was built with MSYS shell via:
- booting x64 Native Tools Command Prompt (comes with MSVC / BuildTools)
- C:\\msys64\\msys2_shell.cmd -defterm -here -no-start -msys -use-full-path
- pacman -S make diffutils pkg-config (nasm)
- ^ here nasm can be installed separately on Windows itself and be included in %PATH% (was actually build this way)
- export PATH="$\(dirname "$(which cl)"\):$PATH"
- ^ else there is a conflict with MSYS's linker
- cd /path/to/libmp3lame
- ^ edit Makefile.MSVC if using 3.100 version, as it hardcodes x86 compilation in it via: /machine:I386 to /machine:X64
- nmake -f Makefile.MSVC MSVCVER=Win64
- ^ put both .lib and lame.h into C:/ffmpeglink/lame/{include/lame/.h | lib/.lib}
- cd /path/to/opus
- cmake
- ^ put in C:/ffmpeglink/opus/{include/opus | lib}
- cd /path/to/ffmpeg
- ./configure --toolchain=msvc --arch=x86_64 --target-os=win64 --enable-shared --disable-static --prefix=./build_msvc --enable-libmp3lame --enable-libopus --extra-cflags="-I/c/ffmpeglink/lame/include -I/c/ffmpeglink/opus/include" --extra-ldflags="-LIBPATH:/c/ffmpeglink/lame/lib -LIBPATH:/c/ffmpeglink/opus/lib"
- make -j$(nproc)
- make install
- move everything needed from ./build_msvc to external_libs/ffmpeg 

The rest of the libraries is easily built with CMAKE alone (use cmake-gui if you are a noob like me). Don't forget to set each library in shared build.

The whisper-cpp used AVX2 and Nvidia CUDA checks to enable hardware acceleration and GPU support in the program itself. For compiling, you will need to install CUDA Toolkit as well.

Miniaduio is also one of the dependencies of this project. Install it in /utils/ directly (.h and .c).

NOTE: libsodium is the only library that I was lazy to compile by hand, because libsodium.org provides precompiled for MSVC via this [link](https://download.libsodium.org/libsodium/releases/). I went for 1.0.22 stable msvc.