#ifndef WELLDONE_WS_SERVER_H
#define WELLDONE_WS_SERVER_H

//此服务为websocket服务，提供取流功能，协议采用json格式，流通过二进制传输，代码仅作为演示和参考
//todo的注释是在实际生产环境必须要解决的问题，这里暂未实现
//todo:实际生产环境为了应对并发问题，需要自己实现线程池
//todo:提供详细的错误码及信息

//暂定协议
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
	websocketpp::connection_hdl hdl;//连接句柄
	TakeStream* takeStream;//取流器
} ConnectObj;

typedef struct {
	websocketpp::connection_hdl hdl;//连接句柄
} Userdata;

typedef struct {
	websocketpp::connection_hdl hdl;//连接句柄
	server::message_ptr msg;//消息
} Msg;

class WSServer
{
public:
	WSServer();
	~WSServer();
	//启动监听
	void run();

private:
	std::mutex m_mutex;
	server m_endpoint;//webscoketpp服务endpoint

	std::map<int, ConnectObj*> connects;//管理所有连接,可能需要找一个线程安全的

	std::queue<Msg*> msgQueue;//存放消息

	//发送文本信息
	void sendTextMessage(websocketpp::connection_hdl hdl, string text);
	//发送二进制流
	void sendBinaryMessage(websocketpp::connection_hdl hdl, void* binary, int size);

	//消息处理器，处理所有接收到的消息
	void messageHandler(websocketpp::connection_hdl hdl, server::message_ptr msg);

	//新连接通知
	void openHandler(websocketpp::connection_hdl hdl);

	//连接断开通知
	void closeHandler(websocketpp::connection_hdl hdl);

	//处理文本消息
	void handleTextMessage(websocketpp::connection_hdl hdl, server::message_ptr msg);

	//回调函数，在下一次回调函数之前数据就会被清除，因此数据如果需要更长生命周期，请自行拷贝保存
	void packetCallback(void* userData, bool isVideoStream, unsigned char* buff, int size, long long pts, long long dts, long long duration);

	//检测自定义通信协议是否符合规范
	bool checkProtocol(server::message_ptr msg);

	//循环处理消息
	void msgHandlerLoop();
};

#endif

