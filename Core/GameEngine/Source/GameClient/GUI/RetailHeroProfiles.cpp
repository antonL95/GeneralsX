#include "PreRTS.h"

#include "GameClient/RetailHeroProfiles.h"

#include "Common/FileSystem.h"
#include "Common/GlobalData.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "realcrc.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <random>

namespace
{
	namespace fs = std::filesystem;
	using Selection = CreateAHeroRules::SelectionData;
	constexpr std::string_view header = "ALAE2STR";
	constexpr size_t retailPowerSlots = 15;
	constexpr size_t maximumFileSize = 4096;

	uint32_t updateChecksum(uint32_t checksum, std::string_view bytes)
	{
		return uint32_t(CRC_Memory(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(), checksum));
	}

	class ProfileReader
	{
	public:
		explicit ProfileReader(std::string_view content) : m_content(content) {}

		std::string_view read(size_t size)
		{
			if (size > m_content.size())
			{
				m_valid = false;
				return {};
			}
			const auto value = m_content.substr(0, size);
			m_content.remove_prefix(size);
			return value;
		}

		uint32_t integer(size_t size)
		{
			uint32_t value = 0;
			unsigned shift = 0;
			for (unsigned char byte : read(size))
			{
				value |= uint32_t(byte) << shift;
				shift += 8;
			}
			return value;
		}

		std::string_view token() { return read(integer(1)); }
		uint32_t field(size_t size)
		{
			m_checksum = updateChecksum(m_checksum, m_content.substr(0, size));
			return integer(size);
		}
		std::string_view textField()
		{
			const auto value = token();
			nameField(value);
			return value;
		}
		void nameField(std::string_view name) { m_checksum = updateChecksum(m_checksum, name); }
		uint32_t checksum() const { return m_checksum; }
		bool finished() const { return m_valid && m_content.empty(); }

	private:
		std::string_view m_content;
		uint32_t m_checksum = 0;
		bool m_valid = true;
	};

	class ProfileWriter
	{
	public:
		void integer(uint32_t value, size_t size)
		{
			for (size_t i = 0; i < size; ++i)
			{
				bytes.push_back(char(value & 255));
				value >>= 8;
			}
		}
		void field(uint32_t value, size_t size)
		{
			const auto start = bytes.size();
			integer(value, size);
			nameField(std::string_view(bytes).substr(start));
		}
		void token(std::string_view value)
		{
			integer(value.size(), 1);
			bytes.append(value);
		}
		void textField(std::string_view value)
		{
			token(value);
			nameField(value);
		}
		void nameField(std::string_view value) { checksum = updateChecksum(checksum, value); }

		std::string bytes{header};
		uint32_t checksum = 0;
	};

	fs::path saveDirectory()
	{
		return fs::path(TheGlobalData->getPath_UserData().str()) / "Save";
	}

	std::vector<AsciiString> appearanceOptions(const CreateAHeroSubClassDef &type, const AsciiString &group)
	{
		std::vector<AsciiString> options;
		for (const auto &upgrade : type.blingUpgrades)
			if (const auto *bling = CreateAHeroSystem::findBlingByUpgrade(upgrade); bling && bling->groupName == group)
				options.push_back(upgrade);
		return options;
	}

	void selectBling(std::vector<std::string> &selected, const std::vector<AsciiString> &options, const AsciiString &upgrade)
	{
		std::erase_if(selected, [&](const std::string &value) {
			return std::any_of(options.begin(), options.end(), [&](const AsciiString &option) {
				return option == value.c_str();
			});
		});
		selected.emplace_back(upgrade.str());
	}

