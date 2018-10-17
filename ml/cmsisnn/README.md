# CMSIS-NN

This folder contains scripts to train, test, and quantize Caffe models and then convert them to an 8-bit binary format for running on the OpenMV Cam.

## Getting started
1. Setup your computer for deep-learning:
    1. CPU Only - https://www.pyimagesearch.com/2017/09/25/configuring-ubuntu-for-deep-learning-with-python/
    2. GPU (recommended) - https://www.pyimagesearch.com/2017/09/27/setting-up-ubuntu-16-04-cuda-gpu-for-deep-learning-with-python/
2. Install caffe:
    1. `pushd ~` (in the folder this `READMD.md` is in)
    2. `git clone --recursive https://github.com/BVLC/caffe.git`
    3. Follow https://github.com/BVLC/caffe/wiki/Ubuntu-16.04-or-15.10-Installation-Guide
    4. Add `export PYTHONPATH=/home/<username>/caffe/:$PYTHONPATH` to your `~/.bashrc` file.
    5. `source ~/.bashrc`
    6. `popd`
    7. `ln -s ~/caffe caffe` (in the folder this `READMD.md` is in)
3. Read http://adilmoujahid.com/posts/2016/06/introduction-deep-learning-python-caffe/

## Training a CIFAR10 Model
1. Now we're going to train a CIFAR10 Model.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe`
        2. `./data/cifar10/get_cifar10.sh`
        3. `./examples/cifar10/create_cifar10.sh`
        4. `./build/tools/compute_image_mean -backend=lmdb examples/cifar10/cifar10_train_lmdb examples/cifar10/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/cifar10/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/cifar10/test.sh` - You should get an accuracy of about 80%.
    2. `python2 nn_quantizer.py --gpu --model models/cifar10/cifar10_train_test.prototxt --weights models/cifar10/cifar10_iter_70000.caffemodel.h5 --save models/cifar10/cifar10.pkl` - Note how the accuracy stays at about 80%.
    3. `python2 nn_convert.py --model models/cifar10/cifar10.pkl --mean caffe/examples/cifar10/mean.binaryproto --output models/cifar10/cifar10.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network. To run the CNN copy the `models/cifar10/cifar10.network` file to your OpenMV Cam's disk and then run our CIFAR10 Machine Learning Examples.
        * Fits on the H7 only.

## Training a CIFAR10 Fast Model
1. Now we're going to train a CIFAR10 Fast Model which is 60% smaller than the cifar10 network with only a 2% loss in accurary.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe`
        2. `./data/cifar10/get_cifar10.sh`
        3. `./examples/cifar10/create_cifar10.sh`
        4. `./build/tools/compute_image_mean -backend=lmdb examples/cifar10/cifar10_train_lmdb examples/cifar10/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/cifar10_fast/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/cifar10_fast/test.sh` - You should get an accuracy of about 78%.
    2. `python2 nn_quantizer.py --gpu --model models/cifar10_fast/cifar10_fast_train_test.prototxt --weights models/cifar10_fast/cifar10_fast_iter_70000.caffemodel.h5 --save models/cifar10_fast/cifar10_fast.pkl` - Note how the accuracy stays at about 78%.
    3. `python2 nn_convert.py --model models/cifar10_fast/cifar10_fast.pkl --mean caffe/examples/cifar10/mean.binaryproto --output models/cifar10_fast/cifar10_fast.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network. To run the CNN copy the `models/cifar10_fast/cifar10_fast.network` file to your OpenMV Cam's disk and then run our CIFAR10 Machine Learning Examples using the cifar10_fast.network.

## Training a MNIST Model
1. Now we're going to train a MNIST Model.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe`
        2. `./data/mnist/get_mnist.sh`
        3. `./examples/mnist/create_mnist.sh`
        4. `./build/tools/compute_image_mean -backend=lmdb examples/mnist/mnist_train_lmdb examples/mnist/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/lenet/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/lenet/test.sh` - You should get an accuracy of about 99%.
    2. `python2 nn_quantizer.py --gpu --model models/lenet/lenet_train_test.prototxt --weights models/lenet/lenet_iter_10000.caffemodel --save models/lenet/lenet.pkl` - Note how the accuracy stays at about 99%.
    3. `python2 nn_convert.py --model models/lenet/lenet.pkl --mean caffe/examples/mnist/mean.binaryproto --output models/lenet/lenet.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network. To run the CNN copy the `models/lenet/lenet.network` file to your OpenMV Cam's disk and then run our LENET Machine Learning Examples.
        * Fits on the H7 only.

## Training a Smile Detection Model
1. Now we're going to train a Smile Detection Model.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe/examples`
        2. `mkdir smile`
        3. `cd smile`
        4. `git clone --recursive https://github.com/hromi/SMILEsmileD.git`
        5. `cd ../../../../..`
        6. The SMILEsmileD dataset has about ~3K positive images and ~9K negative images so we need to augment our positive image dataset so that it is about the same size as our negative image dataset.
            1. `mkdir ml/cmsisnn/caffe/examples/smile/data`
            2. `cp -r ml/cmsisnn/caffe/examples/smile/SMILEsmileD/SMILEs/negatives/negatives7/ ml/cmsisnn/caffe/examples/smile/data/1_negatives`
            3. `sudo pip2 install opencv-python imgaug tqdm`
            4. `mkdir ml/cmsisnn/caffe/examples/smile/data/0_positives`
            5. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/smile/SMILEsmileD/SMILEs/positives/positives7/ --output ml/cmsisnn/caffe/examples/smile/data/0_positives/ --count 3`
        7. Now we need to create an lmdb.
            1. `mkdir ml/cmsisnn/caffe/examples/smile/lmdbin`
            2. `python2 tools/create_labels.py --input ml/cmsisnn/caffe/examples/smile/data/ --output ml/cmsisnn/caffe/examples/smile/lmdbin/`
            3. `cd ml/cmsisnn/caffe/`
            4. `GLOG_logtostderr=1 ./build/tools/convert_imageset --shuffle examples/smile/lmdbin/ examples/smile/train.txt examples/smile/train_lmdb`
            5. `GLOG_logtostderr=1 ./build/tools/convert_imageset --shuffle examples/smile/lmdbin/ examples/smile/test.txt examples/smile/test_lmdb`
        8. `./build/tools/compute_image_mean -backend=lmdb examples/smile/train_lmdb examples/smile/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/smile/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/smile/test.sh` - You should get an accuracy of about 96%.
    2. `python2 nn_quantizer.py --gpu --model models/smile/smile_train_test.prototxt --weights models/smile/smile_iter_200000.caffemodel --save models/smile/smile.pkl` - Note how the accuracy stays at about 96%.
    3. `python2 nn_convert.py --model models/smile/smile.pkl --mean caffe/examples/smile/mean.binaryproto --output models/smile/smile.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network. To run the CNN copy the `models/smile/smile.network` file to your OpenMV Cam's disk and then run our Smile Machine Learning Example.

