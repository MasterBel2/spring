/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Game/GameController.h"
#include "Rendering/Renderer/Renderer.h"

/**
 * @brief current active controller
 *
 * Pointer to the currently active controller
 * (could be a PreGame, could be a Game, etc)
 */
CGameController* activeController = nullptr;

/** 
 * @brief Sets the current controller. 
 * 
 * May be used to set the active controller to nullptr.
 */
void CGameController::SetActiveController(CGameController* controller) 
{
	activeController = controller;
	pRenderer->LoadScene(controller);
}

/** 
 * @brief Returns the currently active cuntroller. 
 * 
 * May be nullptr.
 */
CGameController* CGameController::GetActiveController() { return activeController; }

CGameController::~CGameController()
{
	if (activeController == this)
		activeController = nullptr;
}

