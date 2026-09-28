#!/usr/bin/env python3
"""Resolve GNU ld's deferred ARM absolute addends before 3dsxtool scans them."""

import struct
import sys


def main(path: str) -> None:
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")

    eh = struct.unpack_from("<16sHHIIIIIHHHHHH", data, 0)
    shoff, shnum = eh[6], eh[12]
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * 40)
                for i in range(shnum)]

    loads = [s for s in sections if s[1] == 1 and (s[2] & 2)]
    base = min(s[3] for s in loads)
    changed = 0

    for rel in sections:
        if rel[1] != 9:
            continue
        target = sections[rel[7]]
        if not (target[2] & 2):
            continue
        symtab = sections[rel[6]]
        for i in range(rel[5] // rel[9]):
            offset, info = struct.unpack_from("<II", data, rel[4] + i * rel[9])
            kind, sym_index = info & 0xff, info >> 8
            if kind not in (2, 38):  # R_ARM_ABS32 / R_ARM_TARGET1
                continue
            sym_off = symtab[4] + sym_index * symtab[9]
            sym_value, _, sym_info, _, _, _ = struct.unpack_from("<IIIBBH", data, sym_off)
            if sym_value == 0 and (sym_info >> 4) == 2:  # unbound weak symbol
                continue
            if not (target[3] <= offset < target[3] + target[5]):
                continue
            value_off = target[4] + offset - target[3]
            value = struct.unpack_from("<I", data, value_off)[0]
            if value < base and sym_value:
                struct.pack_into("<I", data, value_off, (value + sym_value) & 0xffffffff)
                changed += 1

    if changed:
        with open(path, "wb") as f:
            f.write(data)
    print(f"fixed {changed} ARM absolute relocations")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} FILE.elf")
    main(sys.argv[1])
