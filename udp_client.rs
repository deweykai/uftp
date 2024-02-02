use std::net::UdpSocket;

fn main() -> std::io::Result<()> {
    {
        let socket = UdpSocket::bind("0.0.0.0:0")?;

        let buf = "hello".bytes().collect::<Vec<_>>();
        socket.send_to(&buf, "127.0.0.1:1234")?;

        let mut buf = [0; 10];
        let (amt, src) = socket.recv_from(&mut buf)?;
        buf[amt] = b'\0';

        let buf_str = std::str::from_utf8(&buf).expect("Not UTF-8");
        println!("[{}] {:?}", &src, buf_str); // Print buf as a string
    } // the socket is closed here
    Ok(())
}
