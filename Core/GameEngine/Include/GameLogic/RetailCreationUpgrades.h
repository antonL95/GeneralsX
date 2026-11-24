#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

class Object;
class Xfer;

namespace RetailCreationUpgrades
{
using GrantLists = std::map<std::string, std::vector<std::string>, std::less<>>;

GrantLists parse(std::string_view events, std::string_view script);
void load(Xfer* crc);
void onObjectCreated(Object* object);
}
