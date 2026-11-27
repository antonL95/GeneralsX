#include "PreRTS.h"

#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/CreateAHeroSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/INI.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/LatchRestore.h"
#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailINIStartup.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/ExperienceLevelRules.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#if !RTS_GENERALS
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#endif
#include "GameLogic/Module/ExperienceModules.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/UnpauseSpecialPowerUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"
#include "WWMath/wwmath.h"

namespace
{
using ExperienceLevelStore::SelectionDecal;

struct Level
{
	AsciiString name;
	std::vector<AsciiString> targetNames;
	Int requiredExperience = 0;
	Int experienceAward = 0;
	Int experienceAwardOwnGuysDie = 0;
	Int rank = 0;
	std::vector<AsciiString> upgrades;
	std::vector<AsciiString> attributeModifiers;
	AsciiString levelUpFx;
	AsciiString levelUpOcl;
	ModelConditionFlags conditions;
	SelectionDecal selectionDecal;
	Bool showLevelUpTint = FALSE;
	RGBColor levelUpTintColor = {0.0f, 0.0f, 0.0f};
	Int levelUpTintPreColorTime = 0;
	Int levelUpTintPostColorTime = 0;
	Int levelUpTintSustainColorTime = 0;
	Real levelUpTintFrequency = 0.0f;
	Real levelUpTintAmplitude = 0.0f;
	Bool informUpdateModule = FALSE;
	AsciiString emotionType;
	Bool singlePlayerOnly = FALSE;
	Bool multiPlayerOnly = FALSE;
};

std::vector<Level> s_levels;
std::unordered_map<std::string, std::vector<std::size_t>> s_levelsByTarget;
std::unordered_map<std::string, std::vector<Real>> s_scalars;
struct PendingLevel
{
	ObjectID object;
	Int rank;
	Bool setStartingRank;
};

std::vector<PendingLevel> s_pending;
Bool s_indexed = FALSE;
const Object* s_grantingLevelUpgrades = nullptr;

void parseSelectionDecal(INI* ini, void*, void* store, const void*)
{
	static const FieldParse decalFields[] =
	{
		{ "Texture", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, texture) },
		{ "Texture2", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, texture2) },
		{ "Style", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, style) },
		{ "OpacityMin", INI::parsePercentToReal, nullptr, offsetof(SelectionDecal, opacityMin) },
		{ "OpacityMax", INI::parsePercentToReal, nullptr, offsetof(SelectionDecal, opacityMax) },
		{ "MinRadius", INI::parseReal, nullptr, offsetof(SelectionDecal, minRadius) },
		{ "MaxRadius", INI::parseReal, nullptr, offsetof(SelectionDecal, maxRadius) },
		{ "MaxSelectedUnits", INI::parseInt, nullptr, offsetof(SelectionDecal, maxSelectedUnits) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, decalFields);
}

