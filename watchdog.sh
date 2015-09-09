modprobe bcm2708_wdog
echo "bcm2708_wdog" | tee /etc/modules-load.d/bcm2708_wdog.conf
pacman -S watchdog
cat /etc/watchdog.conf
systemctl enable watchdog
systemctl start watchdog.service
