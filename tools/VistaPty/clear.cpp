#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdlib.h>
int wmain(int argc,wchar_t **argv){if(argc!=2)return 2;FreeConsole();if(!AttachConsole(wcstoul(argv[1],NULL,10)))return 3;HANDLE out=CreateFileW(L"CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);CONSOLE_SCREEN_BUFFER_INFO info;DWORD n;COORD zero={0,0};BOOL ok=GetConsoleScreenBufferInfo(out,&info);if(ok){DWORD len=(DWORD)info.dwSize.X*info.dwSize.Y;ok=FillConsoleOutputCharacterW(out,L' ',len,zero,&n)&&FillConsoleOutputAttribute(out,info.wAttributes,len,zero,&n)&&SetConsoleCursorPosition(out,zero);}CloseHandle(out);FreeConsole();return ok?0:4;}
