#ifndef WELLDONE_TAKE_STREAM_H
#define WELLDONE_TAKE_STREAM_H

//todo:此取流器只实现了视频的处理，对音频暂不支持
//todo:取流速度过快会导致解码反应不过来，导致数据越堆越多


extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
}
#include <thread>
#include <functional>
#include <mutex>

#define TAKE_SLEEP_TIME_MS 10   //读数据时的间隔时间
#define OPEN_TIMEOUT_S 6 //打开超时时间 单位秒

//裸流回调
typedef std::function<void(void*, bool, unsigned char*, int, long long, long long, long long)> PacketCallbackFunction;
typedef void(*PacketCallback)(void* userData, bool isVideoStream, unsigned char* buff, int size, long long pts, long long dts, long long duration);

typedef std::function<int()> TakeStreamLoop;

//用于记录时间
typedef struct {
	time_t lastTime;
} Runner;

typedef struct {
	int width = 0;
	int height = 0;
	int codeID = -1;//编码类型（h264/hevc）
	double timebase = 0;//单位秒，用于时间戳计算，比如pts*timebase
} VideoInfo;

typedef struct {
	int sampleRate = 0;//采样率
	int channels = 0;//通道数
	int codeID = -1;//编码类型
	unsigned long long channelLayout = 0;//通道布局，用于
} AudioInfo;

class TakeStream
{
public:

	TakeStream();

	~TakeStream();

	//支持两种回调方式，任选其一即可
	int setPacketCallback(PacketCallback  packetCallback, void* userData);
	int setPacketCallback(PacketCallbackFunction packetCallback, void* userData);

	//打开取流器
	//[in]url:取流地址，如rtsp地址
	int open(char* url);

	//获得视频参数（不一定有）
	//[out]videoInfo:存放视频参数
	int getVideoInfo(VideoInfo& videoInfo);

	//获得音频参数（不一定有）
	//[out]audioInfo:存放音频参数
	int getAudioInfo(AudioInfo& audioInfo);

	//开始取流线程，帧数据通过回调接口回调
	//如果调用此接口则不能再调用readOneFrame
	//takeSleepTimeMs:读取一帧数据后的休眠时间，据此控制读取数据
	int startTakeStreamThread(int takeSleepTimeMs = TAKE_SLEEP_TIME_MS);

	//从网络中读取一帧数据，帧数据通过回调接口回调
	//如果决定使用此接口则不要再调用startTakeStreamThread
	//用户可在适当的时机调用此接口读取数据，方便用户自己控制读取速度
	int readOneFrame();


	//关闭取流器
	int close();

private:
	std::mutex m_mutex;

	int takeSleepTimeMs;

	//回调调用者的自定义数据指针
	void* packetCallbackUserData;

	//支持两种回调
	PacketCallback packetCallback;
	PacketCallbackFunction packetCallbackFunction;


	volatile bool running;//取流线程是否在跑
	volatile bool isOpen;//是否打开
	volatile bool hasAudio;

	AVCodecParameters* pVideoCodecParameters;
	AVCodecParameters* pAudioCodecParameters;

	AVFormatContext* pFormatContext;

	AVPacket* packet;//待解码包

	int videoStreamIdx; //视频流下标
	int audioStreamIdx; //音频流下标


	//ffmpeg日志
	void log(int level, const char* fmt, ...);

	//循环取流
	int takeLoop();

};

#endif

