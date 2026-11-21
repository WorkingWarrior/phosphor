#!/bin/zsh
# app backdrops: weather bases + radio first, then timer, hourglass and the weather variants (edits of the bases)
cd "$(dirname $0)"
run() { codex exec --skip-git-repo-check -s workspace-write -i "$(cat $1.guide)" < $1.txt > $1.log 2>&1 }
for j in wx_clear_c64 wx_clear_amiga wx_clear_atari radio_c64 radio_amiga radio_atari; do run $j & done; wait
for j in wxvar_c64 wxvar_amiga wxvar_atari timer_c64 timer_amiga timer_atari sand_c64 sand_amiga sand_atari; do run $j & done; wait
echo ALL DONE
