# ASISDKDemo
Use ZWO ASI SDK to write exposure demos

`main_SDK2_snap.cpp`, `main_SDK2_video.cpp`, `main_SDK2_video_mac.cpp` and `Makefile` is provided by [ZWO ASI](http://zwoasi.com/software)'s [SDK](http://zwoasi.com/softwares/ASI_linux_mac_SDK_V1.13.0930.tar.bz2)

I just wrote `auto_expo.cpp` to take simple exposures under Arch Linux with `./uglybuild.sh` to build.

Depedency:
- c++17 (std::filesystem)
- ASI Cam SDK
- [spdlog](https://github.com/gabime/spdlog)
- [cfitsio](https://heasarc.gsfc.nasa.gov/fitsio/)
- [gflags](https://github.com/gflags/gflags)
