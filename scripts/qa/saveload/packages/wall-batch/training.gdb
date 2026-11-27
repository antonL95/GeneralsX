set $wall_batch_measuring = 0
set $wall_batch_rebuilds = 0
break deleteWalls
commands
silent
set $wall_batch_measuring = 1
set $wall_batch_rebuilds = 0
continue
end
break 'PathfindLayer::rebuildWallCells'
commands
silent
if $wall_batch_measuring
    set $wall_batch_rebuilds = $wall_batch_rebuilds + 1
end
continue
end
break sampleWalls
commands
silent
if $wall_batch_measuring
    printf "TEST wall-batch-rebuilds %d\n", $wall_batch_rebuilds
    set $wall_batch_measuring = 0
end
continue
end
