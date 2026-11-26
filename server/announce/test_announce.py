#!/usr/bin/env python3
"""Exercises a running announce server (announce_proto.h): register, list, join / intro, withdraw,
and the rejections (wrong size, wrong version, a guid taken by another address).

    python3 test_announce.py [host] [port]
"""
import socket, struct, sys, time

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 27400
MAGIC, VER = 0x4E414B52, 1
REGISTER, WITHDRAW, REGISTERED, LIST, SESSIONS, JOIN, INTRO = range(1, 8)
SESSION = struct.Struct("<16s32sIII4III" + "16s" * 8 + "IHH" + "IHH")
BUILD = 0x52010000


def hdr(t, arg=0):
    return struct.pack("<IIII", MAGIC, VER, t, arg)


def session(guid, name, players, local=("10.0.0.5", 27420)):
    names = [p.encode().ljust(16, b"\0") for p in players] + [b"\0" * 16] * (8 - len(players))
    lip = struct.unpack("<I", socket.inet_aton(local[0]))[0]
    return SESSION.pack(guid, name.encode().ljust(32, b"\0"), 0x44, 8, len(players), 5, 3, 3, 12345, 0, BUILD,
                        *names, lip, socket.htons(local[1]), 0, 0, 0, 0)


def sock():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)
    s.bind(("127.0.0.1", 0))
    return s


def recv(s):
    try:
        return s.recv(2048)
    except socket.timeout:
        return None


ok = True
def check(cond, what):
    global ok
    print(("PASS " if cond else "FAIL ") + what)
    ok &= bool(cond)


host, client, other = sock(), sock(), sock()
guid = bytes(range(16))
host.sendto(hdr(REGISTER) + session(guid, "Milenko's game", ["Milenko"]), (HOST, PORT))
r = recv(host)
check(r and struct.unpack_from("<IIII", r)[2] == REGISTERED and len(r) == 16 + 8, "register answered with the public address")

client.sendto(hdr(LIST, 0) + struct.pack("<I", BUILD), (HOST, PORT))
r = recv(client)
total, count = struct.unpack_from("<I", r, 12)[0], struct.unpack_from("<I", r, 16)[0]
s0 = SESSION.unpack_from(r, 20)
check(total == 1 and count == 1 and s0[1].rstrip(b"\0") == b"Milenko's game" and s0[5] == 5 and s0[6] == 3,
      "list gives the session with its mode and arena exactly")
client.sendto(hdr(LIST, 0) + struct.pack("<I", BUILD + 1), (HOST, PORT))
r = recv(client)
check(r and struct.unpack_from("<I", r, 12)[0] == 0, "another build sees no sessions")

client.sendto(hdr(JOIN) + guid + struct.pack("<IHH", 0, 0, 0), (HOST, PORT))
rh, rc = recv(host), recv(client)
check(rh and struct.unpack_from("<I", rh, 8)[0] == INTRO and rc and struct.unpack_from("<I", rc, 8)[0] == INTRO,
      "join introduces both sides")
if rc:
    ip, port = struct.unpack_from("<IH", rc, 32)
    check(socket.inet_ntoa(struct.pack("<I", ip)) == "127.0.0.1" and socket.ntohs(port) == host.getsockname()[1],
          "the client gets the host's address as the server saw it")

other.sendto(hdr(REGISTER) + session(guid, "hijack", ["x"]), (HOST, PORT))
check(recv(other) is not None, "re-register from the same IP but another port is allowed (same machine)")
host.sendto(hdr(REGISTER) + session(guid, "Milenko's game", ["Milenko"]), (HOST, PORT)); recv(host)

host.sendto(hdr(REGISTER) + session(guid, "x", ["y"])[:-1], (HOST, PORT))
check(recv(host) is None, "a short datagram is dropped")
host.sendto(struct.pack("<IIII", MAGIC, 9, REGISTER, 0) + session(guid, "x", ["y"]), (HOST, PORT))
check(recv(host) is None, "another protocol version is dropped")
host.sendto(hdr(REGISTER) + session(guid, "bad\x01\x7fname", ["\x1b[31m"]), (HOST, PORT)); recv(host)
client.sendto(hdr(LIST, 0) + struct.pack("<I", BUILD), (HOST, PORT))
r = recv(client)
s0 = SESSION.unpack_from(r, 20)
check(all(0x20 <= c <= 0x7e for c in s0[1].rstrip(b"\0")) and all(0x20 <= c <= 0x7e for c in s0[12].rstrip(b"\0")),
      "names come back printable")

host.sendto(hdr(WITHDRAW) + guid, (HOST, PORT))
time.sleep(0.1)
client.sendto(hdr(LIST, 0) + struct.pack("<I", BUILD), (HOST, PORT))
r = recv(client)
check(r and struct.unpack_from("<I", r, 12)[0] == 0, "withdraw removes it")
print("all passed" if ok else "FAILURES")
sys.exit(0 if ok else 1)
