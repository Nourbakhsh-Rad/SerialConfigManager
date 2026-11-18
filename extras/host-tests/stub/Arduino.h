#pragma once
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <map>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define SERIAL_5N1 1
#define SERIAL_6N1 2
#define SERIAL_7N1 3
#define SERIAL_8N1 4
#define SERIAL_5N2 5
#define SERIAL_6N2 6
#define SERIAL_7N2 7
#define SERIAL_8N2 8
#define SERIAL_5O1 9
#define SERIAL_6O1 10
#define SERIAL_7O1 11
#define SERIAL_8O1 12
#define SERIAL_5O2 13
#define SERIAL_6O2 14
#define SERIAL_7O2 15
#define SERIAL_8O2 16
#define SERIAL_5E1 17
#define SERIAL_6E1 18
#define SERIAL_7E1 19
#define SERIAL_8E1 20
#define SERIAL_5E2 21
#define SERIAL_6E2 22
#define SERIAL_7E2 23
#define SERIAL_8E2 24
extern int pinModes[64], pinLevels[64];
inline void pinMode(int p,int m){pinModes[p]=m;}
inline void digitalWrite(int p,int v){pinLevels[p]=v;}
inline void delayMicroseconds(unsigned){}
inline void delay(unsigned long){}
inline unsigned long millis(){static unsigned long t=0;return t+=1;}
class String{ public: std::string s;
 String(){} String(const char*c):s(c?c:""){} String(const std::string&x):s(x){}
 String(int v):s(std::to_string(v)){} String(unsigned v):s(std::to_string(v)){} String(long v):s(std::to_string(v)){} String(unsigned long v):s(std::to_string(v)){}
 String(unsigned char v):s(std::to_string((int)v)){} String(signed char v):s(std::to_string((int)v)){}
 unsigned int length()const{return s.size();} void reserve(unsigned){} const char*c_str()const{return s.c_str();}
 int indexOf(char c)const{auto p=s.find(c);return p==std::string::npos?-1:(int)p;}
 int indexOf(const String&o)const{auto p=s.find(o.s);return p==std::string::npos?-1:(int)p;}
 int indexOf(char c,int from)const{auto p=s.find(c,from);return p==std::string::npos?-1:(int)p;}
 String substring(int a,int b)const{return String(s.substr(a,b-a));}
 void trim(){size_t a=s.find_first_not_of(" \t\r\n");size_t b=s.find_last_not_of(" \t\r\n");s=(a==std::string::npos)?"":s.substr(a,b-a+1);}
 void toLowerCase(){for(auto&c:s)c=tolower(c);} long toInt()const{return atol(s.c_str());}
 char operator[](int i)const{return s[i];}
 String& operator+=(const String&o){s+=o.s;return *this;} String& operator+=(const char*o){s+=o;return *this;}
 bool operator==(const char*o)const{return s==o;} bool operator==(const String&o)const{return s==o.s;}
 friend String operator+(String a,const String&b){a.s+=b.s;return a;}
 friend String operator+(String a,const char*b){a.s+=b;return a;}
 friend String operator+(const char*a,const String&b){return String(std::string(a)+b.s);}
};
struct Stream{ std::string out; virtual ~Stream(){}
 void print(const char*c){out+=c;} void print(const String&c){out+=c.s;} void print(long v){out+=std::to_string(v);} void print(unsigned long v){out+=std::to_string(v);} void print(int v){out+=std::to_string(v);} void print(unsigned v){out+=std::to_string(v);}
 void println(const char*c){out+=c;out+="\n";} void println(const String&c){out+=c.s+"\n";} void println(int v){print(v);out+="\n";} void println(unsigned v){print(v);out+="\n";} void println(unsigned long v){print(v);out+="\n";} void println(){out+="\n";} };
struct HardwareSerial: Stream{ bool began=false; unsigned long baud=0; uint32_t cfg=0; int rx=-99,tx=-99; std::string wr;
 void end(){began=false;}
 void begin(unsigned long b,uint32_t c=SERIAL_8N1){began=true;baud=b;cfg=c;rx=tx=-1;}
 void begin(unsigned long b,uint32_t c,int r,int t){began=true;baud=b;cfg=c;rx=r;tx=t;}
 int availableForWrite(){return 64;} size_t write(const uint8_t*d,size_t n){wr.append((const char*)d,n);return n;} void flush(){} };
extern HardwareSerial Serial, Serial1;
