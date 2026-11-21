#pragma once

class Object;
class Xfer;

namespace RetailCreationUpgrades
{
void load(Xfer* crc);
void onObjectCreated(Object* object);
}
