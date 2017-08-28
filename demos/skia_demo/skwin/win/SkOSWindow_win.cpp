
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <WindowsX.h>

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 )  

#include "SkTypes.h"

#if defined(SK_BUILD_FOR_WIN)

//#include <GL/gl.h>
#include "SkWGL.h"
#include "SkCanvas.h"
//#include "SkUtils.h"
#include "SkGraphics.h"

#if SK_ANGLE

#include "gl/GrGLAssembleInterface.h"
#include "gl/GrGLInterface.h"
#include "GLES2/gl2.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define GL_CALL(IFACE, X)                                 \
    SkASSERT(IFACE);                                      \
    do {                                                  \
        (IFACE)->fFunctions.f##X;                         \
    } while (false)

#endif

// end : ignore skia dll warnings
#pragma warning( pop )

#include "..\SkWindow.h"

#define INVALIDATE_DELAY_MS 200

static SkOSWindow* gCurrOSWin;
static HWND gEventTarget;

#define WM_EVENT_CALLBACK (WM_USER+0)

void post_skwinevent()
{
    PostMessage(gEventTarget, WM_EVENT_CALLBACK, 0, 0);
}

SkOSWindow::SkOSWindow(void* hWnd) {
    fHWND = hWnd;
#if SK_SUPPORT_GPU
#if SK_ANGLE
    fDisplay = EGL_NO_DISPLAY;
    fContext = EGL_NO_CONTEXT;
    fSurface = EGL_NO_SURFACE;
#endif
    fHGLRC = NULL;
#endif
    fAttached = kNone_BackEndType;
    gEventTarget = (HWND)hWnd;
}

SkOSWindow::~SkOSWindow() {
#if SK_SUPPORT_GPU
    if (NULL != fHGLRC) {
        wglDeleteContext((HGLRC)fHGLRC);
    }
#if SK_ANGLE
    if (EGL_NO_CONTEXT != fContext) {
        eglDestroyContext(fDisplay, fContext);
        fContext = EGL_NO_CONTEXT;
    }

    if (EGL_NO_SURFACE != fSurface) {
        eglDestroySurface(fDisplay, fSurface);
        fSurface = EGL_NO_SURFACE;
    }

    if (EGL_NO_DISPLAY != fDisplay) {
        eglTerminate(fDisplay);
        fDisplay = EGL_NO_DISPLAY;
    }
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
}

static unsigned getModifiers(UINT message) {
    return 0;   // TODO
}

void SkOSWindow::doPaint(void* ctx) {
    this->update(NULL);

    if (kNone_BackEndType == fAttached)
    {
        HDC hdc = (HDC)ctx;
        const SkBitmap& bitmap = this->getBitmap();

        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = bitmap.width();
        bmi.bmiHeader.biHeight      = -bitmap.height(); // top-down image
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage   = 0;

        //
        // Do the SetDIBitsToDevice.
        //
        // TODO(wjmaclean):
        //       Fix this call to handle SkBitmaps that have rowBytes != width,
        //       i.e. may have padding at the end of lines. The SkASSERT below
        //       may be ignored by builds, and the only obviously safe option
        //       seems to be to copy the bitmap to a temporary (contiguous)
        //       buffer before passing to SetDIBitsToDevice().
        SkASSERT(bitmap.width() * bitmap.bytesPerPixel() == bitmap.rowBytes());
        int ret = SetDIBitsToDevice(hdc,
            0, 0,
            bitmap.width(), bitmap.height(),
            0, 0,
            0, bitmap.height(),
            bitmap.getPixels(),
            &bmi,
            DIB_RGB_COLORS);
        (void)ret; // we're ignoring potential failures for now.
    }
}

static VOID CALLBACK InvalTimerProc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
    DWORD dwTime)     // current system time 
{ 
 
    //RECT* rect = (RECT*)wParam;
    InvalidateRect(hwnd, NULL, FALSE);
}

void SkOSWindow::onHandleInval(const SkIRect& r) {
    /*RECT* rect = new RECT;
    rect->left    = r.fLeft;
    rect->top     = r.fTop;
    rect->right   = r.fRight;
    rect->bottom  = r.fBottom;*/
    //SetTimer((HWND)fHWND, (UINT_PTR)NULL, INVALIDATE_DELAY_MS, InvalTimerProc);
}

static UINT_PTR gTimer;

#if SK_SUPPORT_GPU

