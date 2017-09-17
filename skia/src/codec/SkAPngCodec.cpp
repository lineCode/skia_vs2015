/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkCodecPriv.h"
#include "SkColorPriv.h"
#include "SkColorSpace.h"
#include "SkColorSpacePriv.h"
#include "SkColorTable.h"
#include "SkMath.h"
#include "SkOpts.h"
#include "SkAPngCodec.h"
#include "SkPoint3.h"
#include "SkSize.h"
#include "SkStream.h"
#include "SkSwizzler.h"
#include "SkTemplates.h"
#include "SkUtils.h"

#include "png.h"
#include <algorithm>

#include "SkAPngReader.h"
#include "SkAPngFrameDecoder.h"

// This warning triggers false postives way too often in here.
#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic ignored "-Wclobbered"
#endif

// FIXME (scroggo): We can use png_jumpbuf directly once Google3 is on 1.6
#define PNG_JMPBUF(x) png_jmpbuf((png_structp) x)

///////////////////////////////////////////////////////////////////////////////
// Callback functions
///////////////////////////////////////////////////////////////////////////////

// When setjmp is first called, it returns 0, meaning longjmp was not called.
constexpr int kSetJmpOkay   = 0;
// An error internal to libpng.
constexpr int kPngError     = 1;
// Passed to longjmp when we have decoded as many lines as we need.
constexpr int kStopDecoding = 2;

static void sk_error_fn(png_structp png_ptr, png_const_charp msg) {
    SkCodecPrintf("------ png error %s\n", msg);
    longjmp(PNG_JMPBUF(png_ptr), kPngError);
}

void sk_warning_fn(png_structp, png_const_charp msg);

