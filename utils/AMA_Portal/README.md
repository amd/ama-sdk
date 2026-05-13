<!-- Copyright(C) 2026 Advanced Micro Devices, Inc. All rights reserved -->

# AMA Portal

## Download

Download AMA Portal from https://www.amd.com/content/dam/account/en/licenses/download/ma35d_ama_portal_1.5.0.zip

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

  This section allows you to run FFmpeg demos such as parallel transcoding while checking the metrics section.

## Supported Kernels and distributions

- Ubuntu 22.04 with generic kernel 5.15.0, 6.8.0
- Ubuntu 24.04 with generic kernel 6.8.0, 6.17.0
- Ubuntu 26.04 with generic kernel 7.x

## Supported AMD AMA Video SDK 

AMA Portal supports up to AMA Video SDK version 1.2.1+

## Requirements & Privileges

AMA Portal requires **elevated privileges** to perform system-level operations such as:
- Installing/updating AMD AMA Video SDK packages
- Flashing firmware on MA35D devices
- Configuring system settings (IOMMU, huge pages)
- System reboot during updates

The .deb package automatically configures the `ama_portal` service to run with the necessary permissions. Upon installation:
1. A dedicated `ama_portal` user is created
2. Passwordless sudo rules are automatically configured for required commands
3. The `ama_portal.service` is enabled and started

**No manual configuration is required** — the package handles all privilege setup automatically during installation.

If you encounter permission errors, verify the service is running:
```bash
sudo systemctl status ama-portal.service
```

If the service is not running, restart it:
```bash
sudo systemctl restart ama-portal.service
```

## VM Support & PCIe Passthrough

AMA Portal automatically detects and supports:
- **Bare Metal installations** - Full access to all PF (Physical Function) devices and all features
- **Virtual Machine with PCIe Passthrough** - Supports both:
  - **PF devices** (passthrough physical functions) - Full functionality available
  - **VF devices** (virtual functions) - Limited functionality; firmware flashing is disabled

The UI dynamically restricts operations based on device type detected in the environment.

## Live Logging

SDK installation and firmware flashing operations feature real-time log streaming:
- Logs are displayed live in the operation progress modal
- Full command output is streamed from backend to frontend
- Logs can be reviewed in `/var/log/ama_portal.log` and `/opt/ama_portal/` 
- Failed operations show detailed error messages with troubleshooting guidance

## Installation

> AMA Portal will be available at http://localhost:50000 after the package installation is complete.

The package creates and configures a systemd service named `ama_portal.service` that is responsible for managing the AMA Portal app. Upon installation, the service is enabled and started, so the app will be available immediately and restart automatically if the system reboots.

AMA Portal is configured to run with Nginx:

- Port 50000: serves static files from `/var/www/ama_portal`
- Port 50001: proxies request to the ama_portal service via a Unix socket (`/run/gunicorn.sock`)

### Installation path

AMA Portal is installed in `/opt/ama_portal/`

## Logging

Application logs are saved on `/var/log/ama_portal.log`

### Operation Logs

Individual operation logs (SDK installation, firmware flashing, validation) are stored in:
- `/opt/ama_portal/operation_logs/` - Raw output from commands
- Live streamed to the UI during operations
- Full history available in application logs

## Features

### System Detection & Validation
- **Auto-detection of environment**: Bare metal vs Virtual Machine (PCIe passthrough)
- **Dynamic OS/Kernel detection**: System Requirements panel shows detected OS, kernel version, and distribution
- **Prerequisites validation**: Real-time checks for BIOS settings, IOMMU configuration, huge pages, and device compatibility
- **Severity categorization**: Prerequisite issues marked as blockers (prevent SDK installation) or warnings (non-critical)

### Device Management
- Intelligent device grouping based on bus topology
- Accurate detection of VM assignments and PCI passthrough configurations
- Device capability reporting (PF vs VF, enabled/disabled status)

### Operations
- **SDK Installation**: Guided installation/update with live progress logging
- **Firmware Flashing**: Multi-device support with live status updates (disabled for VF devices in VM)
- **Validation Testing**: Run functionality tests to verify card acceleration capabilities
- **Metrics & Monitoring**: Real-time monitoring of device utilization, temperature, power consumption
- **Parallel Transcoding Demo**: Run FFmpeg demos showcasing multi-device acceleration with real-time metrics viewing

## Configuration

All the steps executed for the installation of the card(s) are configurable through the `/opt/ama_portal/server/app/_internal/utils/config.json` file, as it's common for there to be changes between AMD AMA Video SDK updates.

## Service Management

AMA Portal runs as a systemd service: `ama_portal.service`

Common commands:
```bash
# Check service status
sudo systemctl status ama-portal.service

# Start/Stop/Restart service
sudo systemctl start ama-portal.service
sudo systemctl stop ama-portal.service
sudo systemctl restart ama-portal.service

# Enable/Disable on boot
sudo systemctl enable ama-portal.service
sudo systemctl disable ama-portal.service

# View live logs
sudo journalctl -u ama-portal.service -f
```

## Uninstallation

To uninstall the AMA Portal, simply run: `dpkg -r ama-portal`

Note that it may be necessary to restart the nginx service: `systemctl restart nginx`
