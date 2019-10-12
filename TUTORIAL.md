Description for Taishan-2019-2020

## TL;DR
Just check all hardware power state and cable connection state, and reboot the system when you have problem.

If you want to manual focus. Just use ASICAP under ~/ASICAP. It is recommended to invoke `sudo systemctl stop auto_expo` before starting ASICAP, and MUST remember close ASICAP and then `sudo systemctl start auto_expo` when manual focus finished.

MUST remember close ASICAP when manual focus finished.

MUST remember call `sudo systemctl start auto_expo` after focus.

OR JUST REBOOT THE SYSTEM after manual focus.

## Check Camera USB

Call `lsusb`, you will see something like `ID 03c3:071b`, this is camera usb id. You should see two matched lines (for we have two camera).

## Check Storage mount

Call `df -h` you will see `/data1` to `/data4` are mounted.

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

## Check auto expo service work

1. Open terminal invoke `journalctl -u auto_expo -u file_dispatch -f` to track the program logs.
2. Open a new terminal/tab, invoke `curl localhost:9116/start_expo/1`

something like `file_dispatch[1615]: 2019-10-12 20:50:10 - select /data1` shows that image is saved to /data1. 

something like `auto_expo[1021]: [W] [20:53:01.950] [ASICAM1] [tid-1252] save expo 0` shows cam1 take image successfully

`file_dispatch[1615]: 2019-10-12 20:53:03 - move to /data1/2019/1012/20/debris_L1_20191012_205259_647.fits.fz` means image location

## Check GPS and NTP

- Check `lsusb` output for `0403:6001 Future Technology Devices International, Ltd FT232 Serial (UART) IC`
- Call `cgps` or `gpsmon` to check gps state.
- Call `chronyc sources -v` to check time sync state.

## Control auto expo behavior

- Stop expo: `curl localhost:9116/stop_expo`
- Take 2 expo: `curl localhost:9116/start_expo/2`
- Take expo forever: `curl localhost:9116/start_expo/0`
