<!-- Copyright(C) 2024 Advanced Micro Devices, Inc. All rights reserved -->

# AMA Portal v1.1.0 Release Notes

## Overview
+ AMA Portal is a web app for AMD users to install and test the capabilities of the Alveo MA35D card(s) on bare metal installations.

## Features in This Release
+ List all connected devices with their firmware versions.
+ Firmware update of all connected devices.
+ Functionalities validation of all connected devices.
+ Installation or update of the AMD AMA Video packages.
+ Thermal and eletrical metrics of each card.
+ Utilization and memory metrics of each device.
+ CPU and memory metrics of the server.
+ Parallel transcoding demo.
+ Picture in Picture (PiP).

## Known Limitations
+ AMA Portal is compatible with Ubuntu 22.04 and 24.04 with kernel version 5.15.0, 5.19, 6.2, 6.5 or 6.8.
+ AMA Portal is compatible with Debian 12 with kernel 6.1.

## Known Issues
+ Demos always use 2 devices even if more than 1 card is connected.
+ If the ffmpeg command of the demo fails, the portal keeps loading and the user has to click "Stop Demo". Command logs are created in: `/opt/ama_portal/demos/output`.