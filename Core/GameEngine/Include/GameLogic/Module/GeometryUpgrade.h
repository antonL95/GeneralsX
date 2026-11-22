#pragma once

#include <vector>

#include "GameLogic/Module/UpgradeModule.h"
#include "Common/SpecialPowerINI.h"

class GeometryInfo;

class GeometryUpgradeModuleData : public UpgradeModuleData
{
public:
	std::vector<AsciiString> m_showGeometry;
	std::vector<AsciiString> m_hideGeometry;
	AsciiString m_wallBoundsMesh;
	AsciiString m_rampMesh1;
	AsciiString m_rampMesh2;
	SpecialPowerAnimation m_animation;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class GeometryUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GeometryUpgrade, "GeometryUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GeometryUpgrade, GeometryUpgradeModuleData)

public:
	GeometryUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}

	struct CollisionPart
	{
		const GeometryInfo* shape;
		Coord3D position;
	};

	class ScopedPathfindDeferral
	{
	public:
		explicit ScopedPathfindDeferral(const Object* object);
		~ScopedPathfindDeferral();
		ScopedPathfindDeferral(const ScopedPathfindDeferral&) = delete;
		ScopedPathfindDeferral& operator=(const ScopedPathfindDeferral&) = delete;

	private:
		friend class GeometryUpgrade;
		static Bool contains(const Object* object);
		static thread_local const ScopedPathfindDeferral* s_current;
		const Object* m_object;
		const ScopedPathfindDeferral* m_previous;
	};

	static Bool constructionFootprintsOverlap(const ThingTemplate& build, const Coord3D& position, Real angle, const Object& obstacle);
	static Bool collectCollisionParts(const Object& object, std::vector<CollisionPart>& parts);

	Bool isSubObjectsUpgrade() override { return FALSE; }

protected:
	void upgradeImplementation() override;

private:
	static std::vector<const GeometryUpgradeModuleData*> activeUpgrades(const Object& object, const GeometryUpgrade* applying);
};
