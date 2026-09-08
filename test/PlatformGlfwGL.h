/*
 * 多线程版 PlatformGlfwGL：
 * - 驱动线程持有独立的 QOpenGLContext（与 QOpenGLWidget 共享），渲染到离屏 FBO；
 * - 每帧结束后把离屏颜色缓冲拷贝到环形纹理；
 * - GUI 线程（paintGL）把上一帧的环形纹理 blit 到 Qt 的默认 FBO。
 */
#pragma once

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions_4_5_Core>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdint.h>

#include <backend/platforms/OpenGLPlatform.h>
#include <backend/DriverEnums.h>

class QOpenGLWidget;

namespace filament::backend {

class PlatformGlfwGL : public OpenGLPlatform {
public:
    QOpenGLWidget* glwidget = nullptr;   // 仅保存引用，不再参与 GL 调用
    static constexpr uint32_t RING_COUNT = 3;   // 纹理环形缓冲数量（帧延迟）

    // GUI 线程调用
    void setSharedContext(QOpenGLContext* ctx) noexcept { mSharedContext = ctx; }
    void setTargetSize(uint32_t width, uint32_t height) noexcept {
        mTargetWidth.store(width);
        mTargetHeight.store(height);
    }
    // 等待驱动线程完成 frames 次 commit（即第 frames 帧已拷入环形纹理）
    void waitFrameCompleted(uint64_t frames) noexcept;
    uint32_t getTextureId(uint32_t slot) const noexcept {
        return mRingTextures[slot % RING_COUNT];
    }

protected:
    Driver* createDriver(void* sharedGLContext,
            const Platform::DriverConfig& driverConfig) noexcept override;

    int getOSVersion() const noexcept override;
    void terminate() noexcept override;

    SwapChain* createSwapChain(void* nativewindow, uint64_t flags) noexcept override;
    SwapChain* createSwapChain(uint32_t width, uint32_t height, uint64_t flags) noexcept override;
    virtual uint32_t getDefaultFramebufferObject() noexcept override;
    void destroySwapChain(SwapChain* swapChain) noexcept override;
    bool makeCurrent(ContextType type, SwapChain* drawSwapChain, SwapChain* readSwapChain) override;
    void makeCurrent(SwapChain* drawSwapChain, SwapChain* readSwapChain,
            utils::Invocable<void()> preContextChange,
            utils::Invocable<void(size_t index)> postContextChange) override;
    void commit(SwapChain* swapChain) noexcept override;
    bool canCreateFence() noexcept override;
    Fence* createFence() noexcept override;
    void destroyFence(Fence* fence) noexcept override;
    backend::FenceStatus waitFence(Fence* fence, uint64_t timeout) noexcept override;

private:
    void recreateFramebuffers(uint32_t width, uint32_t height);
    void destroyFramebuffers();

    QOpenGLContext* mDriverContext = nullptr;
    QOffscreenSurface* mOffscreen = nullptr;
    QOpenGLContext* mSharedContext = nullptr;   // QOpenGLWidget 的 context（share 用）
    QOpenGLFunctions_4_5_Core* mGl = nullptr;   // 驱动线程的 GL 入口

    std::atomic<uint32_t> mTargetWidth{0};
    std::atomic<uint32_t> mTargetHeight{0};
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint32_t mLastReportedFbo = 0;

    // 离屏渲染目标（Filament 的"默认 framebuffer"）
    uint32_t mOffscreenFbo = 0;
    uint32_t mOffscreenColorTex = 0;
    uint32_t mOffscreenDepthRbo = 0;

    // 环形呈现纹理（与 GUI 线程共享）
    uint32_t mRingFbos[RING_COUNT] = {};
    uint32_t mRingTextures[RING_COUNT] = {};
    uint32_t mDriverSlot = 0;    // 驱动线程的帧序号

    std::mutex mMutex;
    std::condition_variable mCond;
    uint64_t mCompletedFrames = 0;
};

} // namespace filament::backend
