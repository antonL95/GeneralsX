#pragma once

#include "Common/AsciiString.h"

class INI;
class Object;
class SpecialPowerTemplate;
class AsciiString;
class Xfer;

namespace ExperienceLevelStore
{
struct SelectionDecal
{
	AsciiString texture;
	AsciiString texture2;
	AsciiString style;
	Real opacityMin = 0.0f;
	Real opacityMax = 0.0f;
	Real minRadius = 0.0f;
	Real maxRadius = 0.0f;
	Int maxSelectedUnits = 0;
};

void load();
void parseDefinition(INI* ini);
void parseScalarTable(INI* ini);
int nextThreshold(const Object* obj, int current, int rankCap = 0);
int rank(const Object* obj, int threshold);
int maximumRank(const Object* obj);
int rankForUpgrade(const Object* obj, const AsciiString& upgrade);
int rankForPower(const Object* obj, const SpecialPowerTemplate* power);
int award(const Object* obj, int threshold);
float playerSkillPointsScalar(int rank);
void applyLevel(Object* obj, int threshold, bool feedback);
Object* recipientFor(Object* obj);
void scoreKill(Object* killer, const Object* victim);
void awardProductionExperience(Object* producer, Int cost);
void onObjectCreated(const Object* obj);
void applyPendingLevels(Object* obj);
void update();
void reset();
void xferPendingLevels(Xfer* xfer);
const SelectionDecal* findSelectionDecal(const Object& object);
}
