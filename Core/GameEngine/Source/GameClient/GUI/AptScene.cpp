#include "PreRTS.h"

#include "GameClient/AptScene.h"

#include "Common/File.h"
#include "Common/FileSystem.h"
#include "GameClient/Display.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
	using Box = AptScene::Box;
	using Bytes = std::vector<unsigned char>;

	Bytes readAsset(const std::string &path)
	{
		File *file = TheFileSystem->openFile(path.c_str(), File::READ | File::BINARY);
		if (!file)
			return {};

		const Int size = file->size();
		Bytes bytes;
		if (size > 0 && size <= 8 * 1024 * 1024)
		{
			bytes.resize(size);
			if (file->read(bytes.data(), size) != size)
				bytes.clear();
		}
		file->close();
		return bytes;
	}

	struct Reader
	{
		Bytes bytes;

		void check(size_t pos, size_t length) const
		{
			if (pos > bytes.size() || length > bytes.size() - pos)
				throw std::runtime_error("APT offset outside file");
		}

		uint32_t number(size_t pos) const
		{
			check(pos, 4);
			return uint32_t(bytes[pos]) | uint32_t(bytes[pos + 1]) << 8 |
				uint32_t(bytes[pos + 2]) << 16 | uint32_t(bytes[pos + 3]) << 24;
		}

		Real real(size_t pos) const
		{
			const Real value = std::bit_cast<float>(number(pos));
			if (!std::isfinite(value) || std::abs(value) > 1000000.0f)
				throw std::runtime_error("invalid APT coordinate");
			return value;
		}

		std::string string(size_t pos) const
		{
			const size_t start = number(pos);
			if (!start)
				return {};

			check(start, 1);
			const auto first = bytes.begin() + start;
			const auto limit = first + std::min<size_t>(bytes.size() - start, 4096);
			const auto end = std::find(first, limit, 0);
			if (end == limit)
				throw std::runtime_error("unterminated APT string");
			return std::string(first, end);
		}

		std::pair<size_t, size_t> list(size_t pos, size_t stride) const
		{
			const size_t count = number(pos);
			const size_t offset = number(pos + 4);
			if (count > 16384)
				throw std::runtime_error("APT list too long");
			if (!count)
				return {0, 0};
			check(offset, count * stride);
			return {count, offset};
		}
	};

	struct Transform
	{
		Real a = 1, b = 0, c = 0, d = 1, x = 0, y = 0;

		Coord2D apply(Real px, Real py) const
		{
			return {a * px + c * py + x, b * px + d * py + y};
		}

		Transform compose(const Transform &child) const
		{
			const Coord2D origin = apply(child.x, child.y);
			return {a * child.a + c * child.b, b * child.a + d * child.b,
				a * child.c + c * child.d, b * child.c + d * child.d, origin.x, origin.y};
		}
	};

	Transform matrix(const Reader &r, size_t p)
	{
		return {r.real(p), r.real(p + 4), r.real(p + 8), r.real(p + 12), r.real(p + 16), r.real(p + 20)};
	}

	Box transformBox(const Reader &r, size_t p, const Transform &m)
	{
		const Real l = r.real(p), t = r.real(p + 4), right = r.real(p + 8), b = r.real(p + 12);
		const Coord2D v[] = {m.apply(l, t), m.apply(l, b), m.apply(right, t), m.apply(right, b)};
		Box box{v[0].x, v[0].y, v[0].x, v[0].y};
		for (const Coord2D &point : v)
		{
			box.left = std::min(box.left, point.x);
			box.top = std::min(box.top, point.y);
			box.right = std::max(box.right, point.x);
			box.bottom = std::max(box.bottom, point.y);
		}
		return box;
	}

	void include(Box &box, const Box &child)
	{
		if (!child.valid())
			return;
		if (!box.valid())
			box = child;
		else
			box = {std::min(box.left, child.left), std::min(box.top, child.top),
				std::max(box.right, child.right), std::max(box.bottom, child.bottom)};
	}

	struct Tint
	{
		std::array<Real, 4> multiply{1, 1, 1, 1};
		std::array<Real, 4> add{};

		Tint compose(uint32_t rgba, uint32_t extra) const
		{
			Tint result;
			for (Int i = 0; i < 4; ++i)
			{
				result.multiply[i] = multiply[i] * ((rgba >> (i * 8)) & 255) / 255.0f;
				result.add[i] = add[i] + multiply[i] * ((extra >> (i * 8)) & 255);
			}
			return result;
		}

		Color color(Real r, Real g, Real b, Real a) const
		{
			auto channel = [&](Int i, Real value) {
				return static_cast<Int>(std::clamp(value * multiply[i] + add[i], 0.0f, 255.0f));
			};
			return GameMakeColor(channel(0, r), channel(1, g), channel(2, b), channel(3, a));
		}
	};

	struct Placement
	{
		uint32_t character = UINT32_MAX;
		Transform transform;
		uint32_t rgba = UINT32_MAX, extra = 0, clipDepth = 0;
		std::optional<uint32_t> rgb;
		std::string name;
	};

	struct Geometry
	{
		std::string texture;
		std::array<Real, 4> rgba{};
		Transform uv;
		std::vector<Coord2D> points;
		Real lineWidth = 0;
	};

	struct Movie
	{
		Reader apt, constants;
		std::string name, prefix;
		size_t root = 0;
		std::vector<size_t> characters;
		std::map<uint32_t, std::pair<std::string, std::string>> imports;
		std::map<std::string, uint32_t> exports;
		std::map<Int, Int> atlasPages;
		std::map<uint32_t, std::vector<Geometry>> geometry;

		std::string poolString(size_t pool, uint32_t index) const
		{
			const auto [count, table] = apt.list(pool, 4);
			if (index >= count)
				return {};
			const uint32_t entry = apt.number(table + 4 * index);
			if (entry >= constants.number(24))
				return {};
			const size_t offset = 32 + size_t(entry) * 8;
			return constants.number(offset) == 1 ? constants.string(offset + 4) : std::string{};
		}
	};

	using FrameColors = std::map<std::string, uint32_t>;

	std::optional<uint32_t> parseRgb(std::string_view text)
	{
		if (text.starts_with("0x") || text.starts_with("0X"))
			text.remove_prefix(2);
		if (text.size() != 6)
			return {};
		uint32_t rgb = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), rgb, 16);
		if (error != std::errc{} || end != text.data() + text.size())
			return {};
		return ((rgb >> 16) & 255) | (rgb & 0xFF00) | ((rgb & 255) << 16);
	}

	FrameColors readFrameColors(const Movie &movie) try
	{
		const Reader &r = movie.apt;
		const auto [count, frames] = r.list(movie.root + 8, 8);
		if (!count)
			return {};
		const auto [items, table] = r.list(frames, 4);
		for (size_t i = 0; i < items; ++i)
		{
			const size_t item = r.number(table + 4 * i);
			if (!item || r.number(item) != 1)
				continue;
			const size_t start = r.number(item + 4);
			if (start >= r.bytes.size() || r.bytes[start] != 0x88)
				continue;
			const size_t pool = (start + 4) & ~size_t(3);
			size_t body = pool + 8;
			while (body < r.bytes.size() && r.bytes[body] == 0x9B)
			{
				const size_t header = (body + 4) & ~size_t(3);
				const size_t length = r.number(header + 12);
				body = header + 24;
				r.check(body, length);
				body += length;
			}

			const size_t limit = std::min(r.bytes.size(), body + 16384);
			FrameColors colors;
			for (size_t at = body; at < limit; )
			{
				const auto opcode = r.bytes[at];
				if (limit - at >= 5 && opcode == 0xA2 && r.bytes[at + 4] == 0x1D)
				{
					const auto variable = movie.poolString(pool, r.bytes[at + 1]);
					const auto value = movie.poolString(pool, r.bytes[at + 3]);
					if (r.bytes[at + 2] == 0xA2)
					{
						if (const auto rgb = parseRgb(value))
							colors[variable] = *rgb;
						at += 5;
						continue;
					}
					if (r.bytes[at + 2] == 0xAE)
					{
						if (const auto source = colors.find(value); source != colors.end())
							colors[variable] = source->second;
						at += 5;
						continue;
					}
				}
				switch (opcode)
				{
					case 0xA2: case 0xAE: case 0xAF: case 0xB2: at += 2; break;
					case 0x1D: case 0x47: case 0x4F: case 0x5A: ++at; break;
					default: return colors;
				}
			}
			return colors;
		}
		return {};
	}
	catch (const std::runtime_error &)
	{
		return {};
	}

	std::optional<uint32_t> readClipRgb(const Movie &movie, size_t events, const FrameColors &colors) try
	{
		if (!events || colors.empty())
			return {};
		const Reader &r = movie.apt;
		const auto [count, table] = r.list(events, 12);
		if (count > 32)
			return {};
		for (size_t i = 0; i < count; ++i)
		{
			const size_t event = table + 12 * i;
			if ((r.number(event) & 0xFFFFFF) != 1)
				continue;
			const size_t start = r.number(event + 8);
			if (start >= r.bytes.size() || r.bytes[start] != 0x88)
				continue;
			const size_t pool = (start + 4) & ~size_t(3);
			const size_t body = pool + 8;
			constexpr std::array<int, 21> pattern{
				0xA2, -1, 0x70, 0x5A, 0xA2, -1, 0x40, 0x1D, 0xA2, -1, 0xAE, -1, 0xAF, -1,
				0x47, 0x5A, 0xAE, -1, 0xB2, -1, 0x00
			};
			if (body > r.bytes.size() || pattern.size() > r.bytes.size() - body ||
				!std::equal(pattern.begin(), pattern.end(), r.bytes.begin() + body,
					[](int expected, unsigned char actual) { return expected < 0 || expected == actual; }))
				continue;
			auto name = [&](size_t offset) { return movie.poolString(pool, r.bytes[body + offset]); };
			if (name(1) != name(17) || name(5) != "Color" || name(9) != "0x" ||
				name(11) != "_root" || name(19) != "setRGB")
				continue;
			if (const auto rgb = colors.find(name(13)); rgb != colors.end())
				return rgb->second;
		}
		return {};
	}
	catch (const std::runtime_error &)
	{
		return {};
	}

	bool safeMovieName(const std::string &name)
	{
		return !name.empty() && name.size() <= 64 && name.find_first_not_of(
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
	}

	std::string lowerName(std::string name)
	{
		for (char &ch : name)
			if (ch >= 'A' && ch <= 'Z')
				ch += 'a' - 'A';
		return name;
	}
}

