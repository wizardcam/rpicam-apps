# Dependencies
sudo apt install -y qtbase5-dev libqt5core5a libqt5gui5 libqt5widgets5 libcamera-dev libepoxy-dev libjpeg-dev libtiff5-dev libpng-dev libavcodec-dev libavdevice-dev libavformat-dev libswresample-dev

# Our app requires redis, memcached and opencv, so install the dev packages.
sudo apt install -y libmemcached-dev libhiredis-dev libssl-dev libopencv-dev

# For the build
sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev meson ninja-build