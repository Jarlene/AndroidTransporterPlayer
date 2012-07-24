#include "RtpPcmAssembler.h"
#include "Buffer.h"
#include "android/os/Message.h"
#include <string.h>
#include <stdio.h>

using namespace android::os;
using namespace android::util;

static const size_t FRAMES_PER_UNIT = 2205;
static const size_t BYTES_PER_FRAME = 4;
static const size_t UNIT_SIZE = FRAMES_PER_UNIT * BYTES_PER_FRAME;
static const size_t RTP_PACKET_SMALL_SIZE = 492;
static const size_t RTP_PACKET_LARGE_SIZE = 1388;


RtpPcmAssembler::RtpPcmAssembler(android::util::List< sp<Buffer> >& queue, const sp<android::os::Message>& notifyAccessUnit) :
		mQueue(queue),
		mNotifyAccessUnit(notifyAccessUnit),
		mAccessUnit(NULL),
		mAccessUnitOffset(0),
		mStartStream(false),
		mLastRtpPacketSize(0) {
}

RtpPcmAssembler::~RtpPcmAssembler() {
}

void RtpPcmAssembler::processMediaData() {

	if (!mQueue.empty()) {
		sp<Buffer> buffer = *mQueue.begin();
		mQueue.erase(mQueue.begin());

		const uint8_t* data = buffer->data();
		size_t size = buffer->size();

		if (!mStartStream && mLastRtpPacketSize == RTP_PACKET_SMALL_SIZE && size == RTP_PACKET_LARGE_SIZE) {
			mStartStream = true;
		} else {
			mLastRtpPacketSize = size;
		}

		if (mStartStream) {
			if(mAccessUnit == NULL) {
				mAccessUnit = new Buffer(UNIT_SIZE);
			}
			uint16_t* dest = (uint16_t*) (mAccessUnit->data() + mAccessUnitOffset);
			uint16_t* source = (uint16_t*) data;

			for(size_t i = 0; i < size; i += 2) {
				*dest = (*source >> 8) | ((*source  << 8) & 0xFF00);
				dest++;
				source++;
				mAccessUnitOffset += 2;
				if(mAccessUnitOffset >= UNIT_SIZE) {
					sp<Message> msg = mNotifyAccessUnit->dup();
					msg->obj = new sp<Buffer>(mAccessUnit);
					assert(msg->sendToTarget());
					mAccessUnitOffset = 0;
					mAccessUnit = new Buffer(UNIT_SIZE);
					dest = (uint16_t*) (mAccessUnit->data());
				}
			}
		}
	}
}
