#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include "camera.h"
#include "editor.h"
#include "icons.h"
#include "mesh.h"
#include "renderer.h"
#include "ui.h"
#include "uidraw.h"

int main()
{
    gfxInitDefault();
    gfxSet3D(true); // stereoscopic top screen, driven by the 3D slider
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    Renderer renderer;
    if (!renderer.init())
        return 1;

    ui::init();
    uidraw::init(); // widget label buffer, before Editor builds its menus
    icons::init();

    Editor editor;
    editor.scene.objects.push_back(makeCube(1.0f));
    renderer.uploadTexture(editor.scene.texture.data());

    Camera camera;
    u32 prevTicks = osGetTime();

    while (aptMainLoop())
    {
        hidScanInput();
        const u32 kDown = hidKeysDown();
        const u32 kUp = hidKeysUp();
        if ((kDown & KEY_START) || editor.wantQuit)
            break;

        touchPosition touch;
        hidTouchRead(&touch);
        if (kDown & KEY_TOUCH)
            editor.handleTouchDown(touch.px, touch.py);
        if (hidKeysHeld() & KEY_TOUCH)
            editor.handleTouchMove(touch.px, touch.py);
        if (kUp & KEY_TOUCH)
            editor.handleTouchUp(touch.px, touch.py);

        editor.handleKeys(kDown);

        // run any pending file-menu action (may open the keyboard applet) here,
        // between frames
        editor.serviceFileOps();

        const u32 now = osGetTime();
        const float dt = (float)(now - prevTicks) / 1000.0f;
        prevTicks = now;
        editor.tickStatus(dt);

        circlePosition pad;
        hidCircleRead(&pad);
        if (editor.screen == AppScreen::Browser)
            editor.browser.update(dt);
        else if (editor.is2D())
            editor.tex.navCanvas(pad, hidKeysHeld());
        else
        {
            camera.update(pad, hidKeysHeld(), dt);
            editor.syncZoom(camera.distance);
        }

        if (editor.scene.textureDirty)
        {
            renderer.uploadTexture(editor.scene.texture.data());
            editor.scene.textureDirty = false;
        }

        // what the top screen previews (the editor scene, or the browser's pick)
        const bool browsing = editor.screen == AppScreen::Browser;
        const std::vector<Mesh>& previewObjs = browsing ? editor.browser.preview : editor.scene.objects;
        const Camera& previewCam = browsing ? editor.browser.camera : camera;
        const bool previewWire = browsing ? false : editor.wireframe;
        const float iod = osGet3DSliderState() * 0.7f; // eye offset, 0 when slider down

        renderer.beginFrame();

        // top screen, one pass per eye (mono when iod is 0)
        renderer.drawOn(renderer.topTarget());
        renderer.drawPreview(previewObjs, previewCam, previewWire, editor.shading, -iod);
        renderer.drawOn(renderer.rightTarget());
        renderer.drawPreview(previewObjs, previewCam, previewWire, editor.shading, iod);

        renderer.drawOn(renderer.bottomTarget());
        if (editor.screen == AppScreen::Editor && editor.is3D())
            editor.renderViewports(renderer);

        // hand it to citro2d for the UI. both eyes get the project-name overlay
        // (same coords = screen plane), bottom gets the toolbars/canvas.
        C2D_Prepare();
        C2D_SceneBegin(renderer.topTarget());
        ui::drawTop(editor);
        C2D_SceneBegin(renderer.rightTarget());
        ui::drawTop(editor);
        C2D_SceneBegin(renderer.bottomTarget());
        ui::draw(editor, renderer.texture());

        renderer.endFrame(); // flush
    }

    icons::exit();
    uidraw::exit();
    ui::exit();
    renderer.exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
