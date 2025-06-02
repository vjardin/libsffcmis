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

## ⚠️ WARNING: Work in Progress

This library is currently under active development and **not yet run-ready**.  
Use at your own risk.
