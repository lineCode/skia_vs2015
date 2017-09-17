#ifndef __ANIMATEDGIF_H__
#define __ANIMATEDGIF_H__

#include <memory>
#include <SkCodec.h>
#include <SkBitmap.h>
#include <SkCanvas.h>

class AnimatedGif
{
private:
	double  fBaseTimeNanos;
	double  fCurrTimeNanos;

	std::unique_ptr<SkCodec>        fCodec;
	int                             fFrame;
	double                          fNextUpdate;
	int                             fTotalFrames;
	std::vector<SkCodec::FrameInfo> fFrameInfos;
	std::vector<SkBitmap>           fFrames;

	char fName[256];

	void drawFrame(SkCanvas* canvas, int frameIndex);

	static bool copy_to(SkBitmap* dst, SkColorType dstColorType, const SkBitmap& src);
	bool initCodec();
	bool onAnimate(SkMSec msec);

public:
	AnimatedGif(char *gifName);
	
	bool onDraw(SkCanvas* canvas);	
};

#endif//__ANIMATEDGIF_H__
