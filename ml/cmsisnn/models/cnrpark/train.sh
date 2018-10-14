#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe train \
    --solver=models/cnrpark/cnrpark_solver.prototxt $@