class AptScene::Data
{
public:
	struct Art
	{
		std::string path, texture;
		Color color;
		std::vector<Coord2D> points, uv;
	};

	struct Label
	{
		std::string path;
		Text text;
	};

	std::map<std::string, Movie> movies;
	std::map<std::string, Box> boxes;
	std::map<std::string, Coord2D> positions;
	std::map<std::string, std::string> nestedFrames;
	FrameColors frameColors;
	std::vector<Art> art;
	std::vector<std::vector<Art>> frames;
	std::vector<Label> labels;
	Int width = 0, height = 0;
	size_t visits = 0;
	size_t vertices = 0;

	Movie *load(const std::string &name)
	{
		if (!safeMovieName(name) || movies.size() >= 48)
			return nullptr;
		const std::string key = lowerName(name);
		auto [entry, inserted] = movies.try_emplace(key);
		Movie &movie = entry->second;
		if (!inserted)
			return movie.root ? &movie : nullptr;

		movie.name = name;
		for (const char *prefix : {"", "apt/"})
		{
			const std::string base = std::string(prefix) + name;
			movie.apt.bytes = readAsset(base + ".apt");
			if (movie.apt.bytes.empty())
				continue;
			movie.constants.bytes = readAsset(base + ".const");
			const std::string_view aptHeader(reinterpret_cast<const char *>(movie.apt.bytes.data()), movie.apt.bytes.size());
			if (!aptHeader.starts_with("Apt Data:") || movie.constants.bytes.size() < 32)
				return nullptr;

			const Reader &r = movie.apt;
			const size_t root = movie.constants.number(20);
			if (r.number(root) != 9 || r.number(root + 4) != 0x09876543)
				return nullptr;
			movie.prefix = prefix;
			auto [count, table] = r.list(root + 20, 4);
			for (size_t i = 0; i < count; ++i)
				movie.characters.push_back(r.number(table + 4 * i));
			auto [imports, importTable] = r.list(root + 40, 16);
			for (size_t i = 0; i < imports; ++i)
			{
				const size_t p = importTable + 16 * i;
				movie.imports.emplace(r.number(p + 8), std::pair{r.string(p), r.string(p + 4)});
			}
			auto [exports, exportTable] = r.list(root + 48, 8);
			for (size_t i = 0; i < exports; ++i)
			{
				const size_t p = exportTable + 8 * i;
				movie.exports.emplace(r.string(p), r.number(p + 4));
			}

			const Bytes dat = readAsset(base + ".dat");
			std::istringstream lines(std::string(dat.begin(), dat.end()));
			for (std::string line; std::getline(lines, line); )
			{
				const auto arrow = line.find("->");
				Int image;
				if (arrow != std::string::npos)
				{
					Int page;
					if (std::istringstream(line.substr(0, arrow)) >> image &&
						std::istringstream(line.substr(arrow + 2)) >> page && image >= 0 && page > 0)
						movie.atlasPages.emplace(image, page);
				}
				else if (const auto equals = line.find('='); equals != std::string::npos)
				{
					Int left, top, right, bottom;
					if (std::istringstream(line.substr(0, equals)) >> image && image > 0 &&
						std::istringstream(line.substr(equals + 1)) >> left >> top >> right >> bottom)
						movie.atlasPages.emplace(image, image);
				}
			}
			movie.root = root;
			return &movie;
		}
		return nullptr;
	}

