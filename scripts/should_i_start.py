#!/usr/bin/env python3

import time
from datetime import datetime
import subprocess
from pathlib import Path

cmdline = ['pidof', 'auto_expo']
autoexpo = ['auto_expo', '-expo_count', '5'] 
SpaceFullIndicator = "no_more_space.flag"

res = subprocess.run(cmdline, capture_output=True)
print(res)
if res.stdout == b'': # No running proc
    if not Path(SpaceFullIndicator).exists(): # Disk not full
        subprocess.run(autoexpo, timeout=300)




