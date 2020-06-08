#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/renderPass.h"
#include "pxr/imaging/hd/renderBuffer.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/rendererPlugin.h"
#include "pxr/imaging/hd/rendererPluginRegistry.h"


#include "pxr/usdImaging/usdImaging/delegate.h"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/metrics.h"

#include "pxr/imaging/glf/image.h"

#include "pxr/base/work/loops.h"

#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/camera.h"
#include "pxr/base/gf/frustum.h"

#include "renderTask.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (renderBufferDescriptor)
    (renderTags));

class HdRprTaskDataDelegate : public HdSceneDelegate {
public:
    HdRprTaskDataDelegate(HdRenderIndex* parentIndex, SdfPath const& delegateID)
        : HdSceneDelegate(parentIndex, delegateID) {}
    ~HdRprTaskDataDelegate() override = default;

    template <typename T>
    void SetParameter(SdfPath const& id, TfToken const& key, T const& value) {
        m_valueCacheMap[id][key] = value;
    }

    template <typename T>
    T const& GetParameter(SdfPath const& id, TfToken const& key) const {
        VtValue vParams;
        ValueCache vCache;
        TF_VERIFY(
            TfMapLookup(m_valueCacheMap, id, &vCache) &&
            TfMapLookup(vCache, key, &vParams) &&
            vParams.IsHolding<T>());
        return vParams.Get<T>();
    }

    bool HasParameter(SdfPath const& id, TfToken const& key) const {
        ValueCache vCache;
        if (TfMapLookup(m_valueCacheMap, id, &vCache) &&
            vCache.count(key) > 0) {
            return true;
        }
        return false;
    }

    VtValue Get(SdfPath const& id, TfToken const& key) override {
        auto vcache = TfMapLookupPtr(m_valueCacheMap, id);
        VtValue ret;
        if (vcache && TfMapLookup(*vcache, key, &ret)) {
            return ret;
        }
        TF_CODING_ERROR("%s:%s doesn't exist in the value cache\n",
            id.GetText(), key.GetText());
        return VtValue();
    }

    GfMatrix4d GetTransform(SdfPath const& id) override {
        // We expect this to be called only for the free cam.
        VtValue val = GetCameraParamValue(id, HdCameraTokens->worldToViewMatrix);
        GfMatrix4d xform(1.0);
        if (val.IsHolding<GfMatrix4d>()) {
            xform = val.Get<GfMatrix4d>().GetInverse(); // camera to world
        } else {
            TF_CODING_ERROR(
                "Unexpected call to GetTransform for %s in HdxTaskController's "
                "internal scene delegate.\n", id.GetText());
        }
        return xform;
    }

    VtValue GetCameraParamValue(SdfPath const& id, TfToken const& key) override {
        if (key == HdCameraTokens->worldToViewMatrix ||
            key == HdCameraTokens->projectionMatrix ||
            key == HdCameraTokens->clipPlanes ||
            key == HdCameraTokens->windowPolicy) {

            return Get(id, key);
        } else {
            // XXX: For now, skip handling physical params on the free cam.
            return VtValue();
        }
    }

    VtValue GetLightParamValue(SdfPath const& id, TfToken const& paramName) override {
        return Get(id, paramName);
    }

    HdRenderBufferDescriptor GetRenderBufferDescriptor(SdfPath const& id) override {
        return GetParameter<HdRenderBufferDescriptor>(id, _tokens->renderBufferDescriptor);
    }

    TfTokenVector GetTaskRenderTags(SdfPath const& taskId) override {
        if (HasParameter(taskId, _tokens->renderTags)) {
            return GetParameter<TfTokenVector>(taskId, _tokens->renderTags);
        }
        return TfTokenVector();
    }

private:
    typedef TfHashMap<TfToken, VtValue, TfToken::HashFunctor> ValueCache;
    typedef TfHashMap<SdfPath, ValueCache, SdfPath::Hash> ValueCacheMap;
    ValueCacheMap m_valueCacheMap;
};

HdRenderDelegate* GetRenderDelegate(TfToken const& id) {
    HdRendererPlugin* plugin = nullptr;
    TfToken actualId = id;

    // Special case: TfToken() selects the first plugin in the list.
    if (actualId.IsEmpty()) {
        actualId = HdRendererPluginRegistry::GetInstance().GetDefaultPluginId();
    }
    plugin = HdRendererPluginRegistry::GetInstance().GetRendererPlugin(actualId);

    if (plugin == nullptr) {
        TF_CODING_ERROR("Couldn't find plugin for id %s", actualId.GetText());
        return nullptr;
    } else if (!plugin->IsSupported()) {
        // Don't do anything if the plugin isn't supported on the running
        // system, just return that we're not able to set it.
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return nullptr;
    }

    HdRenderDelegate* renderDelegate = plugin->CreateRenderDelegate();
    if (!renderDelegate) {
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(plugin);
        return nullptr;
    }

    return renderDelegate;
}

