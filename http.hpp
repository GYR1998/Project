#ifndef _M_HTTP_H_
#define _M_HTTP_H_ 

#include<vector>
#include <sstream>
#include<string>
#include <unordered_map>
#include"tcpsocket.hpp"
#include <boost/algorithm/string.hpp>
using namespace std;
class HttpRequest
{
    public:
      string _method;       //请求方法
      string _path;          //路径信息
      unordered_map<string,string> _param;            //参数信息，也就是查询字符串，一个个键值对
      unordered_map<string,string> _headers;            //头部
      string _body;        //正文


    private:


      bool RecvHeader(TcpSocket &sock,string &header)
      {
          while(1)
          {
              
               //1.探测性接受大量数据
               string tmp;
               if(sock.RecvPeek(tmp)==false)
               {
                   return false;
               }
               //2.判断是否包含整个头部
               size_t pos;
               pos=tmp.find("\r\n\r\n",0);      //第一个参数为为要找的元素，第二个参数为从哪里开始找。
                                               //返回值为目标字符的位置，当没有找到目标字符时返回npos。\
                                               //如果找的是字符串返回的就是第一个找到的字符串的第一个字母的序号
            
               //3.判断当前接受的数据长度
               if(pos==string::npos && tmp.size()==8192)        //如果pos等于npos，那么没有找到。不等于就找找到
               {
                   return false;
               }
               else if(pos!=string::npos)
               {
                   //4.若包含整个头部，则直接获取头部。
                   
                   header.assign(&tmp[0],pos);     //准确来说此时的header包括头部和首行
                   size_t header_length=pos+4;            
                   sock.Recv(tmp,header_length);          
                   return true;
               }
          }

      }


      //解析整个首行
      bool FirstLineParse(string &line)
      {
          vector<string> line_list;
          boost::split(line_list,line,boost::is_any_of(" "),boost::token_compress_on);
          if(line_list.size()!=3)
          {
              cerr<<"parse first line error"<<endl;
              return false;
          }

          _method =line_list[0];    //其中存入了请求方法

          size_t pos=line_list[1].find("?",0);   //在？之前就是资源路径,之后就是查询字符串
          if(pos==string::npos)   
          {
              _path=line_list[1];
          }
          else 
          {
              _path=line_list[1].substr(0,pos);    //substr():
                                                   //返回值：一个子字符串，从其指定的位置开始。
                                                   //第一次参数：截取开始的位置，从0开始算
                                                   //第二个参数：要截取的长度
                                                   
                                                   
              string query_string;
              query_string=line_list[1].substr(pos+1);    //这里面放的是？之后的所有数据
                                                          //如果substr中只有一个参数，那么就是说，从这个参数开始一直复制到结尾


              vector<string> param_list;
              boost::split(param_list,query_string,boost::is_any_of("&"),boost::token_compress_on);
              for(auto i:param_list)
              {
                  size_t param_pos=-1;
                  param_pos=i.find("=");
                  if(param_pos==string::npos)
                  {
                      cerr<<"parse param error"<<endl;
                      return false;
                  }
                  string key=i.substr(0,param_pos);
                  string val=i.substr(param_pos+1);
                  _param[key]=val;
              }
          }
          return true;
      }
      bool PathIslegal();
      
    public:
      int RequestParse(TcpSocket &sock)         //请求解析
      {
          //1.接受http头部
          string header; 
          if(RecvHeader(sock,header)==false)
          {
              return 400;
          }

          //2.对整个头部进行按照\r\n进行分割，得到一个list
          vector<string> header_list;
          boost::split(header_list,header,boost::is_any_of("\r\n"),boost::token_compress_on);
          //该函数是将header字符串中的数据以"\r\n"为切割符，每切割一次就将切割的内容放入vector<string> 类型的数组中
          
          
          //打印
          /*
          for(auto i=0;i<header_list.size();i++)
          {
              cout<<"list[i]=["<<header_list[i]<<"]"<<endl;
          }
          */

          //3.list[0]--首行，进行首行解析
          
          if(FirstLineParse(header_list[0])==false)    //数组的第一个元素就是首行
          {
              return 400;
          }

          //4.头部分割校验  key:val\r\n
          
          size_t pos=0;
          for(int i=1;i<header_list.size();i++)
          {
              pos=header_list[i].find(": ");
              if(pos==string::npos)
              {
                  cerr<<"heaeder parse error"<<endl;
                  return false;
              }
              
              string key=header_list[i].substr(0,pos);
              string val=header_list[i].substr(pos+2);
              _headers[key]=val;
          }
          //5.请求信息校验
         
          
         
          //为了保证正确，所以我们各个部分打印出来看一下
          

          /*
          cout<<"method:["<<_method<<"]"<<endl;
          cout<<"path:["<<_path<<"]"<<endl;
          for(auto i:_param)
          {
              cout<<i.first<<"="<<i.second<<endl;
          }
          for(auto i:_headers)
          {
              cout<<i.first<<"="<<i.second<<endl;
          }
          */
          

          // 6.接受正文
          auto it=_headers.find("Content-Length");

          if(it!=_headers.end())   //找到了Content-Length,于是我们接受头部
          {

              //将一个string类型的数字转换为一个int64_t类型的数字
              stringstream tmp;      
              tmp<< it->second;
              int64_t file_len;
              tmp>>file_len;
              
              //开始接受
              sock.Recv(_body,file_len);

          }

          return 200;
      }    
};

class HttpResponse 
{
  public:
    int _status;
    string _body;   //回复的正文
    unordered_map<string,string> _headers;         //头信息

  private:
    string GetDesc()
    {
        switch(_status)
        {
            case 400:return "Bad Request";
            case 404:return "Not Found";
            case 206:return "Partial Content";
            case 200:return "OK";
        }
        return "Unkonw";
    }
  public:

    bool SetHeader(const string  &key,const string &val)
    {
        _headers[key]=val;
        return true;
    }
    bool ErrorProcess(TcpSocket &sock)
    {
        return true;
    }
    bool NormalProcess(TcpSocket &sock)
    {
        stringstream tmp;

        tmp<<"HTTP/1.1"<<" "<<_status<<" "<<GetDesc();
        tmp<<"\r\n";

        if(_headers.find("Content-Length")==_headers.end())
        {
            string len=to_string(_body.size());
            _headers["Content-Length"]=len;
        }
        for(auto i:_headers)
        {
            tmp<<i.first<<": "<<i.second<<"\r\n";
        }

        tmp<<"\r\n";
        sock.Send(tmp.str());
        sock.Send(_body);
    
       

        return true;
    }
};

#endif
