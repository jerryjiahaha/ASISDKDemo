clang++ ${BUILD_OPTS} -std=c++17 \
	-O3 \
	-Wall \
	-I ../include/  \
	`pwd`/../lib/x64/libASICamera2.so.1.13.0930 \
       	-pthread \
	`pkg-config --cflags --libs spdlog` \
	`pkg-config --cflags --libs cfitsio` \
	`pkg-config --cflags --libs gflags` \
	`pkg-config --cflags --libs libgps` \
	auto_expo.cpp -lstdc++fs \
	-o auto_expo
