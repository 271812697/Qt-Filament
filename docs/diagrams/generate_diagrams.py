# -*- coding: utf-8 -*-
"""
生成 Filament 架构文档所用的 SVG 图。

用法（仓库根目录）:
    python docs/diagrams/generate_diagrams.py

输出: docs/diagrams/*.svg
纯标准库实现，无第三方依赖。
"""

import os
import xml.etree.ElementTree as ET

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)))

FONT = "Microsoft YaHei, PingFang SC, Noto Sans SC, sans-serif"

# 配色
C_QT    = ("#EDE7F6", "#4527A0")   # Qt 层
C_FRONT = ("#E3F2FD", "#1565C0")   # Filament 前端
C_DATA  = ("#F3E5F5", "#6A1B9A")   # 数据结构
C_BACK  = ("#FFF3E0", "#E65100")   # 后端
C_GL    = ("#E8F5E9", "#2E7D32")   # OpenGL
C_WARN  = ("#FFEBEE", "#C62828")   # 注意
C_GRAY  = ("#F5F5F5", "#616161")   # 中性
C_TXT   = "#212121"


def esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class SVG:
    def __init__(self, w: int, h: int):
        self.w = w
        self.h = h
        self.body = []

    def _text_block(self, x, y, lines, size, fill, bold=False, anchor="middle"):
        if isinstance(lines, str):
            lines = [lines]
        parts = []
        lh = size + 5
        for i, line in enumerate(lines):
            weight = "bold" if (bold and i == 0) else "normal"
            parts.append(
                f'<tspan x="{x}" y="{y + i * lh}" font-size="{size}" '
                f'font-weight="{weight}">{esc(line)}</tspan>'
            )
        return (
            f'<text x="{x}" y="{y}" fill="{fill}" font-family="{FONT}" '
            f'text-anchor="{anchor}">{"".join(parts)}</text>'
        )

    def box(self, x, y, w, h, title, lines=(), fill=None, stroke=None,
            size=13, title_size=14, dashed=False, rx=8):
        fill = fill or C_FRONT[0]
        stroke = stroke or C_FRONT[1]
        dash = ' stroke-dasharray="6,4"' if dashed else ""
        self.body.append(
            f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="1.6"{dash}/>'
        )
        ty = y + 26 if title else y + 16
        if title:
            self.body.append(
                self._text_block(x + w / 2, ty, [title], title_size, stroke, bold=True)
            )
        if lines:
            self.body.append(
                self._text_block(x + w / 2, ty + 18, list(lines), size, C_TXT)
            )
        return self

    def text(self, x, y, s, size=12, fill=C_TXT, anchor="middle", bold=False, italic=False):
        style = ""
        if bold:
            style += ' font-weight="bold"'
        if italic:
            style += ' font-style="italic"'
        self.body.append(
            f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" '
            f'font-family="{FONT}" text-anchor="{anchor}"{style}>{esc(s)}</text>'
        )
        return self

    def arrow(self, x1, y1, x2, y2, label=None, dashed=False, color="#455A64",
              lx=None, ly=None, two_way=False):
        dash = ' stroke-dasharray="6,4"' if dashed else ""
        mid = (None, None)
        self.body.append(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
            f'stroke-width="1.8" marker-end="url(#arrow)"{dash}/>'
        )
        if two_way:
            self.body.append(
                f'<line x1="{x2}" y1="{y2}" x2="{x1}" y2="{y1}" stroke="{color}" '
                f'stroke-width="1.8" marker-end="url(#arrow)"{dash}/>'
            )
        if label:
            if lx is None or ly is None:
                lx = (x1 + x2) / 2
                ly = (y1 + y2) / 2 - 6
            self.text(lx, ly, label, size=11, fill=color)
        return self

    def save(self, name):
        header = (
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
            f'viewBox="0 0 {self.w} {self.h}">'
            f'<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" '
            f'markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
            f'<path d="M 0 0 L 10 5 L 0 10 z" fill="#455A64"/></marker></defs>'
            f'<rect width="{self.w}" height="{self.h}" fill="white"/>'
        )
        with open(os.path.join(OUT_DIR, name), "w", encoding="utf-8") as f:
            f.write(header + "".join(self.body) + "</svg>")


