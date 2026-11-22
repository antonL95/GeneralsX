#include "PreRTS.h"

#define DEFINE_PARTICLE_SYSTEM_NAMES

#include "Common/INI.h"
#include "GameClient/ParticleSys.h"

#include <cmath>

namespace
{
enum FXModuleEffect
{
	FX_MODULE_KEEP,
	FX_MODULE_VELOCITY,
	FX_MODULE_VOLUME,
	FX_MODULE_DRAW_PARTICLE,
	FX_MODULE_DRAW_STREAK,
	FX_MODULE_DRAW_MODEL,
	FX_MODULE_DRAW_GPU,
	FX_MODULE_UNRENDERED
};

struct FXModuleChoice
{
	const char *section;
	const char *name;
	const FieldParse *table;
	FXModuleEffect effect;
	Int value;
};

struct FXNamedValue
{
	const char *name;
	Int value;
	Bool renderable;
};

const FXNamedValue *findNamedValue(const FXNamedValue *values, const char *token)
{
	for (; values->name; ++values)
	{
		if (stricmp(values->name, token) == 0)
			return values;
	}
	throw INI_INVALID_DATA;
}

const FXNamedValue fxPriorities[] =
{
	{ "NONE",              PARTICLE_PRIORITY_LOWEST, TRUE },
	{ "ULTRA_HIGH_ONLY",   WEAPON_EXPLOSION, TRUE },
	{ "HIGH_OR_ABOVE",     BUILDUP,          TRUE },
	{ "MEDIUM_OR_ABOVE",   UNIT_DAMAGE_FX,   TRUE },
	{ "LOW_OR_ABOVE",      SEMI_CONSTANT,    TRUE },
	{ "VERY_LOW_OR_ABOVE", WEAPON_TRAIL,     TRUE },
	{ "ALWAYS_RENDER",     ALWAYS_RENDER,    TRUE },
	{ nullptr,             0,                FALSE }
};

const FXNamedValue fxShaders[] =
{
	{ "ADDITIVE",                ParticleSystemInfo::ADDITIVE,   TRUE },
	{ "ADDITIVE_ALPHA_TEST",     ParticleSystemInfo::ADDITIVE,   FALSE },
	{ "ALPHA",                   ParticleSystemInfo::ALPHA,      TRUE },
	{ "ALPHA_TEST",              ParticleSystemInfo::ALPHA_TEST, TRUE },
	{ "MULTIPLY",                ParticleSystemInfo::MULTIPLY,   TRUE },
	{ "ADDITIVE_NO_DEPTH_TEST",  ParticleSystemInfo::ADDITIVE,   FALSE },
	{ "ALPHA_NO_DEPTH_TEST",     ParticleSystemInfo::ALPHA,      FALSE },
	{ "W3D_DIFFUSE",             ParticleSystemInfo::ALPHA,      TRUE },
	{ "W3D_ALPHA",               ParticleSystemInfo::ALPHA,      TRUE },
	{ "W3D_EMISSIVE",            ParticleSystemInfo::ADDITIVE,   TRUE },
	{ nullptr,                   0,                              FALSE }
};

const FXNamedValue fxTypes[] =
{
	{ "PARTICLE",         ParticleSystemInfo::PARTICLE,        TRUE },
	{ "DRAWABLE",         ParticleSystemInfo::DRAWABLE,        TRUE },
	{ "STREAK",           ParticleSystemInfo::STREAK,          TRUE },
	{ "VOLUME_PARTICLE",  ParticleSystemInfo::VOLUME_PARTICLE, TRUE },
	{ "SMUDGE",           ParticleSystemInfo::SMUDGE,          TRUE },
	{ "TERRAIN_PARTICLE", ParticleSystemInfo::PARTICLE,        FALSE },
	{ "GPU_PARTICLE",     ParticleSystemInfo::PARTICLE,        TRUE },
	{ "GPU_TERRAINFIRE",  ParticleSystemInfo::PARTICLE,        FALSE },
	{ nullptr,            0,                                   FALSE }
};

void setZero(GameClientRandomVariable &var)
{
	var.setRange(0.0f, 0.0f, GameClientRandomVariable::CONSTANT);
}
}

