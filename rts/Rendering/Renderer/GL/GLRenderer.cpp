/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Rendering/Renderer/Renderer.h"
#include "Rendering/Renderer/GL/GLRPreGameScene.h"
#include "Rendering/Renderer/GL/GLRLuaMenuScene.h"
#include "Rendering/Renderer/GL/GLRenderer.h"

#include <gflags/gflags.h>

#include "Game/UI/MouseHandler.h"
#include "Game/UI/KeyBindings.h"
#include "System/Input/MouseInput.h"
#include "Rendering/GL/FBO.h"
#include "System/Input/KeyInput.h"
#include "System/TimeProfiler.h"
#include "System/Sound/ISound.h"
#include "Game/GameController.h"
#include "Game/GlobalUnsynced.h"
#include "System/Platform/Watchdog.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Game/ClientSetup.h"
#include "Game/PreGame.h"
#include "System/SafeUtil.h"
#include "Lua/LuaOpenGL.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/NamedTextures.h"
#include "Rendering/Fonts/glFont.h"
#include "Game/CameraHandler.h"
#include "System/Config/ConfigHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/GL/myGL.h"
#include "System/Platform/Threading.h"
#include "System/SplashScreen.hpp"
#include "aGui/Gui.h"

DEFINE_bool     (fullscreen,                               false, "Run in fullscreen mode");
DEFINE_bool     (window,                                   false, "Run in windowed mode");
DEFINE_bool     (hidden,                                   false, "Start in background (minimised, no taskbar entry)");

GLRenderer::GLRenderer() {}

void GLRenderer::SetPreGameScene(CPreGame* pPreGame) {
    SetScene(new GLRPreGameScene(pPreGame));
}
void GLRenderer::SetLuaMenuScene(CLuaMenuController* pLuaMenuController) {
	SetScene(new GLRLuaMenuScene(pLuaMenuController));
}

/**
 * @return whether window initialization succeeded
 * @param title char* string with window title
 *
 * Initializes the game window
 */
bool GLRenderer::InitWindow(const char* title) {
	CGlobalRendering::InitStatic();
	globalRendering->SetFullScreen(FLAGS_window, FLAGS_fullscreen);

    // SDL will cause a creation of gpu-driver thread that will clone its name from the starting threads (= this one = mainthread)
	Threading::SetThreadName("gpu-driver");

	// raises an error-prompt in case of failure
	if (!globalRendering->CreateWindowAndContext(title, FLAGS_hidden)) {
        SDL_Quit();
		return false;
    }

	// Something in SDL_SetVideoMode (OpenGL drivers?) messes with the FPU control word.
	// Set single precision floating point math.
	streflop::streflop_init<streflop::Simple>();

	// any other thread spawned from the main-process should be `unknown`
	Threading::SetThreadName("unknown");
	return true;
}

void GLRenderer::PostWindowInit() {
    globalRendering->PostInit();
	globalRendering->UpdateGLConfigs();
	globalRendering->UpdateGLGeometry();
	globalRendering->InitGLState();

	GL::SetAttribStatePointer(true);
	GL::SetMatrixStatePointer(true);

    CCameraHandler::InitStatic();
    CBitmap::InitPool(configHandler->GetInt("TextureMemPoolSize"));
    
    UpdateInterfaceGeometry();
    CglFont::LoadConfigFonts();

    ClearScreen();

	input.AddHandler(std::bind(&GLRenderer::MainEventHandler, this, std::placeholders::_1));

	agui::gui = new agui::Gui();

    CNamedTextures::Init();
	LuaOpenGL::Init();
}

void GLRenderer::Reload() {
    CNamedTextures::Kill();
	CNamedTextures::Init();

	shaderHandler->ClearShaders(false);

	LuaOpenGL::Free();
	LuaOpenGL::Init();

    #if 0
	// rely only on defrag for now since WindowManagerHelper also keeps a global bitmap
	CglFont::ReallocAtlases(true);
	CBitmap::InitPool(configHandler->GetInt("TextureMemPoolSize"));
	CglFont::ReallocAtlases(false);
	#else
	CBitmap::InitPool(configHandler->GetInt("TextureMemPoolSize"));
	#endif
}

void GLRenderer::Kill() {
    spring::SafeDelete(agui::gui);

	LOG("[SpringApp::%s][4] font=%p", __func__, font);

    spring::SafeDelete(font);
	spring::SafeDelete(smallFont);

    CNamedTextures::Kill(true);
    CCameraHandler::KillStatic();

	shaderHandler->ClearAll();
    
    CGlobalRendering::KillStatic();
}

void GLRenderer::UpdateInterfaceGeometry() {
    const int vpx = globalRendering->viewPosX;
	const int vpy = globalRendering->winSizeY - globalRendering->viewSizeY - globalRendering->viewPosY;

	agui::gui->UpdateScreenGeometry(globalRendering->viewSizeX, globalRendering->viewSizeY, vpx, vpy);
}

void GLRenderer::SaveWindowPosAndSize() {
	globalRendering->ReadWindowPosAndSize();
	globalRendering->SaveWindowPosAndSize();
}

void GLRenderer::_ShowSplashScreen(const std::string& splashScreenFile, const std::string& springVersionStr, const std::function<bool()>& testDoneFunc) {
    ShowSplashScreen(splashScreenFile, springVersionStr, testDoneFunc);
}

