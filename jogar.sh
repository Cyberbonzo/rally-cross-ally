#!/bin/bash
set -e
cd "$(dirname "$(realpath "$0")")"
export DEADSTATION_HOME="$PWD/userdata"
export DEADSTATION_RESOLUTION=1024
mkdir -p "$DEADSTATION_HOME"
exec ./app/Rally_Cross_recomp_module "$PWD/RallyCross.chd" \
    --set control.cross=B,R2 --set control.square=Y,L2 \
    --set control.l2=FN1 --set control.r2=FN2 \
    --set control.dpad_left=LEFT,LSLEFT --set control.dpad_right=RIGHT,LSRIGHT >>"$DEADSTATION_HOME/game.log" 2>&1
