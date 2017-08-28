
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkOSWindow_Win_DEFINED
#define SkOSWindow_Win_DEFINED

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 ) 

#if SK_ANGLE
#include "EGL/egl.h"
#endif

// end : ignore skia dll warnings
#pragma warning( pop )

#include "SkWindow.h"

class SkOSWindow : public SkWindow {
public:
    SkOSWindow(void* hwnd);
    virtual ~SkOSWindow();

    void*   getHWND() const { return fHWND; }
    void    setSize(int width, int height);
    void    updateSize();

	struct AttachmentInfo {
		AttachmentInfo()
			: fSampleCount(0)
			, fStencilBits(0)
			, fColorBits(0) {}

		int fSampleCount;
		int fStencilBits;
		int fColorBits;
	};

    bool attach(SkBackEndTypes attachType, int msaaSampleCount, bool deepColor, AttachmentInfo*);
    void detach();
    void present();


protected:
    virtual bool quitOnDeactivate() { return true; }

    // overrides from SkWindow
    virtual void onHandleInval(const SkIRect&);

	void onSetTitle(const char title[]);


private:
    void*               fHWND;

public:
    void                doPaint(void* ctx);
protected:

#if SK_SUPPORT_GPU
    void*               fHGLRC;
#if SK_ANGLE
    EGLDisplay          fDisplay;
    EGLContext          fContext;
    EGLSurface          fSurface;
    EGLConfig           fConfig;
	sk_sp<const GrGLInterface> fANGLEInterface;
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU

    HMENU               fMBar;

    SkBackEndTypes      fAttached;

#if SK_SUPPORT_GPU
    bool attachGL(int msaaSampleCount, bool deepColor, AttachmentInfo* info);
    void detachGL();
    void presentGL();

#if SK_ANGLE
    bool attachANGLE(int msaaSampleCount, AttachmentInfo* info);
    void detachANGLE();
    void presentANGLE();
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU


private:
	typedef SkWindow INHERITED;
};

#endif
