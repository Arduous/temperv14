# temperv14
Temperv14 is a linux tool for Temperature measuring using "TEMPer USB 1.4".

```
$ lsusb |grep Microdia
Bus 001 Device 026: ID 0c45:7401 Microdia TEMPer Temperature Sensor
```

This version based on http://dev-random.net/wp-content/uploads/2013/08/temperv14-1.zip

littleyoda fixed two problems:
* compiler error

    warning: implicit declaration of function ‘isprint’
* runtime usb error

    USB interrupt read: Resource temporarily unavailable
    
    Fatal error> USB read failed

I fixed an additional one with -Waggressive-loop-optimizations
