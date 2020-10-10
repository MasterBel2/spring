/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef GLRGameScene_hpp
#define GLRGameScene_hpp

#include <stdio.h>
#include "Rendering/Renderer/Scene.h"
#include "Rendering/WorldDrawer.h"

class CGame;
struct spring_time;

class GLRGameScene : public Scene {
public:
    GLRGameScene(CGame* pGame);

    // Scene
    bool Draw() override;
    void End() override;

private:
    CGame* pGame;
    CWorldDrawer worldDrawer;

    bool UpdateUnsynced(const spring_time currentTime);
    void DrawSkip(bool blackscreen = true);
    void DrawInputReceivers();
	void DrawInputText();
	void DrawInterfaceWidgets();
};

#endif /* GLRGameScene_hpp */
