/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
** SDL3Mouse.cpp
**
** SDL3-based mouse implementation for Linux builds.
**
** TheSuperHackers @feature CnC_Generals_Linux 10/02/2026 Bender
** Replaces Win32Mouse/Win32DIMouse with SDL3 mouse APIs for Linux.
*/

#ifndef _WIN32

// GeneralsX @bugfix BenderAI 13/02/2026 Fix include path (fighter19 pattern)
#include "SDL3Device/GameClient/SDL3Mouse.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// GeneralsX @bugfix felipebraz 18/02/2026 Include GameLogic for frame tracking
#include "GameLogic/GameLogic.h"
// GeneralsX @bugfix felipebraz 20/02/2026 Include Display to get internal resolution for coordinate scaling
#include "GameClient/Display.h"
#include "GameClient/InGameUI.h"
#include "GameClient/UiPixelSpace.h"
// GeneralsX @bugfix BenderAI 22/02/2026 Add SDL3_image for cursor loading
// SDL3_image now finds system libpng via pkg-config (CMAKE_PREFIX_PATH reordered in cmake/sdl3.cmake)
#include <SDL3_image/SDL_image.h>
// GeneralsX @bugfix BenderAI 22/02/2026 Add array header for AnimatedCursor
#include <algorithm>
#include <array>
#include <memory>
#include <string>
// GeneralsX @bugfix BenderAI 22/02/2026 Add file system includes for cursor loading
#include "Common/Debug.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/RetailFileLayers.h"

/**
 * AnimatedCursor - Helper struct for cursor animation (fighter19 pattern)
 * Holds the frames of an animated cursor with frame timing info
 */
struct AnimatedCursor {
	SDL_Cursor* m_cursor = nullptr;
	std::array<SDL_Surface*, MAX_2D_CURSOR_ANIM_FRAMES> m_frameSurfaces{};
	int m_frameCount = 0;
	int m_frameRate = 0; // the time a frame is displayed in 1/60th of a second

	~AnimatedCursor()
	{
		if (m_cursor)
		{
			SDL_DestroyCursor(m_cursor);
			m_cursor = nullptr;
		}

		for (int i = 0; i < MAX_2D_CURSOR_ANIM_FRAMES; i++)
		{
			if (m_frameSurfaces[i])
			{
				SDL_DestroySurface(m_frameSurfaces[i]);
				m_frameSurfaces[i] = nullptr;
			}
		}
	}
};

// Global cursor resources array - fighter19 pattern
// GeneralsX @bugfix BenderAI 22/02/2026 Global cursor resource cache
AnimatedCursor* cursorResources[Mouse::NUM_MOUSE_CURSORS][MAX_2D_CURSOR_DIRECTIONS];

// RIFF/ANI parsing helpers (fighter19 pattern)
// GeneralsX @bugfix BenderAI 22/02/2026 RIFF format parsing for cursor loading
typedef std::array<char, 4> FourCC;
constexpr FourCC riff_id = {'R', 'I', 'F', 'F'};
constexpr FourCC acon_id = {'A', 'C', 'O', 'N'};
constexpr FourCC anih_id = {'a', 'n', 'i', 'h'};
constexpr FourCC fram_id = {'f', 'r', 'a', 'm'};
constexpr FourCC icon_id = {'i', 'c', 'o', 'n'};
constexpr FourCC seq_id = {'s', 'e', 'q', ' '};
constexpr FourCC rate_id = {'r', 'a', 't', 'e'};
constexpr FourCC list_id = {'L', 'I', 'S', 'T'};

struct ANIHeader
{
	uint32_t size; // Should be 32 bytes (all fields below)
	uint32_t frames;
	uint32_t steps;
	uint32_t width;
	uint32_t height;
	uint32_t bitsPerPixel;
	uint32_t planes;
	uint32_t displayRate;
	uint32_t flags;
};

struct RIFFChunk
{
	FourCC id; // Should be 'RIFF' for the first 4 bytes
	uint32_t size; // Size of the file minus 8 bytes
	FourCC type; // Should be 'ACON' in the first chunk
};

static_assert(sizeof(ANIHeader) == 36, "ANIHeader size is not 36 bytes");

static RIFFChunk* getNextChunk(RIFFChunk* chunk)
{
	return (RIFFChunk*)((char*)chunk + sizeof(RIFFChunk) + chunk->size - 4);
}

static void* getChunkData(RIFFChunk* chunk)
{
	// Type is also part of the chunk, but also data, remove it from the size
	return (char*)chunk + sizeof(RIFFChunk) - 4;
}

/**
 * Constructor - Initialize SDL3Mouse with window handle
 * @param window SDL3 window handle (required for mouse capture)
 */
