import os
import struct
from pathlib import Path

import gdb


def make_legacy_save(save):
    data = save.read_bytes()
    chunks = {}
    offset = 0
    while offset < len(data):
        name_end = offset + 1 + data[offset]
        name = data[offset + 1:name_end]
        if name == b"SG_EOF":
            break
        size, = struct.unpack_from("<I", data, name_end)
        start = name_end + 4
        end = start + size
        if end > len(data):
            raise gdb.GdbError("tree-cover: invalid save chunk size")
        chunks[name] = (name_end, start, end)
        offset = end

    if b"CHUNK_TerrainVisual" not in chunks or b"CHUNK_TerrainLogic" not in chunks:
        raise gdb.GdbError("tree-cover: expected both terrain snapshots")
    name_end, start, end = chunks[b"CHUNK_TerrainLogic"]
    terrain = data[start:end]
    wrapper_version, version, _, water_count = struct.unpack_from("<BBII", terrain)
    if (wrapper_version, version) != (1, 4):
        raise gdb.GdbError("tree-cover: expected W3DTerrainLogic v1 and TerrainLogic v4")
    count_offset = struct.calcsize("<BBII") + water_count * struct.calcsize("<I4f")
    tree_count, = struct.unpack_from("<I", terrain, count_offset)
    tree_data = terrain[count_offset + 4:]
    tree_record = struct.Struct("<3fi")
    trees = list(tree_record.iter_unpack(tree_data))
    if len(trees) != tree_count:
        raise gdb.GdbError("tree-cover: invalid terrain ledger length")

    def has_zero_yield_tree_near(x, y):
        return any((tx - x) ** 2 + (ty - y) ** 2 < 2 ** 2 and boxes == 0 for tx, ty, _, boxes in trees)

    if has_zero_yield_tree_near(1243, 2474) or not has_zero_yield_tree_near(131, 2295):
        raise gdb.GdbError("tree-cover: expected the cleared tree to be gone and the control to have zero yield")
    harvestable = [tree_record.pack(*tree) for tree in trees if tree[3] > 0]
    legacy = terrain[:1] + b"\x03" + terrain[2:count_offset] + struct.pack("<I", len(harvestable)) + b"".join(harvestable)
    save.write_bytes(data[:name_end] + struct.pack("<I", len(legacy)) + legacy + data[end:])
    print("TEST tree-cover wrote TerrainLogic v3 with only harvestable trees")


args = gdb.parameter("args")
if not args.startswith("-headless "):
    raise gdb.GdbError("tree-cover: expected the save and resume runner arguments")
gdb.execute("set args -win -xres 1280 -yres 720 " + args.removeprefix("-headless "))
gdb.execute("set environment HOME " + str(Path(os.environ["XDG_CONFIG_HOME"]).parent))

if Path(os.environ["GENERALSX_TEST_COMMANDS"]).stem == "resume":
    make_legacy_save(Path(os.environ["XDG_DATA_HOME"]) / "GeneralsX/GeneralsZH/Save/parity.sav")
