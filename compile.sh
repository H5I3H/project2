#!/usr/bin/env bash
master=rs232_master
slave=rs232_slave
cd ${master}
make
insmod ${master}.ko
cd ${slave}
make
insmod ${slave}.ko
