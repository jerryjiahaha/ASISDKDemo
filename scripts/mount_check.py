#!/usr/bin/env python3

import time
import os
import subprocess
import shutil
import logging
import argparse
import random
from pathlib import Path

#import psutil

"""
Check filesystem free space availability, auto link new file filesystem to target dir
"""

DEFAULT_FS_CANDIDATE = [
#    "/home/jerryjia/test1",
#    "/home/jerryjia/test2",
    "/data1",
    "/data2",
]
FS_CANDIDATE = DEFAULT_FS_CANDIDATE

LINK_TAR = "/data"

Threshold = 16 * 1024 * 1024 * 1024 # 16G

TarProcName = "auto_expo"

TarServiceName = "auto_expo_start"

SpaceIndicator = "no_more_space.flag"

def selectFs():
    for fs in FS_CANDIDATE:
        try:
            if shutil.disk_usage(fs).free > Threshold:
                try:
                    os.unlink(SpaceIndicator)
                except FileNotFoundError:
                    pass
                return fs
        except FileNotFoundError as e:
            logging.error(e)
            continue
    logging.error("no available dir...")
    open(SpaceIndicator, "w")

def onFindSpaceOK():
    print(f"onFindSpaceOK, start {TarServiceName}.timer")
    os.system(f"systemctl start {TarServiceName}.timer")
    os.system(f"systemctl enable {TarServiceName}.timer")
    
def onFindSpaceFull():
    # NO space, try to kill target process
    """
    systemctl disable xxx
    systemctl stop xxx
    """
    print("No space... stop services")
    os.system(f"systemctl disable {TarServiceName}.timer")
    os.system(f"systemctl stop {TarServiceName}.timer")
    os.system(f"systemctl stop auto_expo.service")
#    for proc in psutil.process_iter():
#        if proc.name() == TarProcName:
#            proc.kill()


def mkLink(src, dst):
    exe_args = ["ln", "-sfn", str(src), str(dst),]
    logging.warning(f"will execute {' '.join(exe_args)}")
#    os.system(f"ln -sfn {src} {dst}")
    res = subprocess.run(exe_args, capture_output=True)
    if res.stderr != b'':
        logging.fatal(f"link failed: {res.stderr.decode()}")

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--target-link', help=f'data dir symlink target, default: {LINK_TAR}', default=LINK_TAR)
    parser.add_argument('-f', '--fs', help=f'candidate dir to be used, default is {FS_CANDIDATE}', action="append", nargs='+')
    parser.add_argument('--threshold', help=f'free space to be ensured in bytes for each dir, default: {Threshold}', type=int, default=Threshold)

    args = parser.parse_args()
    print(args)
    if args.fs is None:
        FS_CANDIDATE = DEFAULT_FS_CANDIDATE
    else:
        fc = set()
        for f in args.fs:
            fc.update(f)
        FS_CANDIDATE = list(fc)
    # avoid one of fs cannot write properly
    # random.shuffle(FS_CANDIDATE)
    Threshold = args.threshold

    # systemctl enable xxx
    # systemctl start xxx
    if Path(SpaceIndicator).exists():
        print("space full ? ...")
#        onFindSpaceFull()
    else:
        print("ensure services ...")
        onFindSpaceOK()
    fs = None
    while True:
        newfs = selectFs()
        if newfs is None:
            print("space full ! ...")
            onFindSpaceFull()
            time.sleep(20000)
            continue
        if newfs != fs:
            logging.warning("will change data dir")
            fs = newfs
            mkLink(fs, LINK_TAR)
            onFindSpaceOK()
        time.sleep(2)
    
