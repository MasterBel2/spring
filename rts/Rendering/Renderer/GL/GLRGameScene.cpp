/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "Rendering/Renderer/GL/GLRGameScene.hpp"

#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/Game.h"
#include "Game/GlobalUnsynced.h"
#include "Game/InMapDraw.h"
#include "Game/IVideoCapturing.h"
#include "Game/Players/PlayerHandler.h"
#include "Game/UI/GuiHandler.h"
#include "Game/UI/InfoConsole.h"
#include "Game/UI/MiniMap.h"
#include "Game/UI/MouseHandler.h"
#include "Game/UI/PlayerRosterDrawer.h"
#include "Game/UI/UnitTracker.h"

#include "Lua/LuaGaia.h"
#include "Lua/LuaInputReceiver.h"
#include "Lua/LuaRules.h"

#include "Net/Protocol/NetProtocol.h"

#include "Rendering/CommandDrawer.h"
#include "Rendering/DebugDrawerAI.h"
#include "Rendering/Fonts/glFont.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/HUDDrawer.h"
#include "Rendering/IconHandler.h"
#include "Rendering/LineDrawer.h"
#include "Rendering/Map/InfoTexture/IInfoTextureHandler.h"
#include "Rendering/TeamHighlight.h"
#include "Rendering/Textures/NamedTextures.h"
#include "Rendering/UnitDrawer.h"
#include "Rendering/WorldDrawer.h"

#include "Sim/Misc/GeometricObjects.h"
#include "Sim/Misc/GlobalSynced.h"

#include "System/EventHandler.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"
#include "System/SafeUtil.h"
#include "System/Sound/ISound.h"
#include "System/TimeProfiler.h"

#include "Lua/LuaUI.h"



GLRGameScene::GLRGameScene(CGame* pGame) : pGame(pGame) {
    geometricObjects = new CGeometricObjects();

    // load components that need to exist before PostLoadSimulation (InitPre)
    worldDrawer.InitPre();

    // These can load after PostLoadSimulation
    worldDrawer.InitPost();
}

