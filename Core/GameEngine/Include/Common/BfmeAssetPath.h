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
