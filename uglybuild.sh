clang++ -std=c++17 \
	-g \
	-Wall \
	-I ../include/  \
	`pwd`/../lib/x64/libASICamera2.so.1.13.0930 \
       	-pthread \
	`pkg-config --cflags --libs spdlog` \
	`pkg-config --cflags --libs cfitsio` \
	`pkg-config --cflags --libs gflags` \
	auto_expo.cpp -lstdc++fs \
	-o auto_expo
