#include "TakeStream.h"

//ffmpeg�ڲ���־�Ļص���������ڿ����׶ηǳ�����
static void ffmpegLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
	static int printPrefix = 1;
	static int count = 0;
	static char prev[1024] = { 0 };
	char line[1024] = { 0 };
	static int is_atty;
	AVClass* avc = ptr ? *(AVClass**)ptr : NULL;
	if (level > AV_LOG_DEBUG) {
		return;
	}

	line[0] = 0;

	if (printPrefix && avc) {
		if (avc->parent_log_context_offset) {
			AVClass** parent = *(AVClass***)(((uint8_t*)ptr) + avc->parent_log_context_offset);
			if (parent && *parent) {
				snprintf(line, sizeof(line), "[%s @ %p] ", (*parent)->item_name(parent), parent);
			}
		}
		snprintf(line + strlen(line), sizeof(line) - strlen(line), "[%s @ %p] ", avc->item_name(ptr), ptr);
	}

	vsnprintf(line + strlen(line), sizeof(line) - strlen(line), fmt, vl);
	line[strlen(line) + 1] = 0;

	printf("%s\n", line);
}

static int interruptCallback(void* opaque) {
	Runner* r = (Runner*)opaque;
	if (r->lastTime > 0) {
		if (time(NULL) - r->lastTime > OPEN_TIMEOUT_S) {
			// �ȴ�����8s���ж�
			return 1;//����1��ʾ�ж�
		}
	}

	return 0;
}

TakeStream::TakeStream()
{

	avformat_network_init();
	packet = av_packet_alloc();
	running = false;
	isOpen = false;
	hasAudio = false;
	this->packetCallback = NULL;
	this->packetCallbackFunction = NULL;

	takeSleepTimeMs = TAKE_SLEEP_TIME_MS;

}
TakeStream::~TakeStream()
{
	close();
	std::lock_guard<std::mutex> lock(m_mutex);//�������Ҫ��Ϊ������ֱ��ȡ���߳��˳�

	if (packet != NULL) {
		av_packet_free(&packet);
		//packet = NULL;
		log(AV_LOG_INFO, "packet released.");
	}
	if (pFormatContext != NULL) {
		avformat_close_input(&pFormatContext);//�ڲ������avformat_free_context
		pFormatContext = NULL;
		log(AV_LOG_INFO, "format context released.");
	}
	log(AV_LOG_INFO, "TakeStream released.");


}


void TakeStream::log(int level, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	av_vlog(NULL, level, fmt, ap);
	printf("\n");
	va_end(ap);
}


int TakeStream::setPacketCallback(PacketCallback  packetCallback, void* userData)
{
	//av_log_set_callback(ffmpegLogCallback);
	this->packetCallback = packetCallback;
	this->packetCallbackUserData = userData;
	return 0;
}

int TakeStream::setPacketCallback(PacketCallbackFunction packetCallback, void* userData)
{
	//av_log_set_callback(ffmpegLogCallback);
	this->packetCallbackFunction = packetCallback;
	this->packetCallbackUserData = userData;
	return 0;
}

int TakeStream::getVideoInfo(VideoInfo& videoInfo) {
	if (!isOpen) {
		log(AV_LOG_INFO, "has not open");
		return -1;
	}
	videoInfo.width = pVideoCodecParameters->width;
	videoInfo.height = pVideoCodecParameters->height;
	videoInfo.codeID = pVideoCodecParameters->codec_id;
	AVStream* st = pFormatContext->streams[videoStreamIdx];
	videoInfo.timebase = av_q2d(st->time_base);
	return 0;
}

int TakeStream::getAudioInfo(AudioInfo& audioInfo) {
	if (!hasAudio) {
		log(AV_LOG_INFO, "no audio");
		return -1;
	}
	audioInfo.channelLayout = pAudioCodecParameters->channel_layout;
	audioInfo.channels = pAudioCodecParameters->channels;
	audioInfo.codeID = pAudioCodecParameters->codec_id;
	audioInfo.sampleRate = pAudioCodecParameters->sample_rate;
	return 0;
}

