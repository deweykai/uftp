#!/bin/bash

# make the network bad
tc qdisc add dev eth0 root netem delay 100ms loss 1%

# run the server
echo "Running FTP Server"
./ftp_server 1234