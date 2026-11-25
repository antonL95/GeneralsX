#pragma once

#include "Common/AsciiString.h"
#include <vector>

class INI;

namespace RetailLargeGroupAudio
{
void load();
void reset();
void update();
void traceGroup(const char* name);
void parseMap(INI* ini);
void parseMultiSound(INI* ini);
AsciiString chooseMultiSoundTrack(const AsciiString& event);
Bool expandMultiSound(const AsciiString& name, std::vector<AsciiString>& events, Bool& loop);
Bool isMultiSound(const AsciiString& name);
void parseUnusedKeys(INI* ini);
}
