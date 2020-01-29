#include "pxr/rprImaging/rprEngine/engine.h"

#include "pxr/usd/usd/stage.h"

#include <stdio.h>

int main(int ac, char** av) {
    if (ac != 2) {
        printf("Usage: %s file.usd\n", av[0]);
        return 1;
    }

    PXR_NAMESPACE_USING_DIRECTIVE

    auto stage = UsdStage::Open(av[1]);
    if (!stage) {
        printf("Failed to open stage at \"%s\"\n", av[1]);
    }

    auto rootPrim = stage->GetPseudoRoot();

    auto renderPlugins = HdRprEngine::GetRendererPlugins();
    if (renderPlugins.empty()) {
        printf("Could not find any renderer plugin\n");
        return 1;
    }

    auto selectedPlugin = renderPlugins.front();
    for (auto& id : renderPlugins) {
        auto displayName = HdRprEngine::GetRendererDisplayName(id);
        printf("%s\n", displayName.c_str());
        if (std::strstr(displayName.c_str(), "RPR")) {
            selectedPlugin = id;
        }
    }
    printf("selectedPlugin: %s\n", selectedPlugin.GetText());

    HdRprEngine engine;
    if (!engine.SetRendererPlugin(selectedPlugin)) {
        printf("Failed to select render plugin\n");
        return 1;
    }

    engine.SetRendererAovs({HdAovTokens->depth, HdAovTokens->color});

    engine.SetRenderViewport({0.0, 0.0, 1024.0, 1024.0});
    engine.SetCameraState(GfMatrix4d(1.0), GfMatrix4d(1.0));

    HdRprEngineRenderParams params;
    do {
        engine.Render(rootPrim, params);
    } while (!engine.IsConverged());

    if (auto colorAov = engine.GetAovBuffer(HdAovTokens->color)) {
        auto mappedMemory = colorAov->Map();

        // Save image

        colorAov->Unmap();
    } else {
        printf("Failed to get color AOV buffer\n");
    }

    return 0;
}
