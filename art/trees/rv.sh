#!/bin/bash
# Review one tree: bash art/trees/rv.sh <species> <seed> <lod> [texture kind]  ->  build/trees/review/<species>_<seed>_<lod>.jpg
cd "$(dirname "$0")/../.."
B="/c/Program Files/Blender Foundation/Blender 5.2/blender.exe"; R=$(cygpath -w "$PWD")
o="build/trees/review/$1_$2_$3"; mkdir -p build/trees/review
"$B" -b --factory-startup -P art/trees/review.py -- "$1" "$2" "$3" "$R\\build\\trees\\textures" "$R\\build\\trees\\review\\$1_$2_$3.png" ${4:-} 2>&1 | grep -E "REVIEW|Error|Traceback|line [0-9]" | cut -c1-160
python -I art/trees/sheet.py "$o.jpg" "${o}_0.png" "${o}_1.png" "${o}_2.png" "${o}_foot.png"
