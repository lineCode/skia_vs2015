#include "DefaultDeviceManager.h"

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 )  

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkColorFilter.h"
#if SK_SUPPORT_GPU
#include "gl/GrGLInterface.h"
#include "GLContext_angle.h"
#include "GrRenderTarget.h"
#include "GrContextOptions.h"
#include "GrContext.h"
#else
class GrContext;
#endif

#include "SkWindow.h"
#include "LyraWindow.h"

// end : ignore skia dll warnings
#pragma warning( pop )



bool IsGpuDeviceType(DeviceType devType) {
#if SK_SUPPORT_GPU
    switch (devType) {
        case kGPU_DeviceType:
#if SK_ANGLE
        case kANGLE_DeviceType:
#endif // SK_ANGLE
            return true;
        default:
            return false;
    }
#endif // SK_SUPPORT_GPU
    return false;
}

DefaultDeviceManager::DefaultDeviceManager() {
#if SK_SUPPORT_GPU
	fCurContext = nullptr;
	fCurIntf = nullptr;
	fMSAASampleCount = 0;
	fDeepColor = false;
	fActualColorBits = 0;
#endif
	fBackend = kNone_BackEndType;
}

DefaultDeviceManager::~DefaultDeviceManager() {
#if SK_SUPPORT_GPU
	SkSafeUnref(fCurContext);
	SkSafeUnref(fCurIntf);
#endif
}

void DefaultDeviceManager::setUpBackend(CLyraWindow* win, const BackendOptions& backendOptions) {
	SkASSERT(kNone_BackEndType == fBackend);

	fBackend = kNone_BackEndType;

#if SK_SUPPORT_GPU
	switch (win->getDeviceType()) {
	case kRaster_DeviceType:
		fBackend = kNone_BackEndType;
		break;
	case kGPU_DeviceType:
		fBackend = kNativeGL_BackEndType;
		break;
#if SK_ANGLE
	case kANGLE_DeviceType:
		// ANGLE is really the only odd man out
		fBackend = kANGLE_BackEndType;
		break;
#endif // SK_ANGLE
	default:
		SkASSERT(false);
		break;
	}
	SkOSWindow::AttachmentInfo attachmentInfo;
	bool result = win->attach(fBackend, backendOptions.fMSAASampleCount, 
		backendOptions.fDeepColor, &attachmentInfo);
	if (!result) {
		SkDebugf("Failed to initialize GL");
		return;
	}
	fMSAASampleCount = backendOptions.fMSAASampleCount;
	fDeepColor = backendOptions.fDeepColor;


	fActualColorBits = 24;
	if (attachmentInfo.fColorBits > 24)
		fActualColorBits = attachmentInfo.fColorBits;

	SkASSERT(NULL == fCurIntf);
	switch (win->getDeviceType()) {
	case kRaster_DeviceType:
		break;
		// fallthrough
	case kGPU_DeviceType:
		// all these guys use the native interface
		fCurIntf = GrGLCreateNativeInterface();
		break;
#if SK_ANGLE
	case kANGLE_DeviceType:
		fCurIntf = sk_gpu_angle::CreateANGLEGLInterface();
		break;
#endif // SK_ANGLE
	default:
		SkASSERT(false);
		break;
	}

	if (kNone_BackEndType != fBackend)
	{
		SkASSERT(nullptr == fCurContext);
		fCurContext = GrContext::MakeGL(fCurIntf, backendOptions.fGrContextOptions).release();

		if (nullptr == fCurContext || nullptr == fCurIntf) {
			// We need some context and interface to see results
			SkSafeUnref(fCurContext);
			SkSafeUnref(fCurIntf);
			fCurContext = nullptr;
			fCurIntf = nullptr;
			SkDebugf("Failed to setup 3D");

			win->detach();
		}
	}
#endif // SK_SUPPORT_GPU
	// call windowSizeChanged to create the render target
	this->windowSizeChanged(win);
}

