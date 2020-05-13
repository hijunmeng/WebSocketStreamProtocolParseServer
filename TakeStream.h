#ifndef WELLDONE_TAKE_STREAM_H
#define WELLDONE_TAKE_STREAM_H

//todo:��ȡ����ֻʵ������Ƶ�Ĵ�������Ƶ�ݲ�֧��
//todo:ȡ���ٶȹ���ᵼ�½��뷴Ӧ����������������Խ��Խ��


extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
}
#include <thread>
#include <functional>
#include <mutex>

#define TAKE_SLEEP_TIME_MS 10   //������ʱ�ļ��ʱ��
#define OPEN_TIMEOUT_S 6 //�򿪳�ʱʱ�� ��λ��

//�����ص�
typedef std::function<void(void*, bool, unsigned char*, int, long long, long long, long long)> PacketCallbackFunction;
typedef void(*PacketCallback)(void* userData, bool isVideoStream, unsigned char* buff, int size, long long pts, long long dts, long long duration);

typedef std::function<int()> TakeStreamLoop;

//���ڼ�¼ʱ��
typedef struct {
	time_t lastTime;
} Runner;

typedef struct {
	int width = 0;
	int height = 0;
	int codeID = -1;//�������ͣ�h264/hevc��
	double timebase = 0;//��λ�룬����ʱ������㣬����pts*timebase
} VideoInfo;

typedef struct {
	int sampleRate = 0;//������
	int channels = 0;//ͨ����
	int codeID = -1;//��������
	unsigned long long channelLayout = 0;//ͨ�����֣�����
} AudioInfo;

class TakeStream
{
public:

	TakeStream();

	~TakeStream();

	//֧�����ֻص���ʽ����ѡ��һ����
	int setPacketCallback(PacketCallback  packetCallback, void* userData);
	int setPacketCallback(PacketCallbackFunction packetCallback, void* userData);

	//��ȡ����
	//[in]url:ȡ����ַ����rtsp��ַ
	int open(char* url);

	//�����Ƶ��������һ���У�
	//[out]videoInfo:�����Ƶ����
	int getVideoInfo(VideoInfo& videoInfo);

	//�����Ƶ��������һ���У�
	//[out]audioInfo:�����Ƶ����
	int getAudioInfo(AudioInfo& audioInfo);

	//��ʼȡ���̣߳�֡����ͨ���ص��ӿڻص�
	//������ô˽ӿ������ٵ���readOneFrame
	//takeSleepTimeMs:��ȡһ֡���ݺ������ʱ�䣬�ݴ˿��ƶ�ȡ����
	int startTakeStreamThread(int takeSleepTimeMs = TAKE_SLEEP_TIME_MS);

	//�������ж�ȡһ֡���ݣ�֡����ͨ���ص��ӿڻص�
	//�������ʹ�ô˽ӿ���Ҫ�ٵ���startTakeStreamThread
	//�û������ʵ���ʱ�����ô˽ӿڶ�ȡ���ݣ������û��Լ����ƶ�ȡ�ٶ�
	int readOneFrame();


	//�ر�ȡ����
	int close();

private:
	std::mutex m_mutex;

	int takeSleepTimeMs;

	//�ص������ߵ��Զ�������ָ��
	void* packetCallbackUserData;

	//֧�����ֻص�
	PacketCallback packetCallback;
	PacketCallbackFunction packetCallbackFunction;


	volatile bool running;//ȡ���߳��Ƿ�����
	volatile bool isOpen;//�Ƿ��
	volatile bool hasAudio;

	AVCodecParameters* pVideoCodecParameters;
	AVCodecParameters* pAudioCodecParameters;

	AVFormatContext* pFormatContext;

	AVPacket* packet;//�������

	int videoStreamIdx; //��Ƶ���±�
	int audioStreamIdx; //��Ƶ���±�


	//ffmpeg��־
	void log(int level, const char* fmt, ...);

	//ѭ��ȡ��
	int takeLoop();

};

#endif

