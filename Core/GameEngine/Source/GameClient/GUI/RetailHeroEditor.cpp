#include "PreRTS.h"

#include "RetailHeroEditor.h"
#include "RetailMenuPainter.h"

#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Keyboard.h"
#include "GameClient/RetailHeroProfiles.h"
#include "GameLogic/CreateAHeroSystem.h"

using namespace RetailMenuPainter;

class RetailHeroEditorData
{
public:
	enum class Page { List, Class, Appearance, Powers };

	AptScene stageArt{"CreateAHero"};
	AptScene managerArt{"CahManager", "#9"};
	AptScene classArt{"CahClass"};
	AptScene appearanceArt{"CahAppearance"};
	AptScene powersArt{"CahPowers"};
	std::vector<RetailHeroProfile> profiles = loadRetailHeroProfiles();
	std::vector<const CommandButton *> powers = CreateAHeroSystem::getDesignerPowerButtons();
	std::vector<const CreateAHeroClassDef *> classes;
	CreateAHeroRules::SelectionData draft;
	Page page = Page::List;
	Int selected = profiles.empty() ? -1 : 0;
	Int editIndex = -1;
	Int classIndex = 0;
	Int typeIndex = 0;
	Int listStart = 0;
	Int powerStart = 0;
	Int rank = 0;
	Bool nameFocused = FALSE;
	Bool attributes = FALSE;
	Bool confirmDelete = FALSE;
	UnicodeString feedback;
	Int baseCost = 0;

	RetailHeroEditorData()
	{
		for (const auto &heroClass : CreateAHeroSystem::getClasses())
			if (!heroClass.subClasses.empty())
				classes.push_back(&heroClass);
#if !RTS_GENERALS
		for (Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i)
		{
			const auto *faction = ThePlayerTemplateStore->getNthPlayerTemplate(i);
			for (const auto &name : faction->getBuildableHeroesMP())
			{
				const auto *unit = TheThingFactory->findTemplate(name, FALSE);
				if (unit && unit->isKindOf(KINDOF_CREATE_A_HERO))
				{
					baseCost = unit->friend_getBuildCost();
					return;
				}
			}
		}
#endif
	}

	const CreateAHeroClassDef &heroClass() const { return *classes[classIndex]; }
	const CreateAHeroSubClassDef &type() const { return heroClass().subClasses[typeIndex]; }

	void resetDraft()
	{
		auto name = std::move(draft.heroName);
		CreateAHeroSystem::buildDefaultSelection(heroClass().upgradeName, type().upgradeName, draft);
		draft.heroName = std::move(name);
		resetAttributes(FALSE);
		powerStart = rank = 0;
	}

	void resetAttributes(Bool recommended)
	{
		draft.attributeUpgrades.clear();
		Int remaining = std::max(0, type().spendableAttributePoints);
		for (const auto &attribute : type().attributes)
		{
			const auto options = CreateAHeroSystem::getGroupUpgrades(attribute.groupName);
			const auto minimum = std::find(options.begin(), options.end(), attribute.minValueUpgrade);
			const auto maximum = std::find(minimum, options.end(), attribute.maxValueUpgrade);
			if (maximum == options.end())
				continue;
			const auto preferred = std::find(minimum, maximum + 1, attribute.defaultValueUpgrade);
			const Int spend = recommended && preferred <= maximum ? std::min(remaining, Int(preferred - minimum)) : 0;
			draft.attributeUpgrades.emplace_back((minimum + spend)->str());
			remaining -= spend;
		}
	}

	const CommandButton *powerAt(const CreateAHeroRules::SelectionData &hero, Int level) const
	{
		if (level >= Int(hero.powerButtons.size()) || hero.powerButtons[level].empty())
			return nullptr;
		return TheRetailCommandCatalog->findCommandButton(hero.powerButtons[level].c_str());
	}

	std::vector<const CommandButton *> eligiblePowers() const
	{
		std::vector<std::string> earlier(draft.powerButtons.begin(),
			draft.powerButtons.begin() + std::min(size_t(rank), draft.powerButtons.size()));
		std::vector<const CommandButton *> eligible;
		for (const auto *power : powers)
			if (CreateAHeroSystem::isPowerButtonLegalForSelection(power, draft, rank + 1, earlier) &&
				std::find(draft.powerButtons.begin(), draft.powerButtons.end(), power->getName().str()) == draft.powerButtons.end())
				eligible.push_back(power);
		return eligible;
	}

