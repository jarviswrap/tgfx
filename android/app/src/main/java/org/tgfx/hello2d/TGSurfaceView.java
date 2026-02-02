package org.tgfx.hello2d;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL10;

public class TGSurfaceView extends GLSurfaceView {

    static {
        System.loadLibrary("hello2d");
        nativeInit();
    }

    private final String TAG = "EGLSurfaceView";
    private long mShowViewPtr = 0;
//    private final EGLEnvironment mEglEnvironment;

    public TGSurfaceView(Context context) {
        super(context);
//        mEglEnvironment = FaceKit.Instance.touchEGL();
//        mShowViewPtr = nativeCreateShowView(mEglEnvironment.getPtr());
        initialize();
    }

    public TGSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
//        mEglEnvironment = FaceKit.Instance.touchEGL();
//        mShowViewPtr = nativeCreateShowView(mEglEnvironment.getPtr());
        initialize();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        super.surfaceDestroyed(holder);
//        mEglEnvironment.release();
        if (mShowViewPtr != 0) {
//            nativeDestroyShowView(mShowViewPtr);
            mShowViewPtr = 0;
        }
    }

    private void initialize() {
        // 设置 EGL 配置
        setEGLConfigChooser(null); // EGLConfig真正的选择选择逻辑在cpp代码中

        // 设置 EGL Context Factory，创建 shared context
        setEGLContextFactory(new EGLContextFactory(){
            @Override
            public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
                mShowViewPtr = makeFakeSurfaceCurrent();//1. 先创建小的PBufferSurface避免报错
                EGLContext context = egl.eglGetCurrentContext();
                Log.e(TAG, "createContext, context:" + context + "; " + (EGL10.EGL_NO_CONTEXT == context) +",mShowViewPtr:" + mShowViewPtr);
                return context;
            }

            @Override
            public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
                Log.e(TAG, "destroyContext, context:" + context);
//                mEglEnvironment.destroyEGLContext();
            }
        });

        setEGLWindowSurfaceFactory(new EGLWindowSurfaceFactory() {
            @Override
            public EGLSurface createWindowSurface(EGL10 egl, EGLDisplay display, EGLConfig config, Object nativeWindow) {
                Log.e(TAG, "createWindowSurface, 0");
                Surface nativeSurface = null;
                if (nativeWindow instanceof SurfaceHolder) {
                    SurfaceHolder holder = (SurfaceHolder) nativeWindow;
                    nativeSurface = holder.getSurface();
                    Log.w(TAG, "[createWindowSurface] nativeWindow is SurfaceHolder");
                } else if (nativeWindow instanceof Surface) {
                    nativeSurface = (Surface) nativeWindow;
                    Log.w(TAG, "[createWindowSurface] nativeWindow is Surface");
                }
                Log.e(TAG, "createWindowSurface, context:" + nativeSurface);
                if (makeWindowSurfaceCurrent(nativeSurface)) {//2. 再创建WindowSurface用于上屏显示
                    return egl.eglGetCurrentSurface(EGL10.EGL_DRAW);
                }
                return EGL10.EGL_NO_SURFACE;
            }

            @Override
            public void destroySurface(EGL10 egl, EGLDisplay display, EGLSurface surface) {
                Log.e(TAG, "destroySurface, surface:" + surface);
                nativeRelease();
            }
        });
        setEGLContextClientVersion(2);
        setRenderer(new Renderer() {
            @Override
            public void onDrawFrame(GL10 gl) {
                nativeDraw();
            }

            @Override
            public void onSurfaceChanged(GL10 gl, int width, int height) {
                Log.e(TAG, "resize:" + width + "x" + height);
            }

            @Override
            public void onSurfaceCreated(GL10 gl, EGLConfig config) {
                Log.e(TAG, "onSurfaceCreated");
            }
        });
        setRenderMode(RENDERMODE_WHEN_DIRTY);
    }

    private static native void nativeInit();
    private static native long makeFakeSurfaceCurrent();
    private native boolean makeWindowSurfaceCurrent(Surface surface);
    private native boolean nativeRelease();
    private native void nativeDraw();
}
