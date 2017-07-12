## description

A camera demo code used for uvc camera device

## install

```
sudo apt-get install libudev-dev
make 
```

use the following command to convert yuv to jpeg

```
ffmpeg -s 640x480  -pix_fmt yuyv422 -i xxx.yuv xxx.jpg
```
