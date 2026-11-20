#include "W3DDevice/GameClient/Module/W3DQuadrupedDraw.h"
#include "Common/INI.h"

#include <memory>

void W3DQuadrupedDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "LeftFrontFootBone", INI::parseAsciiString, nullptr, offsetof(W3DQuadrupedDrawModuleData, m_leftFrontFootBone) },
		{ "RightFrontFootBone", INI::parseAsciiString, nullptr, offsetof(W3DQuadrupedDrawModuleData, m_rightFrontFootBone) },
		{ "LeftRearFootBone", INI::parseAsciiString, nullptr, offsetof(W3DQuadrupedDrawModuleData, m_leftRearFootBone) },
		{ "RightRearFootBone", INI::parseAsciiString, nullptr, offsetof(W3DQuadrupedDrawModuleData, m_rightRearFootBone) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(fields);
	W3DScriptedModelDrawModuleData::buildFieldParse(p);
}

W3DQuadrupedDraw::W3DQuadrupedDraw(Thing* thing, const ModuleData* data)
	: W3DScriptedModelDraw(thing, data)
{
}

W3DQuadrupedDraw::~W3DQuadrupedDraw() = default;

ModuleData* W3DQuadrupedDraw::friend_newModuleData(INI* ini)
{
	std::unique_ptr<W3DQuadrupedDrawModuleData> data(MSGNEW("AllModuleData") W3DQuadrupedDrawModuleData);
	if (ini)
		ini->initFromINIMultiProc(data.get(), W3DQuadrupedDrawModuleData::buildFieldParse);
	data->finishParsing();
	return data.release();
}

void W3DQuadrupedDraw::crc(Xfer* xfer)
{
	W3DScriptedModelDraw::crc(xfer);
}

void W3DQuadrupedDraw::xfer(Xfer* xfer)
{
	W3DScriptedModelDraw::xfer(xfer);
}

void W3DQuadrupedDraw::loadPostProcess()
{
	W3DScriptedModelDraw::loadPostProcess();
}
