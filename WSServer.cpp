#include "WSServer.h"

static void writeFile(unsigned char* buff, int size) {
	FILE* fp = NULL;
	fopen_s(&fp, "./test.hevc", "ab+");
	if (fp != nullptr && buff != nullptr && size != 0) {
		fwrite(buff, size, 1, fp);
	}
	fclose(fp);
}
void WSServer::packetCallback(void* userData, bool isVideoStream, unsigned char* buff, int size, long long pts, long long dts, long long duration) {
	//std::cout << "packetCallback isVideoStream=" << isVideoStream << ",size=" << size << std::endl;
	//todo:此处还需要定义头附加到流数据首部，作为附加信息传递，例如携带流类型，流长度等
	if (userData != nullptr) {
		Userdata* userdata = (Userdata*)userData;

		websocketpp::connection_hdl hdl = userdata->hdl;

		if (isVideoStream) {//暂时只发送视频数据
			//std::cout << "packetCallback isVideoStream=" << isVideoStream << ",size=" << size << std::endl;
			sendBinaryMessage(hdl, buff, size);

			//写文件观察流是否正确,测试用
			//writeFile(buff,size);
		}

	}


}

WSServer::WSServer()
{
	// Set logging settings
	m_endpoint.set_error_channels(websocketpp::log::elevel::all);
	//m_endpoint.set_access_channels(websocketpp::log::alevel::all ^ websocketpp::log::alevel::frame_payload);
	m_endpoint.set_access_channels(websocketpp::log::alevel::frame_header);
	m_endpoint.clear_access_channels(websocketpp::log::alevel::frame_payload);//此项设置是为了避免控制台一直打印二进制数据


	//m_endpoint.set_access_channels(websocketpp::log::alevel::debug_close);
	//m_endpoint.set_error_channels(websocketpp::log::elevel::warn);

	//m_endpoint.set_max_message_size(300000);
	//m_endpoint.set_max_http_body_size(300000);

	// Initialize Asio
	m_endpoint.init_asio();

	// Set the default message handler to the echo handler
	m_endpoint.set_message_handler(std::bind(
		&WSServer::messageHandler, this,
		std::placeholders::_1, std::placeholders::_2
	));

	m_endpoint.set_open_handler(std::bind(
		&WSServer::openHandler, this,
		std::placeholders::_1));

	m_endpoint.set_close_handler(std::bind(
		&WSServer::closeHandler, this,
		std::placeholders::_1));
}


WSServer::~WSServer()
{

	m_endpoint.stop_listening();

	//释放map
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.begin();
	while (l_it != connects.end())
	{
		ConnectObj* obj = l_it->second;
		//释放
		delete obj->takeStream;
		obj->takeStream = nullptr;
		delete obj;
		obj = nullptr;
	}
	connects.clear();


}

bool WSServer::checkProtocol(server::message_ptr msg) {

	cJSON* root = cJSON_Parse(msg->get_payload().c_str());

	cJSON* req = cJSON_GetObjectItem(root, "request");

	if (req == nullptr) {
		cJSON_Delete(root);
		return false;
	}
	/*cJSON*ser = cJSON_GetObjectItem(root, "serial");
	if (ser == nullptr) {
		cJSON_Delete(root);
		return false;
	}
	cJSON_Delete(root);*/

	return true;

}
//发送文本消息
void WSServer::sendTextMessage(websocketpp::connection_hdl hdl, std::string text) {
	try {
		m_endpoint.send(hdl, text, value::TEXT);
	}
	catch (websocketpp::exception const& e) {
		std::cout << "sendTextMessage failed because: "
			<< "(" << e.what() << ")" << std::endl;
	}
}

void WSServer::sendBinaryMessage(websocketpp::connection_hdl hdl, void* binary, int size) {
	try {
		m_endpoint.send(hdl, binary, size, value::BINARY);

	}
	catch (websocketpp::exception const& e) {
		std::cout << "sendTextMessage failed because: "
			<< "(" << e.what() << ")" << std::endl;
	}
}

