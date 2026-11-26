# Network play: what TM2.EXE asks of DirectPlay, and how the host can provide it

Goal: online multiplayer that runs the 1997 network code as it is, with DirectPlay supplied by the
host (src/host/dx_dplay.c, today a stub: `DirectPlayEnumerateA` lists no service provider, so Multi
Player goes back to the title, and `DirectPlayCreate` fails). The plan is a host side "Internet
TCP/IP" service provider over UDP plus a small announce / rendezvous server.

Everything below comes from the verified ports in src/ (emulator diffs against TM2.EXE) and the
notes in the decomp repo's tools/names/verified.tsv. "Unknown" means the ported code does not
settle it.

## 1. The DirectPlay surface the game uses

### Objects and where the game keeps them

The network object is the global 0x0050f9a8, built before WinMain by the static constructor
0x00463240 -> session_ctor 0x004aa3c0(0x0050f9a8, 0x004e5654) (src/platform/pf7_small.c:96,
src/platform/pf8_boot.c:34).

| Field | Global | What |
|---|---|---|
| +0x00 | | pointer to the application GUID 0x004e5654 = `{EA190140-CE70-11CF-A136-68C6FFC00000}` |
| +0x0c | | 1 = we are the host (0x004aaa5c reads it; 0x004aaa60 sets it on host migration) |
| +0x10 | | in a session (Open worked) |
| +0x18 | | a DirectPlay object exists (0x004aaa58 = "networked") |
| +0x1c | | game mode (0x004aaa74 sets it and the session record's +0x58) |
| +0x20 | | number of service providers found at boot |
| +0x24 | 0x0050f9cc | **IDirectPlay3A** (QueryInterface result; every session call goes here) |
| +0x28 | | **IDirectPlay** (v1, the `DirectPlayCreate` result, kept for EnableNewPlayers) |
| +0x2c | 0x0050f9d4 | service provider list (up to 20 entries of 0x98 bytes) |
| +0x30 | 0x0050f9d8 | session record / enumerator (0x25c bytes, src/platform/pf8_boot.c:68): +0 the DirectPlay used by the enumeration thread, +4 app GUID, +8 a DPSESSIONDESC2 for EnumSessions, +0x58 mode, +0x5c session name, +0xdc password, +0x1dc arena, +0x1f0 thread, +0x1f4/+0x1f8/+0x1fc mutexes, +0x208 thread state, +0x210 EnumSessions flags (= 1), +0x214 session list, +0x224 list box, +0x22c a DPCAPS, +0x254 / +0x258 error / success callbacks |
| +0x34 | 0x0050f9dc | player table (0x168 bytes): +0x24 count, +0x28 remote count, +0x30 **the local player's 0x138 byte data record** (+0x30 = its DPID) |
| +0x38 | 0x0050f9e0 | lobby player list (keyed list, nodes are 0x138 byte records, next at +0x134) |

The 0x138 byte player record (local copy at player table +0x30, remote copies in the lobby list
and the player table): +0x00 DPID, +0x08 ready, +0x0c host flag, +0x10 "has the game CD",
+0x14 short name (0x80), +0x94 long name (0x80), +0x114 slot index (0 = local), +0x118 lobby
state (1 not ready, 2 ready, 4 host, 5 enumerated), +0x11c name slot, +0x120..+0x12c (copied by
0x004aa974; meaning unknown), +0x130 car slot state, +0x134 list link (a game pointer; receivers
keep their own). Sources: src/platform/pf8_dplay.c:6, src/engine/en4_io.c:124-160,
src/match/mt2_net.c:237-249, src/platform/pf8_netrun.c:44-72.

Note: several ports carry DirectSound names for what are DirectPlay functions (the engine was
named before the network code was understood):

| Address | Port name | Actually |
|---|---|---|
| 0x004aa524 | `en7_dsound_create` (src/engine/en7_a.c:117) | select provider: `DirectPlayCreate(guid, &+0x28, NULL)` (`EN7_DirectSoundCreate` is the DPLAYX thunk 0x004d1908), then `+0x28->QueryInterface(IID 0x004e51f4, &+0x24)`, +0x18 = 1 |
| 0x004aa5b8 | `en4_dsound_sound_free` (src/engine/en4_io.c:361) | leave and free: IDirectPlay3A DestroyPlayer (+0x24)(local id), Close (+0x10), Release (+0x08), then the v1 Release |
| 0x004aa638 | `en4_dsound_buffer_release` (src/engine/en4_obj.c:297) | leave: DestroyPlayer (+0x24)(local id), Close (+0x10), no Release |
| 0x00499884 | `reg_window` in src/match/mt2_net.h:82 | `find_cd_drive` (tm2.ico on a CD-ROM drive): the "has CD" flag |

### Entry points (DPLAYX.dll by ordinal)

| Import | Thunk / slot | Called from | Arguments | Expects |
|---|---|---|---|---|
| #2 `DirectPlayEnumerateA` | 0x004d1902 / 0x00c907d4 | boot, 0x004acdb8 (src/platform/pf8_boot.c:101) | (callback 0x004acf14, list) | callback (stdcall `guid*, name, major, minor, ctx`, ret 0x14) once per provider; the game stores the **GUID pointer** (entry +0), not the GUID, so it must stay valid in game memory for the whole run. Name copied with lstrcpyA into 0x80 bytes. Up to 20. |
| #1 `DirectPlayCreate` | 0x004d1908 / 0x00c907d8 | boot probe 0x004acb70 (pf8_boot.c:148) and provider select 0x004aa524 | (guid*, IDirectPlay** out, NULL) | DP_OK and an IDirectPlay. The boot probe calls Close (v1 +0x10) and Release (+0x08) at once. |

A provider is only listed if (a) its name contains "Modem", "IPX" or "TCP" (strstr 0x004bc710 with
0x004e35d0 / d6 / da; without a match the probe is skipped and entry +0x8c stays 0) and (b) the
boot `DirectPlayCreate` with its GUID succeeded (+0x8c = 1); 0x004ace8c lists entries with
+0x8c == 1. A provider whose name is exactly "Serial Connection For DirectPlay" (0x004e1428) gets
a message box when chosen (src/match/mt2_netui.c:130). The chosen name is saved in the registry
as NetProviderName and reselected next time.

### IDirectPlay (v1, net +0x28)

| Slot | Method | Use |
|---|---|---|
| 0x00 | QueryInterface | IID 0x004e51f4 = `{133EFE41-32DC-11D0-9CFB-00A0C90A43CB}` = **IID_IDirectPlay3A** (0x004aa524) |
| 0x08 | Release | 0x004aa5b8, boot probe |
| 0x10 | Close | boot probe only |
| 0x28 | EnableNewPlayers(BOOL) | 0x004aaa88 passes 0, 0x004aaacc passes 1 (src/engine/en4_widget.c:204, only when net +0x24 is set); failures are only named, never acted on |

EnableNewPlayers(FALSE) runs on every machine once the lobby is left for a match
(src/platform/pf8_netrun.c:169, 0x004aaa88), and from the lobby's lock toggle (0x00467638,
src/platform/pf8_netmsg.c) through net_set_flag 0x004671e8 (v == 1 -> FALSE); that toggle also
sends game message 0x0e so the other machines call it too (they are not hosts; whatever DirectPlay
answers them is ignored).

