# libsffcmis

libsffcmis is a lightweight C library designed to parse and manage the CMIS and SFF data
structures found in optical modules such as **SFP**, **SFP+**, **QSFP**, **QSFP28**, and
other transceivers that support I²C-based access.

## 🔧 Hardware Recommendation

For development and testing, it is recommended to use a CH341-based USB-to-I²C adapter.  
Example: [CH341 USB I2C Adapter on AliExpress](https://fr.aliexpress.com/item/1005006659842448.html)

## 🛠️ Design

This library was initially based on the `ethtool` codebase but has been significantly simplified
and refactored to rely on a direct I²C interface, making it suitable for userland or embedded
use without requiring kernel drivers.

## 🚀 Usage

### 🔨 Compile and Install

To build and install the library and associated CLI tool:

```sh
make
sudo make install
````

> By default, this installs the `libsffcmis` library and the `sffcmis_test` binary to your default system path.

### 🧪 Read SFP/QSFP EEPROM Data

To run the test tool and parse the contents of a connected optical module:

```sh
sudo sffcmis_test 10
```

Where `10` corresponds to the I2C bus number (e.g., `/dev/i2c-10`).

### 📖 What It Does

`sffcmis_test` performs the following:

* Opens the specified `/dev/i2c-N` adapter
* Probes standard SFF pages or CMIS pages
* Parses and prints key fields such as:
  * Module identifier, connector type
  * Vendor name, part number, serial number, date code
  * Supported transceiver types and encoding
  * Wavelength, nominal bitrate, and diagnostics (if available)

### 🛠️ Example Output

```
sudo sffcmis_test 10
i2c_get_device_desc: Adapter: CH341 I2C USB bus 001 device 003, Device address: 0x0, tenbit: False, internal(word) address: 1 bytes, page max 8 bytes, delay: 1ms, chunk: 32bytes
	Identifier                                : 0x11 (QSFP28)
	Extended identifier                       : 0xcc
	Extended identifier description           : 3.5W max. Power consumption
	Extended identifier description           : CDR present in TX, CDR present in RX
	Extended identifier description           :  High Power Class (> 3.5 W) not enabled
	Power set                                 : Off
	Power override                            : Off
	Connector                                 : 0x0c (MPO Parallel Optic)
	Transceiver codes                         : 0x80 0x00 0x00 0x00 0x00 0x00 0x00 0x00
	Transceiver type                          : 4x10G Ethernet: 10G Base-SR
	Encoding                                  : 0x05 (64B/66B)
    ...
```

### 📦 Developer Integration

You can integrate `libsffcmis` in your own C applications:

```c
#include <sffcmis.h>

    ...
	struct cmd_context ctx;
	ctx = (struct cmd_context){
		.bus_num = 10, // /dev/i2c-10
	};
	i2c_init(&ctx);
	int ret = eeprom_parse(&ctx);
    ...
	printf("i2c_get_device_desc: %s\n", i2c_get_device_desc(ctx.device, desc, sizeof(desc)));
```

```
gcc -o demoapp demoapp.c -lsffcmis
```

## ⚠️ WARNING: Work in Progress

This library is currently under active development and **not yet run-ready**.  
Use at your own risk.
