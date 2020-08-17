/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _RENDERER_H
#define _RENDERER_H

#include "Game/Game.h"
#include "Game/PreGame.h"
#include "Rendering/Renderer/Scene.h"

class Renderer;
// Base renderer does nothing; we'll refer to that as a "Headless" renderer for clarity.
typedef Renderer HeadlessRenderer;

extern Renderer* pRenderer;

/**
 * Provides a rendering API to the engine that handles setup and teardown, 
 * and API-specific drawing for the active controller.
 */
class Renderer {
    public:
        /** 
         * @brief Automatically detects which scene creator is associated with the 
         * controller and sets that scene.
         */
        void LoadScene(CGameController* pGameController) {
            auto pPreGame = dynamic_cast<CPreGame*>(pGameController);
            if (pPreGame != nullptr) {
                SetPreGameScene(pPreGame);
                return;
            }
            auto pGame = dynamic_cast<CGame*>(pGameController);
            if (pGame != nullptr) {
                SetGameScene(pGame);
                return;
            }
        }
        /**
         * @brief Allows custom PreGame scene creation by Renderer subclasses.
         */
        virtual void SetPreGameScene(CPreGame* pPreGame) {}
        /** 
         * @brief Allows custom Game scene creation by Renderer subclasses.
         */
        virtual void SetGameScene(CGame* pGame) {}
        
        /** 
         * @brief Ends the previous scene (if not already ended) and stores 
         * the new scene as current.
         * 
         * It is assumed that the new scene has already been set up.
         */
        void SetScene(Scene* newScene) {
            EndScene();
            currentScene = newScene;
        }

        /** 
         * @brief Allows the current scene (if it exist) to perform cleanup, 
         * then destroys it. 
         */
        void EndScene() {
            if (currentScene != nullptr) {
                currentScene->End();
                delete currentScene;
            }
        }

        /**
         * @brief Initializes the game window
         * 
         * @return whether window initialization succeeded
         * @param title char* string with window title
         */
        virtual bool InitWindow(const char* title) { return true; }
        /**
         * @brief Initialises the actual rendering system after the window
         * has been created.
         */
        virtual void PostWindowInit() {}
        /**
         * @brief Forwards interface updates to interface components to allow 
         * them to dynamically resize.
         */
        virtual void UpdateInterfaceGeometry() {}
        /** 
         * @brief Reloads rendering data (e.g. shaders, textures)
         * 
         * For when the engine does a full reload.
         */
        virtual void Reload() {}

        /**
         * @brief Instructs the renderer to swap buffers.
         * 
         * This should be at the discretion of the renderer, and removed from the Renderer parent class.
         */
        virtual void SwapBuffers(bool allowSwapBuffers, bool clearErrors) {} // (Currently described in GlobalRendering)

        /**
         * @brief Caches window position and size for next launch.
         */
        virtual void SaveWindowPosAndSize() {}
        /** 
         * @brief Destroys the renderer in anticipation of program termination, and
         * caches some information for next launch (e.g. window position and size).
         * 
         * Consider moving such operations up to this level (though data storage 
         * will deinitely happen lower).
         */
        virtual void Kill() {}

        /** 
         * @brief Shows initial loading scene, which shows the logo, VFS progress, 
         * version, and license. 
         * 
         * Underscored to prevent naming conflict with the existing function.
         */
        virtual void _ShowSplashScreen(const std::string& splashScreenFile, const std::string& springVersionStr, const std::function<bool()>& testDoneFunc) {}
        /** 
         * Displays an error message to the user.
         */
        virtual void ErrorMessageBox(const char* msg, const char* caption, unsigned int flags) {}

        /** 
         * @brief Instructs the current scene to draw a frame.
         */
        bool Draw() { 
            if (currentScene != nullptr) {
                return currentScene->Draw(); 
            }
            return true;
        }
    private:
        /** 
         * The scene handling the drawing for activeController.
         */
        Scene* currentScene;
};

#endif /* _RENDERER_H */