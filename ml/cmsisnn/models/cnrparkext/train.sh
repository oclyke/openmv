#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe train \
    --solver=models/cnrparkext/cnrparkext_solver.prototxt $@
