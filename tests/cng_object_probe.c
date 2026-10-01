#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma comment(lib, "bcrypt.lib")

typedef NTSTATUS (WINAPI *CREATE_HASH)(BCRYPT_ALG_HANDLE,BCRYPT_HASH_HANDLE*,PUCHAR,ULONG,PUCHAR,ULONG,ULONG);
typedef NTSTATUS (WINAPI *DUP_HASH)(BCRYPT_HASH_HANDLE,BCRYPT_HASH_HANDLE*,PUCHAR,ULONG,ULONG);
typedef NTSTATUS (WINAPI *DESTROY_HASH)(BCRYPT_HASH_HANDLE);
typedef NTSTATUS (WINAPI *GENERATE_KEY)(BCRYPT_ALG_HANDLE,BCRYPT_KEY_HANDLE*,PUCHAR,ULONG,PUCHAR,ULONG,ULONG);
typedef NTSTATUS (WINAPI *DUP_KEY)(BCRYPT_KEY_HANDLE,BCRYPT_KEY_HANDLE*,PUCHAR,ULONG,ULONG);
typedef NTSTATUS (WINAPI *DESTROY_KEY)(BCRYPT_KEY_HANDLE);
typedef NTSTATUS (WINAPI *IMPORT_KEY)(BCRYPT_ALG_HANDLE,BCRYPT_KEY_HANDLE,LPCWSTR,BCRYPT_KEY_HANDLE*,PUCHAR,ULONG,PUCHAR,ULONG,ULONG);

static const UCHAR sha256abc[32] = {
  0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
  0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
static const UCHAR aesZero[16] = {
  0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e};

#define CHECK(expr) do { NTSTATUS s=(expr); fprintf(log,"%s=%08lx\n",#expr,(ULONG)s); if(s<0) goto Done; } while(0)
#define VECTOR(data,expected) do { if(memcmp(data,expected,sizeof(expected))) { fprintf(log,"VECTOR FAIL\n"); goto Done; } } while(0)

int main(int argc, char **argv)
{
    HMODULE dll;
    FILE *log;
    CREATE_HASH createHash=NULL;
    DUP_HASH duplicateHash=NULL;
    DESTROY_HASH destroyHash=NULL;
    GENERATE_KEY generateKey=NULL;
    DUP_KEY duplicateKey=NULL;
    DESTROY_KEY destroyKey=NULL;
    IMPORT_KEY importKey=NULL;
    BCRYPT_ALG_HANDLE sha=NULL, aes=NULL;
    BCRYPT_HASH_HANDLE hash=NULL, clone=NULL;
    BCRYPT_KEY_HANDLE key=NULL, keyClone=NULL;
    UCHAR digest[32], secret[16]={0}, plain[16]={0}, iv[16]={0}, ciphertext[16];
    UCHAR *callerObject=NULL;
    ULONG objectCb=0, returned=0;
    struct { BCRYPT_KEY_DATA_BLOB_HEADER header; UCHAR key[16]; } blob;
    int result=1, mode;
    if(argc!=3) return 2;
    log=fopen(argv[2],"w");
    if(!log) return 2;
    setbuf(log,NULL);
    dll=LoadLibraryA(argv[1]);
    if(!dll) { fprintf(log,"LoadLibrary error=%lu\n",GetLastError()); fclose(log); return 3; }
#define RESOLVE(variable,type,name) variable=(type)GetProcAddress(dll,name); if(!variable) goto Done
    RESOLVE(createHash,CREATE_HASH,"BCryptCreateHash");
    RESOLVE(duplicateHash,DUP_HASH,"BCryptDuplicateHash");
    RESOLVE(destroyHash,DESTROY_HASH,"BCryptDestroyHash");
    RESOLVE(generateKey,GENERATE_KEY,"BCryptGenerateSymmetricKey");
    RESOLVE(duplicateKey,DUP_KEY,"BCryptDuplicateKey");
    RESOLVE(destroyKey,DESTROY_KEY,"BCryptDestroyKey");
    RESOLVE(importKey,IMPORT_KEY,"BCryptImportKey");
    CHECK(BCryptOpenAlgorithmProvider(&sha,BCRYPT_SHA256_ALGORITHM,NULL,0));
    CHECK(BCryptGetProperty(sha,BCRYPT_OBJECT_LENGTH,(PUCHAR)&objectCb,sizeof(objectCb),&returned,0));
    callerObject=(UCHAR*)malloc(objectCb);
    if(!callerObject) goto Done;
    for(mode=0;mode<2;++mode) {
        fprintf(log,"HASH caller_owned=%d\n",mode);
        CHECK(createHash(sha,&hash,mode?callerObject:NULL,mode?objectCb:0,NULL,0,0));
        CHECK(BCryptHashData(hash,(PUCHAR)"a",1,0));
        CHECK(duplicateHash(hash,&clone,NULL,0,0));
        CHECK(BCryptHashData(hash,(PUCHAR)"bc",2,0));
        CHECK(BCryptFinishHash(hash,digest,sizeof(digest),0));
        VECTOR(digest,sha256abc);
        CHECK(destroyHash(hash)); hash=NULL;
        CHECK(BCryptHashData(clone,(PUCHAR)"bc",2,0));
        CHECK(BCryptFinishHash(clone,digest,sizeof(digest),0));
        VECTOR(digest,sha256abc);
        CHECK(destroyHash(clone)); clone=NULL;
    }
    CHECK(BCryptOpenAlgorithmProvider(&aes,BCRYPT_AES_ALGORITHM,NULL,0));
    CHECK(generateKey(aes,&key,NULL,0,secret,sizeof(secret),0));
    CHECK(duplicateKey(key,&keyClone,NULL,0,0));
    CHECK(destroyKey(key)); key=NULL;
    CHECK(BCryptEncrypt(keyClone,plain,sizeof(plain),NULL,iv,sizeof(iv),ciphertext,sizeof(ciphertext),&returned,0));
    VECTOR(ciphertext,aesZero);
    CHECK(destroyKey(keyClone)); keyClone=NULL;
    memset(&blob,0,sizeof(blob));
    blob.header.dwMagic=BCRYPT_KEY_DATA_BLOB_MAGIC;
    blob.header.dwVersion=BCRYPT_KEY_DATA_BLOB_VERSION1;
    blob.header.cbKeyData=sizeof(blob.key);
    CHECK(importKey(aes,NULL,BCRYPT_KEY_DATA_BLOB,&key,NULL,0,(PUCHAR)&blob,sizeof(blob),0));
    memset(iv,0,sizeof(iv));
    CHECK(BCryptEncrypt(key,plain,sizeof(plain),NULL,iv,sizeof(iv),ciphertext,sizeof(ciphertext),&returned,0));
    VECTOR(ciphertext,aesZero);
    CHECK(destroyKey(key)); key=NULL;
    fprintf(log,"PASS SHA256, hash cloning, caller-owned memory, AES key creation/cloning/import\n");
    result=0;
Done:
    if(hash && destroyHash) destroyHash(hash);
    if(clone && destroyHash) destroyHash(clone);
    if(key && destroyKey) destroyKey(key);
    if(keyClone && destroyKey) destroyKey(keyClone);
    if(sha) BCryptCloseAlgorithmProvider(sha,0);
    if(aes) BCryptCloseAlgorithmProvider(aes,0);
    free(callerObject);
    fclose(log);
    return result;
}
