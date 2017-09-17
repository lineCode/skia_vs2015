#include "AnimatedGif.h"
#include <Windows.h>
#include <shlwapi.h>

#pragma comment (lib, "Shlwapi.lib")




void AnimatedGif::drawFrame(SkCanvas* canvas, int frameIndex)
{
	SkBitmap& bm = fFrames[frameIndex];
	canvas->drawBitmap(bm, 0, 0);
}
AnimatedGif::AnimatedGif(char *gifName)
	: fFrame(0)
	, fNextUpdate(-1)
	, fTotalFrames(-1)
	, fBaseTimeNanos(0)
{
	strcpy(fName, gifName);
	initCodec();
}
bool AnimatedGif::copy_to(SkBitmap* dst, SkColorType dstColorType, const SkBitmap& src)
{
	SkPixmap srcPM;
	if (!src.peekPixels(&srcPM)) {
		return false;
	}

	SkBitmap tmpDst;
	SkImageInfo dstInfo = srcPM.info().makeColorType(dstColorType);
	if (!tmpDst.setInfo(dstInfo)) {
		return false;
	}

	if (!tmpDst.tryAllocPixels()) {
		return false;
	}

	SkPixmap dstPM;
	if (!tmpDst.peekPixels(&dstPM)) {
		return false;
	}

	if (!srcPM.readPixels(dstPM)) {
		return false;
	}

	dst->swap(tmpDst);
	return true;
}
bool AnimatedGif::initCodec() {
	if (fCodec) {
		return true;
	}

	char buf[256] = { 0 };
	GetModuleFileNameA(NULL, buf, 256);
	PathRemoveFileSpecA(buf);
	strcat(buf, "\\");
	strcat(buf, fName);
	
	sk_sp<SkData> data = SkData::MakeFromFileName(buf);
	fCodec = SkCodec::MakeFromData(data);
	if (!fCodec) {
		return false;
	}
	fFrame = 0;
	fFrameInfos = fCodec->getFrameInfo();
	fTotalFrames = fCodec->getFrameCount();

	//fTotalFrames = 2;

	// decode all bitmaps from gif
	int frameIndex = 0;
	fFrames.resize(fTotalFrames);
	if (fTotalFrames == 1)
	{
		SkBitmap& bm = fFrames[0];

		SkImageInfo info = fCodec->getInfo().makeColorType(kN32_SkColorType);
		if (kPremul_SkAlphaType == info.alphaType()) {
			info = info.makeAlphaType(kUnpremul_SkAlphaType);
		}

		if (!bm.tryAllocPixels(info)) {
		}

		if (SkCodec::kSuccess == fCodec->getPixels(info, bm.getPixels(), bm.rowBytes()))
		{
		
		}

		{
			SkBitmap bm2;

			SkImageInfo info = fCodec->getInfo().makeColorType(kN32_SkColorType);
			if (kPremul_SkAlphaType == info.alphaType()) {
				info = info.makeAlphaType(kUnpremul_SkAlphaType);
			}

			bm2.allocPixels(info);

			if (SkCodec::kSuccess == fCodec->getPixels(info, bm2.getPixels(), bm2.rowBytes()))
			{

			}
		}
	}
	else if (fTotalFrames > 1)
	{

		for (frameIndex = 0; frameIndex < fTotalFrames; frameIndex++)
		{
			SkBitmap& bm = fFrames[frameIndex];
			if (!bm.getPixels()) {
				const SkImageInfo info = fCodec->getInfo().makeColorType(kN32_SkColorType);
				bm.allocPixels(info);

				SkCodec::Options opts;
				opts.fFrameIndex = frameIndex;
				const int requiredFrame = fFrameInfos[frameIndex].fRequiredFrame;
				if (requiredFrame != SkCodec::kNone) {
					SkASSERT(requiredFrame >= 0
						&& static_cast<size_t>(requiredFrame) < fFrames.size());
					SkBitmap& requiredBitmap = fFrames[requiredFrame];
					// For simplicity, do not try to cache old frames
					if (requiredBitmap.getPixels() && copy_to(&bm, requiredBitmap.colorType(), requiredBitmap)) {
						opts.fPriorFrame = requiredFrame;
					}
				}

				if (SkCodec::kSuccess != fCodec->getPixels(info, bm.getPixels(),
					bm.rowBytes(), &opts)) {
					OutputDebugStringA("fCodec->getPixels failed\n");
				}

				//SkCanvas tmpCavans(bm);
				//char bufText[16] = { 0 };
				//sprintf(bufText, "%d", frameIndex);
				//SkPaint paint;
				//paint.setTextSize(30);
				//tmpCavans.drawText(bufText, strlen(bufText), 0, 30, paint);
				
			}
		}
	}

	return true;
}
bool AnimatedGif::onDraw(SkCanvas* canvas) {
	if (!fCodec) {
		return false;
	}
	fCurrTimeNanos = GetTickCount();
	const double sec = (fCurrTimeNanos - fBaseTimeNanos);
	SkMSec msec = static_cast<SkMSec>(sec);
	this->onAnimate(msec);
	this->drawFrame(canvas, fFrame);

	if (fTotalFrames > 1)
		return true;
	else
		return false;
}
bool AnimatedGif::onAnimate(SkMSec msec)
{
	if (!fCodec || fTotalFrames == 1) {
		return false;
	}

	double secs = msec;
	if (fNextUpdate < double(0)) {
		// This is a sentinel that we have not done any updates yet.
		// I'm assuming this gets called *after* onOnceBeforeDraw, so our first frame should
		// already have been retrieved.
		SkASSERT(fFrame == 0);
		fNextUpdate = secs + fFrameInfos[fFrame].fDuration;

		return true;
	}

	if (secs < fNextUpdate) {
		return true;
	}

	while (secs >= fNextUpdate) {
		// Retrieve the next frame.
		fFrame++;
		if (fFrame == fTotalFrames) {
			fFrame = 0;
		}

		// Note that we loop here. This is not safe if we need to draw the intermediate frame
		// in order to draw correctly.
		fNextUpdate += fFrameInfos[fFrame].fDuration;
	}

	return true;
}