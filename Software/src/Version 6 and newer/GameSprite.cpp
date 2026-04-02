/******************************************************************************************************************************
 *  Shared game sprite for Morserino-32 Pocket games
 *****************************************************************************************************************************/

#ifdef CONFIG_CW_GAME

#include "GameSprite.h"

LGFX_Sprite* gameCanvas = nullptr;

bool gameCanvasInit(lgfx::LGFX_Device* tft, int w, int h) {
    if (!tft) return false;
    
    if (!gameCanvas) {
        gameCanvas = new LGFX_Sprite(tft);
        if (!gameCanvas) return false;
        gameCanvas->setPsram(false);
        gameCanvas->setColorDepth(16);
        if (!gameCanvas->createSprite(w, h)) {
            delete gameCanvas;
            gameCanvas = nullptr;
            return false;
        }
    }
    // Sprite exists — just clear it
    gameCanvas->fillSprite(0);
    return true;
}

#endif