	const std::vector<Geometry> &getGeometry(Movie &movie, uint32_t id)
	{
		auto [entry, inserted] = movie.geometry.try_emplace(id);
		auto &shapes = entry->second;
		if (!inserted)
			return shapes;

		const Bytes bytes = readAsset(movie.prefix + movie.name + "_geometry/" + std::to_string(id) + ".ru");
		std::istringstream lines(std::string(bytes.begin(), bytes.end()));
		Geometry *shape = nullptr;
		for (std::string line; std::getline(lines, line); )
		{
			std::replace(line.begin(), line.end(), ':', ' ');
			std::istringstream values(line);
			values.imbue(std::locale::classic());
			char mode = 0;
			values >> mode;
			if (mode == 's')
			{
				shape = nullptr;
				Geometry geometry;
				std::string kind;
				values >> kind;
				if (kind == "l")
					values >> geometry.lineWidth;
				values >> geometry.rgba[0] >> geometry.rgba[1] >> geometry.rgba[2] >> geometry.rgba[3];
				if (kind == "tc")
				{
					Int image = -1;
					auto &uv = geometry.uv;
					values >> image >> uv.a >> uv.b >> uv.c >> uv.d >> uv.x >> uv.y;
					const auto page = movie.atlasPages.find(image);
					if (page == movie.atlasPages.end())
						continue;
					geometry.texture = "apt_" + lowerName(movie.name) + "_" + std::to_string(page->second) + ".tga";
				}
				if (values && (kind == "s" || kind == "tc" || kind == "l"))
				{
					shapes.push_back(std::move(geometry));
					shape = &shapes.back();
				}
			}
			else if (mode == 'c')
				shape = nullptr;
			else if (mode == 't' && shape && shape->points.size() < 12000)
			{
				Coord2D v[3];
				if (values >> v[0].x >> v[0].y >> v[1].x >> v[1].y >> v[2].x >> v[2].y)
					shape->points.insert(shape->points.end(), v, v + 3);
			}
			else if (mode == 'l' && shape && shape->lineWidth > 0 && shape->points.size() < 12000)
			{
				Coord2D a, b;
				if (!(values >> a.x >> a.y >> b.x >> b.y))
					continue;
				const Real length = std::hypot(b.x - a.x, b.y - a.y);
				if (!(length > 0) || !std::isfinite(length))
					continue;
				const Real scale = shape->lineWidth / (length * 2);
				const Real x = (a.y - b.y) * scale, y = (b.x - a.x) * scale;
				const Coord2D v[] = {{a.x + x, a.y + y}, {a.x - x, a.y - y}, {b.x + x, b.y + y},
					{b.x + x, b.y + y}, {a.x - x, a.y - y}, {b.x - x, b.y - y}};
				shape->points.insert(shape->points.end(), v, v + 6);
			}
		}
		return shapes;
	}

