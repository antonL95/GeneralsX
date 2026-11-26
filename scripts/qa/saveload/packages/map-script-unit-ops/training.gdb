break TerrainLogic::newMap
commands
  silent
  set $point = this->m_waypointListHead
  set $path_start = (Waypoint *)0
  set $path_end = (Waypoint *)0
  while $point
    if $point->m_name.m_data
      set $waypoint_name = (const char *)($point->m_name.m_data + 1)
      if $_streq($waypoint_name, "AOR_Random Waypoint_03")
        set $path_start = $point
      end
      if $_streq($waypoint_name, "AOR_Random Waypoint_01")
        set $path_end = $point
      end
    end
    set $point = $point->m_pNext
  end
  if $path_start && $path_end && $path_start->m_numLinks == 0
    call $path_start->m_pathLabel1.set((const char *)"MapScriptAttackPath")
    call $path_end->m_pathLabel1.set((const char *)"MapScriptAttackPath")
    set $path_start->m_links[0] = $path_end
    set $path_start->m_numLinks = 1
  end
  continue
end
