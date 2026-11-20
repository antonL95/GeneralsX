#include "W3DDevice/GameClient/Module/W3DSailModelDraw.h"
#include "Common/INI.h"

#include <memory>

void W3DSailModelDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "MaxRotationDegrees", INI::parseInt, nullptr, offsetof(W3DSailModelDrawModuleData, m_maxRotationDegrees) },
		{ "BlowingThresholdDegrees", INI::parseInt, nullptr, offsetof(W3DSailModelDrawModuleData, m_blowingThresholdDegrees) },
		{ "AboutDamping", INI::parseReal, nullptr, offsetof(W3DSailModelDrawModuleData, m_aboutDamping) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(fields);
	W3DScriptedModelDrawModuleData::buildFieldParse(p);
}

W3DSailModelDraw::W3DSailModelDraw(Thing* thing, const ModuleData* data)
	: W3DScriptedModelDraw(thing, data)
{
}

W3DSailModelDraw::~W3DSailModelDraw() = default;

ModuleData* W3DSailModelDraw::friend_newModuleData(INI* ini)
{
	std::unique_ptr<W3DSailModelDrawModuleData> data(MSGNEW("AllModuleData") W3DSailModelDrawModuleData);
	if (ini)
		ini->initFromINIMultiProc(data.get(), W3DSailModelDrawModuleData::buildFieldParse);
	data->finishParsing();
	return data.release();
}

void W3DSailModelDraw::crc(Xfer* xfer)
{
	W3DScriptedModelDraw::crc(xfer);
}

void W3DSailModelDraw::xfer(Xfer* xfer)
{
	W3DScriptedModelDraw::xfer(xfer);
}

void W3DSailModelDraw::loadPostProcess()
{
	W3DScriptedModelDraw::loadPostProcess();
}