	Box emit(Movie &movie, size_t index, const Transform &m, const Tint &tint, const std::string &path, Int depth)
	{
		if (depth > 18 || ++visits > 16000 || index >= movie.characters.size())
			return {};
		const Reader &r = movie.apt;
		positions.emplace(path, Coord2D{m.x, m.y});
		const size_t p = movie.characters[index];
		if (!p)
		{
			const auto entry = movie.imports.find(index);
			if (entry == movie.imports.end())
				return {};
			Movie *owner = load(entry->second.first);
			if (!owner)
				return {};
			const auto exportEntry = owner->exports.find(entry->second.second);
			if (exportEntry == owner->exports.end())
				return {};
			return emit(*owner, exportEntry->second, m, tint, path, depth + 1);
		}
		if (r.number(p + 4) != 0x09876543)
			return {};

		Box box;
		switch (r.number(p))
		{
			case 1:
				box = transformBox(r, p + 8, m);
				for (const Geometry &shape : getGeometry(movie, r.number(p + 24)))
				{
					if (shape.points.empty() || vertices + shape.points.size() > 200000)
						continue;
					vertices += shape.points.size();
					Art part{path, shape.texture, tint.color(shape.rgba[0], shape.rgba[1], shape.rgba[2], shape.rgba[3]), {}, {}};
					for (const auto &point : shape.points)
					{
						part.points.push_back(m.apply(point.x, point.y));
						part.uv.push_back(shape.uv.apply(point.x, point.y));
					}
					art.push_back(std::move(part));
				}
				break;
			case 2:
			{
				box = transformBox(r, p + 8, m);
				const size_t font = r.number(p + 24);
				std::string fontName = "Albertus MT";
				if (font < movie.characters.size() && movie.characters[font] && r.number(movie.characters[font]) == 3)
					fontName = r.string(movie.characters[font] + 8);
				labels.push_back({path, Text{box, r.string(p + 52), fontName,
					r.real(p + 36) * std::abs(m.d), Int(r.number(p + 28))}});
				break;
			}
			case 4:
			{
				box = transformBox(r, p + 12, m);
				const auto [count, table] = r.list(p + 44, 68);
				for (size_t i = 0; i < count; ++i)
				{
					const size_t q = table + 68 * i;
					if (r.number(q) & 1)
						emit(movie, r.number(q + 4), m.compose(matrix(r, q + 12)), tint,
							path + ".up", depth + 1);
				}
				break;
			}
			case 5:
			{
				const auto target = nestedFrames.find(path);
				box = timeline(movie, p + 8, m, tint, path, depth,
					target == nestedFrames.end() ? nullptr : target->second.c_str());
				break;
			}
			case 10:
				box = transformBox(r, p + 8, m);
				break;
		}

		if (box.valid())
			include(boxes[path], box);
		return box;
	}