#ifdef PNG_READ_UNKNOWN_CHUNKS_SUPPORTED
static int sk_read_user_chunk(png_structp png_ptr, png_unknown_chunkp chunk) {
    SkPngChunkReader* chunkReader = (SkPngChunkReader*)png_get_user_chunk_ptr(png_ptr);
    // readChunk() returning true means continue decoding
	// filter apng tags
	const char * tag = (const char*)chunk->name;
	if (!memcmp(tag, "acTL", 4) || !memcmp(tag, "acTL", 4) || !memcmp(tag, "acTL", 4))
	{
		return 1;
	}
    return chunkReader->readChunk((const char*)chunk->name, chunk->data, chunk->size) ? 1 : -1;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Helpers : read frame infos
///////////////////////////////////////////////////////////////////////////////
class AutoCleanAPngFrames : public SkNoncopyable {
public:

	AutoCleanAPngFrames(png_structp png_ptr, png_infop info_ptr, SkStream* stream, SkAPngReader* pAPngReader)
		: fPng_ptr(png_ptr)
		, fInfo_ptr(info_ptr)
		, fStream(stream)
		, fAPngReader(pAPngReader)
	{

	}

	~AutoCleanAPngFrames() {
		if (fPng_ptr) {
			png_infopp info_pp = fInfo_ptr ? &fInfo_ptr : nullptr;
			png_destroy_read_struct(&fPng_ptr, info_pp, nullptr);
		}
		fStream->rewind();
		fAPngReader = nullptr;
	}

	void setInfoPtr(png_infop info_ptr) {
		SkASSERT(nullptr == fInfo_ptr);
		fInfo_ptr = info_ptr;
	}

	static bool decodeFrameInfos(SkStream* stream, SkAPngReader * apngReader);

private:
	png_structp         fPng_ptr;
	png_infop           fInfo_ptr;
	SkStream*           fStream;
	SkAPngReader*       fAPngReader;
};

///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////

class AutoCleanAPng final : public SkNoncopyable {
public:
    /*
     *  This class does not take ownership of stream and reader, but if codecPtr
     *  is non-NULL, and decodeBounds succeeds, it will have created a new
     *  SkCodec (pointed to by *codecPtr) which will own/ref them, as well as
     *  the png_ptr and info_ptr.
     */
    AutoCleanAPng(png_structp png_ptr, SkStream* stream, SkPngChunkReader* reader,
            SkCodec** codecPtr, SkAPngReader *pAPngReader)
        : fPng_ptr(png_ptr)
        , fInfo_ptr(nullptr)
        , fStream(stream)
        , fChunkReader(reader)
        , fOutCodec(codecPtr)
		, fAPngReader(pAPngReader)
    {}

    ~AutoCleanAPng() {
        // fInfo_ptr will never be non-nullptr unless fPng_ptr is.
        if (fPng_ptr) {
            png_infopp info_pp = fInfo_ptr ? &fInfo_ptr : nullptr;
            png_destroy_read_struct(&fPng_ptr, info_pp, nullptr);
        }
		fAPngReader = nullptr;
    }

    void setInfoPtr(png_infop info_ptr) {
        SkASSERT(nullptr == fInfo_ptr);
        fInfo_ptr = info_ptr;
    }

    /**
     *  Reads enough of the input stream to decode the bounds.
     *  @return false if the stream is not a valid PNG (or too short).
     *          true if it read enough of the stream to determine the bounds.
     *          In the latter case, the stream may have been read beyond the
     *          point to determine the bounds, and the png_ptr will have saved
     *          any extra data. Further, if the codecPtr supplied to the
     *          constructor was not NULL, it will now point to a new SkCodec,
     *          which owns (or refs, in the case of the SkPngChunkReader) the
     *          inputs. If codecPtr was NULL, the png_ptr and info_ptr are
     *          unowned, and it is up to the caller to destroy them.
     */
    bool decodeBounds();

private:
    png_structp         fPng_ptr;
    png_infop           fInfo_ptr;
    SkStream*           fStream;
    SkPngChunkReader*   fChunkReader;
    SkCodec**           fOutCodec;
	SkAPngReader        *fAPngReader;

    void infoCallback(size_t idatLength);

    void releasePngPtrs() {
        fPng_ptr = nullptr;
        fInfo_ptr = nullptr;
    }
};
#define AutoCleanAPng(...) SK_REQUIRE_LOCAL_VAR(AutoCleanAPng)

static inline bool is_chunk(const png_byte* chunk, const char* tag) {
    return memcmp(chunk + 4, tag, 4) == 0;
}

static inline bool process_data(png_structp png_ptr, png_infop info_ptr,
        SkStream* stream, void* buffer, size_t bufferSize, size_t length) {
    while (length > 0) {
        const size_t bytesToProcess = std::min(bufferSize, length);
        const size_t bytesRead = stream->read(buffer, bytesToProcess);
        png_process_data(png_ptr, info_ptr, (png_bytep) buffer, bytesRead);
        if (bytesRead < bytesToProcess) {
            return false;
        }
        length -= bytesToProcess;
    }
    return true;
}

bool AutoCleanAPng::decodeBounds() {
    if (setjmp(PNG_JMPBUF(fPng_ptr))) {
        return false;
    }

    png_set_progressive_read_fn(fPng_ptr, nullptr, nullptr, nullptr, nullptr);

    // Arbitrary buffer size, though note that it matches (below)
    // SkPngCodec::processData(). FIXME: Can we better suit this to the size of
    // the PNG header?
    constexpr size_t kBufferSize = 4096;
    char buffer[kBufferSize];

    {
        // Parse the signature.
        if (fStream->read(buffer, 8) < 8) {
            return false;
        }

        png_process_data(fPng_ptr, fInfo_ptr, (png_bytep) buffer, 8);
    }

    while (true) {
        // Parse chunk length and type.
        if (fStream->read(buffer, 8) < 8) {
            // We have read to the end of the input without decoding bounds.
            break;
        }

        png_byte* chunk = reinterpret_cast<png_byte*>(buffer);
        const size_t length = png_get_uint_32(chunk);

        if (is_chunk(chunk, "IDAT")) {
            this->infoCallback(length);
            return true;
        }

        png_process_data(fPng_ptr, fInfo_ptr, chunk, 8);
        // Process the full chunk + CRC.
        if (!process_data(fPng_ptr, fInfo_ptr, fStream, buffer, kBufferSize, length + 4)) {
            return false;
        }
    }

    return false;
}

static const SkColorType kXformSrcColorType = kRGBA_8888_SkColorType;


///////////////////////////////////////////////////////////////////////////////
// Creation
///////////////////////////////////////////////////////////////////////////////
// function define : in SkPngCodec.cpp
sk_sp<SkColorSpace> read_color_space(png_structp png_ptr, png_infop info_ptr, SkColorSpace_Base::ICCTypeFlag iccType);

class SkAPngNormalDecoder final : public SkAPngCodec {
public:
	SkAPngNormalDecoder(const SkEncodedInfo& info, const SkImageInfo& imageInfo,
                       std::unique_ptr<SkStream> stream, SkPngChunkReader* reader,
                       png_structp png_ptr, png_infop info_ptr, int bitDepth, SkAPngReader *pAPngReader)
        : INHERITED(info, imageInfo, std::move(stream), reader, png_ptr, info_ptr, bitDepth, pAPngReader)
        , fRowsWrittenToOutput(0)
        , fDst(nullptr)
        , fRowBytes(0)
        , fFirstRow(0)
        , fLastRow(0)
    {
		if (pAPngReader)
			pAPngReader->setFrameDecoderType(E_Normal_Decoder);
	}

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

    typedef SkAPngCodec INHERITED;

    static SkAPngNormalDecoder* GetDecoder(png_structp png_ptr) {
        return static_cast<SkAPngNormalDecoder*>(png_get_progressive_ptr(png_ptr));
    }

    Result decodeAllRows(void* dst, size_t rowBytes, int* rowsDecoded) override {
        const int height = this->getInfo().height();
        png_set_progressive_read_fn(this->png_ptr(), this, nullptr, AllRowsCallback, nullptr);
        fDst = dst;
        fRowBytes = rowBytes;

        fRowsWrittenToOutput = 0;
        fFirstRow = 0;
        fLastRow = height - 1;

        this->processData();

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
        png_set_progressive_read_fn(this->png_ptr(), this, nullptr, RowCallback, nullptr);
        fFirstRow = firstRow;
        fLastRow = lastRow;
        fDst = dst;
        fRowBytes = rowBytes;
        fRowsWrittenToOutput = 0;
        fRowsNeeded = fLastRow - fFirstRow + 1;
    }

    SkCodec::Result decode(int* rowsDecoded) override {
        if (this->swizzler()) {
            const int sampleY = this->swizzler()->sampleY();
            fRowsNeeded = get_scaled_dimension(fLastRow - fFirstRow + 1, sampleY);
        }
        this->processData();

        if (fRowsWrittenToOutput == fRowsNeeded) {
            return SkCodec::kSuccess;
        }

        if (rowsDecoded) {
            *rowsDecoded = fRowsWrittenToOutput;
        }

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

class SkAPngInterlacedDecoder final : public SkAPngCodec {
public:
	SkAPngInterlacedDecoder(const SkEncodedInfo& info, const SkImageInfo& imageInfo,
            std::unique_ptr<SkStream> stream, SkPngChunkReader* reader, png_structp png_ptr,
            png_infop info_ptr, int bitDepth, int numberPasses, SkAPngReader *pAPngReader)
        : INHERITED(info, imageInfo, std::move(stream), reader, png_ptr, info_ptr, bitDepth, pAPngReader)
        , fNumberPasses(numberPasses)
        , fFirstRow(0)
        , fLastRow(0)
        , fLinesDecoded(0)
        , fInterlacedComplete(false)
        , fPng_rowbytes(0)
    {
		if (pAPngReader)
			pAPngReader->setFrameDecoderType(E_Interlaced_Decoder);
	}

    static void InterlacedRowCallback(png_structp png_ptr, png_bytep row, png_uint_32 rowNum, int pass) {
        auto decoder = static_cast<SkAPngInterlacedDecoder*>(png_get_progressive_ptr(png_ptr));
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

    typedef SkAPngCodec INHERITED;

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
        } else {
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
        // FIXME: We could skip rows in the interlace buffer that we won't put in the output.
        this->setUpInterlaceBuffer(lastRow - firstRow + 1);
        png_set_progressive_read_fn(this->png_ptr(), this, nullptr, InterlacedRowCallback, nullptr);
        fFirstRow = firstRow;
        fLastRow = lastRow;
        fDst = dst;
        fRowBytes = rowBytes;
        fLinesDecoded = 0;
    }

    SkCodec::Result decode(int* rowsDecoded) override {
        this->processData();

        // Now apply Xforms on all the rows that were decoded.
        if (!fLinesDecoded) {
            if (rowsDecoded) {
                *rowsDecoded = 0;
            }
            return SkCodec::kIncompleteInput;
        }

        const int sampleY = this->swizzler() ? this->swizzler()->sampleY() : 1;
        const int rowsNeeded = get_scaled_dimension(fLastRow - fFirstRow + 1, sampleY);
        int rowsWrittenToOutput = 0;

        // FIXME: For resuming interlace, we may swizzle a row that hasn't changed. But it
        // may be too tricky/expensive to handle that correctly.

        // Offset srcRow by get_start_coord rows. We do not need to account for fFirstRow,
        // since the first row in fInterlaceBuffer corresponds to fFirstRow.
        png_bytep srcRow = SkTAddOffset<png_byte>(fInterlaceBuffer.get(),
                                                  fPng_rowbytes * get_start_coord(sampleY));
        void* dst = fDst;
        for (; rowsWrittenToOutput < rowsNeeded; rowsWrittenToOutput++) {
            this->applyXformRow(dst, srcRow);
            dst = SkTAddOffset<void>(dst, fRowBytes);
            srcRow = SkTAddOffset<png_byte>(srcRow, fPng_rowbytes * sampleY);
        }

        if (fInterlacedComplete) {
            return SkCodec::kSuccess;
        }

        if (rowsDecoded) {
            *rowsDecoded = rowsWrittenToOutput;
        }
        return SkCodec::kIncompleteInput;
    }

    void setUpInterlaceBuffer(int height) {
        fPng_rowbytes = png_get_rowbytes(this->png_ptr(), this->info_ptr());
        fInterlaceBuffer.reset(fPng_rowbytes * height);
        fInterlacedComplete = false;
    }
};

// Reads the header and initializes the output fields, if not NULL.
//
// @param stream Input data. Will be read to get enough information to properly
//      setup the codec.
// @param chunkReader SkPngChunkReader, for reading unknown chunks. May be NULL.
//      If not NULL, png_ptr will hold an *unowned* pointer to it. The caller is
//      expected to continue to own it for the lifetime of the png_ptr.
// @param outCodec Optional output variable.  If non-NULL, will be set to a new
//      SkPngCodec on success.
// @param png_ptrp Optional output variable. If non-NULL, will be set to a new
//      png_structp on success.
// @param info_ptrp Optional output variable. If non-NULL, will be set to a new
//      png_infop on success;
// @return if kSuccess, the caller is responsible for calling
//      png_destroy_read_struct(png_ptrp, info_ptrp).
//      Otherwise, the passed in fields (except stream) are unchanged.
static SkCodec::Result read_apng_header(SkStream* stream, SkPngChunkReader* chunkReader,
                                   SkCodec** outCodec,
                                   png_structp* png_ptrp, png_infop* info_ptrp, SkAPngReader *pAPngReader) {
    // The image is known to be a PNG. Decode enough to know the SkImageInfo.
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                 sk_error_fn, sk_warning_fn);
    if (!png_ptr) {
        return SkCodec::kInternalError;
    }

    AutoCleanAPng autoClean(png_ptr, stream, chunkReader, outCodec, pAPngReader);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        return SkCodec::kInternalError;
    }

    autoClean.setInfoPtr(info_ptr);

    if (setjmp(PNG_JMPBUF(png_ptr))) {
        return SkCodec::kInvalidInput;
    }

#ifdef PNG_READ_UNKNOWN_CHUNKS_SUPPORTED
    // Hookup our chunkReader so we can see any user-chunks the caller may be interested in.
    // This needs to be installed before we read the png header.  Android may store ninepatch
    // chunks in the header.
    if (chunkReader) {
        png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, (png_byte*)"", 0);
        png_set_read_user_chunk_fn(png_ptr, (png_voidp) chunkReader, sk_read_user_chunk);
    }
#endif

    const bool decodedBounds = autoClean.decodeBounds();

    if (!decodedBounds) {
        return SkCodec::kIncompleteInput;
    }

    // On success, decodeBounds releases ownership of png_ptr and info_ptr.
    if (png_ptrp) {
        *png_ptrp = png_ptr;
    }
    if (info_ptrp) {
        *info_ptrp = info_ptr;
    }

    // decodeBounds takes care of setting outCodec
    if (outCodec) {
        SkASSERT(*outCodec);
    }
    return SkCodec::kSuccess;
}