	Bool changePower(const CommandButton *power)
	{
		auto candidate = draft;
		candidate.powerButtons.resize(CreateAHeroRules::MAX_POWER_COUNT);
		candidate.powerButtons[rank] = power ? power->getName().str() : "";
		CreateAHeroRules::SelectionData normalized;
		if (!CreateAHeroSystem::validateSelection(candidate, {}, normalized))
		{
			feedback = localize("APT:RequiredHeroLevel", "Remove the dependent power first");
			return FALSE;
		}
		draft = std::move(normalized);
		feedback.clear();
		return TRUE;
	}
};

namespace
{
	constexpr Box editorBack{60, 709, 230, 762};
	constexpr Box editorNext{794, 709, 964, 762};
	constexpr Box editorNew{243, 622, 397, 662};
	constexpr Box editorDelete{80, 622, 234, 662};
	constexpr Box editorCustomize{436, 603, 613, 666};
	constexpr Box editorName{660, 223, 944, 252};
	constexpr Box editorAppearanceTab{647, 264, 798, 289};
	constexpr Box editorAttributesTab{803, 264, 954, 289};
	constexpr Box editorResetAttributes{646, 649, 795, 690};
	constexpr Box editorRecommendAttributes{805, 649, 954, 690};
	constexpr Box editorResetPowers{876, 146, 962, 178};
	constexpr Box editorRemovePower{538, 377, 662, 407};
	constexpr Box editorPowersUp{74, 377, 105, 407};
	constexpr Box editorPowersDown{106, 377, 137, 407};
	constexpr Int editorListRows = 12;
	constexpr Int editorPowerRows = 6;

	Box heroListRow(Int i)
	{
		const Real top = 191 + i * 34;
		return {59, top, 418, top + 32};
	}

	Box heroClassIcon(Int i)
	{
		const Real left = 629 + i * 43;
		return {left, 202, left + 41, 243};
	}

	Box heroTypeIcon(Int i, Int count)
	{
		const Real left = 799 - (count * 42.5f - 1.5f) / 2 + i * 42.5f;
		return {left, 412, left + 41, 453};
	}

	Box heroBlingRow(Int i)
	{
		const Real top = 292 + i * 32.85f;
		return {646, top, 954, top + 26};
	}

	Box heroPowerChoice(Int i)
	{
		const Real top = 177 + i * 31;
		return {74, top, 662, top + 29};
	}

	Box heroPowerRank(Int i)
	{
		const Real top = 181 + i * 47.24f;
		return {692, top, 944, top + 41};
	}

	Box heroColorChannel(Int color, Int component)
	{
		const Real top = 563 + color * 30;
		const Real left = 705 + component * 80;
		return {left, top, left + 74, top + 23};
	}

	UnicodeString heroTag(const AsciiString &tag)
	{
		return tag.isEmpty() ? UnicodeString() : localize(tag.str(), tag.str());
	}

	UnicodeString heroPowerName(const CommandButton &power)
	{
		const auto source = heroTag(power.getTextLabel());
		UnicodeString label;
		for (Int i = 0; i < source.getLength(); ++i)
		{
			const auto ch = source.getCharAt(i);
			if (ch != L'&')
				label.concat(ch);
		}
		return label;
	}

	void editorPanel(const Painter &p, const Box &box)
	{
		fill(p, box, 0xE3213039);
		frame(p, box, "MapPreviewFrame");
	}

	void heroIcon(const Painter &p, const Box &box, const AsciiString &image, Bool selected)
	{
		fill(p, box, selected ? 0xFFBDA56A : 0xFF14222A);
		const Box inner{box.left + 2, box.top + 2, box.right - 2, box.bottom - 2};
		p.picture(TheMappedImageCollection->findImageByName(image), inner);
	}

