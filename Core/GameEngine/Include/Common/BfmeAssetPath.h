#pragma once

#include <string>
#include <string_view>

inline std::string makeBfmeAssetPath(std::string_view directory, std::string_view filename)
{
	if (filename.size() < 2 || filename.rfind('.') < 2 || filename.substr(0, 2) == ".." ||
		filename.find_first_of("/\\") != std::string_view::npos)
		return {};

	std::string path(directory);
	path.append(filename.substr(0, 2));
	path.push_back('/');
	path.append(filename);
	return path;
}

inline std::string makeBfmeJpegPath(std::string_view filename)
{
	if (filename.find_first_of("/\\") != std::string_view::npos)
		return std::string(filename);

	const auto stem = filename.substr(0, filename.rfind('.'));
	const auto first = stem.find_first_not_of(' ');
	const auto last = stem.find_last_not_of(' ');
	if (first == std::string_view::npos || first == last || stem.starts_with("apt_"))
		return "Art/Textures/" + std::string(filename);
	return makeBfmeAssetPath("Art/CompiledTextures/", filename);
}
