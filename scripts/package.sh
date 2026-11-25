#!/bin/sh
# Makes a release zip from a release build:
#   ./scripts/package.sh <build dir> <name>      e.g. build-rel-mac macos-universal
# -> dist/Roadkill-<version>-<name>.zip holding Roadkill-<name>/ with the program, an empty
#    data/ folder with instructions, README.txt, LICENSE.txt and THIRD_PARTY.txt.
set -e
cd "$(dirname "$0")/.."
build=$1; name=$2
[ -n "$build" ] && [ -n "$name" ] || { echo "usage: $0 <build dir> <name>"; exit 1; }
exe=$build/roadkill
[ -f "$build/roadkill.exe" ] && exe=$build/roadkill.exe
[ -f "$exe" ] || { echo "package: no roadkill in $build"; exit 1; }
version=${ROADKILL_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo dev)}
dir=dist/Roadkill-$name
rm -rf "$dir" && mkdir -p "$dir/data"
cp "$exe" "$dir/"
[ -f "$build/SDL2.dll" ] && cp "$build/SDL2.dll" "$dir/"
cp packaging/DATA_README.txt "$dir/data/PUT_YOUR_GAME_FILES_HERE.txt"
cp packaging/README.txt "$dir/README.txt"
cp LICENSE "$dir/LICENSE.txt"
{
    echo "Roadkill $version includes:"
    echo
    echo "SDL 2 (https://libsdl.org), zlib license:"
    echo
    sdl=$(find "$build" -path '*sdl2-src/LICENSE.txt' 2>/dev/null | head -1)
    if [ -n "$sdl" ]; then cat "$sdl"; else echo "(linked from the system)"; fi
    echo
    echo "------------------------------------------------------------------------"
    echo "Indeo 3 / Indeo 4 video decoders from FFmpeg 7.1 (https://ffmpeg.org),"
    echo "GNU Lesser General Public License 2.1 or later. Source, with the changes"
    echo "listed: https://github.com/MrMilenko/Roadkill/tree/main/third_party/indeo"
    echo
    cat third_party/indeo/LICENSE
} > "$dir/THIRD_PARTY.txt"
case "$name" in
    windows*) for f in "$dir"/*.txt "$dir"/data/*.txt; do sed -i.bak 's/$/\r/' "$f" && rm "$f.bak"; done ;;
esac
(cd dist && rm -f "Roadkill-$version-$name.zip" && zip -qr "Roadkill-$version-$name.zip" "Roadkill-$name")
echo "dist/Roadkill-$version-$name.zip"