	Box timeline(Movie &movie, size_t p, const Transform &m, const Tint &tint,
		const std::string &path, Int depth, const char *target, size_t frameLimit = 0)
	{
		const Reader &r = movie.apt;
		const auto [count, table] = r.list(p, 8);
		size_t opening = count;
		size_t endLabel = 0;
		size_t rest = count ? 1 : 0;
		std::map<uint32_t, Placement> placements;
		for (size_t frame = 0; !frameLimit && frame < count; ++frame)
		{
			bool stop = false;
			const auto [items, itemTable] = r.list(table + 8 * frame, 4);
			for (size_t i = 0; i < items; ++i)
			{
				const size_t q = r.number(itemTable + 4 * i);
				if (!q)
					continue;

				const uint32_t type = r.number(q);
				if (type == 2)
				{
					const std::string name = lowerName(r.string(q + 4));
					if (name == "end")
						endLabel = frame + 1;
					if ((target && name == target) || (!target && opening == count &&
						(name == "_open" || name == "_up" || name == "_show" || name == "_fadein" || name == "_in")))
						opening = frame;
				}
				else if (type == 1)
				{
					const size_t action = r.number(q + 4);
					stop = stop || (action < r.bytes.size() && r.bytes[action] == 7);
				}
			}
			if (frame >= opening)
			{
				rest = frame + 1;
				if (stop)
					break;
			}
		}
		if (frameLimit)
			rest = std::min(count, frameLimit);
		else if (target && target[0] == '#')
			rest = std::min(count, size_t(std::strtoul(target + 1, nullptr, 10)));
		else if (target && opening == count)
			rest = count;
		else if (opening == count && endLabel)
			rest = endLabel;

		for (size_t frame = 0; frame < rest; ++frame)
		{
			const auto [items, itemTable] = r.list(table + 8 * frame, 4);
			for (size_t i = 0; i < items; ++i)
			{
				const size_t q = r.number(itemTable + 4 * i);
				if (!q)
					continue;

				switch (r.number(q))
				{
					case 3:
					{
						const uint32_t flags = r.number(q + 4);
						auto &placement = placements[r.number(q + 8)];
						if (!(flags & 1))
							placement = {};
						if (flags & 2)
							placement.character = r.number(q + 12);
						if (flags & 4)
							placement.transform = matrix(r, q + 16);
						if (flags & 8)
						{
							placement.rgba = r.number(q + 40);
							placement.extra = r.number(q + 44);
						}
						if (flags & 32)
							placement.name = r.string(q + 52);
						if (flags & 64)
							placement.clipDepth = r.number(q + 56);
						if ((flags & 128) && !frameColors.empty())
							placement.rgb = readClipRgb(movie, r.number(q + 60), frameColors);
						break;
					}
					case 4:
						placements.erase(r.number(q + 4));
						break;
				}
			}
		}

		Box result;
		for (const auto &[layer, placement] : placements)
		{
			if (placement.clipDepth)
				continue;
			const std::string name = placement.name.empty() ? "@" + std::to_string(layer) : placement.name;
			const Tint color = placement.rgb ?
				tint.compose(placement.rgba & 0xFF000000, (placement.extra & 0xFF000000) | *placement.rgb) :
				tint.compose(placement.rgba, placement.extra);
			include(result, emit(movie, placement.character, m.compose(placement.transform),
				color, path + "." + name, depth + 1));
		}
		return result;
	}
};

