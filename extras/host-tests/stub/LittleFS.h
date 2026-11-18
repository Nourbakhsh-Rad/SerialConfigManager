#pragma once
#include "Arduino.h"
#include <map>
struct File{ std::string* d=nullptr; std::string name; bool w=false; std::string tmp;
 operator bool()const{return d||w;} size_t print(const String&s){ if(w){*d=s.s;} return s.length();} void close(){}
 String readString(){return String(*d);} };
struct FakeFS{ std::map<std::string,std::string> files; bool ok=true;
 bool begin(bool f=false){return ok;} bool exists(const char*p){return files.count(p);}
 File open(const char*p,const char*m){File f; if(m[0]=='w'){f.d=&files[p];f.w=true;} else if(files.count(p)){f.d=&files[p];} return f;}
 bool format(){return true;} bool remove(const char*p){return files.erase(p);} };
extern FakeFS LittleFS;
