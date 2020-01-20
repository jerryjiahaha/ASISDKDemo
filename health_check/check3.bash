#!/bin/bash

OUTPUT=()

function execCommand {
	echo check $1, executing $2
	sudo su -c "$2 > $1 2>&1"
	OUTPUT+=($1)
} 

execCommand output_ps_auxf.log "ps -auxf"
execCommand output_ls_home.txt "ls /home/bsst -ltr"
execCommand output_disk_label.txt "ls /dev/disk/by-partlabel -l"
execCommand output_disk_id.txt "ls /dev/disk/by-id -l"
execCommand output_dmesg.log "dmesg -T | head -n 20000"
execCommand output_failed.log "systemctl --failed --all"
execCommand output_journalctl.log "journalctl -r | grep -iE '(zfs|sd[a-z])' 2>&1 | head -n 30000"
execCommand output_journalctl_xe.log "journalctl -rxe | grep -v gpsd | grep -v auto_expo | head -n 20000"
execCommand output_file_dispatch.log "journalctl -ru file_dispatch | head -n 10000"
execCommand output_auto_expo.log "journalctl -ru auto_expo | head -n 10000"
execCommand output_check_file.log "ls -R /tmp/data/2020/01*"
execCommand output_check_archive.log "ls -R /data*/2020/*"
execCommand output_journalctl_lastboot.log "journalctl -b0 | head -n 20000"
execCommand output_chrony.txt "chronyc sources -v"
execCommand output_list_timers.txt "systemctl list-timers --all"

for sdx in /dev/sd?
do
	echo ${sdx:5}
	_out=smartctl_${sdx:5}.log
	sudo smartctl -t short ${sdx} 2>&1 | sudo tee ${_out}
	sudo su -c "smartctl -x ${sdx} >> ${_out} 2>&1"
	sudo su -c "sudo parted ${sdx} print >> ${_out} 2>&1"
	OUTPUT+=(${_out})
done

execCommand zpool_import.txt "zpool import"
execCommand zpool_import_f_z1.txt "zpool import -F taishandataz1"
execCommand zpool_import_f_z2.txt "zpool import -F taishandataz2"
execCommand mount_all.txt "mount -a"
execCommand zfs_mount.txt "zfs mount"
execCommand zfs_mount_a.txt "zfs mount -a"
execCommand zpool_status.txt "zpool status -v -P"
execCommand zfs_list.txt "zfs list"
execCommand zpool_events.txt "zpool events -v 2>&1 | tail -n 20000"

execCommand zpool_scrub_z1.txt "zpool scrub taishandataz1"
execCommand zpool_scrub_z2.txt "zpool scrub taishandataz2"
execCommand zpool_history.txt "zpool history"
execCommand zpool_history_detail.txt "zpool history -i"

execCommand output_lsblk.txt "lsblk -f"
execCommand output_df_h.txt "df -h"
execCommand output_mount.txt "mount"


echo ${OUTPUT[@]}
GEN=check_log_`date +%m%d_%H%M`.tar.gz
sudo tar -czf ${GEN} ${OUTPUT[@]}
echo dumped: ${GEN}