const FieldParse s_levelFields[] =
{
	{ "TargetNames", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, targetNames) },
	{ "RequiredExperience", INI::parseInt, nullptr, offsetof(Level, requiredExperience) },
	{ "ExperienceAward", INI::parseInt, nullptr, offsetof(Level, experienceAward) },
	{ "ExperienceAwardOwnGuysDie", INI::parseInt, nullptr, offsetof(Level, experienceAwardOwnGuysDie) },
	{ "Rank", INI::parseInt, nullptr, offsetof(Level, rank) },
	{ "Upgrades", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, upgrades) },
	{ "AttributeModifiers", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, attributeModifiers) },
	{ "LevelUpFx", INI::parseAsciiString, nullptr, offsetof(Level, levelUpFx) },
	{ "LevelUpOCL", INI::parseAsciiString, nullptr, offsetof(Level, levelUpOcl) },
	{ "ModelConditionState", ModelConditionFlags::parseFromINI, nullptr, offsetof(Level, conditions) },
	{ "SelectionDecal", parseSelectionDecal, nullptr, offsetof(Level, selectionDecal) },
	{ "ShowLevelUpTint", INI::parseBool, nullptr, offsetof(Level, showLevelUpTint) },
	{ "LevelUpTintColor", INI::parseRGBColor, nullptr, offsetof(Level, levelUpTintColor) },
	{ "LevelUpTintPreColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintPreColorTime) },
	{ "LevelUpTintPostColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintPostColorTime) },
	{ "LevelUpTintSustainColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintSustainColorTime) },
	{ "LevelUpTintFrequency", INI::parseReal, nullptr, offsetof(Level, levelUpTintFrequency) },
	{ "LevelUpTintAmplitude", INI::parseReal, nullptr, offsetof(Level, levelUpTintAmplitude) },
	{ "InformUpdateModule", INI::parseBool, nullptr, offsetof(Level, informUpdateModule) },
	{ "EmotionType", INI::parseAsciiString, nullptr, offsetof(Level, emotionType) },
	{ "SinglePlayerOnly", INI::parseBool, nullptr, offsetof(Level, singlePlayerOnly) },
	{ "MultiPlayerOnly", INI::parseBool, nullptr, offsetof(Level, multiPlayerOnly) },
	{ nullptr, nullptr, nullptr, 0 }
};

Bool redefines(const Level& later, const Level& earlier)
{
	return later.targetNames.empty() || earlier.targetNames.empty() ||
		std::find_first_of(later.targetNames.begin(), later.targetNames.end(),
			earlier.targetNames.begin(), earlier.targetNames.end()) != later.targetNames.end();
}

void buildIndex()
{
	s_levelsByTarget.clear();
	for (std::size_t i = 0; i < s_levels.size(); ++i)
		for (const AsciiString& target : s_levels[i].targetNames)
			s_levelsByTarget[target.str()].push_back(i);
	s_indexed = TRUE;
}

Bool tracing()
{
	static const char* commands = std::getenv("GENERALSX_TEST_COMMANDS");
	return commands != nullptr && *commands != '\0';
}

Bool isMultiplayer()
{
	return TheGameLogic && (TheGameLogic->isInMultiplayerGame() || TheGameLogic->isSimulatingSkirmishGame());
}

Real multiplayerExperienceMultiplier(const Object* obj)
{
	if (!isMultiplayer() || !ThePlayerList || !TheGlobalData)
		return 1.0f;

	Int count = 0;
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		const Player* player = ThePlayerList->getNthPlayer(i);
		if (player && player->isPlayableSide())
			++count;
	}
	if (count <= 0 || count > MAX_PLAYER_COUNT)
		return 1.0f;

	return obj->isKindOf(KINDOF_STRUCTURE)
		? TheGlobalData->m_multiPlayBuildingXPMult[count - 1]
		: TheGlobalData->m_multiPlayUnitXPMult[count - 1];
}

void applyPendingLevel(Object* obj, const PendingLevel& pending)
{
	auto* tracker = obj->getExperienceTracker();
	tracker->setMinRetailRank(pending.rank);
	if (pending.setStartingRank)
		tracker->setRetailStartingRank(tracker->getRank());
}

const std::vector<std::size_t>* levelsFor(const Object* obj)
{
	if (!RetailFileLayers::enabled() || !obj)
		return nullptr;
	if (!s_indexed)
		buildIndex();
	const ThingTemplate* type = obj->getTemplate();
	auto found = s_levelsByTarget.find(type->getName().str());
	if (found == s_levelsByTarget.end() && obj->isKindOf(KINDOF_CREATE_A_HERO) && obj->getHeroIdentityTemplate() != type)
		found = s_levelsByTarget.find(obj->getHeroIdentityTemplate()->getName().str());
	return found == s_levelsByTarget.end() ? nullptr : &found->second;
}

const Level* findLevel(const Object* obj, Int threshold, bool next, Int rankCap = 0)
{
	const auto* indices = levelsFor(obj);
	if (!indices)
		return nullptr;
	const Bool multiplayer = isMultiplayer();
	const Level* best = nullptr;
	for (std::size_t index : *indices)
	{
		const Level& level = s_levels[index];
		if ((multiplayer ? level.singlePlayerOnly : level.multiPlayerOnly) ||
			(!next && level.requiredExperience != threshold) ||
			(next && level.requiredExperience <= threshold))
			continue;
		if (!best || level.requiredExperience < best->requiredExperience)
			best = &level;
	}
	return best && (rankCap <= 0 || best->rank <= rankCap) ? best : nullptr;
}

void parseScalars(INI* ini, void*, void* store, const void*)
{
	auto& scalars = *static_cast<std::vector<Real>*>(store);
	scalars.clear();
	while (const char* token = ini->getNextTokenOrNull())
		scalars.push_back(INI::scanReal(token));
}

void apply(Object* obj, const Level& level, bool feedback)
{
	std::string granted;
	for (const AsciiString& name : level.upgrades)
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (upgrade == nullptr || obj->hasUpgrade(upgrade))
			continue;
		{
			const LatchRestore<const Object*> grant(s_grantingLevelUpgrades, obj);
			obj->giveUpgrade(upgrade);
		}
		if (!granted.empty())
			granted += ',';
		granted += name.str();
	}
#if !RTS_GENERALS
	static const NameKeyType levelSource = NAMEKEY("ExperienceLevel");
	const auto owner = AttributeModifiers::ownerOf(obj, levelSource);
	for (const AsciiString& modifier : level.attributeModifiers)
		AttributeModifiers::apply(obj, modifier, AttributeModifierHelper::USE_LIST_DURATION, 0, owner);
#endif
	if (level.conditions.any())
		obj->setModelConditionFlags(level.conditions);
	if (feedback && TheGameLogic->getFrame() >= 10)
	{
		if (TheFXListStore && level.levelUpFx.isNotEmpty())
		{
			const char* fx = level.levelUpFx.str();
			if (strncmp(fx, "FX:", 3) == 0)
				fx += 3;
			FXList::doFXObj(TheFXListStore->findFXList(fx), obj);
		}
		if (TheObjectCreationListStore && level.levelUpOcl.isNotEmpty())
			ObjectCreationList::create(TheObjectCreationListStore->findObjectCreationList(level.levelUpOcl.str()), obj, nullptr);
#if !RTS_GENERALS
		obj->setTimedModelCondition(MODELCONDITION_LEVELED, 3 * LOGICFRAMES_PER_SECOND);
#endif
	}
	if (tracing())
	{
		std::fprintf(stderr, "TEST level %u object %u %s level %s granted %s\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(), level.name.str(),
			granted.empty() ? "-" : granted.c_str());
		for (const AsciiString& modifier : level.attributeModifiers)
			std::fprintf(stderr, "TEST levelmodifier %u %u %s\n", TheGameLogic->getFrame(),
				static_cast<unsigned>(obj->getID()), modifier.str());
	}
}
}