void AutoCleanAPng::infoCallback(size_t idatLength) {
    png_uint_32 origWidth, origHeight;
    int bitDepth, encodedColorType;
    png_get_IHDR(fPng_ptr, fInfo_ptr, &origWidth, &origHeight, &bitDepth,
                 &encodedColorType, nullptr, nullptr, nullptr);

    // TODO: Should we support 16-bits of precision for gray images?
    if (bitDepth == 16 && (PNG_COLOR_TYPE_GRAY == encodedColorType ||
                           PNG_COLOR_TYPE_GRAY_ALPHA == encodedColorType)) {
        bitDepth = 8;
        png_set_strip_16(fPng_ptr);
    }

    // Now determine the default colorType and alphaType and set the required transforms.
    // Often, we depend on SkSwizzler to perform any transforms that we need.  However, we
    // still depend on libpng for many of the rare and PNG-specific cases.
    SkEncodedInfo::Color color;
    SkEncodedInfo::Alpha alpha;
    switch (encodedColorType) {
        case PNG_COLOR_TYPE_PALETTE:
            // Extract multiple pixels with bit depths of 1, 2, and 4 from a single
            // byte into separate bytes (useful for paletted and grayscale images).
            if (bitDepth < 8) {
                // TODO: Should we use SkSwizzler here?
                bitDepth = 8;
                png_set_packing(fPng_ptr);
            }

            color = SkEncodedInfo::kPalette_Color;
            // Set the alpha depending on if a transparency chunk exists.
            alpha = png_get_valid(fPng_ptr, fInfo_ptr, PNG_INFO_tRNS) ?
                    SkEncodedInfo::kUnpremul_Alpha : SkEncodedInfo::kOpaque_Alpha;
            break;
        case PNG_COLOR_TYPE_RGB:
            if (png_get_valid(fPng_ptr, fInfo_ptr, PNG_INFO_tRNS)) {
                // Convert to RGBA if transparency chunk exists.
                png_set_tRNS_to_alpha(fPng_ptr);
                color = SkEncodedInfo::kRGBA_Color;
                alpha = SkEncodedInfo::kBinary_Alpha;
            } else {
                color = SkEncodedInfo::kRGB_Color;
                alpha = SkEncodedInfo::kOpaque_Alpha;
            }
            break;
        case PNG_COLOR_TYPE_GRAY:
            // Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel.
            if (bitDepth < 8) {
                // TODO: Should we use SkSwizzler here?
                bitDepth = 8;
                png_set_expand_gray_1_2_4_to_8(fPng_ptr);
            }

            if (png_get_valid(fPng_ptr, fInfo_ptr, PNG_INFO_tRNS)) {
                png_set_tRNS_to_alpha(fPng_ptr);
                color = SkEncodedInfo::kGrayAlpha_Color;
                alpha = SkEncodedInfo::kBinary_Alpha;
            } else {
                color = SkEncodedInfo::kGray_Color;
                alpha = SkEncodedInfo::kOpaque_Alpha;
            }
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            color = SkEncodedInfo::kGrayAlpha_Color;
            alpha = SkEncodedInfo::kUnpremul_Alpha;
            break;
        case PNG_COLOR_TYPE_RGBA:
            color = SkEncodedInfo::kRGBA_Color;
            alpha = SkEncodedInfo::kUnpremul_Alpha;
            break;
        default:
            // All the color types have been covered above.
            SkASSERT(false);
            color = SkEncodedInfo::kRGBA_Color;
            alpha = SkEncodedInfo::kUnpremul_Alpha;
    }

    const int numberPasses = png_set_interlace_handling(fPng_ptr);

    if (fOutCodec) {
        SkASSERT(nullptr == *fOutCodec);
        SkColorSpace_Base::ICCTypeFlag iccType = SkColorSpace_Base::kRGB_ICCTypeFlag;
        if (SkEncodedInfo::kGray_Color == color || SkEncodedInfo::kGrayAlpha_Color == color) {
            iccType |= SkColorSpace_Base::kGray_ICCTypeFlag;
        }
        sk_sp<SkColorSpace> colorSpace = read_color_space(fPng_ptr, fInfo_ptr, iccType);
        if (!colorSpace) {
            // Treat unsupported/invalid color spaces as sRGB.
            colorSpace = SkColorSpace::MakeSRGB();
        }

        SkEncodedInfo encodedInfo = SkEncodedInfo::Make(color, alpha, bitDepth);
        SkImageInfo imageInfo = encodedInfo.makeImageInfo(origWidth, origHeight, colorSpace);

        if (SkEncodedInfo::kOpaque_Alpha == alpha) {
            png_color_8p sigBits;
            if (png_get_sBIT(fPng_ptr, fInfo_ptr, &sigBits)) {
                if (5 == sigBits->red && 6 == sigBits->green && 5 == sigBits->blue) {
                    // Recommend a decode to 565 if the sBIT indicates 565.
                    imageInfo = imageInfo.makeColorType(kRGB_565_SkColorType);
                }
            }
        }

        if (1 == numberPasses) {
            *fOutCodec = new SkAPngNormalDecoder(encodedInfo, imageInfo,
                   std::unique_ptr<SkStream>(fStream), fChunkReader, fPng_ptr, fInfo_ptr, bitDepth, fAPngReader);
        } else {
            *fOutCodec = new SkAPngInterlacedDecoder(encodedInfo, imageInfo,
                    std::unique_ptr<SkStream>(fStream), fChunkReader, fPng_ptr, fInfo_ptr, bitDepth,
                    numberPasses, fAPngReader);
        }
        static_cast<SkPngCodec*>(*fOutCodec)->setIdatLength(idatLength);
    }

    // Release the pointers, which are now owned by the codec or the caller is expected to
    // take ownership.
    this->releasePngPtrs();
}

