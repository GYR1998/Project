#include <iostream>
#include<string>
#include<stdio.h>
#include<fstream>
#include<unistd.h>
#include<stdlib.h>
#include<vector>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>
#include<sstream>
using namespace std;


#define WWW_ROOT "./www/"

class Boundary
{
    public:
      int64_t _start_addr;
      int64_t _data_len;
      string _name;
      string _filename;
};


bool GetHeader(const string &key,string &val)
{
    string body;
    char* ptr=getenv(key.c_str());
    if(ptr==NULL)
    {
        return false;
    }
    val=ptr;
    return true;
}

bool HeaderParse(string &header,Boundary &file)
{
    vector<string> list;
    boost::split(list,header,boost::is_any_of("\r\n"),boost::token_compress_on);
    for(int i=0;i<list.size();i++)
    {
        string seq=": ";
        size_t pos=list[i].find(seq);
        if(pos==string::npos)
        {
            return false;
        }
        string key=list[i].substr(0,pos);
        string val=list[i].substr(pos+seq.size());
        if(key!="Content-Disposition")
        {
            continue;
        }

        string name_filed="fileupload";
        string filename_seq="filename=\"";
        pos=val.find(name_filed);
        if(pos==string::npos)
        {
            continue;
        }
        pos=val.find(filename_seq);
        if(pos==string::npos)
        {
            return false;
        }
        pos+=filename_seq.size();
        size_t next_pos=val.find("\"",pos);
        if(next_pos==string::npos)
        {
            return false;
        }
        file._filename=val.substr(pos,next_pos-pos);
        file._name="fileupload";
    }
    
    return true;

}


bool BoundaryParse(string &body,vector<Boundary> &list)
{
    string cont_b="boundary=";
    string tmp;
    if(GetHeader("Content-Type",tmp)==false)
    {
        return false;
    }

    size_t pos,next_pos;
    pos=tmp.find(cont_b);
    if(pos==string::npos)
    {
        return false;
    }

    string dash="--";
    string craf="\r\n";
    string tail="\r\n\r\n";
    string boundary=tmp.substr(pos+cont_b.size());
    string f_boundary=dash+boundary+craf;
    string m_boundary=craf+dash+boundary;

    pos=body.find(f_boundary);
    if(pos!=0)        //如果first boundary位置不是起始位置
    {
        cerr<<"first boundary error"<<endl;
        return false;
    }

    pos+=f_boundary.size();           //指向第一块头部起始位置 
    //midelle boundary 可能有多个所以需要循环处理
    while(pos<body.size())
    {
        next_pos=body.find(tail,pos);          //找寻头部结尾
        if(next_pos==string::npos)
        {
            return false;
        }
        
        string header=body.substr(pos,next_pos-pos);
        pos=next_pos+tail.size();               //数据的起始地址
        next_pos=body.find(m_boundary,pos);        //找\r\n--boundary 
        if(next_pos==string::npos)
        {
            return false;
        }
        int64_t offset=pos;             
        int64_t length=next_pos-pos;          //下一个boundary的位置减去数据的起始地址
        next_pos=pos+m_boundary.size();
        pos=body.find(craf,next_pos);
        if(pos==string::npos)
        {
            return false;
        }
       //pos指向这个下一个 midele boundary 的头部地址
       //若没有midele boundary了，则指向数据的结尾pos=body.size();
       
        pos+=craf.size();
        Boundary file;
        file._data_len=length;
        file._start_addr=offset;
        //解析头部
        HeaderParse(header,file);
        list.push_back(file);
    }  
    return true;  
}


bool StorageFile(string &body,vector<Boundary> &list)
{
    for(int i=0;i<list.size();i++)
    {
        if(list[i]._name!="fileupload")
        {
            continue;
        }
        string realpath=WWW_ROOT+list[i]._filename;
        ofstream file(realpath);
        if(!file.is_open())
        {
            cerr<<"open file"<<realpath<<"failed"<<endl;
            return false;
        }
        file.write(&body[list[i]._start_addr],list[i]._data_len);
        if(!file.good())
        {
            cerr<<"write file error"<<endl;
            return false;
        }

        file.close();
    }

    return true;
}





int main(int argc,char *argv[],char *env[])
{
    string err="<html>Failed</html>";
    string suc="<html>Sucess</html>";
    char* cont_len=getenv("Content-Length");
    string body;
    if(cont_len!=NULL)
    {
        stringstream tmp;
        tmp<<cont_len;
        int64_t fsize;
        tmp>>fsize;

        body.resize(fsize);
        int rlen=0;
        while(rlen<fsize)
        {
            int ret=read(0,&body[0]+rlen,fsize-rlen);
            if(ret<=0)
            {
                exit(-1);
            }
            rlen+=ret;
        }
        vector<Boundary> list;
        bool ret;
        ret=BoundaryParse(body,list);
        if(ret==false)
        {
            cerr<<"Boundary error";
            cout<<err;
            return -1;
        }
        ret=StorageFile(body,list);
        
        if(ret==false)
        {
            cerr<<"StorageFile  error";
            cout<<err;
            return -1;
        }

        cout<<suc;
        return 0;
    }
    cout<<err;
    return 0;
}