	std::vector<AsciiString> heroBlingOptions(const RetailHeroEditorData &editor, const CreateAHeroBlingBinderDef &binder)
	{
		auto options = CreateAHeroSystem::getGroupUpgrades(binder.groupName);
		if (!editor.attributes)
			std::erase_if(options, [&](const auto &upgrade) {
				return std::find(editor.type().blingUpgrades.begin(), editor.type().blingUpgrades.end(), upgrade) ==
					editor.type().blingUpgrades.end();
			});
		else
		{
			const auto &attributes = editor.type().attributes;
			const auto range = std::find_if(attributes.begin(), attributes.end(),
				[&](const auto &attr) { return attr.groupName == binder.groupName; });
			if (range == attributes.end())
				return {};
			const auto lo = std::find(options.begin(), options.end(), range->minValueUpgrade);
			const auto hi = std::find(lo, options.end(), range->maxValueUpgrade);
			if (hi == options.end())
				return {};
			options = std::vector<AsciiString>(lo, hi + 1);
		}
		return options;
	}

	Int heroBlingChoice(const std::vector<AsciiString> &options, const std::vector<std::string> &selected)
	{
		const auto current = std::find_if(options.begin(), options.end(), [&](const auto &upgrade) {
			return std::find(selected.begin(), selected.end(), upgrade.str()) != selected.end();
		});
		return current == options.end() ? -1 : Int(current - options.begin());
	}

	void changeHeroBling(RetailHeroEditorData &editor, const std::vector<AsciiString> &options, Int direction)
	{
		auto candidate = editor.draft;
		auto &selected = editor.attributes ? candidate.attributeUpgrades : candidate.blingUpgrades;
		const Int current = heroBlingChoice(options, selected);
		const Int next = editor.attributes ? std::clamp(current + direction, 0, Int(options.size()) - 1) :
			(current + direction + Int(options.size())) % Int(options.size());
		if (current >= 0)
			std::erase(selected, std::string(options[current].str()));
		selected.emplace_back(options[next].str());
		CreateAHeroRules::SelectionData normalized;
		if (CreateAHeroSystem::validateSelection(candidate, {}, normalized))
		{
			editor.draft = std::move(normalized);
			editor.feedback.clear();
		}
		else
			editor.feedback = localize("APT:DistributeAttributePoints", "No attribute points remaining");
	}