SDL3Mouse::SDL3Mouse(SDL_Window* window)
	: Mouse(),
	  m_Window(window),
	  m_IsCaptured(false),
	  m_IsVisible(true),
	  m_LostFocus(false),           // GeneralsX @bugfix felipebraz 18/02/2026 Track window focus state
	  m_nextFreeIndex(0),   // Fighter19 pattern: write position for new events
	  m_nextGetIndex(0),    // Fighter19 pattern: read position for events
	  m_LeftButtonDownTime(0),
	  m_RightButtonDownTime(0),
	  m_MiddleButtonDownTime(0),
	  m_LastFrameNumber(0),  // GeneralsX @bugfix felipebraz 18/02/2026 Initialize frame tracking
	  m_directionFrame(0),    // GeneralsX @bugfix BenderAI 22/02/2026 Initialize cursor direction frame
	  m_lastSetCursor(INVALID_MOUSE_CURSOR),
	  m_lastSetDirectionFrame(-1)
{
	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "DEBUG: SDL3Mouse::SDL3Mouse() created\n");

	// Initialize event buffer with SDL_EVENT_FIRST sentinel (means "empty" slot)
	// GeneralsX @refactor felipebraz 16/02/2026 Fighter19 pattern
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));

	m_LeftButtonDownPos.x = 0;
	m_LeftButtonDownPos.y = 0;
	m_RightButtonDownPos.x = 0;
	m_RightButtonDownPos.y = 0;
	m_MiddleButtonDownPos.x = 0;
	m_MiddleButtonDownPos.y = 0;
}

/**
 * Destructor
 */
SDL3Mouse::~SDL3Mouse(void)
{
	releaseCapture();
	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "DEBUG: SDL3Mouse::~SDL3Mouse() destroyed\n");
}

/**
 * Load cursor from ANI file (fighter19 pattern with RIFF parsing)
 * GeneralsX @bugfix BenderAI 22/02/2026 Port fighter19 cursor loading
 */
void SDL3Mouse::tintCursorSurface(SDL_Surface*& surface, CursorTint tint)
{
	if (!surface || tint == CursorTint::None)
		return;
	SDL_Surface* tinted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	if (!tinted || !SDL_LockSurface(tinted))
	{
		SDL_DestroySurface(tinted);
		return;
	}
	for (Int y = 0; y < tinted->h; ++y)
	{
		auto* pixel = static_cast<Uint8*>(tinted->pixels) + y * tinted->pitch;
		for (Int x = 0; x < tinted->w; ++x, pixel += 4)
		{
			if (tint == CursorTint::Green)
			{
				const Uint8 brightness = std::max({pixel[0], pixel[1], pixel[2]});
				pixel[0] = brightness * 2 / 5;
				pixel[1] = brightness;
				pixel[2] = brightness / 5;
			}
			else
			{
				pixel[1] = std::max(pixel[1], static_cast<Uint8>(pixel[0] * 4 / 5));
				pixel[2] = std::max(pixel[2], static_cast<Uint8>(pixel[0] * 3 / 8));
			}
		}
	}
	SDL_UnlockSurface(tinted);
	SDL_DestroySurface(surface);
	surface = tinted;
}