SkAPngCodec::SkAPngCodec(const SkEncodedInfo& encodedInfo, const SkImageInfo& imageInfo,
                       std::unique_ptr<SkStream> stream, SkPngChunkReader* chunkReader,
                       void* png_ptr, void* info_ptr, int bitDepth, SkAPngReader *pAPngReader)
    : INHERITED(encodedInfo, imageInfo, std::move(stream), chunkReader, png_ptr, info_ptr, bitDepth)
	, m_pAPngReader(pAPngReader)
{}

SkAPngCodec::~SkAPngCodec() {
	if (m_pAPngReader)
		delete m_pAPngReader;
}

bool SkAPngCodec::onRewind() {
	// This sets fPng_ptr and fInfo_ptr to nullptr. If read_apng_header
	// succeeds, they will be repopulated, and if it fails, they will
	// remain nullptr. Any future accesses to fPng_ptr and fInfo_ptr will
	// come through this function which will rewind and again attempt
	// to reinitialize them.
	this->destroyReadStruct();

	png_structp png_ptr;
	png_infop info_ptr;
	if (kSuccess != read_apng_header(this->stream(), fPngChunkReader.get(), nullptr,
		&png_ptr, &info_ptr, m_pAPngReader)) {
		return false;
	}

	size_t position = stream()->getPosition();

	fPng_ptr = png_ptr;
	fInfo_ptr = info_ptr;
	fDecodedIdat = false;
	return true;
}