bool GLRGameScene::Draw() {
    const spring_time currentTimePreUpdate = spring_gettime();

    if (UpdateUnsynced(currentTimePreUpdate))
        return false;

    const spring_time currentTimePreDraw = spring_gettime();

    SCOPED_SPECIAL_TIMER("Draw");
    globalRendering->SetGLTimeStamp(CGlobalRendering::FRAME_REF_TIME_QUERY_IDX);

    pGame->SetDrawMode(Game::NormalDraw);

    {
        SCOPED_TIMER("Draw::DrawGenesis");
        eventHandler.DrawGenesis();
    }

    if (!globalRendering->active) {
        spring_sleep(spring_msecs(10));

        // return early if and only if less than 30K milliseconds have passed since last draw-frame
        // so we force render two frames per minute when minimized to clear batches and free memory
        // don't need to mess with globalRendering->active since only mouse-input code depends on it
        if ((currentTimePreDraw - pGame->lastDrawFrameTime).toSecsi() < 30)
            return false;
    }

    if (globalRendering->drawDebug) {
        const float deltaFrameTime = (currentTimePreUpdate - pGame->lastSimFrameTime).toMilliSecsf();
        const float deltaNetPacketProcTime  = (currentTimePreUpdate - pGame->lastNetPacketProcessTime ).toMilliSecsf();
        const float deltaReceivedPacketTime = (currentTimePreUpdate - pGame->lastReceivedNetPacketTime).toMilliSecsf();
        const float deltaSimFramePacketTime = (currentTimePreUpdate - pGame->lastSimFrameNetPacketTime).toMilliSecsf();

        const float currTimeOffset = globalRendering->timeOffset;
        static float lastTimeOffset = globalRendering->timeOffset;
        static int lastGameFrame = gs->frameNum;

        static const char* minFmtStr = "assert(CTO >= 0.0f) failed (SF=%u : DF=%u : CTO=%f : WSF=%f : DT=%fms : DLNPPT=%fms | DLRPT=%fms | DSFPT=%fms : NP=%u)";
        static const char* maxFmtStr = "assert(CTO <= 1.3f) failed (SF=%u : DF=%u : CTO=%f : WSF=%f : DT=%fms : DLNPPT=%fms | DLRPT=%fms | DSFPT=%fms : NP=%u)";

        // CTO = MILLISECSF(CT - LSFT) * WSF = MILLISECSF(CT - LSFT) * (SFPS * 0.001)
        // AT 30Hz LHS (MILLISECSF(CT - LSFT)) SHOULD BE ~33ms, RHS SHOULD BE ~0.03
        assert(currTimeOffset >= 0.0f);

        if (currTimeOffset < 0.0f) LOG_L(L_DEBUG, minFmtStr, gs->frameNum, globalRendering->drawFrame, currTimeOffset, globalRendering->weightedSpeedFactor, deltaFrameTime, deltaNetPacketProcTime, deltaReceivedPacketTime, deltaSimFramePacketTime, clientNet->GetNumWaitingServerPackets());
        if (currTimeOffset > 1.3f) LOG_L(L_DEBUG, maxFmtStr, gs->frameNum, globalRendering->drawFrame, currTimeOffset, globalRendering->weightedSpeedFactor, deltaFrameTime, deltaNetPacketProcTime, deltaReceivedPacketTime, deltaSimFramePacketTime, clientNet->GetNumWaitingServerPackets());

        // test for monotonicity, normally should only fail
        // when SimFrame() advances time or if simframe rate
        // changes
        if (lastGameFrame == gs->frameNum && currTimeOffset < lastTimeOffset)
            LOG_L(L_DEBUG, "assert(CTO >= LTO) failed (SF=%u : DF=%u : CTO=%f : LTO=%f : WSF=%f : DT=%fms)", gs->frameNum, globalRendering->drawFrame, currTimeOffset, lastTimeOffset, globalRendering->weightedSpeedFactor, deltaFrameTime);

        lastTimeOffset = currTimeOffset;
        lastGameFrame = gs->frameNum;
    }

    //FIXME move both to UpdateUnsynced?
    CTeamHighlight::Enable(spring_tomsecs(currentTimePreDraw));
    if (unitTracker.Enabled())
        unitTracker.SetCam();

    {                  
        minimap->Update();

        // note: neither this call nor DrawWorld can be made conditional on minimap->GetMaximized()
        // minimap never covers entire screen when maximized unless map aspect-ratio matches screen
        // (unlikely); the minimap update also depends on GenerateIBLTextures for unbinding its FBO
        worldDrawer.GenerateIBLTextures();

        camera->Update();

        worldDrawer.Draw();
        worldDrawer.SetupScreenState();
    }

    {
        SCOPED_TIMER("Draw::Screen");

        eventHandler.DrawScreenEffects();

        hudDrawer->Draw((gu->GetMyPlayer())->fpsController.GetControllee());
        debugDrawerAI->Draw();

        DrawInputReceivers();
        DrawInputText();
        DrawInterfaceWidgets();
        mouse->DrawCursor();

        eventHandler.DrawScreenPost();
    }

    glAttribStatePtr->EnableDepthTest();

    if (videoCapturing->AllowRecord()) {
        videoCapturing->SetLastFrameTime(globalRendering->lastFrameTime = 1000.0f / GAME_SPEED);
        // does nothing unless StartCapturing has also been called via /createvideo (Windows-only)
        videoCapturing->RenderFrame();
    }

    pGame->SetDrawMode(Game::NotDrawing);
    CTeamHighlight::Disable();

    const spring_time currentTimePostDraw = spring_gettime();
    const spring_time currentFrameDrawTime = currentTimePostDraw - currentTimePreDraw;
    gu->avgDrawFrameTime = mix(gu->avgDrawFrameTime, currentFrameDrawTime.toMilliSecsf(), 0.05f);

    eventHandler.DbgTimingInfo(TIMING_VIDEO, currentTimePreDraw, currentTimePostDraw);
    globalRendering->SetGLTimeStamp(CGlobalRendering::FRAME_END_TIME_QUERY_IDX);

    return true;
}