def overview():
    s = SVG(1280, 840)
    s.text(640, 30, "Filament in Qt：整体架构与数据流", 20, C_TXT, bold=True)

    # 应用层（Qt）
    s.box(40, 60, 260, 150, "Qt 应用层", [
        "DebugOpenGLWidget",
        "paintGL() / resizeEvent()",
        "UI 线程，OpenGL context current",
    ], C_QT)
    s.arrow(300, 130, 400, 130, "beginFrame / render / endFrame / execute", ly=120)

    # Filament 前端（大框）
    s.box(400, 50, 500, 470, "Filament 前端（FRenderer / FView / FScene）", [], C_FRONT)
    s.box(420, 110, 460, 90, "① 场景解析", [
        "Scene::prepare → SoA",
        "视锥剔除 Culler → VISIBLE_MASK",
        "prepareVisibleRenderables → UBO / LOD",
    ], C_DATA)
    s.box(420, 225, 460, 95, "② draw 命令生成", [
        "RenderPass::appendCommands",
        "64 字节 Command[]（key + PrimitiveInfo）",
        "sortCommands 排序 → Executor",
    ], C_DATA)
    s.box(420, 345, 460, 105, "③ FrameGraph", [
        "addPass / compile / execute",
        "shadow → color → 后处理 pass",
        "forwardResource → Qt FBO",
    ], C_DATA)
    s.arrow(470, 460, 470, 520, "import viewRenderTarget", lx=490, ly=495)
    s.box(100, 520, 300, 90, "目标 RenderTarget", [
        "默认 framebuffer",
        "PlatformGlfwGL::getDefaultFramebufferObject()",
        "= Qt 的 defaultFramebufferObject()",
    ], C_QT)
    s.arrow(660, 520, 660, 575, "DriverApi::method(...) 写命令", lx=690, ly=555)

    # 后端
    s.box(400, 575, 500, 190, "后端 backend（DriverApi / CommandStream）", [], C_BACK)
    s.box(420, 630, 220, 90, "命令流", [
        "CircularBuffer 环形缓冲",
        "Command = 函数指针 + 参数 tuple",
    ], C_BACK)
    s.box(660, 630, 220, 90, "命令执行", [
        "Dispatcher 函数指针表",
        "命中 OpenGLDriver 成员函数",
    ], C_BACK)

    s.arrow(400 + 500, 700, 980, 700)
    s.box(980, 60, 260, 380, "OpenGL 后端", [
        "OpenGLDriver",
        "beginRenderPass / draw / endRenderPass",
        "glDrawElements / glBindTexture ...",
        "",
        "执行线程：",
        "多线程 → FEngine::loop 驱动线程",
        "单线程 → Engine::execute() 同线程",
    ], C_GL)
    s.save("overview.svg")


