#include <Windows.h>
#include "SkiaTest.h"

// start : ignore skia dll warnings
#pragma warning( push )  
#pragma warning( disable : 4251 )  	

#include "SkCanvas.h"
#include "SkString.h"
#include "SkData.h"
#include "SkGradientShader.h"
#include "SkBlurMaskFilter.h"
#include "SkColorFilter.h"
#include "SkTableColorFilter.h"
#include "Sk2DPathEffect.h"
#include "Sk1DPathEffect.h"
#include "SkCornerPathEffect.h"
#include "SkDashPathEffect.h"
#include "SkDiscretePathEffect.h"
#include "SkTime.h"

#include "SkBitmap.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"

static inline bool decode_file(const char* filename, SkBitmap* bitmap,
	SkColorType colorType = kN32_SkColorType,
	bool requireUnpremul = false) {
	sk_sp<SkData> data(SkData::MakeFromFileName(filename));
	std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(data);
	if (!codec) {
		return false;
	}

	SkImageInfo info = codec->getInfo().makeColorType(colorType);
	if (requireUnpremul && kPremul_SkAlphaType == info.alphaType()) {
		info = info.makeAlphaType(kUnpremul_SkAlphaType);
	}

	if (!bitmap->tryAllocPixels(info)) {
		return false;
	}

	return SkCodec::kSuccess == codec->getPixels(info, bitmap->getPixels(), bitmap->rowBytes());
}

static inline sk_sp<SkImage> decode_file(const char filename[]) {
	sk_sp<SkData> data(SkData::MakeFromFileName(filename));
	return data ? SkImage::MakeFromEncoded(data) : nullptr;
}

// end : ignore skia dll warnings
#pragma warning( pop )

#include "FrameWindowWnd.h"

typedef bool (*DrawFuncPtr)(SkCanvas * canvas);

CSkiaTest* CSkiaTest::getInstance()
{
	static CSkiaTest s_inst;
	return &s_inst;
}
CSkiaTest::CSkiaTest(void)
	: m_iCurrentFuncIndex(0)
{
}


CSkiaTest::~CSkiaTest(void)
{
}

bool TestSkCanvasPath(SkCanvas * canvas)
{
	const SkScalar scale = 256.0f;
    const SkScalar R = 0.45f * scale;
    const SkScalar TAU = 6.2831853f;
    SkPath path;
    path.moveTo(R, 0.0f);
    for (int i = 1; i < 7; ++i) {
        SkScalar theta = 3 * i * TAU / 7;
        path.lineTo(R * cos(theta), R * sin(theta));

    }
    path.close();
    SkPaint p;
    p.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    canvas->translate(0.5f * scale, 0.5f * scale);
    canvas->drawPath(path, p);
	return false;
}

bool TestSkCanvasRotate(SkCanvas * canvas)
{
	canvas->save();
    canvas->translate(SkIntToScalar(128), SkIntToScalar(128));
    canvas->rotate(SkIntToScalar(45));
    SkRect rect = SkRect::MakeXYWH(-90.5f, -90.5f, 181.0f, 181.0f);
    SkPaint paint;
	paint.setColor(SK_ColorBLUE);
    canvas->drawRect(rect, paint);
    canvas->restore();
	return false;
}

bool TestSkCanvasMisc(SkCanvas* canvas) {
    canvas->drawColor(SK_ColorWHITE);

	canvas->save();
	canvas->translate(10, 10);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(4);
    paint.setColor(SK_ColorRED);

    SkRect rect = SkRect::MakeXYWH(50, 50, 40, 60);
    canvas->drawRect(rect, paint);

    SkRRect oval;
    oval.setOval(rect);
    oval.offset(40, 60);
    paint.setColor(SK_ColorBLUE);
    canvas->drawRRect(oval, paint);

    paint.setColor(SK_ColorCYAN);
    canvas->drawCircle(180, 50, 25, paint);

    rect.offset(80, 0);
    paint.setColor(SK_ColorYELLOW);
    canvas->drawRoundRect(rect, 10, 10, paint);

    SkPath path;
    path.cubicTo(768, 0, -512, 256, 256, 256);
    paint.setColor(SK_ColorGREEN);
    canvas->drawPath(path, paint);


	SkBitmap image;
	decode_file("mandrill_128.png", &image);
	canvas->drawBitmap(image, 128, 128, &paint);

    SkRect rect2 = SkRect::MakeXYWH(0, 0, 40, 60);
    canvas->drawBitmapRect(image, rect2, &paint);

    SkPaint paint2;
    const char text[] = "Hello, Skia!";
    canvas->drawText(text, strlen(text), 50, 25, paint2);

	canvas->restore();
	return false;
}