	void drawHeroEditor(const RetailHeroEditorData &editor, const Painter &p)
	{
		using Page = RetailHeroEditorData::Page;
		const Bool list = editor.page == Page::List;
		const Bool classPage = editor.page == Page::Class;
		const Bool appearance = editor.page == Page::Appearance;
		editor.stageArt.drawAt([](const std::string &path) {
			return childOf(path, "_root.Main.Title.@13") || childOf(path, "_root.Main.@1.@5");
		}, 0.0f, 0.0f, p.sx, p.sy, 1.0f);
		p.text(localize("APT:CreateAHero", "Create a Hero"), {367, 0, 659, 55}, 26);
		const auto &backArt = list ? editor.managerArt : classPage ? editor.classArt :
			appearance ? editor.appearanceArt : editor.powersArt;
		const char *backPath = list ? "_root.mcClassButtons.MainMenu" : classPage ? "_root.mcClassButtons.Cancel" :
			appearance ? "_root.MainButtons.Back" : "_root.mcClassButtons.Back";
		if (!moviePart(p, backArt, backPath, editorBack))
			button(p, editorBack, UnicodeString());
		p.text(list ? localize("APT:MainMenu", "Main Menu") : localize("APT:Back", "Back"), editorBack, 18);

		if (list)
		{
			editorPanel(p, {34, 123, 995, 693});
			p.text(localize("APT:SelectHero", "Select Hero"), {331, 104, 693, 139}, 20, muted);
			editorPanel(p, {55, 152, 421, 671});
			p.text(localize("APT:SortHeroName", "Name"), {104, 157, 253, 189}, 17, muted);
			p.text(localize("APT:SortHeroType", "Type"), {253, 157, 417, 189}, 17, muted);
			for (Int i = 0; i < editorListRows && editor.listStart + i < Int(editor.profiles.size()); ++i)
			{
				const Int index = editor.listStart + i;
				const auto &profile = editor.profiles[index];
				const auto *heroClass = CreateAHeroSystem::findClassByUpgrade(profile.selection.classUpgrade.c_str());
				const auto *type = CreateAHeroSystem::findSubClassByUpgrade(heroClass, profile.selection.subClassUpgrade.c_str());
				const auto box = heroListRow(i);
				fill(p, box, index == editor.selected ? 0xFF355467 : 0xD016242D);
				heroIcon(p, {box.left, box.top, box.left + 32, box.bottom}, type->iconImage, index == editor.selected);
				p.text(translated(profile.selection.heroName.c_str()), {104, box.top, 253, box.bottom}, 16, white, "Albertus MT", FALSE);
				p.text(heroTag(type->nameTag), {253, box.top, box.right, box.bottom}, 14, muted, "Albertus MT", FALSE);
			}
			button(p, editorNew, localize("APT:CreateNewHero", "New Hero"));
			if (editor.selected < 0)
				return;
			const auto &profile = editor.profiles[editor.selected];
			const auto *heroClass = CreateAHeroSystem::findClassByUpgrade(profile.selection.classUpgrade.c_str());
			const auto *type = CreateAHeroSystem::findSubClassByUpgrade(heroClass, profile.selection.subClassUpgrade.c_str());
			if (!profile.preset)
			{
				button(p, editorCustomize, localize("APT:CustomizeAppearance", "Customize"));
				button(p, editorDelete, editor.confirmDelete ? translated("Confirm Delete") : localize("APT:DeleteHero", "Delete"));
			}
			editorPanel(p, {773, 191, 954, 658});
			p.text(translated(profile.selection.heroName.c_str()), {773, 194, 954, 229}, 22);
			p.text(heroTag(type->nameTag), {779, 229, 948, 261}, 16, muted);
			p.text(heroTag(type->descriptionTag), {784, 272, 944, 442}, 15, muted, "Albertus MT", FALSE, TRUE);
			auto cost = localize("GUI:CAHHeroCost", "Recruit cost");
			cost.concat(L"  ");
			cost.concat(number(editor.baseCost + CreateAHeroSystem::selectionPowerCost(profile.selection)));
			p.text(cost, {784, 448, 944, 482}, 16);
			heroIcon(p, {507, 254, 699, 446}, type->iconImage, TRUE);
			for (Int i = 0; i < Int(profile.selection.powerButtons.size()); ++i)
				if (const auto *power = editor.powerAt(profile.selection, i))
				{
					const Real left = 437 + i * 32;
					p.picture(power->getButtonImage(), {left, 513, left + 31, 544});
				}
		}
		else
		{
			const char *nextPath = classPage ? "_root.mcClassButtons.Next" :
				appearance ? "_root.MainButtons.Next" : "_root.mcClassButtons.Done";
			if (!moviePart(p, backArt, nextPath, editorNext))
				button(p, editorNext, UnicodeString());
			p.text(editor.page == Page::Powers ? localize("APT:Done", "Done") : localize("APT:Next", "Next"), editorNext, 18);
		}

		if (classPage)
		{
			editorPanel(p, {595, 148, 1000, 698});
			p.text(localize("APT:SelectHeroClassTitle", "Select Hero Class"), {630, 159, 971, 183}, 20, muted);
			for (Int i = 0; i < Int(editor.classes.size()); ++i)
				heroIcon(p, heroClassIcon(i), editor.classes[i]->iconImage, i == editor.classIndex);
			p.text(heroTag(editor.heroClass().nameTag), {639, 253, 959, 277}, 21);
			p.text(heroTag(editor.heroClass().descriptionTag), {639, 284, 959, 349}, 16, muted, "Albertus MT", FALSE, TRUE);
			p.text(localize("GUI:CAHHeroType", "Hero Type"), {639, 366, 959, 391}, 19, muted);
			const auto &types = editor.heroClass().subClasses;
			for (Int i = 0; i < Int(types.size()); ++i)
				heroIcon(p, heroTypeIcon(i, Int(types.size())), types[i].iconImage, i == editor.typeIndex);
			p.text(heroTag(editor.type().nameTag), {639, 462, 959, 486}, 21);
			p.text(heroTag(editor.type().descriptionTag), {639, 494, 959, 671}, 16, muted, "Albertus MT", FALSE, TRUE);
			heroIcon(p, {214, 275, 421, 482}, editor.type().iconImage, TRUE);
		}
		if (appearance)
		{
			editorPanel(p, {599, 143, 992, 697});
			p.text(heroTag(editor.heroClass().nameTag), {646, 154, 954, 178}, 18, muted);
			p.text(heroTag(editor.type().nameTag), {646, 179, 954, 202}, 20);
			const auto name = editor.draft.heroName.empty() ? localize("APT:EnterHeroNamePrompt", "Enter Hero Name") :
				translated(editor.draft.heroName.c_str());
			control(p, editorName, name, editor.nameFocused);
			button(p, editorAppearanceTab, localize("APT:AppearanceTab", "Appearance"));
			button(p, editorAttributesTab, localize("APT:AttributesTab", "Attributes"));
			Int spent = 0;
			for (const auto &binder : CreateAHeroSystem::getBlingBinders())
			{
				if (binder.blingType != Int(!editor.attributes))
					continue;
				const auto options = heroBlingOptions(editor, binder);
				if (options.empty())
					continue;
				const auto &selected = editor.attributes ? editor.draft.attributeUpgrades : editor.draft.blingUpgrades;
				const Int index = heroBlingChoice(options, selected);
				const auto *bling = index < 0 ? nullptr : CreateAHeroSystem::findBlingByUpgrade(options[index]);
				auto label = heroTag(binder.labelTag);
				label.concat(L"  ");
				if (editor.attributes)
				{
					const auto upgrades = CreateAHeroSystem::getGroupUpgrades(binder.groupName);
					const auto value = index < 0 ? upgrades.end() : std::find(upgrades.begin(), upgrades.end(), options[index]);
					if (value != upgrades.end())
						label.concat(number(Int(value - upgrades.begin() + 1)));
					spent += std::max(0, index);
				}
				else if (bling)
				{
					const auto value = bling->nameTag.isEmpty() ? UnicodeString() : localize(bling->nameTag.str(), "");
					label.concat(value.isEmpty() ? number(index + 1) : value);
				}
				const auto box = heroBlingRow(binder.uiSlot);
				control(p, box, UnicodeString());
				p.text(label, inset(box, 28, 28), 16);
				p.text(translated("<"), {box.left, box.top, box.left + 26, box.bottom}, 17, muted);
				p.text(translated(">"), {box.right - 26, box.top, box.right, box.bottom}, 17, muted);
			}
			if (editor.attributes)
			{
				auto label = localize("APT:DistributeAttributePoints", "Attribute points remaining");
				label.concat(L"  ");
				label.concat(number(editor.type().spendableAttributePoints - spent));
				p.text(label, {646, 516, 954, 552}, 17);
				button(p, editorResetAttributes, localize("APT:Reset", "Reset"));
				button(p, editorRecommendAttributes, localize("APT:Recommend", "Recommend"));
			}
			else
			{
				p.text(localize("APT:SelectColors", "Select Colors"), {646, 523, 954, 552}, 17, muted);
				const int *colors[] = {editor.draft.primaryColor, editor.draft.secondaryColor, editor.draft.tertiaryColor};
				for (Int i = 0; i < Int(std::size(colors)); ++i)
				{
					const int *rgb = colors[i];
					const auto first = heroColorChannel(i, 0);
					fill(p, {646, first.top, 692, first.bottom}, GameMakeColor(rgb[0], rgb[1], rgb[2], 255));
					for (Int c = 0; c < 3; ++c)
					{
						const auto box = heroColorChannel(i, c);
						control(p, box, number(rgb[c]));
						fill(p, {box.left, box.bottom - 3, box.left + (box.right - box.left) * rgb[c] / 255, box.bottom},
							c == 0 ? 0xFFCA6758 : c == 1 ? 0xFF72B96B : 0xFF6C9DC8);
					}
				}
			}
			heroIcon(p, {214, 275, 421, 482}, editor.type().iconImage, TRUE);
		}
		if (editor.page == Page::Powers)
		{
			editorPanel(p, {70, 143, 667, 407});
			editorPanel(p, {683, 143, 966, 687});
			auto heading = localize("APT:HeaderChooseHeroPowers", "Choose Hero Powers");
			heading.concat(L"  ");
			heading.concat(number(editor.rank + 1));
			p.text(heading, {74, 145, 662, 177}, 20, muted);
			const auto choices = editor.eligiblePowers();
			for (Int i = 0; i < editorPowerRows && editor.powerStart + i < Int(choices.size()); ++i)
			{
				const auto *power = choices[editor.powerStart + i];
				const auto box = heroPowerChoice(i);
				fill(p, box, 0xDF192B34);
				p.picture(power->getButtonImage(), {box.left, box.top, box.left + 29, box.bottom});
				p.text(heroPowerName(*power), inset(box, 38, 62), 16, white, "Albertus MT", FALSE);
				p.text(number(CreateAHeroSystem::effectivePowerCost(power, editor.rank + 1)),
					{box.right - 58, box.top, box.right, box.bottom}, 16, muted);
			}
			button(p, editorRemovePower, localize("APT:NoPower", "No Power"));
			if (editor.powerStart > 0)
				p.picture(TheMappedImageCollection->findImageByName("AptVSliderUpButtonEnabled"), editorPowersUp);
			if (editor.powerStart + editorPowerRows < Int(choices.size()))
				p.picture(TheMappedImageCollection->findImageByName("AptVSliderDownButtonEnabled"), editorPowersDown);
			button(p, editorResetPowers, localize("APT:Reset", "Reset"));
			p.text(localize("APT:CurrentPowers", "Current Powers"), {692, 145, 871, 178}, 19, muted);
			for (Int i = 0; i < Int(CreateAHeroRules::MAX_POWER_COUNT); ++i)
			{
				const auto box = heroPowerRank(i);
				const auto *power = editor.powerAt(editor.draft, i);
				fill(p, box, editor.rank == i ? 0xFF355467 : 0xDF192B34);
				p.text(number(i + 1), {box.left, box.top, box.left + 28, box.bottom}, 16, muted);
				if (power)
				{
					p.text(heroPowerName(*power), inset(box, 32, 43), 15, white, "Albertus MT", FALSE);
					p.picture(power->getButtonImage(), {box.right - 41, box.top, box.right, box.bottom});
				}
			}
			auto total = localize("GUI:CAHHeroCost", "Recruit cost");
			total.concat(L"  ");
			total.concat(number(editor.baseCost + CreateAHeroSystem::selectionPowerCost(editor.draft)));
			p.text(total, {695, 655, 953, 681}, 17);
			heroIcon(p, {249, 483, 414, 648}, editor.type().iconImage, TRUE);
			if (const auto *power = editor.powerAt(editor.draft, editor.rank))
				p.text(heroTag(power->getDescriptionLabel()), {427, 484, 667, 646}, 16, muted, "Albertus MT", FALSE, TRUE);
		}
		p.text(editor.feedback, {235, 704, 789, 762}, 17, 0xFFFFC980);
	}

