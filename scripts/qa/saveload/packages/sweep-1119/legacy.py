import os
import struct
from pathlib import Path

import gdb


def convert_terrain(save, version):
    data = save.read_bytes()
    offset = 0
    chunks = {}
    while offset < len(data):
        name_end = offset + 1 + data[offset]
        name = data[offset + 1:name_end]
        if name == b"SG_EOF":
            break
        size, = struct.unpack_from("<I", data, name_end)
        start = name_end + 4
        end = start + size
        if end > len(data):
            raise gdb.GdbError("invalid save chunk size")
        chunks[name] = (name_end, start, end)
        offset = end

    if b"CHUNK_TerrainVisual" not in chunks or b"CHUNK_TerrainLogic" not in chunks:
        raise gdb.GdbError("expected both terrain snapshots")
    name_end, start, end = chunks[b"CHUNK_TerrainLogic"]
    terrain = data[start:end]
    header = struct.Struct("<BBII")
    wrapper_version, terrain_version, _, water_count = header.unpack_from(terrain)
    if wrapper_version != 1 or terrain_version < 4:
        raise gdb.GdbError("expected W3DTerrainLogic v1 with a complete tree ledger")
    if version == 1 and water_count:
        raise gdb.GdbError("unexpected dynamic water state in the v1 fixture")
    water_end = header.size + water_count * struct.calcsize("<I4f")
    keep = struct.calcsize("<BBI") if version == 1 else water_end
    legacy = terrain[:1] + bytes((version,)) + terrain[2:keep]
    if version == 3:
        tree_record = struct.Struct("<3fi")
        tree_count, = struct.unpack_from("<I", terrain, water_end)
        tree_start = water_end + struct.calcsize("<I")
        tree_end = tree_start + tree_count * tree_record.size
        trees = tree_record.iter_unpack(terrain[tree_start:tree_end])
        harvestable = [tree_record.pack(*tree) for tree in trees if tree[3] > 0]
        legacy += struct.pack("<I", len(harvestable)) + b"".join(harvestable)
    save.write_bytes(data[:name_end] + struct.pack("<I", len(legacy)) + legacy + data[end:])
    print(f"TEST sweep 1119 wrote TerrainLogic v{version}")


args = gdb.parameter("args")
if not args.startswith("-headless "):
    raise gdb.GdbError("expected the save and resume runner arguments")
gdb.execute("set args -win -xres 1280 -yres 720 " + args.removeprefix("-headless "))
state_dir = Path(os.environ["XDG_CONFIG_HOME"]).parent
gdb.execute(f"set environment HOME {state_dir}")
gdb.execute(f"set environment DXVK_STATE_CACHE_PATH {state_dir}")

version = int(os.environ["SWEEP1119_TERRAIN_VERSION"])
if version and Path(os.environ["GENERALSX_TEST_COMMANDS"]).stem == "resume":
    convert_terrain(Path(os.environ["XDG_DATA_HOME"]) / "GeneralsX/GeneralsZH/Save/parity.sav", version)
