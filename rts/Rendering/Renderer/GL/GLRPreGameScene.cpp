#include "Rendering/Renderer/GL/GLRPreGameScene.h"
#include "Net/Protocol/NetProtocol.h"

#include "Game/PreGame.h"
#include "Rendering/Fonts/glFont.h"
#include "aGui/Gui.h"
#include "Game/ClientSetup.h"

bool GLRPreGameScene::Draw() {
    spring_msecs(10).sleep(true);
	ClearScreen();

	if (!clientNet->Connected()) {
		if (pPreGame->clientSetup->isHost)
			font->glFormat(0.5f, 0.48f, 2.0f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Waiting for server to start");
		else
			font->glFormat(0.5f, 0.48f, 2.0f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Connecting to server (%ds)", (spring_gettime() - pPreGame->connectTimer).toSecsi());
	} else {
		font->glPrint(0.5f, 0.48f, 2.0f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Waiting for server response");
	}

	font->glFormat(0.60f, 0.40f, 1.0f, FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Connecting to: %s", clientNet->ConnectionStr().c_str());
	font->glFormat(0.60f, 0.35f, 1.0f, FONT_SCALE | FONT_NORM | FONT_BUFFERED, "User name: %s", pPreGame->clientSetup->myPlayerName.c_str());

	font->glFormat(0.5f, 0.25f, 0.8f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Press SHIFT + ESC to quit");
	// credits
	font->glFormat(0.5f, 0.06f, 1.0f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "Spring %s", SpringVersion::GetFull().c_str());
	font->glPrint(0.5f, 0.02f, 0.6f, FONT_CENTER | FONT_SCALE | FONT_NORM | FONT_BUFFERED, "This program is distributed under the GNU General Public License, see doc/LICENSE for more info");
	font->DrawBufferedGL4();

	return true;
}

void GLRPreGameScene::End() {
    agui::gui->Clean();
}