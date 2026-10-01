"""One-shot TLS server for validating the Vista Schannel compatibility layer."""

import argparse
import socket
import ssl


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="192.168.56.1")
    parser.add_argument("--port", type=int, default=4443)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--tls", choices=("1.2", "1.3"), default="1.3")
    args = parser.parse_args()

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    version = ssl.TLSVersion.TLSv1_3 if args.tls == "1.3" else ssl.TLSVersion.TLSv1_2
    context.minimum_version = version
    context.maximum_version = version
    context.load_cert_chain(args.cert, args.key)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((args.host, args.port))
        listener.listen(1)
        listener.settimeout(90)
        print(f"LISTEN {args.host}:{args.port} TLS {args.tls}", flush=True)
        connection, address = listener.accept()
        print(f"ACCEPT {address}", flush=True)
        with connection:
            with context.wrap_socket(connection, server_side=True) as secure:
                print(f"NEGOTIATED {secure.version()} {secure.cipher()}", flush=True)
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    chunk = secure.recv(4096)
                    if not chunk:
                        raise RuntimeError("TLS client closed before sending HTTP headers")
                    request.extend(chunk)
                    if len(request) > 8192:
                        raise RuntimeError("HTTP request too large")
                if bytes(request) != b"GET / HTTP/1.0\r\nHost: localhost\r\n\r\n":
                    raise RuntimeError(f"Unexpected decrypted request: {request!r}")
                print("REQUEST decrypted and verified", flush=True)
                secure.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nOK")
                secure.unwrap().close()
                print("CLOSE_NOTIFY accepted", flush=True)


if __name__ == "__main__":
    main()
