#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sspi.h>
#include <schannel.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

#ifndef SP_PROT_TLS1_3_CLIENT
#define SP_PROT_TLS1_3_CLIENT 0x00002000
#endif

static int send_all(SOCKET socket, const char *data, int size)
{
    while (size > 0) {
        int sent = send(socket, data, size, 0);
        if (sent <= 0) return 0;
        data += sent;
        size -= sent;
    }
    return 1;
}

static int exchange_http(FILE *log, SOCKET socket, CredHandle *credential,
    CtxtHandle *context, char *input, int used, DWORD flags)
{
    static const char request[] = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
    static const char expected[] = "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    SecPkgContext_StreamSizes sizes;
    char record[65536], response[4096];
    int iteration, receivedCb = 0;
    SECURITY_STATUS status;
    SecBuffer buffers[4];
    SecBufferDesc desc = {SECBUFFER_VERSION, 4, buffers};
    ULONG attributes;
    TimeStamp expiry;

    status = QueryContextAttributesW(context, SECPKG_ATTR_STREAM_SIZES, &sizes);
    fprintf(log, "StreamSizes=%08lx\n", (ULONG)status);
    if (status != SEC_E_OK || sizeof(request)-1 > sizes.cbMaximumMessage ||
        sizes.cbHeader + sizeof(request)-1 + sizes.cbTrailer > sizeof(record)) return 0;
    ZeroMemory(buffers,sizeof(buffers));
    buffers[0].BufferType=SECBUFFER_STREAM_HEADER;
    buffers[0].cbBuffer=sizes.cbHeader;
    buffers[0].pvBuffer=record;
    buffers[1].BufferType=SECBUFFER_DATA;
    buffers[1].cbBuffer=sizeof(request)-1;
    buffers[1].pvBuffer=record+sizes.cbHeader;
    memcpy(buffers[1].pvBuffer,request,sizeof(request)-1);
    buffers[2].BufferType=SECBUFFER_STREAM_TRAILER;
    buffers[2].cbBuffer=sizes.cbTrailer;
    buffers[2].pvBuffer=record+sizes.cbHeader+sizeof(request)-1;
    status=EncryptMessage(context,0,&desc,0);
    fprintf(log,"EncryptMessage=%08lx\n",(ULONG)status);
    if(status!=SEC_E_OK) return 0;
    for(iteration=0;iteration<3;++iteration) {
        if(!send_all(socket,(char*)buffers[iteration].pvBuffer,buffers[iteration].cbBuffer)) return 0;
    }
    for(iteration=0;iteration<128 && receivedCb<(int)sizeof(expected)-1;++iteration) {
        int index, extra=0;
        if(used==0) {
            used=recv(socket,input,65536,0);
            if(used<=0) return 0;
        }
        ZeroMemory(buffers,sizeof(buffers));
        buffers[0].BufferType=SECBUFFER_DATA;
        buffers[0].cbBuffer=used;
        buffers[0].pvBuffer=input;
        status=DecryptMessage(context,&desc,0,NULL);
        fprintf(log,"DecryptMessage=%08lx input=%d\n",(ULONG)status,used);
        if(status==SEC_E_INCOMPLETE_MESSAGE) {
            int count=recv(socket,input+used,65536-used,0);
            if(count<=0) return 0;
            used+=count;
            continue;
        }
        if(status!=SEC_E_OK) return 0;
        for(index=0;index<4;++index) {
            if(buffers[index].BufferType==SECBUFFER_DATA) {
                if(buffers[index].cbBuffer > sizeof(response)-receivedCb) return 0;
                memcpy(response+receivedCb,buffers[index].pvBuffer,buffers[index].cbBuffer);
                receivedCb+=buffers[index].cbBuffer;
            } else if(buffers[index].BufferType==SECBUFFER_EXTRA) {
                extra=buffers[index].cbBuffer;
            }
        }
        if(extra>used) return 0;
        memmove(input,input+used-extra,extra);
        used=extra;
    }
    if(receivedCb!=sizeof(expected)-1 || memcmp(response,expected,receivedCb)) return 0;
    fprintf(log,"HTTP response decrypted and verified\n");
    {
        ULONG token=SCHANNEL_SHUTDOWN;
        SecBuffer control={sizeof(token),SECBUFFER_TOKEN,&token};
        SecBufferDesc controlDesc={SECBUFFER_VERSION,1,&control};
        SecBuffer output={0,SECBUFFER_TOKEN,NULL};
        SecBufferDesc outputDesc={SECBUFFER_VERSION,1,&output};
        status=ApplyControlToken(context,&controlDesc);
        fprintf(log,"ApplyControlToken=%08lx\n",(ULONG)status);
        if(status!=SEC_E_OK) return 0;
        status=InitializeSecurityContextW(credential,context,L"localhost",flags,0,
            SECURITY_NATIVE_DREP,NULL,0,context,&outputDesc,&attributes,&expiry);
        fprintf(log,"ShutdownISC=%08lx token=%lu\n",(ULONG)status,output.cbBuffer);
        if(status!=SEC_E_OK) return 0;
        if(output.cbBuffer && output.pvBuffer) {
            int sent=send_all(socket,(char*)output.pvBuffer,output.cbBuffer);
            FreeContextBuffer(output.pvBuffer);
            if(!sent) return 0;
        }
        /* The server validates our close_notify before closing its socket. */
        fprintf(log,"Peer shutdown bytes=%d\n",recv(socket,input,65536,0));
    }
    return 1;
}