def call_chain():
    s = SVG(1240, 1420)
    s.text(620, 30, "完整函数调用链：从 Qt paintGL 到 OpenGL 调用", 20, C_TXT, bold=True)
    s.text(620, 52, "左列 = 本帧 CPU 阶段，右列 = 每个阶段实际发生的函数调用", 13, "#616161")

    col_x = 540
    y = 80
    step = 95

    def step_row(yy, num, phase, fn, detail, c, lines=None):
        s.box(30, yy - 22, 150, 64, f"阶段 {num}", [phase], C_GRAY)
        s.box(col_x, yy - 30, 640, 72, fn, lines or [detail], c, size=12)

    step_row(y, 1, "Qt 已 makeCurrent", "paintGL()", "QOpenGLWidget 回调，OpenGL context 已 current", C_QT)
    y += step
    step_row(y, 2, "绑定目标 FBO", "glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject())",
             "把 Qt 的 FBO 设为绘制目标（Filament 也查询同一个 FBO）", C_QT)
    y += step
    step_row(y, 3, "更新场景状态", "animate(engine, view, now, delta)",
             "TransformManager::setTransform / Manipulator::update / Camera::lookAt", C_FRONT)
    y += step
    step_row(y, 4, "开启一帧（入队）", "renderer->beginFrame(swapchain)",
             "FSwapChain::makeCurrent(no-op) → FrameSkipper → driver.beginFrame 入队", C_FRONT)
    y += step
    step_row(y, 5, "渲染一帧（入队）", "renderer->render(view)",
             "FRenderer::render → renderInternal → renderJob（见下方展开）", C_FRONT)
    y += step
    s.arrow(col_x + 280, y - 16, col_x + 280, y + 16, "renderJob 内部", lx=col_x + 360, ly=y)
    y += 40

    inner_y = y
    s.box(40, y, 330, 320, "renderJob 内部展开（第 5 步）", [], C_FRONT)
    inner = [
        ("view.prepare(...)", "scene->prepare(SoA) → Culler 剔除 → UBO/LOD"),
        ("RenderPass::appendCommands", "可见物体 → 64B Command[] → 排序"),
        ("shadow pass", "view.renderShadowMaps(fg) → blackboard[\"shadows\"]"),
        ("color pass", "RendererUtils::colorPass(fg, \"Color Pass\", ...)"),
        ("后处理", "TAA/Bloom/DoF/色调映射/FXAA"),
        ("fg.forwardResource + present", "输出转发到 Qt FBO"),
        ("fg.compile() → fg.execute(driver)", "执行各 pass → DriverApi 入队"),
    ]
    for i, (t, d) in enumerate(inner):
        s.box(60, y + 40 + i * 38, 300, 34, "", [f"{t}"], C_DATA, size=11, title_size=11)
        s.text(520, y + 60 + i * 38, d, 11, "#616161", anchor="start")
    y += 330

    step_row(y, 6, "结束一帧（入队）", "renderer->endFrame()",
             "commit(no-op) → driver.endFrame → engine.flush()", C_FRONT)
    y += step
    step_row(y, 7, "切缓冲 + 通知", "CommandBufferQueue::flush()",
             "追加 NoopCommand(nullptr) → [tail,head) 压入队列 → notify；空间不足则阻塞", C_BACK)
    y += step
    step_row(y, 8, "执行命令链", "Engine::execute()（单线程构建）",
             "waitForCommands → 逐条执行 CommandBase 链", C_BACK)
    y += step
    step_row(y, 9, "命中后端", "ConcreteDispatcher<OpenGLDriver>::method()",
             "static_cast<OpenGLDriver&>(driver) → 展开参数 tuple → 调用成员函数", C_GL)
    y += step
    step_row(y, 10, "真实 GL 调用", "OpenGLDriver::draw / beginRenderPass / blit ...",
             "glDrawElements / glBindFramebuffer / glBlitFramebuffer ...", C_GL)

    s.arrow(col_x + 280, 80, col_x + 280, y + 16, dashed=True)
    s.save("call-chain.svg")


