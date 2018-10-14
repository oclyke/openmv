#!/usr/bin/env sh
set -e

TOOLS=./caffe/build/tools

$TOOLS/caffe test \
    --model=models/cnrpark/cnrpark_train_test.prototxt \
    --weights=models/cnrpark/cnrpark_iter_10000.caffemodel $@
