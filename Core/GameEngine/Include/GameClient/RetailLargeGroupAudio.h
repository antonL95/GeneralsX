#pragma once

class AsciiString;
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
void parseUnusedKeys(INI* ini);
}
