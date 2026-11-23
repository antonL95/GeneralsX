python
import os
if "TREE_COVER_LEGACY_GDB" in os.environ:
    gdb.execute("source " + os.environ["TREE_COVER_LEGACY_GDB"])
end