AnimatedCursor* SDL3Mouse::loadCursorFromFile(const char* filepath, CursorTint tint)
{
	File* file = TheFileSystem->openFile(filepath, File::READ | File::BINARY);
	if (!file)
	{
		DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Failed to open ANI cursor [%s]", filepath));
		return NULL;
	}

	// Read entire file and close it
	const Int size = file->size();
	if (size < static_cast<Int>(sizeof(RIFFChunk)) || size > 16 * 1024 * 1024)
	{
		file->close();
		return nullptr;
	}
	char* file_buffer = file->readEntireAndClose();

	if (!file_buffer)
	{
		DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Failed to read ANI cursor [%s]", filepath));
		return NULL;
	}

	constexpr char curSignature[] = {'\0', '\0', '\2', '\0'};
	if (memcmp(file_buffer, curSignature, sizeof(curSignature)) == 0)
	{
		std::unique_ptr<char[]> bytes(file_buffer);
		SDL_IOStream* stream = SDL_IOFromConstMem(bytes.get(), size);
		if (!stream)
			return nullptr;
		auto cursor = std::make_unique<AnimatedCursor>();
		SDL_Surface* surface = IMG_LoadTyped_IO(stream, true, "CUR");
		if (!surface)
			return nullptr;
		const auto props = SDL_GetSurfaceProperties(surface);
		const int x = std::clamp(static_cast<int>(SDL_GetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_X_NUMBER, 0)), 0, surface->w - 1);
		const int y = std::clamp(static_cast<int>(SDL_GetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_Y_NUMBER, 0)), 0, surface->h - 1);
		tintCursorSurface(surface, tint);
		cursor->m_frameSurfaces[0] = surface;
		cursor->m_frameCount = 1;
		cursor->m_cursor = SDL_CreateColorCursor(surface, x, y);
		return cursor->m_cursor ? cursor.release() : nullptr;
	}

	RIFFChunk *riff_header = (RIFFChunk*)file_buffer;
	if (riff_header->id != riff_id)
	{
		DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Not a RIFF file"));
		delete[] file_buffer;
		return NULL;
	}

	if(riff_header->type != acon_id) {
		DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Not an animated cursor file"));
		delete[] file_buffer;
		return NULL;
	}

	DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: loading %s", filepath));
	AnimatedCursor* cursor = new AnimatedCursor();

	RIFFChunk* chunk = (RIFFChunk*)((char*)file_buffer + sizeof(RIFFChunk));

	while (chunk != NULL && (char *)chunk < file_buffer + size)
	{
		if (chunk->id == anih_id)
		{
			if (chunk->size != sizeof(ANIHeader))
			{
				DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Invalid ANI header size"));
				delete cursor;
				delete[] file_buffer;
				return NULL;
			}

			ANIHeader *ani_header = (ANIHeader*)getChunkData(chunk);

			cursor->m_frameCount = ani_header->frames;
			cursor->m_frameRate = ani_header->displayRate;
		}
		else if (chunk->id == list_id && chunk->type == fram_id)
		{
			int frame_index = 0;
			int hot_spot_x = 0;
			int hot_spot_y = 0;
			bool has_hotspot = false;

			RIFFChunk *frame = (RIFFChunk*)((char *)chunk + sizeof(RIFFChunk));
			while (frame != NULL && (char *)frame < file_buffer + size)
			{
				if (frame->id == icon_id)
				{
					const void *frame_buffer = getChunkData(frame);
					SDL_IOStream *io_stream = SDL_IOFromConstMem(frame_buffer, frame->size);
					SDL_Surface *surface = cursor->m_frameSurfaces[frame_index] = IMG_LoadTyped_IO(io_stream, true, "ico");

					if (!surface)
					{
						DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Failed to load frame"));
						delete cursor;
						delete[] file_buffer;
						return NULL;
					}

					// Allow specifying the hot spot via properties on the surface
					SDL_PropertiesID props = SDL_GetSurfaceProperties(surface);
					if (!has_hotspot)
					{
						hot_spot_x = (int)SDL_GetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_X_NUMBER, 0);
						hot_spot_y = (int)SDL_GetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_Y_NUMBER, 0);
						has_hotspot = true;
					}

					frame_index++;
				}

				if (frame_index >= MAX_2D_CURSOR_ANIM_FRAMES)
				{
					DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Too many frames"));
					break;
				}

				frame = getNextChunk(frame);
			}

			if (frame_index > 0)
			{
				cursor->m_frameCount = frame_index;
				const int clamped_rate = (cursor->m_frameRate > 0) ? cursor->m_frameRate : 4;
				const Uint32 frame_duration_ms = (Uint32)((clamped_rate * 1000) / 60);
				SDL_CursorFrameInfo frame_infos[MAX_2D_CURSOR_ANIM_FRAMES];
				for (int i = 0; i < frame_index; ++i)
				{
					if (i == 0 || tint == CursorTint::Green)
						tintCursorSurface(cursor->m_frameSurfaces[i], tint);
					frame_infos[i].surface = cursor->m_frameSurfaces[i];
					frame_infos[i].duration = frame_duration_ms;
				}
				SDL_Surface *first_surface = cursor->m_frameSurfaces[0];

				if (first_surface)
				{
					if (hot_spot_x < 0) hot_spot_x = 0;
					if (hot_spot_y < 0) hot_spot_y = 0;
					if (hot_spot_x >= first_surface->w) hot_spot_x = first_surface->w - 1;
					if (hot_spot_y >= first_surface->h) hot_spot_y = first_surface->h - 1;
				}

				// GeneralsX @bugfix felipebraz 28/04/2026 Use SDL native animated cursor to ensure frame playback.
				if (tint == CursorTint::Gold && first_surface)
					cursor->m_cursor = SDL_CreateColorCursor(first_surface, hot_spot_x, hot_spot_y);
				else
					cursor->m_cursor = SDL_CreateAnimatedCursor(frame_infos, frame_index, hot_spot_x, hot_spot_y);
				if (!cursor->m_cursor)
				{
					DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: SDL_CreateAnimatedCursor failed [%s]", SDL_GetError()));
					// GeneralsX @bugfix BenderAI 11/05/2026 Fallback to static cursor when ANI animation fails.
					if (first_surface)
					{
						cursor->m_cursor = SDL_CreateColorCursor(first_surface, hot_spot_x, hot_spot_y);
					}
					if (!cursor->m_cursor)
					{
						DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: SDL_CreateColorCursor fallback failed [%s]", SDL_GetError()));
						delete cursor;
						delete[] file_buffer;
						return NULL;
					}
				}
			}

			break;
		}
		else
		{
			DEBUG_LOG(("SDL3Mouse::loadCursorFromFile: Unhandled chunk"));
		}
		chunk = getNextChunk(chunk);
	}

#ifdef _DEBUG
	size_t loaded_frames = 0;
	for (int i = 0; i < MAX_2D_CURSOR_ANIM_FRAMES; i++)
	{
		if (cursor->m_frameSurfaces[i])
			loaded_frames++;
	}

	// DEBUG_ASSERTCRASH(loaded_frames == cursor->m_frameCount, ("Loaded frames do not match header"));
#endif

	delete[] file_buffer;
	return cursor;
}

/**
 * Initialize mouse subsystem
 */
