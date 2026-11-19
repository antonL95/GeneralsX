#include "W3DDevice/GameClient/Module/W3DHordeModelDraw.h"
#include "Common/INI.h"

#include <array>
#include <memory>

void W3DHordeModelDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "LodOptions", W3DHordeModelDrawModuleData::parseLodOption, nullptr, offsetof(W3DHordeModelDrawModuleData, m_lodOptions) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(fields);
	W3DScriptedModelDrawModuleData::buildFieldParse(p);
}

void W3DHordeModelDrawModuleData::parseLodOption(INI* ini, void*, void* store, const void*)
{
	static const char* const levels[] = { "LOW", "MEDIUM", "HIGH", nullptr };
	static const FieldParse fields[] =
	{
		{ "AllowMultipleModels", INI::parseBool, nullptr, offsetof(LodOption, allowMultipleModels) },
		{ "MaxRandomTextures", INI::parseInt, nullptr, offsetof(LodOption, maxRandomTextures) },
		{ "MaxRandomAnimations", INI::parseInt, nullptr, offsetof(LodOption, maxRandomAnimations) },
		{ "MaxAnimFrameDelta", INI::parseReal, nullptr, offsetof(LodOption, maxAnimFrameDelta) },
		{ "RandomStartFramePercent", INI::parseInt, nullptr, offsetof(LodOption, randomStartFramePercent) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	LodOption option;
	option.level = INI::scanIndexList(ini->getNextToken(), levels);
	ini->initFromINI(&option, fields);
	(*static_cast<std::array<LodOption, 3>*>(store))[option.level] = option;
}

W3DHordeModelDraw::W3DHordeModelDraw(Thing* thing, const ModuleData* data)
	: W3DScriptedModelDraw(thing, data)
{
}

W3DHordeModelDraw::~W3DHordeModelDraw() = default;

ModuleData* W3DHordeModelDraw::friend_newModuleData(INI* ini)
{
	std::unique_ptr<W3DHordeModelDrawModuleData> data(MSGNEW("AllModuleData") W3DHordeModelDrawModuleData);
	if (ini)
		ini->initFromINIMultiProc(data.get(), W3DHordeModelDrawModuleData::buildFieldParse);
	data->finishParsing();
	return data.release();
}

void W3DHordeModelDraw::crc(Xfer* xfer)
{
	W3DScriptedModelDraw::crc(xfer);
}

void W3DHordeModelDraw::xfer(Xfer* xfer)
{
	W3DScriptedModelDraw::xfer(xfer);
}

void W3DHordeModelDraw::loadPostProcess()
{
	W3DScriptedModelDraw::loadPostProcess();
}
