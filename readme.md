# websocket流协议解析服务
* 本服务采用webscoket协议，利用ffmpeg进行流协议解析，获取裸流数据后通过websocket协议发送出去
* 此服务主要用途是给web提供裸流数据，由web进行解码
* 本服务与web通信采用json协议的方式，裸流数据通过二进制进行传输

## 开发环境
* windows 10
* visual studio 2019 社区版

## 说明
* 本服务websocket主要使用[websocketpp](https://docs.websocketpp.org/)此库
* 流协议解析主要是使用ffmpeg4.X
* 使用c++智能指针shared_ptr进行多线程对象内存管理
* 本服务处理方式为收到消息后直接加入队列后返回，在另一线程中对队列的消息进行读取并处理

## 通信json协议
### 打开url 
* 客户端请求
```json
{
"request":"open_url",   //请求命令
"serial":111 ,          //请求序列号，客户端指定
"content":"rtsp://192.168.39.122/video/265/surfing.265", //播放路径

}
```
* 服务回应
```json
{
"response":"open_url",  //请求命令，与客户端发送的一致
"serial":111,           //请求序列号，与客户端发送的一致
"code":0,               //返回码，0表示成功
"msg":"success",        //返回信息提示
"codeID":27,            //编码id,与ffmpeg枚举类型对应
"width":1920,           //视频宽度
"height":1080,          //视频高度
}
```
### 关闭url 
* 客户端请求
```json
{
"request":"close_url",   //请求命令
"serial":111             //请求序列号，客户端指定

}
```
* 服务回应
```json
{
"response":"close_url",  //请求命令，与客户端发送的一致
"serial":111,            //请求序列号，与客户端发送的一致
"code":0,               //返回码，0表示成功
"msg":"success"         //返回信息提示
}
```
### 请求裸流数据（连续） 
* 客户端请求
```json
{
"request":"start_take_stream",   //请求命令
"serial":111 ,              //请求序列号，客户端指定
"sleepTimeMs":  30          //可选，流数据发送间隔
}
```
* 服务回应
```json
{
"response":"start_take_stream",  //请求命令，与客户端发送的一致
"serial":111,            //请求序列号，与客户端发送的一致
"code":0,               //返回码，0表示成功
"msg":"success"         //返回信息提示
}
```
### 请求一帧裸流数据 
* 客户端请求
```json
{
"request":"read_one_frame",   //请求命令
"serial":111               //请求序列号，客户端指定
}
```
* 服务回应
```json
{
"response":"read_one_frame",  //请求命令，与客户端发送的一致
"serial":111,            //请求序列号，与客户端发送的一致
"code":0,               //返回码，0表示成功
"msg":"success"         //返回信息提示
}
```
### 裸流数据回调（二进制）
* 裸流数据会以二进制形式发送给客户端
* 二进制数据除了包含一帧裸流外，还应该携带其他信息，如时间戳等，但此处未制定二进制协议，因此暂时并无额外信息

## 以后工作
* 完善返回码及错误信息
* 完善json通信协议以及裸流二进制通信协议
* 消息处理加入线程池，每条消息从线程池取出一个线程处理



