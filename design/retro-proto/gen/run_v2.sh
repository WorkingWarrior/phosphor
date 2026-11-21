#!/bin/zsh
# round 2 art: sky-only weather (base + 7 edits), rocket-free launch field, ocean liner, study room
cd "$(dirname $0)"
run() { codex exec --skip-git-repo-check -s workspace-write -i "$(cat $1.guide)" < $1.txt > $1.log 2>&1 }
for m in c64 amiga atari; do for j in sky_clear launch ship study; do run ${j}_$m & done; done; wait
for m in c64 amiga atari; do run skyvar_$m & done; wait
echo ALL DONE
