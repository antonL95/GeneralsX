#define DEFINE_W3DANIMMODE_NAMES
#define DEFINE_WEAPONSLOTTYPE_NAMES

#include "W3DDevice/GameClient/Module/W3DScriptedModelDraw.h"
#include "GameClient/FXList.h"
#include "Common/ModuleFactory.h"
#include "Common/BitFlagsIO.h"
#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/KindOf.h"
#include "Common/RetailConditionMatch.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/Drawable.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/WeaponSet.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string_view>
#include <utility>

namespace
{
NameKeyType stateKey(AsciiString name)
{
	if (name.isEmpty() || name.isNone())
		return NAMEKEY_INVALID;
	name.toLower();
	return NAMEKEY(name.str());
}

void readConditions(INI* ini, ModelConditionFlags& flags)
{
	try
	{
		ModelConditionFlags::parseFromINI(ini, nullptr, &flags, nullptr);
	}
	catch (...)
	{
		if (!INIAudit::isEnabled())
			throw;
		INIAudit::record("field-error", ini->currentRawLine(), ini->getFilename().str(), ini->getLineNum());
	}
}

void updateProjectileBones(ModelConditionInfo& draw)
{
	draw.m_validStuff &= ~ModelConditionInfo::HAS_PROJECTILE_BONES;
	for (const AsciiString& bone : draw.m_weaponProjectileLaunchBoneName)
	{
		if (bone.isNotEmpty())
			draw.m_validStuff |= ModelConditionInfo::HAS_PROJECTILE_BONES;
	}
}

void copyParticles(ModelConditionInfo& draw, const std::vector<W3DScriptedModelDrawModuleData::ParticleAttachment>& particles)
{
	draw.m_particleSysBones.clear();
	draw.m_particleSysBones.reserve(particles.size());
	for (const auto& particle : particles)
		draw.m_particleSysBones.push_back(particle.draw);
}

class ScriptHost final : public DrawScript::Host
{
public:
	ScriptHost(const W3DScriptedModelDrawModuleData::AnimationState* previous, const TransitionContext& context, Bool sameState)
		: m_previous(previous), m_context(context), m_sameState(sameState)
	{
	}

	const std::string& transition() const { return m_transition; }
	Bool allowsToContinue() const { return m_continue; }
	std::vector<ScriptSubObjectAction> takeSubObjects() { return std::move(m_subObjects); }

	DrawScript::Value call(const std::string& function, const std::vector<DrawScript::Value>& arguments) override
	{
		using Value = DrawScript::Value;
		const char* argument = !arguments.empty() && arguments.front().type == Value::Type::String ?
			arguments.front().text.c_str() : nullptr;
		if (function == "CurDrawableSetTransitionAnimState")
		{
			if (argument)
				m_transition = argument;
		}
		else if (function == "CurDrawableAllowToContinue")
		{
			m_continue = TRUE;
		}
		else if (function == "CurDrawableHideSubObject")
		{
			recordSubObject(argument, TRUE, FALSE);
		}
		else if (function == "CurDrawableShowSubObject")
		{
			recordSubObject(argument, FALSE, FALSE);
		}
		else if (function == "CurDrawableHideSubObjectPermanently")
		{
			recordSubObject(argument, TRUE, TRUE);
		}
		else if (function == "CurDrawableShowSubObjectPermanently")
		{
			recordSubObject(argument, FALSE, TRUE);
		}
		else if (function == "CurDrawablePrevAnimationState")
		{
			if (!m_sameState && m_previous && m_previous->stateName.isNotEmpty())
				return Value::of(std::string(m_previous->stateName.str()));
		}
		else if (function == "CurDrawablePrevAnimation")
		{
			const AsciiString label = previousLabel();
			if (label.isNotEmpty())
				return Value::of(std::string(label.str()));
		}
		else if (function == "CurDrawablePrevAnimFraction")
		{
			return Value::of(m_sameState ? 0.0 : static_cast<double>(m_context.previousFraction));
		}
		else if (function == "CurDrawableModelcondition")
		{
			const Int bit = argument ? ModelConditionFlags::getSingleBitFromName(argument) : -1;
			if (bit >= 0 && m_context.drawable && m_context.drawable->getModelConditionFlags().test(bit))
				return Value::of(1.0);
		}
		else if (function == "CurDrawableIsCurrentTargetKindof")
		{
			const Object* object = m_context.drawable ? m_context.drawable->getObject() : nullptr;
			const AIUpdateInterface* ai = object ? object->getAI() : nullptr;
			const Object* target = ai ? ai->getCurrentVictim() : nullptr;
			const Int bit = argument ? KindOfMaskType::getSingleBitFromName(argument) : -1;
			return Value::of(target != nullptr && bit >= 0 && target->isKindOf(static_cast<KindOfType>(bit)));
		}
		else if (function == "GetClientRandomNumberReal")
		{
			if (arguments.size() == 2 && arguments[0].type == Value::Type::Number && arguments[1].type == Value::Type::Number)
				return Value::of(static_cast<double>(GameClientRandomValueReal(
					static_cast<Real>(arguments[0].number), static_cast<Real>(arguments[1].number))));
		}
		return {};
	}

private:
	void recordSubObject(const char* name, Bool hide, Bool permanent)
	{
		if (!name || *name == '\0')
			return;
		ScriptSubObjectAction action;
		action.subObjName.set(name);
		action.hide = hide;
		action.permanent = permanent;
		m_subObjects.push_back(std::move(action));
	}

