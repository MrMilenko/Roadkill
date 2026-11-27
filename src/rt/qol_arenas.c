/*
 * qol_arenas.c: [gameplay] hidden_arenas (qol_arenas.h, docs/QOL.md). Port code, not from the
 * original game.
 *
 * The hidden arenas are levels 10 (SWAMP, "Florida : Suicide Swamp"), 11 (CYB, "Los Angeles :
 * Cyburbia") and 12 (SROOF, picture SC-OROOF.PCX): arenaSetup 0x004576a8 has their terrain,
 * fog and PTS loaders (src/combat/cb2_arena_setup.c). Only the network host screen offers them:
 * its arena arrows (0x004685d4 / 0x00468650, src/match/mt_shell.c) step on to 10 / 11 / 12 only
 * when pickupCounterTake 0x0044eee4(0x23 / 0x24 / 0x25) answers non zero, and its build
 * (0x00467904, src/match/mt2_ui_net.c) keeps a remembered arena above 9 only when 0x1f (debug
 * mode) is on as well. Those table entries (0x004ffedc + 4 * i) are the cheat handler's
 * (0x0044e1cc, src/platform/pf8_cheats.c): DASBAK (code 31, only while 0x004632b0 says a
 * network session is open) turns debug mode on, then JMSWAMP / TMBURBS / TMROOF (35..37, debug
 * mode only) toggle 0x23 / 0x24 / 0x25, BLOWOUT (34) all three.
 *
 * The switch answers those questions with 1 where the game asks them, without writing the
 * table: the typed codes still toggle what they toggle, and debug mode is not turned on for
 * anything else (0x1f answers 1 only inside the host screen's build). The one other reader of
 * 0x23..0x25 is the level select's secret level buttons (0x00477aa0 with the bytes 0x004e805c),
 * which the game sets up only in network game types 5..7, and no screen of the network flow
 * opens the level select. A single player level select has no buttons there (0x004e8060 is
 * 0xff 0xff 0xff) and its wheel (SHELLDB1\LEVELSEL, switch node 9001) has 8 arenas: the
 * original has no way to pick a hidden arena outside a network game, and the switch adds none.
 *
 * Joining machines need nothing: the arena travels in the session record and every machine loads
 * it, cheat or not.
 */
#include "qol_arenas.h"

#include "host/qol/qol.h"

static int in_host_build;

uint32_t qola_counter(uint32_t i, uint32_t v)
{
    if (v || !qol_hidden_arenas()) return v;
    if (i >= 0x23 && i <= 0x25) return 1;
    if (i == 0x1f && in_host_build) return 1;
    return v;
}

void qola_host_build(int begin) { in_host_build = begin; }
