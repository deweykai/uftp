FROM ubuntu

WORKDIR /code

RUN apt update && apt install -y gcc libc-dev make valgrind iproute2 python3

COPY . .

RUN make clean && make all