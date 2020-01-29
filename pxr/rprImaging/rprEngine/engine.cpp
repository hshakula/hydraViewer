#include "pxr/rprImaging/rprEngine/engine.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"
#include "pxr/base/tf/getenv.h"

PXR_NAMESPACE_OPEN_SCOPE

//----------------------------------------------------------------------------
// Construction
//----------------------------------------------------------------------------

HdRprEngine::HdRprEngine(
    const SdfPath& rootPath,
    const SdfPathVector& excludedPaths,
    const SdfPathVector& invisedPaths,
    const SdfPath& delegateID)
    : m_renderIndex(nullptr)
    // , _selTracker(new HdxSelectionTracker)
    , m_delegateID(delegateID)
    , m_delegate(nullptr)
    , m_rendererPlugin(nullptr)
    , m_taskController(nullptr)
    // , _selectionColor(1.0f, 1.0f, 0.0f, 1.0f)
    , m_rootPath(rootPath)
    , m_excludedPrimPaths(excludedPaths)
    , m_invisedPrimPaths(invisedPaths)
    , m_isPopulated(false) {
    // m_renderIndex, m_taskController, and m_delegate are initialized
    // by the plugin system.
    if (!SetRendererPlugin(_GetDefaultRendererPluginId())) {
        TF_CODING_ERROR("No renderer plugins found! "
                        "Check before creation.");
    }
}

HdRprEngine::~HdRprEngine() { 
    _DeleteHydraResources();
}

//----------------------------------------------------------------------------
// Rendering
//----------------------------------------------------------------------------

void HdRprEngine::PrepareBatch(
    const UsdPrim& root,
    const HdRprEngineRenderParams& params) {
    HD_TRACE_FUNCTION();

    TF_VERIFY(m_delegate);

    if (_CanPrepareBatch(root, params)) {
        if (!m_isPopulated) {
            m_delegate->SetUsdDrawModesEnabled(params.enableUsdDrawModes);
            m_delegate->Populate(root.GetStage()->GetPrimAtPath(m_rootPath), m_excludedPrimPaths);
            m_delegate->SetInvisedPrimPaths(m_invisedPrimPaths);
            m_isPopulated = true;
        }

        // Set the fallback refine level, if this changes from the existing value,
        // all prim refine levels will be dirtied.
        m_delegate->SetRefineLevelFallback(params.refineLevel);

        // TODO: recheck me
        // // Apply any queued up scene edits.
        // m_delegate->ApplyPendingUpdates();

        // SetTime will only react if time actually changes.
        m_delegate->SetTime(params.frame);

        // Apply any queued up scene edits.
        m_delegate->ApplyPendingUpdates();
    }
}

void HdRprEngine::RenderBatch(
    const SdfPathVector& paths, 
    const HdRprEngineRenderParams& params) {
    TF_VERIFY(m_taskController);

    m_taskController->SetFreeCameraClipPlanes(params.clipPlanes);
    _UpdateHydraCollection(&m_renderCollection, paths, params);
    m_taskController->SetCollection(m_renderCollection);

    TfTokenVector renderTags;
    _ComputeRenderTags(params, &renderTags);
    m_taskController->SetRenderTags(renderTags);

    HdxRenderTaskParams hdParams = _MakeHydraHdRprEngineRenderParams(params);
    m_taskController->SetRenderParams(hdParams);
    m_taskController->SetEnableSelection(false); // params.highlight

    // SetColorCorrectionSettings(params.colorCorrectionMode, 
    //                            params.renderResolution);

    // XXX App sets the clear color via 'params' instead of setting up Aovs 
    // that has clearColor in their descriptor. So for now we must pass this
    // clear color to the color AOV.
    HdAovDescriptor colorAovDesc = 
        m_taskController->GetRenderOutputSettings(HdAovTokens->color);
    if (colorAovDesc.format != HdFormatInvalid) {
        colorAovDesc.clearValue = VtValue(params.clearColor);
        m_taskController->SetRenderOutputSettings(
            HdAovTokens->color, colorAovDesc);
    }

    // Forward scene materials enable option to delegate
    m_delegate->SetSceneMaterialsEnabled(params.enableSceneMaterials);

    // VtValue selectionValue(_selTracker);
    // m_engine.SetTaskContextData(HdxTokens->selectionState, selectionValue);

    auto tasks = m_taskController->GetRenderingTasks();
    m_engine.Execute(m_renderIndex, &tasks);
}

