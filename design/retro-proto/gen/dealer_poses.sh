#!/bin/zsh
# Face edits of the dealer bases (gen/dealer_<m>.png) for each pose; dealer.py pastes only the face onto the base.
# usage: ./dealer_poses.sh [machine ...]   (runs 5 Codex jobs at a time)
cd "$(dirname $0)"
D=$PWD
typeset -A FACE
FACE[blink]="his eyes closed in a blink (both eyelids shut, drawn as short curved lines); eyebrows, mustache and grin unchanged"
FACE[talk]="his mouth open in the middle of a word, as if saying 'aah', the mustache lifted a little; eyes and eyebrows unchanged"
FACE[smug]="a smug winner's face: one eyebrow raised high and the other lowered, sly half-closed eyes, a wide self-satisfied grin showing the gold tooth"
FACE[sad]="a sad loser's face: eyebrows tilted up in the middle, droopy eyes looking down, a wobbly frown, the mustache tips drooping, one big sweat drop on his forehead"
FACE[shock]="a shocked face: eyes wide open and round with tiny pupils, eyebrows shot up high, mouth a big round open O, the mustache tips flared out"
jobs=()
ms=($@); (( $#ms )) || ms=(amiga c64 atari vfd)
for m in $ms; do
  [ -f dealer_$m.png ] || { echo "no dealer_$m.png"; continue; }
  if [ $m = vfd ]; then bg="flat pure black background (#000000) and the same teal-cyan glow colors with the hot pink bow tie and cards"; else bg="flat pure magenta background (#FF00FF)"; fi
  for p in blink talk smug sad shock; do
    cat > dealer_${p}_$m.txt <<EOT
The attached image shows a retro pixel-art casino dealer character. Create a new image with your image generation tool, passing the attached image as the input image, and save it as $D/dealer_${p}_$m.png, the same size as the attached image. Edit ONLY his face: ${FACE[$p]}. Keep everything else exactly as in the attached image: the same pixel art style, palette, pixel size and outlines, the same pose, head position and head size, body, hands, cards, visor, hair and clothes, at exactly the same position and size so the two images line up, and the same $bg. No text, no letters. Save the image exactly as generated: do not resize, recolor or post-process it.
EOT
    jobs+=("$p $m")
  done
done
printf '%s\n' $jobs | xargs -P 5 -L 1 zsh -c 'codex exec --skip-git-repo-check -s workspace-write -i dealer_$1.png < dealer_$0_$1.txt > dealer_$0_$1.log 2>&1; echo "EXIT $?" >> dealer_$0_$1.log; echo "done $0 $1: $(ls dealer_$0_$1.png 2>/dev/null)"'
