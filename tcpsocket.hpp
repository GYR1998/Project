
#ifndef _M_SOCK_H_
#define _M_SOCK_H_ 
#include<iostream>
#include<string>
#include<stdio.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>
using namespace std;
class TcpSocket
{
    public:
      TcpSocket():_sockfd(-1){}
      
      int GetFd()
      {
          return _sockfd;
      }

      void SetFd(int fd)
      {
          _sockfd=fd;
      }

      void SetNonBlock()
      {
          int flag=fcntl(_sockfd,F_GETFL,0);
          fcntl(_sockfd,F_SETFL,flag | O_NONBLOCK);
      }

      bool SocketInit(int port)
      {
          _sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
          if(_sockfd<0)
          {
              cerr<<"socket error"<<endl;
              return false;
          }
//////////////////////////////////////////////////////////////////////////
          //作为一个服务器，如果服务器重启，我们并不希望因为端口被占用而导致启动不起来，所有要设置套接字选项SO_REUSEADDR，这叫地址复用
          int opt=1;
          setsockopt(_sockfd,SOL_SOCKET,SO_REUSEADDR,(void*)&opt,sizeof(int));          //设置套接字选项，开启地址重用
          /////////////////////////////////////////
          struct sockaddr_in addr;
          addr.sin_addr.s_addr=INADDR_ANY;
          addr.sin_port=htons(port);
          addr.sin_family=AF_INET;
          socklen_t len=sizeof(addr);
          int ret=bind(_sockfd,(struct sockaddr*)&addr,len);
          if(ret<0)
          {
              cerr<<"bind error"<<endl;
              close(_sockfd);
              return false;
          }

          ret=listen(_sockfd,10);        
          if(ret<0)
          {
              cerr<<"listen error"<<endl;
              close(_sockfd);
              return false;
          }

          return true;
      }

      bool Accept(TcpSocket &sock)
      {
          int fd;
          struct sockaddr_in addr;
          socklen_t len=sizeof(addr);
          fd=accept(_sockfd,(struct sockaddr*)&addr,&len);
          if(fd<0)
          {
              cerr<<"accept error"<<endl;
              return false;
          }
          sock.SetFd(fd);
          sock.SetNonBlock();
          return true;
      }



      //读取数据
      bool RecvPeek(string &buf)
      {
          buf.clear();
          char tmp[8192]={0};   //用于接受头部,这就是为啥要限制头部的大小
          int ret=recv(_sockfd,tmp,4096,MSG_PEEK);       //接受数据,返回值是接受数据的大小
          if(ret<=0)
          {
              if(errno==EAGAIN)
              {
                  return true;
              }
              cerr<<"recv error"<<endl;
              return false;
          }

          buf.assign(tmp,ret);        //从tmp的头开始，将ret个字符赋值给buf
          return true;

      }


      //获取指定长度的数据
      bool Recv(string &buf,int len)
      {
          buf.resize(len);
          int rlen=0,ret;        //rlen是已经接受的长度，len是应该接受的数据


          //为啥要用wehile？
          //假如当前接受的数据长度ret，没有len长度或者缓冲区中的数据长度没有len长度，但是我们要求接受len长度的数据，所以要用whlie
          while(rlen<len)
          {
              ret=recv(_sockfd,&buf[0]+rlen,len-rlen,0);
              if(ret<=0)
              {
                  if(errno==EAGAIN)
                  {
                      usleep(1000);
                      continue;
                  }

                  return false;
              }
              rlen+=ret;
          }
          return true;
      }

      bool Send(const string &buf)
      {
          int64_t slen=0;
          while(slen<buf.size())
          {   
              int ret=send(_sockfd,&buf[slen],buf.size()-slen,0);
              if(ret<0)
              {
                  if(errno==EAGAIN)
                  {
                      usleep(1000);
                      continue;
                  }
                  cerr<<"send error"<<endl;
                  return false;
              }
              slen+=ret;
          }
          return true;
      }
      bool Close()
      {
          close(_sockfd);
          return true;
      }
    private:
      int _sockfd;
};




#endif





