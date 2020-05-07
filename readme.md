# websocket流协议解析服务
* 本服务采用webscoket协议，利用ffmpeg进行流协议解析，获取裸流数据后通过websocket协议发送出去
* 此服务主要用途是给web提供裸流数据，由web进行解码
* 本服务与web通信采用json协议的方式，裸流数据通过二进制进行传输

## 说明
* 本服务websocket主要使用[websocketpp](https://docs.websocketpp.org/)此库
* 流协议解析主要是使用ffmpeg4.X

## 开发环境
* windows 10
* visual studio 2019 社区版