## More Networks

Now let's train some more useful networks.

### CNRPark (Parking Lot Detection)
1. Now we're going to train a Parking Lot Detection Model.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe/examples`
        2. `mkdir cnrpark`
        3. `cd cnrpark`
        4. `wget http://cnrpark.it/dataset/CNRPark-Patches-150x150.zip`
        5. `unzip CNRPark-Patches-150x150.zip`
        6. `cd ../../../../..`
        7. The CNRPark dataset has about ~4K free images and ~8K busy images so we need to augment our free image dataset so that it is about the same size as our busy image dataset.
            1. `mkdir ml/cmsisnn/caffe/examples/cnrpark/data`
            2. `cp -r ml/cmsisnn/caffe/examples/cnrpark/A/busy/ ml/cmsisnn/caffe/examples/cnrpark/data/0_busy/`
            3. `cp -r ml/cmsisnn/caffe/examples/cnrpark/B/busy/* ml/cmsisnn/caffe/examples/cnrpark/data/0_busy/` - Note the `*`.
            4. `sudo pip2 install opencv-python imgaug tqdm`
            5. `mkdir ml/cmsisnn/caffe/examples/cnrpark/data/1_free`
            6. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/cnrpark/A/free/ --output ml/cmsisnn/caffe/examples/cnrpark/data/1_free/ --count 2`
            7. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/cnrpark/B/free/ --output ml/cmsisnn/caffe/examples/cnrpark/data/1_free/ --count 2`
        8. Now we need to create an lmdb.
            1. `mkdir ml/cmsisnn/caffe/examples/cnrpark/lmdbin`
            2. `python2 tools/create_labels.py --input ml/cmsisnn/caffe/examples/cnrpark/data/ --output ml/cmsisnn/caffe/examples/cnrpark/lmdbin/`
            3. `cd ml/cmsisnn/caffe/`
            4. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --shuffle examples/cnrpark/lmdbin/ examples/cnrpark/train.txt examples/cnrpark/train_lmdb`
            5. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --shuffle examples/cnrpark/lmdbin/ examples/cnrpark/test.txt examples/cnrpark/test_lmdb`
        9. `./build/tools/compute_image_mean -backend=lmdb examples/cnrpark/train_lmdb examples/cnrpark/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/cnrpark/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/cnrpark/test.sh` - You should get an accuracy of 100%.
        * While an accurcy this high may give you pause... If you look at the crnpark dataset you can see that a network should have no problem identifying free versus busy spaces given that the two classes of images are widly different.
    2. `python2 nn_quantizer.py --gpu --model models/cnrpark/cnrpark_train_test.prototxt --weights models/cnrpark/cnrpark_iter_10000.caffemodel --save models/cnrpark/cnrpark.pkl` - Note how the accuracy stays at 100%.
    3. `python2 nn_convert.py --model models/cnrpark/cnrpark.pkl --mean caffe/examples/cnrpark/mean.binaryproto --output models/cnrpark/cnrpark.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network.

### CNRPark+EXT (Extended Packing Lot Detection)
1. Now we're going to train an Extended Parking Lot Detection Model which should be robust to alot more conditions.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe/examples`
        2. `mkdir cnrparkext`
        3. `cd cnrparkext`
        4. `wget http://cnrpark.it/dataset/CNR-EXT-Patches-150x150.zip`
        5. `unzip CNR-EXT-Patches-150x150.zip`
        6. The CNRPark+EXT dataset has about ~65K negative images to about ~79K positive images so it's mostly balanced and we can skip augmentation. Additionally, it comes with a label file usable for creating lmdb files so we can skip to just splitting the labels into a test and training dataset.
            1. `split -l $(expr $(cat LABELS/all.txt | wc -l) \* 85 / 100) LABELS/all.txt PATCHES/`
                * This splits the dataset labels into files `aa` and `ab` for the training and test data respectively along with placing the label files in the right directory.
            2. `cd ../..`
            3. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --shuffle examples/cnrparkext/PATCHES/ examples/cnrparkext/PATCHES/aa examples/cnrparkext/train_lmdb`
            4. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --shuffle examples/cnrparkext/PATCHES/ examples/cnrparkext/PATCHES/ab examples/cnrparkext/test_lmdb`
        7. `./build/tools/compute_image_mean -backend=lmdb examples/cnrparkext/train_lmdb examples/cnrparkext/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/cnrparkext/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/cnrparkext/test.sh` - You should get an accuracy of 96%.
        * This network is robust to a lot more conditions than cnrpark while being the same model size - Awesome!
    2. `python2 nn_quantizer.py --gpu --model models/cnrparkext/cnrparkext_train_test.prototxt --weights models/cnrparkext/cnrparkext_iter_10000.caffemodel --save models/cnrparkext/cnrparkext.pkl` - Note how the accuracy stays at 96%.
    3. `python2 nn_convert.py --model models/cnrparkext/cnrparkext.pkl --mean caffe/examples/cnrparkext/mean.binaryproto --output models/cnrparkext/cnrparkext.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network.

### Chars74K (Character Recognition)
1. Now we're going to train a character recognition model for detecting (0-9, A-Z, a-z).
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe/examples`
        2. `mkdir chars74k`
        3. `cd chars74k`
        4. `wget http://www.ee.surrey.ac.uk/CVSSP/demos/chars74k/EnglishImg.tgz`
        5. `wget http://www.ee.surrey.ac.uk/CVSSP/demos/chars74k/EnglishHnd.tgz`
        6. `wget http://www.ee.surrey.ac.uk/CVSSP/demos/chars74k/EnglishFnt.tgz`
        7. `wget http://www.ee.surrey.ac.uk/CVSSP/demos/chars74k/Lists.tgz`
        8. `tar -xvf EnglishImg.tgz`
        9. `tar -xvf EnglishHnd.tgz`
        10. `tar -xvf EnglishFnt.tgz`
        11. `tar -xvf Lists.tgz`
        12. Now to build the label file...
            1. `cat Lists/English/Fnt/all.txt | sed 's/.*Sample\([0-9][0-9][0-9]\).*/Fnt\/\0.png \1/' > English/fnt-labels.txt`
            2. `cat Lists/English/Hnd/all.txt | sed 's/.*Sample\([0-9][0-9][0-9]\).*/Hnd\/\0.png \1/' > English/hnd-labels.txt`
            3. `cat Lists/English/Img/all_good.txt | sed 's/.*Sample\([0-9][0-9][0-9]\).*/Img\/\0.png \1/' > English/img-labels.txt`
            4. `shuf English/fnt-labels.txt > English/fnt-labels_mixed.txt`
            5. `shuf English/hnd-labels.txt > English/hnd-labels_mixed.txt`
            6. `shuf English/img-labels.txt > English/img-labels_mixed.txt`
        13. Now to split the lable file...
            1. `split -l $(expr $(cat English/fnt-labels_mixed.txt | wc -l) \* 85 / 100) English/fnt-labels_mixed.txt English/fnt-`
                * This splits the dataset labels into files `fnt-aa` and `fnt-ab` for the training and test data respectively along with placing the label files in the right directory.
            2. `split -l $(expr $(cat English/hnd-labels_mixed.txt | wc -l) \* 85 / 100) English/hnd-labels_mixed.txt English/hnd-`
                 * This splits the dataset labels into files `hnd-aa` and ``hnd-ab` for the training and test data respectively along with placing the label files in the right directory.
            3. `split -l $(expr $(cat English/img-labels_mixed.txt | wc -l) \* 85 / 100) English/img-labels_mixed.txt English/img-`
                 * This splits the dataset labels into files `img-ab` and ``img-ab` for the training and test data respectively along with placing the label files in the right directory.
        14. `cd ../..`
        15. Now create the files for the font LMDB.
            1. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/fnt-aa examples/chars74k/fnt-train_lmdb`
            2. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/fnt-ab examples/chars74k/fnt-test_lmdb`
            3. `./build/tools/compute_image_mean -backend=lmdb examples/chars74k/fnt-train_lmdb examples/chars74k/fnt-mean.binaryproto`
        16. Now create the files for the handwritten LMDB.
            1. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=48 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/hnd-aa examples/chars74k/hnd-train_lmdb`
            2. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=48 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/hnd-ab examples/chars74k/hnd-test_lmdb`
            3. `./build/tools/compute_image_mean -backend=lmdb examples/chars74k/hnd-train_lmdb examples/chars74k/hnd-mean.binaryproto`
        17. Now create the files for the image LMDB.
            1. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/img-aa examples/chars74k/img-train_lmdb`
            2. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=64 --resize_width=64 --gray --shuffle examples/chars74k/English/ examples/chars74k/English/img-ab examples/chars74k/img-test_lmdb`
            3. `./build/tools/compute_image_mean -backend=lmdb examples/chars74k/img-train_lmdb examples/chars74k/img-mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/chars74k/fnt-train.sh` - This takes a while.
            * Note that because our labels are 1 indexed we make the model have 63 outputs where the first output is never given any training examples and should be ignored.
        3. `./models/chars74k/hnd-train.sh` - This takes a while.
            * Note that because our labels are 1 indexed we make the model have 63 outputs where the first output is never given any training examples and should be ignored.
        4. `./models/chars74k/img-train.sh` - This takes a while.
            * Note that because our labels are 1 indexed we make the model have 63 outputs where the first output is never given any training examples and should be ignored.