void SDL3Mouse::init(void)
{
	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "INFO: SDL3Mouse::init()\n");

	// Call parent init (loads cursor info from INI, etc.)
	Mouse::init();

	// SDL3 always reports absolute window pixel coordinates (not deltas).
	// Without this flag, Mouse::processMouseEvent() uses MOUSE_MOVE_RELATIVE mode
	// and ADDS the incoming coordinate to the current tracked position, causing the
	// internal cursor to drift infinitely away from the visible cursor.
	// fighter19 / Win32Mouse both set this; we MUST too.
	// GeneralsX @bugfix felipebraz 21/02/2026 Fix mouse click never registering on UI
	m_inputMovesAbsolute = TRUE;

	// Show cursor by default
	SDL_ShowCursor();
	m_IsVisible = true;

	// Clear event buffer - Fighter19 pattern
	// GeneralsX @refactor felipebraz 16/02/2026
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));
	m_nextFreeIndex = 0;
	m_nextGetIndex = 0;

	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "INFO: SDL3Mouse::init() complete\n");
}

/**
 * Reset mouse to default state
 */
void SDL3Mouse::reset(void)
{
	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "DEBUG: SDL3Mouse::reset()\n");

	Mouse::reset();

	releaseCapture();
	SDL_ShowCursor();
	m_IsVisible = true;

	m_lastSetCursor = INVALID_MOUSE_CURSOR;
	m_lastSetDirectionFrame = -1;

	// Clear event buffer - Fighter19 pattern
	// GeneralsX @refactor felipebraz 16/02/2026
	memset(m_eventBuffer, 0, sizeof(m_eventBuffer));
	m_nextFreeIndex = 0;
	m_nextGetIndex = 0;
}

/**
 * Update mouse state (called per-frame)
 */
void SDL3Mouse::update(void)
{
	// Call parent update (processes events, updates m_currMouse)
	Mouse::update();
}

/**
 * Initialize cursor resources (load cursor images from ANI files)
 * GeneralsX @bugfix BenderAI 22/02/2026 Port fighter19 cursor loading
 */
void SDL3Mouse::initCursorResources(void)
{
	for (Int cursor=FIRST_CURSOR; cursor<NUM_MOUSE_CURSORS; cursor++)
	{
		for (Int direction=0; direction<m_cursorInfo[cursor].numDirections; direction++)
		{
			if (!cursorResources[cursor][direction] && !m_cursorInfo[cursor].textureName.isEmpty())
			{
				std::string name = m_cursorInfo[cursor].textureName.str();
				const auto dot = name.find_last_of('.');
				const bool explicitExtension = dot != std::string::npos &&
					(stricmp(name.c_str() + dot, ".ani") == 0 || stricmp(name.c_str() + dot, ".cur") == 0);
				const std::string extension = explicitExtension ? name.substr(dot) : ".ani";
				if (explicitExtension)
					name.resize(dot);
				if (m_cursorInfo[cursor].numDirections > 1)
					name += std::to_string(direction);
				const char* directory = RetailFileLayers::enabled() ? "data/cursors/" : "Data/Cursors/";
				std::string resourcePath = directory + name + extension;
				if (!explicitExtension && !TheFileSystem->doesFileExist(resourcePath.c_str()))
					resourcePath.replace(resourcePath.size() - 4, 4, ".cur");
				CursorTint tint = CursorTint::None;
				if (RetailFileLayers::enabled())
				{
					if (cursor == MOVETO)
						tint = CursorTint::Green;
					else if (cursor == ATTACK_OBJECT || cursor == FORCE_ATTACK_OBJECT ||
						cursor == FORCE_ATTACK_GROUND || cursor == OUTRANGE)
						tint = CursorTint::Gold;
				}
				cursorResources[cursor][direction] = loadCursorFromFile(resourcePath.c_str(), tint);
				DEBUG_ASSERTCRASH(cursorResources[cursor][direction], ("MissingCursor %s\n", resourcePath.c_str()));
			}
		}
	}

	// GeneralsX @bugfix felipebraz 16/10/2026 Log cursor initialization status to console
	if (cursorResources[NORMAL][0] && cursorResources[NORMAL][0]->m_cursor)
	{
		fprintf(stderr, "[CURSOR] Default normal cursor loaded successfully\n");
		fflush(stderr);
	}
	else
	{
		fprintf(stderr, "[CURSOR] WARNING: Default normal cursor failed to load!\n");
		fflush(stderr);
	}
}

/**
 * Set mouse cursor type
 * GeneralsX @bugfix BenderAI 22/02/2026 Implement cursor animation and selection
 */
