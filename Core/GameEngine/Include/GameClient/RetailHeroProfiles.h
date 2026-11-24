#pragma once

#include "GameLogic/CreateAHeroRules.h"

struct RetailHeroProfile
{
	CreateAHeroRules::SelectionData selection;
	std::string file;
	bool preset = false;
};

std::vector<RetailHeroProfile> loadRetailHeroProfiles();
std::string saveRetailHeroProfile(const CreateAHeroRules::SelectionData &selection, const std::string &file);
bool deleteRetailHeroProfile(const RetailHeroProfile &profile);
