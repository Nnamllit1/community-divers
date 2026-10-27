/* SPDX-License-Identifier: GPL-3.0-only
 * Community Divers synthetic loader integration test.
 * The HellForge mod exception in ../LICENSE-EXCEPTION is also granted here.
 * Uses only the pinned synthetic fake_game/hook_test_mod DLLs, never a game process.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef int (*Start)(const wchar_t*);
typedef int (*Target)(int);
static void check(int ok, const char* what) {
    if (!ok) { fprintf(stderr, "FAIL %s (Windows %lu)\n", what, GetLastError()); ExitProcess(1); }
}
static void append(wchar_t* out, size_t size, const wchar_t* base, const wchar_t* name) {
    check(swprintf(out, size, L"%ls\\%ls", base, name) > 0, "form fixture path");
}
static void utf8(const wchar_t* in, char* out, int size) {
    check(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, in, -1, out, size, NULL, NULL) != 0, "encode fixture path");
}
static int contains(const wchar_t* path, const char* expected) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    char text[65536];DWORD size=0;
    int ok=ReadFile(file,text,sizeof(text)-1,&size,NULL)!=0;CloseHandle(file);
    text[size]=0;return ok&&strstr(text,expected)!=NULL;
}
int wmain(int argc,wchar_t** argv) {
    check(argc==5,"arguments: loader, synthetic game, synthetic mod, work prefix");
    wchar_t work[1024],plan[1024],mod1[1024],mod2[1024],log1[1024],log2[1024];
    check(swprintf(work,1024,L"%ls-%lu-%llu",argv[4],GetCurrentProcessId(),GetTickCount64())>0,"form work directory");
    check(CreateDirectoryW(work,NULL),"create new fixture directory");
    append(plan,1024,work,L"plan.txt");append(mod1,1024,work,L"hook_one.dll");append(mod2,1024,work,L"hook_two.dll");
    append(log1,1024,work,L"mod-test.one.log");append(log2,1024,work,L"mod-test.two.log");
    check(CopyFileW(argv[3],mod1,TRUE)&&CopyFileW(argv[3],mod2,TRUE),"copy synthetic mods");
    HMODULE game=LoadLibraryExW(argv[2],NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    check(game!=NULL,"load synthetic game");
    FARPROC target_proc=GetProcAddress(game,"hellforge_fake_target");Target target=NULL;
    memcpy(&target,&target_proc,sizeof(target));
    FARPROC pin=GetProcAddress(game,"hellforge_fake_pin");
    check(target&&pin&&target(5)==16,"synthetic target before hooks");
    char folder[4096],first[4096],second[4096];utf8(work,folder,4096);utf8(mod1,first,4096);utf8(mod2,second,4096);
    FILE* output=_wfopen(plan,L"wb");check(output!=NULL,"create plan");
    fprintf(output,"build\tcommunity-fixture\nlogs\t%s\ngame_pin\tfake_game.dll\t%llx\t7575488b15fafc06024881c2e4030000\nmod\ttest.one\t%s\t%s\nmod\ttest.two\t%s\t%s\n",
        folder,(unsigned long long)((uintptr_t)pin-(uintptr_t)game),folder,first,folder,second);
    check(fclose(output)==0,"write plan");
    HMODULE loader=LoadLibraryExW(argv[1],NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    check(loader!=NULL,"load native loader");
    FARPROC start_proc=GetProcAddress(loader,"hellforge_loader_start");Start start=NULL;memcpy(&start,&start_proc,sizeof(start));
    check(start&&start(plan)==0,"accept synthetic session plan");
    ULONGLONG deadline=GetTickCount64()+6000;
    while(!(contains(log1,"detour installed")&&contains(log2,"detour installed"))&&GetTickCount64()<deadline)Sleep(20);
    check(contains(log1,"pattern found the pin")&&contains(log2,"pattern found the pin"),"both mods receive the loaded event");
    check(contains(log1,"code check passed (live code hooked: no)")&&contains(log2,"code check passed (live code hooked: yes)"),"second mod verifies the original code through the first hook");
    check(target(5)==2016,"two detours chain to the original function");
    printf("PASS loader startup, two native mods, game event, pattern lookup and chained hooks on synthetic DLLs\n");
    fflush(stdout);
    // The upstream loader has process-lifetime worker threads and no unload API.
    // End this dedicated fixture process rather than unloading live hook targets.
    ExitProcess(0);
}
