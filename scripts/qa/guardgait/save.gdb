tbreak GameLogic::update if this->m_frame == 236
commands
  silent
  python
guard_hordes = {"GondorFountainGuardHorde", "GondorTowerShieldGuardHorde"}
guard_ai = gdb.lookup_type("HordeAIUpdate").pointer()
obj = gdb.parse_and_eval("TheGameLogic->m_objList")
reform_deadlines = {}
while int(obj):
    name = obj["m_template"]["m_overridable"]["m_nameString"]["m_data"]
    name = (name + 1).cast(gdb.lookup_type("char").pointer()).string() if int(name) else ""
    if name in guard_hordes:
        reform_deadlines[name] = int(obj["m_ai"].cast(guard_ai)["m_reformUntil"])
    obj = obj["m_next"]
assert reform_deadlines.keys() == guard_hordes and min(reform_deadlines.values()) > 236, reform_deadlines
print(f"GAIT save frame=236 reform deadlines {reform_deadlines}")
  end
  continue
end
