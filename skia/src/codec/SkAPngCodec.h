/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkAPngCodec_DEFINED
#define SkAPngCodec_DEFINED

#include "SkPngCodec.h"

class SkAPngReader;
class SkAPngCodec : public SkPngCodec {
public:
    ~SkAPngCodec() override;

	static Result parseAPngInfos(SkCodec** outCodec, SkStream* stream, SkPngChunkReader* chunkReader);

	int getBitDepth()
	{
		return fBitDepth;
	}
	SkAPngReader* getAPngReader()
	{
		return m_pAPngReader;
	}

	SkStream *makeFrameStream(int frameIndex);

protected:
    SkAPngCodec(const SkEncodedInfo&, const SkImageInfo&, std::unique_ptr<SkStream>,
               SkPngChunkReader*, void* png_ptr, void* info_ptr, int bitDepth, SkAPngReader *pAPngReader);

	bool onRewind();
    Result onGetPixels(const SkImageInfo&, void*, size_t, const Options&, int*) override;

	int onGetFrameCount() override;
	bool onGetFrameInfo(int, FrameInfo*) const override;
	int onGetRepetitionCount() override;
	const SkFrameHolder* getFrameHolder() const override;

	SkAPngReader* m_pAPngReader;

private:

    typedef SkPngCodec INHERITED;
};
#endif  // SkAPngCodec_DEFINED