	AsciiString previousLabel() const
	{
		Int index = m_context.previousAnimation;
		if (!m_previous || index < 0)
			return AsciiString::TheEmptyString;
		for (const auto& animation : m_previous->animations)
		{
			if (animation.name.isEmpty() || animation.name.isNone())
				continue;
			if (index-- == 0)
				return animation.label;
		}
		return AsciiString::TheEmptyString;
	}

	const W3DScriptedModelDrawModuleData::AnimationState* m_previous;
	const TransitionContext m_context;
	Bool m_sameState;
	std::string m_transition;
	Bool m_continue = FALSE;
	std::vector<ScriptSubObjectAction> m_subObjects;
};

Int findAnimationByLabel(const W3DScriptedModelDrawModuleData::AnimationState& state, const std::string& label)
{
	Int index = 0;
	for (const auto& animation : state.animations)
	{
		if (animation.name.isEmpty() || animation.name.isNone())
			continue;
		if (stricmp(animation.label.str(), label.c_str()) == 0)
			return index;
		++index;
	}
	return -1;
}
}

void W3DScriptedModelDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "DefaultModelConditionState", parseModelState, (void*)StateType::DefaultModel, 0 },
		{ "ModelConditionState", parseModelState, (void*)StateType::Model, 0 },
		{ "IdleAnimationState", parseAnimationState, (void*)StateType::Idle, 0 },
		{ "AnimationState", parseAnimationState, (void*)StateType::Animation, 0 },
		{ "TransitionState", parseAnimationState, (void*)StateType::Transition, 0 },
		{ "WallBoundsMesh", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_wallBoundsMesh) },
		{ "RampMesh1", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_rampMesh1) },
		{ "RampMesh2", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_rampMesh2) },
		{ "RaisedWallMesh", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_raisedWallMesh) },
		{ "WadingParticleSys", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_wadingParticleSys) },
		{ "TrackMarksLeftBone", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_trackMarksLeftBone) },
		{ "TrackMarksRightBone", INI::parseAsciiString, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_trackMarksRightBone) },
		{ "AlphaCameraFadeOuterRadius", INI::parseInt, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_alphaCameraFadeOuterRadius) },
		{ "AlphaCameraFadeInnerRadius", INI::parseInt, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_alphaCameraFadeInnerRadius) },
		{ "StaticSortLevelWhileFading", INI::parseInt, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_staticSortLevelWhileFading) },
		{ "AlphaCameraAtInnerRadius", INI::parsePercentToReal, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_alphaCameraAtInnerRadius) },
		{ "StaticModelLODMode", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_staticModelLODMode) },
		{ "ShowShadowWhileContained", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_showShadowWhileContained) },
		{ "UseStandardModelNames", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_useStandardModelNames) },
		{ "MultiPlayerOnly", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_multiPlayerOnly) },
		{ "UseDefaultAnimation", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_useDefaultAnimation) },
		{ "AffectedByStealth", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_affectedByStealth) },
		{ "GlowEnabled", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_glowEnabled) },
		{ "GlowEmissive", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_glowEmissive) },
		{ "HighDetailOnly", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_highDetailOnly) },
		{ "NoRotate", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_noRotate) },
		{ "UseProducerTexture", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_useProducerTexture) },
		{ "ShadowForceDisable", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_shadowForceDisable) },
		{ "RandomTextureFixedRandomIndex", INI::parseBool, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_randomTextureFixedRandomIndex) },
		{ "DependencySharedModelFlags", ModelConditionFlags::parseFromINI, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_dependencySharedModelFlags) },
		{ "RandomTexture", W3DScriptedModelDrawModuleData::parseRandomTexture, nullptr, offsetof(W3DScriptedModelDrawModuleData, m_randomTextures) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	if (ModuleFactory::registersRotwkModules())
		p.add(fields);
	W3DModelDrawModuleData::buildFieldParse(p);
}