	bool readProfile(std::string_view bytes, bool preset, Selection &profile)
	{
		ProfileReader input(bytes);
		if (input.read(header.size()) != header || input.integer(4) != 1 || input.integer(4) != 0 || input.integer(1) != 8)
			return false;
		input.field(4);
		const auto nameSize = input.integer(1);
		if (!nameSize || nameSize > CreateAHeroRules::MAX_HERO_NAME_LENGTH)
			return false;
		std::string name;
		for (size_t i = 0; i < nameSize; ++i)
		{
			const auto ch = input.integer(2);
			if (ch > 127 || !CreateAHeroRules::isSafeTokenChar(char(ch)))
				return false;
			name.push_back(char(ch));
		}
		input.nameField(name);
		const auto classIndex = input.field(4);
		const auto typeIndex = input.field(4);
		input.field(4);
		input.field(4);
		const auto &classes = CreateAHeroSystem::getClasses();
		if (classIndex >= classes.size() || typeIndex >= classes[classIndex].subClasses.size())
			return false;
		const auto &heroClass = classes[classIndex];
		const auto &type = heroClass.subClasses[typeIndex];
		Selection picked;
		if (!CreateAHeroSystem::buildDefaultSelection(heroClass.upgradeName, type.upgradeName, picked))
			return false;
		picked.heroName = name;
		for (int *rgb : {picked.primaryColor, picked.secondaryColor, picked.tertiaryColor})
		{
			const auto packed = input.field(4);
			rgb[0] = (packed >> 16) & 255;
			rgb[1] = (packed >> 8) & 255;
			rgb[2] = packed & 255;
		}
		picked.powerButtons.resize(CreateAHeroRules::MAX_POWER_COUNT);
		for (size_t i = 0; i < retailPowerSlots; ++i)
		{
			const auto power = input.textField();
			const auto rank = input.field(4);
			input.field(4);
			if (power.empty() || rank >= picked.powerButtons.size())
				continue;
			auto &atRank = picked.powerButtons[rank];
			if (!atRank.empty())
				return false;
			atRank = power;
		}
		if (preset)
		{
			std::vector<std::string> earlier;
			for (size_t rank = 0; rank < picked.powerButtons.size(); ++rank)
			{
				auto &power = picked.powerButtons[rank];
				const auto *button = power.empty() ? nullptr : TheRetailCommandCatalog->findCommandButton(power.c_str());
				if (!CreateAHeroSystem::isPowerButtonLegalForSelection(button, picked, Int(rank + 1), earlier) ||
					std::find(earlier.begin(), earlier.end(), power) != earlier.end())
					power.clear();
				earlier.push_back(power);
			}
		}

		const auto count = input.field(4);
		if (count > CreateAHeroRules::MAX_BLING_COUNT + CreateAHeroRules::MAX_ATTRIBUTE_COUNT)
			return false;
		std::vector<std::string> groups;
		for (size_t i = 0; i < count; ++i)
		{
			const std::string group(input.textField());
			const auto index = input.field(4);
			if (group.empty() || std::find(groups.begin(), groups.end(), group) != groups.end())
				return false;
			groups.push_back(group);
			const auto attribute = std::find_if(type.attributes.begin(), type.attributes.end(),
				[&](const auto &field) { return field.groupName == group.c_str(); });
			if (attribute != type.attributes.end())
			{
				const auto options = CreateAHeroSystem::getGroupUpgrades(attribute->groupName);
				if (index < options.size())
					selectBling(picked.attributeUpgrades, options, options[index]);
			}
			else
			{
				const auto options = appearanceOptions(type, group.c_str());
				if (index < options.size())
					selectBling(picked.blingUpgrades, options, options[index]);
			}
		}
		input.token();
		input.field(1);
		const auto checksum = input.integer(4);
		if (!input.finished() || checksum != input.checksum())
			return false;
		if (CreateAHeroSystem::validateSelection(picked, {}, profile))
			return true;
		if (!preset)
			return false;
		picked.attributeUpgrades.clear();
		return CreateAHeroSystem::validateSelection(picked, {}, profile);
	}

	std::string writeProfile(const Selection &picked, std::string_view id)
	{
		const auto &classes = CreateAHeroSystem::getClasses();
		const auto heroClass = std::find_if(classes.begin(), classes.end(),
			[&](const auto &entry) { return entry.upgradeName == picked.classUpgrade.c_str(); });
		const auto type = std::find_if(heroClass->subClasses.begin(), heroClass->subClasses.end(),
			[&](const auto &entry) { return entry.upgradeName == picked.subClassUpgrade.c_str(); });
		ProfileWriter output;
		output.integer(1, 4);
		output.integer(0, 4);
		output.integer(8, 1);
		output.field(uint32_t(INVALID_ID), 4);
		output.integer(picked.heroName.size(), 1);
		for (unsigned char ch : picked.heroName)
			output.integer(ch, 2);
		output.nameField(picked.heroName);
		output.field(heroClass - classes.begin(), 4);
		output.field(type - heroClass->subClasses.begin(), 4);
		output.field(0, 4);
		output.field(0, 4);
		for (const int *rgb : {picked.primaryColor, picked.secondaryColor, picked.tertiaryColor})
			output.field(uint32_t(GameMakeColor(rgb[0], rgb[1], rgb[2], 255)), 4);
		std::vector<Int> powerGroups;
		Int nextGroup = 0;
		for (size_t i = 0; i < retailPowerSlots; ++i)
		{
			const std::string_view power = i < picked.powerButtons.size() ? picked.powerButtons[i] : std::string_view();
			Int group = 0;
			if (!power.empty())
			{
				const auto *button = TheRetailCommandCatalog->findCommandButton(power.data());
				const auto &prerequisite = button->getCreateAHeroUIPrerequisiteButtonName();
				const auto earlier = std::find(picked.powerButtons.begin(), picked.powerButtons.begin() + i, prerequisite.str());
				group = earlier == picked.powerButtons.begin() + i ? ++nextGroup : powerGroups[earlier - picked.powerButtons.begin()];
			}
			powerGroups.push_back(group);
			output.textField(power);
			output.field(power.empty() ? 0 : i, 4);
			output.field(group, 4);
		}
		output.field(picked.blingUpgrades.size() + picked.attributeUpgrades.size(), 4);
		auto writeBling = [&](const std::vector<std::string> &selected, bool attribute) {
			for (const auto &upgrade : selected)
			{
				const auto *bling = CreateAHeroSystem::findBlingByUpgrade(upgrade.c_str());
				const auto options = attribute ? CreateAHeroSystem::getGroupUpgrades(bling->groupName) :
					appearanceOptions(*type, bling->groupName);
				output.textField(bling->groupName.str());
				output.field(std::find(options.begin(), options.end(), AsciiString(upgrade.c_str())) - options.begin(), 4);
			}
		};
		writeBling(picked.blingUpgrades, false);
		writeBling(picked.attributeUpgrades, true);
		output.token(id);
		output.field(0, 1);
		output.integer(output.checksum, 4);
		return std::move(output.bytes);
	}
}