AptScene::AptScene(const char *movie, const char *frame,
	std::initializer_list<std::pair<const char*, const char*>> nestedFrames)
{
	try
	{
		auto data = std::make_unique<Data>();
		for (const auto& [path, target] : nestedFrames)
			data->nestedFrames.emplace(path, target);
		Movie *root = data->load(movie);
		if (!root)
			return;

		data->width = root->apt.number(root->root + 28);
		data->height = root->apt.number(root->root + 32);
		if (data->width <= 0 || data->width > 8192 || data->height <= 0 || data->height > 8192)
			return;

		data->frameColors = readFrameColors(*root);
		data->timeline(*root, root->root + 8, {}, {}, "_root", 0, frame);
		if (!data->art.empty())
			m_data = std::move(data);
	}
	catch (const std::runtime_error &)
	{
		m_data.reset();
	}
}

AptScene::~AptScene() = default;

AptScene::AptScene(const char *movie, AnimatedExport exported)
{
	try
	{
		auto data = std::make_unique<Data>();
		Movie *root = data->load(movie);
		if (!root)
			return;
		const auto entry = root->exports.find(exported.name);
		if (entry == root->exports.end() || entry->second >= root->characters.size())
			return;
		const size_t character = root->characters[entry->second];
		if (!character || root->apt.number(character) != 5 || root->apt.number(character + 4) != 0x09876543)
			return;
		const size_t count = root->apt.list(character + 8, 8).first;
		if (!count || count > 120)
			return;

		Bool hasArt = FALSE;
		for (size_t frame = 1; frame <= count; ++frame)
		{
			data->timeline(*root, character + 8, {}, {}, exported.name, 0, nullptr, frame);
			hasArt = hasArt || !data->art.empty();
			data->frames.emplace_back(std::move(data->art));
			data->art.clear();
		}
		if (hasArt)
		{
			data->width = root->apt.number(root->root + 28);
			data->height = root->apt.number(root->root + 32);
			if (data->width > 0 && data->height > 0)
				m_data = std::move(data);
		}
	}
	catch (const std::runtime_error &)
	{
		m_data.reset();
	}
}

