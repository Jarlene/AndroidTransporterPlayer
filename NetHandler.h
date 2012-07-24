#ifndef NETHANDLER_H_
#define NETHANDLER_H_

#include "android/os/Handler.h"
#include "android/os/LooperThread.h"
#include "RtspMediaSource.h"
#include "RtpMediaSource.h"

class RPiPlayer;

using android::os::sp;

class NetHandler :
	public android::os::Handler
{
public:
	static const uint32_t START_MEDIA_SOURCE = 0;
	static const uint32_t STOP_MEDIA_SOURCE = 1;
	static const uint32_t START_VIDEO_TRACK = 2;
	static const uint32_t STOP_VIDEO_TRACK = 3;

	NetHandler();
	~NetHandler();
	virtual void handleMessage(const sp<android::os::Message>& message);

private:
	static const uint16_t RTP_VIDEO_SOURCE_PORT = 56098;
	static const uint8_t AVC_VIDEO_TYPE = 96;

	sp<RPiPlayer> mPlayer;
	sp<RtspMediaSource> mRtspMediaSource;
	sp<RtpMediaSource> mRtpAudioSource;
	sp<RtpMediaSource> mRtpVideoSource;
};

#endif /* NETHANDLER_H_ */
