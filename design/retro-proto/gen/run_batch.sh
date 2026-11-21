#!/bin/zsh
# runs codex image jobs 6 at a time
cd "$(dirname $0)/.."
jobs_list=(aquarium_amiga aquarium_c64 aquarium_atari aquarium_lcd fireplace_amiga fireplace_c64 fireplace_atari fireplace_lcd seaside_amiga seaside_c64 seaside_atari seaside_lcd)
n=0
for j in $jobs_list; do
  codex exec --skip-git-repo-check -s workspace-write -i gen/guide.png < gen/$j.txt > gen/$j.log 2>&1 &
  n=$((n+1)); if (( n % 6 == 0 )); then wait; fi
done
wait
echo ALL DONE
