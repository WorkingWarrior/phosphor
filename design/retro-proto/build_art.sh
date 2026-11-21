#!/bin/zsh
# Rebuilds the scene pictures in assets/art/ (packed into the asset pack by tools/pack_assets.py) from art.txt.
# Previews land in out/.
cd "$(dirname $0)"
mkdir -p out ../../assets/art
grep -v '^#' art.txt | while read name machine src crop; do
  [ -z "$name" ] && continue
  python3 convert.py $machine $src $name out ../../assets/art/$name.img $crop || exit 1
done
