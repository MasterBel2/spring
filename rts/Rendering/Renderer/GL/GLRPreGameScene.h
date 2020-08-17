/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _GLRPreGameScene_h
#define _GLRPreGameScene_h

#include "Rendering/Renderer/Scene.h"
#include "Game/PreGame.h"

/**
 * @brief Handles drawing for a CPreGame object.
 */
class GLRPreGameScene : public Scene {
    public:
        GLRPreGameScene(CPreGame* pPreGame) : pPreGame(pPreGame) {};

        bool Draw() override;
        void End() override;
    private:
        CPreGame* pPreGame;
};


#endif /* GLRPreGameScene.h */