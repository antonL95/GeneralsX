#pragma once

class INI;

namespace RetailLargeGroupAudio
{
void load();
void reset();
void update();
void traceGroup(const char* name);
void parseMap(INI* ini);
void parseMultiSound(INI* ini);
void parseUnusedKeys(INI* ini);
}
