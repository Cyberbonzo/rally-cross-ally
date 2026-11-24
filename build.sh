#!/usr/bin/env bash
set -euo pipefail
# Run in the parent of app/ and RallyCross.chd, inside a Linux x86-64 build environment.
port=$(cd "$(dirname "$0")" && pwd)
root=$PWD
disc="$root/RallyCross.chd"
test -f "$disc"
expected=62c54f5e1f989126a1f034c39646a0336c6c8321
actual=$(chdman info -i "$disc" | awk '$1=="SHA1:" {print $2}')
test "$actual" = "$expected" || { echo "Rally Cross USA CHD SHA1 mismatch" >&2; exit 1; }
if [ ! -d source/.git ]; then
    git clone https://github.com/tyrannotorus/emulator-deadstation-1-recomp-rally-cross.git source
    git -C source checkout "$(cat "$port/upstream-commit")"
fi
cd source
test "$(git rev-parse HEAD)" = "$(cat "$port/upstream-commit")"
git submodule update --init sdk
git -C sdk submodule update --init psxrecomp
git -C sdk/psxrecomp submodule update --init --recursive
sdk/tools/prepare-psxrecomp.sh
if ! git -C sdk apply --reverse --check "$port/sdk-ally.patch" 2>/dev/null; then
    git -C sdk apply "$port/sdk-ally.patch"
fi
mkdir -p ally/vk-include disc
cp "$port/panel.cpp" ally/
ln -sfn /usr/include/vulkan ally/vk-include/vulkan
ln -sfn /usr/include/vk_video ally/vk-include/vk_video
if [ ! -f disc/rallycross.cue ]; then
    chdman extractcd -i "$disc" -o disc/rallycross.cue -sb
fi
python3 sdk/psxrecomp/tools/new_project_layout/probe_disc.py disc/rallycross.cue --write-boot-exe disc --out-dir disc --disc-rel disc/rallycross.cue
cmake -S sdk/psxrecomp/recompiler -B build-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_ENABLE_CHD=OFF
cmake --build build-tools --target psxrecomp-game -j "${JOBS:-5}"
build-tools/psxrecomp-game --config "$PWD/game.toml"
cmake -S . -B build-ally -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DPSX_SDL_BACKEND=SDL2 -DPSX_RECOMP_UI=OFF -DPSX_NETPLAY=OFF -DPSX_REWIND=OFF -DPSX_ENABLE_VULKAN=OFF \
    -DPSX_ZLIB_FETCH=OFF -DPSX_DEBUG_TOOLS=OFF -DPSX_STATIC_RUNTIME=OFF -DPSXRECOMP_BIOS_STEMS=OpenBIOS \
    -DDEADSTATION_VULKAN_INCLUDE="$PWD/ally/vk-include"
cmake --build build-ally --target psx-runtime -j "${JOBS:-5}"
mkdir -p "$root/app/bios" "$root/app/mods"
install -m755 build-ally/Rally_Cross_recomp_module "$root/app/Rally_Cross_recomp_module.new"
mv -f "$root/app/Rally_Cross_recomp_module.new" "$root/app/Rally_Cross_recomp_module"
cp module.cfg game.toml pipelines.keys LICENSE "$root/app/"
cp sdk/psxrecomp/bios/{openbios.bin,OpenBIOS.LICENSE} "$root/app/bios/"
cp -r build-ally/mods/bundled "$root/app/mods/"
install -m755 "$port/jogar.sh" "$root/jogar.sh"