### IDirectPlay3A (net +0x24 / 0x0050f9cc)

Only IDirectPlay2 range slots are called (none of the IDirectPlay3 additions at 0x80 and above:
no EnumConnections / InitializeConnection, no lobby, no SetSessionDesc / GetSessionDesc).

| Slot | Method | Caller | Arguments the game passes | What it does with the result |
|---|---|---|---|---|
| 0x08 | Release | 0x004aa5b8, host failure (en7_h.c:91) | | |
| 0x10 | Close | 0x004aa5b8 / 0x004aa638, open failures (en7_f.c:55) | | |
| 0x18 | CreatePlayer | 0x004988e8 (src/engine/en4_io.c:188) | `(&id = table +0x30, &DPNAME{0x10, 0, short = table +0x44, long = table +0xc4}, hEvent 0, data = table +0x30, 0x138, flags 0)` | the DPID is written straight into the local record's +0; failure closes the session |
| 0x24 | DestroyPlayer | 0x004aa5b8 / 0x004aa638 | `(local DPID)` | ignored |
| 0x30 | EnumPlayers | 0x00498508 (pf8_dplay.c:51): `(NULL, cb 0x004989d0, table, 0)`; 0x00497c7c (en4_list.c:63) mode 1: `(&guidInstance, cb 0x004982d4, list, 0x80 DPENUMPLAYERS_SESSION)` (join screen, for a session not joined), mode 2: `(NULL, cb 0x004982d4, list, 0x20 DPENUMPLAYERS_GROUP)` (lobby refresh 0x004aa900) | callbacks are EnumPlayersCallback2 (`id, type, DPNAME*, flags, ctx`, stdcall); they copy the DPNAME short (+8) / long (+0xc) names; 0x004989d0 stops at 8 players |
| 0x34 | EnumSessions | 0x004ab43c (en7_a.c:145) | `(desc = rec +8: dwSize 0x50, guidApplication, lpszPassword = rec +0xdc; timeout 1000; cb 0x004ac2f0; rec; flags rec +0x210 = 1 DPENUMSESSIONS_AVAILABLE)` | synchronous. Nonzero result: named and the error callback runs, which **cancels the join screen** (0x00468850); 0 runs the success callback (0x00468988, restores a minimized window: the real TCP/IP provider's address dialog minimized the game). The callback (en7_g.c:71) returns 0 on DPESC_TIMEDOUT (flags & 1), else keeps per session: guidInstance, dwMaxPlayers, dwCurrentPlayers, lpszSessionName, dwUser1..4 |
| 0x38 | GetCaps | 0x004aa690 after Open(CREATE) (en7_h.c:98) | `(rec + 0x22c, 0)` | result ignored; dwSize is never set by the code seen; nothing found that reads the DPCAPS back |
| 0x60 | Open | host 0x004aa690 (en7_h.c:70), join 0x004aa7dc (en7_f.c:71) | see below; flags 2 DPOPEN_CREATE / 1 DPOPEN_JOIN | failure: Release (host) or Close + Release (join), HRESULT returned to the UI |
| 0x64 | Receive | pump 0x00463f78 (src/match/mt_net.c:112) | `(&from, &to, 1 DPRECEIVE_ALL, 0x0050f150, &size = 0x400)` | loops until nonzero; 0x887700BE DPERR_NOMESSAGES ends quietly, anything else is named and also ends the pump for this call |
| 0x68 | Send | 0x00463e38 / 0x00463d9c (mt_net.c:36-57) | `(from = local DPID, to = 0 (all) or a DPID, flags 1 DPSEND_GUARANTEED or 0, 0x0050f150, size)` | retried in a tight loop while it returns 0x8877010E DPERR_BUSY; other results ignored |
| 0x74 | SetPlayerData | lobby 0x00465f60 / 0x004672fc, system messages 3 and 0x101 (mt2_net.c:52) | `(local DPID, table +0x30, 0x138, 2 DPSET_GUARANTEED)` | ignored; the game relies on the other machines getting DPSYS_SETPLAYERORGROUPDATA |

Session descriptions (DPSESSIONDESC2, 0x50 bytes):

- Host, Open(CREATE) (en7_h.c:70-94): dwFlags 0x44 = DPSESSION_MIGRATEHOST | DPSESSION_KEEPALIVE,
  guidApplication = the app GUID, dwMaxPlayers **8**, lpszSessionName = rec +0x5c, lpszPassword =
  rec +0xdc, dwUser1 = game mode (5 Tournament, 6 Challenge, 7 Endurance), dwUser2 = arena id,
  dwUser3 = 3 (constant; maybe a version tag), dwUser4 = timeGetTime().
- Join, Open(JOIN) (en7_f.c:71-95): only dwSize, guidInstance (from the enumerated session),
  guidApplication and lpszPassword (rec +0xdc, the join screen's text field 0x0050fab4). Before
  Open the record takes the enumerated session's name, dwUser1 (mode) and dwUser2 (arena): **the
  client's game mode and arena come from the enumerated session description, not from a game
  message**, so the announce server must hand back dwUser1/dwUser2 exactly.

### System messages handled (from == 0, 0x00464088, src/match/mt2_net.c:206-250)

| dwType | DirectPlay | Fields read | Game action |
|---|---|---|---|
| 0x0003 | DPSYS_CREATEPLAYERORGROUP | +4 dwPlayerType (1 = player) | player: CD check, local record +0x10, **SetPlayerData of our own record** (so the newcomer gets everybody's data as 0x102 messages); then all barrier state reset |
| 0x0005 | DPSYS_DESTROYPLAYERORGROUP | +8 dpId | barrier acks of that slot cleared; player removed from the table; in a match its car gets +0x1d8 = -1, HUD message 0x3b, the car leaves |
| 0x0101 | DPSYS_HOST | none | we are the host now (net +0x0c = 1), broadcast game message 0x17, SetPlayerData, barrier reset |
| 0x0102 | DPSYS_SETPLAYERORGROUPDATA | +8 dpId, +0xc lpData (a pointer, followed as a **game address**) | 0x138 bytes copied into that player's lobby node (its +0x114 / +0x134 kept); node +0x0c set -> the server id 0x004e7ae0 = dpId |

Not handled (silently dropped by the game): DPSYS_SESSIONLOST (0x31), SETPLAYERORGROUPNAME,
ADDPLAYERTOGROUP, chat, SETSESSIONDESC and the rest.

## 2. The multiplayer flow

Screens (src/shell/sh_screen.c, src/match/mt2_netui.c, mt2_ui_net.c, src/platform/pf8_net*.c):

1. **Network setup** 0x00465110 (via screen_network 0x0047a18c): provider list ("none" -> message box
   and back to the title, what Roadkill shows today), player name and callsign (registry
   NetPlayerName / NetPlayerCallsign), Join / Host buttons. Connecting selects the provider
   (0x004aa524) and writes the names into the local record (they become the DPNAME short / long).
2. **Join** 0x00468aa4: starts the enumeration thread 0x004ac134 (0x004abb68; src/engine/en7_g.c:20):
   every 50 ms it alternates EnumSessions (1 s timeout) and, for every listed session,
   EnumPlayers(guidInstance, SESSION). The list shows session names; the info panel shows name,
   arena (with picture), mode, "Max Players", "Num Players" (dwMaxPlayers / dwCurrentPlayers) and
   the session's player names. A text field is the password. Join -> 0x004aa7dc.
3. **Host** 0x00467740: session name (required, "Session must have a name to start!"), password,
   mode buttons Tournament / Challenge / Endurance (game types 5 / 6 / 7), arena arrows (arenas
   10..12 need unlocks). Start -> 0x004aa690. Name and mode saved (NetSessionName / NetGameMode).
4. **Lobby** 0x00465f60 (via 0x0047a308): player list with ready marks, chat to all (kind 2) or to
   selected players (kind 3), host buttons Start, Kick (kind 0x0d to the selected players:
   "Host has removed you from the game.") and the EnableNewPlayers toggle; client button
   Ready / Not ready. The host's Start is enabled only when every row is ready (0x00467244) and
   is refused when there are too few CDs: **one game CD per three players** ("Must have at least
   %d CDs for %d Players", 0x004672fc, counts records with +0x10 set). Start broadcasts kind 0x15;
   clients leave the lobby with result 5.
5. On leaving with a game (src/platform/pf8_netrun.c:177-200): players re-enumerated, copied into
   the player table (0x004aa974), EnableNewPlayers(FALSE), game state: humans = number of session
   players, **opponents = 0** (0x00624eaa = 0, opponent cars 0xff), every human mode 5.
6. **Car select** 0x0047baa0 (src/shell/sh3_car_select.c:286-320): barrier 0x13, then each car pick
   asks the host for it (net_join 0x004637c4, kinds 4..8): the host grants a car once ("car is
   already selected" otherwise).
7. **Car wait screen** 0x0047e010 (barrier 0x10), then **sessionRun** 0x00475d34: cars loaded
   (barrier 0x11, src/shell/sh2_flow.c:172), round start (src/combat/cb_round_start.c:17-31:
   barriers 0x0d and 0x0e; between them the host picks spawn points 0x004327fc, clients fetch each
   car's spawn word +0x208 from the host by humanCarsRestart 0x0046500c, kinds 0x18 / 0x17 /
   0x11 / 0x12), the frame loop rtMainLoop (barrier 0x0f, src/match/mt2_rt.c:51).
8. Round end (src/shell/sh4_session.c:172-190): barrier 0x12, stats exchange (kinds 0x0f / 0x10,
   0x00464dec), scores table, then a barrier on the next level number. The match ends when fewer
   than 2 players remain.

Answers:

- **Host decides**: session name, password, mode, arena (fixed at Open; never updated with
  SetSessionDesc), when to start, who is kicked, who gets which car, spawn points, barrier
  release, whether new players may join.
- **Max players**: 8 (dwMaxPlayers, the EnumPlayers callback's limit, 8 ack slots per barrier).
  The CD rule caps it further by the CDs present.
- **Bots**: none in network games: the opponent count is forced to 0 (above). Counts are not
  settable.
- **The host is a player**: it always calls CreatePlayer after Open(CREATE) (en7_h.c:99); there
  is no dedicated server mode.
- **Spectating**: no network spectator was found. CreatePlayer passes flags 0 (no
  DPPLAYER_SPECTATOR); spectate_player 0x0045b5a8 / spectate_ai 0x0045b564 belong to the
  watch / attract camera (car_read_input, the demo branch of round start). Whether a dead
  network player gets a watch camera: unknown.
- **No joining a match in progress**: EnableNewPlayers(FALSE) at start; the barriers assume a
  fixed player set.

## 3. The game's protocol on top of DirectPlay

All game messages share the buffer 0x0050f150 (0x410 bytes). Header: +0 u8 type (0x11 or 0x22),
+2 u16 sequence high part, +8 u32 sequence (separate counters per type; src/match/mt_net.c:20-48).
Receivers never check the sequence. Topology is whatever DirectPlay gives: the game addresses
players by DPID, sends "to all" with DPID 0 and expects every peer to hear every other peer
(peer to peer; the host is only special in game logic).

### Type 0x11: control messages, guaranteed (Send flags 1)

0x20 bytes (`+1` kind, words at +0x10 / +0x14 / +0x18 / +0x1c; chat text at +0x20, up to 0x80).
The original leaves the other bytes as stack garbage; the ports send zeros. Handler 0x0046441c
(src/match/mt2_net.c:59).

| Kind | Dir | Words | Meaning |
|---|---|---|---|
| 0 | client -> host (all) | +0x10 barrier | "I reached barrier n" (host marks ack[n][sender slot]) |
| 1 | host -> all | +0x10 barrier | "everybody reached barrier n" (sent 3 times) |
| 2 / 3 | all / one | text +0x20 | chat (public / private) |
| 4 | client -> all | +0x10 car | ask for car |
| 5 | | +0x10 car, +0x14 DPID | release car |
| 6 | host -> asker | +0x10 car | granted |
| 7 | host -> asker | +0x10 car, +0x14 holder | already taken |
| 8 | host -> all | +0x10 car, +0x14 DPID | car taken by DPID |
| 9 / 10 | | | set / clear flag 0x0050f8dc (sender not found in the ported code) |
| 0x0d | host -> one | | you are kicked |
| 0x0e | all | +0x10 v | EnableNewPlayers toggle mirrored |
| 0x0f / 0x10 | | 0x10: three stats words | stats request / answer (round end) |
| 0x11 / 0x12 | client -> host / host -> client | +0x10 DPID, 0x12: +0x14 value | ask / answer a car's spawn word (+0x208) |
| 0x13 / 0x14 | | | peer ready / not ready (lobby) |
| 0x15 | host -> all | | start the match |
| 0x16 | | +0x10 DPID, +0x14 state, +0x18 CD flag | player state (sender not found in the ported code) |
| 0x17 | host -> all / asker | | "I am the server" (server id = sender, barriers reset) |
| 0x18 | client -> all | | "who is the server?" (the host answers 0x17) |

Barriers (net_wait_ready 0x004632f4, src/match/mt_net.c:69): a client sends kind 0 every 300 ms
until it has the host's kind 1; the host counts the acks of the barrier against the session
player count. Ids seen: 0x0d, 0x0e (round start), 0x0f (frame loop), 0x10 (car wait), 0x11 (cars
loaded), 0x12 (round end), 0x13 (car select), the level number (next level). The arrays hold 20
barriers x 8 slots.

### Type 0x22: per frame car state, not guaranteed (Send flags 0)

Sent once per simulated frame by every peer, to all, from worldUpdate when networked
(src/match/mt2_world.c:128-131): snapshot of the local car (0x00473254), pump (0x00463f78), flush
(0x00473534, src/shell/sh_misc.h:38). Payload (at packet +0xc, 0x17 + n bytes; total size
0xc + 0x17 + n, n < 0x3db):

| Offset | Field |
|---|---|
| +0x00 i16 | car id (+0x6f8) |
| +0x02 i16 | health (+0x1d8) |
| +0x04 i16 | speed * 100 / divisor / fps |
| +0x06 i16 | turn rate * 100 / divisor / fps |
| +0x08, +0x0c i32 | x, y (no z) |
| +0x10 i32 | armor |
| +0x14 u16 | 14 flag bits (12 = frozen) |
| +0x16.. | event stream: records `0x2a, cmd, args`, ended by 0x7f |

Event commands applied by 0x00473714 (src/shell/sh4_net_car.c:79-172): 0 car reset, 1 spawn word
for a player (from cars_spawn_events), 2 finished flag, 3 "the player DPID killed me" (the killer
answers with 4 in its next packet), 4 / 5 score counters (0x0050e0b4 / 0x0050e0d4) for the sender,
7 fire guns, 8 fire special weapon slot, 10 / 11 pickup hidden / shown, 12 car flag +0x84d,
13 another weapon action (0x0044dff8). The receiver maps the sender DPID to its slot (+0x114 via
0x00498660) and applies only slots > 0.

### Authority

- **Each peer owns its car**: position, speeds, health and armor of a remote car are overwritten
  from its owner's packets every frame, so the owner's simulation decides damage to itself; its
  death is announced by the victim (0x0045a39c: cmd 5 with its death count and cmd 3 naming the
  killer, src/match/mt_destroyed.c:43-51).
- **Weapons** are replicated as fire events (cmds 7, 8, 13); projectiles are simulated on every
  machine.
- **Pickups**: whoever collects one broadcasts cmd 10; no arbitration (two players can take the
  same pickup).
- **Host**: car picks, spawn points, barriers, start, kick.
- **No AI cars** to own.

## 4. Timing and sync

- **State broadcast, not lockstep.** No frame numbers, no input exchange; each machine seeds rand
  from its own timer (cb_round_start.c:12) and runs its own simulation. Remote cars snap to the
  last packet received (no interpolation); speeds are sent per second (divided by fps) and scaled
  by the receiver's frame ticks, so peers with different frame rates agree on motion. The frame
  rate is refresh / (refresh / 20): 20 at 60 Hz, 25 at 75 Hz (mt2_rt.c:58).
- **Loss**: a lost 0x22 packet loses its events (fire, pickup, kill credit, score); a late packet
  received after a newer one snaps the car back. This is 1997 behavior on the TCP/IP provider too
  (non-guaranteed went over UDP).
- **Timeouts**: the game has none of its own in a match. Barriers wait forever (300 ms resends);
  a vanished peer only goes away through DPSYS_DESTROYPLAYERORGROUP, so the provider's keepalive
  (the session asks for DPSESSION_KEEPALIVE) is what unblocks everyone. DPSYS_SESSIONLOST is ignored.
- **Pumping**: Receive is only called from the lobby loop, barrier loops and once per frame; long
  loads do not pump. Guaranteed messages must queue meanwhile.
- **Host migration**: the new host gets DPSYS_HOST, announces itself (kind 0x17) and barriers
  restart.

## 5. Implementation plan for the host provider

### Interfaces (src/host/dx_dplay.c, on dx_com.h)

- `DirectPlayEnumerateA`: call the callback once (dx_call_cb) with a GUID in persistent fake
  memory and a name that contains "TCP", e.g. the stock "Internet TCP/IP Connection For
  DirectPlay" with DPSPGUID_TCPIP `{36E95EE0-8577-11CF-960C-0080C7534E82}`; version 5.0 (the
  values are stored, not checked).
- `DirectPlayCreate`: a new object with two interfaces sharing one state: IDirectPlay (v1) and
  IDirectPlay3A (answer QueryInterface for IID Data1 0x133efe41; IDirectPlay2A 0x9d460580 and
  IDirectPlay3 0x133efe40 are not asked for). Each interface pointer has its own count
  (v1 Release and v3A Release are both called).
- v1 methods: QueryInterface, AddRef, Release, Close (0x10), EnableNewPlayers (0x28). The other v1
  slots (25 in all) as stubs returning DPERR_UNSUPPORTED (0x80004001) with a log line.
- IDirectPlay3A (47 slots to 0xb8): Close, CreatePlayer, DestroyPlayer, EnumPlayers, EnumSessions,
  GetCaps, Open, Receive, Send, SetPlayerData as described in section 1. Worth having though not
  called: GetPlayerData (0x50), GetPlayerName (0x54), GetSessionDesc (0x58), GetMessageCount
  (0x44). Everything else a logged DPERR_UNSUPPORTED.
- The pf8 ports call through `PF2_COM(PF2_IUNK, slot)` with slots 0x10 (v1 Close, boot probe),
  0x30 (v3A EnumPlayers) and 0x74 (v3A SetPlayerData) (pf8_boot.c:179, pf8_dplay.c:56,
  pf8_netrun.c:31, pf8_netstart.c:101). Only slots 0, 4, 8 are bound today (src/host/dx_com.c:169); these three need
  bindings that look the object up (dx_obj_get(a[0], NULL)) and dispatch by its interface, since
  slot 0x30 is EnumGroups on v1 but EnumPlayers on v3A. The engine and match ports read the fake
  vtable from game memory and need nothing extra.
- Callbacks into the game (enumerate, EnumSessions, EnumPlayers) go through dx_call_cb with the
  DPSESSIONDESC2 / DPNAME and their strings in game memory.

Return codes the game distinguishes: DP_OK, DPERR_NOMESSAGES 0x887700BE (Receive), DPERR_BUSY
0x8877010E (Send: never return it, the caller spins), anything else = failure. A message larger
than the caller's 0x400 bytes would make Receive fail on every call (DPERR_BUFFERTOOSMALL) and
block the queue: keep every delivered message under 0x400 bytes (system messages included).

### Session and player model

- Session: guidInstance, app GUID, name, password, flags (0x44 plus NEWPLAYERSDISABLED from
  EnableNewPlayers), max 8, current players, dwUser1..4, host DPID.
- Players: DPID (host allocated, nonzero, never 0xffffffff; the game uses -1 as "none"), short /
  long name, the 0x138 byte data blob, owning peer. The host keeps the name table and sends it
  to a joiner; Open(JOIN) returns once the joiner has it, so its EnumPlayers(NULL) is complete.
- Receive builds DirectPlay system messages in the caller's buffer the way DPLAYX does, with
  lpData / DPNAME string pointers pointing inside that buffer (the game follows lpData of 0x102 as
  a game address): DPMSG_CREATEPLAYERORGROUP (to existing peers when a player joins),
  DPMSG_DESTROYPLAYERORGROUP (leave, kick, keepalive timeout), DPMSG_HOST (to the new host only),
  DPMSG_SETPLAYERORGROUPDATA (to every peer but the one that set it). from = 0 for these.
- Send(to = 0) goes to every remote player; Send(to = id) to one. A local player is not sent its
  own messages.
- Host migration: a deterministic choice every peer can make (for example the oldest remaining
  player), then DESTROYPLAYER to everyone and DPSYS_HOST to the new host.
- EnumSessions answers immediately from a cache refreshed in the background from the server (the
  enumeration thread calls it about every second, plus EnumPlayers per session), then calls the
  callback once more with DPESC_TIMEDOUT. Waiting 1 s inside the call would hold the runtime's
  host lock unless it is released the way Sleep releases it. It must not fail (the join screen
  closes on errors) and must not show a dialog.
- Threads: EnumSessions / EnumPlayers come from the game's own thread 0x004ac134 while the main
  thread uses Receive / Send: the provider's queues need their own lock, and its socket thread is
  host code that never enters game code. RUNTIME.md:124 says game threads now have their own
  frame slice and call chain; the header comment in src/host/win32_kernel.c:11-17 still says
  "NOT YET SAFE". Check which one is right before relying on it.

### Wire format over UDP (provider level; the game never sees it)

- Datagram header: magic + protocol version, session token, from DPID, to DPID, channel, sequence,
  ack + ack bitfield.
- Channel R (reliable, ordered per peer pair): game sends with DPSEND_GUARANTEED, all provider
  control messages and SetPlayerData. Resend on timeout (RTT based), cumulative acks; ordering
  matters because the game assumes it (data before a destroy, kinds in order).
- Channel U (unreliable): game sends with flags 0. Optionally drop a 0x22 packet older than the
  last one delivered from the same sender (packet +8 is the game's own counter): DirectPlay allows
  dropping non-guaranteed messages, but it changes what players see, so it is a toggle, off by
  default, like every other change from the 1997 game.
- Control: join request (password, names, CD flag not needed: it travels in player data), join
  accept (DPIDs, name table with data blobs, session desc) or refusal (session full, new players
  disabled, wrong password -> Open fails with a DPERR code), player create / destroy, host change,
  keepalive (about 1 s) and leave. Keepalive timeout about 5-10 s, configurable.
- Size: the largest game message is 0xc + 0x17 + 0x3db = 0x3fe bytes, plus the header: under any
  common MTU. About 35 to 100 bytes per frame per peer at 20 to 25 fps to each other peer (7 in an
  eight player game).
- Topology: full mesh (as DirectPlay peer to peer), NAT traversal per pair through the server;
  a pair that cannot connect falls back to relaying through the host or a relay on the server.

### The announce / rendezvous server

- Register (host): guidInstance, app GUID (`{EA190140-CE70-11CF-A136-68C6FFC00000}`), session
  name, password required (a flag; the host checks the password at join), dwMaxPlayers,
  dwCurrentPlayers, dwFlags (hide sessions with new players disabled: the game enumerates with
  DPENUMSESSIONS_AVAILABLE), dwUser1 (mode 5 / 6 / 7), dwUser2 (arena), dwUser3, dwUser4, the
  host's endpoints, plus the player list (DPID, short and long name) for the join screen. Heartbeat
  every few seconds; entries expire.
- List: by app GUID, the fields above. Also worth carrying a Roadkill build / gameplay toggle
  signature at provider level so incompatible builds are not matched; the game would not notice
  a difference until it desyncs.
- Join: the client asks with a guidInstance; the server gives each side the other's public and
  local endpoints at the same time (UDP hole punching), and a relay when punching fails.
- Passwords are not stored or forwarded by the server.

### Risks and unknowns

- EnumSessions with a password filter: how DirectX 5 treated password protected sessions under
  DPENUMSESSIONS_AVAILABLE with the password set in the filter desc is not pinned down here.
  Check the DirectX 5 SDK before deciding what the list shows.
- EnumPlayers flag 0x20 (DPENUMPLAYERS_GROUP) in the lobby refresh: there are no groups, so
  "players only" should be the same; not verified against DPLAYX.
- GetCaps without dwSize: what DPLAYX returned is unknown; the result is ignored.
- EnableNewPlayers on a non-host: unknown what DPLAYX did; ignored by the game either way.
- Senders of kinds 9 / 10 and 0x16 are not in the ported code found so far.
- Meaning of record +0x120..+0x12c and of dwUser3 = 3: unknown.
- Lost or reordered 0x22 packets drop events and snap cars (original behavior). Any fix
  (latest only delivery, sending 0x22 reliably, redundancy) has to be an opt-in toggle.
- Pickups have no authority: races under latency.
- A peer that stops pumping (loading, a modal box) does not ack keepalives at game level;
  keepalive must be answered by the provider's own thread, not by the game's Receive calls.
- The CD rule: every player needs the "has CD" flag (find_cd_drive) or the host cannot start
  with more than 3 players per CD found. Roadkill's disc image answers it; a player without an
  image counts as having no CD.
- No game side timeouts: if the provider fails to report a dead peer, barriers hang forever.
- Interop with a real DirectPlay installation (an original TM2 on Windows) is not possible with
  this wire format and is not a goal.