	void loadEditorDraft(RetailHeroEditorData &editor)
	{
		const auto &profile = editor.profiles[editor.selected];
		editor.editIndex = profile.preset ? -1 : editor.selected;
		editor.draft = profile.selection;
		const auto *heroClass = CreateAHeroSystem::findClassByUpgrade(editor.draft.classUpgrade.c_str());
		editor.classIndex = Int(std::find(editor.classes.begin(), editor.classes.end(), heroClass) - editor.classes.begin());
		editor.typeIndex = Int(CreateAHeroSystem::findSubClassByUpgrade(heroClass, editor.draft.subClassUpgrade.c_str()) -
			heroClass->subClasses.data());
		editor.rank = editor.powerStart = 0;
		editor.page = RetailHeroEditorData::Page::Appearance;
		editor.attributes = FALSE;
	}

	Bool heroEditorInput(RetailHeroEditorData &editor, UnsignedInt message, WindowMsgData point, Bool key, Real x, Real y)
	{
		using Page = RetailHeroEditorData::Page;
		const Bool click = message == GWM_LEFT_UP;
		const Int scroll = message == GWM_WHEEL_UP || (key && point == KEY_UP) ? -1 :
			message == GWM_WHEEL_DOWN || (key && point == KEY_DOWN) ? 1 : 0;
		const Bool back = (click && editorBack.contains(x, y)) || (key && point == KEY_ESC);
		const Bool next = (click && editorNext.contains(x, y)) || (key && point == KEY_ENTER);
		if (back)
		{
			editor.feedback.clear();
			editor.nameFocused = FALSE;
			switch (editor.page)
			{
				case Page::List: return FALSE;
				case Page::Class: editor.page = Page::List; break;
				case Page::Appearance:
					if (editor.attributes && editor.editIndex < 0)
						editor.attributes = FALSE;
					else
						editor.page = editor.editIndex < 0 ? Page::Class : Page::List;
					break;
				case Page::Powers: editor.page = Page::Appearance; break;
			}
			return TRUE;
		}
		switch (editor.page)
		{
			case Page::List:
				if (scroll)
					editor.listStart = std::clamp(editor.listStart + scroll, 0, std::max(0, Int(editor.profiles.size()) - editorListRows));
				if (!click)
					break;
				if (editorNew.contains(x, y))
				{
					editor.editIndex = -1;
					editor.classIndex = editor.typeIndex = 0;
					editor.attributes = FALSE;
					editor.draft = {};
					editor.resetDraft();
					editor.page = Page::Class;
				}
				else if (editor.selected >= 0 && !editor.profiles[editor.selected].preset && editorCustomize.contains(x, y))
					loadEditorDraft(editor);
				else if (editor.selected >= 0 && !editor.profiles[editor.selected].preset && editorDelete.contains(x, y))
				{
					if (editor.confirmDelete)
					{
						if (deleteRetailHeroProfile(editor.profiles[editor.selected]))
						{
							editor.profiles.erase(editor.profiles.begin() + editor.selected);
							editor.selected = std::min(editor.selected, Int(editor.profiles.size()) - 1);
							editor.listStart = std::min(editor.listStart, std::max(0, Int(editor.profiles.size()) - editorListRows));
						}
						else
							editor.feedback = translated("Could not delete hero");
					}
					editor.confirmDelete = !editor.confirmDelete;
					break;
				}
				else
					for (Int i = 0; i < editorListRows && editor.listStart + i < Int(editor.profiles.size()); ++i)
						if (heroListRow(i).contains(x, y))
							editor.selected = editor.listStart + i;
				editor.confirmDelete = FALSE;
				break;
			case Page::Class:
				if (next)
					editor.page = Page::Appearance;
				else if (click)
				{
					for (Int i = 0; i < Int(editor.classes.size()); ++i)
						if (heroClassIcon(i).contains(x, y) && i != editor.classIndex)
						{
							editor.classIndex = i;
							editor.typeIndex = 0;
							editor.resetDraft();
						}
					const auto &types = editor.heroClass().subClasses;
					for (Int i = 0; i < Int(types.size()); ++i)
						if (heroTypeIcon(i, Int(types.size())).contains(x, y) && i != editor.typeIndex)
						{
							editor.typeIndex = i;
							editor.resetDraft();
						}
				}
				break;
			case Page::Appearance:
				if (next)
				{
					if (!editor.attributes && editor.editIndex < 0)
					{
						editor.attributes = TRUE;
						editor.nameFocused = FALSE;
						editor.feedback.clear();
						break;
					}
					AsciiString name(editor.draft.heroName.c_str());
					name.trim();
					if (name.isEmpty())
					{
						editor.feedback = localize("APT:EnterHeroNamePrompt", "Enter a hero name");
						editor.nameFocused = TRUE;
						break;
					}
					editor.draft.heroName = name.str();
					editor.nameFocused = FALSE;
					editor.feedback.clear();
					editor.page = Page::Powers;
				}
				else if (editor.nameFocused && key && point == KEY_BACKSPACE && !editor.draft.heroName.empty())
					editor.draft.heroName.pop_back();
				else if (editor.nameFocused && message == GWM_IME_CHAR && point <= 127 &&
					CreateAHeroRules::isSafeTokenChar(char(point)) && editor.draft.heroName.size() < CreateAHeroRules::MAX_HERO_NAME_LENGTH)
					editor.draft.heroName.push_back(char(point));
				else if (click)
				{
					editor.nameFocused = editorName.contains(x, y);
					if (editorAppearanceTab.contains(x, y) || editorAttributesTab.contains(x, y))
						editor.attributes = editorAttributesTab.contains(x, y);
					if (editor.attributes && (editorResetAttributes.contains(x, y) || editorRecommendAttributes.contains(x, y)))
					{
						editor.resetAttributes(editorRecommendAttributes.contains(x, y));
						editor.feedback.clear();
					}
					for (const auto &binder : CreateAHeroSystem::getBlingBinders())
						if (binder.blingType == Int(!editor.attributes))
						{
							const auto box = heroBlingRow(binder.uiSlot);
							if (box.contains(x, y))
							{
								const auto options = heroBlingOptions(editor, binder);
								if (!options.empty())
									changeHeroBling(editor, options, x < (box.left + box.right) / 2 ? -1 : 1);
							}
						}
					if (!editor.attributes)
					{
						int *colors[] = {editor.draft.primaryColor, editor.draft.secondaryColor, editor.draft.tertiaryColor};
						for (Int i = 0; i < Int(std::size(colors)); ++i)
							for (Int c = 0; c < 3; ++c)
							{
								const auto box = heroColorChannel(i, c);
								if (box.contains(x, y))
									colors[i][c] = std::clamp(Int((x - box.left) * 255 / (box.right - box.left) + 0.5f), 0, 255);
							}
					}
				}
				break;
			case Page::Powers:
				if (next)
				{
					const auto file = saveRetailHeroProfile(editor.draft,
						editor.editIndex < 0 ? std::string() : editor.profiles[editor.editIndex].file);
					if (file.empty())
					{
						editor.feedback = localize("APT:SaveFailed", "Could not save hero");
						break;
					}
					editor.profiles = loadRetailHeroProfiles();
					const auto selected = std::find_if(editor.profiles.begin(), editor.profiles.end(), [&](const auto &profile) {
						return !profile.preset && profile.file == file;
					});
					editor.selected = selected == editor.profiles.end() ? -1 : Int(selected - editor.profiles.begin());
					editor.listStart = std::max(0, editor.selected - editorListRows + 1);
					editor.feedback.clear();
					editor.page = Page::List;
				}
				else if (scroll || (click && (editorPowersUp.contains(x, y) || editorPowersDown.contains(x, y))))
				{
					const Int direction = scroll ? scroll : editorPowersUp.contains(x, y) ? -1 : 1;
					editor.powerStart = std::clamp(editor.powerStart + direction, 0,
						std::max(0, Int(editor.eligiblePowers().size()) - editorPowerRows));
				}
				else if (click)
				{
					if (editorResetPowers.contains(x, y))
					{
						editor.draft.powerButtons.clear();
						editor.rank = editor.powerStart = 0;
						editor.feedback.clear();
					}
					else if (editorRemovePower.contains(x, y))
					{
						if (editor.changePower(nullptr))
							editor.powerStart = 0;
					}
					else
					{
						for (Int i = 0; i < Int(CreateAHeroRules::MAX_POWER_COUNT); ++i)
							if (heroPowerRank(i).contains(x, y))
							{
								editor.rank = i;
								editor.powerStart = 0;
								return TRUE;
							}
						const auto choices = editor.eligiblePowers();
						for (Int i = 0; i < editorPowerRows && editor.powerStart + i < Int(choices.size()); ++i)
							if (heroPowerChoice(i).contains(x, y) && editor.changePower(choices[editor.powerStart + i]))
							{
								editor.rank = std::min(editor.rank + 1, Int(CreateAHeroRules::MAX_POWER_COUNT) - 1);
								editor.powerStart = 0;
								break;
							}
					}
				}
				break;
		}
		return TRUE;
	}

}

RetailHeroEditor::RetailHeroEditor() : m_data(std::make_unique<RetailHeroEditorData>()) {}
RetailHeroEditor::~RetailHeroEditor() = default;

Bool RetailHeroEditor::available()
{
	const auto &classes = CreateAHeroSystem::getClasses();
	return TheRetailCommandCatalog && std::any_of(classes.begin(), classes.end(), [](const auto &heroClass) {
		return !heroClass.subClasses.empty();
	});
}

void RetailHeroEditor::draw(const Painter &painter) const
{
	drawHeroEditor(*m_data, painter);
}

Bool RetailHeroEditor::input(UnsignedInt message, WindowMsgData point, Bool key, Real x, Real y)
{
	return heroEditorInput(*m_data, message, point, key, x, y);
}

Bool RetailHeroEditor::nameFocused() const
{
	return m_data->nameFocused;
}
