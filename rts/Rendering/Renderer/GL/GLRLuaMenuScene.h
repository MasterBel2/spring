/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _GLR_LuaMenu_Scene_h
#define _GLR_LuaMenu_Scene_h

#include "Rendering/Renderer/Scene.h"
#include "Menu/LuaMenuController.h"

class GLRLuaMenuScene : public Scene {
public:
    GLRLuaMenuScene(CLuaMenuController* pLuaMenuController) 
        : pLuaMenuController(pLuaMenuController) {};

    bool Draw() override;
    void End() override;
private:
    CLuaMenuController* pLuaMenuController;
};

#endif /* _GLR_LuaMenu_Scene_h */