#include "RtspMediaSource.h"
#include "NetHandler.h"
#include "android/os/Message.h"
#include "android/os/Handler.h"
#include "android/util/Buffer.h"
#include <stdio.h>

using namespace android::os;
using namespace android::lang;
using namespace android::util;
using namespace android::net;

RtspMediaSource::RtspMediaSource(const sp<android::os::Handler>& netHandler) :
		mNetHandler(netHandler),
		mCSeq(1) {
}

RtspMediaSource::~RtspMediaSource() {
}

bool RtspMediaSource::start(const android::lang::String& url) {
	if (!url.startsWith("rtsp://")) {
		return false;
	}

	String mediaSource = url.substr(strlen("rtsp://"));
	ssize_t separatorIndex = mediaSource.indexOf("/");
	if  (separatorIndex > 0) {
		mSdpFile = mediaSource.substr(separatorIndex + 1);
		mediaSource = mediaSource.substr(0, separatorIndex);
	} else {
		return false;
	}

	separatorIndex = mediaSource.indexOf(":");
	if (separatorIndex > 0) {
		mHost = mediaSource.substr(0, separatorIndex);
		mPort = mediaSource.substr(separatorIndex + 1);
	} else {
		mHost = mediaSource;
		mPort = "9000";
	}

	mSocket = new RtspSocket();
	if (!mSocket->connect(mHost.c_str(), atoi(mPort.c_str()))) {
		return false;
	}

	mNetReceiver = new NetReceiver(this);
	if (!mNetReceiver->start()) {
		return false;
	}

	describeMediaSource();

	return true;
}

void RtspMediaSource::stop() {
	if (mNetReceiver != NULL) {
		mNetReceiver->stop();
	}
	mNetHandler = NULL;
}

void RtspMediaSource::handleMessage(const sp<android::os::Message>& message) {
	switch (message->what) {
	case DESCRIBE_MEDIA_SOURCE: {
		RtspHeader* rtspHeader = (RtspHeader*) message->obj;
		sp<Bundle> bundle = message->getData();
		sp<Buffer> content;
		if (bundle != NULL) {
			content = bundle->getObject<Buffer>("Content");
		}
		if ((*rtspHeader)[String("ResultCode")] == "200") {
			onDescribeMediaSource(content);
		} else {
			// TODO: error handling
		}
		delete rtspHeader;
		break;
	}
	case START_VIDEO_TRACK: {
		setupVideoTrack(message->arg1);
		break;
	}
	case SETUP_VIDEO_TRACK: {
		RtspHeader* rtspHeader = (RtspHeader*) message->obj;
		if ((*rtspHeader)[String("ResultCode")] == "200") {
			mVideoSessionId = (*rtspHeader)[String("Session").toLowerCase()];
			playVideoTrack();
		} else {
			// TODO: error handling
		}
		delete rtspHeader;
		break;
	}
	case PLAY_VIDEO_TRACK: {
		RtspHeader* rtspHeader = (RtspHeader*) message->obj;
		if ((*rtspHeader)[String("ResultCode")] == "200") {
			// OK
		} else {
			// TODO: error handling
		}
		delete rtspHeader;
		break;
	}
	}
}

void RtspMediaSource::describeMediaSource() {
	setPendingRequest(mCSeq, obtainMessage(DESCRIBE_MEDIA_SOURCE));
	String describeMessage = String::format("DESCRIBE rtsp://%s:%s/%s RTSP/1.0\r\nCSeq: %d\r\n\r\n",
			mHost.c_str(), mPort.c_str(), mSdpFile.c_str(), mCSeq++);
	mSocket->write(describeMessage.c_str(), describeMessage.size());
}

void RtspMediaSource::onDescribeMediaSource(const sp<Buffer>& desc) {
	String mediaSourceDesc((char*)desc->data(), desc->size());
	sp< List<String> > lines = mediaSourceDesc.split("\n");
	List<String>::iterator itr = lines->begin();
	String mediaDesc;
	String audioMediaDesc;
	String videoMediaDesc;

	while (itr != lines->end()) {
		printf("%s\n", itr->c_str());
		if (itr->startsWith("m=")) {
			mediaDesc = *itr;
			if (mediaDesc.startsWith("m=audio 0 RTP/AVP 10")) {
				audioMediaDesc = mediaDesc;
			} else if (mediaDesc.startsWith("m=video 0 RTP/AVP 96")) {
				videoMediaDesc = mediaDesc;
			} else {
				videoMediaDesc = NULL;
			}
		} else if (itr->startsWith("a=")) {
			if (itr->startsWith("a=control:")) {
				if (!audioMediaDesc.isEmpty()) {
					mAudioMediaSource = itr->substr(String::size("a=control:")).trim();
				} else if (!videoMediaDesc.isEmpty()) {
					mVideoMediaSource = itr->substr(String::size("a=control:")).trim();
					sp<Message> msg = mNetHandler->obtainMessage(NetHandler::START_VIDEO_TRACK);
					msg->arg1 = 96;
					msg->sendToTarget();
				}
			}
		}
		++itr;
	}
}