void SDL3Mouse::setCursor(MouseCursor cursor)
{
	// extend
	Mouse::setCursor( cursor );

	if (m_LostFocus)  // GeneralsX @bugfix BenderAI 22/02/2026 Fix case: m_LostFocus not m_lostFocus
		return;	//stop messing with mouse cursor if we don't have focus.

	setCursorDirection(cursor);

	if (cursor == m_lastSetCursor && m_directionFrame == m_lastSetDirectionFrame)
	{
		return; // Avoid redundant OS cursor sets
	}

	m_lastSetCursor = cursor;
	m_lastSetDirectionFrame = m_directionFrame;

	bool bUseDefaultCursor = false;
	if (cursor == NONE || !m_visible)
	{
		bUseDefaultCursor = true;
	}
	else
	{
		MouseCursor resourceCursor = cursor;
		if (RetailFileLayers::enabled() && !cursorResources[cursor][m_directionFrame])
		{
			if (cursor == BUILD_PLACEMENT)
				resourceCursor = MOVETO;
			else if (cursor == INVALID_BUILD_PLACEMENT)
				resourceCursor = GENERIC_INVALID;
		}
		AnimatedCursor* currentCursor = cursorResources[resourceCursor][m_directionFrame];
		if (currentCursor && currentCursor->m_cursor)
		{
			SDL_SetCursor(currentCursor->m_cursor);
		}
		else
		{
			bUseDefaultCursor = true;
		}
	}

	if (bUseDefaultCursor)
	{
		if (cursorResources[NORMAL][0] && cursorResources[NORMAL][0]->m_cursor)
		{
			SDL_SetCursor(cursorResources[NORMAL][0]->m_cursor);
		}
		else
		{
			// Fallback to SDL's default cursor in case of failure
			// This is to avoid crashing in case of case sensitivity issues
			SDL_SetCursor(SDL_GetDefaultCursor());
		}
	}

	// save current cursor
	m_currentCursor = cursor;
}

void SDL3Mouse::draw(void)
{
	setCursor(m_currentCursor);
	if ((m_retailCommandTooltip || m_retailWorldTooltip) && m_visible)
		drawTooltip();
}

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void SDL3Mouse::setCursorDirection(MouseCursor cursor)
{
	Coord2D offset = {0, 0};
	//Check if we have a directional cursor that needs different images for each direction
	if (m_cursorInfo[cursor].numDirections > 1 && TheInGameUI && TheInGameUI->isScrolling())
	{
		offset = TheInGameUI->getScrollAmount();
		if (offset.x || offset.y)
		{
			offset.normalize();
			Real theta = atan2(offset.y, offset.x);
			theta = fmod(theta+M_PI*2,M_PI*2);
			Int numDirections=m_cursorInfo[m_currentCursor].numDirections;
			//Figure out which of our predrawn cursor orientations best matches the
			//actual cursor direction.  Frame 0 is assumed to point right and continue
			//clockwise.
			m_directionFrame=(Int)(theta/(2.0f*M_PI/(Real)numDirections)+0.5f);
			if (m_directionFrame >= numDirections)
				m_directionFrame = 0;
		}
		else
		{
			m_directionFrame=0;
		}
	}
	else
		m_directionFrame = 0;
}

/**
 * Set cursor visibility
 * GeneralsX @bugfix BenderAI 10/03/2026 - Call parent to keep m_visible in sync.
 * Without the parent call, m_visible stays FALSE forever (initialized to FALSE in Mouse constructor).
 * setCursor() checks !m_visible → always uses default NORMAL cursor, breaking attack-move and force-fire
 * cursor feedback, and preventing cursor from showing after ESC menu is dismissed.
 */
void SDL3Mouse::setVisibility(Bool visible)
{
	// Always tell parent cursor is visible so setCursor() selects the correct cursor type.
	// On Windows, setVisibility(FALSE) hides OS cursor so W3D draws its own. On SDL3
	// there is no W3D cursor rendering, so m_visible must stay TRUE.
	Mouse::setVisibility(TRUE);
	m_IsVisible = visible;

	// Always keep SDL cursor visible since it's the only cursor renderer.
	SDL_ShowCursor();
}

/**
 * Handle window losing focus
 */
void SDL3Mouse::loseFocus()
{
	m_LostFocus = true;           // GeneralsX @bugfix felipebraz 18/02/2026 Set focus flag
	releaseCapture();
}

/**
 * Handle window regaining focus
 */
void SDL3Mouse::regainFocus()
{
	m_LostFocus = false;          // GeneralsX @bugfix felipebraz 18/02/2026 Clear focus flag
	// Capture may be re-enabled by game logic
}

/**
 * Capture mouse (confine to window)
 */
void SDL3Mouse::capture(void)
{
	if (!m_Window || m_IsCaptured) {
		return;
	}

	// SDL3: Capture mouse to window
	SDL_CaptureMouse(true);

	// SDL3: Grab mouse (confine to window)
	SDL_SetWindowMouseGrab(m_Window, true);

	m_IsCaptured = true;

	// @fix Notify base class so isCursorCaptured() returns true.
	// This is required for edge scrolling (canScrollAtScreenEdge checks isCursorCaptured).
	onCursorCaptured(true);
}

/**
 * Release mouse capture
 */
void SDL3Mouse::releaseCapture(void)
{
	if (!m_IsCaptured) {
		return;
	}

	SDL_CaptureMouse(false);
	if (m_Window) {
		SDL_SetWindowMouseGrab(m_Window, false);
	}

	m_IsCaptured = false;

	// @fix Notify base class so isCursorCaptured() returns false.
	onCursorCaptured(false);
}

