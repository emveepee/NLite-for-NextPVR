NLite Client for NextPVR
------------------------

1) Build for linux

Need to install using apt

libsdl2-dev
libsdl2-image-dev
libsdl2-ttf-dev
libcurl4-gnutls-dev
libvlc-dev

To build this software you need git, build-essentials and cmake

git clone https://github.com/emveepee/NLite-for-NextPVR.git
mkdir build
cd build
cmake ..
make

2) To build For Windows

git clone https://github.com/emveepee/NLite-for-NextPVR.git

Download development libraries from these links and install

mkdir SDL2
install libsdl2 https://www.libsdl.org/download-2.0.php

mkdir SDL2_image
install libsdl2_image https://www.libsdl.org/projects/SDL_image/
mkdir SDL2_ttf
install libsdl2_ttf https://www.libsdl.org/projects/SDL_ttf/

mkdir libcurl

libcurl (from https://stackoverflow.com/questions/20171165/getting-libcurl-to-work-with-visual-studio-2013)

    Set environment variables with “C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat” x86.
        Note the path and version.

    Download and extract the compressed libcurl file from download libcurl https://curl.haxx.se/download.html
    inside the extracted libcurl folder.
        run buildconf.bat
        cd into winbuild directory
        Run nmake /f Makefile.vc mode=dll MACHINE=x86 to build. For more information on build options, please refer to BUILD.WINDOWS text file in winbuild folder.

    Go up one directory level and cd into builds folder to find the compiled libcurl.dll and libcurl.lib
mkdir VLC

libvlc

    Download the 7z file from https://www.videolan.org/vlc/download-windows.html
    Extract the sdk folder to the VLC folder


load Nlite.sln into Visual Studio.


Runtime

For runtime set environment VLC_PLUGIN_DIR  to the 32 bit vlc program file plugins folder
