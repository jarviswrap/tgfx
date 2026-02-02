//
// Created by wilbert on 2026/2/2.
//

#include "JTGSurfaceView.h"
#include <jni.h>
#include <android/native_window_jni.h>


namespace hello2d {
    static jfieldID TGSurfaceView_nativePtr;

    JTGSurfaceView::JTGSurfaceView(std::shared_ptr<tgfx::Window> window):window(std::move(window)) {}

    bool JTGSurfaceView::onSurfaceCreated(EGLNativeWindowType wd) {
        auto device = std::static_pointer_cast<tgfx::EGLDevice>(window->getDevice());
        device->setNativeWindow(wd);
        nativeWindow = wd;
        return true;
    }

    void JTGSurfaceView::draw() {
        auto device = window->getDevice();
        auto context = device->lockContext();
        if (context == nullptr) {
            return;
        }

        auto surface = window->getSurface(context);
        if (surface == nullptr) {
            device->unlock();
            return;
        }

        auto canvas = surface->getCanvas();
        canvas->clear();
        auto width = ANativeWindow_getWidth(nativeWindow);
        auto height = ANativeWindow_getHeight(nativeWindow);
        auto density =
                     width > 0 ? static_cast<float>(surface->width()) / static_cast<float>(width) : 1.0f;
//        hello2d::DrawBackground(canvas, surface->width(), surface->height(), density);
//
//        displayList.render(surface.get(), false);
//
        tgfx::Paint paint;
        paint.setColor(tgfx::Color::Blue());
        canvas->drawCircle((float)width / 2, (float)height / 2, 100, paint);
        auto recording = context->flush();
//
//        // Delayed one-frame present
//        std::swap(lastRecording, recording);

        if (recording) {
            context->submit(std::move(recording));
            window->present(context);
        }

        device->unlock();
    }

    JTGSurfaceView::~JTGSurfaceView() = default;

} // hello2d

static hello2d::JTGSurfaceView* GetJTGSurfaceView(JNIEnv* env, jobject thiz) {
    return reinterpret_cast<hello2d::JTGSurfaceView*>(
            env->GetLongField(thiz, hello2d::TGSurfaceView_nativePtr));
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_tgfx_hello2d_TGSurfaceView_makeFakeSurfaceCurrent(JNIEnv *env, jclass clazz) {
    auto window = tgfx::EGLWindow::Make(nullptr, true);
    if (window == nullptr) {
        return 0;
    }
    auto jTGFXView = new hello2d::JTGSurfaceView(std::move(window));
    return reinterpret_cast<jlong>(jTGFXView);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_tgfx_hello2d_TGSurfaceView_nativeInit(JNIEnv *env, jclass clz) {
    auto clazz = env->FindClass("org/tgfx/hello2d/TGSurfaceView");
    hello2d::TGSurfaceView_nativePtr = env->GetFieldID(clazz, "mShowViewPtr", "J");
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_org_tgfx_hello2d_TGSurfaceView_makeWindowSurfaceCurrent(JNIEnv *env,
                                                             jobject thiz,
                                                             jobject surface) {
    auto view = GetJTGSurfaceView(env, thiz);
    if (view == nullptr) {
        return false;
    }
    auto nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr) {
        return 0;
    }
    return view->onSurfaceCreated(nativeWindow);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_org_tgfx_hello2d_TGSurfaceView_nativeRelease(JNIEnv *env, jobject thiz) {
    auto jTGFXView =
                 reinterpret_cast<hello2d::JTGSurfaceView*>(env->GetLongField(thiz, hello2d::TGSurfaceView_nativePtr));
    if (jTGFXView != nullptr) {
        delete jTGFXView;
        env->SetLongField(thiz, hello2d::TGSurfaceView_nativePtr, (jlong)0);
        return true;
    }
    return false;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_tgfx_hello2d_TGSurfaceView_nativeDraw(JNIEnv *env, jobject thiz) {
    auto view = GetJTGSurfaceView(env, thiz);
    if (view == nullptr) {
        return;
    }
    view->draw();
}