/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkAPngReader_DEFINED
#define SkAPngReader_DEFINED

#include "SkAPngCodec.h"
#include "SkFrameHolder.h"
#include "png.h"

class SkAPngReader;

struct SkAPngFrameBlock {
public:
	SkAPngFrameBlock(size_t position, size_t size)
		: blockPosition(position), blockSize(size) {}

	size_t blockPosition;
	size_t blockSize;
};

class SkAPngFrameContext : public SkFrame {
public:
	SkAPngFrameContext(SkAPngReader* reader, int id);
	~SkAPngFrameContext() override;

	bool isComplete() const { return m_isComplete; }
	void setComplete() { m_isComplete = true; }

	void addAPngFrameBlock(size_t position, size_t size)
	{
		m_fadtBlocks.push_back(SkAPngFrameBlock(position, size));
	}
	const std::vector<SkAPngFrameBlock> * getIDATBlocks() const
	{
		return &m_fadtBlocks;
	}

protected:
	bool onReportsAlpha() const override
	{
		return false;
	}

private:

	const SkAPngReader* m_owner;

	// apng frame blocks for this frame.
	std::vector<SkAPngFrameBlock> m_fadtBlocks;

	bool m_isComplete;

	typedef SkFrame INHERITED;

	friend class SkAPngReader;
};

enum EAPngFrameDecoderType
{
	E_Normal_Decoder,
	E_Interlaced_Decoder
};

class SkAPngReader final : public SkFrameHolder
{
public:
	SkAPngReader();
	~SkAPngReader();

public:
	static void * CreateAPngHeaderData(size_t data_size);
	static void DestroyAPngHeaderData(void * pData);

public:
	int frameCount() const;
	int repetitionCount() const;

	void fallbackNotAnimated();

	bool needParseFrameInfo()
	{
		return m_bNeedParseFrameInfo;
	}
	void setNeedParseFrameInfo(bool bNeed)
	{
		m_bNeedParseFrameInfo = bNeed;
	}

	const SkAPngFrameContext* frameContext(int index) const;

	// parse apng's all frame info, don't parse frame's data
	bool parseFrameInfos(SkStream* stream, int length, const char * tag);

	// frame png IHDR header
	void setIHDR_w_h(int width, int height);
	void setAPngHeadData(void* apngHeaderData, int len, size_t pos);
	void* getAPngHeadData()
	{
		return m_apngHeaderData;
	}
	size_t getAPngHeadDataLen()
	{
		return m_apngHeaderDataLen;
	}
	
	// frame decoder type
	void setFrameDecoderType(int type)
	{
		m_decodeType = type;
	}
	int getFrameDecoderType()
	{
		return m_decodeType;
	}

	// interlactce decoder's numpasses
	int getNumberPasses()
	{
		return m_iNumberPasses;
	}
	void setNumberPasses(int numberPasses)
	{
		m_iNumberPasses = numberPasses;
	}
	
protected:
	void addFrameIfNecessary();
	const SkFrame* onGetFrame(int i) const override;

protected:
	int m_frameCount;
	int m_playCount;
	int m_totalFrames;
	int m_sequenceNumber;
	int m_decodeType;
	size_t m_posIDHRData;

	png_bytep m_apngHeaderData;
	size_t m_apngHeaderDataLen;

	bool m_bNeedParseFrameInfo;

	std::vector<std::unique_ptr<SkAPngFrameContext>> m_frames;

	int m_iNumberPasses;

private:
	friend class SkAPngCodec;
};

#endif  // SkAPngReader_DEFINED