SkCodec::Result SkAPngCodec::onGetPixels(const SkImageInfo& dstInfo, void* dst,
                                        size_t rowBytes, const Options& options,
                                        int* rowsDecoded) {
	Result result;
	if (options.fFrameIndex > 0)
	{
		std::unique_ptr<SkAPngFrameDecoder> frameCodec = SkAPngFrameDecoder::MakeFrameDecoder(stream(), &result, this, options.fFrameIndex);

		return frameCodec->decodeFrame(dstInfo, dst, options, rowBytes, rowsDecoded);
	}

    result = this->initializeXforms(dstInfo, options);
    if (kSuccess != result) {
        return result;
    }

    if (options.fSubset) {
        return kUnimplemented;
    }

    this->allocateStorage(dstInfo);
    this->initializeXformParams();
    return this->decodeAllRows(dst, rowBytes, rowsDecoded);
}

// decode all apng frame headers
bool AutoCleanAPngFrames::decodeFrameInfos(SkStream* stream, SkAPngReader * apngReader)
{
	if (apngReader && apngReader->needParseFrameInfo())
	{
		png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, sk_error_fn, sk_warning_fn);
		if (!png_ptr) {
			return false;
		}
		AutoCleanAPngFrames autoApngFrames(png_ptr, nullptr, stream, apngReader);
		png_infop info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == nullptr) {
			return false;
		}
		autoApngFrames.setInfoPtr(info_ptr);

		if (setjmp(PNG_JMPBUF(png_ptr))) {
			return false;
		}

		png_set_progressive_read_fn(png_ptr, nullptr, nullptr, nullptr, nullptr);

		constexpr size_t kBufferSize = 4096;
		char buffer[kBufferSize];
		{
			// Parse the signature.
			if (stream->read(buffer, 8) < 8) {
				return false;
			}

			png_process_data(png_ptr, info_ptr, (png_bytep)buffer, 8);
		}

		bool hasParsedHeader = false;

		bool hasParsedFrameHeader = false;

		int posIDHRData = -1;

		while (true) {
			// Parse chunk length and type.
			if (stream->read(buffer, 8) < 8) {
				// We have read to the end of the input without decoding bounds.
				apngReader->setNeedParseFrameInfo(false);
				break;
			}

			png_byte* chunk = reinterpret_cast<png_byte*>(buffer);
			const size_t length = png_get_uint_32(chunk);

			if (is_chunk(chunk, "IHDR"))
			{
				posIDHRData = stream->getPosition();
			}

			if (!hasParsedHeader && is_chunk(chunk, "IDAT"))
			{
				{
					// here : if haven't parsed frame header, this apng not correct
					if (!hasParsedFrameHeader)
						return false;

					if (posIDHRData == -1)
						return false;

					size_t posAPngHeaderEnd = stream->getPosition();

					stream->seek(0);
					void* apngHeaderData = SkAPngReader::CreateAPngHeaderData(posAPngHeaderEnd - 8);
					stream->read(apngHeaderData, posAPngHeaderEnd - 8);
					apngReader->setAPngHeadData(apngHeaderData, posAPngHeaderEnd - 8, posIDHRData);
					stream->seek(posAPngHeaderEnd);

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

					// save this info to SkAPngReader
					const int numberPasses = png_set_interlace_handling(png_ptr);
					apngReader->setNumberPasses(numberPasses);				
				}
				hasParsedHeader = true;
			}

			if (is_chunk(chunk, "acTL")) {
				if (length == 8)
				{
					if (!apngReader->parseFrameInfos(stream, 8, "acTL"))
						return false;
					hasParsedFrameHeader = true;
				}
			}
			if (is_chunk(chunk, "fcTL")) {
				if (length == 26)
				{
					if (!apngReader->parseFrameInfos(stream, 26, "fcTL"))
						return false;
				}
			}
			if (is_chunk(chunk, "fdAT")) {
				if (length >= 4)
				{
					if (!apngReader->parseFrameInfos(stream, length, "fdAT"))
						return false;
				}
			}

			png_process_data(png_ptr, info_ptr, chunk, 8);
			// Process the full chunk + CRC.
			if (!process_data(png_ptr, info_ptr, stream, buffer, kBufferSize, length + 4)) {
				return false;
			}
		}

		if (apngReader->frameCount() <= 1)
		{
			return false;
		}
	}
	return true;
}

