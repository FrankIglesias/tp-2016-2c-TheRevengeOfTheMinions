#!/bin/bash
cd so-commons-library/
sudo make install
cd ..
sudo apt-get install libncurses5-dev
sudo apt-get install apt-file
sudo apt-file update
sudo apt-get install sshfs 
sudo modprobe fuse 
sudo addgroup yourusername fuse 
sudo apt-get install openssl
sudo apt-get update
sudo apt-get install libssl-dev
sudo apt-get update
cd massive/
gcc massive-file-creator.c -o ejecutable -lcrypto -lpthread
cd ..
cd librerias-sw
cd Debug
make clean
make all
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utnso/git/tp-2016-2c-TheRevengeOfTheMinions/librerias-sw/Debug