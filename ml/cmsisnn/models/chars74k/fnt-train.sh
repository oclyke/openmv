#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe train \
    --solver=models/chars74k/fnt-chars74k_solver.prototxt $@
