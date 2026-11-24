#pragma once

#include "Common/UnicodeString.h"

#include <array>
#include <memory>
#include <vector>

class GameInfo;
class GameWindow;
class WindowLayout;

class RetailLoadingScreen
{
public:
	explicit RetailLoadingScreen(GameInfo &game);
	~RetailLoadingScreen();
	Bool valid() const;
	void update(Int slot, Int percent);
	void draw() const;

private:
	class Data;
	std::unique_ptr<Data> m_data;
};

struct RetailMatchResults
{
	struct PlayerResult
	{
		UnicodeString name;
		std::array<Int, 8> totals;
		Bool defeated;
	};

	std::vector<PlayerResult> players;
	Bool hasLocalResult = FALSE;
	Bool localVictory = FALSE;
};

RetailMatchResults collectRetailMatchResults();
Bool showRetailMatchResults(RetailMatchResults results);
Bool createRetailReturnMenu(WindowLayout &layout);
void updateRetailShellLoad();
