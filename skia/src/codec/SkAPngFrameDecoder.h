/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef APngFrameDecoder_DEFINED
#define APngFrameDecoder_DEFINED

#include "SkPngCodec.h"

class SkAPngReader;
class SkAPngCodec;
class SkAPngFrameDecoder : public SkPngCodec {
public:
	~SkAPngFrameDecoder() override;

	static std::unique_ptr<SkAPngFrameDecoder> MakeFrameDecoder(SkStream* stream, Result*, SkAPngCodec* pMainCodec, int frameIndex);

	SkCodec::Result decodeFrame(const SkImageInfo& dstInfo, void* dst, const Options& options, size_t rowBytes, int* rowsDecoded);

protected:
	SkAPngFrameDecoder(const SkEncodedInfo&, const SkImageInfo&, SkStream* pStream, void* png_ptr, void* info_ptr, int bitDepth, SkAPngCodec* pMainCodec, int frameIndex);

	void processfdATData(size_t fdATPos, size_t fdATLength);
	
	SkStream * m_pMainCodecStream;
	SkAPngCodec* m_pMainCodec;
	int m_frameIndex;

private:

    typedef SkPngCodec INHERITED;
};
#endif  // APngFrameDecoder_DEFINED
