#!/usr/bin/env python3

import time
import sys
import os
import subprocess
import inspect
import fcntl
import shutil
import logging
import argparse
import random
from pathlib import Path
from datetime import datetime
import asyncio
from concurrent.futures import ThreadPoolExecutor

#import psutil

from watchdog.observers import Observer
from watchdog.events import PatternMatchingEventHandler

"""
Dispatch src files to candidate dest directories (sth like load balance)

loop-1: periodically check space availability, select candidate
loop-2: watch src fs, write to candidate
"""

FS_CANDIDATE = {
    "/data1",
    "/data2",
    "/data3",
    "/data4",
}

SELECT = None

Threshold = 256 * 1024 * 1024 * 1024 # 256G

def doCheck(fs):
    checkfile = Path(fs).joinpath(".check")
    logging.info(f"check {checkfile}")
    removeit = False
    alive = False
    try:
        # TODO handle proc D state
        if shutil.disk_usage(fs).free < Threshold:
            removeit = True
        else:
            alive = True
        # TO wakeup and check drive
        checkfile.write_text(str(datetime.now()))
    except Exception as e:
        logging.error(f"{e}")
        removeit = True
    else:
        alive = True
    return removeit


async def checkCandidate():
    global ACTUAL_CANDIDATE
    global SELECT
    loop = asyncio.get_event_loop()
    pool = ThreadPoolExecutor()
    while True:
        logging.info(f"select {SELECT}")
        today = datetime.now().day
        ACTUAL_CANDIDATE = FS_CANDIDATE.copy()
        tasks = {}
        for fs in FS_CANDIDATE:
            task = loop.run_in_executor(pool, doCheck, fs)
            tasks[task] = fs
        done, pending = await asyncio.wait(tasks.keys(), timeout=600)
        for task in done:
            if task.result():
                ACTUAL_CANDIDATE.remove(tasks[task])
        for task in pending:
            # TODO not work to kill doCheck
            task.cancel()
        SELECT = sorted(ACTUAL_CANDIDATE)[today % len(ACTUAL_CANDIDATE)]
        logging.info(f"candidates: {ACTUAL_CANDIDATE}")
        await asyncio.sleep(20)


# TODO wrap into asyncio
class _FileHandler(PatternMatchingEventHandler):
    def __init__(self, listen='/', **kwargs):
        self.listen_dir = Path(listen).resolve()
        super().__init__(**kwargs)

    def on_created(self, event):
        logging.info("created {}".format(event.src_path))
        newfile = Path(event.src_path).resolve()
        relative = list(newfile.parts)[len(self.listen_dir.parts):]
        dest = Path(SELECT).joinpath(*relative)
        try:
            dest.parent.mkdir(exist_ok=True, parents=True)
            logging.info("move to {}".format(shutil.move(newfile, dest)))
        except Exception as e:
            logging.error(f"{e}")


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S')

    if not hasattr(sys.modules[__name__], '__file__'):
        __file__ = Path(sys.argv[0]).stem

    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--monitor", help="monitor path", default="/tmp/data")
    parser.add_argument('-f', '--fs', help=f'candidate dir to be used, default is {FS_CANDIDATE}', action="append", nargs='+')
    parser.add_argument('-p', '--pidfile', help=f'pidfile to check singleton', type=str, default=f"/tmp/{Path(__file__).stem}.pid")
    parser.add_argument('--threshold', help=f'free space to be ensured in bytes for each dir, default: {Threshold}', type=int, default=Threshold)

    args = parser.parse_args()
    print(args)

    try:
        f = open(args.pidfile, mode='w')
        fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        f.write(str(os.getpid()))
        os.chmod(args.pidfile, 0o666);
    except BlockingIOError as e:
        logging.error(f"{__file__} only one process allowed, found another one")
        sys.exit(-1)
#        raise e
    except Exception as e:
        pass

    if args.fs is not None:
        for f in args.fs:
            FS_CANDIDATE.add(f)
    SELECT = sorted(FS_CANDIDATE)[datetime.now().day%len(FS_CANDIDATE)]

    listen_dir = args.monitor
    observer = Observer()
    event_handler = _FileHandler(patterns=["*.fit*", "*.fits.fz"], listen=listen_dir)
    observer.schedule(event_handler, listen_dir, recursive=True)
    observer.start()

    loop = asyncio.get_event_loop()
    asyncio.ensure_future(checkCandidate())
    loop.run_forever()
