#include "LyraWindow.h"

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 )  

#include "SkCanvas.h"

// end : ignore skia dll warnings
#pragma warning( pop )

#include "DefaultDeviceManager.h"

#include "..\SkiaTest.h"


CLyraWindow::CLyraWindow(void* hwnd, DeviceManager* dev)
	: SkOSWindow(hwnd)
	, fDevManager(NULL)
{
	fDeviceType = kRaster_DeviceType;

#if SK_ANGLE
    //fDeviceType = kANGLE_DeviceType;
#endif

	fDevManager = dev;

	fMSAASampleCount = 0;

#if SK_SUPPORT_GPU
	fBackendOptions.fGrContextOptions.fGpuPathRenderers = GrContextOptions::GpuPathRenderers::kAll;
	fBackendOptions.fMSAASampleCount = 0;
	fBackendOptions.fDeepColor = false;
#endif
}


CLyraWindow::~CLyraWindow(void)
{
}

void CLyraWindow::Init()
{

	if (!fDevManager)
	{
		fDevManager = new DefaultDeviceManager();
	}
	fDevManager->setUpBackend(this, fBackendOptions);
	
	if (this->height() && this->width()) {
        this->onSizeChange();
    }
}
void CLyraWindow::UnInit()
{
	if (fDevManager)
	{
		fDevManager->tearDownBackend(this);
		delete fDevManager;
		fDevManager = NULL;
	}
}

void CLyraWindow::setDeviceType(DeviceType type) {
    if (type == fDeviceType)
        return;

    fDevManager->tearDownBackend(this);

    fDeviceType = type;

    fDevManager->setUpBackend(this, fBackendOptions);

    this->inval(NULL);
}

sk_sp<SkSurface> CLyraWindow::makeSurface()  {
	sk_sp<SkSurface> surface;
	if (fDevManager) {
		surface = fDevManager->makeSurface(fDeviceType, this);
	}
	if (!surface) {
		surface = SkOSWindow::makeSurface();
	}
	return surface;
}
void CLyraWindow::draw(SkCanvas* canvas)
{
	SkOSWindow::draw(canvas);

	switch (fAttached)
	{
	case kNone_BackEndType:
		this->setTitle("skia demo : Raster");
		OutputDebugStringA("Raster\n");
		break;
#if SK_SUPPORT_GPU
	case kNativeGL_BackEndType:
		this->setTitle("skia demo : GPU");
		OutputDebugStringA("Device\n");
		break;
#if SK_ANGLE
	case kANGLE_BackEndType:
		this->setTitle("skia demo : ANGLE");
		OutputDebugStringA("ANGLE\n");
		break;
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
	default:
		this->setTitle("skia demo : default");
		OutputDebugStringA("default\n");
	}
	
	// do this last
    fDevManager->publishCanvas(fDeviceType, canvas, this);
}
void CLyraWindow::onDraw(SkCanvas* canvas)
{
	canvas->drawColor(SK_ColorWHITE);

	//draw
	CSkiaTest::getInstance()->Draw(canvas);
}
void CLyraWindow::onSizeChange()
{
	SkOSWindow::onSizeChange();

	fDevManager->windowSizeChanged(this);
}

