import os
import gdb

shots = set(map(int, os.environ["GUARDGAIT_SHOTS"].split(",")))
names = {"GondorTowerShieldGuard", "GondorFountainGuard"}
distance_unit = "ArnorCavalry"
objects = None
last_draw = -1

def frame_number():
    logic = gdb.parse_and_eval("TheGameLogic")
    return int(logic["m_frame"]) if int(logic) else 0

def name(value):
    data = value["m_data"]
    return (data + 1).cast(gdb.lookup_type("char").pointer()).string() if int(data) else "-"

def find_guards():
    obj = gdb.parse_and_eval("TheGameLogic->m_objList")
    while int(obj):
        template = name(obj["m_template"]["m_overridable"]["m_nameString"])
        if template == distance_unit or template.removesuffix("Horde") in names:
            yield template, obj
        obj = obj["m_next"]

def guard_draw(drawable):
    module = drawable["m_modules"][0]
    while int(module.dereference()):
        draw = module.dereference()
        if any(kind in str(draw.dynamic_type) for kind in ("W3DHordeModelDraw", "W3DScriptedModelDraw")):
            model = draw.cast(draw_type)
            if int(model["m_renderObject"]):
                return model
        module += 1
    return None

def reform_deadline(obj):
    if not has_reform_deadline:
        return 0
    return int(obj["m_ai"].cast(horde_ai_type.pointer())["m_reformUntil"])

class Draw(gdb.Breakpoint):
    armed = False

    def stop(self):
        global last_draw
        frame = frame_number()
        if frame >= 20 and not self.armed:
            gdb.execute("set variable TheFramePacer->m_enableLogicTimeScale = 0", to_string=True)
            gdb.execute("set variable TheFramePacer->m_enableFpsLimit = 0", to_string=True)
            self.armed = True
        if frame >= 28 and (frame < 44 or frame == last_draw):
            gdb.execute("return", to_string=True)
        last_draw = frame
        return False

class Capture(gdb.Breakpoint):
    def stop(self):
        global objects
        frame = frame_number()
        if 44 <= frame <= 322:
            if objects is None:
                objects = list(find_guards())
                assert len(objects) == 33, len(objects)
            for template, obj in objects:
                pos = obj["m_cachedPos"]
                direction = float(obj["m_cachedAngle"])
                clip_name = "-"
                phase = rate = 0.0
                distance = speed = 0.0
                deadline = 0
                flags = "000"
                drawable = obj["m_drawable"]
                if not template.endswith("Horde") and int(drawable):
                    velocity = obj["m_physics"]["m_vel"]
                    speed = (float(velocity["x"]) ** 2 + float(velocity["y"]) ** 2) ** 0.5
                    bits = drawable["m_conditionState"]["m_bits"]["_M_w"]
                    flags = "".join(str((int(bits[bit // 64]) >> (bit % 64)) & 1) for bit in flag_indices)
                    draw = guard_draw(drawable)
                    if draw is not None:
                        state = draw["m_curState"]
                        index = int(draw["m_whichAnimInCurState"])
                        if int(state) and index >= 0:
                            anim = draw["m_renderObject"].cast(anim_type)
                            animation = state["m_animations"]["_M_impl"]["_M_start"][index]
                            clip_name = name(animation["m_name"])
                            distance = float(animation["m_distanceCovered"])
                            if int(anim["CurMotionMode"]) == 2:
                                phase = float(anim["ModeAnim"]["Frame"])
                                rate = float(anim["ModeAnim"]["frameRateMultiplier"])
                elif template.endswith("Horde"):
                    deadline = reform_deadline(obj)
                print(f"GAIT {frame} {template} {int(obj['m_id'])} {float(pos['x']):.4f} {float(pos['y']):.4f} {direction:.5f} {flags} {clip_name} {phase:.3f} {rate:.3f} {deadline} {distance:.2f} {speed:.4f}")
        if frame in shots:
            prefix = f"GUARDGAIT_PNG_{frame}="
            entry = gdb.parse_and_eval("(char **) __environ")
            while int(entry.dereference()):
                if entry.dereference().string(errors="replace").startswith(prefix):
                    gdb.set_convenience_variable("guardgait_png", entry.dereference() + len(prefix))
                    gdb.execute("return (const char *) $guardgait_png", to_string=True)
                    shots.remove(frame)
                    return False
                entry += 1
            raise gdb.GdbError(f"Missing capture path at frame {frame}")
        return False

flag_indices = [int(gdb.parse_and_eval("MODELCONDITION_" + flag)) for flag in
                ("MOVING", "ALTERNATE_FORMATION", "WEAPONSET_TOGGLE_1")]
draw_type = gdb.lookup_type("W3DModelDraw").pointer()
anim_type = gdb.lookup_type("Animatable3DObjClass").pointer()
horde_ai_type = gdb.lookup_type("HordeAIUpdate")
has_reform_deadline = any(field.name == "m_reformUntil" for field in horde_ai_type.fields())
Draw("W3DDisplay::draw")
Capture("UiTest::capturePath")