void RtspMediaSource::setupAudioTrack(uint16_t port) {
	String setupMessage = String::format("SETUP %s RTSP/1.0\r\nCSeq: %d\r\nTransport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
			mAudioMediaSource.c_str(), mCSeq++, port, port + 1);
	mSocket->write(setupMessage.c_str(), setupMessage.size());
}

void RtspMediaSource::playAudioTrack() {
	String playMessage = String::format("PLAY %s RTSP/1.0\r\nCSeq: %d\r\nRange: npt=0.000-\r\nSession: %s\r\n\r\n",
			mAudioMediaSource.c_str(), mCSeq++, mAudioSessionId.c_str());
	mSocket->write(playMessage.c_str(), playMessage.size());
}

void RtspMediaSource::setupVideoTrack(uint16_t port) {
	setPendingRequest(mCSeq, obtainMessage(SETUP_VIDEO_TRACK));
	String setupMessage = String::format("SETUP %s RTSP/1.0\r\nCSeq: %d\r\nTransport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n",
			mVideoMediaSource.c_str(), mCSeq++, port, port + 1);
	mSocket->write(setupMessage.c_str(), setupMessage.size());
}

void RtspMediaSource::playVideoTrack() {
	setPendingRequest(mCSeq, obtainMessage(PLAY_VIDEO_TRACK));
	String playMessage = String::format("PLAY %s RTSP/1.0\r\nCSeq: %d\r\nRange: npt=0.000-\r\nSession: %s\r\n\r\n",
			mVideoMediaSource.c_str(), mCSeq++, mVideoSessionId.c_str());
	mSocket->write(playMessage.c_str(), playMessage.size());
}

void RtspMediaSource::setPendingRequest(uint32_t id, const sp<Message>& message) {
	AutoLock autoLock(mLock);
	mPendingRequests[id] = message;
}

sp<Message> RtspMediaSource::getPendingRequest(uint32_t id) {
	AutoLock autoLock(mLock);
	return mPendingRequests[id];
}

sp<Message> RtspMediaSource::removePendingRequest(uint32_t id) {
	AutoLock autoLock(mLock);
	sp<Message> message =  mPendingRequests[id];
	mPendingRequests.erase(id);
	return message;
}

RtspMediaSource::NetReceiver::NetReceiver(const sp<RtspMediaSource>& mediaSource) :
		mMediaSource(mediaSource) {
	mSocket = mMediaSource->getSocket();
}

void RtspMediaSource::NetReceiver::run() {
	setSchedulingParams(SCHED_OTHER, -16);

	while (!isInterrupted()) {
		RtspHeader* rtspHeader = mSocket->readPacketHeader();
		if (rtspHeader != NULL) {
			uint32_t seqNum = atoi((*rtspHeader)[String("CSeq").toLowerCase()]);
			sp<Message> reply = mMediaSource->removePendingRequest(seqNum);
			if ((*rtspHeader)[String("ResultCode")] == "200") {
				reply->obj = rtspHeader;

				String strContentLength = (*rtspHeader)[String("Content-Length").toLowerCase()];
				if (strContentLength != NULL) {
					size_t contentLength = atoi(strContentLength.c_str());
					if (contentLength > 0) {
						sp<Buffer> buffer = new Buffer(contentLength);
						mSocket->readFully(buffer->data(), contentLength);
						buffer->setRange(0, contentLength);
						sp<Bundle> bundle = new Bundle();
						bundle->putObject("Content", buffer);
						reply->setData(bundle);
					}
				}

				reply->sendToTarget();
			} else {
				reply->obj = rtspHeader;
				reply->sendToTarget();
			}
		}
	}
}

void RtspMediaSource::NetReceiver::stop() {
	interrupt();
	mSocket->close();
	join();
	mMediaSource = NULL;
}