/**
 * Get next mouse event from buffer
 * Called by Mouse::update() in parent class
 * Fighter19 pattern: check for SDL_EVENT_FIRST sentinel (value 0 = empty)
 *
 * @param result Output MouseIO structure
 * @param flush If true, flush all events
 * @return MOUSE_OK if event retrieved, MOUSE_NONE if buffer empty
 */
UnsignedByte SDL3Mouse::getMouseEvent(MouseIO *result, Bool flush)
{
	// Check if buffer is empty: if event type is SDL_EVENT_FIRST (0), slot is empty
	// GeneralsX @refactor felipebraz 16/02/2026 Fighter19 pattern: raw SDL_Event with sentinel
	if (m_eventBuffer[m_nextGetIndex].type == SDL_EVENT_FIRST) {
		return MOUSE_NONE;
	}

	// Translate SDL_Event to MouseIO
	// GeneralsX @refactor felipebraz 16/02/2026 Use unified translateEvent()
	translateEvent(m_nextGetIndex, result);

	// Mark this slot as empty (sentinel)
	m_eventBuffer[m_nextGetIndex].type = SDL_EVENT_FIRST;

	// Advance read position (circular buffer)
	m_nextGetIndex = (m_nextGetIndex + 1) % MAX_SDL3_MOUSE_EVENTS;

	return MOUSE_OK;
}

/**
 * Add SDL3 mouse motion event to buffer (LEGACY - being phased out)
 * Refactored to use unified addSDLEvent() internally
 * GeneralsX @refactor felipebraz 16/02/2026 Transitioning to Fighter19 model
 */
void SDL3Mouse::addSDL3MouseMotionEvent(const SDL_MouseMotionEvent& event)
{
	// Wrap in SDL_Event and call unified handler
	SDL_Event sdlEvent;
	memset(&sdlEvent, 0, sizeof(sdlEvent));
	sdlEvent.type = SDL_EVENT_MOUSE_MOTION;
	sdlEvent.motion = event;
	addSDLEvent(&sdlEvent);
}

/**
 * Add SDL3 mouse button event to buffer (LEGACY - being phased out)
 * Refactored to use unified addSDLEvent() internally
 * GeneralsX @refactor felipebraz 16/02/2026 Transitioning to Fighter19 model
 */
void SDL3Mouse::addSDL3MouseButtonEvent(const SDL_MouseButtonEvent& event)
{
	// Wrap in SDL_Event and call unified handler
	SDL_Event sdlEvent;
	memset(&sdlEvent, 0, sizeof(sdlEvent));
	sdlEvent.type = event.down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	sdlEvent.button = event;
	addSDLEvent(&sdlEvent);
}

/**
 * Add SDL3 mouse wheel event to buffer (LEGACY - being phased out)
 * Refactored to use unified addSDLEvent() internally
 * GeneralsX @refactor felipebraz 16/02/2026 Transitioning to Fighter19 model
 */
void SDL3Mouse::addSDL3MouseWheelEvent(const SDL_MouseWheelEvent& event)
{
	// Wrap in SDL_Event and call unified handler
	SDL_Event sdlEvent;
	memset(&sdlEvent, 0, sizeof(sdlEvent));
	sdlEvent.type = SDL_EVENT_MOUSE_WHEEL;
	sdlEvent.wheel = event;
	addSDLEvent(&sdlEvent);
}

/**
 * Translate SDL3 motion event to MouseIO
 */
void SDL3Mouse::translateMotionEvent(const SDL_MouseMotionEvent& event, MouseIO *result)
{
	result->pos.x = (Int)event.x;
	result->pos.y = (Int)event.y;
	result->deltaPos.x = (Int)event.xrel;
	result->deltaPos.y = (Int)event.yrel;
	// GeneralsX @bugfix felipebraz 18/02/2026 Normalize timestamp to milliseconds (SDL3 uses nanoseconds)
	result->time = (Uint32)(event.timestamp / 1000000);

	// No button state change on motion
	result->leftState = MBS_None;
	result->rightState = MBS_None;
	result->middleState = MBS_None;
	result->wheelPos = 0;
}

/**
 * Translate SDL3 button event to MouseIO
 */
