#pragma once

class INI;

namespace RetailLargeGroupAudio
{
void load();
void reset();
void update();
void parseMap(INI* ini);
void parseUnusedKeys(INI* ini);
}
