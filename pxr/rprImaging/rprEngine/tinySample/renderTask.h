#ifndef HDRPR_RENDER_TASK_H
#define HDRPR_RENDER_TASK_H

#include "pxr/imaging/hd/task.h"
#include "pxr/imaging/hd/renderPass.h"
#include "pxr/imaging/hd/renderPassState.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRprRenderTask : public HdTask {
public:
    HdRprRenderTask(HdSceneDelegate* delegate, SdfPath const& id);

    HdRprRenderTask() = delete;
    HdRprRenderTask(HdRprRenderTask const&) = delete;
    HdRprRenderTask &operator=(HdRprRenderTask const&) = delete;

    ~HdRprRenderTask() override;

    bool IsConverged() const;

    /// Sync the render pass resources
    void Sync(HdSceneDelegate* delegate,
              HdTaskContext* ctx,
              HdDirtyBits* dirtyBits) override;

    /// Prepare the tasks resources
    void Prepare(HdTaskContext* ctx,
                 HdRenderIndex* renderIndex) override;

    /// Execute render pass task
    void Execute(HdTaskContext* ctx) override;

    /// Collect Render Tags used by the task.
    TfTokenVector const& GetRenderTags() const override;

private:
    HdRenderPassSharedPtr m_pass;
    HdRenderPassStateSharedPtr m_passState;

    TfTokenVector m_renderTags;
    GfVec4d m_viewport;
    SdfPath m_cameraId;
    HdRenderPassAovBindingVector m_aovBindings;
};

struct HdRprRenderTaskParams {
    // Should not be empty.
    HdRenderPassAovBindingVector aovBindings;

    SdfPath camera;
    GfVec4d viewport = GfVec4d(0.0);
};

// VtValue requirements
std::ostream& operator<<(std::ostream& out, const HdRprRenderTaskParams& pv);
bool operator==(const HdRprRenderTaskParams& lhs, const HdRprRenderTaskParams& rhs);
bool operator!=(const HdRprRenderTaskParams& lhs, const HdRprRenderTaskParams& rhs);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_RENDER_TASK_H
