# cs258-project

This repository hosts an attempt to integrate an FPGA-based hardware accelerator for use in the openSSL cryptosystem. The design is intended to run on the Xilinx Zynq SoC, and has been tested for use on the Digilent Zybo board. The directory structure is as follows. 
 
## cs258proj
	.zybo_linux/ : the source for the linux kernel with driver support for the magical hardware block that is beyond the scope of this class

Note: requires *Vivado Design Suite 2016.4* and a Linux distribution. Has been tested with the Xilinx fork of the linux kernel, which can be found at https://github.com/Xilinx/linux-xlnx, and the Xilinx fork of U-boot which can be found at https://github.com/Xilinx/u-boot-xlnx. However, for testing I used the Digilent (licenced Xilinx silicon partner) forks of the respective Xilinx repos, as they offer board-level support. However, for the sake of compiling and running on an emulation layer, the precise source shouldn't matter, and could even use the mainline kernel release.

All setup instructions largely followed from this lovely tutorial: http://www.dbrss.org/zybo/tutorial4.html. If you wish to build your own version of the linux kernel to run on the Zybo, follow these instructions.

I should note that the general layout of this project, as well as TON of code used came from Lauri Võsandi's project on AES hardware accelerators. This project purely modified his code to work with my IP block, and simplified the driver a bit. In fact, I even retrospectively made some changes to my hardware IP block based on the work that he did, as it exposed me to bus transfer issues I had not though of yet. I do not claim to have originated these ideas, or even a large ammount of code featured in this project. We all stand on the shoulders of giants! I purely integrated it into a (somewhat) functional system. 

The original exposé on Lauri's project can be found on his blog: https://lauri.võsandi.com/tub/aep/applied-embedded-systems-project.html


What I learned
=======================
- how to write linux device drivers
- how openSSL internals work
- how the cryptodev engine API works
- that my thesis is going to be VERY hard

I should note that I never ended up being able to test the openSSL extensions under openVPN because I ran out of time, AND I fried my dev board. However I got the module to compile and openSSL to use it, and I think that is enough for the scope of this project


Setup
=========================
Run this set of commands to set up the development
environment:

    bash
    source /opt/Xilinx/Vivado/<Vivado_VERSION>/settings64.sh
    export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
    export ARCH=arm


Partition SDCard
==================
    fdisk /dev/mmcblk0
    mkfs.vfat -n "BOOTFS" -F 32 /dev/mmcblk0p1
    mkfs.ext4 -L "ROOTFS" /dev/mmcblk0p2

Load SDCard
==================
Copy BOOT.BIN, devicetree.dtb, uImage and uramdisk.image.gz to from zybo_linux/sd_image to the first partition of an SD card, as shown in the first image 
NOTE: The first partition of the SD card is usually mounted to /media/BOOTFS

if you would like to build everything from scratch yourself, then check out the instructions on the drbss link above, or at lauri's blog. 


Reset QSPI flash
================
Reset old u-boot configs: connect to device via serial port, press any key during startup
to drop into the u-boot shell, run:
    env default -a
    saveenv
You should now see the prompt as 
`zynq> `

# BOOTING ON AN EMULATOR

https://github.com/Xilinx/qemu

if you wish to boot using qemu, follow the steps at the link above to get the xilinx fork of qemu, and then run the following command from the qemu directory
	./aarch64-softmmu/qemu-system-aarch64 \
		-M arm-generic-fdt-plnx -machine linux=on \
		-serial /dev/null -serial mon:stdio -display none \
		-kernel <path>/uImage -dtb <path>/devicetree.dtb --initrd <path>/uramdisk.image.gz
and you should now see the system boot on the emulated zynq processor core. Then, all the steps below must be done on the emulated image

*NOTE: I was unable to get the openssl plugins to work on qemu. I have no idea why this is the case, but I couldnt even get my modules to build. When I was working in hardware, I was successful, however the design was in its early stages, and so unfortunately the design as you see it here is not actually validated. I only got simple hard coded values to work in hardware, but it is at least a start*

# Running modified OpenSSL/cryptodev code 
The Cryptodev-linux module has to be used in order to give openSSL access to our accelerators. It enables userspace application access to Crypto API backend modules already present in the kernel.

Since such API is not available by default on Linux distributions, the OpenSSL has to be recompiled with additional flags:

	cd /path/to/cryptodev-linux
	./configure -DHAVE_CRYPTODEV -DUSE_CRYPTODEV_DIGESTS
	make
	sudo make install
	
	apt-get source openssl
	cd openssl-*/
	sed -i -e "s/CONFARGS  =/CONFARGS = -DHAVE_CRYPTODEV -DUSE_CRYPTODEV_DIGESTS/" debian/rules
	dch -i "Enabled cryptodev support"
	debuild
	sudo dpkg -i ../openssl*.deb

# testing speed 
	openssl speed -evp aes-128-cbc -engine cryptodev -elapsed