void ParticleSystemTemplate::resetForFXDefinition()
{
	static_cast<ParticleSystemInfo &>(*this) = ParticleSystemInfo();
	m_shaderType = ADDITIVE;
	m_particleType = PARTICLE;
	m_particleTypeName.clear();
#if PARTICLE_USE_XY_ROTATION
	setZero(m_angleX);
	setZero(m_angleY);
	setZero(m_angularRateX);
	setZero(m_angularRateY);
#endif
	setZero(m_angleZ);
	setZero(m_angularRateZ);
	setZero(m_angularDamping);
	setZero(m_velDamping);
	setZero(m_lifetime);
	setZero(m_startSize);
	setZero(m_startSizeRate);
	setZero(m_sizeRate);
	setZero(m_sizeRateDamping);
	for (Int i = 0; i < MAX_KEYFRAMES; ++i)
	{
		setZero(m_alphaKey[i].var);
		m_alphaKey[i].frame = 0;
		m_colorKey[i].color.red = 0.0f;
		m_colorKey[i].color.green = 0.0f;
		m_colorKey[i].color.blue = 0.0f;
		m_colorKey[i].frame = 0;
	}
	setZero(m_colorScale);
	setZero(m_burstDelay);
	setZero(m_burstCount);
	setZero(m_initialDelay);
	m_slaveSystemName.clear();
	m_attachedSystemName.clear();
	m_emissionVelocityType = INVALID_VELOCITY;
	m_emissionVolumeType = POINT;
	memset(&m_emissionVelocity, 0, sizeof(m_emissionVelocity));
	memset(&m_emissionVolume, 0, sizeof(m_emissionVolume));
	m_slaveTemplate = nullptr;
	m_isRenderable = TRUE;
	m_modelHasAlpha = FALSE;
	m_modelOpacityOnly = FALSE;
	m_isGpuParticle = FALSE;
	m_hasGpuDraw = FALSE;
	m_gpuFramesPerRow = 1;
	m_gpuTotalFrames = 1;
	m_gpuSpeedMultiplier = 1.0f;
	setZero(m_gpuAngleXY);
	setZero(m_gpuAngularRateXY);
	m_gpuAngularDampingXY.setRange(1.0f, 1.0f, GameClientRandomVariable::CONSTANT);
	m_gpuDetailTexture.clear();
	for (Int axis = 0; axis < 3; ++axis)
	{
		setZero(m_modelStartSize[axis]);
		setZero(m_modelSizeRate[axis]);
		m_modelSizeDamping[axis].setRange(1.0f, 1.0f, GameClientRandomVariable::CONSTANT);
	}
	m_windStrength = DEFAULT_WIND_STRENGTH;
	m_windFullStrengthDistance = DEFAULT_WIND_FULL_STRENGTH_DISTANCE;
	m_windZeroStrengthDistance = DEFAULT_WIND_ZERO_STRENGTH_DISTANCE;
	m_cylinderOffset.zero();
}

void ParticleSystemTemplate::parseFXSystem( INI* ini, void *instance, void *, const void* userData )
{
	ini->initFromINI( instance, static_cast<const FieldParse *>(userData) );
}

