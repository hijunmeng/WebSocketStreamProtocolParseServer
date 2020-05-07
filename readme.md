# websocket流协议解析服务
* 本服务采用webscoket协议，利用ffmpeg进行流协议解析，获取裸流数据后通过websocket协议发送出去
* 此服务主要用途是给web提供裸流数据，由web进行解码
* 本服务与web通信采用json协议的方式，裸流数据通过二进制进行传输