2. Great! Now let's test and then convert the network.
    1. `./models/chars74k/fnt-test.sh` - You should get an accuracy of 78%.
    2. `python2 nn_quantizer.py --gpu --model models/chars74k/fnt-chars74k_train_test.prototxt --weights models/chars74k/fnt-chars74k_iter_10000.caffemodel --save models/chars74k/fnt-chars74k.pkl` - Note how the accuracy stays at 78%.
    3. `python2 nn_convert.py --model models/chars74k/fnt-chars74k.pkl --mean caffe/examples/chars74k/fnt-mean.binaryproto --output models/chars74k/fnt-chars74k.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network.
        * Fits on the H7 only.

### INRIA Person Detaset (Person Detection)
1. Now we're going to train a person detection using the INRIA person detection dataset.
    1. Open a terminal in this folder.
    2. First we need to get the data and create an lmdb.
        1. `cd caffe/examples`
        2. `mkdir inria`
        3. `cd inria`
        4. `wget ftp://ftp.inrialpes.fr/pub/lear/douze/data/INRIAPerson.tar`
        5. `tar -xvf INRIAPerson.tar`
        6. Now let's build the LMDB files:
            1. `mkdir data && mkdir data/0_pos && mkdir data/1_neg`
            2. `cd ../../../../..`
            3. `sudo pip2 install opencv-python imgaug tqdm`
            4. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/inria/INRIAPerson/test_64x128_H96/neg/ --output ml/cmsisnn/caffe/examples/inria/data/1_neg/ --count 4`
            5. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/inria/INRIAPerson/train_64x128_H96/neg/ --output ml/cmsisnn/caffe/examples/inria/data/1_neg/ --count 4`
            6. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/inria/INRIAPerson/test_64x128_H96/pos/ --output ml/cmsisnn/caffe/examples/inria/data/0_pos/ --count 2`
            7. `python2 tools/augment_images.py --input ml/cmsisnn/caffe/examples/inria/INRIAPerson/train_64x128_H96/pos/ --output ml/cmsisnn/caffe/examples/inria/data/0_pos/ --count 2`
            8. `mkdir ml/cmsisnn/caffe/examples/inria/lmdbin`
            9. `python2 tools/create_labels.py --input ml/cmsisnn/caffe/examples/inria/data/ --output ml/cmsisnn/caffe/examples/inria/lmdbin/`
            10. `cd ml/cmsisnn/caffe/`
            11. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=128 --resize_width=64 --gray --shuffle examples/inria/lmdbin/ examples/inria/train.txt examples/inria/train_lmdb`
            12. `GLOG_logtostderr=1 ./build/tools/convert_imageset --resize_height=128 --resize_width=64 --gray --shuffle examples/inria/lmdbin/ examples/inria/test.txt examples/inria/test_lmdb`
            13. `./build/tools/compute_image_mean -backend=lmdb examples/inria/train_lmdb examples/inria/mean.binaryproto`
    3. Next we need to train our network.
        1. `cd ..`
        2. `./models/inria/train.sh` - This takes a while.
