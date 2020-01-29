#ifndef HDRPR_ENGINE_RENDER_PARAMS_H
#define HDRPR_ENGINE_RENDER_PARAMS_H

#include "pxr/usd/usd/timeCode.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/tf/token.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdRprEngineRenderParams
///
/// Used as an arguments class for various methods in HdRprEngine.
///
struct HdRprEngineRenderParams {
    using ClipPlanesVector = std::vector<GfVec4d>;

    UsdTimeCode frame = UsdTimeCode::Default();
    int refineLevel = 1;
    ClipPlanesVector clipPlanes;
    bool enableSceneMaterials = true;
    bool enableUsdDrawModes = true;
    GfVec4f clearColor = GfVec4f(0, 0, 0, 1);

    bool operator==(const HdRprEngineRenderParams &other) const {
        return frame                == other.frame &&
               refineLevel          == other.refineLevel &&
               clipPlanes           == other.clipPlanes &&
               enableSceneMaterials == other.enableSceneMaterials &&
               enableUsdDrawModes   == other.enableUsdDrawModes &&
               clearColor           == other.clearColor;
    }

    bool operator!=(const HdRprEngineRenderParams &other) const { return !(*this == other); }
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_ENGINE_RENDER_PARAMS_H