int main(int argc, char **argv)
{
#if defined(_M_IX86)
    const char *logPath = "C:\\VxKexProbe\\NextParity\\tls-handshake-x86.txt";
#else
    const char *logPath = "C:\\VxKexProbe\\NextParity\\tls-handshake-x64.txt";
#endif
    FILE *log = fopen(logPath, "w");
    WSADATA wsa;
    SOCKET socket = INVALID_SOCKET;
    struct sockaddr_in address;
    SCHANNEL_CRED settings;
    CredHandle credential;
    CtxtHandle context;
    TimeStamp expiry;
    SECURITY_STATUS status;
    ULONG attributes = 0;
    char input[65536];
    int used = 0, iteration, haveContext = 0;
    int result = 1;
    ULONG protocol = argc>1 && strcmp(argv[1],"tls12")==0 ? SP_PROT_TLS1_2_CLIENT : SP_PROT_TLS1_3_CLIENT;
    int verify = argc>2 && strcmp(argv[2],"verify")==0;
    const WCHAR *target = argc>3 && strcmp(argv[3],"wrong-host")==0 ? L"wrong.invalid" : L"localhost";
    if (!log) return 2;
    setbuf(log, NULL);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) goto Done;
    socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (socket == INVALID_SOCKET) goto Done;
    { DWORD timeout = 10000;
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout));
    }
    ZeroMemory(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(4443);
    address.sin_addr.s_addr = inet_addr("192.168.56.1");
    if (connect(socket, (struct sockaddr *) &address, sizeof(address)) == SOCKET_ERROR) {
        fprintf(log, "connect failed WSA=%d\n", WSAGetLastError());
        goto Done;
    }
    fprintf(log, "TCP connected\n");

    ZeroMemory(&settings, sizeof(settings));
    settings.dwVersion = SCHANNEL_CRED_VERSION;
    settings.grbitEnabledProtocols = protocol;
    settings.dwFlags = SCH_CRED_NO_DEFAULT_CREDS |
        (verify ? 0 : SCH_CRED_MANUAL_CRED_VALIDATION);
    status = AcquireCredentialsHandleW(NULL, L"KxSChanl", SECPKG_CRED_OUTBOUND,
        NULL, &settings, NULL, NULL, &credential, &expiry);
    fprintf(log, "AcquireCredentialsHandleW=0x%08lx\n", (unsigned long) status);
    if (status != SEC_E_OK) goto Done;

    for (iteration = 0; iteration < 32; ++iteration) {
        SecBuffer output = {0, SECBUFFER_TOKEN, NULL};
        SecBufferDesc outputDesc = {SECBUFFER_VERSION, 1, &output};
        SecBuffer incoming[2] = {{0, SECBUFFER_TOKEN, NULL},
                                 {0, SECBUFFER_EMPTY, NULL}};
        SecBufferDesc inputDesc = {SECBUFFER_VERSION, 2, incoming};
        DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                      ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
                      ISC_REQ_STREAM |
                      (verify ? 0 : ISC_REQ_MANUAL_CRED_VALIDATION);
        if (haveContext) {
            incoming[0].pvBuffer = input;
            incoming[0].cbBuffer = used;
        }
        status = InitializeSecurityContextW(&credential,
            haveContext ? &context : NULL, (PWSTR)target, flags, 0,
            SECURITY_NATIVE_DREP, haveContext ? &inputDesc : NULL, 0,
            &context, &outputDesc, &attributes, &expiry);
        haveContext = 1;
        fprintf(log, "ISC[%d]=0x%08lx input=%d output=%lu\n", iteration,
            (unsigned long) status, used, output.cbBuffer);
        if (output.cbBuffer && output.pvBuffer) {
            int sent = send_all(socket, (const char *) output.pvBuffer,
                (int) output.cbBuffer);
            FreeContextBuffer(output.pvBuffer);
            if (!sent) {
                fprintf(log, "send failed WSA=%d\n", WSAGetLastError());
                break;
            }
        }
        if (status == SEC_E_OK) {
            SecPkgContext_ConnectionInfo info;
            SECURITY_STATUS queried = QueryContextAttributesW(&context,
                SECPKG_ATTR_CONNECTION_INFO, &info);
            fprintf(log, "ConnectionInfo=0x%08lx protocol=0x%08lx\n",
                (unsigned long) queried,
                queried == SEC_E_OK ? info.dwProtocol : 0);
            if(incoming[1].BufferType==SECBUFFER_EXTRA && incoming[1].cbBuffer<=(ULONG)used) {
                int extra=incoming[1].cbBuffer;
                memmove(input,input+used-extra,extra);
                used=extra;
            } else used=0;
            result = queried == SEC_E_OK && info.dwProtocol == protocol &&
                exchange_http(log,socket,&credential,&context,input,used,flags) ? 0 : 3;
            break;
        }
        if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_INCOMPLETE_MESSAGE) {
            break;
        }
        if (status == SEC_I_CONTINUE_NEEDED) {
            if (incoming[1].BufferType == SECBUFFER_EXTRA &&
                incoming[1].cbBuffer <= (ULONG) used) {
                int extra = (int) incoming[1].cbBuffer;
                memmove(input, input + used - extra, extra);
                used = extra;
            } else {
                used = 0;
            }
        }
        if (used == 0 || status == SEC_E_INCOMPLETE_MESSAGE) {
            int received = recv(socket, input + used, sizeof(input) - used, 0);
            if (received <= 0) {
                fprintf(log, "recv failed WSA=%d size=%d\n", WSAGetLastError(), received);
                break;
            }
            used += received;
        }
    }
    if (haveContext) DeleteSecurityContext(&context);
    FreeCredentialsHandle(&credential);
Done:
    if (socket != INVALID_SOCKET) closesocket(socket);
    WSACleanup();
    fclose(log);
    return result;
}
