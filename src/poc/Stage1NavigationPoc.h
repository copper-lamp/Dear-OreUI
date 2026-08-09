#pragma once

namespace OreUI {
class Router;
}

class ISceneStack;

namespace dearoreui::poc {

void armStage1Navigation(OreUI::Router& router, ISceneStack& sceneStack, bool isOutOfGameRootScene);
void armStage1NavigationFromStartScreen(OreUI::Router& router);
void consumeStage1Navigation();
void registerRouter(OreUI::Router& router, ISceneStack& sceneStack);
void invalidateRouter(OreUI::Router& router);
void invalidateSceneStack(ISceneStack& sceneStack);
void stopStage1Navigation();

}
