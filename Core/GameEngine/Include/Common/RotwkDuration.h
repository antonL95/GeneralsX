#pragma once

namespace RotwkDuration
{
inline constexpr unsigned int FRAMES_PER_SECOND = 5;
inline constexpr unsigned int MSECS_PER_FRAME = 1000 / FRAMES_PER_SECOND;

inline constexpr unsigned int retailFrames(unsigned int msecs)
{
	return msecs / MSECS_PER_FRAME + (msecs % MSECS_PER_FRAME != 0);
}

inline constexpr unsigned int engineFrames(unsigned int frames, unsigned int engineFramesPerSecond)
{
	return frames * (engineFramesPerSecond / FRAMES_PER_SECOND);
}

inline constexpr unsigned int clipReloadFrames(unsigned int frames)
{
	return frames - frames % 3;
}
}
