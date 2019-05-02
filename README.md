# temperv14
Temperv14 is a linux tool for Temperature measuring using "TEMPer USB 1.4".
Version 2.0 is now relying on libusb-1.0, whereas older version < 2.0 found here and over the internet were relying on libusb-0.1 or its wrapper libusb-compat.

```
$ lsusb |grep Microdia
Bus 001 Device 026: ID 0c45:7401 Microdia TEMPer Temperature Sensor
$ ./temperusb -h
TemperUSB reader version 2.3
      Available options:
          -h help
          -v verbose
          -l[n] loop every 'n' seconds, default value is 5s
          -a[n.n] add a delta of 'n.n' degrees Celsius (may be negative)
          -c output only in Celsius
          -f output only in Fahrenheit
          -m output for mrtg integration
          -d[n] show only device 'n'
```