bool TestDrawText(SkCanvas* canvas) {
    canvas->clear(SK_ColorWHITE);

    SkPaint paint1, paint2, paint3;

    paint1.setTextSize(64.0f);
    paint1.setAntiAlias(true);
    paint1.setColor(SkColorSetRGB(255, 0, 0));
    paint1.setStyle(SkPaint::kFill_Style);

    paint2.setTextSize(64.f);
    paint2.setAntiAlias(true);
    paint2.setColor(SkColorSetRGB(0, 136, 0));
    paint2.setStyle(SkPaint::kStroke_Style);
    paint2.setStrokeWidth(SkIntToScalar(3));

    paint3.setTextSize(64.0f);
    paint3.setAntiAlias(true);
    paint3.setColor(SkColorSetRGB(136, 136, 136));
    paint3.setTextScaleX(SkFloatToScalar(1.5f));

    const char text[] = "Skia!";
    canvas->drawText(text, strlen(text), 20.0f, 64.0f,  paint1);
    canvas->drawText(text, strlen(text), 20.0f, 144.0f, paint2);
    canvas->drawText(text, strlen(text), 20.0f, 224.0f, paint3);
	return false;
}

bool TestDrawStrokeOrFill(SkCanvas* canvas) {
    SkPaint paint1, paint2, paint3;
    paint2.setStyle(SkPaint::kStroke_Style);
    paint2.setStrokeWidth(3);
	paint3.setAntiAlias(true);
    paint3.setColor(SK_ColorRED);
	paint3.setTextSize(80);
    
    canvas->drawRect(SkRect::MakeXYWH(10,10,60,20), paint1);
	canvas->drawRect(SkRect::MakeXYWH(80,10,60,20), paint2);

	paint2.setStrokeWidth(SkIntToScalar(5));
	canvas->drawOval(SkRect::MakeXYWH(150,10,60,20), paint2);

	canvas->drawText("SKIA", 4, 20, 120, paint3);
	paint3.setColor(SK_ColorBLUE);
	canvas->drawText("SKIA", 4, 20, 220, paint3);
	return false;
}

bool TestDrawLinearGradientShader(SkCanvas* canvas) {
    SkPoint points[2] = {
        SkPoint::Make(0.0f, 0.0f),
        SkPoint::Make(256.0f, 256.0f)
    };
    SkColor colors[2] = {SK_ColorBLUE, SK_ColorYELLOW};
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(
                     points, colors, nullptr, 2,
                     SkShader::kClamp_TileMode, 0, nullptr));

	canvas->save();
	//SkRect target;
	//target.MakeXYWH(0, 0, 256, 256);	
	//// why clip no effect
	//canvas->drawRect(target, paint);
	//canvas->clipRect(target, SkRegion::kIntersect_Op, true);
    canvas->drawPaint(paint);
	canvas->restore();
	return false;
}

