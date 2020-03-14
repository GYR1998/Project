//实现Server类，完成服务端的整体结构及流程


#ifndef _MY_SRV_H_
#define _MY_SRV_H_

#include"tcpsocket.hpp"
#include"http.hpp"
#include"epollwait.hpp"
#include"threadpool.hpp"
#include<boost/filesystem.hpp>
#include<fstream>

#define WWW_ROOT "./www"



class Server
{
    public:

      //首先搭建一个TCP服务器
      bool Start(int port)
      {
          bool ret;
          ret=_lst_sock.SocketInit(port);
          if(ret==false)
          {
              return false;
          }



          /*
          ret=_epoll.Init();  //对于epoll的初始化
          if(ret==false)
          {
              return false;
          }
          */

          
          ret=_pool.PoolInit();

          if(ret==false)
          {
              return false;
          }

        
          // _epoll.Add(_lst_sock);//将_lst_sock添加进去开始监控
         
      
          //获取新连接
          while(1)
          {  
              
              TcpSocket cli_sock;
              ret=_lst_sock.Accept(cli_sock);  //获取到了已完成连接
              if(ret==false)     //如果失败的话就要重新获取
              {
                  continue;
              }

              //接受到了新连接,然后就要将其抛到线程池中，然线程池中的线程来执行它。但是抛到线程池后，如果客户端一
              //直不发送消息数，那么线程就会一直被空占，所以用一个要对其进行事件总线的监控，所以加入epoll
              cli_sock.SetNonBlock();        //设置非阻塞
              ThreadTask tt(cli_sock.GetFd(),ThreadHandler);   //直接抛到线程池
              _pool.TaskPush(tt);

            
              /*
              vector<TcpSocket> list;       
              ret=_epoll.Wait(list);
              if(ret==false)
              {     
                continue;
              }
              for(int i=0;i<list.size();i++)
              {
                  if(list[i].GetFd()==_lst_sock.GetFd())
                  {
                      TcpSocket cli_sock;
                      ret=_lst_sock.Accept(cli_sock);
                      if(ret==false)
                      {
                        continue;
                      }
                      cli_sock.SetNonBlock();
                      _epoll.Add(cli_sock);
                  }
                  else
                  {
                      ThreadTask tt(list[i].GetFd(),ThreadHandler); 
                      _pool.TaskPush(tt);
                      _epoll.Del(list[i]);
                  }
              }
              */
            
          }
          _lst_sock.Close();
          return true;
      }

      static void ThreadHandler(int sockfd)
      {
         TcpSocket sock;
         sock.SetFd(sockfd);
      
        
        

          //从clisock接受请求数据进行解析
          HttpRequest req;
          HttpResponse rsp;
          int status=req.RequestParse(sock);
          if(status!=200)
          {
              //解析失败则直接响应错误
              rsp._status=status;
              rsp.ErrorProcess(sock);
              sock.Close();
              return;
          }

          cout<<"=================================="<<endl;
          //客户端没错误才能走下来，所以之后的错误一定是服务端的错误,所以此时就不能用status来限制
          //因为可能有多个状态码
          
          //根据请求，进行处理，将结果填充到rsp中
          HttpProcess(req,rsp);   //处理的结果都在rsp中


          //将处理结果响应给客户端
          rsp.NormalProcess(sock);

          //当前采用短连接，直接处理完毕后关闭套接字
          sock.Close();
          return;
      }

   
      static bool HttpProcess(HttpRequest &req,HttpResponse &rsp)
      {
          //若请求是一个post请求,则多进程CGI处理
          //若请求是一个GET请求，但是查询字符串不为空，
          //若请求是一个GET，且查询字符串为空
          //    若请求是一个目录
          //    若请求是一个文件
       

          string realpath=WWW_ROOT+req._path;
          if(!boost::filesystem::exists(realpath))      //判断文件是否存在
          {
              rsp._status=404;
              return false;
          }


          cerr<<"realpath:    "<<realpath<<endl;
          cerr<<"method:    "<<req._method<<endl;
          if((req._method=="GET" && req._param.size()!=0) || req._method=="POST")
          {
              //这是一文件上传请求
              CGIProcess(req,rsp);
          }
          else 
          {
              //这是一个文件下载/目录列表请求
              
              if(boost::filesystem::is_directory(realpath))
              {
                  //查看目录列表请求
                  ListShow(realpath,rsp._body);
                  rsp.SetHeader("Content-Type","text/html");    //请求的响应信息,传递头信息
                  
              }

              else 
              {
               
                  auto it=req._headers.find("Range");
                  if(it==req._headers.end())
                  {
                      
                      //文件下载请求
                      Download(realpath,rsp._body);
                      rsp.SetHeader("Content-Type","application/octet-stream");      //这个Content-Type很重要，他决定文件究竟是下载还是展示
                      
                      rsp.SetHeader("Accept-Ranges","bytes");
                      rsp.SetHeader("ETag","abcdefg");

                  }
                  else 
                  {
                      //断点续传
                      
                      string range=it->second;
                      RangeDownload(realpath,range,rsp._body);  //断点续传的状态码为206
                      rsp.SetHeader("Content-Type","application/octet-stream");
                      string unit="bytes=";
                      size_t pos=range.find(unit);
                      if(pos==string::npos)
                      {
                          return false;
                      }
                      int64_t len=boost::filesystem::file_size(realpath);
                      stringstream tmp;
                      tmp<<"bytes ";
                      tmp<<range.substr(pos+unit.size());
                      tmp<<"/";
                      tmp<<len;
                      rsp.SetHeader("Content-Range",tmp.str());
                      rsp._status=206;
                      return true;
                  }

                  
              }
          }


/*
          rsp._status=200;
          rsp._body="<html>hello</html>"; 
          rsp.SetHeader("Content-Type","text/html");
  */        
          rsp._status=200;
          return true;
      }

