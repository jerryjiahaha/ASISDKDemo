#!/bin/bash

PROJ_DIR=/home/bsst/proj/ASISDKDemo
sudo cp -v ${PROJ_DIR}/scripts/Reboot_period@.timer /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable Reboot_period@172800.timer
sudo systemctl start Reboot_period@172800.timer
