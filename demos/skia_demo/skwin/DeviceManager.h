#ifndef __DEVICEMANAGER_H__
#define __DEVICEMANAGER_H__

#include "SkSurface.h"

#include "GrContextOptions.h"

class CLyraWindow;
class GrContext;
class GrRenderTarget;
class SkCanvas;

enum DeviceType {
	kRaster_DeviceType,
#if SK_SUPPORT_GPU
	kGPU_DeviceType,
#if SK_ANGLE
	kANGLE_DeviceType,
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
	kDeviceTypeCnt
};

bool IsGpuDeviceType(DeviceType devType);

/**
* SampleApp ports can subclass this manager class if they want to:
*      * filter the types of devices supported
*      * customize plugging of SkBaseDevice objects into an SkCanvas
*      * customize publishing the results of draw to the OS window
*      * manage GrContext / GrRenderTarget lifetimes
*/
class DeviceManager
{
public:
	struct BackendOptions {
#if SK_SUPPORT_GPU
		GrContextOptions   fGrContextOptions;
		int                fMSAASampleCount;
		bool               fDeepColor;
#endif
	};

	virtual void setUpBackend(CLyraWindow* win, const BackendOptions&) = 0;

	virtual void tearDownBackend(CLyraWindow* win) = 0;

	// called before drawing. should install correct device
	// type on the canvas. Will skip drawing if returns false.
	virtual sk_sp<SkSurface> makeSurface(DeviceType dType, CLyraWindow* win) = 0;

	// called after drawing, should get the results onto the
	// screen.
	virtual void publishCanvas(DeviceType dType,
		SkCanvas* canvas,
		CLyraWindow* win) = 0;

	// called when window changes size, guaranteed to be called
	// at least once before first draw (after init)
	virtual void windowSizeChanged(CLyraWindow* win) = 0;

	// return the GrContext backing gpu devices (NULL if not built with GPU support)
	virtual GrContext* getGrContext() = 0;

	// return the GrRenderTarget backing gpu devices (NULL if not built with GPU support)
	virtual int numColorSamples() const = 0;

	virtual int getColorBits() = 0;

private:
	typedef SkRefCnt INHERITED;

};

#endif//__DEVICEMANAGER_H__
