#pragma once

class Drawable;
class AsciiString;

class ClientLuaEvents
{
public:
	static void dispatch(Drawable* drawable, const AsciiString& event);
};