void WSServer::messageHandler(websocketpp::connection_hdl hdl, server::message_ptr msg) {

	if (msg == nullptr || msg->get_payload().empty()) {
		return;
	}
	std::cout << "on_message called with hdl: " << hdl.lock().get() << " and message: " << msg->get_payload()
		<< std::endl;

	if (msg->get_opcode() != value::TEXT) {//目前只处理文本
		std::cout << "not support opcode " << msg->get_opcode() << ",but only support opcode \"text\"" << std::endl;
		sendTextMessage(hdl, "only support the text opcode");
		return;
	}

	//入队
	Msg* msgInfo = new Msg();
	msgInfo->hdl = hdl;
	msgInfo->msg = msg;
	msgQueue.push(msgInfo);

	//处理消息
	//todo:此处为了防止阻塞，应该先加入队列，在线程中处理队列中的消息，未实现
	//handleTextMessage(hdl, msg);
}

void WSServer::openHandler(websocketpp::connection_hdl hdl) {

	std::lock_guard<std::mutex> lock(m_mutex);//加锁，防止还未插入成功时closeHandler已调用完毕，锁会在lock_guard析构时释放，也就是离开创建lock_guard对象的作用域时会释放锁
	std::cout << "openHandler called with hdl: " << hdl.lock().get()
		<< std::endl;
	//新连接加入map
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.find((long)hdl.lock().get());
	if (l_it == connects.end()) {
		TakeStream* ts = new TakeStream();//自己新建需要自己释放
		ConnectObj* obj = new ConnectObj();
		obj->hdl = hdl;
		obj->takeStream = ts;
		//新的连接则插入
		connects.insert(pair<int, ConnectObj*>((long)hdl.lock().get(), obj));
	}

	std::cout << "now the number of connects is : " << connects.size()
		<< std::endl;

}
void WSServer::closeHandler(websocketpp::connection_hdl hdl) {
	std::lock_guard<std::mutex> lock(m_mutex);//加锁，等待openHandler调用完毕释放锁后才能调用
	std::cout << "closeHandler called with hdl: " << hdl.lock().get()
		<< std::endl;
	//连接断开则从map中移除并释放
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.find((long)hdl.lock().get());
	if (l_it != connects.end()) {
		ConnectObj* obj = l_it->second;
		connects.erase(l_it);

		//释放
		delete obj->takeStream;
		obj->takeStream = nullptr;
		delete obj;
		obj = nullptr;
	}
	std::cout << "now the number of connects is : " << connects.size()
		<< std::endl;
}


void WSServer::msgHandlerLoop() {
	while (true) {
		if (msgQueue.size() > 0) {
			Msg* msg = msgQueue.front();
			msgQueue.pop();//删除队首
			handleTextMessage(msg->hdl, msg->msg);
			delete msg;
		}
		else {
			//休眠一段时间
			std::chrono::milliseconds dura(1);
			std::this_thread::sleep_for(dura);
		}

	}
}

void WSServer::run() {

	//开启消息循环处理线程
	//todo:由于是单线程，一旦某个处理任务阻塞，会导致队列里的其他任务得不到及时处理，因此后期需要考虑多线程处理
	MsgHandlerLoop loop = std::bind(&WSServer::msgHandlerLoop, this);
	std::thread msgHandlerThread(loop);
	msgHandlerThread.detach();

	try {
		// Listen on port 9002
		m_endpoint.listen(9002);
		std::cout << "Server listen on port 9002." << std::endl;

		// Start the server accept loop
		m_endpoint.start_accept();
		std::cout << "Start the server accept loop." << std::endl;

		// Start the ASIO io_service run loop
		std::cout << "Start the ASIO io_service run loop,now you can connect it." << std::endl;
		m_endpoint.run();
		std::cout << "exit" << std::endl;
	}
	catch (websocketpp::exception const& e) {
		std::cout << e.what() << std::endl;
	}
	catch (...) {
		std::cout << "other exception" << std::endl;
	}
}

