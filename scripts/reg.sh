#!/bin/bash
insmod source/nifs.ko
dmesg | tail -4
