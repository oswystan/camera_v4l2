## description

A camera demo code used for uvc camera device

## install

```
sudo apt-get install libudev-dev
make 
```

use the following command to convert yuv to jpeg

```
for one in `ls *.yuv`; do echo $one; ffmpeg -s 1600x1200  -pix_fmt yuyv422 -i $one $one.jpg; done
```

## tips

uvc device support ~=(bining size) output

some useful commands:

```
lsusb
lsusb -v -d $vendor_id:$product_id
```