bool GLRenderer::MainEventHandler(const SDL_Event& event) {
	CGameController* activeController = CGameController::GetActiveController();
	switch (event.type) {
		case SDL_WINDOWEVENT: {
			switch (event.window.event) {
				case SDL_WINDOWEVENT_MOVED: {
					SaveWindowPosAndSize();
				} break;
				// case SDL_WINDOWEVENT_RESIZED: // always preceded by CHANGED
				case SDL_WINDOWEVENT_SIZE_CHANGED: {
					LOG("%s", "");
					LOG("[SpringApp::%s][SDL_WINDOWEVENT_SIZE_CHANGED][1] fullScreen=%d", __func__, globalRendering->fullScreen);

					Watchdog::ClearTimer(WDT_MAIN, true);

					{
						ScopedOnceTimer timer("GlobalRendering::UpdateGL");

						SaveWindowPosAndSize();
						globalRendering->UpdateGLConfigs();
						globalRendering->UpdateGLGeometry();
						globalRendering->InitGLState();
						UpdateInterfaceGeometry();
					}
					{
						ScopedOnceTimer timer("ActiveController::ResizeEvent");

						activeController->ResizeEvent();
						mouseInput->InstallWndCallback();
					}

					LOG("[SpringApp::%s][SDL_WINDOWEVENT_SIZE_CHANGED][2]\n", __func__);
				} break;
				case SDL_WINDOWEVENT_MAXIMIZED:
				case SDL_WINDOWEVENT_RESTORED:
				case SDL_WINDOWEVENT_SHOWN: {
					LOG("%s", "");
					LOG("[SpringApp::%s][SDL_WINDOWEVENT_SHOWN][1] fullScreen=%d", __func__, globalRendering->fullScreen);

					// reactivate sounds and other
					globalRendering->active = true;

					if (ISound::IsInitialized()) {
						ScopedOnceTimer timer("Sound::Iconified");
						sound->Iconified(false);
					}

					if (globalRendering->fullScreen) {
						ScopedOnceTimer timer("FBO::GLContextReinit");
						FBO::GLContextReinit();
					}

					LOG("[SpringApp::%s][SDL_WINDOWEVENT_SHOWN][2]\n", __func__);
				} break;
				case SDL_WINDOWEVENT_MINIMIZED:
				case SDL_WINDOWEVENT_HIDDEN: {
					LOG("%s", "");
					LOG("[SpringApp::%s][SDL_WINDOWEVENT_HIDDEN][1] fullScreen=%d", __func__, globalRendering->fullScreen);

					// deactivate sounds and other
					globalRendering->active = false;

					if (ISound::IsInitialized()) {
						ScopedOnceTimer timer("Sound::Iconified");
						sound->Iconified(true);
					}

					if (globalRendering->fullScreen) {
						ScopedOnceTimer timer("FBO::GLContextLost");
						FBO::GLContextLost();
					}

					LOG("[SpringApp::%s][SDL_WINDOWEVENT_HIDDEN][2]\n", __func__);
				} break;

				case SDL_WINDOWEVENT_FOCUS_GAINED: {
					// update keydown table
					KeyInput::Update(0, keyBindings.GetFakeMetaKey());
				} break;
				case SDL_WINDOWEVENT_FOCUS_LOST: {
					Watchdog::ClearTimer(WDT_MAIN, true);

					// SDL has some bug and does not update modstate on alt+tab/minimize etc.
					//FIXME check if still happens with SDL2 (2013)
					SDL_SetModState((SDL_Keymod)(SDL_GetModState() & (KMOD_NUM | KMOD_CAPS | KMOD_MODE)));

					// release all keyboard keys
					KeyInput::ReleaseAllKeys();

					if (mouse != nullptr) {
						// simulate mouse release to prevent hung buttons
						for (int i = 1; i <= NUM_BUTTONS; ++i) {
							if (!mouse->buttons[i].pressed)
								continue;

							SDL_Event event;
							event.type = event.button.type = SDL_MOUSEBUTTONUP;
							event.button.state = SDL_RELEASED;
							event.button.which = 0;
							event.button.button = i;
							event.button.x = -1;
							event.button.y = -1;
							SDL_PushEvent(&event);
						}

						// unlock mouse
						if (mouse->locked)
							mouse->ToggleMiddleClickScroll();
					}

					// and make sure to un-capture mouse
					globalRendering->SetWindowInputGrabbing(false);
				} break;

				case SDL_WINDOWEVENT_CLOSE: {
					gu->globalQuit = true;
				} break;
			};
		} break;
		case SDL_QUIT: {
			gu->globalQuit = true;
		} break;
		case SDL_TEXTEDITING: {
			if (activeController != nullptr)
				activeController->TextEditing(event.edit.text, event.edit.start, event.edit.length);

		} break;
		case SDL_TEXTINPUT: {
			if (activeController != nullptr)
				activeController->TextInput(event.text.text);

		} break;
		case SDL_KEYDOWN: {
			KeyInput::Update(event.key.keysym.sym, keyBindings.GetFakeMetaKey());

			if (activeController != nullptr)
				activeController->KeyPressed(KeyInput::GetNormalizedKeySymbol(event.key.keysym.sym), event.key.repeat);

		} break;
		case SDL_KEYUP: {
			KeyInput::Update(event.key.keysym.sym, keyBindings.GetFakeMetaKey());

			if (activeController != nullptr) {
				gameTextInput.ignoreNextChar = false;
				activeController->KeyReleased(KeyInput::GetNormalizedKeySymbol(event.key.keysym.sym));
			}
		} break;
	};

	return false;
}