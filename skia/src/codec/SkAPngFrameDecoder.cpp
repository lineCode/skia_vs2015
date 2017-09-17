/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkAPngFrameDecoder.h"
#include "SkAPngReader.h"
#include "SkAPngCodec.h"

#include <algorithm>

 // FIXME (scroggo): We can use png_jumpbuf directly once Google3 is on 1.6
#define PNG_JMPBUF(x) png_jmpbuf((png_structp) x)

 ///////////////////////////////////////////////////////////////////////////////
 // Callback functions
 ///////////////////////////////////////////////////////////////////////////////

 // When setjmp is first called, it returns 0, meaning longjmp was not called.
constexpr int kSetJmpOkay = 0;
// An error internal to libpng.
constexpr int kPngError = 1;
// Passed to longjmp when we have decoded as many lines as we need.
constexpr int kStopDecoding = 2;

class SkAPngNormalFrameDecoder final : public SkAPngFrameDecoder {
public:
	SkAPngNormalFrameDecoder(const SkEncodedInfo& encodedInfo, const SkImageInfo& imageInfo, SkStream* pngStream, void* png_ptr, void* info_ptr, int bitDepth, SkAPngCodec* pMainCodec, int frameIndex)
		: INHERITED(encodedInfo, imageInfo, pngStream, png_ptr, info_ptr, bitDepth, pMainCodec, frameIndex)
		, fRowsWrittenToOutput(0)
		, fDst(nullptr)
		, fRowBytes(0)
		, fFirstRow(0)
		, fLastRow(0)
	{}

	static void AllRowsCallback(png_structp png_ptr, png_bytep row, png_uint_32 rowNum, int /*pass*/) {
		GetDecoder(png_ptr)->allRowsCallback(row, rowNum);
	}

	static void RowCallback(png_structp png_ptr, png_bytep row, png_uint_32 rowNum, int /*pass*/) {
		GetDecoder(png_ptr)->rowCallback(row, rowNum);
	}

private:
	int                         fRowsWrittenToOutput;
	void*                       fDst;
	size_t                      fRowBytes;

	// Variables for partial decode
	int                         fFirstRow;  // FIXME: Move to baseclass?
	int                         fLastRow;
	int                         fRowsNeeded;

	typedef SkAPngFrameDecoder INHERITED;

	static SkAPngNormalFrameDecoder* GetDecoder(png_structp png_ptr) {
		return static_cast<SkAPngNormalFrameDecoder*>(png_get_progressive_ptr(png_ptr));
	}

	Result decodeAllRows(void* dst, size_t rowBytes, int* rowsDecoded) override {
		const int height = this->getInfo().height();
		png_set_progressive_read_fn(this->png_ptr(), this, nullptr, AllRowsCallback, nullptr);
		fDst = dst;
		fRowBytes = rowBytes;

		fRowsWrittenToOutput = 0;
		fFirstRow = 0;
		fLastRow = height - 1;


		const std::vector<SkAPngFrameBlock>* blocks = m_pMainCodec->getAPngReader()->frameContext(m_frameIndex)->getIDATBlocks();
		for (std::vector<SkAPngFrameBlock>::const_iterator it = blocks->begin();
			it != blocks->end(); it++)
		{
			processfdATData(it->blockPosition, it->blockSize);
		}

		if (fRowsWrittenToOutput == height) {
			return SkCodec::kSuccess;
		}

		if (rowsDecoded) {
			*rowsDecoded = fRowsWrittenToOutput;
		}

		return SkCodec::kIncompleteInput;
	}

	void allRowsCallback(png_bytep row, int rowNum) {
		SkASSERT(rowNum == fRowsWrittenToOutput);
		fRowsWrittenToOutput++;
		this->applyXformRow(fDst, row);
		fDst = SkTAddOffset<void>(fDst, fRowBytes);
	}

	void setRange(int firstRow, int lastRow, void* dst, size_t rowBytes) override {
	}

	SkCodec::Result decode(int* rowsDecoded) override {
		return SkCodec::kIncompleteInput;
	}

	void rowCallback(png_bytep row, int rowNum) {
		if (rowNum < fFirstRow) {
			// Ignore this row.
			return;
		}

		SkASSERT(rowNum <= fLastRow);
		SkASSERT(fRowsWrittenToOutput < fRowsNeeded);

		// If there is no swizzler, all rows are needed.
		if (!this->swizzler() || this->swizzler()->rowNeeded(rowNum - fFirstRow)) {
			this->applyXformRow(fDst, row);
			fDst = SkTAddOffset<void>(fDst, fRowBytes);
			fRowsWrittenToOutput++;
		}

		if (fRowsWrittenToOutput == fRowsNeeded) {
			// Fake error to stop decoding scanlines.
			longjmp(PNG_JMPBUF(this->png_ptr()), kStopDecoding);
		}
	}
};

