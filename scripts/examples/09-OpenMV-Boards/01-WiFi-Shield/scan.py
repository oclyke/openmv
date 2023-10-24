# This work is licensed under the MIT license.
# Copyright (c) 2013-2023 OpenMV LLC. All rights reserved.
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# Scan Example
#
# This example shows how to scan for networks with the WiFi shield.

import time
import network

wlan = network.WINC()
print("\nFirmware version:", wlan.fw_version())

while True:
    scan_result = wlan.scan()
    for ap in scan_result:
        print("Channel:%d RSSI:%d Auth:%d BSSID:%s SSID:%s" % (ap))
    print()
    time.sleep_ms(1000)