void HdRprEngine::Render(
    const UsdPrim& root, 
    const HdRprEngineRenderParams &params) {
    TF_VERIFY(m_taskController);

    PrepareBatch(root, params);

    // XXX(UsdImagingPaths): Is it correct to map USD root path directly
    // to the cachePath here?
    SdfPath cachePath = root.GetPath();
    SdfPathVector paths(1, m_delegate->ConvertCachePathToIndexPath(cachePath));

    RenderBatch(paths, params);
}

bool HdRprEngine::IsConverged() const {
    TF_VERIFY(m_taskController);
    return m_taskController->IsConverged();
}

//----------------------------------------------------------------------------
// Camera State
//----------------------------------------------------------------------------

void HdRprEngine::SetRenderViewport(GfVec4d const& viewport) {
    TF_VERIFY(m_taskController);
    m_taskController->SetRenderViewport(viewport);
}

void HdRprEngine::SetWindowPolicy(CameraUtilConformWindowPolicy policy) {
    TF_VERIFY(m_taskController);
    // Note: Free cam uses SetCameraState, which expects the frustum to be
    // pre-adjusted for the viewport size.
    
    // The usdImagingDelegate manages the window policy for scene cameras.
    m_delegate->SetWindowPolicy(policy);
}

void HdRprEngine::SetCameraPath(SdfPath const& id) {
    TF_VERIFY(m_taskController);
    m_taskController->SetCameraPath(id);

    // The camera that is set for viewing will also be used for
    // time sampling.
    m_delegate->SetCameraForSampling(id);
}

void HdRprEngine::SetCameraState(
    const GfMatrix4d& viewMatrix,
    const GfMatrix4d& projectionMatrix) {
    TF_VERIFY(m_taskController);
    m_taskController->SetFreeCameraMatrices(viewMatrix, projectionMatrix);
}

//----------------------------------------------------------------------------
// Renderer Plugin Management
//----------------------------------------------------------------------------

/* static */
TfTokenVector HdRprEngine::GetRendererPlugins() {
    HfPluginDescVector pluginDescriptors;
    HdRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescriptors);

    TfTokenVector plugins;
    for(size_t i = 0; i < pluginDescriptors.size(); ++i) {
        plugins.push_back(pluginDescriptors[i].id);
    }
    return plugins;
}

/* static */
std::string HdRprEngine::GetRendererDisplayName(TfToken const &id) {
    if (id.IsEmpty()) {
        return std::string();
    }

    HfPluginDesc pluginDescriptor;
    if (!TF_VERIFY(HdRendererPluginRegistry::GetInstance().GetPluginDesc(id, &pluginDescriptor))) {
        return std::string();
    }

    return pluginDescriptor.displayName;
}

