#ifndef __DefaultDeviceManager_H__
#define __DefaultDeviceManager_H__

#include "DeviceManager.h"
#include "SkWindow.h"

struct GrGLInterface;

class CLyraWindow;

class DefaultDeviceManager : public DeviceManager
{
public:
	DefaultDeviceManager();
	virtual ~DefaultDeviceManager();

	virtual void setUpBackend(CLyraWindow* win, const BackendOptions& backendOptions);
	virtual void tearDownBackend(CLyraWindow *win);
	sk_sp<SkSurface> makeSurface(DeviceType dType, CLyraWindow* win);
	virtual void publishCanvas(DeviceType dType,
                               SkCanvas* canvas,
                               CLyraWindow* win);
	virtual void windowSizeChanged(CLyraWindow* win);
	virtual GrContext* getGrContext();
	virtual int numColorSamples() const;
	virtual int getColorBits();

	
private:
#if SK_SUPPORT_GPU
	GrContext*              fCurContext;
	const GrGLInterface*    fCurIntf;
	sk_sp<SkSurface>        fGpuSurface;
	int fMSAASampleCount;
	bool fDeepColor;
	int fActualColorBits;
#endif
	SkBackEndTypes fBackend;

	typedef DeviceManager INHERITED;
};

#endif//__DefaultDeviceManager_H__
