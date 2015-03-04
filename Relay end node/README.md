This is a relay end node designed to be used as a remote power strip.

A device class is provided to allow more flexibility in creating future devices. 
The device folder needs to be moved into your arduino library folder.
All that's required is any normal setup for the device (pinMode, i2c, spi, etc) and creating a read and write function.
To register the new device create an instance of it like this:

Device devicename(deviceID, check_periodically?, read_function, write_function(optional));

Then add it to the array of devices. All messages will be parsed automatically.

The node also tries to conserve power by sleeping when not actively doing anything. 
If the node is passively sending data, and does not need to be polled, you can save more power by shutting down the radio between broadcasts.
