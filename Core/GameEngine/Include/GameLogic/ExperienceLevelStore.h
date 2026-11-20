#pragma once

class INI;
class Object;

namespace ExperienceLevelStore
{
void load();
void parseDefinition(INI* ini);
void onObjectCreated(const Object* obj);
void update();
void reset();
}
