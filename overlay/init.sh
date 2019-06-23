#!/bin/sh

mkdir /config/device-tree/overlays/fpga
dtc -I dts -O dtb -o fpga-load.dtb fpga-load.dts
cp fpga-load.dtb /config/device-tree/overlays/fpga/dtbo

mkdir /config/device-tree/overlays/fclk0
dtc -I dts -O dtb -o fclk0-zynqmp.dtb fclk0-zynqmp.dts
cp fclk0-zynqmp.dtb /config/device-tree/overlays/fclk0/dtbo

mkdir /config/device-tree/overlays/udmabuf0
dtc -I dts -O dtb -o udmabuf0.dtb udmabuf0.dts
cp udmabuf0.dtb /config/device-tree/overlays/udmabuf0/dtbo

mkdir /config/device-tree/overlays/udmabuf1
dtc -I dts -O dtb -o udmabuf1.dtb udmabuf1.dts
cp udmabuf1.dtb /config/device-tree/overlays/udmabuf1/dtbo

mkdir /config/device-tree/overlays/hog_svm
dtc -I dts -O dtb -o hog_svm.dtb hog_svm.dts
cp hog_svm.dtb /config/device-tree/overlays/hog_svm/dtbo

mkdir /config/device-tree/overlays/dma0
dtc -I dts -O dtb -o dma0.dtb dma0.dts
cp dma0.dtb /config/device-tree/overlays/dma0/dtbo