      static bool RangeDownload(string &path,string &range,string &body)
      {
          //Range: bytes=staet-end；
          
          string unit="bytes=";
          size_t pos=range.find(unit);
          if(pos==string::npos)
          {
              return false;
          }
          pos+=unit.size();
          size_t pos2=range.find("-",pos);
          if(pos2==string::npos)
          {
              return false;
          }

          string start=range.substr(pos,pos2-pos);
          string end=range.substr(pos2+1);
          stringstream tmp;
          int64_t dig_start,dig_end;
          tmp<<start;
          tmp>>dig_start;
          tmp.clear();
          if(end.size()==0)
          {
              dig_end=boost::filesystem::file_size(path)-1;
          }
          else 
          {
              tmp<<end;
              tmp>>dig_end;
          }

          int64_t len=dig_end-dig_start+1;
          body.resize(len);
          ifstream file(path);
          if(!file.is_open())
          {
              return false;
          }
          file.seekg(dig_start,ios::beg);
          file.read(&body[0],len);
          if(!file.good())
          {
              cerr<<"read error"<<endl;
              return false;
          }

          file.close();
          return true;
      }
      //父进程通过fork（）函数创建一个子进程，子进程将和父进程运行相同的代码，但创建子进程的大多情况，是希望能够运行一些其他的程序，这时候就需要用到进程程序替换。
      //CGIProcess这是父进程所需要做的事情，剩下的就是在子进程中做了，也就是外部处理程序upload
      static bool CGIProcess(HttpRequest &req,HttpResponse &rsp)
      {

          //因为子进程我们需要程序替换，所以父进程原来的数据都会消失，所以我们要把数据都传给子进程
          //http的头部信息通过环境变量传递给子进程,环境变量需要在子进程中设置
          //数据正文是通过管道传递的
          //管道是一个半双工通信，所以需要建立两个管道，一个父进程用来传递数据，另一个子进程用来将处理结果返回给父进程
          
          
          //定义两个管道
          int pipe_in[2],pipe_out[2];      //父进程从pipe_in中读数据，向pipe_out中输出数据
          if(pipe(pipe_in)<0 || pipe(pipe_out)<0)
          {
              cerr<<"creat pipe error"<<endl;
              return false;
          }
          
          int pid=fork();
          if(pid<0)
          {
              return false;
          }
          else 
          {
              if(pid==0)
              {
                  close(pipe_in[0]);   //子进程先写，父进程读数据，子进程只写不读，所以关闭pipe_in[0],即关闭读端

                  close(pipe_out[1]);   //子进程只读，所以把写关闭,所以关闭写端
                  
                  //一旦程序替换，两个pipe也会被初始化，所以要重定向。
                  dup2(pipe_out[0],0);       //将标准输入重定向的管道的读端
                  dup2(pipe_in[1],1);        //将标准输出重定向到管道的写端，只要标准输出打印啥，就相当于向管道中写啥

                  //直接在子进程中设置环境变量
                  setenv("METHOD",req._method.c_str(),1);
                  setenv("PATH",req._path.c_str(),1);
                  for(auto i:req._headers)
                  {
                      setenv(i.first.c_str(),i.second.c_str(),1);
                  }
                  string realpath=WWW_ROOT+req._path;
                  execl(realpath.c_str(),realpath.c_str(),NULL);
                  exit(0);
              }
              
              close(pipe_in[1]);
              close(pipe_out[0]);
              write(pipe_out[1],&req._body[0],req._body.size());      //写正文,存在风险，如果管道写满了就无法再写了
              
              //子进程返回结果
              while(1)
              {
                  char buf[1024]={0};
                  int ret=read(pipe_in[0],buf,1024);
                  if(ret==0)        //表示没有人向管道中写入数据
                  {
                      break;
                  }
                  buf[ret]='\0';
                  rsp._body+=buf;

              }

              close(pipe_in[0]);
              close(pipe_out[1]);
              return true;
          }

          
      }

