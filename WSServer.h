#ifndef WELLDONE_WS_SERVER_H
#define WELLDONE_WS_SERVER_H

//�˷���Ϊwebsocket�����ṩȡ�����ܣ�Э�����json��ʽ����ͨ�������ƴ��䣬�������Ϊ��ʾ�Ͳο�
//todo��ע������ʵ��������������Ҫ��������⣬������δʵ��
//todo:ʵ����������Ϊ��Ӧ�Բ������⣬��Ҫ�Լ�ʵ���̳߳�
//todo:�ṩ��ϸ�Ĵ����뼰��Ϣ

//�ݶ�Э��
//{"request":"open_url","content":"rtsp://192.168.39.122/video/265/surfing.265","serial":111}
//{"request":"close_url","serial":111}
//{"request":"start_take_stream","serial":111,"sleepTimeMs":30}
//{"request":"start_take_stream","serial":111}
//{"request":"read_one_frame","serial":111}

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <queue>
#include "TakeStream.h"

extern "C" {
#include "cJSON.h"
}

using namespace std;

//using websocketpp::lib::placeholders::_1;
//using websocketpp::lib::placeholders::_2;
//using websocketpp::lib::bind;
using websocketpp::frame::opcode::value;

typedef websocketpp::server<websocketpp::config::asio> server;

typedef std::function<void()> MsgHandlerLoop;

typedef struct {
	websocketpp::connection_hdl hdl;//���Ӿ��
	TakeStream* takeStream;//ȡ����
} ConnectObj;

typedef struct {
	websocketpp::connection_hdl hdl;//���Ӿ��
} Userdata;

typedef struct {
	websocketpp::connection_hdl hdl;//���Ӿ��
	server::message_ptr msg;//��Ϣ
} Msg;

class WSServer
{
public:
	WSServer();
	~WSServer();
	//��������
	void run();

private:
	std::mutex m_mutex;
	server m_endpoint;//webscoketpp����endpoint

	std::map<int, ConnectObj*> connects;//������������,������Ҫ��һ���̰߳�ȫ��

	std::queue<Msg*> msgQueue;//�����Ϣ

	//�����ı���Ϣ
	void sendTextMessage(websocketpp::connection_hdl hdl, string text);
	//���Ͷ�������
	void sendBinaryMessage(websocketpp::connection_hdl hdl, void* binary, int size);

	//��Ϣ���������������н��յ�����Ϣ
	void messageHandler(websocketpp::connection_hdl hdl, server::message_ptr msg);

	//������֪ͨ
	void openHandler(websocketpp::connection_hdl hdl);

	//���ӶϿ�֪ͨ
	void closeHandler(websocketpp::connection_hdl hdl);

	//�����ı���Ϣ
	void handleTextMessage(websocketpp::connection_hdl hdl, server::message_ptr msg);

	//�ص�����������һ�λص�����֮ǰ���ݾͻᱻ�����������������Ҫ�����������ڣ������п�������
	void packetCallback(void* userData, bool isVideoStream, unsigned char* buff, int size, long long pts, long long dts, long long duration);

	//����Զ���ͨ��Э���Ƿ���Ϲ淶
	bool checkProtocol(server::message_ptr msg);

	//ѭ��������Ϣ
	void msgHandlerLoop();
};

#endif