def scene_pipeline():
    s = SVG(1280, 800)
    s.text(640, 28, "场景解析流水线（每帧 view.prepare 期间）", 19, C_TXT, bold=True)

    s.box(30, 70, 250, 110, "Scene::mEntities", [
        "robin_set<Entity>",
        "（加入过的实体集合）",
    ], C_DATA)
    s.arrow(280, 120, 360, 120, "em.isAlive(e) 过滤", ly=110)

    s.box(360, 60, 330, 130, "实体分类（并行前）", [
        "tcm.getInstance(e) → Transform",
        "rcm.getInstance(e) → Renderable?",
        "lcm.getInstance(e) → Light?",
        "最强方向光单独挑出",
    ], C_FRONT)
    s.arrow(690, 100, 770, 100, "instance 对", ly=90)
    s.arrow(690, 140, 770, 140, "instance 对", ly=130)

    s.box(770, 60, 480, 130, "填充 SoA（JobSystem 并行）", [
        "RenderableSoa：WORLD_TRANSFORM / VISIBILITY / AABB / PRIMITIVES ...",
        "LightSoa：POSITION_RADIUS / DIRECTION / SHADOW_INFO ...",
        "容量按 16 对齐（SIMD），index 0 = 主方向光",
    ], C_DATA)

    s.arrow(770 + 240, 190, 770 + 240, 240)
    s.box(770, 240, 480, 100, "剔除", [
        "Frustum = cullingProjection × view",
        "Culler::intersects（每 8 个一组 SIMD）",
        "结果写 VISIBLE_MASK 位掩码",
    ], C_FRONT)
    s.arrow(770 + 240, 340, 770 + 240, 390)
    s.box(770, 390, 480, 100, "prepareVisibleRenderables", [
        "填充 PerRenderableData UBO：",
        "worldFromModelMatrix / 法线矩阵 / objectId / 实例标志",
    ], C_FRONT)
    s.arrow(770 + 240, 490, 770 + 240, 540)
    s.box(770, 540, 480, 90, "updatePrimitivesLod + 可见范围", [
        "按相机距离选 LOD",
        "得到 visibleRenderables Range",
    ], C_FRONT)
    s.arrow(770 + 240, 630, 770 + 240, 690, "交给 RenderPass", ly=670)
    s.box(770, 690, 480, 70, "RenderPass::appendCommands", [
        "生成 64 字节 draw 命令（见 renderpass.svg）",
    ], C_BACK)

    s.box(30, 260, 250, 120, "灯光剔除（与上面并行）", [
        "视锥对光源球体求交",
        "过滤：不投阴影 / 强度≤0 /",
        "不可能相交的聚光灯",
    ], C_FRONT)
    s.box(30, 440, 250, 110, "Froxelizer（动态光）", [
        "把可见点光/聚光灯切进",
        "视锥体素网格",
        "供着色器查询",
    ], C_FRONT)
    s.box(30, 610, 250, 120, "LightSoa 结果", [
        "mHasDirectionalLighting",
        "mHasDynamicLighting",
        "决定着色变体 DIR/DYN",
    ], C_DATA)
    s.arrow(155, 380, 155, 440)
    s.arrow(155, 550, 155, 610)
    s.arrow(280, 670, 770, 670, "灯光数据", dashed=True)
    s.save("scene-pipeline.svg")


def renderpass():
    s = SVG(1280, 760)
    s.text(640, 28, "RenderPass：从可见物体到排序后的 draw 命令", 19, C_TXT, bold=True)

    s.box(30, 80, 260, 100, "输入", [
        "visibleRenderables Range",
        "RenderableSoa（含 LOD 后 PRIMITIVES）",
    ], C_DATA)
    s.arrow(290, 120, 380, 120)
    s.box(380, 70, 300, 120, "generateCommands（可并行）", [
        "每 128 条命令一个 job",
        "逐 primitive 生成 Command",
        "变体/材质/混合状态写入",
    ], C_FRONT)
    s.arrow(680, 120, 770, 120)

    s.box(770, 50, 480, 170, "Command（64 字节，RenderPass.h）", [], C_DATA)
    s.box(800, 105, 420, 90, "CommandKey 位布局（8 字节）", [], C_DATA)
    fields = [
        (800 + 0 * 52, "CH", "61-62"),
        (800 + 1 * 52, "PASS", "58-59"),
        (800 + 2 * 52, "CUSTOM", "56-57"),
        (800 + 3 * 52, "BL", "53"),
        (800 + 4 * 52, "PRIO", "50-52"),
        (800 + 5 * 52, "Z", "32-41"),
        (800 + 6 * 52, "MAT", "20-31"),
        (800 + 7 * 52, "VAR", "12-19"),
    ]
    for x, name, bits in fields:
        s.box(x, 140, 48, 34, "", [name], C_GRAY, size=9, title_size=10)
        s.text(x + 24, 195, bits, 9, "#616161")
    s.text(1010, 225, "低 32 位 = 材质排序键；BLENDED 命令用距离排序", 11, "#616161")

    s.arrow(1010, 220, 1010, 280, "key 决定全局排序", ly=255)
    s.box(770, 280, 480, 100, "sortCommands（插入排序优化）", [
        "同 pass / 同材质 / 同混合状态相邻",
        "去掉 SENTINEL，得到命令区间 [begin, end)",
    ], C_FRONT)
    s.arrow(1010, 380, 1010, 440)
    s.box(770, 440, 480, 90, "finalize + Executor", [
        "把命令区间包装成可执行闭包",
        "传给 FrameGraph 颜色 pass",
    ], C_FRONT)

    s.box(30, 300, 260, 130, "CommandTypeFlags", [
        "COLOR / DEPTH",
        "SHADOW = DEPTH + shadow casters",
        "SSAO = DEPTH + 滤掉半透明",
        "SCREEN_SPACE_REFLECTIONS",
    ], C_GRAY)
    s.box(30, 480, 260, 130, "排序键里的其它维度", [
        "channel（通道）",
        "自定义命令 PROLOGUE/PASS/EPILOGUE",
        "混合顺序 / 双面渲染",
    ], C_GRAY)
    s.arrow(290, 540, 770, 540, "自定义命令（清屏等）", dashed=True)
    s.save("renderpass.svg")