Int AptScene::frameCount() const
{
	return m_data ? static_cast<Int>(m_data->frames.size()) : 0;
}

void AptScene::drawFrameAt(Int frame, Real x, Real y, Real sx, Real sy) const
{
	if (frame >= 0 && frame < frameCount())
		drawArt({}, {}, x, y, sx, sy, 1.0f, frame);
}

Bool AptScene::Box::contains(Real x, Real y) const
{
	return x >= left && x < right && y >= top && y < bottom;
}

Bool AptScene::Box::valid() const
{
	return right > left && bottom > top;
}

Bool AptScene::valid() const
{
	return static_cast<Bool>(m_data != nullptr);
}

AptScene::Box AptScene::bounds(const std::string &path) const
{
	if (!m_data)
		return {};
	const auto entry = m_data->boxes.find(path);
	return entry == m_data->boxes.end() ? Box{} : entry->second;
}

Coord2D AptScene::position(const std::string &path, Coord2D fallback) const
{
	if (!m_data)
		return fallback;
	const auto entry = m_data->positions.find(path);
	return entry == m_data->positions.end() ? fallback : entry->second;
}

Real AptScene::xScale() const
{
	return m_data ? Real(TheDisplay->getWidth()) / m_data->width : 1.0f;
}

Real AptScene::yScale() const
{
	return m_data ? Real(TheDisplay->getHeight()) / m_data->height : 1.0f;
}

void AptScene::draw(const Visibility &visible, const DrawText &text, const HorizontalScale &scale) const
{
	if (!m_data)
		return;

	drawArt(visible, scale, 0, 0, xScale(), yScale(), 1.0f);
	for (const auto &label : m_data->labels)
		if (visible(label.path))
			text(label.path, label.text);
}

void AptScene::drawAt(const Visibility &visible, Real x, Real y, Real sx, Real sy, Real opacity) const
{
	if (m_data)
		drawArt(visible, {}, x, y, sx, sy, opacity);
}

void AptScene::drawArt(const Visibility &visible, const HorizontalScale &scale, Real x, Real y, Real sx, Real sy, Real opacity, Int frame) const
{
	std::vector<Coord2D> points;
	const auto &parts = frame < 0 ? m_data->art : m_data->frames[frame];
	for (const auto &art : parts)
	{
		if (visible && !visible(art.path))
			continue;

		const Real stretch = scale ? scale(art.path) : 1.0f;
		if (stretch <= 0)
			continue;

		points.clear();
		const Real origin = stretch != 1.0f ? bounds(art.path).left : 0;
		for (const Coord2D &point : art.points)
			points.push_back({x + (origin + (point.x - origin) * stretch) * sx, y + point.y * sy});
		const Color color = (art.color & 0x00FFFFFF) |
			(static_cast<UnsignedInt>(((art.color >> 24) & 255) * opacity) << 24);
		TheDisplay->drawImageTriangles(art.texture.empty() ? nullptr : art.texture.c_str(), points.data(),
			art.uv.data(), static_cast<Int>(points.size()), color);
	}
}
