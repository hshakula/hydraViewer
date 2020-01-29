#ifndef HDRPR_ENGINE_H
#define HDRPR_ENGINE_H

#include "api.h"

#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/renderBuffer.h"
#include "pxr/imaging/hd/renderDelegate.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/rendererPlugin.h"

#include "pxr/imaging/hdx/taskController.h"
#include "pxr/imaging/hdx/renderSetupTask.h"

#include "pxr/usdImaging/usdImaging/delegate.h"

#include "pxr/rprImaging/rprEngine/renderParams.h"

#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprEngine {
public:

    // ---------------------------------------------------------------------
    /// \name Construction
    /// @{
    // ---------------------------------------------------------------------
    HDRPR_API
    HdRprEngine(const SdfPath& rootPath= SdfPath::AbsoluteRootPath(),
                const SdfPathVector& excludedPaths=SdfPathVector(),
                const SdfPathVector& invisedPaths=SdfPathVector(),
                const SdfPath& delegateID = SdfPath::AbsoluteRootPath());

    // Disallow copies
    HdRprEngine(const HdRprEngine&) = delete;
    HdRprEngine& operator=(const HdRprEngine&) = delete;

    HDRPR_API
    ~HdRprEngine();

    /// @}

    // ---------------------------------------------------------------------
    /// \name Rendering
    /// @{
    // ---------------------------------------------------------------------

    /// Support for batched drawing
    HDRPR_API
    void PrepareBatch(const UsdPrim& root, 
                      const HdRprEngineRenderParams& params);
    HDRPR_API
    void RenderBatch(const SdfPathVector& paths, 
                     const HdRprEngineRenderParams& params);

    /// Entry point for kicking off a render
    HDRPR_API
    void Render(const UsdPrim& root, 
                const HdRprEngineRenderParams &params);

    /// Returns true if the resulting image is fully converged.
    /// (otherwise, caller may need to call Render() again to refine the result)
    HDRPR_API
    bool IsConverged() const;

    /// @}

    // ---------------------------------------------------------------------
    /// \name Camera State
    /// @{
    // ---------------------------------------------------------------------
    
    /// Set the viewport to use for rendering as (x,y,w,h), where (x,y)
    /// represents the lower left corner of the viewport rectangle, and (w,h)
    /// is the width and height of the viewport in pixels.
    HDRPR_API
    void SetRenderViewport(GfVec4d const& viewport);

    /// Set the window policy to use.
    /// XXX: This is currently used for scene cameras set via SetCameraPath.
    /// See comment in SetCameraState for the free cam.
    HDRPR_API
    void SetWindowPolicy(CameraUtilConformWindowPolicy policy);
    
    /// Scene camera API
    /// Set the scene camera path to use for rendering.
    HDRPR_API
    void SetCameraPath(SdfPath const& id);

    /// Free camera API
    /// Set camera framing state directly (without pointing to a camera on the 
    /// USD stage). The projection matrix is expected to be pre-adjusted for the
    /// window policy.
    HDRPR_API
    void SetCameraState(const GfMatrix4d& viewMatrix,
                        const GfMatrix4d& projectionMatrix);

    // /// @}

    // // ---------------------------------------------------------------------
    // /// \name Selection Highlighting
    // /// @{
    // // ---------------------------------------------------------------------

    // /// Sets (replaces) the list of prim paths that should be included in 
    // /// selection highlighting. These paths may include root paths which will 
    // /// be expanded internally.
    // HDRPR_API
    // void SetSelected(SdfPathVector const& paths);

    // /// Clear the list of prim paths that should be included in selection
    // /// highlighting.
    // HDRPR_API
    // void ClearSelected();

    // /// Add a path with instanceIndex to the list of prim paths that should be
    // /// included in selection highlighting. UsdImagingDelegate::ALL_INSTANCES
    // /// can be used for highlighting all instances if path is an instancer.
    // HDRPR_API
    // void AddSelected(SdfPath const &path, int instanceIndex);

    // /// Sets the selection highlighting color.
    // HDRPR_API
    // void SetSelectionColor(GfVec4f const& color);

    /// @}
    
    // // ---------------------------------------------------------------------
    // /// \name Picking
    // /// @{
    // // ---------------------------------------------------------------------
    
    // /// Finds closest point of intersection with a frustum by rendering.
    // /// 
    // /// This method uses a PickRender and a customized depth buffer to find an
    // /// approximate point of intersection by rendering. This is less accurate
    // /// than implicit methods or rendering with GL_SELECT, but leverages any 
    // /// data already cached in the renderer.
    // ///
    // /// Returns whether a hit occurred and if so, \p outHitPoint will contain
    // /// the intersection point in world space (i.e. \p projectionMatrix and
    // /// \p viewMatrix factored back out of the result).
    // ///
    // HDRPR_API
    // bool TestIntersection(
    //     const GfMatrix4d &viewMatrix,
    //     const GfMatrix4d &projectionMatrix,
    //     const GfMatrix4d &worldToLocalSpace,
    //     const UsdPrim& root,
    //     const HdRprEngineRenderParams& params,
    //     GfVec3d *outHitPoint,
    //     SdfPath *outHitPrimPath = NULL,
    //     SdfPath *outHitInstancerPath = NULL,
    //     int *outHitInstanceIndex = NULL,
    //     int *outHitElementIndex = NULL);

    // /// Using an Id extracted from an Id render, returns the associated
    // /// rprim path.
    // ///
    // /// Note that this function doesn't resolve instancer relationship.
    // /// returning prim can be a prototype mesh which may not exist in usd stage.
    // /// It can be resolved to the actual usd prim and corresponding instance
    // /// index by GetPrimPathFromInstanceIndex().
    // ///
    // HDRPR_API
    // SdfPath GetRprimPathFromPrimId(int primId) const;

    // /// Using colors extracted from an Id render, returns the associated
    // /// prim path and optional instance index.
    // ///
    // /// Note that this function doesn't resolve instancer relationship.
    // /// returning prim can be a prototype mesh which may not exist in usd stage.
    // /// It can be resolved to the actual usd prim and corresponding instance
    // /// index by GetPrimPathFromInstanceIndex().
    // ///
    // /// XXX: consider renaming to GetRprimPathFromPrimIdColor
    // ///
    // HDRPR_API
    // SdfPath GetPrimPathFromPrimIdColor(
    //     GfVec4i const & primIdColor,
    //     GfVec4i const & instanceIdColor,
    //     int * instanceIndexOut = NULL);

    // /// Returns the rprim id path of the instancer being rendered by this
    // /// engine that corresponds to the instance index generated by the
    // /// specified instanced prototype rprim id.
    // /// Returns an empty path if no such instance prim exists.
    // ///
    // /// \p instancerIndex is also returned, which is an instance index
    // /// of all instances in the top-level instancer. Note that if the instancer
    // /// instances heterogeneously, or there are multiple levels of hierarchy,
    // /// \p protoIndex of the prototype rprim doesn't match the
    // /// \p instancerIndex in the instancer (see usdImaging/delegate.h)
    // /// 
    // /// If \p masterCachePath is not NULL, and the input rprim is an instance
    // /// resulting from an instanceable reference (and not from a
    // /// PointInstancer), then it will be set to the cache path of the
    // /// corresponding instance master prim. Otherwise, it will be set to null.
    // ///
    // /// If \p instanceContext is not NULL, it is populated with the list of 
    // /// instance roots that must be traversed to get to the rprim. If this
    // /// list is non-empty, the last prim is always the forwarded rprim.
    // /// 
    // HDRPR_API
    // SdfPath GetPrimPathFromInstanceIndex(
    //     const SdfPath &protoRprimId,
    //     int protoIndex,
    //     int *instancerIndex=NULL,
    //     SdfPath *masterCachePath=NULL,
    //     SdfPathVector *instanceContext=NULL);

    // /// Resolves a 4-byte pixel from an id render to an int32 prim ID.
    // static inline int DecodeIDRenderColor(unsigned char const idColor[4]) {
    //     return HdxPickTask::DecodeIDRenderColor(idColor);
    // }

    /// @}
    
    // ---------------------------------------------------------------------
    /// \name Renderer Plugin Management
    /// @{
    // ---------------------------------------------------------------------

    /// Return the vector of available render-graph delegate plugins.
    HDRPR_API
    static TfTokenVector GetRendererPlugins();

    /// Return the user-friendly description of a renderer plugin.
    HDRPR_API
    static std::string GetRendererDisplayName(TfToken const &id);

    /// Return the id of the currently used renderer plugin.
    HDRPR_API
    TfToken GetCurrentRendererId() const { return m_rendererId; };

    /// Set the current render-graph delegate to \p id.
    /// the plugin will be loaded if it's not yet.
    HDRPR_API
    bool SetRendererPlugin(TfToken const &id);

    /// @}
    
    // ---------------------------------------------------------------------
    /// \name AOVs
    /// @{
    // ---------------------------------------------------------------------

    /// Return the vector of available renderer AOV settings.
    HDRPR_API
    TfTokenVector const& GetRendererAovs() const { return m_rendererAovs; }

    /// Set the current renderer AOV to \p id.
    HDRPR_API
    bool SetRendererAovs(TfTokenVector const& ids);

    HDRPR_API
    HdRenderBuffer* GetAovBuffer(TfToken const& id);

    /// @}

    // // ---------------------------------------------------------------------
    // /// \name Color Correction
    // /// @{
    // // ---------------------------------------------------------------------

    // /// Set \p id to one of the HdxColorCorrectionTokens.
    // /// \p framebufferResolution should be the size of the bound framebuffer
    // /// that will be color corrected. It is recommended that a 16F or higher
    // /// AOV is bound for color correction.
    // HDRPR_API
    // void SetColorCorrectionSettings(
    //     TfToken const& id, 
    //     GfVec2i const& framebufferResolution);

    // /// @}

    // /// Returns true if the platform is color correction capable.
    // HDRPR_API
    // static bool IsColorCorrectionCapable();

    // ---------------------------------------------------------------------
    /// \name Render and Scene delegate access
    /// @{
    // ---------------------------------------------------------------------

    /// Returns render delegate.
    ///
    HDRPR_API
    HdRenderDelegate* GetRenderDelegate() const { return m_renderIndex->GetRenderDelegate(); }


    /// Returns render delegate.
    ///
    HDRPR_API
    UsdImagingDelegate* GetSceneDelegate() const { return m_delegate; }

    /// @}

