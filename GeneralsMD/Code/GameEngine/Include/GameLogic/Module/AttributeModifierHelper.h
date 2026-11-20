#pragma once

#include <vector>

#include "Common/AttributeModifierRules.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ObjectHelper.h"

class Object;
struct SpecialPowerObjectFilter;

class AttributeModifierHelperModuleData : public ModuleData
{
};

class AttributeModifierHelper : public ObjectHelper
{
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AttributeModifierHelper, AttributeModifierHelperModuleData )
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AttributeModifierHelper, "AttributeModifierHelper" )

public:
	static constexpr UnsignedInt USE_LIST_DURATION = 0xFFFFFFFFu;

	AttributeModifierHelper( Thing *thing, const ModuleData *modData );

	virtual UpdateSleepTime update() override;

	AttributeModifierRules::ApplyResult apply( Int list, UnsignedInt durationFrames, UnsignedInt antiMask );
	Bool remove( Int list );
	void removeCategories( UnsignedInt mask );
	void removeTimed();
	void clearAll();

	const AttributeModifierRules::Stack& getStack() const { return m_stack; }
	const AttributeModifierRules::Totals& getTotals() const;
	Bool hasAnyModifier() const;
	UnsignedInt getVersion() const { return m_version; }
	void refreshDerived();

private:
	void changed( const std::vector<Int>& added, const std::vector<Int>& removed );
	void scheduleWake();
	void grantUpgrades( Int list );
	void revokeUpgrades( Int list );
	const AttributeModifierHelper* hordeHelper() const;
	void refreshMembers();

	struct PendingUpgrade
	{
		Int list;
		UnsignedInt frame;
	};

	AttributeModifierRules::Stack m_stack;
	std::vector<PendingUpgrade> m_pendingUpgrades;
	std::vector<Int> m_appliedConditions;
	Real m_appliedHealthAdd;
	Real m_appliedHealthMult;
	Real m_appliedShroudFactor;
	UnsignedInt m_version;

	mutable AttributeModifierRules::Totals m_cachedTotals;
	mutable UnsignedInt m_cachedVersion;
	mutable const AttributeModifierHelper* m_cachedHorde;
	mutable UnsignedInt m_cachedHordeVersion;
	mutable Bool m_cacheValid;
};

namespace AttributeModifiers
{
const AttributeModifierRules::Totals* totalsOf( const Object* obj );
Bool apply( Object* target, Int list, UnsignedInt durationFrames = AttributeModifierHelper::USE_LIST_DURATION, UnsignedInt antiMask = 0 );
Bool apply( Object* target, const AsciiString& listName, UnsignedInt durationFrames = AttributeModifierHelper::USE_LIST_DURATION, UnsignedInt antiMask = 0 );
Bool remove( Object* target, const AsciiString& listName );
Int findList( const AsciiString& listName );
UnsignedInt categoryMask( const std::vector<Int>& categories );

Real damageDealtFactor( const Object* obj );
Real damageTakenFactor( const Object* obj, DamageType type );
Real rangeFactor( const Object* obj );
Real rateOfFireFactor( const Object* obj );
Real speedFactor( const Object* obj );
Real visionFactor( const Object* obj );
Real shroudFactor( const Object* obj );
Real experienceFactor( const Object* obj );
Real productionFactor( const Object* obj );
Real rechargeFactor( const Object* obj );
Real value( const Object* obj, Int kind );

enum Sides
{
	SIDES_ALLIES,
	SIDES_ENEMIES,
	SIDES_ALL
};

Bool sidesMatch( const Object* source, const Object* target, const SpecialPowerObjectFilter* filter, Sides sides );
void applyInRange( Object* source, const Coord3D* center, Real range, const SpecialPowerObjectFilter* filter, Bool affectSelf,
	Int list, UnsignedInt durationFrames, UnsignedInt antiMask, const AsciiString& fxName, Sides sides = SIDES_ALLIES );
void describe( const Object* obj, AsciiString& out );
}
