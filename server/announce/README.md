# Roadkill announce / rendezvous server

Where Roadkill hosts announce their games and players find them: one UDP port (27400), one
static binary, nothing on disk. It introduces a joining player and the host to each other so they
can reach each other through home routers (UDP hole punching); the game itself then runs peer to
peer between the players. It never carries game traffic and never sees a password.

The public one: **`roadkill.horrible.dev:27400`** (the game's default; `[net] announce` points
elsewhere).

Protocol: `announce_proto.h` (shared with the game's provider). Limits: wrong sized or versioned
datagrams dropped, 4 sessions and 20 requests a second per IP, names forced to printable ASCII,
sessions gone 15 s after their host's last heartbeat.

```sh
docker build -t roadkill-announce server/announce
docker run -d --name roadkill-announce --restart unless-stopped -p 27400:27400/udp roadkill-announce
python3 server/announce/test_announce.py <host> 27400     # checks a running server
```

`ANNOUNCE_LOG=1` (`docker run -e ANNOUNCE_LOG=1 ...`) logs each session and introduction.
