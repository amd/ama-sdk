<!-- Copyright(C) 2024 Advanced Micro Devices, Inc. All rights reserved -->

# AMA Portal

## Description

AMA Portal is a web app for AMD users to install and test the capabilities of the Alveo MA35D card(s) on bare metal installations.
It has the following sections:

- ### Devices:

  This section is for installing the cards. Here you can:

  - Install or update the AMD AMA Video SDK packages
  - Check the MA35D cards info and their devices
  - Flash the firmware of all the connected cards
  - Validate the acceleration functionalities of all the connected cards

- ### Metrics:

  In this section, you can check hardware metrics of the server and connected cards, such as their electrical info, devices utilization and more.

- ### Demos:

  This section allows you to run the `parallel transcoding` and `pip` ffmpeg demos while checking the metrics section.
  More demos will be available soon!

## Supported Kernels and distributions

- Ubuntu 22.04, 24.04 with generic kernel 5.15.0, 5.19.0, 6.2.0, 6.5.0 or 6.8.0.
- Debian 12 with generic kernel 6.1.

## Supported AMD AMA Video SDK 

AMA Portal was tested with version 1.1.1, 1.2.0 and 1.2.1.

## Installation

> AMA Portal will be available at http://localhost:50000 after the package installation is complete.

The package creates and configures a systemd service named `ama_portal.service` that is responsible for managing the AMA Portal app. Upon installation, the service is enabled and started, so the app will be available immediately and restart automatically if the system reboots.

AMA Portal is configured to run with Nginx:

- Port 50000: serves static files from `/var/www/ama_portal`
- Port 50001: proxies request to the ama_portal service via a Unix socket (`/run/gunicron.sock`)

### Installation path

AMA Portal is installed in `/opt/ama_portal/`

### Logging

Application logs are saved on `/var/log/ama_portal.log`

### Configuration

All the steps executed for the installation of the card(s) are configurable through the `/opt/ama_portal/server/app/_internal/utils/config.json` file, as it's common for there to be changes between AMD AMA Video SDK updates.

## Uninstallation

To uninstall the AMA Portal, simply run: `dpkg -r ama-portal`

Note that it may be necessary to restart the nginx service: `systemctl restart nginx`