bool GLRGameScene::UpdateUnsynced(const spring_time currentTime)
{
    SCOPED_TIMER("Update");

    // timings and frame interpolation
    const spring_time deltaDrawFrameTime = currentTime - pGame->lastDrawFrameTime;

    const float modGameDeltaTimeSecs = mix(deltaDrawFrameTime.toMilliSecsf() * 0.001f, 0.01f, pGame->skipping);
    const float unsyncedUpdateDeltaTime = (currentTime - pGame->lastUnsyncedUpdateTime).toSecsf();

    pGame->lastDrawFrameTime = currentTime;

    {
        // update game timings
        globalRendering->lastFrameStart = currentTime;
        globalRendering->lastFrameTime = deltaDrawFrameTime.toMilliSecsf();

        gu->avgFrameTime = mix(gu->avgFrameTime, deltaDrawFrameTime.toMilliSecsf(), 0.05f);
        gu->gameTime += modGameDeltaTimeSecs;
        gu->modGameTime += (modGameDeltaTimeSecs * gs->speedFactor * (1 - gs->paused));

        pGame->totalGameTime += (modGameDeltaTimeSecs * (pGame->playing && !pGame->gameOver));
        pGame->updateDeltaSeconds = modGameDeltaTimeSecs;
    }

    {
        // update sim-FPS counter once per second
        static int lsf = gs->frameNum;
        static spring_time lsft = currentTime;

        // toSecsf throws away too much precision
        const float diffMilliSecs = (currentTime - lsft).toMilliSecsf();

        if (diffMilliSecs >= 1000.0f) {
            gu->simFPS = (gs->frameNum - lsf) / (diffMilliSecs * 0.001f);
            lsft = currentTime;
            lsf = gs->frameNum;
        }
    }

    if (pGame->skipping) {
        // when fast-forwarding, maintain a draw-rate of 2Hz
        if (spring_tomsecs(currentTime - pGame->skipLastDrawTime) < 500.0f)
            return true;

        pGame->skipLastDrawTime = currentTime;
        return true;
    }

    pGame->numDrawFrames++;

    // Update the interpolation coefficient (globalRendering->timeOffset)
    if (!gs->paused && !pGame->IsSimLagging() && !gs->PreSimFrame() && !videoCapturing->AllowRecord()) {
        globalRendering->weightedSpeedFactor = 0.001f * gu->simFPS;
        globalRendering->timeOffset = (currentTime - pGame->lastFrameTime).toMilliSecsf() * globalRendering->weightedSpeedFactor;
    } else {
        globalRendering->timeOffset = videoCapturing->GetTimeOffset();

        pGame->lastSimFrameTime = currentTime;
        pGame->lastFrameTime = currentTime;
    }

    if ((currentTime - pGame->frameStartTime).toMilliSecsf() >= 1000.0f) {
        globalRendering->FPS = (pGame->numDrawFrames * 1000.0f) / std::max(0.01f, (currentTime - pGame->frameStartTime).toMilliSecsf());

        // update draw-FPS counter once every second
        pGame->frameStartTime = currentTime;
        pGame->numDrawFrames = 0;

    }

    const bool newSimFrame = (pGame->lastSimFrame != gs->frameNum);
    const bool forceUpdate = (unsyncedUpdateDeltaTime >= (1.0f / GAME_SPEED));

    pGame->lastSimFrame = gs->frameNum;

    // set camera
    camHandler->UpdateController(playerHandler.Player(gu->myPlayerNum), gu->fpsMode, pGame->fullscreenEdgeMove, pGame->windowedEdgeMove);

    unitDrawer->Update();
    lineDrawer.UpdateLineStipple();


    {
        worldDrawer.Update(newSimFrame);

        CNamedTextures::Update();
        CFontTexture::Update();
    }

    // always update InfoTexture and SoundListener at <= 30Hz (even when paused)
    if (newSimFrame || forceUpdate) {
        pGame->lastUnsyncedUpdateTime = currentTime;

        // TODO: should be moved to WorldDrawer::Update
        infoTextureHandler->Update();
        // TODO call only when camera changed
        sound->UpdateListener(camera->GetPos(), camera->GetDir(), camera->GetUp());
    }


    if (luaUI != nullptr) {
        luaUI->CheckStack();
        luaUI->CheckAction();
    }
    if (luaGaia != nullptr)
        luaGaia->CheckStack();
    if (luaRules != nullptr)
        luaRules->CheckStack();


    if (gameTextInput.SendPromptInput()) {
        gameConsoleHistory.AddLine(gameTextInput.userInput);
        pGame->SendNetChat(gameTextInput.userInput);
        gameTextInput.ClearInput();
    }
    if (inMapDrawer->IsWantLabel() && gameTextInput.SendLabelInput())
        gameTextInput.ClearInput();

    infoConsole->PushNewLinesToEventHandler();
    infoConsole->Update();

    mouse->Update();
    mouse->UpdateCursors();
    guihandler->Update();
    commandDrawer->Update();

    {
        SCOPED_TIMER("Update::EventHandler");
        eventHandler.Update();
    }
    eventHandler.DbgTimingInfo(TIMING_UNSYNCED, currentTime, spring_now());
    return false;
}