int TakeStream::open(char* url)
{
	if (url == nullptr) {
		return -1;
	}
	if (isOpen) {
		log(AV_LOG_INFO, "has open ,do not open again");
		return -1;
	}

	int ret = 0;

	log(AV_LOG_INFO, "avformat_alloc_context.");
	if (pFormatContext != NULL) {
		avformat_close_input(&pFormatContext);//�ڲ������avformat_free_context
		pFormatContext = NULL;
	}
	pFormatContext = avformat_alloc_context();
	//�������ó�ʱ������ᵼ�³�ʱ�䲻����
	Runner input_runner = { 0 };
	pFormatContext->interrupt_callback.callback = interruptCallback;
	pFormatContext->interrupt_callback.opaque = &input_runner;
	//���ò���ʱ���Ա���avformat_find_stream_info��ʱ����
	pFormatContext->probesize = 100 * 1024;//Ĭ����5000000
	pFormatContext->max_analyze_duration = 2 * AV_TIME_BASE;//2��,Ĭ����5��

	log(AV_LOG_INFO, "avformat_open_input...");
	AVDictionary* options = NULL;
	av_dict_set(&options, "buffer_size", "1048576", 0);//524288=512KB  1048576=1MB
	//av_dict_set(&options, "pkt_size", "524288", 0);//524288=512KB  1048576=1MB

	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	av_dict_set(&options, "stimeout", "5000000", 0); //���ó�ʱ�Ͽ�����ʱ�䣬��λ΢��,������Է�ֹtcpģʽ��av_read_frameһֱ�������˳�
	//av_dict_set(&options, "rtsp_flags", "prefer_tcp", 0);//���ȳ���tcp
	av_dict_set(&options, "bbb", "1048576", 0);//������

	input_runner.lastTime = time(NULL);
	ret = avformat_open_input(&pFormatContext, url, NULL, &options);
	if (ret != 0) {
		char err_info[32] = { 0 };
		av_strerror(ret, err_info, 32);
		log(AV_LOG_ERROR, "avformat_open_input failed %d %s.", ret, err_info);
		return -1;
	}
	log(AV_LOG_INFO, "avformat_open_input success.");
	AVDictionaryEntry* entry = NULL;
	while (entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX)) {
		log(AV_LOG_INFO, "Option %s not recognized by the demuxer,and the count is %d \n", entry->key, av_dict_count(options));
	}


	av_dict_free(&options);



	ret = avformat_find_stream_info(pFormatContext, NULL);
	if (ret < 0) {
		log(AV_LOG_ERROR, "av_find_stream_info failed %d.", ret);
		return -1;
	}
	log(AV_LOG_INFO, "avformat_find_stream_info success.");


	ret = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (ret < 0) {
		log(AV_LOG_ERROR, "Could not find %s stream.", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
		return -1;
	}
	videoStreamIdx = ret;

	AVStream* st = pFormatContext->streams[videoStreamIdx];
	pVideoCodecParameters = st->codecpar;
	log(AV_LOG_INFO, "VideoCodecParameters:videoCodecID=%d,width=%d,height=%d.",
		pVideoCodecParameters->codec_id,
		pVideoCodecParameters->width,
		pVideoCodecParameters->height);


	ret = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (ret < 0) {
		log(AV_LOG_ERROR, "Could not find %s stream.", av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
		hasAudio = false;
	}
	else {
		hasAudio = true;
		audioStreamIdx = ret;
		AVStream* st = pFormatContext->streams[audioStreamIdx];
		pAudioCodecParameters = st->codecpar;
		log(AV_LOG_INFO, "AudioCodecParameters:audioCodecID=%d,sample_rate=%d,channels=%d,channel_layout=%d.",
			pAudioCodecParameters->codec_id,
			pAudioCodecParameters->sample_rate,
			pAudioCodecParameters->channels,
			pAudioCodecParameters->channel_layout

		);
	}




	isOpen = true;
	running = false;

	return 0;
}



int TakeStream::close()
{
	running = false;
	isOpen = false;
	hasAudio = false;
	return 0;
}

int TakeStream::startTakeStreamThread(int takeSleepTimeMs) {
	if (!isOpen) {
		log(AV_LOG_INFO, "has not open,please open it first");
		return -1;
	}
	if (running) {
		log(AV_LOG_INFO, "has start take stream thread,do not try again");
		return -1;
	}
	if (takeSleepTimeMs >= 0) {
		this->takeSleepTimeMs = takeSleepTimeMs;
	}
	else {
		log(AV_LOG_WARNING, "takeSleepTimeMs must be greater than or equal to 0");
	}

	//����ȡ���߳�
	TakeStreamLoop loop = std::bind(&TakeStream::takeLoop, this);
	std::thread take(loop);
	take.detach();
	running = true;
	return 0;
}
static void writeFile(unsigned char* buff, int size) {
	FILE* fp = NULL;
	fopen_s(&fp, "./test.hevc", "ab+");
	if (fp != nullptr && buff != nullptr && size != 0) {
		fwrite(buff, size, 1, fp);
	}
	fclose(fp);
}
//��Ҫ�������߳���
int TakeStream::takeLoop() {
	std::lock_guard<std::mutex> lock(m_mutex);//�������Ҫ��Ϊ�˱�֤����ǰ��ǰ�߳��Ѿ��˳�
	log(AV_LOG_INFO, "enter take Stream Loop");
	av_init_packet(packet);
	packet->data = NULL;
	packet->size = 0;

	while (running) {
		av_packet_unref(packet);//����������һ���ͷ�֮ǰռ�õ��ڴ�
		int ret = av_read_frame(pFormatContext, packet);
		if (ret == AVERROR_EOF) {
			log(AV_LOG_ERROR, "av_read_frame end of file.");
			running = false;
			break;
		}
		else if (ret < 0) {
			log(AV_LOG_ERROR, "av_read_frame faile or end of file.");
			continue;
		}
		bool isVideoStream = packet->stream_index == videoStreamIdx;
		bool isKeyFrame = packet->flags & AV_PKT_FLAG_KEY;//�ж��Ƿ�ؼ�֡
		//log(AV_LOG_INFO, "av_read_frame packet size=%d,stream_index=%d,isKeyFrame=%d.\n", packet->size, packet->stream_index, isKeyFrame);


		/*if (isVideoStream) {//������
			log(AV_LOG_INFO, "av_read_frame packet size=%d,stream_index=%d", packet->size, packet->stream_index);
			writeFile(packet->data, packet->size);
		}*/
		//�������ص���ȥ
		if (packetCallback) {
			packetCallback(packetCallbackUserData, isVideoStream, packet->data, packet->size, packet->pts, packet->dts, packet->duration);
		}
		if (packetCallbackFunction) {
			packetCallbackFunction(packetCallbackUserData, isVideoStream, packet->data, packet->size, packet->pts, packet->dts, packet->duration);
		}
		//�������һ��ʱ��,���ƶ�ȡ�ٶ�
		std::chrono::milliseconds dura(takeSleepTimeMs);
		std::this_thread::sleep_for(dura);
	}
	log(AV_LOG_INFO, "take stream Loop has exit");
	//ȡ���߳��˳����ر�������
	if (pFormatContext != NULL) {
		avformat_close_input(&pFormatContext);
		pFormatContext = NULL;
	}
	close();
	return 0;
}

int TakeStream::readOneFrame() {
	if (!isOpen) {
		log(AV_LOG_INFO, "has not open,please open it first");
		return -1;
	}
	if (running) {
		log(AV_LOG_INFO, "has start take stream thread,can not call this function");
		return -1;
	}
	av_init_packet(packet);
	packet->data = NULL;
	packet->size = 0;
	av_packet_unref(packet);//����������һ���ͷ�֮ǰռ�õ��ڴ�
	int ret = av_read_frame(pFormatContext, packet);
	if (ret != 0) {
		log(AV_LOG_ERROR, "av_read_frame faile");
		return ret;
	}

	bool isVideoStream = packet->stream_index == videoStreamIdx;
	bool isKeyFrame = packet->flags & AV_PKT_FLAG_KEY;

	//�������ص���ȥ
	if (packetCallback) {
		packetCallback(packetCallbackUserData, isVideoStream, packet->data, packet->size, packet->pts, packet->dts, packet->duration);
	}
	if (packetCallbackFunction) {
		packetCallbackFunction(packetCallbackUserData, isVideoStream, packet->data, packet->size, packet->pts, packet->dts, packet->duration);
	}

	return 0;
}

