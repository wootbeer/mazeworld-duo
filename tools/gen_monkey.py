#!/usr/bin/env python3
"""Seeded random-play input scripts for coverage ("monkey testing").
Usage: gen_monkey.py <seed> <ticks> [catacombs|original] > tests/monkey_<seed>.txt

Catacombs: starts a game with space, then holds random combinations of movement/turn keys for
random spans while firing with shift now and then; after a death or level end it keeps going, and
presses space periodically so a finished game restarts, and Return every 400 ticks so a high-score
name dialog (filled with whatever keypad digits were typed into it) gets confirmed.
Original (mouse-driven): a click enters Game mode, then random mouse movement (forward, back,
turning), energy fired by holding the button and moving towards one of the four corners, clicks
that fire again or (released without moving) take the next text, and now and then Desktop mode and
back (space, then a click in the window)."""
import random, sys

seed, ticks = int(sys.argv[1]), int(sys.argv[2])
game = sys.argv[3] if len(sys.argv) > 3 else 'catacombs'
rnd = random.Random(seed)
out = [f"# monkey script seed={seed} ticks={ticks} game={game}"]
t = 60
if game == 'original':
    out += ["60 click 320 240"]; t = 120
    corners = [(-24, -24), (24, -24), (-24, 24), (24, 24)]
    while t < ticks:
        r = rnd.random()
        if r < 0.6:                                   # wander: a burst of small movements
            dx, dy = rnd.randint(-10, 10), rnd.randint(-12, 6)
            for k in range(rnd.randint(3, 20)):
                out.append(f"{t} mmove {dx + rnd.randint(-2, 2)} {dy + rnd.randint(-2, 2)}"); t += rnd.randint(1, 4)
        elif r < 0.8:                                 # select an energy and fire it
            cx, cy = rnd.choice(corners)
            out += [f"{t} mousedown", f"{t + 3} mmove {cx} {cy}", f"{t + 8} mouseup"]; t += 12
        elif r < 0.97:                                # click: fire again, or take the next text
            out += [f"{t} mousedown", f"{t + 3} mouseup"]; t += 6
        else:                                         # Desktop mode and back
            out += [f"{t} tap space", f"{t + 30} click 320 240"]; t += 40
        t += rnd.randint(0, 20)
    print('\n'.join(out)); sys.exit(0)
else:
    out += ["60 tap space"]; t = 120
moves = ['kp8', 'kp2', 'kp4', 'kp6']
turns = ['kp7', 'kp9']
while t < ticks:
    span = rnd.randint(8, 90)
    held = []
    if rnd.random() < 0.8: held.append(rnd.choice(moves))
    if rnd.random() < 0.5: held.append(rnd.choice(turns))
    if rnd.random() < 0.15 and held: held.append(rnd.choice([m for m in moves if m not in held]))
    for k in held: out.append(f"{t} down {k}")
    if rnd.random() < 0.6:
        f = t + rnd.randint(0, max(1, span - 6))
        out += [f"{f} down shift", f"{f + rnd.randint(2, 10)} up shift"]
    if game == 'original' and rnd.random() < 0.2:
        out.append(f"{t + span // 2} click {rnd.randint(40, 600)} {rnd.randint(20, 400)}")
    t += span
    for k in held: out.append(f"{t} up {k}")
    if rnd.random() < 0.05: out.append(f"{t + 1} tap space" if game == 'catacombs' else f"{t + 1} tap return")
    t += rnd.randint(0, 6)
if game == 'catacombs':
    out += [f"{r} tap return" for r in range(400, ticks, 400)]
print('\n'.join(out))
