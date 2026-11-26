break *'FXList::doFXObj(Object const*, Object const*, bool) const' if TheGameLogic && ((TheGameLogic->m_frame >= 888 && TheGameLogic->m_frame <= 892) || TheGameLogic->m_frame == 895)
commands
  silent
  if primary != 0
    printf "TEST pkg20 fx-object frame=%u address=%p id=%u\n", TheGameLogic->m_frame, this, primary->m_id
  end
  continue
end

break *'FXList::doFXPos(Coord3D const*, Matrix3D const*, float, Coord3D const*, float, Object const*, Object const*, bool) const' if TheGameLogic && TheGameLogic->m_frame == 890
commands
  silent
  if primary != 0
    printf "TEST pkg20 fx-position frame=%u address=%p x=%.1f y=%.1f\n", TheGameLogic->m_frame, this, primary->x, primary->y
  end
  continue
end
