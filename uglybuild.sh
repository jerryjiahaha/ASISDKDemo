CXX=clang++
CC=clang

# ref: https://stackoverflow.com/questions/49793632/clang-fsanitize-undefined-with-128-integer-operations-undefined-reference-to
CLANG_WORKAROUND='-lgcc_s --rtlib=compiler-rt -fsanitize=undefined'

WITH_SSL=0 WITH_LIBUV=1 make CC=${CC} CXX=${CXX} -C uWebSockets/uSockets

${CXX} ${BUILD_OPTS} ${CLANG_WORKAROUND} \
	-std=c++17 \
	-flto \
	-O3 \
	-Wall \
	-I ../include/  \
	-I uWebSockets/src -I uWebSockets/uSockets/src \
	uWebSockets/uSockets/*.o \
	`pwd`/../lib/x64/libASICamera2.so.1.13.0930 \
	-pthread -lz \
	`pkg-config --cflags --libs spdlog` \
	`pkg-config --cflags --libs cfitsio` \
	`pkg-config --cflags --libs gflags` \
	`pkg-config --cflags --libs libgps` \
	`pkg-config --cflags --libs libuv` \
	auto_expo.cpp -lstdc++fs \
	-o auto_expo
