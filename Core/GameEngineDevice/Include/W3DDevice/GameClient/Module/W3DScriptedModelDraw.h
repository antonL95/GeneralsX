#pragma once

#include "W3DDevice/GameClient/Module/W3DModelDraw.h"

#include <string>

class W3DScriptedModelDrawModuleData : public W3DModelDrawModuleData
{
public:
	struct RandomTexture
	{
		AsciiString texture;
		Int index = 0;
		AsciiString replacement;
	};

	struct FXEvent
	{
		Int frame = 0;
		Int frameStop = 0;
		AsciiString name;
		Bool fireWhenSkipped = FALSE;
	};

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
		AsciiString fxTrigger;
		AsciiString persist;
		Coord3D offset{};
		Int persistId = 0;
		Bool followBone = FALSE;
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
		AsciiString label;
		AsciiString name;
		Int mode = RenderObjClass::ANIM_MODE_ONCE;
		Real minSpeed = 1.0f;
		Real maxSpeed = 1.0f;
		Real distance = 0.0f;
		Int priority = 1;
		Int blendTime = 0;
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

	const std::multimap<NameKeyType, AnimationState>& getNamedTransitions() const { return m_namedTransitions; }

private:
	enum class StateType { DefaultModel, Model, Idle, Animation, Transition };

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

	ModelState m_defaultModel;
	std::map<std::size_t, ModelState> m_modelStates;
	std::vector<AnimationState> m_animationStates;
	std::multimap<NameKeyType, AnimationState> m_namedTransitions;
	mutable std::map<std::pair<std::size_t, std::size_t>, ModelConditionInfo> m_combinedStates;

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
	static ModuleData* friend_newModuleData(INI* ini);
};
