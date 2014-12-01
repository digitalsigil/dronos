echo 90 > /sys/class/gpio/export
echo 86 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio90/direction 
echo out > /sys/class/gpio/gpio86/direction 
echo 1 > /sys/class/gpio/gpio90/value 
echo 1 > /sys/class/gpio/gpio86/value 
