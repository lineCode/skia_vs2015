/*
 * Copyright 2017 IQY Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBitmap.h"
#include "SkAPngReader.h"
#include "SkPngCodec.h"

const unsigned long cMaxPNGSize = 1000000UL;

// FIXME (scroggo): We can use png_jumpbuf directly once Google3 is on 1.6
#define JMPBUF(x) png_jmpbuf((png_structp) x)

SkAPngFrameContext::SkAPngFrameContext(SkAPngReader* reader, int id)
	: INHERITED(id)
	, m_owner(reader)
	, m_isComplete(false)
{
	setRequiredFrame(SkCodec::kNone);
}
SkAPngFrameContext::~SkAPngFrameContext()
{
	
}

///////////////////////////////////////////////////////////////////////////////
// apng chunk reader
///////////////////////////////////////////////////////////////////////////////
SkAPngReader::SkAPngReader()
	: m_frameCount(1)
	, m_playCount(0)
	, m_totalFrames(0)
	, m_sequenceNumber(0)
	, m_decodeType(E_Normal_Decoder)
	, m_bNeedParseFrameInfo(true)
	, m_apngHeaderData(nullptr)
{

}
SkAPngReader::~SkAPngReader()
{
	if (m_apngHeaderData)
		DestroyAPngHeaderData(m_apngHeaderData);
	m_apngHeaderData = nullptr;
}
int SkAPngReader::frameCount() const
{
	return m_frameCount;
}
int SkAPngReader::repetitionCount() const
{
	if (!m_playCount)
		return SkCodec::kRepetitionCountInfinite;
	return m_playCount;
}

void SkAPngReader::fallbackNotAnimated()
{
	m_frames.clear();
	m_totalFrames = 0;
	m_playCount = 0;
}

const SkAPngFrameContext* SkAPngReader::frameContext(int index) const
{
	return index >= 0 && index < static_cast<int>(m_frames.size())
		? m_frames[index].get() : 0;
}

void SkAPngReader::addFrameIfNecessary()
{
	if (m_frames.empty() || m_frames.back()->isComplete()) {
		const size_t i = m_frames.size();
		std::unique_ptr<SkAPngFrameContext> frame(new SkAPngFrameContext(this, static_cast<int>(i)));
		m_frames.push_back(std::move(frame));
	}
}
const SkFrame* SkAPngReader::onGetFrame(int i) const {
	return static_cast<const SkFrame*>(this->frameContext(i));
}

bool SkAPngReader::parseFrameInfos(SkStream* stream, int length, const char * tag)
{
	if (!memcmp(tag, "acTL", 4) && length == 8) 
	{	
		char acTLBuf[8];
		stream->read(acTLBuf, 8);
		stream->move(-8);
		m_frameCount = png_get_uint_32((png_byte*)acTLBuf);
		m_playCount = png_get_uint_32((png_byte*)acTLBuf + 4);

		if (!m_frameCount || m_frameCount > PNG_UINT_31_MAX || m_playCount > PNG_UINT_31_MAX) {
			fallbackNotAnimated();
			return false;
		}
	}
	if (!memcmp(tag, "fcTL", 4) && length == 26) 
	{
		char fcTLBuf[26];
		stream->read(fcTLBuf, 26);
		stream->move(-26);

		unsigned sequenceNumber = png_get_uint_32((png_byte*)fcTLBuf);
		if (sequenceNumber != m_sequenceNumber++) {
			fallbackNotAnimated();
			return false;
		}

		int m_width;
		int m_height;
		int m_xOffset;
		int m_yOffset;
		int m_delayNumerator;
		int m_delayDenominator;
		int m_dispose;
		int m_blend;

		m_width = png_get_uint_32((png_byte*)fcTLBuf + 4);
		m_height = png_get_uint_32((png_byte*)fcTLBuf + 8);

		if (sequenceNumber == 0)
		{
			fScreenWidth = m_width;
			fScreenHeight = m_height;
		}

		m_xOffset = png_get_uint_32((png_byte*)fcTLBuf + 12);
		m_yOffset = png_get_uint_32((png_byte*)fcTLBuf + 16);
		m_delayNumerator = png_get_uint_16((png_byte*)fcTLBuf + 20);
		m_delayDenominator = png_get_uint_16((png_byte*)fcTLBuf + 22);
		m_dispose = ((png_byte*)fcTLBuf)[24];
		m_blend = ((png_byte*)fcTLBuf)[25];		

		// 
		addFrameIfNecessary();

		if (m_totalFrames < (int)(m_frames.size())) {
			SkAPngFrameContext* buffer = m_frames[m_totalFrames].get();

			if (!m_delayDenominator)
				buffer->setDuration(m_delayNumerator * 10);
			else
				buffer->setDuration(m_delayNumerator * 1000 / m_delayDenominator);

			//buffer->setDuration(3000);
			int duration = buffer->getDuration();
			char buf[64];
			sprintf(buf, "fcTL-sequence:%d,w(%d),h(%d),d(%d)\n", m_totalFrames, m_width, m_height, duration);
			//OutputDebugStringA(buf);

			if (m_dispose == 2)
				buffer->setDisposalMethod(SkCodecAnimation::DisposalMethod::kRestorePrevious);
			else if (m_dispose == 1)
				buffer->setDisposalMethod(SkCodecAnimation::DisposalMethod::kRestoreBGColor);
			else
				buffer->setDisposalMethod(SkCodecAnimation::DisposalMethod::kKeep);

			buffer->setXYWH(m_xOffset, m_yOffset, m_width, m_height);

			if (m_blend)
				buffer->setBlend(SkCodecAnimation::Blend::kPriorFrame);
			else 
				buffer->setBlend(SkCodecAnimation::Blend::kBG);

			setAlphaAndRequiredFrame(buffer);
		}
		m_frames.back()->setComplete();

		m_totalFrames++;
	}
	if (!memcmp(tag, "fdAT", 4) && length >= 4) 
	{
		char fdATBuf[4];
		stream->read(fdATBuf, 4);
		stream->move(-4);

		unsigned sequenceNumber = png_get_uint_32((png_byte*)fdATBuf);
		if (sequenceNumber != m_sequenceNumber++) {
			fallbackNotAnimated();
			return false;
		}

		SkAPngFrameContext* frame = m_frames.back().get();
		frame->addAPngFrameBlock(stream->getPosition() + 4, length - 4);
		
	}
	return true;
}

void SkAPngReader::setIHDR_w_h(int width, int height)
{
	png_save_uint_32(m_apngHeaderData + m_posIDHRData, width);
	png_save_uint_32(m_apngHeaderData + m_posIDHRData + 4, height);
}

void SkAPngReader::setAPngHeadData(void* apngHeaderData, int len, size_t pos)
{
	if (m_apngHeaderData)
		DestroyAPngHeaderData(m_apngHeaderData);
	m_apngHeaderData = (png_bytep)apngHeaderData;
	m_apngHeaderDataLen = len;
	m_posIDHRData = pos;
}

void * SkAPngReader::CreateAPngHeaderData(size_t data_size)
{
	return malloc(data_size);
}
void SkAPngReader::DestroyAPngHeaderData(void * pData)
{
	free(pData);
}