class SkAPngInterlacedFrameDecoder final : public SkAPngFrameDecoder {
public:
	SkAPngInterlacedFrameDecoder(const SkEncodedInfo& encodedInfo, const SkImageInfo& imageInfo, SkStream* pngStream, void* png_ptr, void* info_ptr, int bitDepth, int numberPasses, SkAPngCodec* pMainCodec, int frameIndex)
		: INHERITED(encodedInfo, imageInfo, pngStream, png_ptr, info_ptr, bitDepth, pMainCodec, frameIndex)
		, fNumberPasses(numberPasses)
		, fFirstRow(0)
		, fLastRow(0)
		, fLinesDecoded(0)
		, fInterlacedComplete(false)
		, fPng_rowbytes(0)
	{}

	static void InterlacedRowCallback(png_structp png_ptr, png_bytep row, png_uint_32 rowNum, int pass) {
		auto decoder = static_cast<SkAPngInterlacedFrameDecoder*>(png_get_progressive_ptr(png_ptr));
		decoder->interlacedRowCallback(row, rowNum, pass);
	}

private:
	const int               fNumberPasses;
	int                     fFirstRow;
	int                     fLastRow;
	void*                   fDst;
	size_t                  fRowBytes;
	int                     fLinesDecoded;
	bool                    fInterlacedComplete;
	size_t                  fPng_rowbytes;
	SkAutoTMalloc<png_byte> fInterlaceBuffer;

	typedef SkAPngFrameDecoder INHERITED;

	// FIXME: Currently sharing interlaced callback for all rows and subset. It's not
	// as expensive as the subset version of non-interlaced, but it still does extra
	// work.
	void interlacedRowCallback(png_bytep row, int rowNum, int pass) {
		if (rowNum < fFirstRow || rowNum > fLastRow || fInterlacedComplete) {
			// Ignore this row
			return;
		}

		png_bytep oldRow = fInterlaceBuffer.get() + (rowNum - fFirstRow) * fPng_rowbytes;
		png_progressive_combine_row(this->png_ptr(), oldRow, row);

		if (0 == pass) {
			// The first pass initializes all rows.
			SkASSERT(row);
			SkASSERT(fLinesDecoded == rowNum - fFirstRow);
			fLinesDecoded++;
		}
		else {
			SkASSERT(fLinesDecoded == fLastRow - fFirstRow + 1);
			if (fNumberPasses - 1 == pass && rowNum == fLastRow) {
				// Last pass, and we have read all of the rows we care about.
				fInterlacedComplete = true;
				if (fLastRow != this->getInfo().height() - 1 ||
					(this->swizzler() && this->swizzler()->sampleY() != 1)) {
					// Fake error to stop decoding scanlines. Only stop if we're not decoding the
					// whole image, in which case processing the rest of the image might be
					// expensive. When decoding the whole image, read through the IEND chunk to
					// preserve Android behavior of leaving the input stream in the right place.
					longjmp(PNG_JMPBUF(this->png_ptr()), kStopDecoding);
				}
			}
		}
	}

	SkCodec::Result decodeAllRows(void* dst, size_t rowBytes, int* rowsDecoded) override {
		const int height = this->getInfo().height();
		this->setUpInterlaceBuffer(height);
		png_set_progressive_read_fn(this->png_ptr(), this, nullptr, InterlacedRowCallback,
			nullptr);

		fFirstRow = 0;
		fLastRow = height - 1;
		fLinesDecoded = 0;

		this->processData();

		png_bytep srcRow = fInterlaceBuffer.get();
		// FIXME: When resuming, this may rewrite rows that did not change.
		for (int rowNum = 0; rowNum < fLinesDecoded; rowNum++) {
			this->applyXformRow(dst, srcRow);
			dst = SkTAddOffset<void>(dst, rowBytes);
			srcRow = SkTAddOffset<png_byte>(srcRow, fPng_rowbytes);
		}
		if (fInterlacedComplete) {
			return SkCodec::kSuccess;
		}

		if (rowsDecoded) {
			*rowsDecoded = fLinesDecoded;
		}

		return SkCodec::kIncompleteInput;
	}

	void setRange(int firstRow, int lastRow, void* dst, size_t rowBytes) override {
	}

	SkCodec::Result decode(int* rowsDecoded) override {
		return SkCodec::kIncompleteInput;
	}