void ParticleSystemTemplate::parseFXModule( INI* ini, void *instance, void *, const void* userData )
{
	static const FXModuleChoice choices[] =
	{
		{ "Color",            "DefaultColor",                  m_fxColorFieldParseTable,               FX_MODULE_KEEP,          0 },
		{ "Alpha",            "DefaultAlpha",                  m_fxAlphaFieldParseTable,               FX_MODULE_KEEP,          0 },
		{ "Update",           "DefaultUpdate",                 m_fxDefaultUpdateFieldParseTable,       FX_MODULE_KEEP,          0 },
		{ "Update",           "RenderObjectUpdate",            m_fxRenderObjectUpdateFieldParseTable,  FX_MODULE_KEEP,          0 },
		{ "Physics",          "DefaultPhysics",                m_fxPhysicsFieldParseTable,             FX_MODULE_KEEP,          0 },
		{ "Wind",             "DefaultWind",                   m_fxWindFieldParseTable,                FX_MODULE_KEEP,          0 },
		{ "Event",            "TerrainCollision",              m_fxEventFieldParseTable,               FX_MODULE_KEEP,          0 },
		{ "Emitter",          "DefaultEmitter",                m_fieldParseTable,                      FX_MODULE_KEEP,          0 },
		{ "EmissionVelocity", "OrthoEmissionVelocity",         m_fxOrthoVelocityFieldParseTable,       FX_MODULE_VELOCITY,      ORTHO },
		{ "EmissionVelocity", "SphericalEmissionVelocity",     m_fxSphericalVelocityFieldParseTable,   FX_MODULE_VELOCITY,      SPHERICAL },
		{ "EmissionVelocity", "HemisphericalEmissionVelocity", m_fxHemisphericalVelocityFieldParseTable, FX_MODULE_VELOCITY,    HEMISPHERICAL },
		{ "EmissionVelocity", "CylindricalEmissionVelocity",   m_fxCylindricalVelocityFieldParseTable, FX_MODULE_VELOCITY,      CYLINDRICAL },
		{ "EmissionVelocity", "OutwardEmissionVelocity",       m_fxOutwardVelocityFieldParseTable,     FX_MODULE_VELOCITY,      OUTWARD },
		{ "EmissionVolume",   "PointEmissionVolume",           m_fxPointVolumeFieldParseTable,         FX_MODULE_VOLUME,        POINT },
		{ "EmissionVolume",   "LineEmissionVolume",            m_fxLineVolumeFieldParseTable,          FX_MODULE_VOLUME,        LINE },
		{ "EmissionVolume",   "BoxEmissionVolume",             m_fxBoxVolumeFieldParseTable,           FX_MODULE_VOLUME,        BOX },
		{ "EmissionVolume",   "SphereEmissionVolume",          m_fxSphereVolumeFieldParseTable,        FX_MODULE_VOLUME,        SPHERE },
		{ "EmissionVolume",   "CylinderEmissionVolume",        m_fxCylinderVolumeFieldParseTable,      FX_MODULE_VOLUME,        CYLINDER },
		{ "EmissionVolume",   "LightningEmission",             m_fxLightningVolumeFieldParseTable,     FX_MODULE_UNRENDERED,    0 },
		{ "EmissionVolume",   "TerrainFireEmission",           m_fxTerrainFireVolumeFieldParseTable,   FX_MODULE_UNRENDERED,    0 },
		{ "Draw",             "DefaultDraw",                   m_fxDefaultDrawFieldParseTable,         FX_MODULE_DRAW_PARTICLE, 0 },
		{ "Draw",             "StreakDraw",                    m_fxDefaultDrawFieldParseTable,         FX_MODULE_DRAW_STREAK,   0 },
		{ "Draw",             "RenderObjectDraw",              m_fxRenderObjectDrawFieldParseTable,    FX_MODULE_DRAW_MODEL,    0 },
		{ "Draw",             "GpuDraw",                       m_fxGpuDrawFieldParseTable,             FX_MODULE_DRAW_GPU,      0 },
		{ "Draw",             "LightningDraw",                 m_fxLightningDrawFieldParseTable,       FX_MODULE_UNRENDERED,    0 },
		{ "Draw",             "ButterflyDraw",                 m_fxDefaultDrawFieldParseTable,         FX_MODULE_UNRENDERED,    0 },
		{ "Draw",             "QuadDraw",                      m_fxDefaultDrawFieldParseTable,         FX_MODULE_UNRENDERED,    0 },
		{ nullptr,            nullptr,                         nullptr,                                FX_MODULE_KEEP,          0 }
	};

	ParticleSystemTemplate *self = static_cast<ParticleSystemTemplate *>(instance);
	const char *section = static_cast<const char *>(userData);
	const char *token = ini->getNextToken();
	const FXModuleChoice *choice = choices;
	for (; choice->section; ++choice)
	{
		if (strcmp(choice->section, section) == 0 && stricmp(choice->name, token) == 0)
			break;
	}
	if (choice->section == nullptr)
		throw INI_INVALID_DATA;

	switch (choice->effect)
	{
		case FX_MODULE_DRAW_MODEL:
			self->m_particleType = DRAWABLE;
			break;
		case FX_MODULE_VELOCITY:
			self->m_emissionVelocityType = static_cast<EmissionVelocityType>(choice->value);
			break;
		case FX_MODULE_VOLUME:
			self->m_emissionVolumeType = static_cast<EmissionVolumeType>(choice->value);
			break;
		case FX_MODULE_DRAW_STREAK:
			self->m_particleType = STREAK;
			break;
		case FX_MODULE_DRAW_GPU:
			self->m_hasGpuDraw = TRUE;
			break;
		case FX_MODULE_UNRENDERED:
			self->m_isRenderable = FALSE;
			break;
		case FX_MODULE_DRAW_PARTICLE:
		case FX_MODULE_KEEP:
			break;
	}
	if (strcmp(section, "Alpha") == 0)
		self->m_modelHasAlpha = TRUE;
	ini->initFromINI( instance, choice->table );
}