      static bool Download(string &path,string &body)
      {
          int64_t fsize=boost::filesystem::file_size(path);
          body.resize(fsize);
          ifstream file(path);
          if(!file.is_open())
          {
              cerr<<"open file error"<<endl;
              return false;
          }
          file.read(&body[0],fsize);
          if(!file.good())
          {
              cerr<<"read file data error"<<endl;
              return false;
          }
          file.close();
          return true;
      }


      static bool ListShow(string &path,string &body)
      {

          //   ./www/textdir/a.txt      ---------->      /textdir/a.txt
          
          string www=WWW_ROOT;
          
          size_t pos =path.find(www);       //wwww往后都是请求的路径
          if(pos==string ::npos)   //路径不合法
          {
              return false;
          }

          string req_path=path.substr(www.size());      //    /textdir/a.txt
          



          stringstream tmp;



          tmp<<"<html><head><style>";
          tmp<<"*{margin : 0;}";
          tmp<<" .main-window {height : 100%;width : 80%;margin : 0 auto;}";
          tmp<<".upload {position : relative;height : 20%;width : 100%;background-color : #33c0b9;text-align:center;}";
          tmp<<".listshow {position : relative;height : 80%;width : 100%;background : #6fcad6;}";
          tmp<<" </style></head>";
          tmp<<"<body><div class='main-window'>";
          tmp<<"<div class='upload'>";

          //from之间的input数据都是提交给服务器的，分别有
          tmp<<"<form action='/upload'  method='POST' enctype='multipart/form-data'>";     //form中的upload就是我们所请求的一个
                                                              //功能（处理文件上传），所以这个请求的名称应该和实现这个功能的外部程序的名称一致
          tmp<<"<div class='upload-bth'>";
          tmp<<"<input type='file' name='fileupload'>";
          tmp<<"<input type='submit' name='submit'>";
          tmp<<"</div></from>";
          // //// 
          
          
          tmp<<"</div><hr />";
          tmp<<"<div class='listshow'><ol>";

          //组织每一个文件的信息结点
          boost::filesystem::directory_iterator begin(path); //begin传入的是一个文件的路径
          boost::filesystem::directory_iterator end;         //end是一个空节点，所以啥都不用传
          for(;begin!=end;++begin)
          {
            
              

              int64_t ssize,mtime;
              string pathname=begin->path().string();

              //首先可以获取文件名称,注意：这个name不仅有这个文件的名称,还有这个文件的路径
              string name=begin->path().filename().string();
              
              string uri=req_path+name;    


              if(boost::filesystem::is_directory(pathname))
              {
                  //目录显示
                  
                  
                  tmp<<"<li><strong><a href='";      // <a href='/textdir/a.txt'>
                  tmp<<uri<<"'>";

                  tmp<<name<<"/";
                  tmp<<"</a><br /></strong>";
                  tmp<<"<small>filetype:directory";
                  tmp<<"</small></li>";
              }
              else 
              {
                   
                  //普通文件显示
                  //
                  

                  ssize=boost::filesystem::file_size(pathname);
               
                  //最后一次修改时间
                  mtime=boost::filesystem::last_write_time(pathname);
             
              
                  tmp<<"<li><strong><a href='";
                  tmp<<uri<<"'>";
                  tmp<<name;
                  tmp<<"</a><br/></strong>";
                  tmp<<"<small><modified:";
                  tmp<<mtime;
                  tmp<<"br/>filetype: application-ostream";
                  tmp<<ssize/1024<<"kbytes";
                  tmp<<" 16kbytes";
                  tmp<<"</small></li>";
              }
              
          }
          
          //组织每一个文件的信息结点
          

          //从<li> 到</li> 是每一个结点的信息
         
          




          tmp<<"</ol></div><hr /></div></body></html>";
          body=tmp.str();
      
          
          return true;
      }

    private:
      TcpSocket _lst_sock;         
      ThreadPool _pool;             //线程池
          Epoll _epoll;             //多路转接模型

};

#endif
