#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe test \
    --model=models/cnrparkext/cnrparkext_train_test.prototxt \
    --weights=models/cnrparkext/cnrparkext_iter_10000.caffemodel $@