bool TextDrawBlendMode(SkCanvas* canvas) {
	SkBlendMode modes[] = {
		SkBlendMode::kClear,
		SkBlendMode::kSrc,
		SkBlendMode::kDst,
		SkBlendMode::kSrcOver,
		SkBlendMode::kDstOver,
		SkBlendMode::kSrcIn,
		SkBlendMode::kDstIn,
		SkBlendMode::kSrcOut,
		SkBlendMode::kDstOut,
		SkBlendMode::kSrcATop,
		SkBlendMode::kDstATop,
		SkBlendMode::kXor,
		SkBlendMode::kPlus,
		SkBlendMode::kModulate,
		SkBlendMode::kScreen,
		SkBlendMode::kOverlay,
		SkBlendMode::kDarken,
		SkBlendMode::kLighten,
		SkBlendMode::kColorDodge,
		SkBlendMode::kColorBurn,
		SkBlendMode::kHardLight,
		SkBlendMode::kSoftLight,
		SkBlendMode::kDifference,
		SkBlendMode::kExclusion,
		SkBlendMode::kMultiply,
		SkBlendMode::kHue,
		SkBlendMode::kSaturation,
		SkBlendMode::kColor,
		SkBlendMode::kLuminosity,
	};
	SkRect rect = SkRect::MakeWH(64.0f, 64.0f);
	SkPaint text, stroke, src, dst;
	stroke.setStyle(SkPaint::kStroke_Style);
	text.setTextSize(24.0f);
	text.setAntiAlias(true);
	SkPoint srcPoints[2] = {
		SkPoint::Make(0.0f, 0.0f),
		SkPoint::Make(64.0f, 0.0f)
	};
	SkColor srcColors[2] = {
		SK_ColorMAGENTA & 0x00FFFFFF,
		SK_ColorMAGENTA };
	src.setShader(SkGradientShader::MakeLinear(
		srcPoints, srcColors, nullptr, 2,
		SkShader::kClamp_TileMode, 0, nullptr));

	SkPoint dstPoints[2] = {
		SkPoint::Make(0.0f, 0.0f),
		SkPoint::Make(0.0f, 64.0f)
	};
	SkColor dstColors[2] = {
		SK_ColorCYAN & 0x00FFFFFF,
		SK_ColorCYAN };
	dst.setShader(SkGradientShader::MakeLinear(
		dstPoints, dstColors, nullptr, 2,
		SkShader::kClamp_TileMode, 0, nullptr));
	canvas->clear(SK_ColorWHITE);
	size_t N = sizeof(modes) / sizeof(modes[0]);
	size_t K = (N - 1) / 3 + 1;
	SkASSERT(K * 64 == 640);  // tall enough
	for (size_t i = 0; i < N; ++i) {
		SkAutoCanvasRestore autoCanvasRestore(canvas, true);
		canvas->translate(192.0f * (i / K), 64.0f * (i % K));
		const char* desc = SkBlendMode_Name(modes[i]);
		canvas->drawText(desc, strlen(desc), 68.0f, 30.0f, text);
		canvas->clipRect(SkRect::MakeWH(64.0f, 64.0f));
		canvas->drawColor(SK_ColorLTGRAY);
		(void)canvas->saveLayer(nullptr, nullptr);
		canvas->clear(SK_ColorTRANSPARENT);
		canvas->drawPaint(dst);
		src.setBlendMode(modes[i]);
		canvas->drawPaint(src);
		canvas->drawRect(rect, stroke);
	}
	return false;
}

bool TestDrawMaskFilter(SkCanvas* canvas) {
    canvas->drawColor(SK_ColorWHITE);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextSize(120);
    paint.setMaskFilter(SkBlurMaskFilter::Make(
            kNormal_SkBlurStyle, 5.0f, 0));
    const char text[] = "Skia";
    canvas->drawText(text, strlen(text), 0, 160, paint);
	return false;
}

bool TestDrawTableColorFilter(SkCanvas* canvas) { 
    canvas->scale(0.5, 0.5);
    uint8_t ct[256]; 
    for (int i = 0; i < 256; ++i) {
        int x = (i - 96) * 255 / 64;
        ct[i] = x < 0 ? 0 : x > 255 ? 255 : x;
    }
	SkBitmap image;
    decode_file("mandrill_128.png", &image);
    SkPaint paint;
	paint.setColorFilter(SkTableColorFilter::MakeARGB(nullptr, ct, ct, ct));
    canvas->drawBitmap(image, 10, 10, &paint);
	return false;
}

bool TestDraw2DPathEffect(SkCanvas* canvas) {
    SkPaint paint;
    SkMatrix lattice;
    lattice.setScale(8.0f, 8.0f);
    lattice.preRotate(30.0f);
    paint.setPathEffect(SkLine2DPathEffect::Make(0.0f, lattice));
    paint.setAntiAlias(true);
    SkRect bounds = SkRect::MakeWH(256, 256);
    bounds.outset(8.0f, 8.0f);
    canvas->clear(SK_ColorWHITE);
    canvas->drawRect(bounds, paint);
	return false;
}