//for apng
SkCodec::Result SkAPngCodec::parseAPngInfos(SkCodec** outCodec, SkStream* stream, SkPngChunkReader* chunkReader)
{
	SkAPngReader * pAPngReader = new SkAPngReader();
	bool parseSuccess = AutoCleanAPngFrames::decodeFrameInfos(stream, pAPngReader);
	if (!parseSuccess)
	{
		delete pAPngReader;
		return kUnimplemented;
	}
	Result result = read_apng_header(stream, chunkReader, outCodec, nullptr, nullptr, pAPngReader);
	if (kSuccess == result) {
		// Codec has taken ownership of the stream.
		SkASSERT(outCodec);
	}
	else
	{
		delete pAPngReader;
	}

	return result;
}


int SkAPngCodec::onGetFrameCount()
{
	return m_pAPngReader->frameCount();
}
bool SkAPngCodec::onGetFrameInfo(int i, FrameInfo* frameInfo) const
{
	if (!m_pAPngReader)
		return false;

	if (i >= m_pAPngReader->frameCount()) {
		return false;
	}

	const SkAPngFrameContext* frameContext = m_pAPngReader->frameContext(i);
	if (!frameContext)
		return false;
	SkASSERT(frameContext->reachedStartOfData());

	if (frameInfo)
	{
		frameInfo->fDuration = frameContext->getDuration();
		frameInfo->fRequiredFrame = frameContext->getRequiredFrame();
		frameInfo->fFullyReceived = frameContext->isComplete();
#ifdef SK_LEGACY_FRAME_INFO_ALPHA_TYPE
		frameInfo->fAlphaType = frameContext->hasAlpha() ? kUnpremul_SkAlphaType
			: kOpaque_SkAlphaType;
#endif
		frameInfo->fAlpha = frameContext->hasAlpha() ? SkEncodedInfo::kBinary_Alpha
			: SkEncodedInfo::kOpaque_Alpha;
		frameInfo->fDisposalMethod = frameContext->getDisposalMethod();
	}
	return true;
}
int SkAPngCodec::onGetRepetitionCount()
{
	if (m_pAPngReader)
	{
		return m_pAPngReader->repetitionCount();
	}
	return 0;
}
const SkFrameHolder* SkAPngCodec::getFrameHolder() const
{
	return m_pAPngReader;
}

SkStream *SkAPngCodec::makeFrameStream(int frameIndex)
{
	return this->stream();
}