void W3DScriptedModelDrawModuleData::parseModelDrawField(INI* ini, void*, void* store, const void* userData)
{
	ModelConditionInfo* draw = static_cast<ModelConditionInfo*>(store);
	const FieldParse* field = static_cast<const FieldParse*>(userData);
	field->parse(ini, draw, reinterpret_cast<char*>(draw) + field->offset, field->userData);
}

void W3DScriptedModelDrawModuleData::parseModelState(INI* ini, void* instance, void*, const void* userData)
{
	static const FieldParse fields[] =
	{
		{ "Skeleton", INI::parseAsciiString, nullptr, offsetof(ModelState, skeleton) },
		{ "OverrideTooltip", INI::parseAsciiString, nullptr, offsetof(ModelState, tooltip) },
		{ "ModelAnimationPrefix", INI::parseAsciiString, nullptr, offsetof(ModelState, animationPrefix) },
		{ "PortraitImageName", INI::parseAsciiString, nullptr, offsetof(ModelState, portraitImage) },
		{ "ButtonImageName", INI::parseAsciiString, nullptr, offsetof(ModelState, buttonImage) },
		{ "Shadow", INI::parseAsciiString, nullptr, offsetof(ModelState, shadow) },
		{ "ShadowTexture", INI::parseAsciiString, nullptr, offsetof(ModelState, shadowTexture) },
		{ "StateName", INI::parseQuotedAsciiString, nullptr, offsetof(ModelState, stateName) },
		{ "Texture", parseTexture, nullptr, offsetof(ModelState, draw.m_textureSwaps) },
		{ "ShadowSizeX", INI::parseInt, nullptr, offsetof(ModelState, shadowSizeX) },
		{ "ShadowSizeY", INI::parseInt, nullptr, offsetof(ModelState, shadowSizeY) },
		{ "ShadowMaxHeight", INI::parseInt, nullptr, offsetof(ModelState, shadowMaxHeight) },
		{ "ShadowOpacityStart", INI::parseInt, nullptr, offsetof(ModelState, shadowOpacityStart) },
		{ "ShadowOpacityPeak", INI::parseInt, nullptr, offsetof(ModelState, shadowOpacityPeak) },
		{ "ShadowOpacityEnd", INI::parseInt, nullptr, offsetof(ModelState, shadowOpacityEnd) },
		{ "ShadowOpacityFadeInTime", INI::parseInt, nullptr, offsetof(ModelState, shadowOpacityFadeInTime) },
		{ "ShadowOpacityFadeOutTime", INI::parseInt, nullptr, offsetof(ModelState, shadowOpacityFadeOutTime) },
		{ "ShadowOverrideLODVisibility", INI::parseBool, nullptr, offsetof(ModelState, shadowOverrideLODVisibility) },
		{ "RetainSubObjects", INI::parseBool, nullptr, offsetof(ModelState, retainSubObjects) },
		{ "FXEvent", W3DScriptedModelDrawModuleData::parseFXEvent, nullptr, offsetof(ModelState, fxEvents) },
		{ "ParticleSysBone", parseParticleBone, nullptr, offsetof(ModelState, particles) },
		{ "WeaponLaunchBone", parseWeaponLaunchBone, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	static const std::vector<FieldParse> drawFields = []
	{
		std::vector<FieldParse> result;
		for (const FieldParse* field = conditionStateParseTable(); field->token; ++field)
			result.push_back({ field->token, parseModelDrawField, field, offsetof(ModelState, draw), field->isScalar });
		result.push_back({ nullptr, nullptr, nullptr, 0 });
		return result;
	}();

	auto* self = static_cast<W3DScriptedModelDrawModuleData*>(instance);
	const Bool isDefault = static_cast<StateType>(reinterpret_cast<uintptr_t>(userData)) == StateType::DefaultModel;
	ModelState state = isDefault ? ModelState() : self->m_defaultModel;
	ModelConditionFlags flags;
	readConditions(ini, flags);
	state.draw.m_conditionsYesVec.assign(1, flags);

	MultiIniFieldParse tables;
	tables.add(fields);
	tables.add(drawFields.data());
	ini->initFromINIMulti(&state, tables);

	if (state.draw.m_modelName.isNone())
		state.draw.m_modelName.clear();
	updateProjectileBones(state.draw);
	copyParticles(state.draw, state.particles);

	if (isDefault)
	{
		self->m_defaultModel = state;
		self->m_defaultState = static_cast<Int>(self->m_conditionStates.size());
	}
	const std::size_t index = self->m_conditionStates.size();
	self->m_conditionStates.push_back(std::move(state.draw));
	self->m_modelStates.emplace(index, std::move(state));
}

void W3DScriptedModelDrawModuleData::parseAnimationState(INI* ini, void* instance, void*, const void* userData)
{
	static const FieldParse fields[] =
	{
		{ "Animation", parseAnimation, nullptr, offsetof(AnimationState, animations) },
		{ "Flags", INI::parseBitString32, animationFlagNames(), offsetof(AnimationState, draw.m_flags) },
		{ "ParticleSysBone", parseParticleBone, nullptr, offsetof(AnimationState, particles) },
		{ "StateName", INI::parseQuotedAsciiString, nullptr, offsetof(AnimationState, stateName) },
		{ "EnteringStateFX", INI::parseAsciiString, nullptr, offsetof(AnimationState, enteringStateFX) },
		{ "FrameForPristineBonePositions", INI::parseInt, nullptr, offsetof(AnimationState, draw.m_pristineBoneFrame) },
		{ "SimilarRestart", INI::parseBool, nullptr, offsetof(AnimationState, similarRestart) },
		{ "ShareAnimation", INI::parseBool, nullptr, offsetof(AnimationState, shareAnimation) },
		{ "AllowRepeatInRandomPick", INI::parseBool, nullptr, offsetof(AnimationState, allowRepeat) },
		{ "FXEvent", W3DScriptedModelDrawModuleData::parseFXEvent, nullptr, offsetof(AnimationState, fxEvents) },
		{ "LuaEvent", W3DScriptedModelDrawModuleData::parseLuaEvent, nullptr, offsetof(AnimationState, luaEvents) },
		{ "BeginScript", W3DScriptedModelDrawModuleData::parseScript, nullptr, offsetof(AnimationState, script) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	auto* self = static_cast<W3DScriptedModelDrawModuleData*>(instance);
	const StateType type = static_cast<StateType>(reinterpret_cast<uintptr_t>(userData));
	AnimationState state;
	state.idle = type == StateType::Idle;
	AsciiString transition;
	if (type == StateType::Transition)
	{
		transition = ini->getNextAsciiString();
		state.stateName = transition;
	}
	ModelConditionFlags flags;
	if (type == StateType::Animation || !ModuleFactory::registersRotwkModules())
		readConditions(ini, flags);
	state.draw.m_conditionsYesVec.push_back(flags);

	ini->initFromINI(&state, fields);
	copyParticles(state.draw, state.particles);
	state.draw.m_transitionKey = stateKey(state.stateName);
	for (const Animation& animation : state.animations)
	{
		if (animation.name.isEmpty() || animation.name.isNone())
			continue;
		AsciiString name = animation.name;
		name.toLower();
		state.draw.m_animations.emplace_back(name, type == StateType::Idle, animation.distance);
		state.draw.m_animations.back().setPlayback(static_cast<RenderObjClass::AnimMode>(animation.mode),
			animation.minSpeed, animation.maxSpeed,
			animation.blendTime < 0.0f ? Animation::DEFAULT_BLEND_FRAMES : animation.blendTime, animation.mustCompleteBlend);
		state.draw.m_animations.back().setUseWeaponTiming(animation.useWeaponTiming);
		state.draw.m_animations.back().setPriority(animation.priority);
	}
	if (type == StateType::Transition)
	{
		const NameKeyType key = stateKey(transition);
		state.draw.m_transitionSig = buildTransitionSig(key, key);
		self->m_namedTransitions.emplace(key, std::move(state));
	}
	else if (state.idle && RetailFileLayers::enabled())
		self->m_animationStates.insert(self->m_animationStates.begin(), std::move(state));
	else
		self->m_animationStates.push_back(std::move(state));
}

void W3DScriptedModelDrawModuleData::parseAnimation(INI* ini, void* instance, void* store, const void*)
{
	static const FieldParse fields[] =
	{
		{ "AnimationMode", INI::parseIndexList, TheAnimModeNames, offsetof(Animation, mode) },
		{ "AnimationName", INI::parseAsciiString, nullptr, offsetof(Animation, name) },
		{ "AnimationPriority", INI::parseInt, nullptr, offsetof(Animation, priority) },
		{ "AnimationBlendTime", INI::parseReal, nullptr, offsetof(Animation, blendTime) },
		{ "Distance", INI::parseReal, nullptr, offsetof(Animation, distance) },
		{ "FadeBeginFrame", INI::parseReal, nullptr, offsetof(Animation, fadeBeginFrame) },
		{ "FadeEndFrame", INI::parseReal, nullptr, offsetof(Animation, fadeEndFrame) },
		{ "UseWeaponTiming", INI::parseBool, nullptr, offsetof(Animation, useWeaponTiming) },
		{ "AnimationMustCompleteBlend", INI::parseBool, nullptr, offsetof(Animation, mustCompleteBlend) },
		{ "FadingIn", INI::parseBool, nullptr, offsetof(Animation, fadingIn) },
		{ "AnimationSpeedFactorRange", W3DScriptedModelDrawModuleData::parseAnimationRange, nullptr, offsetof(Animation, minSpeed) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	Animation animation;
	if (RetailFileLayers::enabled() && !static_cast<const AnimationState*>(instance)->idle)
		animation.mode = RenderObjClass::ANIM_MODE_LOOP;
	animation.label = ini->getNextAsciiString();
	ini->initFromINI(&animation, fields);
	static_cast<std::vector<Animation>*>(store)->push_back(std::move(animation));
}

void W3DScriptedModelDrawModuleData::parseAnimationRange(INI* ini, void* instance, void*, const void*)
{
	auto* animation = static_cast<Animation*>(instance);
	const Real minimum = INI::scanReal(ini->getNextToken());
	const Real maximum = INI::scanReal(ini->getNextToken());
	animation->minSpeed = minimum;
	animation->maxSpeed = maximum;
}

void W3DScriptedModelDrawModuleData::parseParticleBone(INI* ini, void*, void* store, const void*)
{
	ParticleAttachment particle;
	particle.draw.boneName = ini->getNextQuotedAsciiString();
	particle.draw.boneName.toLower();
	particle.templateName = ini->getNextQuotedAsciiString();
	if (particle.draw.boneName.isEmpty() || particle.templateName.isEmpty())
		throw INI_INVALID_DATA;
	if (TheParticleSystemManager && !particle.templateName.isNone())
		particle.draw.particleSystemTemplate = TheParticleSystemManager->findTemplate(particle.templateName);
	while (const char* option = ini->getNextTokenOrNull(INI::getSepsColon()))
	{
		if (stricmp(option, "FollowBone") == 0)
			particle.draw.followBone = INI::scanBool(ini->getNextToken());
		else if (stricmp(option, "HouseColor") == 0)
			particle.houseColor = INI::scanBool(ini->getNextToken());
		else if (stricmp(option, "FXTrigger") == 0)
			particle.draw.fxTrigger = ini->getNextToken();
		else if (stricmp(option, "Persist") == 0)
			particle.persist = ini->getNextToken();
		else if (stricmp(option, "PersistID") == 0)
			particle.persistId = INI::scanInt(ini->getNextToken());
		else if (stricmp(option, "OnlyIfOnWater") == 0)
			particle.onlyIfOnWater = INI::scanBool(ini->getNextToken());
		else if (stricmp(option, "OnlyIfOnLand") == 0)
			particle.onlyIfOnLand = INI::scanBool(ini->getNextToken());
		else if (stricmp(option, "Offset") == 0)
			INI::parseCoord3D(ini, nullptr, &particle.offset, nullptr);
		else if (stricmp(option, "No") == 0)
			particle.draw.followBone = FALSE;
		else
			throw INI_INVALID_DATA;
	}
	static_cast<std::vector<ParticleAttachment>*>(store)->push_back(std::move(particle));
}

void W3DScriptedModelDrawModuleData::parseTexture(INI* ini, void*, void* store, const void*)
{
	std::vector<AsciiString> row;
	INI::parseAsciiStringVector(ini, nullptr, &row, nullptr);
	if (row.empty())
		throw INI_INVALID_DATA;
	if (row.size() < 2)
		return;
	auto& swaps = *static_cast<std::vector<std::pair<AsciiString, AsciiString>>*>(store);
	const auto it = std::find_if(swaps.begin(), swaps.end(), [&](const auto& swap)
	{
		return !swap.first.compareNoCase(row[0]);
	});
	if (it == swaps.end())
		swaps.emplace_back(row[0], row[1]);
	else
		it->second = row[1];
}

void W3DScriptedModelDrawModuleData::parseWeaponLaunchBone(INI* ini, void* instance, void*, const void*)
{
	const Int slot = INI::scanIndexList(ini->getNextToken(), TheWeaponSlotTypeNames);

	AsciiString name = ini->getNextAsciiString();
	name.toLower();
	if (name.isNone())
		name.clear();

	auto* state = static_cast<ModelState*>(instance);
	state->draw.m_weaponProjectileLaunchBoneName[slot] = name;
	state->draw.addPublicBone(name);
}

void W3DScriptedModelDrawModuleData::parseFXEvent(INI* ini, void*, void* store, const void*)
{
	FXEvent event;
	while (const char* attribute = ini->getNextTokenOrNull(INI::getSepsColon()))
	{
		if (stricmp(attribute, "Frame") == 0)
			event.frame = INI::scanInt(ini->getNextToken());
		else if (stricmp(attribute, "FrameStop") == 0)
			event.frameStop = INI::scanInt(ini->getNextToken());
		else if (stricmp(attribute, "Name") == 0)
			event.name = ini->getNextToken();
		else if (stricmp(attribute, "FireWhenSkipped") == 0)
			event.fireWhenSkipped = TRUE;
		else
			throw INI_INVALID_DATA;
	}
	static_cast<std::vector<FXEvent>*>(store)->push_back(std::move(event));
}

void W3DScriptedModelDrawModuleData::parseLuaEvent(INI* ini, void*, void* store, const void*)
{
	LuaEvent event;
	while (const char* attribute = ini->getNextTokenOrNull(INI::getSepsColon()))
	{
		if (stricmp(attribute, "Frame") == 0)
			event.frame = INI::scanInt(ini->getNextToken());
		else if (stricmp(attribute, "Data") == 0)
			event.data = ini->getNextQuotedAsciiString();
		else if (stricmp(attribute, "OnStateEnter") == 0)
			event.onStateEnter = TRUE;
		else
			throw INI_INVALID_DATA;
	}
	static_cast<std::vector<LuaEvent>*>(store)->push_back(std::move(event));
}

void W3DScriptedModelDrawModuleData::parseScript(INI* ini, void*, void* store, const void*)
{
	*static_cast<std::string*>(store) = ModuleFactory::registersRotwkModules() ?
		ini->readStructuredBlock("BeginScript") : ini->readRawBlock("EndScript");
}

void W3DScriptedModelDrawModuleData::parseRandomTexture(INI* ini, void*, void* store, const void*)
{
	RandomTexture texture;
	texture.texture = ini->getNextToken();
	texture.index = INI::scanInt(ini->getNextToken());
	texture.replacement = ini->getNextAsciiString();
	static_cast<std::vector<RandomTexture>*>(store)->push_back(std::move(texture));
}

void W3DScriptedModelDrawModuleData::finishParsing()
{
	if (RetailFileLayers::enabled())
	{
		for (AnimationState& state : m_animationStates)
			addScriptedState(state);
		for (auto& [key, state] : m_namedTransitions)
			addScriptedState(state);
	}

	for (const ModelConditionInfo& model : m_conditionStates)
	{
		for (const ModelConditionFlags& flags : model.m_conditionsYesVec)
		{
			if (!flags.any())
				return;
		}
	}
	ModelConditionInfo fallback;
	fallback.m_conditionsYesVec.emplace_back();
	m_conditionStates.push_back(std::move(fallback));
}

const ModelConditionInfo* W3DScriptedModelDrawModuleData::findBestInfo(const ModelConditionFlags& flags) const
{
	const ModelConditionInfo* model = W3DModelDrawModuleData::findBestInfo(flags);
	if (!model || m_animationStates.empty())
		return model;

	ModelConditionFlags conditions = flags;
	conditions.clear(m_ignoreConditionStates);
	const AnimationState* best = nullptr;
	if (RetailFileLayers::enabled())
	{
		best = findRetailConditionMatchIf(m_animationStates, conditions,
			[](const AnimationState& state) { return state.idle; }, &AnimationState::draw);
	}
	else
	{
		Int bestMatches = 0;
		Int bestExtra = MODELCONDITION_COUNT + 1;
		for (const AnimationState& state : m_animationStates)
		{
			const ModelConditionFlags& required = state.draw.m_conditionsYesVec.front();
			const Int matches = conditions.countIntersection(required);
			const Int extra = conditions.countInverseIntersection(required);
			if (matches > bestMatches || (matches == bestMatches && extra < bestExtra))
			{
				best = &state;
				bestMatches = matches;
				bestExtra = extra;
			}
		}
		if (bestMatches == 0 && bestExtra != 0)
			best = nullptr;
	}

	if (!best)
		return model;

	return combineState(model, *best);
}

const ModelConditionInfo* W3DScriptedModelDrawModuleData::combineState(const ModelConditionInfo* model, const AnimationState& animation) const
{
	const auto key = std::make_pair(model - m_conditionStates.data(), &animation.draw);
	auto [entry, inserted] = m_combinedStates.try_emplace(key, *model);
	if (inserted)
	{
		ModelConditionInfo& combined = entry->second;
		combined.m_animations = animation.draw.m_animations;
		combined.m_flags = animation.draw.m_flags;
		combined.m_transitionKey = animation.draw.m_transitionKey;
		combined.m_transitionSig = animation.draw.m_transitionSig;
		combined.m_pristineBoneFrame = animation.draw.m_pristineBoneFrame;
		combined.m_animationState = &animation.draw;
		combined.m_modelState = model;
		combined.m_enteringStateFX = animation.enteringStateFX.isEmpty() || TheFXListStore == nullptr ? nullptr : TheFXListStore->findFXList(animation.enteringStateFX.str());
		if (!animation.fxEvents.empty())
			combined.m_animationFXEvents = &animation.fxEvents;
		combined.m_validStuff &= ModelConditionInfo::HAS_PROJECTILE_BONES | ModelConditionInfo::PUBLIC_BONES_VALID;
		combined.m_particleSysBones.insert(combined.m_particleSysBones.end(),
			animation.draw.m_particleSysBones.begin(), animation.draw.m_particleSysBones.end());
		combined.m_hideShowVec.insert(combined.m_hideShowVec.end(),
			animation.draw.m_hideShowVec.begin(), animation.draw.m_hideShowVec.end());
	}
	return &entry->second;
}

void W3DScriptedModelDrawModuleData::addScriptedState(const AnimationState& state)
{
	m_scriptedStates.emplace(&state.draw, &state);
	if (state.script.empty())
		return;

	std::shared_ptr<const DrawScript> script = DrawScript::compile(state.script);
	if (script)
		m_stateScripts.emplace(&state.draw, std::move(script));
	else
		DEBUG_LOG(("Draw script of state %s does not compile", state.stateName.str()));
}

ScriptedStateResult W3DScriptedModelDrawModuleData::runStateScript(const ModelConditionInfo* previous,
	const ModelConditionInfo* destination, const TransitionContext& context, DrawScript::Environment& globals,
	Bool inTransition) const
{
	ScriptedStateResult result;
	if (!destination)
		return result;

	const auto entry = m_stateScripts.find(destination->m_animationState);
	if (entry == m_stateScripts.end())
		return result;

	const auto previousEntry = previous ? m_scriptedStates.find(previous->m_animationState) : m_scriptedStates.end();
	const AnimationState* previousState = previousEntry == m_scriptedStates.end() ? nullptr : previousEntry->second;
	const Bool sameState = previous != nullptr && previous->m_animationState == destination->m_animationState;

	ScriptHost host(previousState, context, sameState);
	const DrawScript::Value returned = entry->second->run(host, globals);
	result.subObjects = host.takeSubObjects();

	if (!host.transition().empty())
	{
		const auto transition = m_namedTransitions.find(stateKey(host.transition().c_str()));
		if (transition != m_namedTransitions.end())
		{
			result.transition = combineState(destination->m_modelState, transition->second);
			return result;
		}
	}

	if (!inTransition && host.allowsToContinue())
	{
		result.continuePrevious = TRUE;
		return result;
	}

	if (returned.type == DrawScript::Value::Type::String && !returned.text.empty())
	{
		const auto destinationEntry = m_scriptedStates.find(destination->m_animationState);
		if (destinationEntry != m_scriptedStates.end() && !destinationEntry->second->draw.m_animations.empty())
		{
			const Int matched = findAnimationByLabel(*destinationEntry->second, returned.text);
			if (matched >= 0)
			{
				result.animationIndex = matched;
			}
			else
			{
				DEBUG_LOG(("Cannot find animation named %s", returned.text.c_str()));
				result.animationIndex = 0;
			}
		}
	}

	return result;
}

W3DScriptedModelDraw::W3DScriptedModelDraw(Thing* thing, const ModuleData* data)
	: W3DModelDraw(thing, data)
{
}

W3DScriptedModelDraw::~W3DScriptedModelDraw() = default;

ScriptedStateResult W3DScriptedModelDraw::evaluateStateScript(const ModelConditionInfo* previous,
	const ModelConditionInfo* destination, Int previousAnimation, Real previousFraction, Bool inTransition)
{
	if (!destination)
		return {};
	const TransitionContext context{getDrawable(), previousAnimation, previousFraction};
	return static_cast<const W3DScriptedModelDrawModuleData*>(getModuleData())->runStateScript(
		previous, destination, context, m_scriptGlobals, inTransition);
}

ModuleData* W3DScriptedModelDraw::friend_newModuleData(INI* ini)
{
	std::unique_ptr<W3DScriptedModelDrawModuleData> data(MSGNEW("AllModuleData") W3DScriptedModelDrawModuleData);
	if (ini)
		ini->initFromINIMultiProc(data.get(), W3DScriptedModelDrawModuleData::buildFieldParse);
	data->finishParsing();
	return data.release();
}

void W3DScriptedModelDraw::crc(Xfer* xfer)
{
	W3DModelDraw::crc(xfer);
}

void W3DScriptedModelDraw::xfer(Xfer* xfer)
{
	W3DModelDraw::xfer(xfer);
}

void W3DScriptedModelDraw::loadPostProcess()
{
	W3DModelDraw::loadPostProcess();
}

const W3DScriptedModelDrawModuleData::ModelState* W3DScriptedModelDrawModuleData::findPlacementDecal(
	const ModelConditionFlags& flags) const
{
	const ModelConditionInfo* model = W3DModelDrawModuleData::findBestInfo(flags);
	if (!model || m_shadowForceDisable)
		return nullptr;

	const auto found = m_modelStates.find(model - m_conditionStates.data());
	if (found == m_modelStates.end())
		return nullptr;
	const ModelState& state = found->second;
	return state.shadow.compareNoCase("SHADOW_ALPHA_DECAL") == 0 && state.shadowTexture.isNotEmpty() ?
		&state : nullptr;
}

void W3DScriptedModelDraw::updatePlacementDecal(const Drawable* ghost, WorldGroundDecal& decal)
{
	if (ghost)
	{
		static const NameKeyType scriptedDrawKey = NAMEKEY("W3DScriptedModelDraw");
		for (const DrawModule* const* module = ghost->getDrawModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() != scriptedDrawKey)
				continue;
			const auto* draw = static_cast<const W3DScriptedModelDraw*>(*module);
			const auto* data = static_cast<const W3DScriptedModelDrawModuleData*>(draw->getModuleData());
			const auto* state = data->findPlacementDecal(ghost->getModelConditionFlags());
			if (state)
			{
				decal.set(state->shadowTexture, *ghost->getPosition(),
					state->shadowSizeX, state->shadowSizeY, 255);
				return;
			}
		}
	}
	decal.clear();
}