bool TestDraw1DEffect(SkCanvas* canvas) {
    SkPaint paint;
    SkPath path;
    path.addOval(SkRect::MakeWH(16.0f, 6.0f));
    paint.setPathEffect(SkPath1DPathEffect::Make(
            path, 32.0f, 0.0f, SkPath1DPathEffect::kRotate_Style));
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    canvas->drawCircle(128.0f, 128.0f, 122.0f, paint);
	return false;
}

SkPath star() {
    const SkScalar R = 115.2f, C = 128.0f;
    SkPath path;
    path.moveTo(C + R, C);
    for (int i = 1; i < 7; ++i) {
        SkScalar a = 2.6927937f * i;
        path.lineTo(C + R * cos(a), C + R * sin(a));
    }
    path.close();
    return path;
}
bool TestDrawCornerPathEffect(SkCanvas* canvas) {
    SkPaint paint;
    paint.setPathEffect(SkCornerPathEffect::Make(32.0f));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);;
    SkPath path(star());
    canvas->drawPath(path, paint);
	return false;
}

bool TestDrawDashPathEffect(SkCanvas* canvas) {
    const SkScalar intervals[] = { 10.0f, 5.0f, 2.0f, 5.0f };
    size_t count  = sizeof(intervals) / sizeof(intervals[0]);
    SkPaint paint;
    paint.setPathEffect(SkDashPathEffect::Make(intervals, count, 0.0f));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    SkPath path(star());
    canvas->drawPath(path, paint);
	return false;
}

bool TestDrawDiscretePathEffect(SkCanvas* canvas) {
    SkPaint paint;
    paint.setPathEffect(SkDiscretePathEffect::Make(10.0f, 4.0f));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    SkPath path(star());
    canvas->drawPath(path, paint);
	return false;
}

bool TestDrawComposePathEffect(SkCanvas* canvas) {
    const SkScalar intervals[] = { 10.0f, 5.0f, 2.0f, 5.0f };
    size_t count  = sizeof(intervals) / sizeof(intervals[0]);
    SkPaint paint;
	paint.setPathEffect(SkDashPathEffect::Make(intervals, count, 0.0f));// SkDiscretePathEffect::Make(10.0f, 4.0f)
    
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    SkPath path(star());
    canvas->drawPath(path, paint);
	return false;
}
bool TestDrawSumPathEffect(SkCanvas* canvas) {
#if 0
    SkPaint paint;
    paint.setPathEffect(SkSumPathEffect::Create(
        SkDiscretePathEffect::Create(10.0f, 4.0f),
        SkDiscretePathEffect::Create(10.0f, 4.0f, 1245u)
    ));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    paint.setAntiAlias(true);
    canvas->clear(SK_ColorWHITE);
    SkPath path(star());
    canvas->drawPath(path, paint);
#endif
    
    SkBitmap bitmap;
    SkImageInfo imageInfo = SkImageInfo::MakeN32(100, 100, kPremul_SkAlphaType);

    bitmap.allocPixels(imageInfo, imageInfo.bytesPerPixel() * imageInfo.width());
    
    SkCanvas* localCanvas = new SkCanvas(bitmap);
    SkRect localRect = SkRect::MakeXYWH(0, 0, 100, 50);
    SkPaint localPaint;
    localPaint.setColor(SK_ColorBLUE);
    localCanvas->drawRect(localRect, localPaint);
   
    canvas->save();
	canvas->clear(SK_ColorWHITE);
    SkMatrix skMatrix;
	skMatrix.setIdentity();
	SkRect rect = SkRect::MakeXYWH(0.f, 0.f, 100.f, 50.f);
	skMatrix.setRotate(SkIntToScalar(14), rect.centerX(), rect.centerY());
	canvas->setMatrix(skMatrix);
	SkPaint paint;
	paint.setFlags(SkPaint::kAntiAlias_Flag);
	canvas->drawBitmap(bitmap, 100, 100, &paint);
	canvas->restore();

	return false;
}
//class AnimGif {
//	SkMovie*    fMovie;
//public:
//	AnimGif() {
//		fMovie = SkMovie::DecodeFile("welcome2.gif");
//	}
//
//	virtual ~AnimGif() {
//		SkSafeUnref(fMovie);
//	}
//
//public:
//
//	void drawBG(SkCanvas* canvas) {
//		canvas->drawColor(0xFFDDDDDD);
//	}
//
//	void onDraw(SkCanvas* canvas) {
//		this->drawBG(canvas);
//
//		if (fMovie) {
//			if (fMovie->duration()) {
//				fMovie->setTime(SkTime::GetMSecs() % fMovie->duration());
//			}
//			else {
//				fMovie->setTime(0);
//			}
//
//			if (false)
//			{
//				SkBitmap bmp;
//				bmp.allocN32Pixels(fMovie->bitmap().width(), fMovie->bitmap().height());
//				fMovie->bitmap().copyTo(&bmp);
//
//				SkCanvas* tmpcanvas = new SkCanvas(bmp);
//				SkPaint paint;
//				tmpcanvas->drawText("i", 1, 0, 40, paint);
//
//				canvas->drawBitmap(bmp, SkIntToScalar(20),
//					SkIntToScalar(20));
//				SkSafeRef(tmpcanvas);
//			}
//			else {
//				fMovie->bitmap().notifyPixelsChanged();
//				canvas->drawBitmap(fMovie->bitmap(), SkIntToScalar(20),
//					SkIntToScalar(20));
//			}
//
//			{
//				static int index = 0;
//
//				if (false)
//				{
//					SkPaint paint;
//					SkBitmap bmp;
//					bmp.allocN32Pixels(fMovie->bitmap().width(), fMovie->bitmap().height());
//					fMovie->bitmap().copyTo(&bmp);
//					SkCanvas* tmpcanvas = new SkCanvas(bmp);
//					SkString pngName;
//					//pngName.appendf("tmp%d.png", index);
//					pngName.append("i", 1);
//					//tmpcanvas->drawText(pngName.c_str(), pngName.size(), 0, 40, paint);
//
//					canvas->drawBitmap(bmp, SkIntToScalar(20), SkIntToScalar(20));
//					SkBitmap bm;
//					bm.allocPixels(canvas->imageInfo());
//					canvas->readPixels(&bm, 0, 0);
//
//
//					//SkImageEncoder::EncodeFile(pngName.c_str(), bm,SkImageEncoder::kPNG_Type, 0);
//
//					SkString pngNameGif;
//					pngNameGif.appendf("tmpGif%d.png", index++);
//					//SkImageEncoder::EncodeFile(pngNameGif.c_str(), fMovie->bitmap(),SkImageEncoder::kPNG_Type, 0);
//				}
//
//			}
//		}
//	}
//
//};

