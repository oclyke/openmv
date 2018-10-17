#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe test \
    --model=models/chars74k/img-chars74k_train_test.prototxt \
    --weights=models/chars74k/img-chars74k_iter_10000.caffemodel $@