static GfCamera
_ComputeCameraToFrameStage(const UsdStageRefPtr& stage, UsdTimeCode timeCode,
    const TfTokenVector& includedPurposes) {
    // Start with a default (50mm) perspective GfCamera.
    GfCamera gfCamera;
    UsdGeomBBoxCache bboxCache(timeCode, includedPurposes,
        /* useExtentsHint = */ true);
    GfBBox3d bbox = bboxCache.ComputeWorldBound(stage->GetPseudoRoot());
    GfVec3d center = bbox.ComputeCentroid();
    GfRange3d range = bbox.ComputeAlignedRange();
    GfVec3d dim = range.GetSize();
    TfToken upAxis = UsdGeomGetStageUpAxis(stage);
    // Find corner of bbox in the focal plane.
    GfVec2d plane_corner;
    if (upAxis == UsdGeomTokens->y) {
        plane_corner = GfVec2d(dim[0], dim[1]) / 2;
    } else {
        plane_corner = GfVec2d(dim[0], dim[2]) / 2;
    }
    float plane_radius = sqrt(GfDot(plane_corner, plane_corner));
    // Compute distance to focal plane.
    float half_fov = gfCamera.GetFieldOfView(GfCamera::FOVHorizontal) / 2.0;
    float distance = plane_radius / tan(GfDegreesToRadians(half_fov));
    // Back up to frame the front face of the bbox.
    if (upAxis == UsdGeomTokens->y) {
        distance += dim[2] / 2;
    } else {
        distance += dim[1] / 2;
    }
    // Compute local-to-world transform for camera filmback.
    GfMatrix4d xf;
    //if (upAxis == UsdGeomTokens->y) {
        xf.SetTranslate(center + GfVec3d(0, 0, distance));
    /*} else {
        xf.SetRotate(GfRotation(GfVec3d(1, 0, 0), 90));
        xf.SetTranslateOnly(center + GfVec3d(0, -distance, 0));
    }*/
    gfCamera.SetTransform(xf);
    return gfCamera;
}

// Prman linear to display
static float DspyLinearTosRGB(float u) {
    return u < 0.0031308f ? 12.92f * u : 1.055f * powf(u, 0.4167f) - 0.055f;
}

