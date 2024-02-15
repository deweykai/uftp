## Building

This project is compiled using `make`. It expects the `gcc` compiler to be installed and up to date. Python is used for testing, but is not required for the programs to run.

```bash
# Install dependencies
sudo apt update;
sudo apt install gcc libc-dev make python3;

# Build the project
make all
```

After compiling, there will be two executables in the root directory of the project: `ftp_server` and `ftp_client`.

## Server

**Usage:**

```bash
./ftp_server <port>
```

## Client

This program starts a repl that supports basic ftp commands like `ls`, `get`, `put`, `delete`, and `exit`.

**Usage:**

```bash
./ftp_client <server-ip> <server-port>
```

## UDP communication

This project uses a custom communication functions built on top of the UDP protocol. These functions are found in `my_udp.c` and `my_udp.h`.
The main functions are `send_data` and `recv_data`, functionally, they are similar to `sendto` and `recvfrom`, but with added logic to support large data transfers and reliability.

A transfer is initiated by sending a `START` packet, which contains information about how much data is being sent. Then the data is sent in chunks to the receiver. After all chunks are sent, an `END` packet is sent so make sure that the final chunk was received.

After all every packet, the sender expects to receive an `ACK` packet from the receiver. If the sender does not receive an `ACK` packet, it will resend the packet. For the `START` and `END` packets, the sender will wait for an `ACK` packet to be received before sending the next packet.

The `DATA` packets are sent using the GO_BACK_N algorithm so larger files can be sent faster. The number of packets sent changes dynamically based on how many packets are dropped.

Since packets may be lost, the sender has a timeout period for receiving an the next packet. This can be used to detect if the client was dropped, or if a packet was lost so it can be resent.

## Testing

All ftp commands have been tested using under the following emulated network conditions on the server side:

-   10% packet loss
-   200ms delay

Using the following command located in the startup script `run.bash`

```bash
tc qdisc add dev eth0 root netem delay 200ms loss 10%
```

The client was started using the `client_test.py` script, which runs every supported command. The output is then manually checked. The script moved a png file to and from the server during the test.