std::vector<RetailHeroProfile> loadRetailHeroProfiles()
{
	std::vector<RetailHeroProfile> profiles;
	if (!CreateAHeroSystem::isLoaded() || !TheRetailCommandCatalog)
		return profiles;

	FilenameList files;
	TheFileSystem->getFileListInDirectory("Data/SystemHeroes", "myhero*.cah", files, FALSE);
	for (const auto &path : files)
	{
		File *file = TheFileSystem->openFile(path.str(), File::READ | File::BINARY);
		if (!file)
			continue;
		const Int size = file->size();
		std::string bytes(size > 0 && size <= Int(maximumFileSize) ? size : 0, '\0');
		const auto read = bytes.empty() ? 0 : file->read(bytes.data(), Int(bytes.size()));
		file->close();
		Selection profile;
		if (read > 0 && read == Int(bytes.size()) && readProfile(bytes, true, profile))
			profiles.push_back({std::move(profile), path.str(), true});
	}
	std::error_code error;
	for (fs::directory_iterator file(saveDirectory(), error), end; !error && file != end; file.increment(error))
	{
		const auto path = file->path();
		AsciiString filename(path.filename().string().c_str());
		filename.toLower();
		if (!filename.startsWith("myhero_") || !filename.endsWith(".cah") || filename.getLength() <= 11 ||
			!file->is_regular_file(error))
			continue;
		const auto size = file->file_size(error);
		if (error)
		{
			error.clear();
			continue;
		}
		if (!size || size > maximumFileSize)
			continue;
		std::string bytes(size, '\0');
		std::ifstream input(path, std::ios::binary);
		Selection profile;
		if (input.read(bytes.data(), bytes.size()) && readProfile(bytes, false, profile))
			profiles.push_back({std::move(profile), path.filename().string(), false});
	}
	std::stable_sort(profiles.begin(), profiles.end(), [](const auto &a, const auto &b) {
		const AsciiString name(a.selection.heroName.c_str());
		const Int order = name.compareNoCase(b.selection.heroName.c_str());
		return order ? order < 0 : a.file < b.file;
	});
	return profiles;
}

std::string saveRetailHeroProfile(const Selection &selection, const std::string &file)
{
	Selection profile;
	if (!CreateAHeroRules::isCanonicalToken(selection.heroName, CreateAHeroRules::MAX_HERO_NAME_LENGTH) ||
		!CreateAHeroSystem::validateSelection(selection, {}, profile))
		return {};

	std::error_code error;
	const auto directory = saveDirectory();
	fs::create_directories(directory, error);
	if (error)
		return {};

	std::string filename = file;
	if (filename.empty())
	{
		std::random_device random;
		AsciiString id;
		id.format("%08X%08X%07X", unsigned(random()), unsigned(random()), unsigned(random()) & 0x0FFFFFFFu);
		filename = std::string("MyHero_") + id.str() + ".cah";
	}
	const auto target = directory / filename;
	const auto temporary = directory / (filename + ".tmp");
	const auto bytes = writeProfile(profile, std::string_view(filename).substr(7, filename.size() - 11));
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), bytes.size());
	output.close();
	if (output)
	{
		fs::rename(temporary, target, error);
		if (!error)
			return filename;
	}
	fs::remove(temporary, error);
	return {};
}

bool deleteRetailHeroProfile(const RetailHeroProfile &profile)
{
	std::error_code error;
	return !profile.preset && fs::remove(saveDirectory() / profile.file, error);
}
