import re

render_env = {
    "GENERALSX_TEST_DRAWTRACE": "GondorFarmMultiplayer,MordorOrcPit,MenWallHubSmallExpansion,GondorFarm,MordorTrollCage",
    "GENERALSX_TRACE_RETAIL_EFFECTS": "1",
    "GENERALSX_TRACE_RETAIL_PARTICLE_SPAWNS": "1",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
    "GENERALSX_TEST_BUILDING_DEATH": "1",
}


def check(log):
    buildings = (
        ("GondorFarmMultiplayer", "gbngran_skn", "gbngran_d3", 166, (156, 162), True),
        ("MordorOrcPit", "mbnbarr_skn", "mbnbarr_d2", 132, (120, 126), False),
        ("MenWallHubSmallExpansion", "gbnwhub_skn", "gbnwhub_d3", 180, (), True),
    )
    draw_pattern = re.compile(
        r"^TEST draw frame (\d+) id (\d+) (\w+) module ModuleTag_Draw model (\S+) clip \S+"
        r" at ([\d.]+)/(\d+) .* flags (.*?) subobjects (\S+)$",
        re.MULTILINE,
    )
    phase_pattern = re.compile(
        r"^TEST collapse-phase frame=(\d+) id=\d+ actor=(\w+) phase=(\w+) fx=(\d+) ocl=(\d+) "
        r"world=[-\d.]+,[-\d.]+,([-\d.]+)$",
        re.MULTILINE,
    )
    draws = list(draw_pattern.findall(log))
    phases = list(phase_pattern.findall(log))
    for actor, live_model, rubble_model, last, late_frames, plays_clip in buildings:
        samples = {int(frame): (actor_id, model, float(pose), int(length), flags, meshes)
                   for frame, actor_id, name, model, pose, length, flags, meshes in draws if name == actor}
        if samples.get(59, (None, None))[1] != live_model or max(samples, default=0) != last:
            raise RuntimeError(f"{actor} did not draw alive at 59 and finish at {last}")
        drawable = samples[59][0]
        for frame in range(61, last + 1):
            sample = samples.get(frame)
            if (not sample or sample[:2] != (drawable, rubble_model)
                    or "RUBBLE" not in sample[4].split(",") or ":shown" not in sample[5]):
                raise RuntimeError(f"{actor} lost its rubble model at frame {frame}: {sample}")
        if plays_clip:
            initial_pose, length = samples[61][2:4]
            if length < 2 or initial_pose != 0 or not any(samples[f][2] > 0 for f in range(62, last + 1)):
                raise RuntimeError(f"{actor} did not play its rubble animation after frame 61")

        actor_phases = [(int(frame), phase, int(fx), int(ocl), float(z))
                        for frame, name, phase, fx, ocl, z in phases if name == actor]
        late = [frame for frame, phase, fx, ocl, z in actor_phases
                if phase == "ALMOST_FINAL" and fx == 1 and ocl == 0 and z < 10]
        if tuple(late) != late_frames or any(ocl for _, _, _, ocl, _ in actor_phases):
            raise RuntimeError(f"{actor} has unexpected late FX or rubble OCLs: {actor_phases}")
        if late_frames and (last, "FINAL") not in [(frame, phase) for frame, phase, *_ in actor_phases]:
            raise RuntimeError(f"{actor} did not finish its collapse at frame {last}")
        if re.search(rf"^TEST list 183 Player_1 {actor} ", log, re.MULTILINE):
            raise RuntimeError(f"{actor} remained after death")

    if not re.search(r"^TEST slow-death frame=180 .* actor=MenWallHubSmallExpansion .* "
                     r"fadetime=0 destroy=180 scale=1\.000 hidden=0 shroud=0 opacity=1\.000$", log, re.MULTILINE):
        raise RuntimeError("The wall hub should remain visible until its authored slow-death cleanup")

    siege_cases = (
        ("GondorFarm", "GondorTrebuchetRockWarheadFlaming", "gbngran_d3", "USER_10"),
        ("MordorTrollCage", "MordorCatapultRockWarhead", "mbtrollpit_d3", None),
    )
    for actor, weapon, rubble_model, level_flag in siege_cases:
        lethal = re.search(rf"^TEST damage frame=(\d+) weapon={weapon} .* victim=\d+\({actor}\) "
                           r".* health=[\d.]+->0\.00$", log, re.MULTILINE)
        if not lethal:
            raise RuntimeError(f"{actor} did not die to its siege projectile")
        kill = int(lethal[1])
        rubble = [(int(frame), model, float(pose), flags.split(","), meshes)
                  for frame, _, name, model, pose, _, flags, meshes in draws
                  if name == actor and int(frame) > kill]
        if (not rubble or not any(pose > 0 for _, _, pose, _, _ in rubble)
                or any(model != rubble_model or "RUBBLE" not in flags or ":shown" not in meshes
                       or (level_flag and level_flag not in flags)
                       for _, model, _, flags, meshes in rubble)):
            raise RuntimeError(f"{actor} failed to draw its animated rubble after impact at {kill}")
        if not any(name == actor and phase == "FINAL" and int(frame) > kill
                   for frame, name, phase, _, _, _ in phases):
            raise RuntimeError(f"{actor} did not finish its collapse")
        if re.search(rf"^TEST list 640 Player_1 {actor} ", log, re.MULTILINE):
            raise RuntimeError(f"{actor} remained after its collapse")
