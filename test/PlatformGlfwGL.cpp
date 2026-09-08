#include <PlatformGlfwGL.h>

#include <utils/Logger.h>
#include <utils/Panic.h>

// 只声明 bluegl::bind()，避免引入 bluegl 的 glcorearb.h 与 Qt 的 GL/gl.h 冲突
namespace bluegl {
int bind();
}

namespace filament::backend {

using namespace backend;

Driver* PlatformGlfwGL::createDriver(void* sharedGLContext,
        const Platform::DriverConfig& driverConfig) noexcept {
    // 本函数在驱动线程上执行：创建属于驱动线程的 GL 上下文（与 Qt widget 共享），
    // 并绑定到离屏 surface，之后 Filament 后端的所有 GL 调用都在这里发生。
	if (mSharedContext) {
		mDriverContext = new QOpenGLContext();
		// 与 widget 上下文保持完全相同的格式，最大化 wglShareLists 成功概率
		QSurfaceFormat fmt = mSharedContext->format();
		fmt.setSamples(0);
        mDriverContext->setFormat(fmt);
        mDriverContext->setShareContext(mSharedContext);
        if (mDriverContext->create()) {
            mOffscreen = new QOffscreenSurface();
            mOffscreen->setFormat(fmt);
            mOffscreen->create();
            if (!mDriverContext->makeCurrent(mOffscreen)) {
                utils::slog.e << "PlatformGlfwGL: cannot make driver context current"
                              << utils::io::endl;
            } else {
                mGl = mDriverContext->versionFunctions<QOpenGLFunctions_4_5_Core>();
                if (mGl) {
                    mGl->initializeOpenGLFunctions();
                }
            }
        } else {
            utils::slog.e << "PlatformGlfwGL: cannot create driver context" << utils::io::endl;
        }
    }

    int result = bluegl::bind();
    if (result != 0) {
        utils::slog.e << "Unable to load OpenGL entry points." << utils::io::endl;
        return nullptr;
    }

    // 先创建一次离屏 FBO，避免驱动初始化期间查询默认 FBO 时拿到无效 id
    const uint32_t w = mTargetWidth.load() ? mTargetWidth.load() : 1280;
    const uint32_t h = mTargetHeight.load() ? mTargetHeight.load() : 800;
    recreateFramebuffers(w, h);
    mLastReportedFbo = mOffscreenFbo;

    return OpenGLPlatform::createDefaultDriver(this, sharedGLContext, driverConfig);
}

int PlatformGlfwGL::getOSVersion() const noexcept {
    return 0;
}

void PlatformGlfwGL::terminate() noexcept {
    if (mDriverContext) {
        if (QOpenGLContext::currentContext() == mDriverContext) {
            destroyFramebuffers();
            mDriverContext->doneCurrent();
        }
        delete mOffscreen;
        delete mDriverContext;
        mOffscreen = nullptr;
        mDriverContext = nullptr;
        mGl = nullptr;
    }
}

Platform::SwapChain* PlatformGlfwGL::createSwapChain(
        void* nativeWindow, uint64_t flags) noexcept {
    return (SwapChain*)nativeWindow;
}

Platform::SwapChain* PlatformGlfwGL::createSwapChain(
        uint32_t width, uint32_t height, uint64_t flags) noexcept {
    // headless swapchain 未使用
    return nullptr;
}

uint32_t PlatformGlfwGL::getDefaultFramebufferObject() noexcept {
    // Filament 的"默认 framebuffer" = 驱动线程上的离屏 FBO
    return mOffscreenFbo;
}

void PlatformGlfwGL::destroySwapChain(Platform::SwapChain* swapChain) noexcept {
}

bool PlatformGlfwGL::makeCurrent(ContextType type, SwapChain* drawSwapChain,
        SwapChain* readSwapChain) {
    return true;
}

void PlatformGlfwGL::makeCurrent(SwapChain* drawSwapChain, SwapChain* readSwapChain,
        utils::Invocable<void()> preContextChange,
        utils::Invocable<void(size_t index)> postContextChange) {
    // 驱动线程：确保共享上下文 current，并按需重建离屏缓冲
    if (!mDriverContext || !mOffscreen) {
        return;
    }
    if (QOpenGLContext::currentContext() != mDriverContext) {
        mDriverContext->makeCurrent(mOffscreen);
    }

    const uint32_t w = mTargetWidth.load();
    const uint32_t h = mTargetHeight.load();
    if (w && h && (!mOffscreenFbo || w != mWidth || h != mHeight)) {
        recreateFramebuffers(w, h);
    }

    // 离屏 FBO id 变化时通知后端重新解析默认 framebuffer（避免缓存旧 id）
    if (mOffscreenFbo != mLastReportedFbo) {
        mLastReportedFbo = mOffscreenFbo;
        postContextChange(0);
    }
}

void PlatformGlfwGL::commit(Platform::SwapChain* swapChain) noexcept {
    // 驱动线程：把本帧离屏颜色拷贝进环形纹理，并通知 GUI 线程可以 blit 了
    if (mOffscreenFbo && mGl) {
        const uint32_t slot = mDriverSlot % RING_COUNT;
        if (mRingFbos[slot]) {
            mGl->glBindFramebuffer(GL_READ_FRAMEBUFFER, mOffscreenFbo);
            mGl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mRingFbos[slot]);
            mGl->glBlitFramebuffer(0, 0, (GLint)mWidth, (GLint)mHeight,
                    0, 0, (GLint)mWidth, (GLint)mHeight,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
            mGl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
            mDriverSlot++;
        }
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCompletedFrames++;
    }
    mCond.notify_all();
}

void PlatformGlfwGL::waitFrameCompleted(uint64_t frames) noexcept {
    std::unique_lock<std::mutex> lock(mMutex);
    mCond.wait(lock, [&]() { return mCompletedFrames >= frames; });
}

// 帧跳过后端用 fence 判断"GPU 是否落后"。我们的渲染循环自带环形缓冲节流，
// 不需要跳帧，这里让 fence 永远满足，保证每帧都渲染并呈现（否则会黑屏/闪烁）。
bool PlatformGlfwGL::canCreateFence() noexcept {
    return true;
}

Platform::Fence* PlatformGlfwGL::createFence() noexcept {
    return new Platform::Fence();
}

void PlatformGlfwGL::destroyFence(Platform::Fence* fence) noexcept {
    delete fence;
}

backend::FenceStatus PlatformGlfwGL::waitFence(Platform::Fence*, uint64_t) noexcept {
    return backend::FenceStatus::CONDITION_SATISFIED;
}

void PlatformGlfwGL::destroyFramebuffers() {
    if (!mGl) return;
    if (mOffscreenFbo) {
        mGl->glDeleteFramebuffers(1, &mOffscreenFbo);
        mGl->glDeleteTextures(1, &mOffscreenColorTex);
        mGl->glDeleteRenderbuffers(1, &mOffscreenDepthRbo);
        mOffscreenFbo = mOffscreenColorTex = mOffscreenDepthRbo = 0;
    }
    for (uint32_t i = 0; i < RING_COUNT; i++) {
        if (mRingFbos[i]) mGl->glDeleteFramebuffers(1, &mRingFbos[i]);
        if (mRingTextures[i]) mGl->glDeleteTextures(1, &mRingTextures[i]);
        mRingFbos[i] = mRingTextures[i] = 0;
    }
}

void PlatformGlfwGL::recreateFramebuffers(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || !mGl) return;
    if (mOffscreenFbo && width == mWidth && height == mHeight) return;