void ParticleSystemTemplate::parseFXPriority( INI* ini, void *, void *store, const void* )
{
	const FXNamedValue *value = findNamedValue(fxPriorities, ini->getNextToken());
	*static_cast<ParticlePriorityType *>(store) = static_cast<ParticlePriorityType>(value->value);
}

void ParticleSystemTemplate::parseFXShader( INI* ini, void *instance, void *store, const void* )
{
	const char *token = ini->getNextToken();
	const FXNamedValue *value = findNamedValue(fxShaders, token);
	*static_cast<ParticleShaderType *>(store) = static_cast<ParticleShaderType>(value->value);
	auto *self = static_cast<ParticleSystemTemplate *>(instance);
	self->m_modelOpacityOnly = stricmp(token, "W3D_ALPHA") == 0;
	if (!value->renderable)
		self->m_isRenderable = FALSE;
}

void ParticleSystemTemplate::parseFXType( INI* ini, void *instance, void *store, const void* )
{
	const char *token = ini->getNextToken();
	const FXNamedValue *value = findNamedValue(fxTypes, token);
	*static_cast<ParticleType *>(store) = static_cast<ParticleType>(value->value);
	auto *self = static_cast<ParticleSystemTemplate *>(instance);
	self->m_isGpuParticle = stricmp(token, "GPU_PARTICLE") == 0;
	if (!value->renderable)
		self->m_isRenderable = FALSE;
}

void ParticleSystemTemplate::parseFXUnrendered( INI* ini, void *, void *, const void* )
{
	if (ini->getNextTokenOrNull() == nullptr)
		throw INI_INVALID_DATA;
	while (ini->getNextTokenOrNull() != nullptr)
	{
	}
}

Bool ParticleSystemTemplate::canDrawGpuParticles() const
{
	return m_isRenderable && m_isGpuParticle && m_hasGpuDraw && m_gpuDetailTexture.isEmpty() &&
		(m_shaderType == ADDITIVE || m_shaderType == ALPHA) && m_particleAlignment == PARTICLE_ALIGNMENT_BILLBOARD &&
		m_gpuFramesPerRow > 0 && m_gpuFramesPerRow <= 256 && m_gpuTotalFrames > 0 &&
		m_gpuTotalFrames <= m_gpuFramesPerRow * m_gpuFramesPerRow &&
		m_gpuSpeedMultiplier >= 0.0f && std::isfinite(m_gpuSpeedMultiplier);
}

ParticleSystemTemplate::GpuDrawSettings ParticleSystemTemplate::getGpuDrawSettings(const Particle &particle, Real sizeScale) const
{
	const auto range = [](const GameClientRandomVariable &value, Real scale)
	{
		return std::array<Real, 4>{value.getMinimumValue() * scale, value.getMaximumValue() * scale, 0.0f, 0.0f};
	};
	GpuDrawSettings data;
	data.draw[0] = {static_cast<Real>(m_gpuFramesPerRow), static_cast<Real>(m_gpuTotalFrames), 0.0f, 0.0f};
	for (unsigned key = 0; key < 4; ++key)
	{
		const auto &color = m_colorKey[key].color;
		const Real alpha = m_modelHasAlpha ? particle.m_alphaKey[key].value : 1.0f;
		data.draw[key + 1] = {color.red, color.green, color.blue, alpha};
		data.draw[5][key] = static_cast<Real>(m_colorKey[key].frame);
	}
	data.draw[6][0] = m_shaderType == ADDITIVE ? 1.0f : 3.0f;
	data.draw[7][0] = m_gpuSpeedMultiplier;
	data.draw[8] = range(m_colorScale, 1.0f);
	data.update = {{
		range(m_startSize, sizeScale), range(m_sizeRate, sizeScale), range(m_sizeRateDamping, 1.0f),
		range(m_gpuAngleXY, 1.0f), range(m_gpuAngularRateXY, 1.0f), range(m_gpuAngularDampingXY, 1.0f),
		range(m_angleZ, 1.0f), range(m_angularRateZ, 1.0f), range(m_angularDamping, 1.0f)
	}};
	data.physics = {{
		{0.0f, 0.0f, m_gravity, 0.0f},
		{m_driftVelocity.x, m_driftVelocity.y, m_driftVelocity.z, 0.0f},
		range(m_velDamping, 1.0f)
	}};
	return data;
}