PXR_NAMESPACE_CLOSE_SCOPE

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

    HdEngine engine;
    auto renderDelegate = GetRenderDelegate(TfToken("HdRprPlugin"));
    auto renderIndex = HdRenderIndex::New(renderDelegate, {});
    auto sceneDelegateId = SdfPath::AbsoluteRootPath().AppendElementString("usdImagingDelegate");
    auto sceneDelegate = new UsdImagingDelegate(renderIndex, sceneDelegateId);
    sceneDelegate->Populate(rootPrim);

    //renderDelegate->SetRenderSetting(TfToken("maxSamples"), VtValue(32));

    auto taskDataDelegate = new HdRprTaskDataDelegate(renderIndex, SdfPath::AbsoluteRootPath().AppendElementString("taskDataDelegate"));
    auto taskDataDelegateId = taskDataDelegate->GetDelegateID();

    auto camera = _ComputeCameraToFrameStage(stage, UsdTimeCode::Default(), {UsdGeomTokens->default_, UsdGeomTokens->proxy});
    auto frustum = camera.GetFrustum();

    //auto viewMatrix = GfMatrix4d(GfMatrix3d(1.0), GfVec3d(0.0, -0.5, -0.25));
    //auto projMatrix = GfMatrix4d(1.0);
    auto viewMatrix = frustum.ComputeViewMatrix();
    auto projMatrix = frustum.ComputeProjectionMatrix();

    auto freeCameraId = taskDataDelegateId.AppendElementString("freeCamera");
    renderIndex->InsertSprim(HdPrimTypeTokens->camera, taskDataDelegate, freeCameraId);
    taskDataDelegate->SetParameter(freeCameraId, HdCameraTokens->windowPolicy, VtValue(CameraUtilFit));
    taskDataDelegate->SetParameter(freeCameraId, HdCameraTokens->worldToViewMatrix, VtValue(viewMatrix));
    taskDataDelegate->SetParameter(freeCameraId, HdCameraTokens->projectionMatrix, VtValue(projMatrix));
    taskDataDelegate->SetParameter(freeCameraId, HdCameraTokens->clipPlanes, VtValue(std::vector<GfVec4d>()));

    GfVec4d viewport(0, 0, 1024, 1024);
    auto aovDimensions = GfVec3i(viewport[2], viewport[3], 1);

    HdRenderPassAovBindingVector aovBindings;
    if (TF_VERIFY(renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer))) {
        for (auto& aov : {HdAovTokens->color, HdAovTokens->depth}) {
            auto aovDesc = renderDelegate->GetDefaultAovDescriptor(aov);
            if (aovDesc.format != HdFormatInvalid) {
                auto renderBufferId = taskDataDelegateId.AppendElementString("aov_" + aov.GetString());
                renderIndex->InsertBprim(HdPrimTypeTokens->renderBuffer, taskDataDelegate, renderBufferId);

                HdRenderBufferDescriptor desc;
                desc.dimensions = aovDimensions;
                desc.format = aovDesc.format;
                desc.multiSampled = aovDesc.multiSampled;
                taskDataDelegate->SetParameter(renderBufferId, _tokens->renderBufferDescriptor, desc);
                renderIndex->GetChangeTracker().MarkBprimDirty(renderBufferId, HdRenderBuffer::DirtyDescription);

                HdRenderPassAovBinding binding;
                binding.aovName = aov;
                binding.renderBufferId = renderBufferId;
                binding.aovSettings = aovDesc.aovSettings;
                aovBindings.push_back(binding);
            } else {
                TF_RUNTIME_ERROR("Could not set \"%s\" AOV: unsupported by render delegate\n", aov.GetText());
            }
        }
    }

    auto renderTaskId = taskDataDelegateId.AppendElementString("renderTask");
    renderIndex->InsertTask<HdRprRenderTask>(taskDataDelegate, renderTaskId);

    HdRprRenderTaskParams renderTaskParams;
    renderTaskParams.viewport = viewport;
    renderTaskParams.camera = freeCameraId;
    renderTaskParams.aovBindings = aovBindings;
    taskDataDelegate->SetParameter(renderTaskId, HdTokens->params, renderTaskParams);
    renderIndex->GetChangeTracker().MarkTaskDirty(renderTaskId, HdChangeTracker::DirtyParams);

    auto reprSelector = HdReprSelector(HdReprTokens->smoothHull);
    HdRprimCollection rprimCollection(HdTokens->geometry, reprSelector, false, TfToken());
    rprimCollection.SetRootPath(SdfPath::AbsoluteRootPath());
    taskDataDelegate->SetParameter(renderTaskId, HdTokens->collection, rprimCollection);
    renderIndex->GetChangeTracker().MarkTaskDirty(renderTaskId, HdChangeTracker::DirtyCollection);

    TfTokenVector renderTags{HdRenderTagTokens->geometry};
    taskDataDelegate->SetParameter(renderTaskId, HdTokens->renderTags, renderTags);
    renderIndex->GetChangeTracker().MarkTaskDirty(renderTaskId, HdChangeTracker::DirtyRenderTags);

    {
        auto renderTask = std::static_pointer_cast<HdRprRenderTask>(renderIndex->GetTask(renderTaskId));
        HdTaskSharedPtrVector tasks = { renderTask };

        engine.Execute(renderIndex, &tasks);
        while (!renderTask->IsConverged()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    aovBindings[0].renderBuffer = static_cast<HdRenderBuffer*>(renderIndex->GetBprim(HdPrimTypeTokens->renderBuffer, aovBindings[0].renderBufferId));
    if (aovBindings[0].renderBuffer) {
        GlfImage::StorageSpec storage;
        storage.width = aovDimensions[0];
        storage.height = aovDimensions[1];
        storage.depth = aovDimensions[2];
        storage.format = GL_RGBA;
        storage.type = GL_FLOAT;
        storage.flipped = true;
        storage.data = aovBindings[0].renderBuffer->Map();

        WorkParallelForN(size_t(storage.width) * storage.height, [&storage](size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                for (int j = 0; j < 3; ++j) {
                    float* value = (float*)storage.data + 4 * i + j;
                    *value = DspyLinearTosRGB(*value);
                }
            }
        });

        GlfImageSharedPtr image = GlfImage::OpenForWriting("color.png");
        bool writeSuccess = image && image->Write(storage);

        aovBindings[0].renderBuffer->Unmap();
    }

    delete taskDataDelegate;
    delete sceneDelegate;
    delete renderIndex;
    delete renderDelegate;

    return 0;
}