namespace ExperienceLevelStore
{
void load()
{
	if (!RetailFileLayers::enabled())
		return;
	try
	{
		if (!RetailINIStartup::loadSubsystem("TheExperienceLevelSystem", nullptr, "ExperienceLevel", parseDefinition))
		{
			INI ini;
			ini.load("Data\\INI\\ExperienceLevels.ini", INI_LOAD_OVERWRITE, nullptr, "ExperienceLevel", parseDefinition);
		}
	}
	catch (...)
	{
		std::fprintf(stderr, "Experience levels: Data\\INI\\ExperienceLevels.ini could not be read\n");
	}
	buildIndex();
}

void parseDefinition(INI* ini)
{
	const char* token = ini->getNextToken();
	AsciiString name = token;
	Level level;
	level.name = name;
	ini->initFromINI(&level, s_levelFields);
	std::size_t index = 0;
	while (index < s_levels.size() && !(s_levels[index].name == name && redefines(level, s_levels[index])))
		++index;
	if (index == s_levels.size())
		s_levels.push_back(std::move(level));
	else
		s_levels[index] = std::move(level);
	s_indexed = FALSE;
}

void parseScalarTable(INI* ini)
{
	const std::string name = ini->getNextToken();
	static const FieldParse fields[] =
	{
		{ "Scalars", parseScalars, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	std::vector<Real> scalars;
	ini->initFromINI(&scalars, fields);
	s_scalars[name] = std::move(scalars);
}

int nextThreshold(const Object* obj, int current, int rankCap)
{
	const Level* next = findLevel(obj, current, true, rankCap);
	return next ? next->requiredExperience : 0;
}

int rank(const Object* obj, int threshold)
{
	if (!threshold)
		threshold = nextThreshold(obj, 0);
	const Level* current = findLevel(obj, threshold, false);
	return current ? current->rank : 1;
}

int maximumRank(const Object* obj)
{
	const auto* indices = levelsFor(obj);
	if (!indices)
		return 0;
	int maximum = 0;
	const Bool multiplayer = isMultiplayer();
	for (std::size_t index : *indices)
	{
		const Level& level = s_levels[index];
		if (!(multiplayer ? level.singlePlayerOnly : level.multiPlayerOnly))
			maximum = std::max(maximum, level.rank);
	}
	return maximum;
}

int rankForUpgrade(const Object* obj, const AsciiString& upgrade)
{
	const auto* indices = levelsFor(obj);
	if (!indices)
		return 0;
	int required = 0;
	const Bool multiplayer = isMultiplayer();
	for (std::size_t index : *indices)
	{
		const Level& level = s_levels[index];
		if (!(multiplayer ? level.singlePlayerOnly : level.multiPlayerOnly) &&
			std::find(level.upgrades.begin(), level.upgrades.end(), upgrade) != level.upgrades.end() &&
			(!required || level.rank < required))
			required = level.rank;
	}
	return required;
}

int rankForPower(const Object* obj, const SpecialPowerTemplate* power)
{
	if (!obj || !power)
		return 0;
	const SpecialPowerModuleInterface* module = obj->getSpecialPowerModule(power);
	Int required = module ? module->getRequiredRank() : 0;
	const NameKeyType unpause = NAMEKEY("UnpauseSpecialPowerUpgrade");
	for (BehaviorModule** it = obj->getBehaviorModules(); *it; ++it)
	{
		if ((*it)->getModuleNameKey() != unpause)
			continue;
		const auto* upgrade = static_cast<const UnpauseSpecialPowerUpgrade*>(*it)->getUnpauseData();
		if (upgrade->m_specialPower != power)
			continue;
		const auto& triggers = upgrade->m_upgradeMuxData;
#if RTS_GENERALS
		const auto& names = triggers.m_activationUpgradeNames;
#else
		const auto& names = triggers.m_triggerUpgradeNames.empty()
			? triggers.m_activationUpgradeNames : triggers.m_triggerUpgradeNames;
#endif
		Int fromUpgrades = 0;
		for (const AsciiString& name : names)
		{
			const Int rank = rankForUpgrade(obj, name);
			if (triggers.m_requiresAllTriggers)
				fromUpgrades = std::max(fromUpgrades, rank);
			else if (!rank)
			{
				fromUpgrades = 0;
				break;
			}
			else if (!fromUpgrades || rank < fromUpgrades)
				fromUpgrades = rank;
		}
		required = std::max(required, fromUpgrades);
	}
	return required;
}

int award(const Object* obj, int threshold)
{
	if (!threshold)
		threshold = nextThreshold(obj, 0);
	const Level* current = findLevel(obj, threshold, false);
	return current ? current->experienceAward : 0;
}

float playerSkillPointsScalar(int rank)
{
	const auto found = s_scalars.find("PlayerSkillPointsScalarTable");
	if (found == s_scalars.end() || found->second.empty())
		return 1.0f;
	const auto index = static_cast<std::size_t>(std::max(1, rank) - 1);
	return found->second[std::min(index, found->second.size() - 1)];
}

static Real experienceScalar(const Object* object, Int currentRank, Int startingRank)
{
	if (object->isKindOf(KINDOF_STRUCTURE))
		return 1.0f;
	auto found = s_scalars.find(object->getTemplate()->getRotWKObjectData().experienceScalarTable.str());
	if (found == s_scalars.end())
		found = s_scalars.find("DefaultExperienceScalarTable");
	if (found == s_scalars.end() || found->second.empty())
		return 1.0f;
	const Int difference = std::max(0, currentRank - startingRank);
	return found->second[std::min(static_cast<std::size_t>(difference), found->second.size() - 1)];
}

void applyLevel(Object* obj, int threshold, bool feedback)
{
	if (const Level* level = findLevel(obj, threshold, false))
	{
		apply(obj, *level, feedback);
		if (level->informUpdateModule && level->rank > 1)
			if (auto* horde = const_cast<HordeContain*>(HordeContain::get(obj)))
				horde->onRankAdvanced(level->rank, feedback);
#if !RTS_GENERALS
		if (level->emotionType.isNotEmpty())
			EmotionTrackerUpdate::queuePromotionEmotion(obj, level->emotionType);
#endif
	}
}

static Object* resolveRecipient(Object* obj, Real* explicitSinkPoints)
{
	for (Int links = 0; obj && links < 32; ++links)
	{
		const ExperienceTracker* tracker = obj->getExperienceTracker();
		Object* next = TheGameLogic->findObjectByID(tracker->getExperienceSink());
		if (!next && !explicitSinkPoints)
		{
			if (obj->isKindOf(KINDOF_PASS_EXPERIENCE_TO_PRODUCER))
				next = TheGameLogic->findObjectByID(obj->getProducerID());
			if (!next)
				next = obj->isKindOf(KINDOF_PASS_EXPERIENCE_TO_CONTAINER) ? obj->getContainedBy() : HordeContain::getAnchor(obj);
		}
		if (!next || next->isEffectivelyDead())
			return obj;
		if (explicitSinkPoints)
			*explicitSinkPoints *= tracker->getExperienceScalar();
		obj = next;
	}
	return nullptr;
}

Object* recipientFor(Object* obj)
{
	return resolveRecipient(obj, nullptr);
}

void awardVictoryExperience(Object* obj)
{
	const ExperienceTracker* tracker = obj->getExperienceTracker();
	if (!tracker)
		return;

	const Int next = nextThreshold(obj, tracker->getRetailThreshold());
	const Real current = tracker->getRetailExperience();
	if (current >= 0.0f && current < Real(next))
	{
		Real points = static_cast<Real>(next - static_cast<Int>(current));
		if (Object* recipient = resolveRecipient(obj, &points))
			recipient->getExperienceTracker()->addRetailExperience(points, FALSE);
	}
}

void scoreKill(Object* killer, const Object* victim)
{
	if (killer->getControllingPlayer() == victim->getControllingPlayer() ||
		killer->getRelationship(victim) == ALLIES || victim->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return;
	const Int points = victim->getExperienceTracker()->getExperienceValue(killer);
	if (points <= 0)
		return;
	if (Object* recipient = recipientFor(killer))
	{
		recipient->getExperienceTracker()->addRetailExperience(points);
		std::vector<SharedExperience> shared;
		const auto collect = [&shared](const Object* source)
		{
			for (BehaviorModule** module = source->getBehaviorModules(); *module; ++module)
				if ((*module)->getModuleNameKey() == NAMEKEY("ShareExperienceBehavior"))
					static_cast<const ShareExperienceBehavior*>(*module)->collectRecipients(shared);
		};
		collect(recipient);
		if (killer != recipient && !killer->isKindOf(KINDOF_PROJECTILE))
			collect(killer);
		for (SharedExperience& share : shared)
		{
			Object* ally = recipientFor(TheGameLogic->findObjectByID(share.object));
			share.object = ally && ally != recipient ? ally->getID() : INVALID_ID;
		}
		std::sort(shared.begin(), shared.end(), [](const SharedExperience& a, const SharedExperience& b)
		{
			return a.object == b.object ? a.fraction > b.fraction : a.object < b.object;
		});
		ObjectID previous = INVALID_ID;
		for (const SharedExperience& share : shared)
		{
			if (share.object == previous)
				continue;
			previous = share.object;
			Object* ally = TheGameLogic->findObjectByID(share.object);
			if (ally && !ally->isEffectivelyDead())
				ally->getExperienceTracker()->addRetailExperience(points * share.fraction);
		}
	}
}

void awardProductionExperience(Object* producer, Int cost)
{
	if (cost <= 0)
		return;
	if (Object* recipient = recipientFor(producer))
		recipient->getExperienceTracker()->addRetailExperience(static_cast<Real>(cost));
}

void onObjectCreated(const Object* obj)
{
	if (nextThreshold(obj, 0))
		s_pending.push_back({obj->getID(), 1, FALSE});
}

void applyPendingLevels(Object* obj)
{
	std::vector<PendingLevel> pending;
	for (const PendingLevel& entry : s_pending)
		if (entry.object == obj->getID())
			pending.push_back(entry);
	std::erase_if(s_pending, [obj](const PendingLevel& entry) { return entry.object == obj->getID(); });
	for (const PendingLevel& entry : pending)
		applyPendingLevel(obj, entry);
}

void update()
{
	for (std::size_t i = 0; i < s_pending.size(); ++i)
	{
		const PendingLevel pending = s_pending[i];
		Object* obj = TheGameLogic->findObjectByID(pending.object);
		if (obj == nullptr || obj->isDestroyed())
			continue;
		applyPendingLevel(obj, pending);
	}
	s_pending.clear();
}

void reset()
{
	s_pending.clear();
}

void xferPendingLevels(Xfer* xfer)
{
	UnsignedInt count = static_cast<UnsignedInt>(s_pending.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		s_pending.clear();
		for (UnsignedInt i = 0; i < count; ++i)
		{
			PendingLevel entry{};
			xfer->xferObjectID(&entry.object);
			xfer->xferInt(&entry.rank);
			xfer->xferBool(&entry.setStartingRank);
			s_pending.push_back(entry);
		}
	}
	else
	{
		for (PendingLevel& entry : s_pending)
		{
			xfer->xferObjectID(&entry.object);
			xfer->xferInt(&entry.rank);
			xfer->xferBool(&entry.setStartingRank);
		}
	}
}

const SelectionDecal* findSelectionDecal(const Object& object)
{
	const auto* indices = levelsFor(&object);
	if (!indices)
		return nullptr;

	const ExperienceTracker* tracker = object.getExperienceTracker();
	const Int rank = tracker ? tracker->getRank() : 1;
	const Bool multiplayer = isMultiplayer();
	const Level* best = nullptr;
	for (std::size_t index : *indices)
	{
		const Level& level = s_levels[index];
		if (level.selectionDecal.texture.isEmpty() || level.rank > rank ||
			(multiplayer ? level.singlePlayerOnly : level.multiPlayerOnly))
			continue;
		if (!best || level.rank > best->rank)
			best = &level;
	}
	return best ? &best->selectionDecal : nullptr;
}
}
Int ExperienceTracker::getRank() const
{
	return RetailFileLayers::enabled() ? ExperienceLevelStore::rank(m_parent, m_retailThreshold) : static_cast<Int>(m_currentLevel) + 1;
}

Real ExperienceTracker::getLevelProgress() const
{
	return ExperienceLevelRules::progress(m_retailExperience, m_retailThreshold,
		ExperienceLevelStore::nextThreshold(m_parent, m_retailThreshold));
}

void ExperienceTracker::initializeRetailLevel()
{
	if (m_retailThreshold)
		return;
	const Int first = ExperienceLevelStore::nextThreshold(m_parent, 0);
	if (!first)
		return;
	m_retailThreshold = first;
	m_retailStartingRank = getRank();
	m_retailExperience = static_cast<Real>(first);
	ExperienceLevelStore::applyLevel(m_parent, first, false);
}

void ExperienceTracker::advanceRetailLevel(Real experience, Bool feedback)
{
	initializeRetailLevel();
	if (!m_retailThreshold || !std::isfinite(experience))
		return;
	m_retailExperience = std::max(m_retailExperience, experience);
	while (const Int next = ExperienceLevelStore::nextThreshold(m_parent, m_retailThreshold))
	{
		if (m_retailExperience < static_cast<Real>(next))
			break;
		const Int previousRank = getRank();
		m_retailThreshold = next;
		ExperienceLevelStore::applyLevel(m_parent, next, feedback != FALSE);
		CreateAHeroSystem::grantPowerUpgrades(m_parent, previousRank, getRank());
		if (const HordeContain* horde = HordeContain::get(m_parent))
			for (Object* member : *horde->getContainedItemsList())
				if (!member->isEffectivelyDead())
					member->getExperienceTracker()->setMinRetailRank(getRank());
	}
}

Bool ExperienceTracker::isBelowRetailRankCap() const
{
	return m_retailRankCap < 1 || getRank() < m_retailRankCap;
}

void ExperienceTracker::addRetailExperience(Real points, Bool scale)
{
	if (points <= 0.0f || !isTrainable())
		return;
	initializeRetailLevel();
	if (!isBelowRetailRankCap())
		return;
	if (scale)
	{
		points = m_experienceScalar * points * multiplayerExperienceMultiplier(m_parent);
		points *= ExperienceLevelStore::experienceScalar(m_parent, getRank(), m_retailStartingRank);
	}
	advanceRetailLevel(m_retailExperience + points, TRUE);
}

ExperienceTracker* ExperienceTracker::resolveScriptExperienceSink(Real* scaledPoints)
{
	ExperienceTracker* recipient = this;
	std::vector<ObjectID> visited;
	while (recipient->m_experienceSink != INVALID_ID)
	{
		Object* next = TheGameLogic->findObjectByID(recipient->m_experienceSink);
		if (!next)
			break;
		if (std::find(visited.begin(), visited.end(), next->getID()) != visited.end())
			return nullptr;
		visited.push_back(next->getID());
		if (scaledPoints)
			*scaledPoints *= recipient->m_experienceScalar;
		recipient = next->getExperienceTracker();
	}
	return recipient;
}

void ExperienceTracker::addScriptRetailExperience(Real points)
{
	ExperienceTracker* recipient = resolveScriptExperienceSink(&points);
	if (!recipient)
		return;
	if (points >= 0.0f)
	{
		recipient->addRetailExperience(points, TRUE);
		return;
	}
	if (!recipient->isTrainable() || !std::isfinite(points))
		return;
	recipient->initializeRetailLevel();
	if (!recipient->m_retailThreshold || !recipient->isBelowRetailRankCap())
		return;
	points *= recipient->m_experienceScalar * multiplayerExperienceMultiplier(recipient->m_parent);
	points *= ExperienceLevelStore::experienceScalar(recipient->m_parent, recipient->getRank(), recipient->m_retailStartingRank);
	if (std::isfinite(points + recipient->m_retailExperience))
		recipient->m_retailExperience += points;
}

void ExperienceTracker::setScriptRetailExperience(Int points)
{
	ExperienceTracker* recipient = resolveScriptExperienceSink(nullptr);
	if (!recipient || !recipient->m_isTrainable)
		return;
	recipient->initializeRetailLevel();
	recipient->m_retailExperience = INT_TO_REAL(points);
	recipient->advanceRetailLevel(recipient->m_retailExperience, TRUE);
}

void ExperienceTracker::setMinRetailRank(Int rank, Bool feedback)
{
	initializeRetailLevel();
	if (rank <= getRank())
		return;
	Int threshold = m_retailThreshold;
	while (const Int next = ExperienceLevelStore::nextThreshold(m_parent, threshold, rank))
		threshold = next;
	if (threshold > m_retailThreshold)
		advanceRetailLevel(static_cast<Real>(threshold), feedback);
}

Bool ExperienceTracker::gainRetailLevels(Int count, Int rankCap, Bool trainableOnly)
{
	if ((trainableOnly && !isTrainable()) || count <= 0)
		return FALSE;
	initializeRetailLevel();
	Int threshold = m_retailThreshold;
	for (Int i = 0; i < count; ++i)
	{
		const Int next = ExperienceLevelStore::nextThreshold(m_parent, threshold, rankCap);
		if (!next)
			break;
		threshold = next;
	}
	if (threshold == m_retailThreshold)
		return FALSE;
	advanceRetailLevel(static_cast<Real>(threshold), TRUE);
	return TRUE;
}

void ExperienceLevelCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	CreateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "LevelToGrant", INI::parseInt, nullptr, offsetof(ExperienceLevelCreateModuleData, m_levelToGrant) },
		{ "MPOnly", INI::parseBool, nullptr, offsetof(ExperienceLevelCreateModuleData, m_multiplayerOnly) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ExperienceLevelCreate::~ExperienceLevelCreate() = default;
void ExperienceLevelCreate::crc(Xfer* xfer) { CreateModule::crc(xfer); }
void ExperienceLevelCreate::xfer(Xfer* xfer) { CreateModule::xfer(xfer); }
void ExperienceLevelCreate::loadPostProcess() { CreateModule::loadPostProcess(); }

void ExperienceLevelCreate::onCreate()
{
	if (!RetailFileLayers::enabled() || (TheGameState && TheGameState->isInLoadGame()))
		return;
	const auto* data = getExperienceLevelCreateModuleData();
	if (!data->m_multiplayerOnly || isMultiplayer())
		s_pending.push_back({getObject()->getID(), data->m_levelToGrant, TRUE});
}

void LevelUpUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "LevelsToGain", INI::parseInt, nullptr, offsetof(LevelUpUpgradeModuleData, m_levelsToGain) },
		{ "LevelCap", INI::parseInt, nullptr, offsetof(LevelUpUpgradeModuleData, m_levelCap) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

LevelUpUpgrade::~LevelUpUpgrade() = default;
void LevelUpUpgrade::crc(Xfer* xfer) { UpgradeModule::crc(xfer); }
void LevelUpUpgrade::xfer(Xfer* xfer) { UpgradeModule::xfer(xfer); }
void LevelUpUpgrade::loadPostProcess() { UpgradeModule::loadPostProcess(); }

void LevelUpUpgrade::upgradeImplementation()
{
	if (isAlreadyUpgraded() || !RetailFileLayers::enabled() || (TheGameState && TheGameState->isInLoadGame()))
		return;
	setUpgradeExecuted(TRUE);
	if (s_grantingLevelUpgrades == getObject())
		return;
	const auto* data = getLevelUpUpgradeModuleData();
	getObject()->getExperienceTracker()->gainRetailLevels(data->m_levelsToGain, data->m_levelCap, FALSE);
}

void ShareExperienceBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(ShareExperienceBehaviorModuleData, m_filter) },
		{ "Radius", INI::parseReal, nullptr, offsetof(ShareExperienceBehaviorModuleData, m_radius) },
		{ "DropOff", INI::parseReal, nullptr, offsetof(ShareExperienceBehaviorModuleData, m_dropOff) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

ShareExperienceBehavior::~ShareExperienceBehavior() = default;
void ShareExperienceBehavior::crc(Xfer* xfer) { BehaviorModule::crc(xfer); }
void ShareExperienceBehavior::xfer(Xfer* xfer) { BehaviorModule::xfer(xfer); }
void ShareExperienceBehavior::loadPostProcess() { BehaviorModule::loadPostProcess(); }

void ShareExperienceBehavior::collectRecipients(std::vector<SharedExperience>& recipients) const
{
	const auto* data = getShareExperienceBehaviorModuleData();
	if (data->m_radius <= 0.0f)
		return;
	const Object* origin = getObject();
	PartitionFilterSamePlayer samePlayer(origin->getControllingPlayer());
	PartitionFilter* filters[] = { &samePlayer, nullptr };
	ObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(origin->getPosition(),
		data->m_radius, FROM_CENTER_2D, filters, ITER_FASTEST);
	MemoryPoolObjectHolder hold(iter);
	for (Object* candidate = iter->first(); candidate; candidate = iter->next())
	{
		if (candidate == origin || candidate->isEffectivelyDead() ||
			!data->m_filter.matchesTemplate(*candidate->getTemplate()))
			continue;
		Real fraction = 1.0f;
		if (data->m_dropOff == 1.0f)
		{
			const Coord3D delta = *candidate->getPosition() - *origin->getPosition();
			fraction = std::max(0.0f, 1.0f - WWMath::Sqrt(delta.lengthSqr()) / data->m_radius);
		}
		if (fraction > 0.0f)
			recipients.push_back({candidate->getID(), fraction});
	}
}

void LevelGrantSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Experience", INI::parseInt, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_experience) },
		{ "RadiusEffect", INI::parseReal, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_radius) },
		{ "AcceptanceFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_acceptanceFilter) },
		{ "LevelFX", INI::parseAsciiStringVectorAppend, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_effects) },
		{ "UseKindOf", INI::parseBool, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_useKindOf) },
		{ "AffectsKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(LevelGrantSpecialPowerModuleData, m_affectsKindOf) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

LevelGrantSpecialPower::~LevelGrantSpecialPower() = default;
void LevelGrantSpecialPower::crc(Xfer* xfer) { SpecialAbilityUpdate::crc(xfer); }
void LevelGrantSpecialPower::xfer(Xfer* xfer) { SpecialAbilityUpdate::xfer(xfer); }
void LevelGrantSpecialPower::loadPostProcess() { SpecialAbilityUpdate::loadPostProcess(); }

void LevelGrantSpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	if (!RetailFileLayers::enabled())
		return;

	const auto* data = getLevelGrantSpecialPowerModuleData();
	Object* source = getObject();
	Object* target = TheGameLogic->findObjectByID(getAbilityTargetID());
	const Coord3D* center = target ? target->getPosition() : &getAbilityTargetPos();
	std::vector<ObjectID> recipients;
	const auto collect = [&](Object* candidate)
	{
		if (candidate->isEffectivelyDead() || source->getRelationship(candidate) != ALLIES ||
			!data->m_acceptanceFilter.matchesTemplate(*candidate->getTemplate()) ||
			(data->m_useKindOf && !candidate->isAnyKindOf(data->m_affectsKindOf)))
			return;
		Object* horde = HordeContain::getAnchor(candidate);
		recipients.push_back(horde ? horde->getID() : candidate->getID());
	};
	if (data->m_radius > 0.0f)
	{
		ObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(center, data->m_radius, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		for (Object* candidate = iter->first(); candidate; candidate = iter->next())
			collect(candidate);
	}
	else if (target)
		collect(target);

	std::sort(recipients.begin(), recipients.end());
	recipients.erase(std::unique(recipients.begin(), recipients.end()), recipients.end());
	for (ObjectID id : recipients)
	{
		Object* recipient = TheGameLogic->findObjectByID(id);
		if (!recipient || recipient->isEffectivelyDead())
			continue;
		auto* tracker = recipient->getExperienceTracker();
		const Real before = tracker->getRetailExperience();
		tracker->addRetailExperience(static_cast<Real>(data->m_experience), FALSE);
		if (tracker->getRetailExperience() <= before || !TheFXListStore)
			continue;
		for (const AsciiString& effect : data->m_effects)
		{
			const char* name = effect.str();
			FXList::doFXObj(TheFXListStore->findFXList(strncmp(name, "FX:", 3) == 0 ? name + 3 : name), recipient);
		}
	}
}
