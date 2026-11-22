import gdb
import struct


expected = {(760.0, 485.0), (905.0, 485.0)}
observed = set()
candidate = None
uploaded = None
render_states = {}
armed = False
saved_blocks = set()
drawn_state = None


def register(name):
    return int(gdb.parse_and_eval(f"${name}"))


def queued_prop():
    address = int(gdb.parse_and_eval("&'DX8Wrapper::render_state'"))
    data = gdb.selected_inferior().read_memory(address, 576)
    for offset in range(0, len(data) - 64, 4):
        matrix = struct.unpack_from("<16f", data, offset)
        xy = matrix[12:14]
        if xy in expected and matrix[3:12:4] == (0.0, 0.0, 0.0) and matrix[15] == 1.0:
            return xy, matrix
    return None, None


class Camera(gdb.Breakpoint):
    def stop(self):
        global armed
        data = gdb.selected_inferior().read_memory(register("x1"), 8)
        if struct.unpack("<2f", data) == (828.0, 545.0):
            armed = True
            self.enabled = False
        return False


def seed_previous_matrix():
    x, y = next(iter(expected - {candidate}))
    matrix = queued_matrix[:12] + (x, y) + queued_matrix[14:]
    address = int(gdb.parse_and_eval("$fx_stale_world"))
    gdb.selected_inferior().write_memory(address, struct.pack("<16f", *matrix))


class MeshEntry(gdb.Breakpoint):
    def stop(self):
        global candidate, queued_matrix
        candidate, queued_matrix = queued_prop() if armed and observed != expected else (None, None)
        return candidate is not None and candidate not in observed


class NativeEntry(gdb.Breakpoint):
    def stop(self):
        global uploaded, drawn_state
        if candidate is None or candidate in observed:
            return False
        uploaded = None
        drawn_state = None
        render_states.clear()
        constants.enabled = True
        states.enabled = True
        capture_state.enabled = True
        draw.enabled = True
        gdb.set_convenience_variable("fx_mesh_device", gdb.parse_and_eval("$x1"))
        NativeExit(gdb.newest_frame(), internal=True)
        return True


class NativeExit(gdb.FinishBreakpoint):
    def stop(self):
        if drawn_state is not None and uploaded is not None:
            observed.add(candidate)
            restored = tuple(render_states.get(state) for state in (14, 22))
            matches = all(abs(value - target) < 0.01 for value, target in zip(uploaded, candidate))
            result = "PASS" if matches and drawn_state == (1, 2) and restored == (0, 1) and len(saved_blocks) == 1 else "FAIL"
            print(f"FX rigid mesh {result} queued={candidate} uploaded={uploaded} submitted={drawn_state} restored={restored} blocks={len(saved_blocks)}")
        for observer in (constants, states, capture_state, draw):
            observer.enabled = False
        return False


class Constants(gdb.Breakpoint):
    def stop(self):
        global uploaded
        if register("x1") == 0 and register("x3") == 256:
            base = register("x2") + 19 * 16
            data = gdb.selected_inferior().read_memory(base, 32)
            uploaded = struct.unpack_from("<f", data, 12)[0], struct.unpack_from("<f", data, 28)[0]
        return False


class States(gdb.Breakpoint):
    def stop(self):
        state = register("x1")
        if state in (14, 22):
            render_states[state] = register("x2")
        return False


class CaptureState(gdb.Breakpoint):
    def stop(self):
        saved_blocks.add(register("x0"))
        return False


class Draw(gdb.Breakpoint):
    def stop(self):
        global drawn_state
        drawn_state = tuple(render_states.get(state) for state in (14, 22))
        self.enabled = False
        return False


gdb.execute("set breakpoint pending on")
camera = Camera("*'W3DView::lookAt(Coord3D const*)'")
mesh = MeshEntry("RetailFX::(anonymous namespace)::drawRetailMesh")
native = NativeEntry("RetailFX::NativePass::drawNormalMapped")
constants = Constants("dxvk::D3D9DeviceEx::SetVertexShaderConstantF")
states = States("dxvk::D3D9DeviceEx::SetRenderState")
capture_state = CaptureState("dxvk::D3D9StateBlock::Capture")
draw = Draw("dxvk::D3D9DeviceEx::DrawIndexedPrimitiveUP")
for observer in (constants, states, capture_state, draw):
    observer.enabled = False
setter = "((int (*)(void *, unsigned int, unsigned int)) &'dxvk::D3D9DeviceEx::SetRenderState')"
native.commands = f"silent\ncall {setter}($fx_mesh_device, 14, 0)\ncall {setter}($fx_mesh_device, 22, 1)\ncontinue"
transform = "((int (*)(void *, unsigned int, void *)) &'dxvk::D3D9DeviceEx::SetTransform')"
device = "((void *(*)()) &'DX8Wrapper::Get_Effect_Device9')()"
mesh.commands = (
    "silent\nset $fx_stale_world = (void *) malloc(64)\npython seed_previous_matrix()\n"
    f"set $fx_seed_device = {device}\ncall {transform}($fx_seed_device, 256, $fx_stale_world)\n"
    "call (void) free($fx_stale_world)\ncontinue"
)
