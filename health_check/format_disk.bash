#!/bin/bash

disk=$(ls /dev/disk/by-partlabel -l| grep TAISHAN_DATA_Z2  |awk '{print $NF}' |awk -F '/' '{print $NF}')
echo Will format disk ${disk}

sudo mkfs.ext4 -F -L taishandatae2 ${disk} 2>&1 | sudo tee format.log

echo "Done"
