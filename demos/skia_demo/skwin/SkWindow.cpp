
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 ) 

#include "SkWindow.h"
#include "SkSurface.h"
#include "SkCanvas.h"
#include "SkTime.h"

// end : ignore skia dll warnings
#pragma warning( pop )

#define SK_EventDelayInval "\xd" "n" "\xa" "l"

SkWindow::SkWindow() 
	: fWidth(0), fHeight(0)
	, fFlags(kVisible_Mask)
	, fSurfaceProps(SkSurfaceProps::kLegacyFontHost_InitType)
{
	fLoc.set(0, 0);
	fMatrix.setIdentity();

    fWaitingOnInval = false;

    fColorType = kN32_SkColorType;

    fMatrix.reset();
}

SkWindow::~SkWindow() {

}

sk_sp<SkSurface> SkWindow::makeSurface()
{
	const SkBitmap& bm = this->getBitmap();
	return SkSurface::MakeRasterDirect(bm.info(), bm.getPixels(), bm.rowBytes(), &fSurfaceProps);
}

void SkWindow::setMatrix(const SkMatrix& matrix) {
    if (fMatrix != matrix) {
        fMatrix = matrix;
        this->inval(NULL);
    }
}

void SkWindow::preConcat(const SkMatrix& matrix) {
    SkMatrix m;
    m.setConcat(fMatrix, matrix);
    this->setMatrix(m);
}

void SkWindow::postConcat(const SkMatrix& matrix) {
    SkMatrix m;
    m.setConcat(matrix, fMatrix);
    this->setMatrix(m);
}

void SkWindow::setColorType(SkColorType ct) {
    this->resize(fBitmap.width(), fBitmap.height(), ct);
}

void SkWindow::resize(int width, int height, SkColorType ct) {
    if (ct == kUnknown_SkColorType)
        ct = fColorType;

    if (width != fBitmap.width() || height != fBitmap.height() || ct != fColorType) {
        fColorType = ct;
        fBitmap.allocPixels(SkImageInfo::Make(width, height,
                                              ct, kPremul_SkAlphaType));

        this->setSize(SkIntToScalar(width), SkIntToScalar(height));
        this->inval(NULL);
    }
}

bool SkWindow::handleInval(const SkRect* localR) {
    SkIRect ir;

    if (localR) {
        SkRect devR;
        SkMatrix inverse;
        if (!fMatrix.invert(&inverse)) {
            return false;
        }
        fMatrix.mapRect(&devR, *localR);
        devR.round(&ir);
    } else {
        ir.set(0, 0,
               SkScalarRoundToInt(this->width()),
               SkScalarRoundToInt(this->height()));
    }
    fDirtyRgn.op(ir, SkRegion::kUnion_Op);

    this->onHandleInval(ir);
    return true;
}

void SkWindow::forceInvalAll() {
    fDirtyRgn.setRect(0, 0,
                      SkScalarCeilToInt(this->width()),
                      SkScalarCeilToInt(this->height()));
}

#if defined(SK_BUILD_FOR_WINCE) && defined(USE_GX_SCREEN)
    #include <windows.h>
    #include <gx.h>
    extern GXDisplayProperties gDisplayProps;
#endif

#ifdef SK_SIMULATE_FAILED_MALLOC
extern bool gEnableControlledThrow;
#endif

bool SkWindow::update(SkIRect* updateArea) {
    if (!fDirtyRgn.isEmpty()) {
        SkBitmap bm = this->getBitmap();

		sk_sp<SkSurface> surface(this->makeSurface());
		SkCanvas* canvas = surface->getCanvas();

        canvas->clipRegion(fDirtyRgn);
        if (updateArea)
            *updateArea = fDirtyRgn.getBounds();

        SkAutoCanvasRestore acr(canvas, true);
        canvas->concat(fMatrix);

        // empty this now, so we can correctly record any inval calls that
        // might be made during the draw call.
        fDirtyRgn.setEmpty();

#ifdef SK_SIMULATE_FAILED_MALLOC
        gEnableControlledThrow = true;
#endif
#ifdef SK_BUILD_FOR_WIN32
        //try {
            this->draw(canvas);
        //}
        //catch (...) {
        //}
#else
        this->draw(canvas);
#endif
#ifdef SK_SIMULATE_FAILED_MALLOC
        gEnableControlledThrow = false;
#endif

#if defined(SK_BUILD_FOR_WINCE) && defined(USE_GX_SCREEN)
        GXEndDraw();
#endif

        return true;
    }
    return false;
}

void SkWindow::onHandleInval(const SkIRect&) {
}

void SkWindow::draw(SkCanvas* canvas)
{
    if (fWidth && fHeight && this->isVisible())
    {
        SkRect    r;
        r.set(fLoc.fX, fLoc.fY, fLoc.fX + fWidth, fLoc.fY + fHeight);
        if (this->isClipToBounds() &&
            canvas->quickReject(r)) {
                return;
        }

        SkAutoCanvasRestore    as(canvas, true);

        if (this->isClipToBounds()) {
            canvas->clipRect(r);
        }

        canvas->translate(fLoc.fX, fLoc.fY);
        canvas->concat(fMatrix);

        int sc = canvas->save();
        this->onDraw(canvas);
        canvas->restoreToCount(sc);

        
    }
}

void SkWindow::inval(SkRect* rect) {
    SkWindow*    view = this;
    SkRect storage;

    for (;;) {
        if (!view->isVisible()) {
            return;
        }
        if (view->isClipToBounds()) {
            SkRect bounds;
            view->getLocalBounds(&bounds);
            if (rect && !bounds.intersect(*rect)) {
                return;
            }
            storage = bounds;
            rect = &storage;
        }
        if (view->handleInval(rect)) {
            return;
        }

        if (rect) {
            rect->offset(view->fLoc.fX, view->fLoc.fY);
        }
    }
}
void SkWindow::onDraw(SkCanvas* canvas) {

}

void SkWindow::onSizeChange() {}

void SkWindow::setSize(SkScalar width, SkScalar height)
{
    width = SkMaxScalar(0, width);
    height = SkMaxScalar(0, height);

    if (fWidth != width || fHeight != height)
    {
        this->inval(NULL);
        fWidth = width;
        fHeight = height;
        this->inval(NULL);
        this->onSizeChange();
    }
}
void SkWindow::getLocalBounds(SkRect* bounds) const {
    if (bounds) {
        bounds->set(0, 0, fWidth, fHeight);
    }
}

void SkWindow::setFlags(uint32_t flags)
{
    SkASSERT((flags & ~kAllFlagMasks) == 0);

    uint32_t diff = fFlags ^ flags;

    if (diff & kVisible_Mask)
        this->inval(NULL);

    fFlags = SkToU8(flags);

    if (diff & kVisible_Mask)
    {
        this->inval(NULL);
    }
}

void SkWindow::setTitle(const char title[]) {
    if (NULL == title) {
        title = "";
    }
    fTitle.set(title);
    this->onSetTitle(title);
}

