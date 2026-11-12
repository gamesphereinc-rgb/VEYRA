#include <windows.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::uint16_t Read16(const std::vector<std::uint8_t>& data,std::size_t at){
  return static_cast<std::uint16_t>(data[at])|
         static_cast<std::uint16_t>(data[at+1]<<8);
}
std::uint32_t Read32(const std::vector<std::uint8_t>& data,std::size_t at){
  return static_cast<std::uint32_t>(data[at])|
         (static_cast<std::uint32_t>(data[at+1])<<8)|
         (static_cast<std::uint32_t>(data[at+2])<<16)|
         (static_cast<std::uint32_t>(data[at+3])<<24);
}
std::wstring ErrorText(DWORD error){
  wchar_t* buffer=nullptr;
  const DWORD length=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|
    FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,
    MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
    reinterpret_cast<LPWSTR>(&buffer),0,nullptr);
  std::wstring result=length&&buffer?std::wstring(buffer,length):L"Unknown error";
  if(buffer)LocalFree(buffer);
  return result;
}
bool Fail(const std::wstring& message){
  std::wcerr<<L"VEYRA icon patcher: "<<message<<L"\n";
  return false;
}
bool Patch(const std::wstring& executable,const std::wstring& icon){
  std::ifstream input(icon,std::ios::binary);
  if(!input)return Fail(L"Cannot open icon: "+icon);
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
  if(bytes.size()<6||Read16(bytes,0)!=0||Read16(bytes,2)!=1)
    return Fail(L"Invalid ICO header.");
  const WORD count=Read16(bytes,4);
  if(count==0||count>64)return Fail(L"Invalid ICO entry count.");
  const std::size_t header=6,entrySize=16;
  if(header+static_cast<std::size_t>(count)*entrySize>bytes.size())
    return Fail(L"Truncated ICO directory.");

  std::vector<std::uint8_t> group(6+static_cast<std::size_t>(count)*14,0);
  group[2]=1;
  group[4]=static_cast<std::uint8_t>(count&255);
  group[5]=static_cast<std::uint8_t>((count>>8)&255);
  for(WORD i=0;i<count;++i){
    const std::size_t src=header+static_cast<std::size_t>(i)*entrySize;
    const DWORD dataSize=Read32(bytes,src+8);
    const DWORD dataOffset=Read32(bytes,src+12);
    if(dataSize==0||static_cast<std::uint64_t>(dataOffset)+dataSize>bytes.size())
      return Fail(L"ICO image data points outside the file.");
    const std::size_t dst=6+static_cast<std::size_t>(i)*14;
    for(int j=0;j<8;++j)group[dst+j]=bytes[src+j];
    for(int j=0;j<4;++j)group[dst+8+j]=bytes[src+8+j];
    const WORD resourceId=static_cast<WORD>(i+1);
    group[dst+12]=static_cast<std::uint8_t>(resourceId&255);
    group[dst+13]=static_cast<std::uint8_t>((resourceId>>8)&255);
  }

  HANDLE update=BeginUpdateResourceW(executable.c_str(),FALSE);
  if(!update)return Fail(L"BeginUpdateResourceW failed: "+ErrorText(GetLastError()));
  bool ok=true;
  const WORD language=MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL);
  for(WORD i=0;i<count&&ok;++i){
    const std::size_t src=header+static_cast<std::size_t>(i)*entrySize;
    const DWORD dataSize=Read32(bytes,src+8);
    const DWORD dataOffset=Read32(bytes,src+12);
    ok=UpdateResourceW(update,RT_ICON,MAKEINTRESOURCEW(i+1),language,
                       bytes.data()+dataOffset,dataSize)!=FALSE;
  }
  if(ok){
    ok=UpdateResourceW(update,RT_GROUP_ICON,MAKEINTRESOURCEW(1),language,
                       group.data(),static_cast<DWORD>(group.size()))!=FALSE;
  }
  const DWORD error=GetLastError();
  if(!EndUpdateResourceW(update,ok?FALSE:TRUE))
    return Fail(L"EndUpdateResourceW failed: "+ErrorText(GetLastError()));
  if(!ok)return Fail(L"Could not write icon resources: "+ErrorText(error));
  std::wcout<<L"Applied "<<count<<L" icon sizes to "<<executable<<L"\n";
  return true;
}
}
int wmain(int argc,wchar_t** argv){
  if(argc!=3){
    std::wcerr<<L"Usage: VEYRAIconPatcher.exe <target.exe> <icon.ico>\n";
    return 2;
  }
  return Patch(argv[1],argv[2])?0:1;
}
