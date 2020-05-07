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
	//todo:�˴�����Ҫ����ͷ���ӵ��������ײ�����Ϊ������Ϣ���ݣ�����Я�������ͣ������ȵ�
	if (userData != nullptr) {
		Userdata* userdata = (Userdata*)userData;

		websocketpp::connection_hdl hdl = userdata->hdl;

		if (isVideoStream) {//��ʱֻ������Ƶ����
			//std::cout << "packetCallback isVideoStream=" << isVideoStream << ",size=" << size << std::endl;
			sendBinaryMessage(hdl, buff, size);

			//д�ļ��۲����Ƿ���ȷ,������
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
	m_endpoint.clear_access_channels(websocketpp::log::alevel::frame_payload);//����������Ϊ�˱������̨һֱ��ӡ����������


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

	//�ͷ�map
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.begin();
	while (l_it != connects.end())
	{
		ConnectObj* obj = l_it->second;
		//�ͷ�
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
//�����ı���Ϣ
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

	if (msg->get_opcode() != value::TEXT) {//Ŀǰֻ�����ı�
		std::cout << "not support opcode " << msg->get_opcode() << ",but only support opcode \"text\"" << std::endl;
		sendTextMessage(hdl, "only support the text opcode");
		return;
	}

	//���
	Msg* msgInfo = new Msg();
	msgInfo->hdl = hdl;
	msgInfo->msg = msg;
	msgQueue.push(msgInfo);

	//������Ϣ
	//todo:�˴�Ϊ�˷�ֹ������Ӧ���ȼ�����У����߳��д�������е���Ϣ��δʵ��
	//handleTextMessage(hdl, msg);
}

void WSServer::openHandler(websocketpp::connection_hdl hdl) {

	std::lock_guard<std::mutex> lock(m_mutex);//��������ֹ��δ����ɹ�ʱcloseHandler�ѵ�����ϣ�������lock_guard����ʱ�ͷţ�Ҳ�����뿪����lock_guard�����������ʱ���ͷ���
	std::cout << "openHandler called with hdl: " << hdl.lock().get()
		<< std::endl;
	//�����Ӽ���map
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.find((long)hdl.lock().get());
	if (l_it == connects.end()) {
		TakeStream* ts = new TakeStream();//�Լ��½���Ҫ�Լ��ͷ�
		ConnectObj* obj = new ConnectObj();
		obj->hdl = hdl;
		obj->takeStream = ts;
		//�µ����������
		connects.insert(pair<int, ConnectObj*>((long)hdl.lock().get(), obj));
	}

	std::cout << "now the number of connects is : " << connects.size()
		<< std::endl;

}
void WSServer::closeHandler(websocketpp::connection_hdl hdl) {
	std::lock_guard<std::mutex> lock(m_mutex);//�������ȴ�openHandler��������ͷ�������ܵ���
	std::cout << "closeHandler called with hdl: " << hdl.lock().get()
		<< std::endl;
	//���ӶϿ����map���Ƴ����ͷ�
	std::map<int, ConnectObj*> ::iterator l_it;
	l_it = connects.find((long)hdl.lock().get());
	if (l_it != connects.end()) {
		ConnectObj* obj = l_it->second;
		connects.erase(l_it);

		//�ͷ�
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
			msgQueue.pop();//ɾ������
			handleTextMessage(msg->hdl, msg->msg);
			delete msg;
		}
		else {
			//����һ��ʱ��
			std::chrono::milliseconds dura(1);
			std::this_thread::sleep_for(dura);
		}

	}
}

void WSServer::run() {

	//������Ϣѭ�������߳�
	//todo:�����ǵ��̣߳�һ��ĳ�����������������ᵼ�¶��������������ò�����ʱ������˺�����Ҫ���Ƕ��̴߳���
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
		else {//ʵ������һ��ֻ��Ϊ�˱�������������ǲ���������������
			TakeStream* ts = new TakeStream();//�Լ��½���Ҫ�Լ��ͷ�
			obj = new ConnectObj();
			obj->hdl = hdl;
			obj->takeStream = ts;
			//�µ����������
			connects.insert(pair<int, ConnectObj*>((long)hdl.lock().get(), obj));

		}
	}


	//{"request":"open_url",content:""}
	cJSON* response = cJSON_CreateObject();//��Ӧjson
	cJSON* requestRoot = cJSON_Parse(msg->get_payload().c_str());//����json
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

			//���ûص�
			Userdata* userdata = new Userdata();
			userdata->hdl = hdl;
			PacketCallbackFunction fun = std::bind(&WSServer::packetCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7);
			obj->takeStream->setPacketCallback(fun, userdata);

			//��url
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



