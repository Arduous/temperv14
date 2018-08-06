# temperv14
Temperv14 is a linux tool for Temperature measuring using "TEMPer USB 1.4".

```
$ lsusb |grep Microdia
Bus 001 Device 026: ID 0c45:7401 Microdia TEMPer Temperature Sensor
$ ./temperv14 -h
TemperUSB reader version 1.1
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