2. Great! Now let's test and then convert the network.
    1. `./models/inria/test.sh` - You should get an accuracy of 95%.
    2. `python2 nn_quantizer.py --gpu --model models/inria/inria_train_test.prototxt --weights models/inria/inria_iter_10000.caffemodel --save models/inria/inria.pkl` - Note how the accuracy stays at 95%.
    3. `python2 nn_convert.py --model models/inria/inria.pkl --mean caffe/examples/inria/mean.binaryproto --output models/inria/inria.network`.
    4. And that's it! You've created a CNN that will run on the OpenMV Cam! Keep in mind that your OpenMV Cam has limited weight/bias heap space so this limits the size of the network.

## Train A Custom Net
If you'd like to train your own custom CNN you need to assemble a dataset of hundreds (preferably thousands) of images of training examples. Start searching [here](http://homepages.inf.ed.ac.uk/rbf/CVonline/Imagedbase.htm) if you think the dataset you need already exists. Anyway, once you've collected all the training examples save the images per class of training examples in seperate folders structed like this:
* data/
    * 0_some_class/
    * 1_some_other_class/
    * 2_etc/

Once you've built a folder structure like this please refer to the examples above to:
1. Create a labeled training dataset using `create_labels.py` and `augment_images.py` if you need to balance the training examples.
2. Create training and test lmdb files.
3. Create a mean.binaryproto file.
4. Train the network (copy how the smile `train.sh` script and solver/train/test protobufs work to do this).
    * If your network loss is stuck at much greater than 1 then this means your `base_lr` (learning rate) is too high. Reduce it.
    * If your network accuracy goes to 1 for training and testing it
    means your network is overfitting. That said, if you 100% the test data and 100% the training data your network is probably okay.
5. Test the network (copy how the smile `test.sh` script and solver/train/test protobufs works to do this).
6. Quantize the network.
7. And finally convert the network.

## Known Limitations 
1. Parser supports conv, pool, relu, fc layers only.
2. Quantizer supports only networks with feed-forward structures (e.g. conv-relu-pool-fc)  without branch-out/branch-in (as in inception/squeezeNet, etc.).
3. See [ARM ML-examples](https://github.com/ARM-software/ML-examples/tree/master/cmsisnn-cifar10) for more information.
4. Source Code https://github.com/ARM-software/CMSIS_5/tree/develop/CMSIS/NN