def framegraph():
    s = SVG(1440, 900)
    s.text(720, 28, "FrameGraph：虚拟资源图 → 编译 → 执行", 19, C_TXT, bold=True)

    s.box(30, 70, 430, 130, "构建：addPass(name, setup, execute)", [
        "setup 同步执行：builder.read/write/sample/create",
        "只声明资源依赖，不发命令",
        "execute 存起来，fg.execute() 才执行",
    ], C_FRONT)
    s.arrow(460, 120, 560, 120)

    s.box(560, 60, 430, 160, "compile()", [
        "① 依赖图 cull：无引用的 pass 被裁剪",
        "② 每个资源算 refcount / first / last",
        "③ 生成 devirtualize（首个使用者）与",
        "   destroy（末个使用者）列表",
        "④ 合并 usage 位",
    ], C_FRONT)
    s.arrow(990, 120, 1090, 120)

    s.box(1090, 60, 330, 160, "execute(driver)", [
        "按 pass 顺序：",
        "devirtualize → node->execute(driver)",
        "→ destroy",
        "真正发命令的地方",
    ], C_BACK)

    s.box(560, 270, 860, 110, "本帧的 pass 链（renderJob 构建）", [], C_FRONT)
    passes = ["Shadow pass", "Structure/SSAO", "Color pass", "Refraction", "TAA/Bloom/DoF", "色调映射/FXAA", "present"]
    x = 580
    for i, p in enumerate(passes):
        w = 110
        s.box(x, 320, w - 8, 44, "", [p], C_DATA, size=10, title_size=11)
        if i < len(passes) - 1:
            s.arrow(x + w - 8, 342, x + w + 4, 342)
        x += w + 8

    s.box(30, 280, 460, 110, "VirtualResource", [
        "声明阶段只有 descriptor（虚拟的）",
        "refcount / first / last 在 compile 计算",
    ], C_DATA)
    s.arrow(250, 390, 250, 450)
    s.box(30, 450, 460, 130, "devirtualize → 实例化", [
        "FrameGraphTexture::create",
        "→ TextureCache::createTexture",
        "→ driverApi.createTexture（入队）",
        "→ OpenGL 纹理对象",
    ], C_GL)
    s.box(30, 630, 460, 120, "TextureCache（资源池）", [
        "按 descriptor 做键缓存",
        "跨帧复用，延迟销毁（gc）",
        "避免每帧反复创建中间缓冲",
    ], C_GRAY)
    s.arrow(250, 580, 250, 630)

    s.box(1090, 300, 330, 110, "import viewRenderTarget", [
        "外部资源 = 默认 RenderTarget",
        "本项目 = Qt 的 FBO",
    ], C_QT)
    s.arrow(1230, 410, 1230, 480, "forwardResource + present", ly=450)
    s.box(1090, 480, 330, 110, "最终输出", [
        "后处理结果被转发进 Qt FBO",
        "Filament 自己不 swap",
    ], C_QT)

    s.box(560, 430, 860, 100, "present pass（副作用）", [
        "fg.present(viewRenderTarget)：声明输出副作用，防止被依赖图裁剪",
    ], C_GRAY)
    s.arrow(990, 330, 990, 430, "pass 链", dashed=True)
    s.arrow(720, 530, 720, 590)
    s.box(560, 590, 860, 100, "FrameGraph 自身的 arena", [
        "PassNode / ResourceNode 分配在 mArena（256 KiB），随 fg 析构",
    ], C_GRAY)
    s.save("framegraph.svg")