bool HdRprEngine::SetRendererPlugin(TfToken const &id) {
    HdRendererPlugin *plugin = nullptr;
    TfToken actualId = id;

    // Special case: TfToken() selects the first plugin in the list.
    if (actualId.IsEmpty()) {
        actualId = HdRendererPluginRegistry::GetInstance().GetDefaultPluginId();
    }
    plugin = HdRendererPluginRegistry::GetInstance().GetRendererPlugin(actualId);

    if (plugin == nullptr) {
        TF_CODING_ERROR("Couldn't find plugin for id %s", actualId.GetText());
        return false;
    } else if (plugin == m_rendererPlugin) {
        // It's a no-op to load the same plugin twice.
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return true;
    } else if (!plugin->IsSupported()) {
        // Don't do anything if the plugin isn't supported on the running
        // system, just return that we're not able to set it.
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return false;
    }

    HdRenderDelegate *renderDelegate = plugin->CreateRenderDelegate();
    if(!renderDelegate) {
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return false;
    }

    // Pull old delegate/task controller state.
    GfMatrix4d rootTransform = GfMatrix4d(1.0);
    bool isVisible = true;
    if (m_delegate != nullptr) {
        rootTransform = m_delegate->GetRootTransform();
        isVisible = m_delegate->GetRootVisibility();
    }
    // HdSelectionSharedPtr selection = m_selTracker->GetSelectionMap();
    // if (!selection) {
    //     selection.reset(new HdSelection);
    // }

    // Delete hydra state.
    _DeleteHydraResources();

    // Recreate the render index.
    m_rendererPlugin = plugin;
    m_rendererId = actualId;

    m_renderIndex = HdRenderIndex::New(renderDelegate);

    // Create the new delegate & task controller.
    m_delegate = new UsdImagingDelegate(m_renderIndex, m_delegateID);
    m_isPopulated = false;

    m_taskController = new HdxTaskController(m_renderIndex,
        m_delegateID.AppendChild(TfToken(TfStringPrintf(
            "_UsdImaging_%s_%p",
            TfMakeValidIdentifier(actualId.GetText()).c_str(),
            this))));

    // Rebuild state in the new delegate/task controller.
    m_delegate->SetRootVisibility(isVisible);
    m_delegate->SetRootTransform(rootTransform);
    // m_selTracker->SetSelection(selection);
    // m_taskController->SetSelectionColor(m_selectionColor);

    return true;
}

//----------------------------------------------------------------------------
// AOVs and Renderer Settings
//----------------------------------------------------------------------------

