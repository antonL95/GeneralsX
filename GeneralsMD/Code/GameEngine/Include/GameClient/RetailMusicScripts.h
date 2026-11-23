#pragma once

class AsciiString;
class ScriptAction;

namespace RetailMusicScripts
{
void reset();
void update();
void enqueue(ScriptAction& action);
void recordSpecialPower(int playerIndex, const AsciiString& power);
bool isEvaluating();
}