def arena():
    s = SVG(1240, 800)
    s.text(620, 28, "Arena 分配器：一帧内的三层内存", 19, C_TXT, bold=True)

    s.box(60, 70, 500, 180, "FEngine::mPerRenderPassArena", [
        "LinearAllocatorArena（线性 bump）",
        "Engine 创建时按 perRenderPassArenaSizeMB 分配",
        "跨帧复用，整块常驻",
    ], C_DATA)
    s.arrow(310, 250, 310, 320, "每帧 renderInternal 里：", ly=290)
    s.box(60, 320, 500, 120, "RootArenaScope rootArenaScope(arena)", [
        "构造：记录 rewind 点",
        "作用域内 allocate = 指针后移（零开销）",
        "析构：finalizer 链 + arena.rewind()",
    ], C_FRONT)
    s.arrow(310, 440, 310, 510, "allocate(perFrameCommandsSize)", ly=480)
    s.box(60, 510, 500, 150, "per-frame 命令区", [
        "RenderPass::Arena（StaticArea +",
        "LinearAllocatorWithFallback + HighWatermark）",
        "64 字节 Command[] 都分配在这里",
        "renderJob 结束整块归还",
    ], C_BACK)
    s.text(310, 690, "溢出 → debug 构建 abort，调大 Engine::Config::perFrameCommandsSizeMB", 12, "#C62828", bold=True)

    s.box(680, 70, 500, 180, "renderJob 里其它临时对象", [
        "剔除距离缓冲、灯光距离缓冲……",
        "全部从 RootArenaScope 分配",
        "作用域结束一并回收",
    ], C_GRAY)
    s.arrow(680 + 250, 250, 680 + 250, 320)
    s.box(680, 320, 500, 140, "FrameGraph 内部 mArena（独立）", [
        "“FrameGraph Arena”, 262144 字节（256 KiB）",
        "PassNode / ResourceNode / 依赖边",
        "随 fg 对象析构",
    ], C_GRAY)
    s.arrow(680 + 250, 460, 680 + 250, 530)
    s.box(680, 530, 500, 130, "为什么用 Arena", [
        "每帧大量短命对象，逐对象 free 是浪费",
        "线性分配 + 整块回退 = 无碎片、无锁",
        "HighWatermark 监控水位，指导调参",
    ], C_FRONT)

    s.save("arena.svg")