void SDL3Mouse::translateButtonEvent(const SDL_MouseButtonEvent& event, MouseIO *result)
{
	result->pos.x = (Int)event.x;
	result->pos.y = (Int)event.y;
	result->deltaPos.x = 0;
	result->deltaPos.y = 0;
	// GeneralsX @bugfix felipebraz 18/02/2026 Normalize timestamp to milliseconds (SDL3 uses nanoseconds)
	result->time = (Uint32)(event.timestamp / 1000000);
	result->wheelPos = 0;

	// Initialize all button states to None
	result->leftState = MBS_None;
	result->rightState = MBS_None;
	result->middleState = MBS_None;

	// GeneralsX @bugfix felipebraz 18/02/2026 Initialize frame tracking for replay determinism
	result->leftFrame = 0;
	result->rightFrame = 0;
	result->middleFrame = 0;

	MouseButtonState state = event.down ? MBS_Down : MBS_Up;

	// GeneralsX @bugfix BenderAI 17/02/2026 Debug mouse button events
	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "[MOUSE] Button event: button=%d state=%s pos=(%d,%d)\n",
	//	event.button, event.down ? "DOWN" : "UP", (Int)event.x, (Int)event.y);

	// Get current frame for replay determinism
	// GeneralsX @bugfix felipebraz 18/02/2026 Use game frame instead of timestamp
	UnsignedInt currentFrame = (TheGameLogic) ? TheGameLogic->getFrame() : 1;

	// Map SDL3 button to MouseIO button
	switch (event.button) {
		case SDL_BUTTON_LEFT:
			// GeneralsX @bugfix BenderAI 07/03/2026 Only detect double-click on DOWN events
			// (matching right/middle button logic). Without this check, rapid clicks cause
			// UP events with clicks>=2 to become MBS_DoubleClick, which GadgetPushButton
			// doesn't handle, leaving buttons stuck in WIN_STATE_SELECTED (white overlay).
			if (event.down && event.clicks >= 2) {
				result->leftState = MBS_DoubleClick;
			} else {
				result->leftState = state;
			}
			result->leftFrame = currentFrame;  // GeneralsX @bugfix felipebraz 18/02/2026 Track frame for replay
			break;

		case SDL_BUTTON_RIGHT:
			// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
			// fprintf(stderr, "[MOUSE] Right button: %s\n", event.down ? "DOWN" : "UP");
			// GeneralsX @bugfix felipebraz 18/02/2026 Use native SDL3 clicks for right button too
			if (event.down && event.clicks >= 2) {
				result->rightState = MBS_DoubleClick;
				// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
				// fprintf(stderr, "[MOUSE] Right double-click detected (clicks=%d)\n", event.clicks);
			} else {
				result->rightState = state;
			}
			result->rightFrame = currentFrame;  // GeneralsX @bugfix felipebraz 18/02/2026 Track frame for replay
			break;

		case SDL_BUTTON_MIDDLE:
			// GeneralsX @bugfix felipebraz 18/02/2026 Support double-click for middle button
			if (event.down && event.clicks >= 2) {
				result->middleState = MBS_DoubleClick;
			} else {
				result->middleState = state;
			}
			result->middleFrame = currentFrame;  // GeneralsX @bugfix felipebraz 18/02/2026 Track frame for replay
			break;
	}
}

/**
 * Translate SDL3 wheel event to MouseIO
 */
void SDL3Mouse::translateWheelEvent(const SDL_MouseWheelEvent& event, MouseIO *result)
{
	// SDL3 mouse position not provided in wheel event, get current position
	float mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	result->pos.x = (Int)mouseX;
	result->pos.y = (Int)mouseY;
	result->deltaPos.x = 0;
	result->deltaPos.y = 0;
	// GeneralsX @bugfix felipebraz 18/02/2026 Normalize timestamp to milliseconds (SDL3 uses nanoseconds)
	result->time = (Uint32)(event.timestamp / 1000000);

	// SDL3 wheel: positive = up/away, negative = down/toward user
	// Multiply by MOUSE_WHEEL_DELTA (120) to match Windows behavior
	// GeneralsX @bugfix msmesoft 17/06/2026 macOS "natural scrolling" makes SDL
	// flag the wheel values as SDL_MOUSEWHEEL_FLIPPED; invert them so the in-game
	// scroll direction matches the Windows convention regardless of the OS setting.
	float wheelY = event.y;
	if (event.direction == SDL_MOUSEWHEEL_FLIPPED)
		wheelY = -wheelY;
	result->wheelPos = (Int)(wheelY * MOUSE_WHEEL_DELTA);

	result->leftState = MBS_None;
	result->rightState = MBS_None;
	result->middleState = MBS_None;
}

/**
 * Scale raw SDL3 window coordinates to game internal resolution.
 * This is CRITICAL for correct hit-testing: the game's UI layout is based on
 * its internal resolution (e.g. 800x600), but SDL reports window coordinates
 * relative to the actual window size (e.g. 1920x1080). Without scaling, clicks
 * land at the wrong position and the game ignores them.
 *
 * Port of fighter19's scaleMouseCoordinates() - direct adaptation.
 *
 * GeneralsX @bugfix felipebraz 20/02/2026 Fix mouse click coordinates not matching UI layout
 */