const FieldParse ParticleSystemTemplate::m_fxSystemFieldParseTable[] =
{
	{ "Priority",                   ParticleSystemTemplate::parseFXPriority, nullptr, offsetof( ParticleSystemTemplate, m_priority ) },
	{ "IsOneShot",                  INI::parseBool,                          nullptr, offsetof( ParticleSystemTemplate, m_isOneShot ) },
	{ "Shader",                     ParticleSystemTemplate::parseFXShader,   nullptr, offsetof( ParticleSystemTemplate, m_shaderType ) },
	{ "Type",                       ParticleSystemTemplate::parseFXType,     nullptr, offsetof( ParticleSystemTemplate, m_particleType ) },
	{ "ParticleName",               INI::parseAsciiString,                   nullptr, offsetof( ParticleSystemTemplate, m_particleTypeName ) },
	{ "PerParticleAttachedSystem",  INI::parseAsciiString,                   nullptr, offsetof( ParticleSystemTemplate, m_attachedSystemName ) },
	{ "SlaveSystem",                INI::parseAsciiString,                   nullptr, offsetof( ParticleSystemTemplate, m_slaveSystemName ) },
	{ "SlavePosOffset",             INI::parseCoord3D,                       nullptr, offsetof( ParticleSystemTemplate, m_slavePosOffset ) },
	{ "Lifetime",                   INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_lifetime ) },
	{ "SystemLifetime",             INI::parseUnsignedInt,                   nullptr, offsetof( ParticleSystemTemplate, m_systemLifetime ) },
	{ "Size",                       INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_startSize ) },
	{ "StartSizeRate",              INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_startSizeRate ) },
	{ "IsGroundAligned",            INI::parseIndexList,                     GroundAlignmentTypeNames, offsetof( ParticleSystemTemplate, m_particleAlignment ) },
	{ "IsEmitAboveGroundOnly",      INI::parseBool,                          nullptr, offsetof( ParticleSystemTemplate, m_isEmitAboveGroundOnly ) },
	{ "IsParticleUpTowardsEmitter", INI::parseBool,                          nullptr, offsetof( ParticleSystemTemplate, m_isParticleUpTowardsEmitter ) },
	{ "BurstDelay",                 INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_burstDelay ) },
	{ "BurstCount",                 INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_burstCount ) },
	{ "InitialDelay",               INI::parseGameClientRandomVariable,      nullptr, offsetof( ParticleSystemTemplate, m_initialDelay ) },
	{ "SortLevel",                  ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "UseMaximumHeight",           ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "ShroudEmitter",              ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,                      nullptr,                                 nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxColorFieldParseTable[] =
{
	{ "Color1",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[0] ) },
	{ "Color2",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[1] ) },
	{ "Color3",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[2] ) },
	{ "Color4",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[3] ) },
	{ "Color5",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[4] ) },
	{ "Color6",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[5] ) },
	{ "Color7",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[6] ) },
	{ "Color8",     ParticleSystemTemplate::parseRGBColorKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_colorKey[7] ) },
	{ "ColorScale", INI::parseGameClientRandomVariable,            nullptr, offsetof( ParticleSystemTemplate, m_colorScale ) },
	{ nullptr,      nullptr,                                       nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxAlphaFieldParseTable[] =
{
	{ "Alpha1", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[0] ) },
	{ "Alpha2", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[1] ) },
	{ "Alpha3", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[2] ) },
	{ "Alpha4", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[3] ) },
	{ "Alpha5", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[4] ) },
	{ "Alpha6", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[5] ) },
	{ "Alpha7", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[6] ) },
	{ "Alpha8", ParticleSystemTemplate::parseRandomKeyframe, nullptr, offsetof( ParticleSystemTemplate, m_alphaKey[7] ) },
	{ nullptr,  nullptr,                                     nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxDefaultUpdateFieldParseTable[] =
{
	{ "SizeRate",         INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_sizeRate ) },
	{ "SizeRateDamping",  INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_sizeRateDamping ) },
	{ "AngleZ",           INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_angleZ ) },
	{ "AngularRateZ",     INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_angularRateZ ) },
	{ "AngularDamping",   INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_angularDamping ) },
	{ "Rotation",         ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "AngleXY",          INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_gpuAngleXY) },
	{ "AngularRateXY",    INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_gpuAngularRateXY) },
	{ "AngularDampingXY", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_gpuAngularDampingXY) },
	{ nullptr,            nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxRenderObjectUpdateFieldParseTable[] =
{
	{ "AngleX",         ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "AngleY",         ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "AngleZ",         INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_angleZ) },
	{ "AngularRateX",   ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "AngularRateY",   ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "AngularRateZ",   INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_angularRateZ) },
	{ "AngularDamping", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_angularDamping) },
	{ "StartSizeX", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelStartSize[0]) },
	{ "StartSizeY", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelStartSize[1]) },
	{ "StartSizeZ", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelStartSize[2]) },
	{ "SizeRateX", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeRate[0]) },
	{ "SizeRateY", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeRate[1]) },
	{ "SizeRateZ", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeRate[2]) },
	{ "SizeDampingX", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeDamping[0]) },
	{ "SizeDampingY", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeDamping[1]) },
	{ "SizeDampingZ", INI::parseGameClientRandomVariable, nullptr, offsetof(ParticleSystemTemplate, m_modelSizeDamping[2]) },
	{ "Rotation",       ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,          nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxPhysicsFieldParseTable[] =
{
	{ "Gravity",               INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_gravity ) },
	{ "VelocityDamping",       INI::parseGameClientRandomVariable,        nullptr, offsetof( ParticleSystemTemplate, m_velDamping ) },
	{ "DriftVelocity",         INI::parseCoord3D,                         nullptr, offsetof( ParticleSystemTemplate, m_driftVelocity ) },
	{ "ParticlesAttachToBone", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Swirly",                ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,                 nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxWindFieldParseTable[] =
{
	{ "WindMotion",                INI::parseIndexList,                       WindMotionNames, offsetof( ParticleSystemTemplate, m_windMotion ) },
	{ "WindStrength",              INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windStrength ) },
	{ "WindFullStrengthDist",      INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windFullStrengthDistance ) },
	{ "WindZeroStrengthDist",      INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windZeroStrengthDistance ) },
	{ "WindAngleChangeMin",        INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windAngleChangeMin ) },
	{ "WindAngleChangeMax",        INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windAngleChangeMax ) },
	{ "WindPingPongStartAngleMin", INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windMotionStartAngleMin ) },
	{ "WindPingPongStartAngleMax", INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windMotionStartAngleMax ) },
	{ "WindPingPongEndAngleMin",   INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windMotionEndAngleMin ) },
	{ "WindPingPongEndAngleMax",   INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_windMotionEndAngleMax ) },
	{ "TurbulenceAmplitude",       ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "TurbulenceFrequency",       ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,                     nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxEventFieldParseTable[] =
{
	{ "EventFX",           ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "PerParticle",       ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "KillAfterEvent",    ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "HeightOffset",      ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "OrientFXToTerrain", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,             nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxOrthoVelocityFieldParseTable[] =
{
	{ "X",     INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.ortho.x ) },
	{ "Y",     INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.ortho.y ) },
	{ "Z",     INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.ortho.z ) },
	{ nullptr, nullptr,                            nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxSphericalVelocityFieldParseTable[] =
{
	{ "Speed", INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.spherical.speed ) },
	{ nullptr, nullptr,                            nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxHemisphericalVelocityFieldParseTable[] =
{
	{ "Speed", INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.hemispherical.speed ) },
	{ nullptr, nullptr,                            nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxCylindricalVelocityFieldParseTable[] =
{
	{ "Radial", INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.cylindrical.radial ) },
	{ "Normal", INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.cylindrical.normal ) },
	{ nullptr,  nullptr,                            nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxOutwardVelocityFieldParseTable[] =
{
	{ "Speed",      INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.outward.speed ) },
	{ "OtherSpeed", INI::parseGameClientRandomVariable, nullptr, offsetof( ParticleSystemTemplate, m_emissionVelocity.outward.otherSpeed ) },
	{ nullptr,      nullptr,                            nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxPointVolumeFieldParseTable[] =
{
	{ "IsHollow", INI::parseBool, nullptr, offsetof( ParticleSystemTemplate, m_isEmissionVolumeHollow ) },
	{ nullptr,    nullptr,        nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxLineVolumeFieldParseTable[] =
{
	{ "IsHollow",   INI::parseBool,    nullptr, offsetof( ParticleSystemTemplate, m_isEmissionVolumeHollow ) },
	{ "StartPoint", INI::parseCoord3D, nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.line.start ) },
	{ "EndPoint",   INI::parseCoord3D, nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.line.end ) },
	{ nullptr,      nullptr,           nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxBoxVolumeFieldParseTable[] =
{
	{ "IsHollow", INI::parseBool,    nullptr, offsetof( ParticleSystemTemplate, m_isEmissionVolumeHollow ) },
	{ "HalfSize", INI::parseCoord3D, nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.box.halfSize ) },
	{ nullptr,    nullptr,           nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxSphereVolumeFieldParseTable[] =
{
	{ "IsHollow", INI::parseBool, nullptr, offsetof( ParticleSystemTemplate, m_isEmissionVolumeHollow ) },
	{ "Radius",   INI::parseReal, nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.sphere.radius ) },
	{ nullptr,    nullptr,        nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxCylinderVolumeFieldParseTable[] =
{
	{ "IsHollow",   INI::parseBool,                            nullptr, offsetof( ParticleSystemTemplate, m_isEmissionVolumeHollow ) },
	{ "Radius",     INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.cylinder.radius ) },
	{ "Length",     INI::parseReal,                            nullptr, offsetof( ParticleSystemTemplate, m_emissionVolume.cylinder.length ) },
	{ "Offset",     INI::parseCoord3D,                         nullptr, offsetof( ParticleSystemTemplate, m_cylinderOffset ) },
	{ "RadiusRate", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,      nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxLightningVolumeFieldParseTable[] =
{
	{ "IsHollow",   ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "StartPoint", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "EndPoint",   ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Amplitude1", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Amplitude2", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Amplitude3", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Frequency1", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Frequency2", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Frequency3", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Phase1",     ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Phase2",     ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Phase3",     ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,      nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxTerrainFireVolumeFieldParseTable[] =
{
	{ "IsHollow",           ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Xoffset",            ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Yoffset",            ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "Zoffset",            ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "CellEmissionChance", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,              nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxDefaultDrawFieldParseTable[] =
{
	{ nullptr, nullptr, nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxGpuDrawFieldParseTable[] =
{
	{ "FramesPerRow",     INI::parseUnsignedInt,              nullptr, offsetof(ParticleSystemTemplate, m_gpuFramesPerRow) },
	{ "TotalFrames",      INI::parseUnsignedInt,              nullptr, offsetof(ParticleSystemTemplate, m_gpuTotalFrames) },
	{ "SpeedMultiplier", INI::parseReal,                     nullptr, offsetof(ParticleSystemTemplate, m_gpuSpeedMultiplier) },
	{ "DetailTexture",    INI::parseAsciiString,              nullptr, offsetof(ParticleSystemTemplate, m_gpuDetailTexture) },
	{ nullptr,           nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxLightningDrawFieldParseTable[] =
{
	{ "OffsetX", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "OffsetY", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ "OffsetZ", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,   nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxRenderObjectDrawFieldParseTable[] =
{
	{ "Shader1", ParticleSystemTemplate::parseFXUnrendered, nullptr, 0 },
	{ nullptr,   nullptr,                                   nullptr, 0 }
};

const FieldParse ParticleSystemTemplate::m_fxFieldParseTable[] =
{
	{ "System",           ParticleSystemTemplate::parseFXSystem, m_fxSystemFieldParseTable, 0 },
	{ "Color",            ParticleSystemTemplate::parseFXModule, "Color",                   0 },
	{ "Alpha",            ParticleSystemTemplate::parseFXModule, "Alpha",                   0 },
	{ "Update",           ParticleSystemTemplate::parseFXModule, "Update",                  0 },
	{ "Physics",          ParticleSystemTemplate::parseFXModule, "Physics",                 0 },
	{ "Draw",             ParticleSystemTemplate::parseFXModule, "Draw",                    0 },
	{ "Wind",             ParticleSystemTemplate::parseFXModule, "Wind",                    0 },
	{ "EmissionVelocity", ParticleSystemTemplate::parseFXModule, "EmissionVelocity",        0 },
	{ "EmissionVolume",   ParticleSystemTemplate::parseFXModule, "EmissionVolume",          0 },
	{ "Event",            ParticleSystemTemplate::parseFXModule, "Event",                   0 },
	{ "Emitter",          ParticleSystemTemplate::parseFXModule, "Emitter",                 0 },
	{ nullptr,            nullptr,                               nullptr,                   0 }
};