bool SkOSWindow::attachGL(int msaaSampleCount, bool deepColor, AttachmentInfo* info) {
    HDC dc = GetDC((HWND)fHWND);
    if (NULL == fHGLRC) {
		fHGLRC = SkCreateWGLContext(dc, msaaSampleCount, deepColor,
			kGLPreferCompatibilityProfile_SkWGLContextRequest);
        if (NULL == fHGLRC) {
            return false;
        }
        glClearStencil(0);
        glClearColor(0, 0, 0, 0);
        glStencilMask(0xffffffff);
        glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
    if (wglMakeCurrent(dc, (HGLRC)fHGLRC)) {
        // use DescribePixelFormat to get the stencil bit depth.
        int pixelFormat = GetPixelFormat(dc);
        PIXELFORMATDESCRIPTOR pfd;
        DescribePixelFormat(dc, pixelFormat, sizeof(pfd), &pfd);
        info->fStencilBits = pfd.cStencilBits;

        // Get sample count if the MSAA WGL extension is present
        SkWGLExtensions extensions;
        if (extensions.hasExtension(dc, "WGL_ARB_multisample")) {
            static const int kSampleCountAttr = SK_WGL_SAMPLES;
            extensions.getPixelFormatAttribiv(dc,
                                              pixelFormat,
                                              0,
                                              1,
                                              &kSampleCountAttr,
                                              &info->fSampleCount);
        } else {
            info->fSampleCount = 0;
        }

        glViewport(0, 0,
                   SkScalarRoundToInt(this->width()),
                   SkScalarRoundToInt(this->height()));
        return true;
    }
    return false;
}

void SkOSWindow::detachGL() {
    wglMakeCurrent(GetDC((HWND)fHWND), 0);
    wglDeleteContext((HGLRC)fHGLRC);
    fHGLRC = NULL;
}

void SkOSWindow::presentGL() {
    glFlush();
    HDC dc = GetDC((HWND)fHWND);
    SwapBuffers(dc);
    ReleaseDC((HWND)fHWND, dc);
}

#if SK_ANGLE

static void* get_angle_egl_display(void* nativeDisplay) {
	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
	eglGetPlatformDisplayEXT =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

	// We expect ANGLE to support this extension
	if (!eglGetPlatformDisplayEXT) {
		return EGL_NO_DISPLAY;
	}

	EGLDisplay display = EGL_NO_DISPLAY;
	// Try for an ANGLE D3D11 context, fall back to D3D9, and finally GL.
	EGLint attribs[3][3] = {
		{
			EGL_PLATFORM_ANGLE_TYPE_ANGLE,
			EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
			EGL_NONE
		},
		{
			EGL_PLATFORM_ANGLE_TYPE_ANGLE,
			EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE,
			EGL_NONE
		},
	};
	for (int i = 0; i < 3 && display == EGL_NO_DISPLAY; ++i) {
		display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, nativeDisplay, attribs[i]);
	}
	return display;
}

struct ANGLEAssembleContext {
	ANGLEAssembleContext() {
		fEGL = GetModuleHandleA("libEGL.dll");
		fGL = GetModuleHandleA("libGLESv2.dll");
	}

	bool isValid() const { return SkToBool(fEGL) && SkToBool(fGL); }

	HMODULE fEGL;
	HMODULE fGL;
};

static GrGLFuncPtr angle_get_gl_proc(void* ctx, const char name[]) {
	const ANGLEAssembleContext& context = *reinterpret_cast<const ANGLEAssembleContext*>(ctx);
	GrGLFuncPtr proc = (GrGLFuncPtr)GetProcAddress(context.fGL, name);
	if (proc) {
		return proc;
	}
	proc = (GrGLFuncPtr)GetProcAddress(context.fEGL, name);
	if (proc) {
		return proc;
	}
	return eglGetProcAddress(name);
}

static const GrGLInterface* get_angle_gl_interface() {
	ANGLEAssembleContext context;
	if (!context.isValid()) {
		return nullptr;
	}
	return GrGLAssembleGLESInterface(&context, angle_get_gl_proc);
}

bool create_ANGLE(EGLNativeWindowType hWnd,
                  int msaaSampleCount,
                  EGLDisplay* eglDisplay,
                  EGLContext* eglContext,
                  EGLSurface* eglSurface,
                  EGLConfig* eglConfig) {
    static const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE, EGL_NONE
    };
    static const EGLint configAttribList[] = {
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     8,
        EGL_DEPTH_SIZE,     8,
        EGL_STENCIL_SIZE,   8,
        EGL_NONE
    };
    static const EGLint surfaceAttribList[] = {
        EGL_NONE, EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(GetDC(hWnd));
    if (display == EGL_NO_DISPLAY ) {
       return false;
    }

    // Initialize EGL
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(display, &majorVersion, &minorVersion)) {
       return false;
    }

    EGLint numConfigs;
    if (!eglGetConfigs(display, NULL, 0, &numConfigs)) {
       return false;
    }

    // Choose config
    bool foundConfig = false;
    if (msaaSampleCount) {
        static const int kConfigAttribListCnt =
                                SK_ARRAY_COUNT(configAttribList);
        EGLint msaaConfigAttribList[kConfigAttribListCnt + 4];
        memcpy(msaaConfigAttribList,
               configAttribList,
               sizeof(configAttribList));
        SkASSERT(EGL_NONE == msaaConfigAttribList[kConfigAttribListCnt - 1]);
        msaaConfigAttribList[kConfigAttribListCnt - 1] = EGL_SAMPLE_BUFFERS;
        msaaConfigAttribList[kConfigAttribListCnt + 0] = 1;
        msaaConfigAttribList[kConfigAttribListCnt + 1] = EGL_SAMPLES;
        msaaConfigAttribList[kConfigAttribListCnt + 2] = msaaSampleCount;
        msaaConfigAttribList[kConfigAttribListCnt + 3] = EGL_NONE;
        if (eglChooseConfig(display, configAttribList, eglConfig, 1, &numConfigs)) {
            SkASSERT(numConfigs > 0);
            foundConfig = true;
        }
    }
    if (!foundConfig) {
        if (!eglChooseConfig(display, configAttribList, eglConfig, 1, &numConfigs)) {
           return false;
        }
    }

    // Create a surface
    EGLSurface surface = eglCreateWindowSurface(display, *eglConfig,
                                                (EGLNativeWindowType)hWnd,
                                                surfaceAttribList);
    if (surface == EGL_NO_SURFACE) {
       return false;
    }

    // Create a GL context
    EGLContext context = eglCreateContext(display, *eglConfig,
                                          EGL_NO_CONTEXT,
                                          contextAttribs );
    if (context == EGL_NO_CONTEXT ) {
       return false;
    }

    // Make the context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
       return false;
    }

    *eglDisplay = display;
    *eglContext = context;
    *eglSurface = surface;
    return true;
}

