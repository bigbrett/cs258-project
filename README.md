# cs258-project

This repository hosts an attempt to integrate an FPGA-based hardware accelerator for use in the openSSL cryptosystem. The design is intended to run on the Xilinx Zynq SoC, and has been tested for use on the Digilent Zybo board. The directory structure is as follows. 

Note: requires *Vivado Design Suite 2016.4* and a Linux distribution. Has been tested with the Xilinx fork of the linux kernel, which can be found at https://github.com/Xilinx/linux-xlnx, and the Xilinx fork of U-boot which can be found at https://github.com/Xilinx/u-boot-xlnx. However, for testing I used the Digilent (licenced Xilinx silicon partner) forks of the respective Xilinx repos, as they offer board-level support. However, for the sake of compiling and running on an emulation layer, the precise source shouldn't matter, and could even use the mainline kernel release.

All setup instructions largely followed from this lovely tutorial: http://www.dbrss.org/zybo/tutorial4.html

Setup
=========================
Run this set of commands to set up the development
environment:

    bash
    source /opt/Xilinx/Vivado/<Vivado_VERSION>/settings64.sh
    export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
    export ARCH=arm


Partition SDCard
=========
    fdisk /dev/mmcblk0
    mkfs.vfat -n "BOOTFS" -F 32 /dev/mmcblk0p1
    mkfs.ext4 -L "ROOTFS" /dev/mmcblk0p2
    cp sd_card/* /media/BOOTFS

Reset QSPI flash
================

Reset old u-boot configs: connect to device via serial port, press any key during startup
to drop into the u-boot shell, run:

    env default -a
    saveenv

You should now see the prompt as 
`zynq> `
