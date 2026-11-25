# Contributing to Roadkill

Roadkill is a native port of the TM2-Win9x Decomp: Twisted Metal 2 for Windows 95, running on
modern machines. It takes changes through pull requests here.

## The rule: PC behavior by default

Roadkill plays like the 1997 Twisted Metal 2 PC game unless the player switches something on.

- **A change to how the game behaves** (looks, plays, sounds, its rules, its bugs) is a setting
  in `roadkill.ini`, off by default, so that the default is the PC game. Document it in
  `docs/QOL.md`: what it changes, its setting, and for a fix the original behavior it replaces.
- **The original's own bugs** stay as they are by default (the decomp documents each one in its
  verified function lines); a fix is a setting like any other change.
- **Bugs the port introduces** (something the PC game did not do: a stub, a host service, the
  runtime) are fixed outright, no setting.
- **Game code** in `src/<area>` is decompiled and verified against `TM2.EXE`: a change there goes
  behind its setting in a small, marked branch, and the default path stays the verified code.
  When the decompiled behavior itself is wrong, fix it in the
  [TM2-Win9x Decomp](https://github.com/MrMilenko/TM2-Win9x-Decomp) first, with its check.

## Pull requests

- Open them against `MrMilenko/Roadkill`'s `main`.
- One change per pull request, with how you tested it (which platforms you built and ran, and that
  the default still plays like the PC game).

## License

Roadkill is licensed under the GNU GPL, version 3 or later (`GPL-3.0-or-later`, [LICENSE](LICENSE)).
By opening a pull request, you agree that your contribution is under the same license. Code or
assets from somewhere else must be under a license that works with the GPL, credited, with its
license beside it; say where it comes from in the pull request.

## Using AI tools

You can use AI tools when you contribute. You're responsible for what you submit: understand every
line, test it, and be able to explain it in review. Say in the pull request if a large part of it
was generated.

## Game files

Roadkill never provides game files. Don't commit, attach or link the game's files, disc images or
anything made from them (screenshots of the game are fine in issues and pull requests).
