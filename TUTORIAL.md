Description for Taishan-2019-2020

## TL;DR
Just check all hardware power state and cable connection state, and reboot the system when you have problem.

If you want to manual focus. Just use ASICAP under ~/ASICAP. It is recommended to invoke `sudo systemctl stop auto_expo` before starting ASICAP, and MUST remember close ASICAP and then `sudo systemctl start auto_expo` when manual focus finished.

MUST remember close ASICAP when manual focus finished.
MUST remember call `sudo systemctl start auto_expo` after focus.

## Timer and Services
(NOTE These timers and services are **Auto started** at system booting time, already configured)

- `gpsd.service` execute `gpsd` to communicate with gps hardware.
- `chronyd.service` perform time sync based on gps.
- `auto_expo.service` keep `auto_expo` running (could auto restart)
- `file_dispatch.service` call `file_dispatch` to move output images at tmpfs into different directories based on days.
- `expo_trigger@.service` send expo i images command to `auto_expo` web server.
- `periodic_expo_trigger.timer` take some images per 20 min
- `loop_expo_trigger.timer` keep looping expo from 04-15 to 09-30

## Check system service logs

`journalctl -f`: keep tracking latest system logs
`journalctl -u <service_name> -f`: track particular service logs


## Check GPS and NTP

Call `cgps` or `gpsmon` to check gps state.
Call `chronyc sources -v` to time sync state.

## Control auto expo behavior

Stop expo: `curl localhost:9116/stop_expo`
Take 2 expo: `curl localhost:9116/start_expo/2`
Take expo forever: `curl localhost:9116/start_expo/0`
