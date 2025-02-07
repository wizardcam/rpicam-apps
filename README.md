# rpicam-apps
This is a small suite of libcamera-based applications to drive the cameras on a Raspberry Pi platform.
It is forked from the official repo and we have adapted rpicam-raw to suit our needs.

Official repo status
------

[![ToT libcamera build/run test](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml/badge.svg)](https://github.com/raspberrypi/rpicam-apps/actions/workflows/rpicam-test.yml)

The source code is made available under the simplified [BSD 2-Clause license](https://spdx.org/licenses/BSD-2-Clause.html).

Update your raspi
-----

Make sure your raspi is up to date.

```sh
sudo apt-get update
sudo apt-get upgrade
sudo apt full-upgrade
sudo rpi-update
```

Install OV9281 Camera
-----

[From here.](https://www.raspberrypi.com/documentation/computers/camera_software.html#configuration)

Edit file `/boot/firmware/config.txt`

Edit line `camera_auto_detect=0`

Add line `dtoverlay=ov9281`

Reboot now and test afterwards if libcamera-hello works. **Only proceed if it works!** If it doesnt work, restart or read the manufacturers docs.

Building
------

The following is directly extracted from the [official rpi documentation](https://www.raspberrypi.com/documentation/computers/camera_software.html#building-rpicam-apps-without-building-libcamera).
At some point it might be out of date.
Just check the documentation for building the apps.

Remove pre-installed rpicam-apps
------

```sh
sudo apt remove --purge rpicam-apps
```

Building rpicam-apps without building libcamera locally
------

To build rpicam-apps without first rebuilding libcamera and libepoxy, install libcamera, libepoxy and their dependencies with apt:
If you do not need support for the GLES/EGL preview window, omit libepoxy-dev.

```sh
./install.sh
```

Building rpicam-apps
------

For repeated rebuilds use this script

```sh
./build.sh
```

Make sure the memcached configurations are equal to `memcached.conf`. 

Container Memcached & Redis (recommended)
------

See seampilot/services/infrastructure.yml to have a memcached and redis instance.
`docker compose up -f infrastructure.yml -d`

Local Memcached (not recommended)
------

```sh
sudo apt install memcached
sudo apt install libmemcached-tools
sudo mkdir -p /var/run/memcached/
sudo chmod 775 /var/run/memcached/
sudo chown memcache:memcache /var/run/memcached/
sudo systemctl start memcached
sudo chmod 2750 /var/run/memcached
sudo ldconfig
```

SeamPilot rpicam-raw
------

We have developed the following files to suit our usecase of capturing frames and saving them to memcached while streaming an event to redis.
In `output/memcached_output.cpp` we define the procedure of saving the images to memcached and writing an event to redis.
In `output/output.cpp` we define the execution argument mem:// which executes the above routine

This is how we would start the app:

```sh
rpicam-raw ---n --framerate 120 --mode 640:400:8 --width 640 --height 400 -o test%05d.raw
rpicam-raw ---n --framerate 120 --mode 640:400:8 --width 640 --height 400 -o mem:// -t 0 --redis localhost:6379 --memcached localhost:11211
rpicam-raw ---n --framerate 120 --mode 1280:800:8 --width 1280 --height 800 -o mem:// -t 0 --redis localhost:6379 --memcached localhost:11211
```

Other options:

- gain
- roi
- exposure

Full list of options
------

Have a look inside `core/options.cpp`

TODO's
------

Search for `TODO` to see what is left to be done.

Modified Files
------

Search for the term "ADDED" to find code added section in the below files.

output/output.cpp
output/meson.build
core/options.hpp
core/options.cpp

Created Files
------

- build.sh
- memcached.conf
- memcached_output.hpp
- overlay.sh
- raw_frame.ipynb
- requirements.txt
