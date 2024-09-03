#!/bin/bash 

echo "READ A0"
./i2c -s72 -dw -ib -c2500 3 0x01 0xC4 0xE3

./i2c -s72 -dw -ib -c2500 1 0x00

./i2c -s72 -dr -ib -c2500 2 0x00

echo "READ A1"
./i2c -s72 -dw -ib -c2500 3 0x01 0xD4 0xE3

./i2c -s72 -dw -ib -c2500 1 0x00

./i2c -s72 -dr -ib -c2599 2 0x00