def command_stream():
    s = SVG(1400, 900)
    s.text(700, 28, "命令流：环形缓冲 + 生产/消费 + 分发", 19, C_TXT, bold=True)

    # 生产者
    s.box(40, 70, 560, 150, "生产者（前端线程 / UI 线程）", [], C_FRONT)
    s.box(60, 120, 250, 80, "DriverApi::method(args)", [
        "DECL_DRIVER_API 宏生成",
    ], C_FRONT)
    s.arrow(310, 150, 390, 150)
    s.box(390, 110, 190, 90, "Command 构造", [
        "allocateCommand()",
        "placement new",
        "函数指针 + 参数 tuple",
    ], C_DATA)

    s.arrow(310, 220, 310, 290)
    s.box(40, 290, 560, 150, "CircularBuffer（环形缓冲）", [
        "固定容量、页对齐",
        "allocate = mHead += size（bump）",
        "head/tail 指针",
    ], C_BACK)
    s.arrow(310, 440, 310, 510, "flush()", ly=480)
    s.box(40, 510, 560, 180, "CommandBufferQueue", [
        "追加 NoopCommand(nullptr) 终止命令",
        "把 [tail, head) 压入 mCommandBuffersToExecute",
        "mCondition.notify_one()",
        "剩余空间 < requiredSize → 阻塞等待",
        "溢出 → “Backend CommandStream overflow”",
    ], C_BACK)

    s.arrow(600, 600, 720, 600, "notify / wait", lx=660, ly=585)

    # 消费者
    s.box(720, 60, 640, 150, "消费者（驱动线程，或单线程构建的 execute()）", [], C_GL)
    s.box(740, 110, 280, 80, "waitForCommands()", [
        "多线程：阻塞等待",
        "单线程：直接返回缓冲",
    ], C_GL)
    s.arrow(1020, 140, 1100, 140)
    s.box(1100, 100, 240, 90, "CommandStream::execute(buffer)", [
        "while (p) p = p->execute(driver)",
    ], C_GL)

    s.arrow(1240, 210, 1240, 280)
    s.box(720, 280, 640, 130, "CommandBase 链", [
        "CommandBase { Execute mExecute }   // 函数指针",
        "Command<&Driver::method> { std::tuple<参数...> mArgs }",
        "mExecute → Dispatcher 函数指针表",
    ], C_DATA)
    s.arrow(1240, 410, 1240, 480)
    s.box(720, 480, 640, 140, "ConcreteDispatcher<OpenGLDriver>::method", [
        "static_cast<OpenGLDriver&>(driver)",
        "Cmd::execute(&OpenGLDriver::method, driver, base, next)",
        "apply() 展开 tuple → (driver.*method)(args...)",
        "→ OpenGLDriver 成员函数 → 真实 GL 调用",
    ], C_BACK)

    s.arrow(1240, 620, 1240, 700, "releaseBuffer() 归还空间", dashed=True, ly=665)
    s.box(720, 700, 640, 90, "FEngine::loop（多线程） / Engine::execute（单线程）", [
        "多线程：命令在驱动线程执行",
        "单线程（本项目）：命令在 GUI 线程执行",
    ], C_GRAY)
    s.save("command-stream.svg")


def qt_integration():
    s = SVG(1240, 800)
    s.text(620, 28, "Qt 集成层：PlatformGlfwGL 与 FBO 链路", 19, C_TXT, bold=True)

    s.box(40, 70, 360, 140, "DebugOpenGLWidget", [
        "initializeGL：build engine / swapchain",
        "paintGL：render + execute",
        "resizeEvent：viewport + manipulator",
    ], C_QT)
    s.arrow(400, 120, 500, 120)

    s.box(500, 60, 420, 170, "PlatformGlfwGL（自定义 OpenGLPlatform）", [], C_QT)
    s.box(520, 110, 380, 100, "关键 override", [
        "createSwapChain → 原样返回 widget 指针",
        "makeCurrent → true（no-op）",
        "commit → no-op",
        "getDefaultFramebufferObject → Qt FBO",
    ], C_QT)
    s.arrow(920, 130, 1020, 130)
    s.box(1020, 60, 190, 150, "OpenGLDriver", [
        "创建时 bluegl::bind()",
        "查询 FBO / makeCurrent / commit",
    ], C_GL)

    s.box(40, 300, 360, 150, "SwapChain = QOpenGLWidget*", [
        "createSwapChain(this, 0)",
        "Filament 认为它是 nativeWindow",
        "真正渲染目标是 Qt 的 FBO",
    ], C_DATA)
    s.arrow(400, 360, 520, 360, "makeCurrent / commit 无操作的前提：", lx=420, ly=345)
    s.box(500, 300, 420, 150, "Qt 负责上下文与呈现", [
        "paintGL 时 context 已 current",
        "Qt 合成器呈现 widget 内容",
        "Filament 不 swap，只写 FBO",
    ], C_GL)
    s.arrow(920, 360, 1020, 360)
    s.box(1020, 300, 190, 150, "单线程构建", [
        "FILAMENT_SINGLE_THREADED",
        "Engine::execute() 在 GUI 线程",
    ], C_WARN)

    s.box(40, 540, 1160, 90, "改多线程时要注意", [
        "去掉 FILAMENT_SINGLE_THREADED 后 Engine::execute() 会 precondition panic",
        "PlatformGlfwGL 的 makeCurrent/commit 必须实现真实语义（QOpenGLContext 迁移）",
        "QOpenGLWidget 的 FBO 只在 context current 时有效",
    ], C_WARN)
    s.arrow(310, 450, 310, 540, "高 DPI / resize：viewport 用物理像素", dashed=True)
    s.save("qt-integration.svg")