void GLRGameScene::DrawInputReceivers()
{
    if (!pGame->hideInterface) {
        {
            SCOPED_TIMER("Draw::Screen::InputReceivers");
            CInputReceiver::DrawReceivers();
        }
        {
            // this has MANUAL ordering, draw it last (front-most)
            SCOPED_TIMER("Draw::Screen::DrawScreen");
            luaInputReceiver->Draw();
        }
    } else {
        SCOPED_TIMER("Draw::Screen::Minimap");

        if (globalRendering->dualScreenMode) {
            // minimap is on its own screen, so always draw it
            minimap->Draw();
        }
    }
}
void GLRGameScene::DrawInterfaceWidgets()
{
    if (pGame->hideInterface)
        return;

    #define KEY_FONT_FLAGS (FONT_SCALE | FONT_CENTER | FONT_NORM)
    #define INF_FONT_FLAGS (FONT_RIGHT | FONT_SCALE | FONT_NORM | (FONT_OUTLINE * guihandler->GetOutlineFonts()))

    if (pGame->showClock) {
        const int seconds = gs->frameNum / GAME_SPEED;
        const int minutes = seconds / 60;

        smallFont->SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);

        if (seconds < 3600) {
            smallFont->glFormat(0.99f, 0.94f, 1.0f, INF_FONT_FLAGS | FONT_BUFFERED, "%02i:%02i", minutes, seconds % 60);
        } else {
            smallFont->glFormat(0.99f, 0.94f, 1.0f, INF_FONT_FLAGS | FONT_BUFFERED, "%02i:%02i:%02i", seconds / 3600, minutes % 60, seconds % 60);
        }
    }

    if (pGame->showFPS) {
        smallFont->SetTextColor(1.0f, 1.0f, 0.25f, 1.0f);
        smallFont->glFormat(0.99f, 0.92f, 1.0f, INF_FONT_FLAGS | FONT_BUFFERED, "%.0f", globalRendering->FPS);
    }

    if (pGame->showSpeed) {
        smallFont->SetTextColor(1.0f, (gs->speedFactor < gs->wantedSpeedFactor * 0.99f)? 0.25f : 1.0f, 0.25f, 1.0f);
        smallFont->glFormat(0.99f, 0.90f, 1.0f, INF_FONT_FLAGS | FONT_BUFFERED, "%2.2f", gs->speedFactor);
    }

    CPlayerRosterDrawer::Draw();
    smallFont->DrawBufferedGL4();
}

void GLRGameScene::DrawInputText()
{
    gameTextInput.Draw();
}

void GLRGameScene::DrawSkip(bool blackscreen) {
    #if 0
    const int framesLeft = (skipEndFrame - gs->frameNum);

    if (blackscreen) {
        glAttribStatePtr->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glAttribStatePtr->Clear(GL_COLOR_BUFFER_BIT);
    }

    font->SetTextColor(0.5f, 1.0f, 0.5f, 1.0f);
    font->glFormat(0.5f, 0.55f, 2.5f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Skipping %.1f game seconds", skipSeconds);
    font->SetTextColor(1.0f, 1.0f, 1.0f, 1.0f);
    font->glFormat(0.5f, 0.45f, 2.0f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "(%i frames left)", framesLeft);
    font->DrawBufferedGL4();

    const float b = 0.004f; // border
    const float yn = 0.35f;
    const float yp = 0.38f;
    const float ff = (float)framesLeft / (float)skipTotalFrames;

    glColor3f(0.2f, 0.2f, 1.0f);
    glRectf(0.25f - b, yn - b, 0.75f + b, yp + b);
    glColor3f(0.25f + (0.75f * ff), 1.0f - (0.75f * ff), 0.0f);
    glRectf(0.5 - (0.25f * ff), yn, 0.5f + (0.25f * ff), yp);
    #endif
}

void GLRGameScene::End() {
    LOG("[GLRGameScene::%s][1]", __func__);
    icon::iconHandler.Kill();
    spring::SafeDelete(geometricObjects);
    worldDrawer.Kill();
}