    destroyFramebuffers();

    // 离屏渲染目标：颜色纹理 + 深度 renderbuffer
    mGl->glGenFramebuffers(1, &mOffscreenFbo);
    mGl->glGenTextures(1, &mOffscreenColorTex);
    mGl->glBindTexture(GL_TEXTURE_2D, mOffscreenColorTex);
    mGl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mGl->glGenRenderbuffers(1, &mOffscreenDepthRbo);
    mGl->glBindRenderbuffer(GL_RENDERBUFFER, mOffscreenDepthRbo);
    mGl->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
            (GLsizei)width, (GLsizei)height);

    mGl->glBindFramebuffer(GL_FRAMEBUFFER, mOffscreenFbo);
    mGl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, mOffscreenColorTex, 0);
    mGl->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, mOffscreenDepthRbo);
    mGl->glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 环形呈现纹理（与 GUI 线程共享，供 blit）
    for (uint32_t i = 0; i < RING_COUNT; i++) {
        mGl->glGenTextures(1, &mRingTextures[i]);
        mGl->glBindTexture(GL_TEXTURE_2D, mRingTextures[i]);
        mGl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        mGl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        mGl->glGenFramebuffers(1, &mRingFbos[i]);
        mGl->glBindFramebuffer(GL_FRAMEBUFFER, mRingFbos[i]);
        mGl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, mRingTextures[i], 0);
        mGl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    mWidth = width;
    mHeight = height;
}

} // namespace filament::backend
