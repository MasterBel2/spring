/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Rendering/Renderer/GL/GLRLuaMenuScene.h"
#include "Rendering/GlobalRendering.h"
#include "System/EventHandler.h"
#include "Lua/LuaMenu.h"
#include "Game/UI/MouseHandler.h"
#include "Game/UI/InfoConsole.h"

bool GLRLuaMenuScene::Draw() {
    // we should not become the active controller unless this holds (see ::Activate)
	assert(luaMenu != nullptr);

	eventHandler.CollectGarbage(false);
	infoConsole->PushNewLinesToEventHandler();
	mouse->Update();
	mouse->UpdateCursors();
	eventHandler.Update();
	// calls IsAbove
	mouse->GetCurrentTooltip();

	// render if global rendering active + luamenu allows it, and at least once per 30s
	const bool allowDraw = (globalRendering->active && luaMenu->AllowDraw());
	const bool forceDraw = ((spring_gettime() - luaMenuController->lastDrawFrameTime).toSecsi() > 30);

	if (allowDraw || forceDraw) {
		ClearScreen();

		eventHandler.DrawGenesis();
		eventHandler.DrawScreen();
		mouse->DrawCursor();
		eventHandler.DrawScreenPost();

		luaMenuController->lastDrawFrameTime = spring_gettime();
		return true;
	}

	spring_msecs(10).sleep(true); // no draw needed, sleep a bit
	return false;
}

void GLRLuaMenuScene::End() {}