	void setUpInterlaceBuffer(int height) {
		fPng_rowbytes = png_get_rowbytes(this->png_ptr(), this->info_ptr());
		fInterlaceBuffer.reset(fPng_rowbytes * height);
		fInterlacedComplete = false;
	}
};



SkAPngFrameDecoder::~SkAPngFrameDecoder()
{
}

SkAPngFrameDecoder::SkAPngFrameDecoder(const SkEncodedInfo& encodedInfo, const SkImageInfo& imageInfo, SkStream* pStream, void* png_ptr, void* info_ptr, int bitDepth, SkAPngCodec* pMainCodec, int frameIndex)
	: INHERITED(encodedInfo, imageInfo, nullptr, nullptr, png_ptr, info_ptr, pMainCodec->getBitDepth())
	, m_pMainCodecStream(pStream)
	, m_pMainCodec(pMainCodec)
	, m_frameIndex(frameIndex)
{
}

static SkCodec::Result read_frame_header(png_structp* png_ptrp, png_infop* info_ptrp, SkAPngReader* pAPngReader, int frameIndex) {
	// The image is known to be a PNG. Decode enough to know the SkImageInfo.
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png_ptr) {
		return SkCodec::kInternalError;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) {
		return SkCodec::kInternalError;
	}

	if (setjmp(PNG_JMPBUF(png_ptr))) {
		return SkCodec::kInvalidInput;
	}

	png_set_progressive_read_fn(png_ptr, nullptr, nullptr, nullptr, nullptr);

	const SkFrame* frame = pAPngReader->getFrame(frameIndex);

	png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
	// parse png header
	pAPngReader->setIHDR_w_h(frame->width(), frame->height());
	png_bytep pAPngHeadData = (png_bytep)pAPngReader->getAPngHeadData();	
	size_t lenAPngHeadData = pAPngReader->getAPngHeadDataLen();
	png_process_data(png_ptr, info_ptr, pAPngHeadData, lenAPngHeadData);

	// set every frame's png_ptr
	{
		png_uint_32 origWidth, origHeight;
		int bitDepth, encodedColorType;
		png_get_IHDR(png_ptr, info_ptr, &origWidth, &origHeight, &bitDepth,
			&encodedColorType, nullptr, nullptr, nullptr);

		// TODO: Should we support 16-bits of precision for gray images?
		if (bitDepth == 16 && (PNG_COLOR_TYPE_GRAY == encodedColorType ||
			PNG_COLOR_TYPE_GRAY_ALPHA == encodedColorType)) {
			bitDepth = 8;
			png_set_strip_16(png_ptr);
		}

		switch (encodedColorType) {
		case PNG_COLOR_TYPE_PALETTE:
			// Extract multiple pixels with bit depths of 1, 2, and 4 from a single
			// byte into separate bytes (useful for paletted and grayscale images).
			if (bitDepth < 8) {
				// TODO: Should we use SkSwizzler here?
				bitDepth = 8;
				png_set_packing(png_ptr);
			}

			break;
		case PNG_COLOR_TYPE_RGB:
			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
				// Convert to RGBA if transparency chunk exists.
				png_set_tRNS_to_alpha(png_ptr);
			}
			break;
		case PNG_COLOR_TYPE_GRAY:
			// Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel.
			if (bitDepth < 8) {
				// TODO: Should we use SkSwizzler here?
				bitDepth = 8;
				png_set_expand_gray_1_2_4_to_8(png_ptr);
			}

			if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
				png_set_tRNS_to_alpha(png_ptr);
			}
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			break;
		case PNG_COLOR_TYPE_RGBA:
			break;
		default:
			// All the color types have been covered above.
			SkASSERT(false);
		}
	}

	*png_ptrp = png_ptr;
	*info_ptrp = info_ptr;

	return SkCodec::kSuccess;
}

SkCodec::Result SkAPngFrameDecoder::decodeFrame(const SkImageInfo& dstInfo, void* dst, const Options& options, size_t rowBytes, int* rowsDecoded)
{
	const SkFrame * frame = m_pMainCodec->getAPngReader()->getFrame(options.fFrameIndex);
	SkImageInfo frameDstInfo = this->getInfo();

	Result result = this->initializeXforms(frameDstInfo, options);
	if (kSuccess != result) {
		return result;
	}

	this->allocateStorage(frameDstInfo);
	this->initializeXformParams();
	//int frameRowBytes2 = frameDstInfo.minRowBytes();
	int frameRowBytes = png_get_rowbytes(fPng_ptr, fInfo_ptr);

	png_bytep frameDst = (png_bytep)malloc(frame->height() * frameRowBytes);

	result = this->decodeAllRows(frameDst, frameRowBytes, rowsDecoded);

	memset(dst, 0, dstInfo.height() * rowBytes);
	int xStart = frame->frameRect().fLeft;
	int yStart = frame->frameRect().fTop;
	int xEnd = frame->frameRect().fRight;
	int yEnd = frame->frameRect().fBottom;

	for (int y = yStart; y < yEnd; y++)
	{
		memcpy((png_bytep)dst + (xStart * dstInfo.bytesPerPixel() + y * rowBytes), frameDst + (y - yStart) * frameRowBytes, frameRowBytes);
	}

	free(frameDst);

	return result;
}

