#!/usr/bin/env bash

if [[ $(id -u) -ne 0 ]]; then
	echo "Please run as root (with sudo)"
	exit 1
fi

DISTRO="buster"
if [[ -e "/etc/os-release" ]]; then
	DISTRO=$(cat /etc/os-release | grep VERSION_CODENAME | cut -d'=' -f2)
else
	echo "*** WARNING *** something don't smell right. Is this debian flavoured?"
fi
echo "THIS IS $DISTRO"

echo "Creating iio group"
groupadd iio

echo "adding $(logname) to iio group"
usermod -aG iio $(logname)

echo "copying 90-iio.rules to /etc/udev/rules.d/"
cp 90-iio.rules /etc/udev/rules.d/

CONFIG_PATH="/boot/config.txt"
OVERLAY_ENABLE="mpu9250,addr=0x68,int_pin=4"

if [[ $DISTRO == "bookworm" ]]; then
	CONFIG_PATH="/boot/firmware/config.txt"
	OVERLAY_ENABLE="i2c-sensor,$OVERLAY_ENABLE"
elif [[ $DISTRO == "bullseye" ]]; then
	OVERLAY_ENABLE="i2c-sensor,$OVERLAY_ENABLE"
elif [[ $DISTRO == "buster" ]]; then
	if [[ -z $(which dtc) ]]; then
		echo "device-tree-compiler not installed, installing now"
		apt-get install -y device-tree-compiler
	fi

	echo "compiling dt overlay"
	dtc -@ -Hepapr -I dts -O dtb -o mpu9250.dtbo mpu9250-overlay.dts

	echo "moving mpu9250.dtbo to /boot/overlays/"
	mv mpu9250.dtbo /boot/overlays/
else 
	echo "*** WARNING *** unsupported release. Thar be dragons afoot!"
fi

echo "adding device tree overlay enable to $CONFIG_PATH"
echo "dtoverlay=$OVERLAY_ENABLE" | tee -a $CONFIG_PATH

echo "Installing libiio + utils + python binding"
apt-get install -y libiio-dev libiio-utils python3-libiio

echo "Finished! Reboot required. Do you want to reboot now? [Y/n]"
read response

if [[ -n $response && !($response =~ ^[Yy]) ]]; then
	echo "Okay then. That was always allowed."
	exit 1
fi

echo "Rebooting in 2 seconds"
sleep 2
systemctl reboot
