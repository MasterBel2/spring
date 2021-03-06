/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _GL_RENDERER_H
#define _GL_RENDERER_H

#include "Rendering/Renderer/Renderer.h"

#include <SDL.h>
#include <SDL_events.h>

/**
 * A renderer using Spring's bleeding edge of GL development.
 */
class GLRenderer : public Renderer {
public:
    GLRenderer();

    void SetSelectMenuScene(std::shared_ptr<ClientSetup> clientSetup) override;
    void SetPreGameScene(CPreGame* pPreGame) override;
    void SetLuaMenuScene(CLuaMenuController* pLuaMenuController) override;
    void SetGameScene(CGame* pGame) override;

    /** 
     * Basic initialisation and creates an SDL window 
     */
    bool Init(const char* windowTitle) override;
    /**
     * Does further initilisation after basic initialisation and window has been created.
     */
    void PostFileSystemInit() override;
    /**
     * Reloads stored data (e.g. textures, shaders) 
     */
    void Reload() override;
    /** Destroys the renderer in anticipation of program termination, and
     * caches some information for next launch (e.g. window position and size).
     */
    void Kill() override;

    /**
     * Forwards interface updates to interface components to allow them to dynamically resize.
     */
    void UpdateInterfaceGeometry() override;
    /**
     * Caches the window position and size for next launch.
     */
    void SaveWindowPosAndSize() override;
    void SwapBuffers(bool clearErrors) override;

    /**
     * 
     */
    void _ShowSplashScreen(const std::string& splashScreenFile, const std::string& springVersionStr, const std::function<bool()>& testDoneFunc) override;

private:
    /**
     *  @brief Handles SDL input events
     */
    bool MainEventHandler(const SDL_Event& event);
};

#endif /* _GL_RENDERER_H */