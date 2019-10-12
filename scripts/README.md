(Note: some scripts need some modification)

- ~~`should_i_start.*` Control take frames in half an hour period~~
- ~~`mount_check` Check free disk usage~~
- `auto_expo.service` Auto expo
- ~~`auto_expo_start.timer` trigger `auto_expo.service` in time range~~
- ~~`auto_expo_stop.timer` stop `auto_expo` in other time~~
- `file_dispatch.py` monitor src dir, dispatch files to candidate destination, need watchdog, can be compiled to binary with Makefile(need cython)
- `expo_trigger@.service` send http request to trigger `auto_expo` to take expo (note. need `auto_expo` keep running)
- `loop_expo_trigger.timer` In observing days, keep taking expo.
- `periodic_expo_trigger.timer` In idle days, take some images periodically (just wake up cam)

`mount_check` and `file_dispatch`, just select one (at present, `file_dispatch` selected) to use:

Strategy of `mount_check`: keep symlink name fixed, but change the target of symlink when currently used candidate dir has no more space. 

Strategy of `file_dispatch`: sth like load balance, monitor source directory, move created file to selected candidate directory. Prefer to move files to different directory in different day.


