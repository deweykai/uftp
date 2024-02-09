FROM ubuntu

WORKDIR /code

RUN apt update && apt install -y gcc libc-dev make valgrind

COPY . .

RUN make clean && make all