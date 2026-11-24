python
import os
if "SWEEP1119_RENDERED_GDB" in os.environ:
    gdb.execute("source " + os.environ["SWEEP1119_RENDERED_GDB"])
end
