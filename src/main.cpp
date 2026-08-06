#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include "camera.h"
#include "editor.h"
#include "icons.h"
#include "mesh.h"
#include "renderer.h"
#include "ui.h"

int main()
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    Renderer renderer;
    if (!renderer.init())
        return 1;

    ui::init();
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

        const u32 now = osGetTime();
        const float dt = (float)(now - prevTicks) / 1000.0f;
        prevTicks = now;
        editor.tickStatus(dt);

        circlePosition pad;
        hidCircleRead(&pad);
        if (editor.is2D())
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

        renderer.beginFrame();

        renderer.drawOn(renderer.topTarget());
        renderer.drawPreview(editor.scene.objects, camera, editor.wireframe, editor.shading);

        renderer.drawOn(renderer.bottomTarget());
        if (editor.is3D())
            editor.renderViewports(renderer);

        // hand it to citro2d for the UI
        C2D_Prepare();
        C2D_SceneBegin(renderer.bottomTarget());
        ui::draw(editor, renderer.texture());

        renderer.endFrame(); // flush
    }

    icons::exit();
    ui::exit();
    renderer.exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
