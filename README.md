# Game Save Corruption & Recovery Engine

A command-line engine (written in C) that backs up game save files, detects
corruption, and restores known-good versions. Built for the Advanced Data
Structures course — each core component is powered by a real data structure.

## Overview

The engine treats a save slot as something to snapshot, verify, and roll back:

- **Backup** — snapshot the current save with a timestamp and content hashes.
- **Check** — verify a save's integrity and pinpoint which part is corrupted.
- **List** — view the version history for a slot.
- **Restore** — roll back to a chosen snapshot.
- **Repair** — automatically restore the last good snapshot when a check fails.

Each member owns one module and its data structure:

| Module         | Data structure | Responsibility                                  |
|----------------|----------------|-------------------------------------------------|
| `integrity.c`  | Merkle tree    | Corruption detection + fault localization       |
| `versions.c`   | Version tree   | Snapshot history, branching, restore            |
| `store.c`      | Hash table     | Content-addressed chunk storage + dedup         |
| `inventory.c`  | Trie           | Item catalog, inventory, prefix search, compare |

## Setup

Requirements: a C compiler (gcc/clang) and make.

```
git clone <repo-url>
cd game-save-recovery
make
```

This produces the `gsr` binary in the project root.

## Usage

```
./gsr backup <slot>            # snapshot the current save
./gsr check <slot>             # verify integrity
./gsr list <slot>              # show version history
./gsr restore <slot> <version> # roll back to a snapshot
./gsr repair <slot>            # restore last good snapshot
```

## Cleanup

```
make clean
```
