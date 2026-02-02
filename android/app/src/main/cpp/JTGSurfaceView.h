//
// Created by wilbert on 2026/2/2.
//

#ifndef HELLO2D_JTGSURFACEVIEW_H
#define HELLO2D_JTGSURFACEVIEW_H
#include "tgfx/gpu/opengl/egl/EGLWindow.h"

namespace hello2d {

    class JTGSurfaceView {
    public:
        explicit JTGSurfaceView(std::shared_ptr<tgfx::Window> window);
        ~JTGSurfaceView();

        bool onSurfaceCreated(EGLNativeWindowType nativeWindow);
        void draw();
    private:
        std::shared_ptr<tgfx::Window> window;
        ANativeWindow* nativeWindow = nullptr;
    };

} // hello2d

#endif //HELLO2D_JTGSURFACEVIEW_H