void SDL3Mouse::scaleMouseCoordinates(float rawX, float rawY, Uint32 windowID, int& scaledX, int& scaledY)
{
	SDL_Window* window = SDL_GetWindowFromID(windowID);
	if (!window || !TheDisplay) {
		scaledX = static_cast<int>(rawX);
		scaledY = static_cast<int>(rawY);
		return;
	}

	UiPixelSpace::WindowSize size;
	SDL_GetWindowSize(window, &size.pointWidth, &size.pointHeight);
	SDL_GetWindowSizeInPixels(window, &size.pixelWidth, &size.pixelHeight);

	UiPixelSpace::Viewport viewport;
	Int viewportX = 0, viewportY = 0, viewportWidth = 0, viewportHeight = 0;
	if (TheDisplay->getViewportRect(viewportX, viewportY, viewportWidth, viewportHeight)) {
		viewport = { viewportX, viewportY, viewportWidth, viewportHeight };
	}

	const UiPixelSpace::Point point = UiPixelSpace::pointToRender(rawX, rawY, size, viewport,
		static_cast<int>(TheDisplay->getWidth()), static_cast<int>(TheDisplay->getHeight()));
	scaledX = point.x;
	scaledY = point.y;
}

Bool SDL3Mouse::getEdgeScrollPosition(ICoord2D& position) const
{
	if (!m_Window || SDL_GetKeyboardFocus() != m_Window || SDL_GetMouseFocus() != m_Window)
		return FALSE;

	Int width, height;
	if (!SDL_GetWindowSize(m_Window, &width, &height))
		return FALSE;
	float x, y;
	SDL_GetMouseState(&x, &y);
	if (!(x >= 0.0f && y >= 0.0f && x < width && y < height))
		return FALSE;

	scaleMouseCoordinates(x, y, SDL_GetWindowID(m_Window), position.x, position.y);
	return TRUE;
}

/**
 * Add raw SDL_Event to mouse event buffer
 * Fighter19 pattern: unified method that accepts any SDL_Event
 * Called by SDL3GameEngine::serviceWindowsOS() directly from event loop
 *
 * @param event Raw SDL_Event from SDL_PollEvent()
 *
 * GeneralsX @refactor felipebraz 16/02/2026 Fighter19 event model
 */
void SDL3Mouse::addSDLEvent(SDL_Event *event)
{
	if (!event) {
		return;
	}

	// Filter only mouse-related events
	if (event->type != SDL_EVENT_MOUSE_MOTION &&
	    event->type != SDL_EVENT_MOUSE_BUTTON_DOWN &&
	    event->type != SDL_EVENT_MOUSE_BUTTON_UP &&
	    event->type != SDL_EVENT_MOUSE_WHEEL) {
		return;  // Not a mouse event, ignore
	}

	// Check if buffer is full
	UnsignedInt nextFreeIndex = (m_nextFreeIndex + 1) % MAX_SDL3_MOUSE_EVENTS;
	if (nextFreeIndex == m_nextGetIndex) {
		// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
		// fprintf(stderr, "WARNING: SDL3Mouse::addSDLEvent() buffer full (dropped event)\n");
		return;
	}

	// Copy entire event to buffer
	m_eventBuffer[m_nextFreeIndex] = *event;

	// GeneralsX @bugfix BenderAI 18/02/2026 Temporarily disable debug logging (Phase 1.8)
	// fprintf(stderr, "DEBUG: SDL3Mouse::addSDLEvent() type=%u index=%u\n", event->type, m_nextFreeIndex);

	// Advance write position (circular buffer)
	m_nextFreeIndex = nextFreeIndex;
}

/**
 * Translate SDL_Event at given buffer index to MouseIO
 * Unified translation method that handles all mouse event types
 *
 * @param eventIndex Index in m_eventBuffer to translate
 * @param result Output MouseIO structure
 *
 * GeneralsX @refactor felipebraz 16/02/2026 Unified translation
 */
void SDL3Mouse::translateEvent(UnsignedInt eventIndex, MouseIO *result)
{
	if (eventIndex >= MAX_SDL3_MOUSE_EVENTS || !result) {
		return;
	}

	const SDL_Event& event = m_eventBuffer[eventIndex];

	// Raw SDL logical window coordinates and window ID, extracted per event type.
	float rawX = 0.0f, rawY = 0.0f;
	Uint32 windowID = 0;

	// Switch on event type and delegate to appropriate translation method
	switch (event.type) {
		case SDL_EVENT_MOUSE_MOTION:
			translateMotionEvent(event.motion, result);
			rawX     = event.motion.x;
			rawY     = event.motion.y;
			windowID = event.motion.windowID;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			translateButtonEvent(event.button, result);
			rawX     = event.button.x;
			rawY     = event.button.y;
			windowID = event.button.windowID;
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			translateWheelEvent(event.wheel, result);
			rawX     = event.wheel.mouse_x;
			rawY     = event.wheel.mouse_y;
			windowID = event.wheel.windowID;
			break;
		default:
			// Should not happen (sentinel value)
			memset(result, 0, sizeof(*result));
			return;
	}

	// Scale from SDL logical window space to game internal resolution.
	// GeneralsX @bugfix felipebraz 20/02/2026 Without this, UI hit-testing fails because
	// the game checks clicks against its internal resolution (e.g. 800x600) but SDL
	// reports coordinates in the current window coordinate space.
	int scaledX = 0, scaledY = 0;
	scaleMouseCoordinates(rawX, rawY, windowID, scaledX, scaledY);
	result->pos.x = scaledX;
	result->pos.y = scaledY;
}

#endif // !_WIN32
