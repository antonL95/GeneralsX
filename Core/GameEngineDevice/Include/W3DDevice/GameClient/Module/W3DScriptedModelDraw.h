#pragma once

#include "W3DDevice/GameClient/Module/W3DModelDraw.h"

#include <string>

class WorldGroundDecal;

class W3DScriptedModelDrawModuleData : public W3DModelDrawModuleData
{
public:
	struct RandomTexture
	{
		AsciiString texture;
		Int index = 0;
		AsciiString replacement;
	};

	using FXEvent = ModelConditionInfo::AnimationFXEvent;

	struct LuaEvent
	{
		Int frame = 0;
		AsciiString data;
		Bool onStateEnter = FALSE;
	};

	struct ParticleAttachment
	{
		ParticleSysBoneInfo draw{};
		AsciiString templateName;
		AsciiString persist;
		Coord3D offset{};
		Int persistId = 0;
		Bool houseColor = FALSE;
		Bool onlyIfOnWater = FALSE;
		Bool onlyIfOnLand = FALSE;
	};

	struct ModelState
	{
		ModelConditionInfo draw;
		AsciiString skeleton;
		std::vector<FXEvent> fxEvents;
		std::vector<ParticleAttachment> particles;
		AsciiString tooltip;
		AsciiString stateName;
		AsciiString animationPrefix;
		AsciiString portraitImage;
		AsciiString buttonImage;
		AsciiString shadow;
		AsciiString shadowTexture;
		Int shadowSizeX = 0;
		Int shadowSizeY = 0;
		Int shadowMaxHeight = 0;
		Int shadowOpacityStart = 0;
		Int shadowOpacityPeak = 0;
		Int shadowOpacityEnd = 0;
		Int shadowOpacityFadeInTime = 0;
		Int shadowOpacityFadeOutTime = 0;
		Bool shadowOverrideLODVisibility = FALSE;
		Bool retainSubObjects = FALSE;
	};

	struct Animation
	{
		static constexpr Real DEFAULT_BLEND_FRAMES = 5.0f;

		AsciiString label;
		AsciiString name;
		Int mode = RenderObjClass::ANIM_MODE_ONCE;
		Real minSpeed = 1.0f;
		Real maxSpeed = 1.0f;
		Real distance = 0.0f;
		Int priority = 1;
		Real blendTime = DEFAULT_BLEND_FRAMES;
		Real fadeBeginFrame = 0.0f;
		Real fadeEndFrame = 0.0f;
		Bool fadingIn = FALSE;
		Bool useWeaponTiming = FALSE;
		Bool mustCompleteBlend = FALSE;
	};

	struct AnimationState
	{
		ModelConditionInfo draw;
		std::vector<Animation> animations;
		std::vector<FXEvent> fxEvents;
		std::vector<LuaEvent> luaEvents;
		std::vector<ParticleAttachment> particles;
		std::string script;
		AsciiString stateName;
		AsciiString enteringStateFX;
		Bool similarRestart = FALSE;
		Bool shareAnimation = FALSE;
		Bool allowRepeat = FALSE;
		Bool idle = FALSE;
	};

	static void buildFieldParse(MultiIniFieldParse& p);
	void finishParsing();
	const ModelConditionInfo* findBestInfo(const ModelConditionFlags& flags) const override;
	const ModelConditionInfo* findNamedTransition(const ModelConditionInfo* previous, const ModelConditionInfo* destination,
		Bool& continuePrevious) const override;
	const ModelState* findPlacementDecal(const ModelConditionFlags& flags) const;
	Bool isGlowEnabled() const override { return m_glowEnabled; }
	Bool isGlowEmissive() const override { return m_glowEmissive; }
	Bool getWallRampMeshes(AsciiString& model, AsciiString& wall, AsciiString& ramp1, AsciiString& ramp2) const override
	{
		model = m_defaultModel.draw.m_modelName;
		wall = m_wallBoundsMesh;
		ramp1 = m_rampMesh1;
		ramp2 = m_rampMesh2;
		return wall.isNotEmpty() || ramp1.isNotEmpty() || ramp2.isNotEmpty();
	}

private:
	enum class StateType { DefaultModel, Model, Idle, Animation, Transition };

	struct TransitionRequest
	{
		NameKeyType target = NAMEKEY_INVALID;
		std::vector<NameKeyType> previousStates;
		Bool excludePreviousStates = FALSE;
		Bool continuePrevious = FALSE;
	};

	static void parseModelState(INI*, void*, void*, const void*);
	static void parseAnimationState(INI*, void*, void*, const void*);
	static void parseAnimation(INI*, void*, void*, const void*);
	static void parseModelDrawField(INI*, void*, void*, const void*);
	static void parseParticleBone(INI*, void*, void*, const void*);
	static void parseTexture(INI*, void*, void*, const void*);
	static void parseWeaponLaunchBone(INI*, void*, void*, const void*);
	static void parseFXEvent(INI*, void*, void*, const void*);
	static void parseLuaEvent(INI*, void*, void*, const void*);
	static void parseScript(INI*, void*, void*, const void*);
	static void parseRandomTexture(INI*, void*, void*, const void*);
	static void parseAnimationRange(INI*, void*, void*, const void*);
	static TransitionRequest readTransitionRequest(const std::string& script);

	const ModelConditionInfo* combineState(const ModelConditionInfo* model, const AnimationState& animation) const;

	ModelState m_defaultModel;
	std::map<std::size_t, ModelState> m_modelStates;
	std::vector<AnimationState> m_animationStates;
	std::multimap<NameKeyType, AnimationState> m_namedTransitions;
	std::map<const ModelConditionInfo*, TransitionRequest> m_transitionRequests;
	mutable std::map<std::pair<std::size_t, const ModelConditionInfo*>, ModelConditionInfo> m_combinedStates;

	std::vector<RandomTexture> m_randomTextures;
	ModelConditionFlags m_dependencySharedModelFlags;
	AsciiString m_wallBoundsMesh;
	AsciiString m_rampMesh1;
	AsciiString m_rampMesh2;
	AsciiString m_raisedWallMesh;
	AsciiString m_wadingParticleSys;
	AsciiString m_trackMarksLeftBone;
	AsciiString m_trackMarksRightBone;
	Int m_alphaCameraFadeOuterRadius = 0;
	Int m_alphaCameraFadeInnerRadius = 0;
	Int m_staticSortLevelWhileFading = 0;
	Real m_alphaCameraAtInnerRadius = 0.0f;
	Bool m_staticModelLODMode = FALSE;
	Bool m_showShadowWhileContained = FALSE;
	Bool m_useStandardModelNames = FALSE;
	Bool m_multiPlayerOnly = FALSE;
	Bool m_useDefaultAnimation = FALSE;
	Bool m_affectedByStealth = TRUE;
	Bool m_glowEnabled = FALSE;
	Bool m_glowEmissive = FALSE;
	Bool m_highDetailOnly = FALSE;
	Bool m_noRotate = FALSE;
	Bool m_useProducerTexture = FALSE;
	Bool m_shadowForceDisable = FALSE;
	Bool m_randomTextureFixedRandomIndex = FALSE;
};

class W3DScriptedModelDraw : public W3DModelDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DScriptedModelDraw, "W3DScriptedModelDraw")
	MAKE_STANDARD_MODULE_MACRO(W3DScriptedModelDraw)

public:
	W3DScriptedModelDraw(Thing* thing, const ModuleData* data);
	static void updatePlacementDecal(const Drawable* ghost, WorldGroundDecal& decal);
	static ModuleData* friend_newModuleData(INI* ini);
};