std::unique_ptr<SkAPngFrameDecoder> SkAPngFrameDecoder::MakeFrameDecoder(SkStream* stream, Result* result, SkAPngCodec* pMainCodec, int frameIndex)
{
	SkAPngFrameDecoder* outCodec = nullptr;

	if (pMainCodec)
	{
		png_structp png_ptr;
		png_infop info_ptr;

		if (kSuccess != read_frame_header(&png_ptr, &info_ptr, pMainCodec->getAPngReader(), frameIndex)) {
			*result = kErrorInInput;
		}

		if (pMainCodec->getAPngReader())
		{
			int decodeType = (int)pMainCodec->getAPngReader()->getFrameDecoderType();
			if (E_Normal_Decoder == decodeType)
			{
				SkStream * frameStream = pMainCodec->makeFrameStream(frameIndex);
				SkEncodedInfo encodedInfo = pMainCodec->getEncodedInfo();
				SkImageInfo imageInfo = pMainCodec->getInfo();
				const SkFrame* frame = pMainCodec->getAPngReader()->getFrame(frameIndex);
				int frameW = frame->width();
				int frameH = frame->height();
				SkImageInfo frameImageInfo = SkImageInfo::Make(frameW, frameH, imageInfo.colorType(), imageInfo.alphaType(), imageInfo.refColorSpace());
				int bitDepth = pMainCodec->getBitDepth();
				outCodec = new SkAPngNormalFrameDecoder(encodedInfo, frameImageInfo, frameStream, png_ptr, info_ptr, bitDepth, pMainCodec, frameIndex);
			}
			else if (E_Interlaced_Decoder == decodeType)
			{
				SkStream * frameStream = pMainCodec->makeFrameStream(frameIndex);
				SkEncodedInfo encodedInfo = pMainCodec->getEncodedInfo();
				SkImageInfo imageInfo = pMainCodec->getInfo();
				const SkFrame* frame = pMainCodec->getAPngReader()->getFrame(frameIndex);
				int frameW = frame->width();
				int frameH = frame->height();
				int numPasses = pMainCodec->getAPngReader()->getNumberPasses();
				SkImageInfo frameImageInfo = SkImageInfo::Make(frameW, frameH, imageInfo.colorType(), imageInfo.alphaType(), imageInfo.refColorSpace());
				int bitDepth = pMainCodec->getBitDepth();
				outCodec = new SkAPngInterlacedFrameDecoder(encodedInfo, frameImageInfo, frameStream, png_ptr, info_ptr, bitDepth, numPasses, pMainCodec, frameIndex);
			}
		}
	}

	return std::unique_ptr<SkAPngFrameDecoder>(outCodec);
}

void SkAPngFrameDecoder::processfdATData(size_t fdATPos, size_t fdATLength) {
	m_pMainCodecStream->seek(fdATPos);
	switch (setjmp(PNG_JMPBUF(fPng_ptr))) {
	case kPngError:
		// There was an error. Stop processing data.
		// FIXME: Do we need to discard png_ptr?
		return;
	case kStopDecoding:
		// We decoded all the lines we want.
		return;
	case kSetJmpOkay:
		// Everything is okay.
		break;
	default:
		// No other values should be passed to longjmp.
		SkASSERT(false);
	}

	// Arbitrary buffer size
	constexpr size_t kBufferSize = 4096;
	char buffer[kBufferSize];

	size_t fIdatLength = fdATLength;

	bool iend = false;

	// process fake IDAT header
	png_byte idat[] = { 0, 0, 0, 0, 'I', 'D', 'A', 'T' };
	png_save_uint_32(idat, fIdatLength);
	png_process_data(fPng_ptr, fInfo_ptr, idat, 8);

	// process fdAT data
	while (fIdatLength > 0) {
		const size_t bytesToProcess = std::min(kBufferSize, fIdatLength);
		const size_t bytesRead = m_pMainCodecStream->read(buffer, bytesToProcess);
		png_process_data(fPng_ptr, fInfo_ptr, (png_bytep)buffer, bytesRead);
		if (bytesRead < bytesToProcess) {
			return;
		}
		fIdatLength -= bytesToProcess;
	}
}