void WSServer::handleTextMessage(websocketpp::connection_hdl hdl, server::message_ptr msg) {

	ConnectObj* obj = nullptr;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::map<int, ConnectObj*> ::iterator l_it;
		l_it = connects.find((long)hdl.lock().get());
		if (l_it != connects.end()) {
			obj = l_it->second;
		}
		else {//实际上这一步只是为了保险起见，正常是不会进入这个条件的
			TakeStream* ts = new TakeStream();//自己新建需要自己释放
			obj = new ConnectObj();
			obj->hdl = hdl;
			obj->takeStream = ts;
			//新的连接则插入
			connects.insert(pair<int, ConnectObj*>((long)hdl.lock().get(), obj));

		}
	}


	//{"request":"open_url",content:""}
	cJSON* response = cJSON_CreateObject();//响应json
	cJSON* requestRoot = cJSON_Parse(msg->get_payload().c_str());//请求json
	int code = 9999;
	do {
		bool valid = checkProtocol(msg);
		if (!valid) {
			break;
		}

		cJSON* req = cJSON_GetObjectItem(requestRoot, "request");
		cJSON_AddStringToObject(response, "response", req->valuestring);
		cJSON* serial = cJSON_GetObjectItem(requestRoot, "serial");
		if (serial != nullptr) {
			cJSON_AddNumberToObject(response, "serial", serial->valueint);
		}
		else {
			cJSON_AddNumberToObject(response, "serial", 0);
		}
		if (strcmp("open_url", req->valuestring) == 0) {

			cJSON* content = cJSON_GetObjectItem(requestRoot, "content");
			if (content == nullptr) {
				break;
			}
			char* url = content->valuestring;

			//设置回调
			Userdata* userdata = new Userdata();
			userdata->hdl = hdl;
			PacketCallbackFunction fun = std::bind(&WSServer::packetCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
			obj->takeStream->setPacketCallback(fun, userdata);

			//打开url
			code = obj->takeStream->open(url);
			cJSON_AddNumberToObject(response, "code", code);
			if (code == 0) {
				cJSON_AddStringToObject(response, "msg", "success");
				VideoInfo info;
				obj->takeStream->getVideoInfo(info);
				cJSON_AddNumberToObject(response, "codeID", info.codeID);
				cJSON_AddNumberToObject(response, "width", info.width);
				cJSON_AddNumberToObject(response, "height", info.height);
			}
			else {
				cJSON_AddStringToObject(response, "msg", "failed");
			}
			char* out = cJSON_Print(response);
			sendTextMessage(hdl, out);
			free(out);

		}
		else if (strcmp("close_url", req->valuestring) == 0) {
			code = obj->takeStream->close();
			cJSON_AddNumberToObject(response, "code", code);
			cJSON_AddStringToObject(response, "msg", "success");
			char* out = cJSON_Print(response);
			sendTextMessage(hdl, out);
			free(out);


		}
		else if (strcmp("start_take_stream", req->valuestring) == 0) {

			cJSON* sleepTimeMs = cJSON_GetObjectItem(requestRoot, "sleepTimeMs");
			if (sleepTimeMs != nullptr) {
				code = obj->takeStream->startTakeStreamThread(sleepTimeMs->valueint);
			}
			else {
				code = obj->takeStream->startTakeStreamThread();
			}

			cJSON_AddNumberToObject(response, "code", code);
			if (code == 0) {
				cJSON_AddStringToObject(response, "msg", "success");
			}
			else {
				cJSON_AddStringToObject(response, "msg", "failed,may be not open");
			}

			char* out = cJSON_Print(response);
			sendTextMessage(hdl, out);
			free(out);
		}
		else if (strcmp("read_one_frame", req->valuestring) == 0) {
			code = obj->takeStream->readOneFrame();
			cJSON_AddNumberToObject(response, "code", code);
			if (code == 0) {
				cJSON_AddStringToObject(response, "msg", "success");
			}
			else {
				cJSON_AddStringToObject(response, "msg", "failed,may be not open or has using take stream thread");
			}

			char* out = cJSON_Print(response);
			sendTextMessage(hdl, out);
			free(out);
		}


	} while (0);

	if (code == 9999) {
		cJSON_AddNumberToObject(response, "code", -1);
		cJSON_AddStringToObject(response, "msg", "unknown protocol");
		char* out = cJSON_Print(response);
		sendTextMessage(hdl, out);
		free(out);
	}
	cJSON_Delete(response);
	cJSON_Delete(requestRoot);



}



