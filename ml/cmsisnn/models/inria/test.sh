#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe test \
    --model=models/inria/inria_train_test.prototxt \
    --weights=models/inria/inria_iter_10000.caffemodel $@