void DefaultDeviceManager::tearDownBackend(CLyraWindow *win) {
#if SK_SUPPORT_GPU
	if (fCurContext) {
		// in case we have outstanding refs to this guy (lua?)
		fCurContext->abandonContext();
		fCurContext->unref();
		fCurContext = nullptr;
	}

	SkSafeUnref(fCurIntf);
	fCurIntf = NULL;

	fGpuSurface = nullptr;
#endif
	win->detach();
	fBackend = kNone_BackEndType;
}
sk_sp<SkSurface> DefaultDeviceManager::makeSurface(DeviceType dType, CLyraWindow* win) {
#if SK_SUPPORT_GPU
	if (IsGpuDeviceType(dType) && fCurContext) {
		SkSurfaceProps props(win->getSurfaceProps());
		if (kRGBA_F16_SkColorType == win->info().colorType()|| fActualColorBits > 24) 
		{
			// If we're rendering to F16, we need an off-screen surface - the current render
// target is most likely the wrong format.
//
// If we're using a deep (10-bit or higher) surface, we probably need an off-screen
// surface. 10-bit, in particular, has strange gamma behavior.
			return SkSurface::MakeRenderTarget(fCurContext, SkBudgeted::kNo, win->info(),
				fMSAASampleCount, &props);

		}
		else {
			return fGpuSurface;
		}
	}
#endif
	return nullptr;
}

void DefaultDeviceManager::publishCanvas(DeviceType dType,
	SkCanvas* renderingCanvas,
	CLyraWindow* win) {
#if SK_SUPPORT_GPU
	if (fGpuSurface) {

		if (!IsGpuDeviceType(dType)) {
			// need to send the raster bits to the (gpu) window
			SkImageInfo info = win->info();
			size_t rowBytes = info.minRowBytes();
			size_t size = info.getSafeSize(rowBytes);
			auto data = SkData::MakeUninitialized(size);
			SkASSERT(data);

			if (!renderingCanvas->readPixels(info, data->writable_data(), rowBytes, 0, 0)) {
				SkDEBUGFAIL("Failed to read canvas pixels");
				return;
			}

			// Now, re-interpret those pixels as sRGB, so they won't be color converted when we
			// draw then to FBO0. This ensures that if we rendered in any strange gamut, we'll see
			// the "correct" output (because we generated the pixel values we wanted in the
			// offscreen canvas).
			auto colorSpace = kRGBA_F16_SkColorType == info.colorType()
				? SkColorSpace::MakeSRGBLinear()
				: SkColorSpace::MakeSRGB();
			auto offscreenImage = SkImage::MakeRasterData(info.makeColorSpace(colorSpace), data,
				rowBytes);

			SkCanvas* gpuCanvas = fGpuSurface->getCanvas();

			// With ten-bit output, we need to manually apply the gamma of the output device
			// (unless we're in non-gamma correct mode, in which case our data is already
			// fake-sRGB, like we're expected to put in the 10-bit buffer):
			bool doGamma = (fActualColorBits == 30) && win->info().colorSpace();

			SkPaint gammaPaint;
			gammaPaint.setBlendMode(SkBlendMode::kSrc);
			if (doGamma) {
				gammaPaint.setColorFilter(SkColorFilter::MakeLinearToSRGBGamma());
			}

			gpuCanvas->drawImage(offscreenImage, 0, 0, &gammaPaint);
		}

		fGpuSurface->prepareForExternalIO();
	}
#endif

	win->present();
}

void DefaultDeviceManager::windowSizeChanged(CLyraWindow* win) {
#if SK_SUPPORT_GPU
	if (fCurContext) {
		SkOSWindow::AttachmentInfo attachmentInfo;
		win->attach(fBackend, fMSAASampleCount, fDeepColor, &attachmentInfo);
		fActualColorBits = SkTMax(attachmentInfo.fColorBits, 24);
		int w = (int)(win->width());
		int h = (int)(win->height());
		fGpuSurface = sk_gpu_angle::makeGpuBackedSurface(fMSAASampleCount, 0, 0, w, h, win->info(), 
			win->getSurfaceProps(), fCurIntf, fCurContext);
	}
#endif
}

GrContext* DefaultDeviceManager::getGrContext() {
#if SK_SUPPORT_GPU
	return fCurContext;
#else
	return NULL;
#endif
}

int DefaultDeviceManager::numColorSamples() const
{
#if SK_SUPPORT_GPU
	return fMSAASampleCount;
#else
	return 0;
#endif
}

int DefaultDeviceManager::getColorBits() 
{
#if SK_SUPPORT_GPU
	return fActualColorBits;
#else
	return 24;
#endif
}