private:
    // These functions factor batch preparation into separate steps so they
    // can be reused by both the vectorized and non-vectorized API.
    HDRPR_API
    bool _CanPrepareBatch(const UsdPrim& root, 
        const HdRprEngineRenderParams& params);

    // Create a hydra collection given root paths and render params.
    // Returns true if the collection was updated.
    HDRPR_API
    static bool _UpdateHydraCollection(HdRprimCollection *collection,
                          SdfPathVector const& roots,
                          HdRprEngineRenderParams const& params);
    HDRPR_API
    static HdxRenderTaskParams _MakeHydraHdRprEngineRenderParams(
                          HdRprEngineRenderParams const& params);
    HDRPR_API
    static void _ComputeRenderTags(HdRprEngineRenderParams const& params,
                          TfTokenVector *renderTags);

    // This function disposes of: the render index, the render plugin,
    // the task controller, and the usd imaging delegate.
    HDRPR_API
    void _DeleteHydraResources();

    HDRPR_API
    static TfToken _GetDefaultRendererPluginId();

private:
    HdEngine m_engine;
    HdRenderIndex* m_renderIndex;

    SdfPath const m_delegateID;
    UsdImagingDelegate* m_delegate;

    SdfPath m_rootPath;
    SdfPathVector m_excludedPrimPaths;
    SdfPathVector m_invisedPrimPaths;
    bool m_isPopulated;

    HdRendererPlugin* m_rendererPlugin;
    TfToken m_rendererId;
    TfTokenVector m_rendererAovs;

    HdxTaskController* m_taskController;
    HdRprimCollection m_renderCollection;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_ENGINE_H