bool SkOSWindow::attachANGLE(int msaaSampleCount, AttachmentInfo* info) {
    if (EGL_NO_DISPLAY == fDisplay) {
        bool bResult = create_ANGLE((HWND)fHWND,
                                    msaaSampleCount,
                                    &fDisplay,
                                    &fContext,
                                    &fSurface,
                                    &fConfig);
        if (false == bResult) {
            return false;
        }
		fANGLEInterface.reset(get_angle_gl_interface());
		if (!fANGLEInterface) {
			this->detachANGLE();
			return false;
		}
		GL_CALL(fANGLEInterface, ClearStencil(0));
		GL_CALL(fANGLEInterface, ClearColor(0, 0, 0, 0));
		GL_CALL(fANGLEInterface, StencilMask(0xffffffff));
		GL_CALL(fANGLEInterface, Clear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT));
	}
	if (!eglMakeCurrent(fDisplay, fSurface, fSurface, fContext)) {
		this->detachANGLE();
		return false;
	}
	eglGetConfigAttrib(fDisplay, fConfig, EGL_STENCIL_SIZE, &info->fStencilBits);
	eglGetConfigAttrib(fDisplay, fConfig, EGL_SAMPLES, &info->fSampleCount);

	GL_CALL(fANGLEInterface, Viewport(0, 0, SkScalarRoundToInt(this->width()),
		SkScalarRoundToInt(this->height())));
	return true;
}

void SkOSWindow::detachANGLE() {
    eglMakeCurrent(fDisplay, EGL_NO_SURFACE , EGL_NO_SURFACE , EGL_NO_CONTEXT);

    eglDestroyContext(fDisplay, fContext);
    fContext = EGL_NO_CONTEXT;

    eglDestroySurface(fDisplay, fSurface);
    fSurface = EGL_NO_SURFACE;

    eglTerminate(fDisplay);
    fDisplay = EGL_NO_DISPLAY;
}

void SkOSWindow::presentANGLE() {
	GL_CALL(fANGLEInterface, Flush());

	eglSwapBuffers(fDisplay, fSurface);
}
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU

// return true on success
bool SkOSWindow::attach(SkBackEndTypes attachType, int msaaSampleCount, bool deepColor, AttachmentInfo* info) {

    // attach doubles as "windowResize" so we need to allo
    // already bound states to pass through again
    // TODO: split out the resize functionality
//    SkASSERT(kNone_BackEndType == fAttached);
    bool result = true;

    switch (attachType) {
    case kNone_BackEndType:
        // nothing to do
        break;
#if SK_SUPPORT_GPU
    case kNativeGL_BackEndType:
        result = attachGL(msaaSampleCount, false, info);
        break;
#if SK_ANGLE
    case kANGLE_BackEndType:
        result = attachANGLE(msaaSampleCount, info);
        break;
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
    default:
        SkASSERT(false);
        result = false;
        break;
    }

    if (result) {
        fAttached = attachType;
    }

    return result;
}

void SkOSWindow::detach() {
    switch (fAttached) {
    case kNone_BackEndType:
        // nothing to do
        break;
#if SK_SUPPORT_GPU
    case kNativeGL_BackEndType:
        detachGL();
        break;
#if SK_ANGLE
    case kANGLE_BackEndType:
        detachANGLE();
        break;
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
    default:
        SkASSERT(false);
        break;
    }
    fAttached = kNone_BackEndType;
}

void SkOSWindow::present() {
    switch (fAttached) {
    case kNone_BackEndType:
        // nothing to do
        return;
#if SK_SUPPORT_GPU
    case kNativeGL_BackEndType:
        presentGL();
        break;
#if SK_ANGLE
    case kANGLE_BackEndType:
        presentANGLE();
        break;
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
    default:
        SkASSERT(false);
        break;
    }
}

void SkOSWindow::onSetTitle(const char title[]){
    SetWindowTextA((HWND)fHWND, title);
}
#endif
