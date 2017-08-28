/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkWindow_DEFINED
#define SkWindow_DEFINED

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 )  

#include "SkBitmap.h"
#include "SkMatrix.h"
#include "SkRegion.h"
#include "SkString.h"
#include "SkSurface.h"

// end : ignore skia dll warnings
#pragma warning( pop )

#if SK_SUPPORT_GPU
struct GrGLInterface;
class GrContext;
class GrRenderTarget;
#endif

class SkCanvas;

enum SkBackEndTypes {
	kNone_BackEndType,
#if SK_SUPPORT_GPU
	kNativeGL_BackEndType,
#if SK_ANGLE
	kANGLE_BackEndType,
#endif // SK_ANGLE
#endif // SK_SUPPORT_GPU
};

class SkWindow : public SkRefCnt {
public:
    SkWindow();
    virtual ~SkWindow();

	virtual void Init() = 0;
	virtual void UnInit() = 0;

	SkImageInfo info() const { return fBitmap.info(); }
    const SkBitmap& getBitmap() const { return fBitmap; }

    void    setColorType(SkColorType);
    void    resize(int width, int height, SkColorType = kUnknown_SkColorType);

    bool    isDirty() const { return !fDirtyRgn.isEmpty(); }
    bool    update(SkIRect* updateArea);
    // does not call through to onHandleInval(), but does force the fDirtyRgn
    // to be wide open. Call before update() to ensure we redraw everything.
    void    forceInvalAll();
    // return the bounds of the dirty/inval rgn, or [0,0,0,0] if none
    const SkIRect& getDirtyBounds() const { return fDirtyRgn.getBounds(); }

    const   SkMatrix& getMatrix() const { return fMatrix; }
    void    setMatrix(const SkMatrix&);
    void    preConcat(const SkMatrix&);
    void    postConcat(const SkMatrix&);

	virtual sk_sp<SkSurface> makeSurface();

	SkSurfaceProps getSurfaceProps() const { return fSurfaceProps; }

	enum Flag_Shift {
        kVisible_Shift,
        kEnabled_Shift,
        kFocusable_Shift,
        kFlexH_Shift,
        kFlexV_Shift,
        kNoClip_Shift,

        kFlagShiftCount
    };
    enum Flag_Mask {
        kVisible_Mask   = 1 << kVisible_Shift,      //!< set if the view is visible
        kEnabled_Mask   = 1 << kEnabled_Shift,      //!< set if the view is enabled
        kFocusable_Mask = 1 << kFocusable_Shift,    //!< set if the view can receive focus
        kFlexH_Mask     = 1 << kFlexH_Shift,        //!< set if the view's width is stretchable
        kFlexV_Mask     = 1 << kFlexV_Shift,        //!< set if the view's height is stretchable
        kNoClip_Mask    = 1 << kNoClip_Shift,        //!< set if the view is not clipped to its bounds

        kAllFlagMasks   = (uint32_t)(0 - 1) >> (32 - kFlagShiftCount)
    };
	/** Return the flags associated with the view
    */
    uint32_t    getFlags() const { return fFlags; }
    /** Set the flags associated with the view
    */
    void        setFlags(uint32_t flags);

    /** Helper that returns non-zero if the kVisible_Mask bit is set in the view's flags
    */
    int         isVisible() const { return fFlags & kVisible_Mask; }
    int         isEnabled() const { return fFlags & kEnabled_Mask; }
	int         isFocusable() const { return fFlags & kFocusable_Mask; }
    int         isClipToBounds() const { return !(fFlags & kNoClip_Mask); }

	/** Return the view's width */
    SkScalar    width() const { return fWidth; }
    /** Return the view's height */
    SkScalar    height() const { return fHeight; }
	/** Set the view's width and height. These must both be >= 0. This does not affect the view's loc */
    void        setSize(SkScalar width, SkScalar height);
    void        setSize(const SkPoint& size) { this->setSize(size.fX, size.fY); }
    void        setWidth(SkScalar width) { this->setSize(width, fHeight); }
    void        setHeight(SkScalar height) { this->setSize(fWidth, height); }
	/** Return a rectangle set to [0, 0, width, height] */
    void        getLocalBounds(SkRect* bounds) const;

    /** Loc - the view's offset with respect to its parent in its view hiearchy.
        NOTE: For more complex transforms, use Local Matrix. The tranformations
        are applied in the following order:
             canvas->translate(fLoc.fX, fLoc.fY);
             canvas->concat(fMatrix);
    */
    /** Return the view's left edge */
    SkScalar    locX() const { return fLoc.fX; }
    /** Return the view's top edge */
    SkScalar    locY() const { return fLoc.fY; }
    /** Set the view's left and top edge. This does not affect the view's size */
    void        setLoc(SkScalar x, SkScalar y);
    void        setLoc(const SkPoint& loc) { this->setLoc(loc.fX, loc.fY); }
    void        setLocX(SkScalar x) { this->setLoc(x, fLoc.fY); }
    void        setLocY(SkScalar y) { this->setLoc(fLoc.fX, y); }
	/** Call this to invalidate part of all of a view, requesting that the view's
        draw method be called. The rectangle parameter specifies the part of the view
        that should be redrawn. If it is null, it specifies the entire view bounds.
    */
    void        inval(SkRect* rectOrNull);
	/** Call this to have the view draw into the specified canvas. */
    virtual void draw(SkCanvas* canvas);

	void setTitle(const char title[]);

protected:
	/** Override this to draw inside the view. Be sure to call the inherited version too */
    virtual void    onDraw(SkCanvas*);
    /** Override this to be notified when the view's size changes. Be sure to call the inherited version too */
    virtual void    onSizeChange();
    // called if part of our bitmap is invalidated
    virtual void onHandleInval(const SkIRect&);

    // overrides from SkView
    virtual bool handleInval(const SkRect*);

protected:
	virtual void onSetTitle(const char title[]) {}

private:
	SkScalar    fWidth, fHeight;
	SkPoint     fLoc;
	uint8_t     fFlags;
    SkColorType fColorType;
	SkSurfaceProps  fSurfaceProps;
    SkBitmap    fBitmap;
    SkRegion    fDirtyRgn;

    bool    fWaitingOnInval;

	SkString    fTitle;
    SkMatrix    fMatrix;

private:
	typedef SkRefCnt INHERITED;
};

////////////////////////////////////////////////////////////////////////////////

#if defined(SK_BUILD_FOR_NACL)
    #include "SkOSWindow_NaCl.h"
#elif defined(SK_BUILD_FOR_MAC)
    #include "SkOSWindow_Mac.h"
#elif defined(SK_BUILD_FOR_WIN)
    #include ".\win\SkOSWindow_Win.h"
#elif defined(SK_BUILD_FOR_ANDROID)
    #include "SkOSWindow_Android.h"
#elif defined(SK_BUILD_FOR_UNIX)
  #include "SkOSWindow_Unix.h"
#elif defined(SK_BUILD_FOR_SDL)
    #include "SkOSWindow_SDL.h"
#elif defined(SK_BUILD_FOR_IOS)
    #include "SkOSWindow_iOS.h"
#endif

#endif
