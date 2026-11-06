#!/bin/sh
# The PS2 release's two videos for the console's player (loader/movie.c),
# which the Android build doesn't have: MOVIES/VCNTSC.PSS (the Rockstar
# logos) and MOVIES/OPEN1.PSS (the opening), 640x480 MPEG-2 with 48 kHz PCM
# sound in Sony's SShd wrapper (tools/pss_audio.py takes it out), to H.264
# baseline + AAC 44.1 kHz stereo at the same 640x480.
#   tools/movies.sh <the disc's MOVIES folder, or the disc as .iso/.bin> [out]
# A .chd first: chdman extractcd -i game.chd -o game.cue -ob game.iso
# Out: logos.mp4, intro.mp4 in [out] (default: the repo's gamedata/movies),
# for game/movies/. Needs python3, ffmpeg (libx264) and, for a disc image,
# bsdtar.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
src=${1:?usage: tools/movies.sh <MOVIES folder or disc .iso/.bin> [out folder]}
out=${2:-$here/../gamedata/movies}
mkdir -p "$out"
tmp=$(mktemp -d)
if [ -d "$src" ]; then
	mv_dir=$src
else
	bsdtar -xf "$src" -C "$tmp" MOVIES/VCNTSC.PSS MOVIES/OPEN1.PSS
	mv_dir=$tmp/MOVIES
fi
for pair in VCNTSC:logos OPEN1:intro; do
	pss=$mv_dir/${pair%%:*}.PSS
	name=${pair#*:}
	python3 -I "$here/pss_audio.py" "$pss" "$tmp/$name.wav"
	ffmpeg -v error -y -i "$pss" -i "$tmp/$name.wav" -map 0:v -map 1:a \
		-c:v libx264 -profile:v baseline -level 3.0 -preset slow -crf 20 -pix_fmt yuv420p \
		-c:a aac -b:a 128k -ar 44100 -ac 2 -shortest -movflags +faststart "$out/$name.mp4"
done
rm -rf "$tmp"
ls -la "$out"
