.PHONY: default

default: file_dispatch

file_dispatch.pyx: file_dispatch.py
	cp file_dispatch.py file_dispatch.pyx

file_dispatch.c: file_dispatch.pyx
	cython3 -3 file_dispatch.pyx --embed

file_dispatch: file_dispatch.c
	gcc -O2 file_dispatch.c  `python3-config --ldflags --cflags` -fPIC -o file_dispatch

clean:
	rm file_dispatch.pyx