def performance_breakdown():
    s = SVG(1000, 560)
    s.text(500, 30, "Release 帧时间构成（23469 draw calls，1.58M 三角形，1280x800）", 19, C_TXT, bold=True)

    x0 = 280
    y0 = 90
    bar_h = 46
    gap = 26
    scale = 18.0  # px per ms
    total_ms = 34.0
    rows = [
        ("总帧时间", 34.0, C_GRAY, "34.0 ms（29.4 fps）"),
        ("frontend(cpu)", 9.2, C_FRONT, "9.2 ms：场景解析/剔除/命令生成/FrameGraph"),
        ("backend(cpu)", 23.7, C_BACK, "23.7 ms：GL 提交与状态切换（≈1.0µs/draw）"),
        ("gpu", 22.8, C_GL, "22.8 ms：三角形光栅化 + 后处理（被 CPU 掩盖）"),
    ]
    for i, (name, ms, (fill, stroke), note) in enumerate(rows):
        y = y0 + i * (bar_h + gap)
        w = ms * scale
        s.box(x0, y, w, bar_h, "", [name], fill, stroke, size=12, title_size=13)
        s.text(x0 + w + 12, y + bar_h / 2 + 4, note, 12, "#424242", anchor="start")
        s.text(x0 - 10, y + bar_h / 2 + 4, f"{ms:.1f}ms", 12, "#212121", anchor="end", bold=True)

    s.text(500, y0 + 4 * (bar_h + gap) + 16,
        "GPU 时间与 backend 并行（负的 other 即来自重叠），CPU 侧 frontend+backend ≈ 33ms 主导帧时间",
        12, "#616161")
    s.save("performance-breakdown.svg")


def perf_compare():
    s = SVG(1000, 420)
    s.text(500, 30, "多线程扫描：draw call 数量 vs 帧时间（1090万三角形，RTX 4060 Laptop）", 19, C_TXT, bold=True)

    x0 = 300
    y0 = 90
    bar_h = 52
    gap = 26
    scale = 16.0  # px per ms
    rows = [
        ("1 draw call", 2.7, "165 fps（GPU 基线）", C_GL),
        ("235 draws", 6.0, "165 fps", C_FRONT),
        ("4694 draws", 6.5, "≈155 fps", C_FRONT),
        ("23469 draws", 29.0, "≈33 fps（backend 瓶颈）", C_BACK),
    ]
    for i, (name, ms, fps, (fill, stroke)) in enumerate(rows):
        y = y0 + i * (bar_h + gap)
        w = ms * scale
        s.box(x0, y, w, bar_h, "", [name], fill, stroke, size=12, title_size=13)
        s.text(x0 + w + 12, y + bar_h / 2 + 4, f"{ms:.1f} ms / {fps}", 12, "#424242", anchor="start")
        s.text(x0 - 10, y + bar_h / 2 + 4, f"{ms:.1f}", 12, "#212121", anchor="end", bold=True)

    s.text(500, y0 + 4 * (bar_h + gap) + 16,
        "三角形 158万→971万→1090万 后各档帧率基本不变：瓶颈是 draw call 提交，不是面数",
        12, "#616161")
    s.save("perf-compare.svg")


def main():
    overview()
    call_chain()
    scene_pipeline()
    renderpass()
    framegraph()
    arena()
    command_stream()
    qt_integration()
    performance_breakdown()
    perf_compare()
    # 校验 XML 良构
    for name in os.listdir(OUT_DIR):
        if name.endswith(".svg"):
            ET.parse(os.path.join(OUT_DIR, name))
            print("ok:", name)


if __name__ == "__main__":
    main()
