FROM devkitpro/devkita64:latest
RUN dkp-pacman -Syu --noconfirm && \
    dkp-pacman -S --noconfirm \
    switch-sdl2 switch-sdl2_mixer switch-sdl2_gfx switch-sdl2_image \
    switch-freetype switch-libpng switch-libogg switch-libvorbis \
    switch-libmodplug switch-mpg123 switch-flac libnx
WORKDIR /work
COPY . .
RUN make -j$(nproc) 2>&1