bool HdRprEngine::SetRendererAovs(TfTokenVector const &ids) {
    TF_VERIFY(m_renderIndex);
    if (m_renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {
        m_rendererAovs = TfTokenVector();
        auto renderDelegate = m_renderIndex->GetRenderDelegate();
        for (auto const& aov : ids) {
            if (renderDelegate->GetDefaultAovDescriptor(aov).format != HdFormatInvalid) {
                m_rendererAovs.push_back(aov);
            } else {
                TF_RUNTIME_ERROR("Could not set \"%s\" AOV: unsupported by render delegate\n", aov.GetText());
            }
        }

        m_taskController->SetRenderOutputs(m_rendererAovs);
        return true;
    }
    return false;
}

HdRenderBuffer* HdRprEngine::GetAovBuffer(TfToken const& id) {
    TF_VERIFY(m_taskController);
    return m_taskController->GetRenderOutput(id);
}

//----------------------------------------------------------------------------
// Private/Protected
//----------------------------------------------------------------------------

bool  HdRprEngine::_CanPrepareBatch(
    const UsdPrim& root, 
    const HdRprEngineRenderParams& params) {
    HD_TRACE_FUNCTION();

    if (!TF_VERIFY(root, "Attempting to draw an invalid/null prim\n")) 
        return false;

    if (!root.GetPath().HasPrefix(m_rootPath)) {
        TF_CODING_ERROR("Attempting to draw path <%s>, but engine is rooted at <%s>\n",
            root.GetPath().GetText(), m_rootPath.GetText());
        return false;
    }

    return true;
}

/* static */
bool HdRprEngine::_UpdateHydraCollection(
    HdRprimCollection *collection,
    SdfPathVector const& roots,
    HdRprEngineRenderParams const& params) {
    if (collection == nullptr) {
        TF_CODING_ERROR("Null passed to _UpdateHydraCollection");
        return false;
    }

    bool refined = params.refineLevel > 1;
    
    // Smooth shading
    auto reprSelector = HdReprSelector(refined ? HdReprTokens->refined : HdReprTokens->smoothHull);

    // By default our main collection will be called geometry
    TfToken colName = HdTokens->geometry;

    // Check if the collection needs to be updated (so we can avoid the sort).
    SdfPathVector const& oldRoots = collection->GetRootPaths();

    // inexpensive comparison first
    bool match = collection->GetName() == colName &&
                 oldRoots.size() == roots.size() &&
                 collection->GetReprSelector() == reprSelector;

    // Only take the time to compare root paths if everything else matches.
    if (match) {
        // Note that oldRoots is guaranteed to be sorted.
        for(size_t i = 0; i < roots.size(); i++) {
            // Avoid binary search when both vectors are sorted.
            if (oldRoots[i] == roots[i])
                continue;
            // Binary search to find the current root.
            if (!std::binary_search(oldRoots.begin(), oldRoots.end(), roots[i])) 
            {
                match = false;
                break;
            }
        }

        // if everything matches, do nothing.
        if (match) return false;
    }

    // Recreate the collection.
    *collection = HdRprimCollection(colName, reprSelector);
    collection->SetRootPaths(roots);

    return true;
}

/* static */
HdxRenderTaskParams HdRprEngine::_MakeHydraHdRprEngineRenderParams(
    HdRprEngineRenderParams const& renderParams) {
    HdxRenderTaskParams params;

    // params.overrideColor       = renderParams.overrideColor;
    // params.wireframeColor      = renderParams.wireframeColor;

    params.enableLighting = true;
    params.enableSceneMaterials = renderParams.enableSceneMaterials;

    // We don't provide the following because task controller ignores them:
    // - params.camera
    // - params.viewport

    return params;
}

/* static */
void HdRprEngine::_ComputeRenderTags(
    HdRprEngineRenderParams const& params,
    TfTokenVector *renderTags) {
    // Calculate the rendertags needed based on the parameters passed by
    // the application
    renderTags->clear();
    renderTags->reserve(2);
    renderTags->push_back(HdRenderTagTokens->geometry);
    // if (params.showGuides) {
    //     renderTags->push_back(HdRenderTagTokens->guide);
    // }
    // if (params.showProxy) {
    //     renderTags->push_back(HdRenderTagTokens->proxy);
    // }
    renderTags->push_back(HdRenderTagTokens->render);
}

void HdRprEngine::_DeleteHydraResources() {
    // Unwinding order: remove data sources first (task controller, scene
    // delegate); then render index; then render delegate; finally the
    // renderer plugin used to manage the render delegate.
    
    if (m_taskController != nullptr) {
        delete m_taskController;
        m_taskController = nullptr;
    }
    if (m_delegate != nullptr) {
        delete m_delegate;
        m_delegate = nullptr;
    }
    HdRenderDelegate* renderDelegate = nullptr;
    if (m_renderIndex != nullptr) {
        renderDelegate = m_renderIndex->GetRenderDelegate();
        delete m_renderIndex;
        m_renderIndex = nullptr;
    }
    if (m_rendererPlugin != nullptr) {
        if (renderDelegate != nullptr) {
            m_rendererPlugin->DeleteRenderDelegate(renderDelegate);
        }
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(m_rendererPlugin);
        m_rendererPlugin = nullptr;
        m_rendererId = TfToken();
    }
}

/* static */
TfToken HdRprEngine::_GetDefaultRendererPluginId() {
    std::string defaultRendererDisplayName = 
        TfGetenv("HD_DEFAULT_RENDERER", "");

    if (defaultRendererDisplayName.empty()) {
        return TfToken();
    }

    HfPluginDescVector pluginDescs;
    HdRendererPluginRegistry::GetInstance().GetPluginDescs(&pluginDescs);

    // Look for the one with the matching display name
    for (size_t i = 0; i < pluginDescs.size(); ++i) {
        if (pluginDescs[i].displayName == defaultRendererDisplayName) {
            return pluginDescs[i].id;
        }
    }

    TF_WARN("Failed to find default renderer with display name '%s'.",
            defaultRendererDisplayName.c_str());

    return TfToken();
}

PXR_NAMESPACE_CLOSE_SCOPE
