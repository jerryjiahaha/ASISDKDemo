# ASISDKDemo
Use ZWO ASI SDK to write exposure demos

`main_SDK2_snap.cpp`, `main_SDK2_video.cpp`, `main_SDK2_video_mac.cpp` and `Makefile` is provided by [ZWO ASI](http://zwoasi.com/software)'s [SDK](http://zwoasi.com/softwares/ASI_linux_mac_SDK_V1.13.0930.tar.bz2)

I just wrote `auto_expo.cpp` to take simple exposures under with `./uglybuild.sh` to build.

Call `./auto_expo --help` for help. See more in [Tutorial](TUTORIAL.md)

At present, a web server listening 9116 is embedded into `auto_expo`, so the programm could accept http request during runtime:

Stop exposure:

```
curl localhost:9116/stop_expo
```

Start exposure for N times (N=0 means loop forever):

```
curl localhost:9116/start_expo/N
```

There are some important scripts under dir scripts. I also tryied to build python script into executable binary with cython.

Tested under Archlinux and Debian 10.

Depedency:
- c++17 (std::filesystem)
- ASI Cam SDK
- [spdlog](https://github.com/gabime/spdlog)
- [cfitsio](https://heasarc.gsfc.nasa.gov/fitsio/)
- [gflags](https://github.com/gflags/gflags)
- [libgps](https://gpsd.gitlab.io/gpsd/)
- [uWebSockets](https://www.cnblogs.com/baldermurphy/p/9759660.html) (already as repo subtree, it also optionally needs **libuv**)