bool TestDrawGif(SkCanvas* canvas) { 
	//static AnimGif anim;
	//
	//anim.onDraw(canvas);

	//return true;
	return false;
}

DrawFuncPtr ms_arrFuncs[] = 
{
	TestDrawGif,
	TestDrawSumPathEffect,
	TestDrawComposePathEffect,
	TestDrawDiscretePathEffect,
	TestDrawDashPathEffect,
	TestDrawCornerPathEffect,
	TestDraw1DEffect,
	TestDraw2DPathEffect,
	TestSkCanvasMisc,
	TestDrawMaskFilter,
	TextDrawBlendMode,
	TestSkCanvasPath,
	TestSkCanvasRotate,	
	TestDrawText,
	TestDrawStrokeOrFill,
	TestDrawLinearGradientShader,
	TestDrawTableColorFilter,	
};
int s_iCountOfFuncs = sizeof(ms_arrFuncs) / sizeof(ms_arrFuncs[0]);



int CSkiaTest::getCountOfFuncs()
{
	return s_iCountOfFuncs;
}
void CSkiaTest::next()
{
	m_iCurrentFuncIndex = (m_iCurrentFuncIndex + 1) % s_iCountOfFuncs;
}
void CSkiaTest::prev()
{
	m_iCurrentFuncIndex -= 1;
	if (m_iCurrentFuncIndex < 0)
		m_iCurrentFuncIndex = s_iCountOfFuncs - 1;
}
void CSkiaTest::Draw(SkCanvas *canvas)
{
	DrawFuncPtr fun = ms_arrFuncs[m_iCurrentFuncIndex];
	bool needRedraw = fun(canvas);

	if (needRedraw)
		frame->SetReDrawTimer(20);
	else
		frame->KillReDrawTimer();
}