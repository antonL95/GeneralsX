#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/OpenContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void handleContainerFields(const RetailTestCommands::Context& ctx)
{
	const std::string_view action(ctx.action);
	if (action == "container-fields-alive")
	{
		std::fprintf(stderr, "TEST container-fields frame=%u %s alive=%d\n",
			ctx.frame, ctx.argument, ctx.findObject(TRUE) != nullptr);
		return;
	}
	Object* object = ctx.requireObject();
	if (!object)
		return;
	if (action == "container-fields-enter" && ctx.target)
	{
		GameMessage* selection = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		selection->appendBooleanArgument(TRUE);
		selection->appendObjectIDArgument(object->getID());
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		message->appendObjectIDArgument(object->getID());
		message->appendObjectIDArgument(ctx.target->getID());
	}
	else if (action == "container-fields-grab" && ctx.target)
	{
		const WeaponTemplate* templateData = TheWeaponStore->findWeaponTemplate("Slot7ContainerGrab");
		if (templateData)
		{
			Weapon* weapon = TheWeaponStore->allocateNewWeapon(templateData, PRIMARY_WEAPON);
			weapon->loadAmmoNow(object);
			weapon->fireWeapon(object, ctx.target);
			deleteInstance(weapon);
		}
	}
	else if (action == "container-fields-evacuate")
	{
		GameMessage* selection = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		selection->appendBooleanArgument(TRUE);
		selection->appendObjectIDArgument(object->getID());
		TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
	}
	else if (action == "container-fields-flame")
	{
		DamageInfo damage;
		damage.in.m_amount = 2.0f;
		damage.in.m_damageType = DAMAGE_FLAME;
		damage.in.m_deathType = DEATH_BURNED;
		object->attemptDamage(&damage);
	}
	else if (action == "container-fields-kill")
	{
		object->kill();
	}
	else if (action == "container-fields-turn")
	{
		object->setOrientation(PI / 2.0f);
	}
	else if (action == "container-fields-place" && ctx.hasPosition)
	{
		const Coord3D position = ctx.groundLocation();
		object->setPosition(&position);
	}
	else if (action == "container-fields-sample")
	{
		const Object* carrier = object->getContainedBy();
		const OpenContain* open = carrier ? carrier->getContain()->asOpenContain() : nullptr;
		const AsciiString* bone = open ? open->getPassengerBone(object->getID()) : nullptr;
		Matrix3D bonePose;
		const Bool followsBone = bone && carrier->getSingleLogicalBonePosition(bone->str(), nullptr, &bonePose) &&
			WWMath::Fabs(object->getOrientation() - bonePose.Get_Z_Rotation()) < WWMATH_EPSILON;
		const auto* body = object->getBodyModule();
		const auto* contain = object->getContain();
		const Drawable* draw = object->getDrawable();
		Matrix3D pose = *object->getTransformMatrix();
		if (draw)
			draw->applyRetailPassengerTransform(pose);
		const Vector3 offset = pose.Get_Translation() - object->getTransformMatrix()->Get_Translation();
		std::fprintf(stderr,
			"TEST container-fields frame=%u %s count=%d attached=%d bone=%s enclosed=%d follows-bone=%d health=%.1f render-offset=%.2f\n",
			ctx.frame, ctx.argument, contain ? static_cast<Int>(contain->getContainCount()) : 0,
			carrier != nullptr, bone ? bone->str() : "-", object->getEnclosingContainedBy() != nullptr,
			followsBone, body ? body->getHealth() : 0.0f, offset.Length());
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailContainerFieldsTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("container-fields-", handleContainerFields);
#endif
}
}
