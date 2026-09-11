#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <wchar.h>
#include <ctype.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

#define APP_NAME "Windows Control Center"
#define APP_VERSION "2.0 PRO MAX"
#define PROG_NAME "WindowsControl.exe"
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define INSTALL_DIR_NAME "WindowsControl"
#define REPORT_MAX 262144
#define ARG_MAX 4096

#define IDC_CAT        101
#define IDC_CMD        102
#define IDC_STATUS     103
#define IDC_BTN_RUN    104
#define IDC_BTN_INSTALL 105
#define IDC_BTN_UNINSTALL 106
#define IDC_BTN_ABOUT  107
#define IDC_BTN_EXIT   108
#define IDC_BTN_FLOAT  109
#define IDC_DESC       110
#define IDC_ARG_MAIN   111
#define IDC_BTN_TERM   120
#define IDC_BTN_EXPLAIN 121
#define IDC_BTN_MON    130
#define IDC_BTN_UNINS  131
#define IDC_BTN_FIND   132
#define IDC_BTN_WIFI   133
#define IDC_BTN_PROC   134
#define IDC_SEARCH     135
#define IDC_ARG_TOOL   136
#define IDC_LB_TOOL    137
#define IDC_BTN_TOOL1  138
#define IDC_BTN_TOOL2  139
#define IDC_BTN_TOOL3  140
#define IDC_BTN_TOOL4  141
#define IDC_FIND_ROOT  142

#define IDC_ARG_EDIT   201
#define IDC_ARG_OK     202
#define IDC_ARG_CANCEL 203

#define IDC_RPT_EDIT   211
#define IDC_RPT_OK     212

typedef void (*CommandHandler)(const char *arg);

typedef struct {
    const char *name;
    const char *desc;
    CommandHandler fn;
    int needsArg;
} Command;

#include "details.inc"

static char g_detailOut[1200];

static void DecodeDetail(const unsigned char *p, unsigned int len, char *out, unsigned int base) {
    unsigned int i;
    for (i = 0; i < len; i++)
        out[i] = (char)(p[i] ^ (0x5A ^ ((base + i) & 0x3F)));
    out[len] = 0;
}

static const char *CmdDetail(const char *cat, const char *name) {
    char key[96];
    unsigned int i;
    unsigned int n = (unsigned int)(sizeof(g_detailsIdx) / sizeof(g_detailsIdx[0]));
    snprintf(key, sizeof key, "%s/%s", cat, name);
    for (i = 0; i < n; i++) {
        unsigned int st = g_detailsIdx[i].off;
        unsigned int ln = g_detailsIdx[i].len;
        if (ln >= sizeof g_detailOut) ln = (unsigned int)(sizeof g_detailOut - 1);
        DecodeDetail(g_detailsEnc + st, ln, g_detailOut, st);
        if (strcmp(g_detailOut, key) == 0) {
            const char *d = g_detailOut + strlen(g_detailOut) + 1;
            return *d ? d : NULL;
        }
    }
    return NULL;
}

void OnExplain(const char *arg);
void OnUtilTerminal(const char *arg);
void TermExecInternal(const char *line, void (*out)(const char *), int *closeFlag);
void OpenTerminalWindow(void);
void OnMonitorTool(const char *arg);
void OnSystemStats(const char *arg);
void OnUninstallTool(const char *arg);
void OnListApps(const char *arg);
void OnFindFiles(const char *arg);
void OnFindTool(const char *arg);
void OnWifiTool(const char *arg);
void OnProcessList(const char *arg);
void OnProcessTool(const char *arg);
void OnNetSpeed(const char *arg);
void OnToolsMenu(const char *arg);

LRESULT CALLBACK GuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef struct {
    const char *name;
    const Command *items;
    int count;
} Category;

static HINSTANCE g_hInst;
static int g_consoleMode = 0;
static int g_consoleReady = 0;
static int g_launchGui = 0;
static int g_cliInteractive = 0;
static char g_report[REPORT_MAX];
static int g_reportLen = 0;
static char g_argBuf[ARG_MAX];

void Notify(const char *msg) {
    if (g_consoleMode) {
        printf("\n[Info] %s\n", msg);
    } else {
        MessageBoxA(NULL, msg, APP_NAME, MB_OK | MB_ICONINFORMATION);
    }
}

void Run(const char *file, const char *args, int admin) {
    char verb[6];
    verb[0] = 'r'; verb[1] = 'u'; verb[2] = 'n'; verb[3] = 'a'; verb[4] = 's'; verb[5] = 0;
    ShellExecuteA(NULL, admin ? verb : "open", file, args, NULL, SW_SHOWNORMAL);
}

void RunURI(const char *uri) {
    ShellExecuteA(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
}

void RunCmd(const char *command, int keepOpen) {
    char args[900];
    sprintf(args, "/c %s%s", command, keepOpen ? " & pause" : "");
    Run("cmd.exe", args, 0);
}

void RunElevatedCmd(const char *command) {
    char args[900];
    sprintf(args, "/c %s", command);
    Run("cmd.exe", args, 1);
}

void OpenSettings(const char *page) {
    char uri[256];
    if (page && page[0])
        sprintf(uri, "ms-settings:%s", page);
    else
        strcpy(uri, "ms-settings:");
    RunURI(uri);
}

void OpenCpl(const char *cpl) {
    Run("control.exe", cpl, 0);
}

void OpenCplName(const char *name) {
    char args[300];
    sprintf(args, "/name %s", name);
    Run("control.exe", args, 0);
}

void OpenShellFolder(const char *folder) {
    char args[700];
    sprintf(args, "\"%s\"", folder);
    Run("explorer.exe", args, 0);
}

void SimulateKey(BYTE vk) {
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}

void SimulateCombo(BYTE vk1, BYTE vk2) {
    keybd_event(vk1, 0, 0, 0);
    keybd_event(vk2, 0, 0, 0);
    keybd_event(vk2, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(vk1, 0, KEYEVENTF_KEYUP, 0);
}

void SimulateTriple(BYTE vk1, BYTE vk2, BYTE vk3) {
    keybd_event(vk1, 0, 0, 0);
    keybd_event(vk2, 0, 0, 0);
    keybd_event(vk3, 0, 0, 0);
    keybd_event(vk3, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(vk2, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(vk1, 0, KEYEVENTF_KEYUP, 0);
}

void GetSelfPath(char *buf, int size) {
    GetModuleFileNameA(NULL, buf, (DWORD)size);
}

void ElevateSelf(const char *cliArgs) {
    char self[MAX_PATH];
    char verb[6];
    verb[0] = 'r'; verb[1] = 'u'; verb[2] = 'n'; verb[3] = 'a'; verb[4] = 's'; verb[5] = 0;
    GetSelfPath(self, MAX_PATH);
    ShellExecuteA(NULL, verb, self, cliArgs, NULL, SW_SHOWNORMAL);
}

static void (*g_reportSink)(const char *text) = NULL;

void ReportClear(void) {
    g_report[0] = 0;
    g_reportLen = 0;
}

void ReportAdd(const char *fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    int left = REPORT_MAX - g_reportLen - 2;
    if (left <= 0) return;
    int len = (int)strlen(buf);
    if (len >= left) len = left - 1;
    memcpy(g_report + g_reportLen, buf, (size_t)len);
    g_reportLen += len;
    g_report[g_reportLen] = '\n';
    g_reportLen++;
    g_report[g_reportLen] = 0;
}

void ShowReportDialog(void);

void ShowReport(void) {
    if (!g_report[0]) return;
    if (g_reportSink) {
        g_reportSink(g_report);
        return;
    }
    if (g_consoleMode) {
        printf("\n%s\n", g_report);
    } else {
        ShowReportDialog();
    }
}

int WriteRegString(HKEY root, const char *subkey, const char *value, const char *data) {
    HKEY hKey;
    LONG r = RegCreateKeyExA(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
    if (r != ERROR_SUCCESS) return 0;
    r = RegSetValueExA(hKey, value, 0, REG_SZ, (const BYTE *)data, (DWORD)strlen(data) + 1);
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

void RunKeyPath(char *out, int size, int once) {
    strncpy(out, "Software\\Microsoft\\Windows\\CurrentVersion", size - 1);
    out[size - 1] = 0;
    strncat(out, once ? "\\RunOnce" : "\\Run", size - strlen(out) - 1);
}

int DeleteRegValue(HKEY root, const char *subkey, const char *value) {
    HKEY hKey;
    if (RegOpenKeyExA(root, subkey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return 0;
    LONG r = RegDeleteValueA(hKey, value);
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

int ReadRegStr(HKEY root, const char *sub, const char *val, char *out, int size) {
    HKEY hk;
    LONG r = RegOpenKeyExA(root, sub, 0, KEY_QUERY_VALUE, &hk);
    if (r != ERROR_SUCCESS) return 0;
    DWORD type = 0, sz = (DWORD)size;
    r = RegQueryValueExA(hk, val, NULL, &type, (BYTE *)out, &sz);
    RegCloseKey(hk);
    if (r == ERROR_SUCCESS && type == REG_SZ) {
        out[size - 1] = 0;
        return 1;
    }
    return 0;
}

typedef struct { void *lpVtbl; } WcUnknown;
typedef HRESULT (STDMETHODCALLTYPE *WcQIMethod)(void *, const IID *, void **);
typedef ULONG (STDMETHODCALLTYPE *WcReleaseMethod)(void *);
typedef HRESULT (STDMETHODCALLTYPE *WcSetPathMethod)(void *, const wchar_t *);
typedef HRESULT (STDMETHODCALLTYPE *WcSaveMethod)(void *, const wchar_t *, BOOL);

int CreateShortcut(const char *lnkPath, const char *target) {
    wchar_t wLnk[MAX_PATH], wTarget[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, lnkPath, -1, wLnk, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, target, -1, wTarget, MAX_PATH);

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0;

    void *shellLink = NULL;
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IUnknown, (void **)&shellLink);
    if (FAILED(hr) || !shellLink) { CoUninitialize(); return 0; }

    void **vtbl = (void **)((WcUnknown *)shellLink)->lpVtbl;
    void *slw = NULL;
    hr = ((WcQIMethod)vtbl[0])(shellLink, &IID_IShellLinkW, (void **)&slw);
    if (FAILED(hr) || !slw) { ((WcReleaseMethod)vtbl[2])(shellLink); CoUninitialize(); return 0; }

    void **wvt = (void **)((WcUnknown *)slw)->lpVtbl;
    ((WcSetPathMethod)wvt[20])(slw, wTarget);

    void *persistFile = NULL;
    ((WcQIMethod)wvt[0])(slw, &IID_IPersistFile, (void **)&persistFile);
    if (persistFile) {
        void **pvtbl = (void **)((WcUnknown *)persistFile)->lpVtbl;
        hr = ((WcSaveMethod)pvtbl[6])(persistFile, wLnk, TRUE);
        ((WcReleaseMethod)pvtbl[2])(persistFile);
    }
    ((WcReleaseMethod)wvt[2])(slw);
    ((WcReleaseMethod)vtbl[2])(shellLink);
    CoUninitialize();
    return SUCCEEDED(hr);
}

int AskYesNo(const char *question) {
    if (g_consoleMode) {
        char line[64];
        printf("%s [y/N]: ", question);
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) return 0;
        return line[0] == 'y' || line[0] == 'Y';
    }
    return MessageBoxA(NULL, question, APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

int GetInstallPaths(char *destDir, char *destExe, int size) {
    char local[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, local)))
        return 0;
    sprintf(destDir, "%s\\%s", local, INSTALL_DIR_NAME);
    sprintf(destExe, "%s\\%s.exe", destDir, INSTALL_DIR_NAME);
    return 1;
}

int IsRunningFromInstallDir(void) {
    char self[MAX_PATH], destDir[MAX_PATH], destExe[MAX_PATH];
    GetSelfPath(self, MAX_PATH);
    if (!GetInstallPaths(destDir, destExe, MAX_PATH)) return 0;
    return stricmp(self, destExe) == 0;
}

void DoInstall(void) {
    char self[MAX_PATH], destDir[MAX_PATH], destExe[MAX_PATH];
    char desktop[MAX_PATH], startmenu[MAX_PATH];
    GetSelfPath(self, MAX_PATH);
    GetInstallPaths(destDir, destExe, MAX_PATH);

    if (g_consoleMode) {
        printf("\n=== Installing " APP_NAME " ===\n");
        printf("Source : %s\n", self);
        printf("Target : %s\n", destExe);
    }

    CreateDirectoryA(destDir, NULL);
    if (stricmp(self, destExe) != 0) {
        if (!CopyFileA(self, destExe, FALSE)) {
            Notify("Install failed: could not copy the program.");
            return;
        }
    }
    if (g_consoleMode) printf("[OK] Program copied\n");

    SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);
    sprintf(desktop, "%s\\%s.lnk", desktop, APP_NAME);
    if (CreateShortcut(desktop, destExe)) {
        if (g_consoleMode) printf("[OK] Desktop shortcut created\n");
    }

    SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, startmenu);
    sprintf(startmenu, "%s\\%s.lnk", startmenu, APP_NAME);
    if (CreateShortcut(startmenu, destExe)) {
        if (g_consoleMode) printf("[OK] Start Menu shortcut created\n");
    }

    WriteRegString(HKEY_CURRENT_USER, "Software\\WindowsControlCenter",
                   "InstallDir", destDir);
    WriteRegString(HKEY_CURRENT_USER, "Software\\WindowsControlCenter",
                   "Installed", "1");

    if (AskYesNo("Start " APP_NAME " automatically with Windows?")) {
        char cmd[700];
        char rk[128];
        sprintf(cmd, "\"%s\"", destExe);
        RunKeyPath(rk, sizeof rk, 0);
        WriteRegString(HKEY_CURRENT_USER, rk, APP_NAME, cmd);
        if (g_consoleMode) printf("[OK] Auto-start registered\n");
    }

    if (g_consoleMode) {
        printf("\n=== " APP_NAME " installed successfully ===\n");
        printf("Use the desktop / start menu shortcut, or run:\n");
        printf("  %s --gui\n", destExe);
    } else {
        Notify(APP_NAME " installed successfully.\n\nDesktop and Start Menu shortcuts were created.");
    }
}

void DoUninstall(void) {
    char destDir[MAX_PATH], destExe[MAX_PATH];
    char desktop[MAX_PATH], startmenu[MAX_PATH];
    GetInstallPaths(destDir, destExe, MAX_PATH);

    if (g_consoleMode) printf("\n=== Uninstalling " APP_NAME " ===\n");

    SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);
    sprintf(desktop, "%s\\%s.lnk", desktop, APP_NAME);
    DeleteFileA(desktop);
    SHGetFolderPathA(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, startmenu);
    sprintf(startmenu, "%s\\%s.lnk", startmenu, APP_NAME);
    DeleteFileA(startmenu);

    char rk[128];
    RunKeyPath(rk, sizeof rk, 0);
    DeleteRegValue(HKEY_CURRENT_USER, rk, APP_NAME);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\WindowsControlCenter");

    int runningFromInstall = IsRunningFromInstallDir();

    if (runningFromInstall) {
        char cmd[900];
        sprintf(cmd, "/c ping -n 3 127.0.0.1 >nul & rmdir /s /q \"%s\"", destDir);
        Run("cmd.exe", cmd, 0);
    } else {
        DeleteFileA(destExe);
        RemoveDirectoryA(destDir);
    }

    if (g_consoleMode) {
        printf("[OK] Shortcuts and auto-start removed\n");
        if (runningFromInstall) printf("[OK] Installed copy will be deleted automatically\n");
        printf("=== Uninstall complete ===\n");
    } else {
        Notify(APP_NAME " was uninstalled.\nShortcuts and auto-start entry were removed.");
    }
}

void FormatSize(ULONGLONG bytes, char *out, int size) {
    if (bytes >= (1024ULL * 1024 * 1024))
        sprintf(out, "%.2f GB", (double)bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= (1024ULL * 1024))
        sprintf(out, "%.1f MB", (double)bytes / (1024.0 * 1024));
    else if (bytes >= 1024)
        sprintf(out, "%.0f KB", (double)bytes / 1024.0);
    else
        sprintf(out, "%llu B", bytes);
}

const char *RegRootName(HKEY root) {
    if (root == HKEY_LOCAL_MACHINE) return "HKLM";
    if (root == HKEY_CURRENT_USER) return "HKCU";
    if (root == HKEY_CLASSES_ROOT) return "HKCR";
    if (root == HKEY_USERS) return "HKU";
    return "HKCC";
}

int ParseRegPath(const char *arg, HKEY *root, char *subkey, int subkeySize,
                 char *value, int valueSize) {
    char tmp[ARG_MAX];
    strncpy(tmp, arg, ARG_MAX - 1);
    tmp[ARG_MAX - 1] = 0;
    if (value && valueSize > 0) value[0] = 0;
    char *pipe = strchr(tmp, '|');
    if (pipe) {
        *pipe = 0;
        if (value && valueSize > 0) {
            strncpy(value, pipe + 1, valueSize - 1);
            value[valueSize - 1] = 0;
        }
    }
    char *slash = strchr(tmp, '\\');
    if (!slash) return 0;
    *slash = 0;
    if (stricmp(tmp, "HKLM") == 0 || stricmp(tmp, "HKEY_LOCAL_MACHINE") == 0)
        *root = HKEY_LOCAL_MACHINE;
    else if (stricmp(tmp, "HKCU") == 0 || stricmp(tmp, "HKEY_CURRENT_USER") == 0)
        *root = HKEY_CURRENT_USER;
    else if (stricmp(tmp, "HKCR") == 0 || stricmp(tmp, "HKEY_CLASSES_ROOT") == 0)
        *root = HKEY_CLASSES_ROOT;
    else if (stricmp(tmp, "HKU") == 0 || stricmp(tmp, "HKEY_USERS") == 0)
        *root = HKEY_USERS;
    else if (stricmp(tmp, "HKCC") == 0 || stricmp(tmp, "HKEY_CURRENT_CONFIG") == 0)
        *root = HKEY_CURRENT_CONFIG;
    else
        return 0;
    strncpy(subkey, slash + 1, subkeySize - 1);
    subkey[subkeySize - 1] = 0;
    return 1;
}

int PickFile(char *out) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof ofn);
    char buf[MAX_PATH];
    buf[0] = 0;
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = "Select a file";
    if (!GetOpenFileNameA(&ofn)) return 0;
    strncpy(out, buf, MAX_PATH - 1);
    out[MAX_PATH - 1] = 0;
    return 1;
}

void TimeStampFile(const char *prefix, const char *ext, char *out, int size) {
    char desktop[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    sprintf(out, "%s\\%s-%04d%02d%02d-%02d%02d%02d.%s",
            desktop, prefix, tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday,
            tmv->tm_hour, tmv->tm_min, tmv->tm_sec, ext);
}

int CalcHashFile(const char *path, ALG_ID alg, char *hexOut, int hexSize) {
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (!CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
            return 0;
    }
    HCRYPTHASH hh = 0;
    if (!CryptCreateHash(prov, alg, 0, 0, &hh)) {
        CryptReleaseContext(prov, 0);
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        CryptDestroyHash(hh);
        CryptReleaseContext(prov, 0);
        return 0;
    }
    BYTE buf[65536];
    size_t rd;
    while ((rd = fread(buf, 1, sizeof buf, f)) > 0)
        CryptHashData(hh, buf, (DWORD)rd, 0);
    fclose(f);
    BYTE digest[64];
    DWORD dl = sizeof digest;
    if (!CryptGetHashParam(hh, HP_HASHVAL, digest, &dl, 0)) {
        CryptDestroyHash(hh);
        CryptReleaseContext(prov, 0);
        return 0;
    }
    for (DWORD i = 0; i < dl && i * 2 + 2 < (DWORD)hexSize; i++)
        sprintf(hexOut + i * 2, "%02x", digest[i]);
    CryptDestroyHash(hh);
    CryptReleaseContext(prov, 0);
    return 1;
}

void OnHashFile(const char *arg) {
    char path[MAX_PATH];
    if (arg && arg[0]) {
        strncpy(path, arg, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
    } else if (!PickFile(path)) {
        Notify("No file selected.");
        return;
    }
    char md5[33], sha1[41], sha256[65];
    md5[0] = sha1[0] = sha256[0] = 0;
    if (!CalcHashFile(path, CALG_MD5, md5, sizeof md5))
        strcpy(md5, "failed");
    if (!CalcHashFile(path, CALG_SHA1, sha1, sizeof sha1))
        strcpy(sha1, "failed");
    if (!CalcHashFile(path, CALG_SHA_256, sha256, sizeof sha256))
        strcpy(sha256, "failed");
    ReportClear();
    ReportAdd("File   : %s", path);
    ReportAdd("MD5    : %s", md5);
    ReportAdd("SHA-1  : %s", sha1);
    ReportAdd("SHA-256: %s", sha256);
    ShowReport();
}

void OnScreenshot(const char *arg) {
    char path[MAX_PATH];
    if (arg && arg[0]) {
        strncpy(path, arg, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
    } else {
        TimeStampFile("screenshot", "bmp", path, MAX_PATH);
    }
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC hdc = GetDC(NULL);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof bi);
    bi.biSize = sizeof bi;
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    DWORD imgSize = (DWORD)w * (DWORD)h * 4;
    BYTE *pixels = (BYTE *)malloc(imgSize);
    BOOL ok = GetDIBits(mem, bmp, 0, h, pixels, (BITMAPINFO *)&bi, DIB_RGB_COLORS) != 0;

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, hdc);

    if (ok) {
        FILE *f = fopen(path, "wb");
        if (f) {
            BITMAPFILEHEADER bf;
            memset(&bf, 0, sizeof bf);
            bf.bfType = 0x4D42;
            bf.bfOffBits = (DWORD)(sizeof bf + sizeof bi);
            bf.bfSize = bf.bfOffBits + imgSize;
            fwrite(&bf, 1, sizeof bf, f);
            fwrite(&bi, 1, sizeof bi, f);
            fwrite(pixels, 1, imgSize, f);
            fclose(f);
            ReportClear();
            ReportAdd("Screenshot saved: %s", path);
            ReportAdd("Resolution: %dx%d", w, h);
            ShowReport();
        } else {
            ok = FALSE;
        }
    }
    free(pixels);
    if (!ok) {
        ReportClear();
        ReportAdd("Screenshot failed.");
        ShowReport();
    }
}

void OnSetWallpaper(const char *arg) {
    char path[MAX_PATH];
    if (arg && arg[0]) {
        strncpy(path, arg, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
    } else if (!PickFile(path)) {
        Notify("No file selected.");
        return;
    }
    SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)path,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    ReportClear();
    ReportAdd("Wallpaper set to: %s", path);
    ShowReport();
}

void OnVolumeSet(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: volume-set <0-100>");
        ReportAdd("Example: volume-set 50");
        ShowReport();
        return;
    }
    int v = atoi(arg);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    DWORD vol = (DWORD)((v * 0xFFFF) / 100);
    waveOutSetVolume(0, (vol << 16) | vol);
    ReportClear();
    ReportAdd("System volume set to %d%%", v);
    ShowReport();
}

void OnVolumePreset(const char *arg) {
    (void)arg;
    OnVolumeSet(NULL);
}

void OnMonitorOff(const char *arg) { (void)arg;
    SendMessageA(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
}

void OnMouseSwapOn(const char *arg) { (void)arg;
    SwapMouseButton(TRUE);
}

void OnMouseSwapOff(const char *arg) { (void)arg;
    SwapMouseButton(FALSE);
}

void OnToggleKey(BYTE vk) {
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
}

void OnCapsToggle(const char *arg) { (void)arg; OnToggleKey(VK_CAPITAL); }
void OnNumlockToggle(const char *arg) { (void)arg; OnToggleKey(VK_NUMLOCK); }
void OnScrollToggle(const char *arg) { (void)arg; OnToggleKey(VK_SCROLL); }

void OnClipboardClear(const char *arg) { (void)arg;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        CloseClipboard();
        Notify("Clipboard cleared.");
    } else {
        Notify("Clipboard is locked by another program.");
    }
}

void OnGodMode(const char *arg) { (void)arg;
    char desktop[MAX_PATH], folder[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop);
    sprintf(folder, "%s\\GodMode.{ED7BA470-8E54-465E-825C-99712043E01C}", desktop);
    if (CreateDirectoryA(folder, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        Notify("God Mode created on your desktop.");
    } else {
        Notify("Could not create God Mode folder.");
    }
}

void OnShutdownTimer(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: shutdown-timer <seconds>");
        ReportAdd("Example: shutdown-timer 3600  (1 hour)");
        ShowReport();
        return;
    }
    int sec = atoi(arg);
    if (sec < 0) sec = 0;
    char cmd[64];
    sprintf(cmd, "shutdown /s /t %d", sec);
    RunCmd(cmd, 0);
    ReportClear();
    ReportAdd("Shutdown scheduled in %d seconds. Use 'abort-shutdown' to cancel.", sec);
    ShowReport();
}

void OnAbortShutdown(const char *arg) { (void)arg;
    RunCmd("shutdown /a", 0);
    Notify("Shutdown aborted (if one was pending).");
}

void OnOpenFile(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: open-file <path-or-url>");
        ReportAdd("Opens a file, folder or web URL with its default program.");
        ShowReport();
        return;
    }
    Run(arg, NULL, 0);
}

void OnDateSync(const char *arg) { (void)arg;
    RunElevatedCmd("w32tm /resync");
}

void OnCleanTemp(const char *arg) { (void)arg;
    char temp[MAX_PATH], pattern[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    sprintf(pattern, "%s*", temp);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    ULONGLONG freed = 0;
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char full[MAX_PATH];
                sprintf(full, "%s%s", temp, fd.cFileName);
                ULARGE_INTEGER sz;
                sz.LowPart = fd.nFileSizeLow;
                sz.HighPart = fd.nFileSizeHigh;
                if (DeleteFileA(full)) {
                    freed += sz.QuadPart;
                    count++;
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char fs[64];
    FormatSize(freed, fs, sizeof fs);
    ReportClear();
    ReportAdd("Temp folder: %s", temp);
    ReportAdd("Deleted %d files, freed %s", count, fs);
    ShowReport();
}

void OnRegBackup(const char *arg) {
    char key[512];
    if (arg && arg[0]) {
        strncpy(key, arg, sizeof key - 1);
        key[sizeof key - 1] = 0;
    } else {
        strcpy(key, "HKCU\\Software");
    }
    char fname[MAX_PATH];
    TimeStampFile("regbackup", "reg", fname, MAX_PATH);
    char cmd[1400];
    sprintf(cmd, "reg export \"%s\" \"%s\" /y", key, fname);
    RunCmd(cmd, 0);
    ReportClear();
    ReportAdd("Registry export started:");
    ReportAdd("  Key : %s", key);
    ReportAdd("  File: %s", fname);
    ShowReport();
}

void OnRegImport(const char *arg) { (void)arg;
    char path[MAX_PATH];
    if (!PickFile(path)) {
        Notify("No file selected.");
        return;
    }
    char args[700];
    sprintf(args, "/s \"%s\"", path);
    Run("regedit.exe", args, 0);
    ReportClear();
    ReportAdd("Importing registry file: %s", path);
    ReportAdd("Note: HKLM entries need an administrator.");
    ShowReport();
}

void OnRegOpen(const char *arg) {
    HKEY root;
    char sub[512], val[256];
    if (!arg || !arg[0] || !ParseRegPath(arg, &root, sub, sizeof sub, val, sizeof val)) {
        ReportClear();
        ReportAdd("Usage: registry-open <ROOT\\path>");
        ReportAdd("Example: registry-open HKCU\\Software\\Microsoft\\Windows");
        ShowReport();
        return;
    }
    Run("regedit.exe", NULL, 0);
    ReportClear();
    ReportAdd("Registry Editor opened.");
    ReportAdd("Navigate to: %s\\%s", RegRootName(root), sub);
    ShowReport();
}

void OnRegRead(const char *arg) {
    HKEY root;
    char sub[512], val[256];
    if (!arg || !arg[0] || !ParseRegPath(arg, &root, sub, sizeof sub, val, sizeof val)) {
        ReportClear();
        ReportAdd("Usage: registry-read <ROOT\\path|value>");
        ReportAdd("Example: registry-read HKCU\\Software\\MyApp|MyValue");
        ReportAdd("Without |value the whole key is listed.");
        ShowReport();
        return;
    }
    HKEY hk;
    LONG r = RegOpenKeyExA(root, sub, 0, KEY_QUERY_VALUE, &hk);
    if (r != ERROR_SUCCESS) {
        ReportClear();
        ReportAdd("Cannot open key '%s\\%s' (error %ld)", RegRootName(root), sub, r);
        ShowReport();
        return;
    }
    ReportClear();
    ReportAdd("Registry key: %s\\%s", RegRootName(root), sub);
    if (val[0]) {
        char data[4096];
        DWORD type = 0, size = sizeof data;
        r = RegQueryValueExA(hk, val, NULL, &type, (BYTE *)data, &size);
        if (r == ERROR_SUCCESS) {
            switch (type) {
                case REG_SZ:
                case REG_EXPAND_SZ:
                    data[sizeof data - 1] = 0;
                    ReportAdd("%s = \"%s\"", val, data);
                    break;
                case REG_DWORD:
                    ReportAdd("%s = 0x%08lX (%lu)", val, *(DWORD *)data, *(DWORD *)data);
                    break;
                case REG_QWORD:
                    ReportAdd("%s = 0x%016llX (%llu)", val, *(ULONGLONG *)data, *(ULONGLONG *)data);
                    break;
                case REG_BINARY:
                    ReportAdd("%s = binary, %lu bytes", val, size);
                    break;
                default:
                    ReportAdd("%s = type %lu, %lu bytes", val, type, size);
            }
        } else {
            ReportAdd("Value '%s' not found (error %ld)", val, r);
        }
    } else {
        DWORD idx = 0;
        char vn[1024];
        BYTE vd[4096];
        DWORD vns, vds, vt;
        ReportAdd("Values:");
        for (;;) {
            vns = sizeof vn;
            vds = sizeof vd;
            r = RegEnumValueA(hk, idx++, vn, &vns, NULL, &vt, vd, &vds);
            if (r != ERROR_SUCCESS) break;
            if (vt == REG_SZ || vt == REG_EXPAND_SZ) {
                vd[sizeof vd - 1] = 0;
                ReportAdd("  %s = \"%s\"", vn, (char *)vd);
            } else if (vt == REG_DWORD) {
                ReportAdd("  %s = 0x%08lX (%lu)", vn, *(DWORD *)vd, *(DWORD *)vd);
            } else if (vt == REG_QWORD) {
                ReportAdd("  %s = 0x%016llX", vn, *(ULONGLONG *)vd);
            } else {
                ReportAdd("  %s = type %lu, %lu bytes", vn, vt, vds);
            }
        }
        if (idx == 0) ReportAdd("  (no values)");
        DWORD sidx = 0;
        char sn[1024];
        DWORD sns;
        ReportAdd("Subkeys:");
        for (;;) {
            sns = sizeof sn;
            r = RegEnumKeyExA(hk, sidx++, sn, &sns, NULL, NULL, NULL, NULL);
            if (r != ERROR_SUCCESS) break;
            ReportAdd("  %s\\", sn);
        }
        if (sidx == 0) ReportAdd("  (no subkeys)");
    }
    RegCloseKey(hk);
    ShowReport();
}

void OnRegWrite(const char *arg) {
    char tmp[ARG_MAX];
    strncpy(tmp, arg ? arg : "", ARG_MAX - 1);
    tmp[ARG_MAX - 1] = 0;
    char *p1 = strchr(tmp, '|');
    if (!p1) {
        ReportClear();
        ReportAdd("Usage: registry-write <ROOT\\path|value|data>");
        ReportAdd("Numeric data becomes REG_DWORD, anything else REG_SZ.");
        ShowReport();
        return;
    }
    *p1 = 0;
    char *p2 = strchr(p1 + 1, '|');
    if (!p2) {
        ReportClear();
        ReportAdd("Usage: registry-write <ROOT\\path|value|data>");
        ShowReport();
        return;
    }
    *p2 = 0;
    HKEY root;
    char sub[512];
    if (!ParseRegPath(tmp, &root, sub, sizeof sub, NULL, 0)) {
        ReportClear();
        ReportAdd("Invalid registry path: %s", tmp);
        ShowReport();
        return;
    }
    const char *value = p1 + 1;
    const char *data = p2 + 1;
    HKEY hk;
    LONG r = RegCreateKeyExA(root, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &hk, NULL);
    if (r != ERROR_SUCCESS) {
        char cmd[1600];
        sprintf(cmd, "reg add \"%s\\%s\" /v \"%s\" /d \"%s\" /f",
                RegRootName(root), sub, value, data);
        ReportClear();
        ReportAdd("Direct write failed (error %ld). Retrying elevated...", r);
        ShowReport();
        RunElevatedCmd(cmd);
        return;
    }
    char *end;
    long num = strtol(data, &end, 0);
    if (data[0] && *end == 0) {
        DWORD dw = (DWORD)num;
        r = RegSetValueExA(hk, value, 0, REG_DWORD, (const BYTE *)&dw, sizeof dw);
    } else {
        r = RegSetValueExA(hk, value, 0, REG_SZ, (const BYTE *)data, (DWORD)strlen(data) + 1);
    }
    RegCloseKey(hk);
    ReportClear();
    if (r == ERROR_SUCCESS) {
        ReportAdd("Value written: %s\\%s\\%s = %s", RegRootName(root), sub, value, data);
    } else {
        ReportAdd("Write failed (error %ld)", r);
    }
    ShowReport();
}

void OnRegDelete(const char *arg) {
    HKEY root;
    char sub[512], val[256];
    if (!arg || !arg[0] || !ParseRegPath(arg, &root, sub, sizeof sub, val, sizeof val)) {
        ReportClear();
        ReportAdd("Usage: registry-delete <ROOT\\path|value>");
        ReportAdd("With |value only that value is removed.");
        ReportAdd("Without |value the whole key is removed.");
        ShowReport();
        return;
    }
    ReportClear();
    if (val[0]) {
        if (DeleteRegValue(root, sub, val))
            ReportAdd("Deleted value: %s\\%s\\%s", RegRootName(root), sub, val);
        else
            ReportAdd("Could not delete value (access denied or not found).");
    } else {
        LONG r = RegDeleteTreeA(root, sub);
        if (r == ERROR_SUCCESS)
            ReportAdd("Deleted key: %s\\%s", RegRootName(root), sub);
        else if (r == ERROR_FILE_NOT_FOUND)
            ReportAdd("Key not found: %s\\%s", RegRootName(root), sub);
        else
            ReportAdd("Could not delete key (error %ld - try as administrator)", r);
    }
    ShowReport();
}

void SearchReg(HKEY root, const char *path, const char *needle, int depth, int *count) {
    HKEY hk;
    if (RegOpenKeyExA(root, path, 0, KEY_ENUMERATE_SUB_KEYS, &hk) != ERROR_SUCCESS) return;
    DWORD idx = 0;
    char name[1024];
    DWORD nsz;
    for (;;) {
        nsz = sizeof name;
        LONG r = RegEnumKeyExA(hk, idx++, name, &nsz, NULL, NULL, NULL, NULL);
        if (r != ERROR_SUCCESS) break;
        if (strstr(name, needle)) {
            ReportAdd("  %s\\%s", path, name);
            (*count)++;
        }
        if (*count >= 200) break;
        if (depth > 0) {
            char sub[2048];
            sprintf(sub, "%s\\%s", path, name);
            SearchReg(root, sub, needle, depth - 1, count);
        }
        if (*count >= 200) break;
    }
    RegCloseKey(hk);
}

void OnRegSearch(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: registry-search <text>");
        ReportAdd("Searches subkey names under HKLM\\Software and HKCU\\Software.");
        ShowReport();
        return;
    }
    ReportClear();
    ReportAdd("Searching registry subkeys for '%s'...", arg);
    int count = 0;
    SearchReg(HKEY_LOCAL_MACHINE, "Software", arg, 2, &count);
    SearchReg(HKEY_CURRENT_USER, "Software", arg, 3, &count);
    ReportAdd("Found %d match(es).", count);
    ShowReport();
}

const char *ServiceStateName(DWORD state) {
    switch (state) {
        case SERVICE_STOPPED: return "Stopped";
        case SERVICE_START_PENDING: return "Start Pending";
        case SERVICE_STOP_PENDING: return "Stop Pending";
        case SERVICE_RUNNING: return "Running";
        case SERVICE_CONTINUE_PENDING: return "Continue Pending";
        case SERVICE_PAUSE_PENDING: return "Pause Pending";
        case SERVICE_PAUSED: return "Paused";
        default: return "Unknown";
    }
}

const char *ServiceStartName(DWORD start) {
    switch (start) {
        case SERVICE_BOOT_START: return "Boot";
        case SERVICE_SYSTEM_START: return "System";
        case SERVICE_AUTO_START: return "Auto";
        case SERVICE_DEMAND_START: return "Manual";
        case SERVICE_DISABLED: return "Disabled";
        default: return "Unknown";
    }
}

void OnServicesList(const char *arg) { (void)arg;
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) {
        ReportClear();
        ReportAdd("Cannot open Service Control Manager (error %lu).", GetLastError());
        ReportAdd("Try running as administrator.");
        ShowReport();
        return;
    }
    DWORD needed = 0, count = 0;
    EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                          NULL, 0, &needed, &count, NULL, NULL);
    if (needed == 0) {
        CloseServiceHandle(scm);
        ReportClear();
        ReportAdd("No services found.");
        ShowReport();
        return;
    }
    ENUM_SERVICE_STATUS_PROCESSA *svcs = (ENUM_SERVICE_STATUS_PROCESSA *)malloc(needed);
    if (!svcs || !EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                                        SERVICE_STATE_ALL, (LPBYTE)svcs, needed,
                                        &needed, &count, NULL, NULL)) {
        free(svcs);
        CloseServiceHandle(scm);
        ReportClear();
        ReportAdd("Failed to enumerate services.");
        ShowReport();
        return;
    }
    ReportClear();
    ReportAdd("Windows Services (%lu):", count);
    ReportAdd("  %-28s %-38s %-12s %-8s %8s", "Name", "Display Name", "State", "Start", "PID");
    for (DWORD i = 0; i < count; i++) {
        ReportAdd("  %-28s %-38s %-12s %-8s %8lu",
                  svcs[i].lpServiceName,
                  svcs[i].lpDisplayName,
                  ServiceStateName(svcs[i].ServiceStatusProcess.dwCurrentState),
                  ServiceStartName(svcs[i].ServiceStatusProcess.dwServiceType & SERVICE_DRIVER
                                       ? SERVICE_DEMAND_START
                                       : svcs[i].ServiceStatusProcess.dwServiceType & SERVICE_BOOT_START
                                       ? SERVICE_BOOT_START
                                       : svcs[i].ServiceStatusProcess.dwServiceType & SERVICE_SYSTEM_START
                                       ? SERVICE_SYSTEM_START
                                       : svcs[i].ServiceStatusProcess.dwServiceType & SERVICE_AUTO_START
                                       ? SERVICE_AUTO_START
                                       : SERVICE_DEMAND_START),
                  svcs[i].ServiceStatusProcess.dwProcessId);
    }
    free(svcs);
    CloseServiceHandle(scm);
    ShowReport();
}

void DoServiceAction(const char *arg, int action, const char *actionName) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: services-%s <service-name>", actionName);
        ReportAdd("Example: services-%s spooler", actionName);
        ReportAdd("Tip: use 'services-list' to see service names.");
        ShowReport();
        return;
    }
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        ReportClear();
        ReportAdd("Cannot open Service Control Manager (error %lu).", GetLastError());
        ShowReport();
        return;
    }
    DWORD rights = SERVICE_QUERY_STATUS;
    switch (action) {
        case 0: rights = SERVICE_START; break;
        case 1: rights = SERVICE_STOP; break;
        case 2: rights = SERVICE_STOP | SERVICE_START; break;
        case 3:
        case 4: rights = SERVICE_CHANGE_CONFIG; break;
        case 5: rights = SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS; break;
    }
    SC_HANDLE svc = OpenServiceA(scm, arg, rights);
    if (!svc) {
        DWORD err = GetLastError();
        CloseServiceHandle(scm);
        if (err == ERROR_ACCESS_DENIED) {
            ReportClear();
            ReportAdd("Access denied for '%s'. Retrying elevated...", arg);
            ShowReport();
            char cli[600];
            sprintf(cli, "services-%s %s", actionName, arg);
            ElevateSelf(cli);
            return;
        }
        ReportClear();
        ReportAdd("Service '%s' not found (error %lu).", arg, err);
        ReportAdd("Tip: use 'services-list' to see service names.");
        ShowReport();
        return;
    }
    ReportClear();
    BOOL ok = FALSE;
    SERVICE_STATUS ss;
    switch (action) {
        case 0:
            ok = StartServiceA(svc, 0, NULL);
            if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
                ReportAdd("Service '%s' is already running.", arg);
                ok = TRUE;
            }
            break;
        case 1:
            ok = ControlService(svc, SERVICE_CONTROL_STOP, &ss);
            break;
        case 2: {
            ControlService(svc, SERVICE_CONTROL_STOP, &ss);
            Sleep(500);
            ok = StartServiceA(svc, 0, NULL);
            break;
        }
        case 3:
            ok = ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START,
                                      SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL);
            break;
        case 4:
            ok = ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                                      SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL);
            break;
        case 5: {
            DWORD bytes = 0;
            QueryServiceConfigA(svc, NULL, 0, &bytes);
            if (bytes > 0) {
                QUERY_SERVICE_CONFIGA *cfg = (QUERY_SERVICE_CONFIGA *)malloc(bytes);
                if (cfg && QueryServiceConfigA(svc, cfg, bytes, &bytes)) {
                    SERVICE_STATUS_PROCESS ssp;
                    DWORD needed = 0;
                    if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                                             sizeof ssp, &needed)) {
                        ReportAdd("Service: %s", arg);
                        ReportAdd("State  : %s", ServiceStateName(ssp.dwCurrentState));
                        ReportAdd("Start  : %s", ServiceStartName(cfg->dwStartType));
                        ReportAdd("PID    : %lu", ssp.dwProcessId);
                        ReportAdd("Binary : %s", cfg->lpBinaryPathName);
                    }
                }
                free(cfg);
            }
            ok = TRUE;
            break;
        }
    }
    if (!ok)
        ReportAdd("Action '%s' failed for service '%s' (error %lu).",
                  actionName, arg, GetLastError());
    else if (action != 5)
        ReportAdd("Service '%s': %s done.", arg, actionName);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    ShowReport();
}

void OnServiceStart(const char *arg) { DoServiceAction(arg, 0, "start"); }
void OnServiceStop(const char *arg) { DoServiceAction(arg, 1, "stop"); }
void OnServiceRestart(const char *arg) { DoServiceAction(arg, 2, "restart"); }
void OnServiceEnable(const char *arg) { DoServiceAction(arg, 3, "enable"); }
void OnServiceDisable(const char *arg) { DoServiceAction(arg, 4, "disable"); }
void OnServiceQuery(const char *arg) { DoServiceAction(arg, 5, "query"); }

void OnProcessesList(const char *arg) { (void)arg;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        ReportClear();
        ReportAdd("Failed to snapshot processes (error %lu).", GetLastError());
        ShowReport();
        return;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    int count = 0;
    ReportClear();
    ReportAdd("Running processes:");
    ReportAdd("  %-32s %8s %6s %8s", "Name", "PID", "Threads", "Parent");
    if (Process32FirstW(snap, &pe)) {
        do {
            ReportAdd("  %-32ls %8lu %6lu %8lu",
                      pe.szExeFile, pe.th32ProcessID, pe.cntThreads, pe.th32ParentProcessID);
            count++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    ReportAdd("Total: %d processes", count);
    ShowReport();
}

void OnProcessCount(const char *arg) { (void)arg;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        ReportClear();
        ReportAdd("Failed to snapshot processes (error %lu).", GetLastError());
        ShowReport();
        return;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    int count = 0;
    if (Process32FirstW(snap, &pe)) {
        do { count++; } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    ReportClear();
    ReportAdd("Total running processes: %d", count);
    ShowReport();
}

static int KillPid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return 0;
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok;
}

void OnProcessKill(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: processes-kill <process-name> or <PID>");
        ReportAdd("Example: processes-kill notepad.exe");
        ShowReport();
        return;
    }
    ReportClear();
    if (arg[0] >= '0' && arg[0] <= '9') {
        DWORD pid = (DWORD)atol(arg);
        if (KillPid(pid)) {
            ReportAdd("Terminated process with PID %lu.", pid);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED) {
                ReportAdd("Access denied for PID %lu. Retrying elevated...", pid);
                char cli[600];
                sprintf(cli, "processes-kill %lu", pid);
                ShowReport();
                ElevateSelf(cli);
                return;
            }
            ReportAdd("Failed to terminate PID %lu (error %lu).", pid, err);
        }
        ShowReport();
        return;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        ReportAdd("Failed to snapshot processes (error %lu).", GetLastError());
        ShowReport();
        return;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    wchar_t wname[256];
    MultiByteToWideChar(CP_ACP, 0, arg, -1, wname, 256);
    int found = 0, killed = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wname) == 0) {
                found = 1;
                if (KillPid(pe.th32ProcessID)) killed++;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found)
        ReportAdd("No process named '%s' is running.", arg);
    else if (killed == 0) {
        ReportAdd("Could not terminate '%s' (access denied). Retrying elevated...", arg);
        char cli[600];
        sprintf(cli, "processes-kill %s", arg);
        ShowReport();
        ElevateSelf(cli);
        return;
    } else
        ReportAdd("Terminated %d process(es) named '%s'.", killed, arg);
    ShowReport();
}

void OnProcessKillAll(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: processes-kill-all <process-name>");
        ReportAdd("Example: processes-kill-all notepad.exe");
        ShowReport();
        return;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        ReportClear();
        ReportAdd("Failed to snapshot processes (error %lu).", GetLastError());
        ShowReport();
        return;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    wchar_t wname[256];
    MultiByteToWideChar(CP_ACP, 0, arg, -1, wname, 256);
    int found = 0, killed = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wname) == 0) {
                found = 1;
                if (KillPid(pe.th32ProcessID)) killed++;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    ReportClear();
    if (!found)
        ReportAdd("No process named '%s' is running.", arg);
    else if (killed == 0) {
        ReportAdd("Could not terminate '%s' (access denied). Retrying elevated...", arg);
        char cli[600];
        sprintf(cli, "processes-kill-all %s", arg);
        ShowReport();
        ElevateSelf(cli);
        return;
    } else
        ReportAdd("Terminated %d process(es) named '%s'.", killed, arg);
    ShowReport();
}

void OnStartupList(const char *arg) { (void)arg;
    ReportClear();
    ReportAdd("Startup entries:");
    struct { HKEY root; const char *name; } roots[] = {
        { HKEY_CURRENT_USER, "HKCU" },
        { HKEY_LOCAL_MACHINE, "HKLM" },
    };
    const char *runKeys[2];
    runKeys[0] = "Software\\Microsoft\\Windows\\CurrentVersion";
    runKeys[1] = runKeys[0];
    for (int r = 0; r < 2; r++) {
        for (int k = 0; k < 2; k++) {
            char full[160];
            snprintf(full, sizeof full, "%s%s", runKeys[k], k == 1 ? "\\RunOnce" : "\\Run");
            HKEY hk;
            if (RegOpenKeyExA(roots[r].root, full, 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
                continue;
            DWORD idx = 0;
            char vn[1024];
            BYTE vd[4096];
            DWORD vns, vds, vt;
            for (;;) {
                vns = sizeof vn;
                vds = sizeof vd;
                LONG q = RegEnumValueA(hk, idx++, vn, &vns, NULL, &vt, vd, &vds);
                if (q != ERROR_SUCCESS) break;
                vd[sizeof vd - 1] = 0;
                ReportAdd("  [%s\\%s] %s = %s", roots[r].name, k == 1 ? "RunOnce" : "Run", vn, (char *)vd);
            }
            RegCloseKey(hk);
        }
    }
    char startup[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, SHGFP_TYPE_CURRENT, startup);
    char pattern[MAX_PATH];
    sprintf(pattern, "%s\\*.*", startup);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                ReportAdd("  [Startup Folder] %s", fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    ShowReport();
}

void OnStartupRemove(const char *arg) {
    if (!arg || !arg[0]) {
        ReportClear();
        ReportAdd("Usage: startup-remove <value-name>");
        ReportAdd("Example: startup-remove OneDrive");
        ReportAdd("Tip: use 'startup-list' to see entry names.");
        ShowReport();
        return;
    }
    int removed = 0;
    char runKeys[2][160];
    for (int k = 0; k < 2; k++) {
        snprintf(runKeys[k], sizeof runKeys[k], "Software\\Microsoft\\Windows\\CurrentVersion%s",
                 k == 1 ? "\\RunOnce" : "\\Run");
        if (DeleteRegValue(HKEY_CURRENT_USER, runKeys[k], arg)) removed++;
        if (DeleteRegValue(HKEY_LOCAL_MACHINE, runKeys[k], arg)) removed++;
    }
    ReportClear();
    if (removed)
        ReportAdd("Removed '%s' from %d startup location(s).", arg, removed);
    else
        ReportAdd("Entry '%s' not found in the startup list.", arg);
    ShowReport();
}

typedef LONG (WINAPI *RtlGetVersionFn)(PRTL_OSVERSIONINFOW);

void OnSysInfoOs(const char *arg) { (void)arg;
    char prod[512] = "?", edition[128] = "?", buildLab[256] = "?", dispVer[128] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               "ProductName", prod, sizeof prod);
    ReadRegStr(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               "EditionID", edition, sizeof edition);
    ReadRegStr(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               "BuildLabEx", buildLab, sizeof buildLab);
    ReadRegStr(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               "DisplayVersion", dispVer, sizeof dispVer);
    OSVERSIONINFOW ovi;
    memset(&ovi, 0, sizeof ovi);
    ovi.dwOSVersionInfoSize = sizeof ovi;
    RtlGetVersionFn fn = (RtlGetVersionFn)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (fn) fn(&ovi);
    ReportClear();
    ReportAdd("Windows OS Information");
    ReportAdd("Product  : %s", prod);
    ReportAdd("Edition  : %s", edition);
    ReportAdd("Version  : %lu.%lu (build %lu)",
              ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber);
    if (dispVer[0] && strcmp(dispVer, "?") != 0)
        ReportAdd("Release  : %s", dispVer);
    ReportAdd("BuildLab : %s", buildLab);
    ReportAdd("Arch     : %s", sizeof(void *) == 8 ? "x64" : "x86");
    ShowReport();
}

void OnSysInfoCpu(const char *arg) { (void)arg;
    char name[512] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE,
               "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
               "ProcessorNameString", name, sizeof name);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const char *arch = "?";
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_ARM: arch = "arm"; break;
    }
    ReportClear();
    ReportAdd("CPU Information");
    ReportAdd("Processor : %s", name);
    ReportAdd("Processors: %lu", si.dwNumberOfProcessors);
    ReportAdd("Arch      : %s", arch);
    ReportAdd("Page size : %lu bytes", si.dwPageSize);
    ShowReport();
}

void OnSysInfoRam(const char *arg) { (void)arg;
    MEMORYSTATUSEX mse;
    memset(&mse, 0, sizeof mse);
    mse.dwLength = sizeof mse;
    GlobalMemoryStatusEx(&mse);
    char t1[64], t2[64];
    FormatSize(mse.ullTotalPhys, t1, sizeof t1);
    FormatSize(mse.ullAvailPhys, t2, sizeof t2);
    char t3[64], t4[64];
    FormatSize(mse.ullTotalPageFile, t3, sizeof t3);
    FormatSize(mse.ullAvailPageFile, t4, sizeof t4);
    ReportClear();
    ReportAdd("RAM Information");
    ReportAdd("Total     : %s", t1);
    ReportAdd("Available : %s", t2);
    ReportAdd("In use    : %lu%%", mse.dwMemoryLoad);
    ReportAdd("Commit    : %s of %s", t4, t3);
    ShowReport();
}

void OnSysInfoDisk(const char *arg) { (void)arg;
    ReportClear();
    ReportAdd("Disk drives:");
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(drives & (1 << i))) continue;
        char root[4];
        sprintf(root, "%c:\\", 'A' + i);
        ULARGE_INTEGER freeAvail, total, freeTotal;
        if (GetDiskFreeSpaceExA(root, &freeAvail, &total, &freeTotal)) {
            char t1[64], t2[64];
            FormatSize(freeAvail.QuadPart, t1, sizeof t1);
            FormatSize(total.QuadPart, t2, sizeof t2);
            int pct = total.QuadPart ? (int)(100.0 * freeAvail.QuadPart / total.QuadPart) : 0;
            ReportAdd("  %s  free %s / total %s (%d%% free)", root, t1, t2, pct);
        } else {
            ReportAdd("  %s  not accessible", root);
        }
    }
    ShowReport();
}

void OnSysInfoUptime(const char *arg) { (void)arg;
    ULONGLONG ms = GetTickCount64();
    ULONGLONG d = ms / 86400000ULL;
    ms %= 86400000ULL;
    ULONGLONG h = ms / 3600000ULL;
    ms %= 3600000ULL;
    ULONGLONG m = ms / 60000ULL;
    ms %= 60000ULL;
    ULONGLONG s = ms / 1000ULL;
    ReportClear();
    ReportAdd("System uptime: %llu days, %02llu:%02llu:%02llu", d, h, m, s);
    ShowReport();
}

void OnSysInfoUser(const char *arg) { (void)arg;
    char user[256] = "?", comp[256] = "?", domain[256] = "?";
    DWORD sz = sizeof user;
    if (GetUserNameA(user, &sz)) user[sizeof user - 1] = 0;
    sz = sizeof comp;
    if (GetComputerNameA(comp, &sz)) comp[sizeof comp - 1] = 0;
    sz = sizeof domain;
    if (GetComputerNameExA(ComputerNameDnsDomain, domain, &sz) && domain[0])
        domain[sizeof domain - 1] = 0;
    else
        strcpy(domain, "(workgroup)");
    ReportClear();
    ReportAdd("User Information");
    ReportAdd("User      : %s", user);
    ReportAdd("Computer  : %s", comp);
    ReportAdd("Domain    : %s", domain);
    ShowReport();
}

void OnSysInfoBattery(const char *arg) { (void)arg;
    SYSTEM_POWER_STATUS sps;
    ReportClear();
    if (!GetSystemPowerStatus(&sps)) {
        ReportAdd("Could not read power status.");
        ShowReport();
        return;
    }
    ReportAdd("Battery / Power Status");
    if (sps.ACLineStatus == 1)
        ReportAdd("AC power : on (plugged in)");
    else if (sps.ACLineStatus == 0)
        ReportAdd("AC power : off (on battery)");
    else
        ReportAdd("AC power : unknown");
    if (sps.BatteryLifePercent != 255)
        ReportAdd("Charge   : %u%%", sps.BatteryLifePercent);
    else
        ReportAdd("Charge   : unknown (no battery)");
    if (sps.BatteryFlag & 8)
        ReportAdd("State    : charging");
    else if (sps.BatteryFlag & 1)
        ReportAdd("State    : high (ok)");
    else if (sps.BatteryFlag & 4)
        ReportAdd("State    : critical");
    else
        ReportAdd("State    : ok");
    if (sps.BatteryLifeTime != (DWORD)-1) {
        DWORD t = sps.BatteryLifeTime;
        ReportAdd("Remaining: %lu:%02lu minutes", t / 60, t % 60);
    }
    ShowReport();
}

void OnInstalledApps(const char *arg) { (void)arg;
    ReportClear();
    ReportAdd("Installed applications:");
    struct { HKEY root; const char *name; } roots[] = {
        { HKEY_LOCAL_MACHINE, "HKLM" },
        { HKEY_CURRENT_USER, "HKCU" },
    };
    const char *subkeys[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };
    int total = 0;
    for (int r = 0; r < 2; r++) {
        for (int s = 0; s < 2; s++) {
            HKEY hk;
            if (RegOpenKeyExA(roots[r].root, subkeys[s], 0, KEY_ENUMERATE_SUB_KEYS, &hk)
                != ERROR_SUCCESS)
                continue;
            DWORD idx = 0;
            char name[1024];
            DWORD nsz;
            for (;;) {
                nsz = sizeof name;
                LONG q = RegEnumKeyExA(hk, idx++, name, &nsz, NULL, NULL, NULL, NULL);
                if (q != ERROR_SUCCESS) break;
                char sub[1600];
                sprintf(sub, "%s\\%s", subkeys[s], name);
                char disp[512], ver[128];
                if (!ReadRegStr(roots[r].root, sub, "DisplayName", disp, sizeof disp))
                    continue;
                if (!ReadRegStr(roots[r].root, sub, "DisplayVersion", ver, sizeof ver))
                    strcpy(ver, "");
                ReportAdd("  %s  (v%s)  [%s]", disp, ver[0] ? ver : "?", roots[r].name);
                total++;
                if (total >= 1500) break;
            }
            RegCloseKey(hk);
            if (total >= 1500) break;
        }
        if (total >= 1500) break;
    }
    ReportAdd("Total: %d applications", total);
    ShowReport();
}

void OnHotfixList(const char *arg) { (void)arg;
    ReportClear();
    ReportAdd("Installed Windows updates (hotfixes):");
    const char *sub = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix";
    HKEY hk;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, sub, 0, KEY_ENUMERATE_SUB_KEYS, &hk) != ERROR_SUCCESS) {
        ReportAdd("Hotfix key not found.");
        ShowReport();
        return;
    }
    DWORD idx = 0;
    char name[1024];
    DWORD nsz;
    int total = 0;
    for (;;) {
        nsz = sizeof name;
        LONG q = RegEnumKeyExA(hk, idx++, name, &nsz, NULL, NULL, NULL, NULL);
        if (q != ERROR_SUCCESS) break;
        char fsub[1200];
        sprintf(fsub, "%s\\%s", sub, name);
        char installed[64] = "";
        ReadRegStr(HKEY_LOCAL_MACHINE, fsub, "InstalledBy", installed, sizeof installed);
        ReportAdd("  %s  (installed by %s)", name, installed[0] ? installed : "Windows Update");
        total++;
    }
    RegCloseKey(hk);
    ReportAdd("Total: %d hotfixes", total);
    ShowReport();
}

void OnSysInfoOverview(const char *arg) { (void)arg;
    char prod[512] = "?", cpu[512] = "?", manuf[256] = "?", model[256] = "?",
         bios[256] = "?", user[256] = "?", comp[256] = "?", gpu[256] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
               "ProductName", prod, sizeof prod);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
               "ProcessorNameString", cpu, sizeof cpu);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "SystemManufacturer", manuf, sizeof manuf);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "SystemProductName", model, sizeof model);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "BIOSVendor", bios, sizeof bios);
    ReadRegStr(HKEY_LOCAL_MACHINE,
               "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000",
               "DriverDesc", gpu, sizeof gpu);
    DWORD sz = sizeof user;
    GetUserNameA(user, &sz);
    sz = sizeof comp;
    GetComputerNameA(comp, &sz);
    MEMORYSTATUSEX mse;
    memset(&mse, 0, sizeof mse);
    mse.dwLength = sizeof mse;
    GlobalMemoryStatusEx(&mse);
    char ram[64], disk[64];
    FormatSize(mse.ullTotalPhys, ram, sizeof ram);
    ULARGE_INTEGER freeAvail, total, freeTotal;
    if (GetDiskFreeSpaceExA("C:\\", &freeAvail, &total, &freeTotal))
        FormatSize(total.QuadPart, disk, sizeof disk);
    else
        strcpy(disk, "?");
    OSVERSIONINFOW ovi;
    memset(&ovi, 0, sizeof ovi);
    ovi.dwOSVersionInfoSize = sizeof ovi;
    RtlGetVersionFn fn = (RtlGetVersionFn)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (fn) fn(&ovi);
    ReportClear();
    ReportAdd("System Overview");
    ReportAdd("Computer : %s (%s)", comp, user);
    ReportAdd("OS       : %s (build %lu)", prod, ovi.dwBuildNumber);
    ReportAdd("CPU      : %s", cpu);
    ReportAdd("RAM      : %s", ram);
    ReportAdd("Disk C:  : %s", disk);
    ReportAdd("GPU      : %s", gpu);
    ReportAdd("Motherboard: %s %s", manuf, model);
    ReportAdd("BIOS     : %s", bios);
    ReportAdd("Uptime   : %llu hours",
              GetTickCount64() / 3600000ULL);
    ShowReport();
}

void OnSysInfoGpu(const char *arg) { (void)arg;
    char gpu[512] = "?", vid[256] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE,
               "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000",
               "DriverDesc", gpu, sizeof gpu);
    ReadRegStr(HKEY_LOCAL_MACHINE,
               "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}\\0000",
               "ProviderName", vid, sizeof vid);
    ReportClear();
    ReportAdd("GPU Information");
    ReportAdd("GPU      : %s", gpu);
    ReportAdd("Provider : %s", vid);
    ShowReport();
}

void OnSysInfoMotherboard(const char *arg) { (void)arg;
    char manuf[256] = "?", model[256] = "?", ver[256] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "SystemManufacturer", manuf, sizeof manuf);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "SystemProductName", model, sizeof model);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "SystemVersion", ver, sizeof ver);
    ReportClear();
    ReportAdd("Motherboard Information");
    ReportAdd("Manufacturer : %s", manuf);
    ReportAdd("Model        : %s", model);
    ReportAdd("Version      : %s", ver);
    ShowReport();
}

void OnSysInfoBios(const char *arg) { (void)arg;
    char vendor[256] = "?", rel[256] = "?", date[256] = "?";
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "BIOSVendor", vendor, sizeof vendor);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "BIOSReleaseDate", date, sizeof date);
    ReadRegStr(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS",
               "BIOSVersion", rel, sizeof rel);
    ReportClear();
    ReportAdd("BIOS Information");
    ReportAdd("Vendor : %s", vendor);
    ReportAdd("Version: %s", rel);
    ReportAdd("Date   : %s", date);
    ShowReport();
}

void OnSysInfoNetwork(const char *arg) { (void)arg;
    ReportClear();
    ReportAdd("Network configuration (ipconfig /all):");
    FILE *p = _popen("ipconfig /all", "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof line, p)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r'))
                line[--l] = 0;
            ReportAdd("%s", line);
        }
        _pclose(p);
    } else {
        ReportAdd("Could not run ipconfig.");
    }
    ShowReport();
}

#define SIMPLE(fn, body) void fn(const char *arg) { (void)arg; body }

SIMPLE(OnSysProps, OpenCpl("sysdm.cpl");)
SIMPLE(OnSysAdvanced, OpenCpl("sysdm.cpl,,3");)
SIMPLE(OnSysEnvVar, OpenCpl("sysdm.cpl,,3");)
SIMPLE(OnSysPerfOpt, OpenCpl("sysdm.cpl,,2");)
SIMPLE(OnSysUserProf, OpenCpl("sysdm.cpl,,1");)
SIMPLE(OnSysStartRec, OpenCpl("sysdm.cpl,,4");)
SIMPLE(OnSysProtection, OpenCpl("sysdm.cpl,,4");)
SIMPLE(OnSysDevMgr, Run("devmgmt.msc", NULL, 0);)
SIMPLE(OnSysDiskMgmt, Run("diskmgmt.msc", NULL, 0);)
SIMPLE(OnSysCompMgmt, Run("compmgmt.msc", NULL, 0);)
SIMPLE(OnSysServices, Run("services.msc", NULL, 0);)
SIMPLE(OnSysTaskSchd, Run("taskschd.msc", NULL, 0);)
SIMPLE(OnSysEventVwr, Run("eventvwr.msc", NULL, 0);)
SIMPLE(OnSysRegedit, Run("regedit.exe", NULL, 0);)
SIMPLE(OnSysGpedit, Run("gpedit.msc", NULL, 0);)
SIMPLE(OnSysMsinfo, Run("msinfo32.exe", NULL, 0);)
SIMPLE(OnSysInfoCmd, RunCmd("systeminfo", 1);)
SIMPLE(OnSysDxdiag, Run("dxdiag.exe", NULL, 0);)
SIMPLE(OnSysTaskmgr, Run("taskmgr.exe", NULL, 0);)
SIMPLE(OnSysMsconfig, Run("msconfig.exe", NULL, 0);)
SIMPLE(OnSysWinver, Run("winver.exe", NULL, 0);)
SIMPLE(OnSysPerfMon, Run("perfmon.msc", NULL, 0);)
SIMPLE(OnSysResMon, Run("resmon.exe", NULL, 0);)
SIMPLE(OnSysRestartExplorer, RunCmd("taskkill /f /im explorer.exe & start explorer.exe", 0);)
SIMPLE(OnSysEmptyRecycle, SHEmptyRecycleBinA(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);)
SIMPLE(OnSysEditHosts, Run("notepad.exe", "C:\\Windows\\System32\\drivers\\etc\\hosts", 1);)
SIMPLE(OnSysPrinters, OpenSettings("printers");)
SIMPLE(OnSysTaskbar, OpenSettings("taskbar");)
SIMPLE(OnSysBluetooth, OpenSettings("bluetooth");)
SIMPLE(OnSysStorage, OpenSettings("storagesense");)
SIMPLE(OnSysClipboard, OpenSettings("clipboard");)
SIMPLE(OnSysAboutWin, OpenSettings("about");)
SIMPLE(OnSysSharedFolders, Run("fsmgmt.msc", NULL, 0);)
SIMPLE(OnSysPrintMgmt, Run("printmanagement.msc", NULL, 0);)

SIMPLE(OnStHome, OpenSettings("");)
SIMPLE(OnStDisplay, OpenSettings("display");)
SIMPLE(OnStSound, OpenSettings("sound");)
SIMPLE(OnStNotif, OpenSettings("notifications");)
SIMPLE(OnStPower, OpenSettings("powersleep");)
SIMPLE(OnStBatterySaver, OpenSettings("batterysaver");)
SIMPLE(OnStStorage, OpenSettings("storagesense");)
SIMPLE(OnStBluetooth, OpenSettings("bluetooth");)
SIMPLE(OnStPrinters, OpenSettings("printers");)
SIMPLE(OnStMouse, OpenSettings("mousetouchpad");)
SIMPLE(OnStTyping, OpenSettings("devices-typing");)
SIMPLE(OnStTouch, OpenSettings("devices-touch");)
SIMPLE(OnStUsb, OpenSettings("usb");)
SIMPLE(OnStPen, OpenSettings("pen");)
SIMPLE(OnStAutoPlay, OpenSettings("autoplay");)
SIMPLE(OnStNetwork, OpenSettings("network");)
SIMPLE(OnStWifi, OpenSettings("network-wifi");)
SIMPLE(OnStEthernet, OpenSettings("network-ethernet");)
SIMPLE(OnStCellular, OpenSettings("network-cellular");)
SIMPLE(OnStVpn, OpenSettings("network-vpn");)
SIMPLE(OnStProxy, OpenSettings("network-proxy");)
SIMPLE(OnStAirplane, OpenSettings("network-airplanemode");)
SIMPLE(OnStMobileHotspot, OpenSettings("network-mobilehotspot");)
SIMPLE(OnStDataUsage, OpenSettings("network-datausage");)
SIMPLE(OnStBackground, OpenSettings("personalization-background");)
SIMPLE(OnStColors, OpenSettings("colors");)
SIMPLE(OnStLockScreen, OpenSettings("lockscreen");)
SIMPLE(OnStThemes, OpenSettings("themes");)
SIMPLE(OnStStart, OpenSettings("personalization-start");)
SIMPLE(OnStTaskbar, OpenSettings("taskbar");)
SIMPLE(OnStApps, OpenSettings("appsfeatures");)
SIMPLE(OnStDefaultApps, OpenSettings("apps-defaultapps");)
SIMPLE(OnStAppsWebsites, OpenSettings("apps-appsforwebsites");)
SIMPLE(OnStOptFeat, OpenSettings("optionalfeatures");)
SIMPLE(OnStStartupApps, OpenSettings("startupapps");)
SIMPLE(OnStAccounts, OpenSettings("yourinfo");)
SIMPLE(OnStSignin, OpenSettings("accounts-signinoptions");)
SIMPLE(OnStHello, OpenSettings("accounts-windowshello");)
SIMPLE(OnStEmail, OpenSettings("accounts-emailandaccounts");)
SIMPLE(OnStWorkSchool, OpenSettings("accounts-accessworkorschool");)
SIMPLE(OnStOtherUsers, OpenSettings("accounts-otherusers");)
SIMPLE(OnStSync, OpenSettings("accounts-syncsettings");)
SIMPLE(OnStFamilies, OpenSettings("accounts-families");)
SIMPLE(OnStTime, OpenSettings("dateandtime");)
SIMPLE(OnStLanguage, OpenSettings("regionlanguage");)
SIMPLE(OnStGaming, OpenSettings("gaming-gamebar");)
SIMPLE(OnStGameMode, OpenSettings("gaming-gamemode");)
SIMPLE(OnStBroadcasting, OpenSettings("gaming-broadcasting");)
SIMPLE(OnStXboxNet, OpenSettings("gaming-xboxnetworking");)
SIMPLE(OnStAccess, OpenSettings("easeofaccess");)
SIMPLE(OnStMagnifier, OpenSettings("easeofaccess-magnifier");)
SIMPLE(OnStNarrator, OpenSettings("easeofaccess-narrator");)
SIMPLE(OnStHighContrast, OpenSettings("easeofaccess-highcontrast");)
SIMPLE(OnStClosedCaptions, OpenSettings("easeofaccess-closedcaptioning");)
SIMPLE(OnStKeyboardAccess, OpenSettings("easeofaccess-keyboard");)
SIMPLE(OnStMouseAccess, OpenSettings("easeofaccess-mouse");)
SIMPLE(OnStAudioAccess, OpenSettings("easeofaccess-audio");)
SIMPLE(OnStDisplayAccess, OpenSettings("easeofaccess-display");)
SIMPLE(OnStPrivacy, OpenSettings("privacy");)
SIMPLE(OnStLocation, OpenSettings("privacy-location");)
SIMPLE(OnStCamera, OpenSettings("privacy-camera");)
SIMPLE(OnStMicrophone, OpenSettings("privacy-microphone");)
SIMPLE(OnStSpeech, OpenSettings("privacy-speechtyping");)
SIMPLE(OnStActivityHistory, OpenSettings("privacy-activityhistory");)
SIMPLE(OnStBackgroundApps, OpenSettings("privacy-backgroundapps");)
SIMPLE(OnStDocuments, OpenSettings("privacy-documents");)
SIMPLE(OnStDownloads, OpenSettings("privacy-downloadsfolder");)
SIMPLE(OnStPictures, OpenSettings("privacy-pictures");)
SIMPLE(OnStVideos, OpenSettings("privacy-videos");)
SIMPLE(OnStClipboard, OpenSettings("clipboard");)
SIMPLE(OnStFocus, OpenSettings("quiethours");)
SIMPLE(OnStProject, OpenSettings("project");)
SIMPLE(OnStTablet, OpenSettings("tabletmode");)
SIMPLE(OnStMultitasking, OpenSettings("multitasking");)
SIMPLE(OnStRemoteDesktop, OpenSettings("system-remotedesktop");)
SIMPLE(OnStTrouble, OpenSettings("troubleshoot");)
SIMPLE(OnStRecovery, OpenSettings("recovery");)
SIMPLE(OnStUpdate, OpenSettings("windowsupdate");)
SIMPLE(OnStBackup, OpenSettings("backup");)
SIMPLE(OnStAboutWin, OpenSettings("about");)
SIMPLE(OnStPhoneLink, OpenSettings("mobile-devices");)
SIMPLE(OnStNearby, OpenSettings("crossdevice");)
SIMPLE(OnStNightLight, OpenSettings("nightlight");)
SIMPLE(OnStGpu, OpenSettings("display-advancedgraphics");)
SIMPLE(OnStDisplayAdvanced, OpenSettings("display-advanced");)
SIMPLE(OnStCaptures, OpenSettings("gaming-captures");)
SIMPLE(OnStAppVolume, OpenSettings("apps-volume");)
SIMPLE(OnStCalendar, OpenSettings("privacy-calendar");)
SIMPLE(OnStContacts, OpenSettings("privacy-contacts");)
SIMPLE(OnStEmailP, OpenSettings("privacy-email");)
SIMPLE(OnStCallHistory, OpenSettings("privacy-call-history");)
SIMPLE(OnStRadios, OpenSettings("privacy-radios");)
SIMPLE(OnStNotifP, OpenSettings("privacy-notifications");)
SIMPLE(OnStFeedback, OpenSettings("privacy-feedback");)
SIMPLE(OnStDiagnostics, OpenSettings("privacy-diagnostics");)
SIMPLE(OnStNetAdvanced, OpenSettings("network-advanced-settings");)
SIMPLE(OnStAutoDl, OpenSettings("privacy-automatic-file-downloads");)
SIMPLE(OnSP_windows_update_history, OpenSettings("windowsupdate-history");)
SIMPLE(OnSP_windows_update_restart_options, OpenSettings("windowsupdate-restartoptions");)
SIMPLE(OnSP_windows_update_optional, OpenSettings("windowsupdate-optionalupdates");)
SIMPLE(OnSP_windows_update_advanced, OpenSettings("windowsupdate-advancedoptions");)
SIMPLE(OnSP_windows_update_active_hours, OpenSettings("windowsupdate-activehours");)
SIMPLE(OnSP_windows_update_pause, OpenSettings("windowsupdate-pause");)
SIMPLE(OnSP_windows_update_delivery_optimization, OpenSettings("windowsupdate-deliveryoptimization");)
SIMPLE(OnSP_windows_update_targeted_version, OpenSettings("windowsupdate-targetedversion");)
SIMPLE(OnSP_windows_update_troubleshoot, OpenSettings("windowsupdate-troubleshoot");)
SIMPLE(OnSP_start_layout, OpenSettings("personalization-start");)
SIMPLE(OnSP_personalization_taskbar, OpenSettings("personalization-taskbar");)
SIMPLE(OnSP_accent_colors, OpenSettings("personalization-colors");)
SIMPLE(OnSP_desktop_background, OpenSettings("personalization-background");)
SIMPLE(OnSP_lockscreen_detailed, OpenSettings("personalization-lockscreen");)
SIMPLE(OnSP_personalization_fonts, OpenSettings("personalization-fonts");)
SIMPLE(OnSP_personalization_tasks, OpenSettings("personalization-tasks");)
SIMPLE(OnSP_dynamic_lock, OpenSettings("personalization-dynamiclock");)
SIMPLE(OnSP_privacy_activity_history, OpenSettings("privacy-activityhistory");)
SIMPLE(OnSP_location_privacy, OpenSettings("privacy-location");)
SIMPLE(OnSP_camera_privacy, OpenSettings("privacy-webcam");)
SIMPLE(OnSP_microphone_privacy, OpenSettings("privacy-microphone");)
SIMPLE(OnSP_privacy_voice_activation, OpenSettings("privacy-voiceactivation");)
SIMPLE(OnSP_speech_privacy, OpenSettings("privacy-speech");)
SIMPLE(OnSP_privacy_account_info, OpenSettings("privacy-accountinfo");)
SIMPLE(OnSP_privacy_tasks, OpenSettings("privacy-tasks");)
SIMPLE(OnSP_privacy_messaging, OpenSettings("privacy-messaging");)
SIMPLE(OnSP_privacy_other_devices, OpenSettings("privacy-otherdevices");)
SIMPLE(OnSP_privacy_app_diagnostics, OpenSettings("privacy-appdiagnostics");)
SIMPLE(OnSP_documents_privacy, OpenSettings("privacy-documents");)
SIMPLE(OnSP_pictures_privacy, OpenSettings("privacy-pictures");)
SIMPLE(OnSP_videos_privacy, OpenSettings("privacy-videos");)
SIMPLE(OnSP_privacy_notifications, OpenSettings("privacy-notifications");)
SIMPLE(OnSP_privacy_screenshots, OpenSettings("privacy-screenshots");)
SIMPLE(OnSP_background_apps_privacy, OpenSettings("privacy-backgroundapps");)
SIMPLE(OnSP_privacy_devices, OpenSettings("privacy-devices");)
SIMPLE(OnSP_privacy_graphics_capture, OpenSettings("privacy-graphicscapture");)
SIMPLE(OnSP_privacy_search, OpenSettings("privacy-search");)
SIMPLE(OnSP_privacy_broadcasting, OpenSettings("privacy-broadcasting");)
SIMPLE(OnSP_account_your_info, OpenSettings("yourinfo");)
SIMPLE(OnSP_account_passwordless, OpenSettings("passwordless");)
SIMPLE(OnSP_account_payment, OpenSettings("payment");)
SIMPLE(OnSP_family_group, OpenSettings("familygroup");)
SIMPLE(OnSP_language_date_time, OpenSettings("dateandtime");)
SIMPLE(OnSP_language_region, OpenSettings("regionlanguage");)
SIMPLE(OnSP_language_region_format, OpenSettings("regionlanguage-format");)
SIMPLE(OnSP_language_options, OpenSettings("languageoptions");)
SIMPLE(OnSP_language_keyboard, OpenSettings("keyboard");)
SIMPLE(OnSP_speech_language, OpenSettings("speech");)
SIMPLE(OnSP_language_handwriting, OpenSettings("handwriting");)
SIMPLE(OnSP_typing_language, OpenSettings("typing");)
SIMPLE(OnSP_accessibility_display, OpenSettings("accessibility-display");)
SIMPLE(OnSP_accessibility_text_size, OpenSettings("accessibility-textsize");)
SIMPLE(OnSP_accessibility_visual_effects, OpenSettings("accessibility-visualeffects");)
SIMPLE(OnSP_accessibility_mouse_pointer, OpenSettings("accessibility-mousepointer");)
SIMPLE(OnSP_accessibility_text_cursor, OpenSettings("accessibility-textcursor");)
SIMPLE(OnSP_accessibility_color_filters, OpenSettings("accessibility-colorfilter");)
SIMPLE(OnSP_accessibility_keyboard, OpenSettings("accessibility-keyboard");)
SIMPLE(OnSP_accessibility_mouse, OpenSettings("accessibility-mouse");)
SIMPLE(OnSP_accessibility_eye_control, OpenSettings("accessibility-eyecontrol");)
SIMPLE(OnSP_accessibility_audio, OpenSettings("accessibility-audio");)
SIMPLE(OnSP_accessibility_captioning, OpenSettings("accessibility-captioning");)
SIMPLE(OnSP_accessibility_voice, OpenSettings("accessibility-voice");)
SIMPLE(OnSP_accessibility_privacy, OpenSettings("accessibility-privacy");)
SIMPLE(OnSP_accessibility_cursor, OpenSettings("accessibility-cursor");)
SIMPLE(OnSP_gaming_game_bar, OpenSettings("gaming-gamebar");)
SIMPLE(OnSP_gaming_game_dvr, OpenSettings("gaming-gamedvr");)
SIMPLE(OnSP_gaming_game_mode, OpenSettings("gaming-gamemode");)
SIMPLE(OnSP_gaming_controllers, OpenSettings("gaming-controllers");)
SIMPLE(OnSP_apps_installed, OpenSettings("appsfeatures");)
SIMPLE(OnSP_apps_default_browser, OpenSettings("appsdefaultapps-browser");)
SIMPLE(OnSP_apps_default_email, OpenSettings("appsdefaultapps-email");)
SIMPLE(OnSP_apps_default_media, OpenSettings("appsdefaultapps-media");)
SIMPLE(OnSP_apps_default_maps, OpenSettings("appsdefaultapps-maps");)
SIMPLE(OnSP_apps_default_music, OpenSettings("appsdefaultapps-music");)
SIMPLE(OnSP_apps_default_photos, OpenSettings("appsdefaultapps-photos");)
SIMPLE(OnSP_apps_default_video, OpenSettings("appsdefaultapps-video");)
SIMPLE(OnSP_apps_alerts, OpenSettings("apps-alerts");)
SIMPLE(OnSP_apps_startup, OpenSettings("appsstartup");)
SIMPLE(OnSP_apps_clipboard, OpenSettings("apps-clipboard");)
SIMPLE(OnSP_devices_bluetooth, OpenSettings("bluetoothdevices");)
SIMPLE(OnSP_devices_printers, OpenSettings("printers");)
SIMPLE(OnSP_devices_mouse, OpenSettings("devices-mouse");)
SIMPLE(OnSP_devices_touchpad, OpenSettings("devices-touchpad");)
SIMPLE(OnSP_devices_typing, OpenSettings("devices-typing");)
SIMPLE(OnSP_devices_pen, OpenSettings("devices-pen");)
SIMPLE(OnSP_devices_autoplay, OpenSettings("devices-autoplay");)
SIMPLE(OnSP_devices_multitasking, OpenSettings("devices-multitasking");)
SIMPLE(OnSP_devices_project, OpenSettings("projection");)
SIMPLE(OnSP_devices_cameras, OpenSettings("devices-cameras");)
SIMPLE(OnSP_devices_touch, OpenSettings("devices-touch");)
SIMPLE(OnSP_devices_usb, OpenSettings("devices-usb");)
SIMPLE(OnSP_network_wifi, OpenSettings("network-wifi");)
SIMPLE(OnSP_network_ethernet, OpenSettings("network-ethernet");)
SIMPLE(OnSP_network_vpn, OpenSettings("network-vpn");)
SIMPLE(OnSP_network_airplane, OpenSettings("network-airplanemode");)
SIMPLE(OnSP_network_mobile_hotspot, OpenSettings("network-mobilehotspot");)
SIMPLE(OnSP_network_proxy, OpenSettings("network-proxy");)
SIMPLE(OnSP_network_dialup, OpenSettings("network-dialup");)
SIMPLE(OnSP_network_cellular, OpenSettings("network-cellular");)
SIMPLE(OnSP_network_data_usage, OpenSettings("network-datausage");)
SIMPLE(OnSP_network_nearby, OpenSettings("network-nearby");)
SIMPLE(OnSP_system_storage_sense, OpenSettings("storagesense");)
SIMPLE(OnSP_system_storage_cleanup, OpenSettings("storagecleanup");)
SIMPLE(OnSP_system_storage_optimization, OpenSettings("storage-optimization");)
SIMPLE(OnSP_system_battery_saver, OpenSettings("batterysaver");)
SIMPLE(OnSP_system_battery_usage, OpenSettings("batterysaver-usage");)
SIMPLE(OnSP_system_power, OpenSettings("power");)
SIMPLE(OnSP_system_multitasking, OpenSettings("multitasking");)
SIMPLE(OnSP_system_multitasking_snap, OpenSettings("multitasking-snap");)
SIMPLE(OnSP_system_multitasking_display, OpenSettings("multitasking-multidisplay");)
SIMPLE(OnSP_system_clipboard, OpenSettings("clipboard");)
SIMPLE(OnSP_system_remote_desktop, OpenSettings("remote-desktop");)
SIMPLE(OnSP_system_recovery, OpenSettings("recovery");)
SIMPLE(OnSP_system_activation, OpenSettings("activation");)
SIMPLE(OnSP_system_troubleshooters, OpenSettings("troubleshooters");)
SIMPLE(OnSP_system_about, OpenSettings("about");)
SIMPLE(OnSP_system_backup, OpenSettings("backups");)
SIMPLE(OnSP_system_developers, OpenSettings("developers");)
SIMPLE(OnSP_sync, OpenSettings("sync");)
SIMPLE(OnSP_taskbar_settings, OpenSettings("taskbar");)
SIMPLE(OnSP_insider_program, OpenSettings("experience-insiderprogram");)
SIMPLE(OnSP_sound_devices, OpenSettings("sound-devices");)
SIMPLE(OnSP_windows_update_pause_reboot, OpenSettings("windowsupdate-pauserebooter");)
SIMPLE(OnSP_windows_update_reboot_schedule, OpenSettings("windowsupdate-rebootschedule");)
SIMPLE(OnSP_windows_update_reboot, OpenSettings("windowsupdate-reboot");)
SIMPLE(OnSP_windows_update_reboot_week, OpenSettings("windowsupdate-rebootweek");)
SIMPLE(OnSP_windows_update_reboot_day, OpenSettings("windowsupdate-rebootday");)
SIMPLE(OnSP_windows_update_reboot_time, OpenSettings("windowsupdate-reboottime");)
SIMPLE(OnSP_windows_update_restart, OpenSettings("windowsupdate-restart");)
SIMPLE(OnSP_windows_update_restart_required, OpenSettings("windowsupdate-restartrequired");)
SIMPLE(OnSP_windows_update_snooze, OpenSettings("windowsupdate-snooze");)
SIMPLE(OnSP_windows_update_snooze_reboot, OpenSettings("windowsupdate-snoozereboot");)
SIMPLE(OnSP_windows_update_install, OpenSettings("windowsupdate-install");)
SIMPLE(OnSP_windows_update_download, OpenSettings("windowsupdate-download");)
SIMPLE(OnSP_windows_update_uninstall, OpenSettings("windowsupdate-uninstall");)
SIMPLE(OnSP_windows_update_drivers, OpenSettings("windowsupdate-drivers");)
SIMPLE(OnSP_windows_update_hide, OpenSettings("windowsupdate-hide");)
SIMPLE(OnSP_windows_update_locked, OpenSettings("windowsupdate-locked");)
SIMPLE(OnSP_windows_update_manual, OpenSettings("windowsupdate-manual");)
SIMPLE(OnSP_windows_update_compliance, OpenSettings("windowsupdate-compliance");)
SIMPLE(OnSP_windows_update_preview, OpenSettings("windowsupdate-preview");)
SIMPLE(OnSP_windows_update_experimental, OpenSettings("windowsupdate-experimental");)
SIMPLE(OnSP_windows_update_cooldown, OpenSettings("windowsupdate-cooldown");)
SIMPLE(OnSP_windows_update_registry_recovery, OpenSettings("windowsupdate-registryrecovery");)
SIMPLE(OnSP_windows_update_delivery_optimization_advanced, OpenSettings("windowsupdate-deliveryoptimization-advanced");)
SIMPLE(OnSP_windows_update_delivery_optimization_activity, OpenSettings("windowsupdate-deliveryoptimization-activity");)
SIMPLE(OnSP_bluetooth_add_device, OpenSettings("bluetoothdevices-addevices");)
SIMPLE(OnSP_bluetooth_discovered, OpenSettings("bluetoothdevices-discovered");)
SIMPLE(OnSP_bluetooth_paired, OpenSettings("bluetoothdevices-paired");)
SIMPLE(OnSP_bluetooth_other_devices, OpenSettings("bluetoothdevices-otherdevices");)
SIMPLE(OnSP_bluetooth_firmware, OpenSettings("bluetoothdevices-firmware");)
SIMPLE(OnSP_network_adapters_advanced, OpenSettings("network-advancedsettings-adapters");)
SIMPLE(OnSP_network_dns_advanced, OpenSettings("network-advancedsettings-dns");)
SIMPLE(OnSP_network_proxy_advanced, OpenSettings("network-advancedsettings-proxy");)
SIMPLE(OnSP_network_manual_proxy, OpenSettings("network-manualproxy");)
SIMPLE(OnSP_network_auto_proxy, OpenSettings("network-autoproxy");)
SIMPLE(OnSP_network_pac_url, OpenSettings("network-pacurl");)
SIMPLE(OnSP_network_connection_details, OpenSettings("network-connectiondetails");)
SIMPLE(OnSP_network_sim, OpenSettings("network-sim");)
SIMPLE(OnSP_network_roaming, OpenSettings("network-roaming");)
SIMPLE(OnSP_network_wifi_settings, OpenSettings("network-wifisettings");)
SIMPLE(OnSP_devices_audio, OpenSettings("devices-audio");)
SIMPLE(OnSP_devices_display, OpenSettings("devices-display");)
SIMPLE(OnSP_devices_display_performance, OpenSettings("devices-display-perf");)
SIMPLE(OnSP_devices_nfc, OpenSettings("devices-nfc");)
SIMPLE(OnSP_devices_stylus, OpenSettings("devices-stylus");)
SIMPLE(OnSP_devices_android, OpenSettings("devices-android");)
SIMPLE(OnSP_devices_car, OpenSettings("devices-car");)
SIMPLE(OnSP_devices_device_properties, OpenSettings("devices-deviceproperties");)
SIMPLE(OnSP_devices_other_devices, OpenSettings("devices-otherdevices");)
SIMPLE(OnSP_devices_servicelist, OpenSettings("devices-servicelist");)
SIMPLE(OnSP_personalization_getting_started, OpenSettings("personalization-gettingstarted");)
SIMPLE(OnSP_personalization_tile_launcher, OpenSettings("personalization-tilelauncher");)
SIMPLE(OnSP_personalization_variants, OpenSettings("personalization-variants");)
SIMPLE(OnSP_personalization_pane, OpenSettings("personalization-pane");)
SIMPLE(OnSP_personalization_fon, OpenSettings("personalization-fon");)
SIMPLE(OnSP_sound_settings, OpenSettings("sound-settings");)
SIMPLE(OnSP_sound_volume, OpenSettings("sound-volume");)
SIMPLE(OnSP_sound_audio_bluetooth, OpenSettings("sound-audiobluetooth");)
SIMPLE(OnSP_sound_audio_usb, OpenSettings("sound-audiousb");)
SIMPLE(OnSP_sound_audio_output, OpenSettings("sound-audiooutput");)
SIMPLE(OnSP_sound_audio_input, OpenSettings("sound-audioinput");)
SIMPLE(OnSP_taskbar_personalize, OpenSettings("taskbar-personalize");)
SIMPLE(OnSP_taskbar_tray, OpenSettings("taskbar-tray");)
SIMPLE(OnSP_taskbar_behavior, OpenSettings("taskbar-behavior");)
SIMPLE(OnSP_taskbar_system_tray, OpenSettings("taskbar-systemtray");)
SIMPLE(OnSP_taskbar_notification_center, OpenSettings("taskbar-notificationcenter");)
SIMPLE(OnSP_taskbar_caption, OpenSettings("taskbar-caption");)
SIMPLE(OnSP_taskbar_search_settings, OpenSettings("taskbar-search");)
SIMPLE(OnSP_taskbar_corner, OpenSettings("taskbar-corner");)
SIMPLE(OnSP_default_apps_browser_desktop, OpenSettings("defaultapps-browserdesktop");)
SIMPLE(OnSP_default_apps_email_desktop, OpenSettings("defaultapps-emaildesktop");)
SIMPLE(OnSP_default_apps_media_desktop, OpenSettings("defaultapps-mediadesktop");)
SIMPLE(OnSP_default_apps_maps_desktop, OpenSettings("defaultapps-mapsdesktop");)
SIMPLE(OnSP_default_apps_music_desktop, OpenSettings("defaultapps-musicdesktop");)
SIMPLE(OnSP_default_apps_photos_desktop, OpenSettings("defaultapps-photosdesktop");)
SIMPLE(OnSP_default_apps_video_desktop, OpenSettings("defaultapps-videodesktop");)
SIMPLE(OnSP_default_apps_search, OpenSettings("defaultapps-search");)
SIMPLE(OnSP_printers_add, OpenSettings("printers-adddevice");)
SIMPLE(OnSP_printers_advanced, OpenSettings("printers-advanced");)
SIMPLE(OnSP_printers_default, OpenSettings("printers-default");)
SIMPLE(OnSP_printers_scanner, OpenSettings("printers-scanner");)
SIMPLE(OnSP_region_display_language, OpenSettings("regionlanguage-setdisplaylanguage");)
SIMPLE(OnSP_region_display_language_options, OpenSettings("regionlanguage-setdisplaylanguage-options");)
SIMPLE(OnSP_region_format_region, OpenSettings("regionlanguage-setregion");)
SIMPLE(OnSP_region_language_list, OpenSettings("regionlanguage-languages");)
SIMPLE(OnSP_cortana, OpenSettings("cortana");)
SIMPLE(OnSP_cortana_suggestions, OpenSettings("cortana-suggestions");)
SIMPLE(OnSP_cortana_account, OpenSettings("cortana-account");)
SIMPLE(OnSP_cortana_skills, OpenSettings("cortana-skills");)
SIMPLE(OnSP_cortana_notifications, OpenSettings("cortana-notifications");)
SIMPLE(OnSP_cortana_permissions, OpenSettings("cortana-permissions");)
SIMPLE(OnSP_holographic, OpenSettings("holographic");)
SIMPLE(OnSP_holographic_headset, OpenSettings("holographic-headset");)
SIMPLE(OnSP_holographic_display, OpenSettings("holographic-display");)
SIMPLE(OnSP_holographic_audio, OpenSettings("holographic-audio");)
SIMPLE(OnSP_holographic_room, OpenSettings("holographic-room");)
SIMPLE(OnSP_surfacehub, OpenSettings("surfacehub");)
SIMPLE(OnSP_surfacehub_calling, OpenSettings("surfacehub-calling");)
SIMPLE(OnSP_surfacehub_sessions, OpenSettings("surfacehub-sessions");)
SIMPLE(OnSP_surfacehub_device, OpenSettings("surfacehub-device");)
SIMPLE(OnSP_gaming_trueplay, OpenSettings("gaming-trueplay");)
SIMPLE(OnSP_gaming_audio_capture, OpenSettings("gaming-audiocapture");)
SIMPLE(OnSP_gaming_podcasts, OpenSettings("gaming-podcasts");)
SIMPLE(OnSP_storage_policies, OpenSettings("storagepolicies");)
SIMPLE(OnSP_storage_devices, OpenSettings("storagedevices");)
SIMPLE(OnSP_storage_usage, OpenSettings("storageusage");)
SIMPLE(OnSP_session, OpenSettings("session");)
SIMPLE(OnSP_session_devices, OpenSettings("session-devices");)
SIMPLE(OnSP_session_connect, OpenSettings("session-connect");)
SIMPLE(OnSP_tips, OpenSettings("tips");)
SIMPLE(OnSP_tips_getting_started, OpenSettings("tips-gettingstarted");)
SIMPLE(OnSP_shared_folders, OpenSettings("sharedfolders");)
SIMPLE(OnSP_shared_folders_pc, OpenSettings("sharedfolders-pc");)
SIMPLE(OnSP_crossrealm_devices, OpenSettings("crossrealmdevices");)
SIMPLE(OnSP_emergency_alerts, OpenSettings("emergencyalerts");)
SIMPLE(OnSP_phone, OpenSettings("phone");)
SIMPLE(OnSP_windows_anywhere, OpenSettings("windowsanywhere");)
SIMPLE(OnSP_workloads, OpenSettings("workloads");)
SIMPLE(OnSP_windows_update_optional_drivers, OpenSettings("windowsupdate-optionalupdates-drivers");)
SIMPLE(OnSP_windows_update_optional_apps, OpenSettings("windowsupdate-optionalupdates-apps");)
SIMPLE(OnSP_windows_update_optional_firmware, OpenSettings("windowsupdate-optionalupdates-firmware");)
SIMPLE(OnSP_windows_update_optional_security, OpenSettings("windowsupdate-optionalupdates-security");)
SIMPLE(OnSP_windows_update_optional_critical, OpenSettings("windowsupdate-optionalupdates-critical");)
SIMPLE(OnSP_windows_update_optional_bugfixes, OpenSettings("windowsupdate-optionalupdates-bugfixes");)
SIMPLE(OnSP_windows_update_optional_tools, OpenSettings("windowsupdate-optionalupdates-tools");)
SIMPLE(OnSP_windows_update_optional_compatibility, OpenSettings("windowsupdate-optionalupdates-compatibility");)
SIMPLE(OnSP_windows_update_optional_enhancements, OpenSettings("windowsupdate-optionalupdates-enhancements");)
SIMPLE(OnSP_windows_update_optional_featurepacks, OpenSettings("windowsupdate-optionalupdates-featurepacks");)
SIMPLE(OnSP_windows_update_optional_servicing, OpenSettings("windowsupdate-optionalupdates-servicing");)
SIMPLE(OnSP_windows_update_optional_previews, OpenSettings("windowsupdate-optionalupdates-previews");)
SIMPLE(OnSP_windows_update_optional_required, OpenSettings("windowsupdate-optionalupdates-required");)
SIMPLE(OnSP_windows_update_active_hours_options, OpenSettings("windowsupdate-activehoursoptions");)
SIMPLE(OnSP_windows_update_advanced_options_options, OpenSettings("windowsupdate-advancedoptions-options");)
SIMPLE(OnSP_windows_update_targeted_version_options, OpenSettings("windowsupdate-targetedversion-options");)
SIMPLE(OnSP_windows_update_troubleshoot_reset, OpenSettings("windowsupdate-troubleshootreset");)
SIMPLE(OnSP_windows_update_limit_connectivity, OpenSettings("windowsupdate-limitconnectivity");)
SIMPLE(OnSP_windows_update_suppress_driver_update, OpenSettings("windowsupdate-suppressdriverupdate");)
SIMPLE(OnSP_windows_update_usb_device, OpenSettings("windowsupdate-usbdevice");)
SIMPLE(OnSP_windows_update_logout, OpenSettings("windowsupdate-logout");)
SIMPLE(OnSP_windows_update_logon, OpenSettings("windowsupdate-logon");)
SIMPLE(OnSP_windows_update_mobile_hotspot, OpenSettings("windowsupdate-mobilehotspot");)
SIMPLE(OnSP_windows_update_saved, OpenSettings("windowsupdate-saved");)
SIMPLE(OnSP_windows_update_software_policy_notification, OpenSettings("windowsupdate-softwarepolicynotification");)
SIMPLE(OnSP_windows_update_stuck, OpenSettings("windowsupdate-stuck");)
SIMPLE(OnSP_windows_update_update_comp, OpenSettings("windowsupdate-updatecomp");)
SIMPLE(OnSP_bluetooth_device, OpenSettings("bluetoothdevice");)
SIMPLE(OnSP_bluetooth_settings, OpenSettings("bluetoothdevices-settings");)
SIMPLE(OnSP_bluetooth_events, OpenSettings("bluetoothdevices-events");)
SIMPLE(OnSP_network_connections, OpenSettings("network-connections");)
SIMPLE(OnSP_network_connectivity, OpenSettings("network-connectivity");)
SIMPLE(OnSP_network_devices, OpenSettings("network-devices");)
SIMPLE(OnSP_network_device_usage, OpenSettings("network-deviceusage");)
SIMPLE(OnSP_network_operator_messages, OpenSettings("network-operatormessages");)
SIMPLE(OnSP_network_wifi_hotspot, OpenSettings("network-wifi-hotspot");)
SIMPLE(OnSP_devices_tel, OpenSettings("devices-tel");)
SIMPLE(OnSP_devices_rsd, OpenSettings("devices-rsd");)
SIMPLE(OnSP_devices_telemetry, OpenSettings("devices-telemetry");)
SIMPLE(OnSP_devices_mobilephone, OpenSettings("devices-mobilephone");)
SIMPLE(OnSP_devices_sim, OpenSettings("devices-sim");)
SIMPLE(OnSP_devices_sim_settings, OpenSettings("devices-simsettings");)
SIMPLE(OnSP_apps_features_app, OpenSettings("appsfeatures-app");)
SIMPLE(OnSP_apps_features_advanced, OpenSettings("appsfeatures-advanced");)
SIMPLE(OnSP_apps_notification_settings, OpenSettings("appsnotificationsettings");)
SIMPLE(OnSP_apps_advanced_notification_settings, OpenSettings("appsadvancednotificationsettings");)
SIMPLE(OnSP_face_enrollment, OpenSettings("signinoptions-launchfaceenrollment");)
SIMPLE(OnSP_fingerprint_enrollment, OpenSettings("signinoptions-launchfingerprintenrollment");)
SIMPLE(OnSP_security_key_enrollment, OpenSettings("signinoptions-launchsecuritykeyenrollment");)
SIMPLE(OnSP_create_security_key, OpenSettings("signinoptions-createsecuritykey");)
SIMPLE(OnSP_family_sign_in, OpenSettings("familygroup-signin");)
SIMPLE(OnSP_family_manage, OpenSettings("familygroup-manage");)
SIMPLE(OnSP_family_invite, OpenSettings("familygroup-invite");)
SIMPLE(OnSP_family_group_options, OpenSettings("familygroup-options");)
SIMPLE(OnSP_family_suggestions, OpenSettings("familygroup-suggestions");)
SIMPLE(OnSP_family_account, OpenSettings("familygroup-account");)
SIMPLE(OnSP_region_relaunch, OpenSettings("regionlanguage-relaunch");)
SIMPLE(OnSP_region_language_options, OpenSettings("regionlanguage-languageoptions");)
SIMPLE(OnSP_region_set_region_options, OpenSettings("regionlanguage-setregion-options");)
SIMPLE(OnSP_easeofaccess_display, OpenSettings("easeofaccess-display");)
SIMPLE(OnSP_easeofaccess_narrator, OpenSettings("easeofaccess-narrator");)
SIMPLE(OnSP_easeofaccess_magnifier, OpenSettings("easeofaccess-magnifier");)
SIMPLE(OnSP_easeofaccess_closedcaptioning, OpenSettings("easeofaccess-closedcaptioning");)
SIMPLE(OnSP_easeofaccess_keyboard, OpenSettings("easeofaccess-keyboard");)
SIMPLE(OnSP_easeofaccess_mouse, OpenSettings("easeofaccess-mouse");)
SIMPLE(OnSP_easeofaccess_otheroptions, OpenSettings("easeofaccess-otheroptions");)
SIMPLE(OnSP_easeofaccess_highcontrast, OpenSettings("easeofaccess-highcontrast");)
SIMPLE(OnSP_easeofaccess_colorfilter, OpenSettings("easeofaccess-colorfilter");)
SIMPLE(OnSP_easeofaccess_cursor, OpenSettings("easeofaccess-cursor");)
SIMPLE(OnSP_easeofaccess_textcursor, OpenSettings("easeofaccess-textcursor");)
SIMPLE(OnSP_easeofaccess_eyecontrol, OpenSettings("easeofaccess-eyecontrol");)
SIMPLE(OnSP_easeofaccess_audio, OpenSettings("easeofaccess-audio");)
SIMPLE(OnSP_easeofaccess_speechrecognition, OpenSettings("easeofaccess-speechrecognition");)
SIMPLE(OnSP_easeofaccess_dictation, OpenSettings("easeofaccess-dictation");)
SIMPLE(OnSP_easeofaccess_visual, OpenSettings("easeofaccess-visual");)
SIMPLE(OnSP_easeofaccess_hearing, OpenSettings("easeofaccess-hearing");)
SIMPLE(OnSP_taskbar_corner_overflow, OpenSettings("taskbar-corneroverflow");)
SIMPLE(OnSP_taskbar_view, OpenSettings("taskbar-view");)
SIMPLE(OnSP_taskbar_widgets, OpenSettings("taskbar-widgets");)
SIMPLE(OnSP_taskbar_copilot, OpenSettings("taskbar-copilot");)
SIMPLE(OnSP_taskbar_pinned, OpenSettings("taskbar-pinned");)
SIMPLE(OnSP_printers_fax, OpenSettings("printers-fax");)
SIMPLE(OnSP_printers_shared, OpenSettings("printers-shared");)
SIMPLE(OnSP_printers_print_server, OpenSettings("printers-printserver");)
SIMPLE(OnSP_printers_fax_server, OpenSettings("printers-faxserver");)
SIMPLE(OnSP_printers_printer_server, OpenSettings("printers-printerserver");)
SIMPLE(OnSP_printers_print_queue, OpenSettings("printers-printqueue");)
SIMPLE(OnSP_cortana_language, OpenSettings("cortana-language");)
SIMPLE(OnSP_cortana_recent, OpenSettings("cortana-recent");)
SIMPLE(OnSP_holographic_apps, OpenSettings("holographic-apps");)
SIMPLE(OnSP_holographic_environment, OpenSettings("holographic-environment");)
SIMPLE(OnSP_surfacehub_wifi, OpenSettings("surfacehub-wifi");)
SIMPLE(OnSP_surfacehub_casual, OpenSettings("surfacehub-casual");)
SIMPLE(OnSP_workloads_ai, OpenSettings("workloads-ai");)
SIMPLE(OnSP_workloads_auto_settings, OpenSettings("workloads-autosettings");)
SIMPLE(OnSP_workloads_search, OpenSettings("workloads-search");)
SIMPLE(OnSP_workloads_apps, OpenSettings("workloads-apps");)
SIMPLE(OnSP_workloads_data, OpenSettings("workloads-data");)
SIMPLE(OnSP_workloads_devices, OpenSettings("workloads-devices");)
SIMPLE(OnSP_workloads_developers, OpenSettings("workloads-developers");)
SIMPLE(OnSP_workloads_security, OpenSettings("workloads-security");)
SIMPLE(OnSP_wormhole, OpenSettings("wormhole");)
SIMPLE(OnSP_wormhole_account, OpenSettings("wormhole-account");)
SIMPLE(OnSP_wormhole_device, OpenSettings("wormhole-device");)
SIMPLE(OnSP_wormhole_bridge, OpenSettings("wormhole-bridge");)
SIMPLE(OnSP_wormhole_display, OpenSettings("wormhole-display");)
SIMPLE(OnSP_wormhole_experience, OpenSettings("wormhole-experience");)
SIMPLE(OnSP_wormhole_security, OpenSettings("wormhole-security");)
SIMPLE(OnSP_wormhole_settings, OpenSettings("wormhole-settings");)
SIMPLE(OnSP_clusters, OpenSettings("clusters");)
SIMPLE(OnSP_clusters_keys, OpenSettings("clusters-keys");)
SIMPLE(OnSP_clusters_resources, OpenSettings("clusters-resources");)
SIMPLE(OnSP_clusters_health, OpenSettings("clusters-health");)
SIMPLE(OnSP_windows_anywhere_mobile, OpenSettings("windowsanywhere-mobile");)
SIMPLE(OnSP_windows_anywhere_account, OpenSettings("windowsanywhere-account");)
SIMPLE(OnSP_windows_anywhere_options, OpenSettings("windowsanywhere-options");)
SIMPLE(OnSP_windows_anywhere_devices, OpenSettings("windowsanywhere-devices");)
SIMPLE(OnSP_session_disk, OpenSettings("session-disk");)
SIMPLE(OnSP_tips_feedback, OpenSettings("tips-feedback");)
SIMPLE(OnSP_tips_settings, OpenSettings("tips-settings");)
SIMPLE(OnSP_maps, OpenSettings("maps");)
SIMPLE(OnSP_maps_download_maps, OpenSettings("maps-downloadmaps");)
SIMPLE(OnSP_eas, OpenSettings("eas");)
SIMPLE(OnSP_shared_folders_mobile, OpenSettings("sharedfolders-mobile");)

SIMPLE(OnCplAll, OpenCpl("");)
SIMPLE(OnCplPrograms, OpenCpl("appwiz.cpl");)
SIMPLE(OnCplDateTime, OpenCpl("timedate.cpl");)
SIMPLE(OnCplRegion, OpenCpl("intl.cpl");)
SIMPLE(OnCplKeyboard, OpenCpl("main.cpl");)
SIMPLE(OnCplMouse, OpenCpl("main.cpl");)
SIMPLE(OnCplSound, OpenCpl("mmsys.cpl");)
SIMPLE(OnCplSoundEffects, OpenCpl("mmsys.cpl,,2");)
SIMPLE(OnCplSoundComm, OpenCpl("mmsys.cpl,,3");)
SIMPLE(OnCplFonts, OpenShellFolder("shell:Fonts");)
SIMPLE(OnCplFolderOpts, OpenCpl("folders");)
SIMPLE(OnCplInetOpts, OpenCpl("inetcpl.cpl");)
SIMPLE(OnCplUsers, OpenCpl("userpasswords2");)
SIMPLE(OnCplPower, OpenCpl("powercfg.cpl");)
SIMPLE(OnCplFirewall, OpenCpl("firewall.cpl");)
SIMPLE(OnCplAdminTools, OpenShellFolder("shell:::{D20EA4E1-3957-11d2-A40B-0C5020524153}");)
SIMPLE(OnCplBitlocker, OpenCplName("Microsoft.BitLockerDriveEncryption");)
SIMPLE(OnCplCredMgr, OpenCplName("Microsoft.CredentialManager");)
SIMPLE(OnCplAutoPlay, OpenCplName("Microsoft.AutoPlay");)
SIMPLE(OnCplColor, OpenCplName("Microsoft.ColorManagement");)
SIMPLE(OnCplGameCtrl, OpenCpl("joy.cpl");)
SIMPLE(OnCplOdbc, Run("odbcad32.exe", NULL, 0);)
SIMPLE(OnCplDevicesPrinters, OpenCplName("Microsoft.DevicesAndPrinters");)
SIMPLE(OnCplDisplay, OpenCpl("desk.cpl");)
SIMPLE(OnCplScreenSaver, OpenCpl("desk.cpl,,1");)
SIMPLE(OnCplTroubleshooting, OpenCplName("Microsoft.Troubleshooting");)
SIMPLE(OnCplRecovery, OpenCplName("Microsoft.Recovery");)
SIMPLE(OnCplFileHistory, OpenCplName("Microsoft.FileHistory");)
SIMPLE(OnCplBackupRestore, OpenCplName("Microsoft.BackupAndRestore");)
SIMPLE(OnCplEaseOfAccess, OpenCplName("Microsoft.EaseOfAccess");)

SIMPLE(OnNetSettings, OpenSettings("network");)
SIMPLE(OnNetAdapters, OpenCpl("ncpa.cpl");)
SIMPLE(OnNetWifi, OpenSettings("network-wifi");)
SIMPLE(OnNetVpn, OpenSettings("network-vpn");)
SIMPLE(OnNetProxy, OpenSettings("network-proxy");)
SIMPLE(OnNetShareCenter, OpenCplName("Microsoft.NetworkAndSharingCenter");)
SIMPLE(OnNetAdvShare, OpenCplName("Microsoft.NetworkAndSharingCenter /page Advanced");)
SIMPLE(OnNetReset, OpenSettings("network-reset");)
SIMPLE(OnNetWifiSense, OpenSettings("network-wifisense");)
SIMPLE(OnNetFwRules, Run("wf.msc", NULL, 0);)
SIMPLE(OnNetInetProp, OpenCpl("inetcpl.cpl");)
SIMPLE(OnNetRdp, Run("mstsc.exe", NULL, 0);)
SIMPLE(OnNetIpconfig, RunCmd("ipconfig", 1);)
SIMPLE(OnNetIpconfigAll, RunCmd("ipconfig /all", 1);)
SIMPLE(OnNetIpconfigRelease, RunCmd("ipconfig /release", 0);)
SIMPLE(OnNetIpconfigRenew, RunCmd("ipconfig /renew", 0);)
SIMPLE(OnNetPing, RunCmd("ping google.com -t", 1);)
SIMPLE(OnNetPingLocal, RunCmd("ping 127.0.0.1 -t", 1);)
SIMPLE(OnNetTracert, RunCmd("tracert google.com", 1);)
SIMPLE(OnNetNslookup, RunCmd("nslookup", 1);)
SIMPLE(OnNetDns, RunCmd("nslookup google.com", 1);)
SIMPLE(OnNetNetstat, RunCmd("netstat -an", 1);)
SIMPLE(OnNetNetstatB, RunCmd("netstat -b", 1);)
SIMPLE(OnNetRoute, RunCmd("route print", 1);)
SIMPLE(OnNetArp, RunCmd("arp -a", 1);)
SIMPLE(OnNetGetmac, RunCmd("getmac", 1);)
SIMPLE(OnNetNbtstat, RunCmd("nbtstat -n", 1);)
SIMPLE(OnNetFlushDns, RunCmd("ipconfig /flushdns", 0);)
SIMPLE(OnNetWinsock, RunCmd("netsh winsock reset", 0);)
SIMPLE(OnNetBrowser, RunURI("https://www.google.com");)

SIMPLE(OnPwSleep, SetSystemPowerState(FALSE, FALSE);)
SIMPLE(OnPwHibernate, SetSystemPowerState(TRUE, FALSE);)
SIMPLE(OnPwRestart, RunCmd("shutdown /r /t 3", 0);)
SIMPLE(OnPwShutdown, RunCmd("shutdown /s /t 3", 0);)
SIMPLE(OnPwSignout, RunCmd("shutdown /l", 0);)
SIMPLE(OnPwLock, LockWorkStation();)
SIMPLE(OnPwOptions, OpenCpl("powercfg.cpl");)
SIMPLE(OnPwListPlans, RunCmd("powercfg /list", 1);)
SIMPLE(OnPwPlanBalanced, RunCmd("powercfg /setactive 381b4222-f694-41f0-9685-ff5bb260df2e", 0);)
SIMPLE(OnPwPlanHighPerf, RunCmd("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c", 0);)
SIMPLE(OnPwPlanSaver, RunCmd("powercfg /setactive a1841308-3541-4fab-bc81-f71556f20b4a", 0);)
SIMPLE(OnPwBatteryReport, RunCmd("powercfg /batteryreport", 0);)
SIMPLE(OnPwEnergyReport, RunElevatedCmd("powercfg /energy");)

SIMPLE(OnDispSettings, OpenSettings("display");)
SIMPLE(OnDispPersonal, OpenSettings("personalization");)
SIMPLE(OnDispBackground, OpenSettings("personalization-background");)
SIMPLE(OnDispColors, OpenSettings("colors");)
SIMPLE(OnDispLockScreen, OpenSettings("lockscreen");)
SIMPLE(OnDispThemes, OpenSettings("themes");)
SIMPLE(OnDispStart, OpenSettings("personalization-start");)
SIMPLE(OnDispTaskbar, OpenSettings("taskbar");)
SIMPLE(OnDispResolution, OpenCpl("desk.cpl");)
SIMPLE(OnDispScreenSaver, OpenCpl("desk.cpl,,1");)
SIMPLE(OnDispNightLight, OpenSettings("nightlight");)
SIMPLE(OnDispMultiple, OpenSettings("display-multipledisplays");)
SIMPLE(OnDispShowDesktop, SimulateCombo(VK_LWIN, 'D');)
SIMPLE(OnDispZoomIn, SimulateCombo(VK_CONTROL, VK_ADD);)
SIMPLE(OnDispZoomOut, SimulateCombo(VK_CONTROL, VK_SUBTRACT);)
SIMPLE(OnDispZoom100, SimulateCombo(VK_CONTROL, '0');)

SIMPLE(OnAudVolUp, SimulateKey(VK_VOLUME_UP);)
SIMPLE(OnAudVolDown, SimulateKey(VK_VOLUME_DOWN);)
SIMPLE(OnAudMute, SimulateKey(VK_VOLUME_MUTE);)
SIMPLE(OnAudMixer, Run("sndvol.exe", NULL, 0);)
SIMPLE(OnAudSettings, OpenSettings("sound");)
SIMPLE(OnAudPlayback, OpenCpl("mmsys.cpl,,0");)
SIMPLE(OnAudRecording, OpenCpl("mmsys.cpl,,1");)
SIMPLE(OnAudSounds, OpenCpl("mmsys.cpl,,2");)
SIMPLE(OnAudComm, OpenCpl("mmsys.cpl,,3");)
SIMPLE(OnAudControl, OpenCpl("mmsys.cpl");)

SIMPLE(OnSecDashboard, OpenSettings("windowsdefender");)
SIMPLE(OnSecQuickScan, Run("powershell.exe", "-NoProfile -Command \"Start-MpScan -ScanType QuickScan\"", 1);)
SIMPLE(OnSecVirusThreat, OpenSettings("windowsdefender");)
SIMPLE(OnSecThreatSettings, OpenSettings("windowsdefender");)
SIMPLE(OnSecAccountProtection, OpenSettings("windowsdefender-accountprotection");)
SIMPLE(OnSecFirewall, OpenCpl("firewall.cpl");)
SIMPLE(OnSecFwProtect, OpenSettings("windowsdefender-firewall");)
SIMPLE(OnSecAppBrowser, OpenSettings("windowsdefender-appbrowser");)
SIMPLE(OnSecDeviceSec, OpenSettings("windowsdefender-devicesecurity");)
SIMPLE(OnSecDeviceHealth, OpenSettings("windowsdefender-deviceperformance");)
SIMPLE(OnSecExploitProtection, OpenSettings("windowsdefender-exploitprotection");)
SIMPLE(OnSecFamily, OpenSettings("windowsdefender-family");)
SIMPLE(OnSecUpdate, OpenSettings("windowsupdate");)
SIMPLE(OnSecBackup, OpenSettings("backup");)
SIMPLE(OnSecCredMgr, OpenCplName("Microsoft.CredentialManager");)
SIMPLE(OnSecBitlocker, OpenCplName("Microsoft.BitLockerDriveEncryption");)

SIMPLE(OnAppNotepad, Run("notepad.exe", NULL, 0);)
SIMPLE(OnAppWordpad, Run("write.exe", NULL, 0);)
SIMPLE(OnAppPaint, Run("mspaint.exe", NULL, 0);)
SIMPLE(OnAppCalc, Run("calc.exe", NULL, 0);)
SIMPLE(OnAppSnipping, Run("SnippingTool.exe", NULL, 0);)
SIMPLE(OnAppSticky, Run("StikyNot.exe", NULL, 0);)
SIMPLE(OnAppEdge, Run("msedge.exe", NULL, 0);)
SIMPLE(OnAppStore, RunURI("ms-windows-store:");)
SIMPLE(OnAppIE, Run("iexplore.exe", NULL, 0);)
SIMPLE(OnAppWmp, Run("wmplayer.exe", NULL, 0);)
SIMPLE(OnAppGameBar, OpenSettings("gaming-gamebar");)
SIMPLE(OnAppExplorer, Run("explorer.exe", NULL, 0);)
SIMPLE(OnAppThisPC, OpenShellFolder("shell:MyComputerFolder");)
SIMPLE(OnAppDocuments, OpenShellFolder("shell:Personal");)
SIMPLE(OnAppDownloads, OpenShellFolder("shell:Downloads");)
SIMPLE(OnAppDesktop, OpenShellFolder("shell:Desktop");)
SIMPLE(OnAppPictures, OpenShellFolder("shell:MyPictures");)
SIMPLE(OnAppMusic, OpenShellFolder("shell:MyMusic");)
SIMPLE(OnAppVideos, OpenShellFolder("shell:MyVideo");)
SIMPLE(OnAppOneDrive, OpenShellFolder("shell:OneDrive");)
SIMPLE(OnAppNetFolder, OpenShellFolder("shell:NetworkPlacesFolder");)
SIMPLE(OnAppRecycle, OpenShellFolder("shell:RecycleBinFolder");)
SIMPLE(OnAppControlPanel, OpenCpl("");)
SIMPLE(OnAppRunDialog, SimulateCombo(VK_LWIN, 'R');)
SIMPLE(OnAppSearch, SimulateCombo(VK_LWIN, 'S');)
SIMPLE(OnAppSettings, SimulateCombo(VK_LWIN, 'I');)
SIMPLE(OnAppNotification, SimulateCombo(VK_LWIN, 'A');)
SIMPLE(OnAppScreenshot, SimulateTriple(VK_LWIN, VK_SHIFT, 'S');)
SIMPLE(OnAppEmoji, SimulateCombo(VK_LWIN, VK_OEM_PERIOD);)
SIMPLE(OnAppClipboardHist, SimulateCombo(VK_LWIN, 'V');)
SIMPLE(OnAppMagnifier, SimulateCombo(VK_LWIN, VK_ADD);)

SIMPLE(OnMaintClean, Run("cleanmgr.exe", NULL, 0);)
SIMPLE(OnMaintDefrag, Run("dfrgui.exe", NULL, 0);)
SIMPLE(OnMaintChkdsk, RunCmd("chkdsk", 1);)
SIMPLE(OnMaintSfc, RunElevatedCmd("sfc /scannow");)
SIMPLE(OnMaintDismCheck, RunElevatedCmd("dism /online /cleanup-image /checkhealth");)
SIMPLE(OnMaintDismRestore, RunElevatedCmd("dism /online /cleanup-image /restorehealth");)
SIMPLE(OnMaintSysRestore, Run("rstrui.exe", NULL, 0);)
SIMPLE(OnMaintWinBackup, OpenSettings("backup");)
SIMPLE(OnMaintReliability, Run("perfmon.exe", "/rel", 0);)
SIMPLE(OnMaintPrintMgmt, Run("printmanagement.msc", NULL, 0);)
SIMPLE(OnMaintShared, Run("fsmgmt.msc", NULL, 0);)
SIMPLE(OnMaintStorageSense, OpenSettings("storagesense");)
SIMPLE(OnMaintStartupApps, OpenSettings("startupapps");)

SIMPLE(OnRecTrouble, OpenSettings("troubleshoot");)
SIMPLE(OnRecStartupRepair, RunElevatedCmd("reagentc /boottore");)
SIMPLE(OnRecMemDiag, Run("MdSched.exe", NULL, 0);)
SIMPLE(OnRecSteps, Run("psr.exe", NULL, 0);)
SIMPLE(OnRecResetPc, OpenSettings("recovery");)
SIMPLE(OnRecAdvStartup, RunCmd("shutdown /r /o /f /t 0", 0);)
SIMPLE(OnRecDrive, Run("RecoveryDrive.exe", NULL, 0);)
SIMPLE(OnRecSysImage, OpenCplName("Microsoft.BackupAndRestore");)
SIMPLE(OnRecFileHistory, OpenCplName("Microsoft.FileHistory");)
SIMPLE(OnRecBcd, RunElevatedCmd("bcdedit /enum");)
SIMPLE(OnRecSafeMode, Run("msconfig.exe", NULL, 0);)

SIMPLE(OnDevCmd, Run("cmd.exe", NULL, 0);)
SIMPLE(OnDevCmdAdmin, Run("cmd.exe", NULL, 1);)
SIMPLE(OnDevPowershell, Run("powershell.exe", NULL, 0);)
SIMPLE(OnDevPowershellAdmin, Run("powershell.exe", NULL, 1);)
SIMPLE(OnDevTerminal, Run("wt.exe", NULL, 0);)
SIMPLE(OnDevTerminalAdmin, Run("wt.exe", NULL, 1);)
SIMPLE(OnDevVsPrompt, RunCmd("call \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -property installationPath", 1);)
SIMPLE(OnDevWsl, Run("wsl.exe", NULL, 0);)
SIMPLE(OnDevWslList, RunCmd("wsl --list --verbose", 1);)
SIMPLE(OnDevWslShutdown, RunCmd("wsl --shutdown", 0);)
SIMPLE(OnDevWslUpdate, RunCmd("wsl --update", 0);)
SIMPLE(OnDevPerfRec, Run("wprui.exe", NULL, 0);)
SIMPLE(OnDevWinTools, OpenShellFolder("shell:::{D20EA4E1-3957-11d2-A40B-0C5020524153}");)
SIMPLE(OnDevDotnetCheck, RunCmd("dotnet --list-runtimes", 1);)
SIMPLE(OnDevNodeCheck, RunCmd("node --version", 1);)
SIMPLE(OnDevGitCheck, RunCmd("git --version", 1);)
SIMPLE(OnDevPythonCheck, RunCmd("python --version", 1);)
SIMPLE(OnDevGoCheck, RunCmd("go version", 1);)
SIMPLE(OnDevJavaCheck, RunCmd("java -version", 1);)
SIMPLE(OnDevCurlCheck, RunCmd("curl --version", 1);)

SIMPLE(OnFldAppData, OpenShellFolder("%APPDATA%");)
SIMPLE(OnFldLocalAppData, OpenShellFolder("%LOCALAPPDATA%");)
SIMPLE(OnFldProgramData, OpenShellFolder("%PROGRAMDATA%");)
SIMPLE(OnFldStartup, OpenShellFolder("shell:startup");)
SIMPLE(OnFldTemp, OpenShellFolder("%TEMP%");)
SIMPLE(OnFldWindows, OpenShellFolder("%WINDIR%");)
SIMPLE(OnFldSystem32, OpenShellFolder("%WINDIR%\\System32");)
SIMPLE(OnFldSendTo, OpenShellFolder("shell:sendto");)
SIMPLE(OnFldQuickLaunch, OpenShellFolder("%APPDATA%\\Microsoft\\Internet Explorer\\Quick Launch");)
SIMPLE(OnFldRecent, OpenShellFolder("shell:Recent");)
SIMPLE(OnFldTemplates, OpenShellFolder("shell:Templates");)
SIMPLE(OnFldFavorites, OpenShellFolder("shell:Favorites");)
SIMPLE(OnFldCookies, OpenShellFolder("shell:Cookies");)
SIMPLE(OnFldHistory, OpenShellFolder("shell:History");)
SIMPLE(OnFldNetHood, OpenShellFolder("shell:NetHood");)
SIMPLE(OnFldPrintHood, OpenShellFolder("shell:PrintHood");)
SIMPLE(OnFldPublic, OpenShellFolder("shell:Public");)
SIMPLE(OnFldUsers, OpenShellFolder("shell:UsersFilesFolder");)
SIMPLE(OnFldContacts, OpenShellFolder("shell:Contacts");)
SIMPLE(OnFldLibraries, OpenShellFolder("shell:Libraries");)
SIMPLE(OnFldSavedGames, OpenShellFolder("shell:SavedGames");)
SIMPLE(OnFldSearches, OpenShellFolder("shell:Searches");)
SIMPLE(OnFldDesktop, OpenShellFolder("shell:Desktop");)
SIMPLE(OnFldDownloads, OpenShellFolder("shell:Downloads");)

SIMPLE(OnAdmLusrmgr, Run("lusrmgr.msc", NULL, 0);)
SIMPLE(OnAdmSecpol, Run("secpol.msc", NULL, 0);)
SIMPLE(OnAdmCertmgr, Run("certmgr.msc", NULL, 0);)
SIMPLE(OnAdmComServices, Run("comexp.msc", NULL, 0);)
SIMPLE(OnAdmTpm, Run("tpm.msc", NULL, 0);)
SIMPLE(OnAdmHyperV, Run("virtmgmt.msc", NULL, 0);)
SIMPLE(OnAdmIscsi, Run("iscsicpl.exe", NULL, 0);)
SIMPLE(OnAdmStorageSpaces, OpenSettings("storagepool");)
SIMPLE(OnAdmWmi, Run("wmimgmt.msc", NULL, 0);)
SIMPLE(OnAdmRsop, Run("rsop.msc", NULL, 0);)

/* ===================== All Microsoft settings pages (ms-settings deep links) ===================== */
SIMPLE(OnSP_workplace, OpenSettings("workplace");)
SIMPLE(OnSP_emailandaccounts, OpenSettings("emailandaccounts");)
SIMPLE(OnSP_otherusers, OpenSettings("otherusers");)
SIMPLE(OnSP_assignedaccess, OpenSettings("assignedaccess");)
SIMPLE(OnSP_signinoptions, OpenSettings("signinoptions");)
SIMPLE(OnSP_signinoptions_dynamiclock, OpenSettings("signinoptions-dynamiclock");)
SIMPLE(OnSP_provisioning, OpenSettings("provisioning");)
SIMPLE(OnSP_workplace_provisioning, OpenSettings("workplace-provisioning");)
SIMPLE(OnSP_workplace_repairtoken, OpenSettings("workplace-repairtoken");)
SIMPLE(OnSP_appsforwebsites, OpenSettings("appsforwebsites");)
SIMPLE(OnSP_defaultapps, OpenSettings("defaultapps");)
SIMPLE(OnSP_defaultbrowsersettings, OpenSettings("defaultbrowsersettings");)
SIMPLE(OnSP_videoplayback, OpenSettings("videoplayback");)
SIMPLE(OnSP_controlcenter, OpenSettings("controlcenter");)
SIMPLE(OnSP_connecteddevices, OpenSettings("connecteddevices");)
SIMPLE(OnSP_camera, OpenSettings("camera");)
SIMPLE(OnSP_pen_button, OpenSettings("pen-button");)
SIMPLE(OnSP_devicestyping_hwkbtextsuggestions, OpenSettings("devicestyping-hwkbtextsuggestions");)
SIMPLE(OnSP_wheel, OpenSettings("wheel");)
SIMPLE(OnSP_mobile_devices_addphone, OpenSettings("mobile-devices-addphone");)
SIMPLE(OnSP_mobile_devices_addphone_direct, OpenSettings("mobile-devices-addphone-direct");)
SIMPLE(OnSP_deviceusage, OpenSettings("deviceusage");)
SIMPLE(OnSP_easeofaccess_colorfilter_adaptivecolorlink, OpenSettings("easeofaccess-colorfilter-adaptivecolorlink");)
SIMPLE(OnSP_easeofaccess_colorfilter_bluelightlink, OpenSettings("easeofaccess-colorfilter-bluelightlink");)
SIMPLE(OnSP_easeofaccess_hearingaids, OpenSettings("easeofaccess-hearingaids");)
SIMPLE(OnSP_easeofaccess_mousepointer, OpenSettings("easeofaccess-mousepointer");)
SIMPLE(OnSP_easeofaccess_narrator_isautostartenabled, OpenSettings("easeofaccess-narrator-isautostartenabled");)
SIMPLE(OnSP_easeofaccess_visualeffects, OpenSettings("easeofaccess-visualeffects");)
SIMPLE(OnSP_easeofaccess_fonts, OpenSettings("easeofaccess-fonts");)
SIMPLE(OnSP_extras, OpenSettings("extras");)
SIMPLE(OnSP_family_group_ms, OpenSettings("family-group");)
SIMPLE(OnSP_quietmomentsgame, OpenSettings("quietmomentsgame");)
SIMPLE(OnSP_privacy_holographic_environment, OpenSettings("privacy-holographic-environment");)
SIMPLE(OnSP_holographic_management, OpenSettings("holographic-management");)
SIMPLE(OnSP_holographic_startupandesktop, OpenSettings("holographic-startupandesktop");)
SIMPLE(OnSP_network_status, OpenSettings("network-status");)
SIMPLE(OnSP_network_advancedsettings, OpenSettings("network-advancedsettings");)
SIMPLE(OnSP_proximity, OpenSettings("proximity");)
SIMPLE(OnSP_network_directaccess, OpenSettings("network-directaccess");)
SIMPLE(OnSP_wifi_provisioning, OpenSettings("wifi-provisioning");)
SIMPLE(OnSP_personalization_start_places, OpenSettings("personalization-start-places");)
SIMPLE(OnSP_personalization_touchkeyboard, OpenSettings("personalization-touchkeyboard");)
SIMPLE(OnSP_fonts, OpenSettings("fonts");)
SIMPLE(OnSP_personalization_textinput, OpenSettings("personalization-textinput");)
SIMPLE(OnSP_personalization_textinput_copilot_hardwarekey, OpenSettings("personalization-textinput-copilot-hardwarekey");)
SIMPLE(OnSP_personalization_lighting, OpenSettings("personalization-lighting");)
SIMPLE(OnSP_privacy_accessoryapps, OpenSettings("privacy-accessoryapps");)
SIMPLE(OnSP_privacy_advertisingid, OpenSettings("privacy-advertisingid");)
SIMPLE(OnSP_privacy_automaticfiledownloads, OpenSettings("privacy-automaticfiledownloads");)
SIMPLE(OnSP_privacy_backgroundspatialperception, OpenSettings("privacy-backgroundspatialperception");)
SIMPLE(OnSP_privacy_callhistory, OpenSettings("privacy-callhistory");)
SIMPLE(OnSP_privacy_eyetracker, OpenSettings("privacy-eyetracker");)
SIMPLE(OnSP_privacy_broadfilesystemaccess, OpenSettings("privacy-broadfilesystemaccess");)
SIMPLE(OnSP_privacy_general, OpenSettings("privacy-general");)
SIMPLE(OnSP_privacy_graphicscaptureprogrammatic, OpenSettings("privacy-graphicscaptureprogrammatic");)
SIMPLE(OnSP_privacy_graphicscapturewithoutborder, OpenSettings("privacy-graphicscapturewithoutborder");)
SIMPLE(OnSP_privacy_motion, OpenSettings("privacy-motion");)
SIMPLE(OnSP_privacy_musiclibrary, OpenSettings("privacy-musiclibrary");)
SIMPLE(OnSP_privacy_customdevices, OpenSettings("privacy-customdevices");)
SIMPLE(OnSP_privacy_phonecalls, OpenSettings("privacy-phonecalls");)
SIMPLE(OnSP_search, OpenSettings("search");)
SIMPLE(OnSP_search_moredetails, OpenSettings("search-moredetails");)
SIMPLE(OnSP_search_permissions, OpenSettings("search-permissions");)
SIMPLE(OnSP_sound_defaultinputproperties, OpenSettings("sound-defaultinputproperties");)
SIMPLE(OnSP_sound_defaultoutputproperties, OpenSettings("sound-defaultoutputproperties");)
SIMPLE(OnSP_screenrotation, OpenSettings("screenrotation");)
SIMPLE(OnSP_display_advancedgraphics_default, OpenSettings("display-advancedgraphics-default");)
SIMPLE(OnSP_batterysaver_settings, OpenSettings("batterysaver-settings");)
SIMPLE(OnSP_batterysaver_usagedetails, OpenSettings("batterysaver-usagedetails");)
SIMPLE(OnSP_savelocations, OpenSettings("savelocations");)
SIMPLE(OnSP_deviceencryption, OpenSettings("deviceencryption");)
SIMPLE(OnSP_energyrecommendations, OpenSettings("energyrecommendations");)
SIMPLE(OnSP_quietmomentsscheduled, OpenSettings("quietmomentsscheduled");)
SIMPLE(OnSP_quietmomentspresentation, OpenSettings("quietmomentspresentation");)
SIMPLE(OnSP_multitasking_sgupdate, OpenSettings("multitasking-sgupdate");)
SIMPLE(OnSP_remotedesktop, OpenSettings("remotedesktop");)
SIMPLE(OnSP_presence, OpenSettings("presence");)
SIMPLE(OnSP_storagerecommendations, OpenSettings("storagerecommendations");)
SIMPLE(OnSP_disksandvolumes, OpenSettings("disksandvolumes");)
SIMPLE(OnSP_regionlanguage_jpnime, OpenSettings("regionlanguage-jpnime");)
SIMPLE(OnSP_regionformatting, OpenSettings("regionformatting");)
SIMPLE(OnSP_keyboard_advanced, OpenSettings("keyboard-advanced");)
SIMPLE(OnSP_regionlanguage_bpmfime, OpenSettings("regionlanguage-bpmfime");)
SIMPLE(OnSP_regionlanguage_cangjieime, OpenSettings("regionlanguage-cangjieime");)
SIMPLE(OnSP_regionlanguage_chsime_wubi_udp, OpenSettings("regionlanguage-chsime-wubi-udp");)
SIMPLE(OnSP_regionlanguage_quickime, OpenSettings("regionlanguage-quickime");)
SIMPLE(OnSP_regionlanguage_korime, OpenSettings("regionlanguage-korime");)
SIMPLE(OnSP_regionlanguage_chsime_pinyin, OpenSettings("regionlanguage-chsime-pinyin");)
SIMPLE(OnSP_regionlanguage_chsime_pinyin_domainlexicon, OpenSettings("regionlanguage-chsime-pinyin-domainlexicon");)
SIMPLE(OnSP_regionlanguage_chsime_pinyin_keyconfig, OpenSettings("regionlanguage-chsime-pinyin-keyconfig");)
SIMPLE(OnSP_regionlanguage_chsime_pinyin_udp, OpenSettings("regionlanguage-chsime-pinyin-udp");)
SIMPLE(OnSP_regionlanguage_chsime_wubi, OpenSettings("regionlanguage-chsime-wubi");)
SIMPLE(OnSP_delivery_optimization, OpenSettings("delivery-optimization");)
SIMPLE(OnSP_delivery_optimization_activity, OpenSettings("delivery-optimization-activity");)
SIMPLE(OnSP_delivery_optimization_advanced, OpenSettings("delivery-optimization-advanced");)
SIMPLE(OnSP_findmydevice, OpenSettings("findmydevice");)
SIMPLE(OnSP_windowsinsider, OpenSettings("windowsinsider");)
SIMPLE(OnSP_windowsinsider_optin, OpenSettings("windowsinsider-optin");)
SIMPLE(OnSP_windowsupdate_action, OpenSettings("windowsupdate-action");)
SIMPLE(OnSP_windowsupdate_options, OpenSettings("windowsupdate-options");)
SIMPLE(OnSP_windowsupdate_seekerondemand, OpenSettings("windowsupdate-seekerondemand");)
SIMPLE(OnSP_cortana_moredetails, OpenSettings("cortana-moredetails");)
SIMPLE(OnSP_cortana_windowssearch, OpenSettings("cortana-windowssearch");)
SIMPLE(OnSP_personalization_glance, OpenSettings("personalization-glance");)
SIMPLE(OnSP_personalization_navbar, OpenSettings("personalization-navbar");)
SIMPLE(OnSP_privacy_feedback_telemetryviewergroup, OpenSettings("privacy-feedback-telemetryviewergroup");)

/* ===================== Extended toolset: helpers ===================== */
static int stristr(const char *hay, const char *needle);

static int WriteRegDword(HKEY root, const char *subkey, const char *value, DWORD data) {
    HKEY k;
    LONG r = RegOpenKeyExA(root, subkey, 0, KEY_SET_VALUE, &k);
    if (r != ERROR_SUCCESS) return 0;
    r = RegSetValueExA(k, value, 0, REG_DWORD, (const BYTE *)&data, sizeof data);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static int ReadRegDword(HKEY root, const char *subkey, const char *value, DWORD *out) {
    HKEY k;
    DWORD type = 0, size = sizeof *out;
    if (RegOpenKeyExA(root, subkey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return 0;
    LONG r = RegQueryValueExA(k, value, NULL, &type, (BYTE *)out, &size);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && type == REG_DWORD;
}

static void RefreshShell(void) {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0, SMTO_ABORTIFHUNG, 3000, NULL);
}

static void RefreshIntl(void) {
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Intl", SMTO_ABORTIFHUNG, 3000, NULL);
}

static void RefreshEnv(void) {
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 3000, NULL);
}

static int SafeArg(const char *s) {
    if (!s || !*s) return 0;
    if (strchr(s, '&') || strchr(s, '|') || strchr(s, '<') || strchr(s, '>') || strchr(s, '"'))
        return 0;
    return 1;
}

static void TrimSpaces(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t l = strlen(s);
    while (l > 0 && (s[l - 1] == ' ' || s[l - 1] == '\t')) s[--l] = 0;
}

static int SplitArgs(const char *arg, char parts[][256], int maxParts) {
    int n = 0;
    if (!arg) return 0;
    const char *p = arg;
    while (n < maxParts && *p) {
        const char *sep = strchr(p, '|');
        size_t l = sep ? (size_t)(sep - p) : strlen(p);
        if (l > 255) l = 255;
        memcpy(parts[n], p, l);
        parts[n][l] = 0;
        TrimSpaces(parts[n]);
        n++;
        if (!sep) break;
        p = sep + 1;
    }
    return n;
}

static void NotifyF(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Notify(buf);
}

static void TweakApply(HKEY root, const char *sub, const char *val, DWORD data, const char *what) {
    if (WriteRegDword(root, sub, val, data)) {
        NotifyF("Applied: %s", what);
    } else {
        char cmd[800];
        snprintf(cmd, sizeof cmd, "reg add \"%s\\%s\" /v %s /t REG_DWORD /d %lu /f",
                 root == HKEY_LOCAL_MACHINE ? "HKLM" : "HKCU", sub, val, (unsigned long)data);
        RunElevatedCmd(cmd);
        NotifyF("Applied (elevated): %s", what);
    }
    RefreshShell();
}

static int HttpGetTextUA(const char *host, const char *path, char *out, int outSize, const wchar_t *ua) {
    out[0] = 0;
    BOOL ok = FALSE;
    HINTERNET hS = WinHttpOpen(ua, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return 0;
    WinHttpSetTimeouts(hS, 10000, 10000, 10000, 15000);
    wchar_t wh[256], wp[2048];
    MultiByteToWideChar(CP_ACP, 0, host, -1, wh, 256);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, 2048);
    HINTERNET hC = WinHttpConnect(hS, wh, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hC) {
        HINTERNET hR = WinHttpOpenRequest(hC, L"GET", wp, NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hR) {
            if (WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hR, NULL)) {
                DWORD total = 0;
                while (total < (DWORD)(outSize - 1)) {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(hR, &avail) || avail == 0) break;
                    if (avail > (DWORD)(outSize - 1) - total) avail = (DWORD)(outSize - 1) - total;
                    DWORD got = 0;
                    if (!WinHttpReadData(hR, out + total, avail, &got) || got == 0) break;
                    total += got;
                }
                out[total] = 0;
                ok = total > 0;
            }
            WinHttpCloseHandle(hR);
        }
        WinHttpCloseHandle(hC);
    }
    WinHttpCloseHandle(hS);
    return ok;
}

static int HttpGetText(const char *host, const char *path, char *out, int outSize) {
    return HttpGetTextUA(host, path, out, outSize, L"WindowsControl/2.0");
}

/* ===================== Windows Manual & Microsoft Docs =====================
   A rich Windows manual. Partly built-in (offline), partly fetched live from
   the official Microsoft Learn documentation (learn.microsoft.com) through
   WinHTTP: page fetching + the official Learn search API.                  */

static void UrlEncode(const char *in, char *out, int outSize) {
    static const char hex[] = "0123456789ABCDEF";
    int o = 0;
    for (const char *p = in; *p && o < outSize - 4; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15];
        }
    }
    out[o] = 0;
}

/* Convert an HTML document to readable text: strips tags, comments,
   script/style content, decodes entities and collapses whitespace. */
static int HtmlToText(const char *html, char *out, int outSize) {
    char *o = out;
    char *oend = out + outSize - 1;
    char last = 0;
    const char *p = html;
    while (*p && o < oend) {
        char c = *p;
        if (c == '<') {
            const char *q = p + 1;
            if (*q == '!') {
                if (strncmp(q, "!--", 3) == 0) {
                    const char *e = strstr(p, "-->");
                    if (!e) break;
                    p = e + 3;
                    continue;
                }
                const char *e = strchr(q, '>');
                if (!e) break;
                p = e + 1;
                continue;
            }
            if (*q == '/') q++;
            char tag[64];
            int ti = 0;
            while (*q && *q != '>' && *q != ' ' && *q != '\t' && *q != '\n' && ti < 63)
                tag[ti++] = (char)tolower((unsigned char)*q++);
            tag[ti] = 0;
            const char *e = strchr(p, '>');
            if (!e) break;
            p = e + 1;
            if (!tag[0]) continue;
            if (strcmp(tag, "script") == 0 || strcmp(tag, "style") == 0 ||
                strcmp(tag, "head") == 0 || strcmp(tag, "noscript") == 0 ||
                strcmp(tag, "svg") == 0 || strcmp(tag, "template") == 0 ||
                strcmp(tag, "iframe") == 0) {
                char close[80];
                snprintf(close, sizeof close, "</%s", tag);
                const char *ce = strstr(p, close);
                if (!ce) { p += strlen(p); break; }
                const char *gt = strchr(ce, '>');
                p = gt ? gt + 1 : ce + strlen(close);
                continue;
            }
            if (strcmp(tag, "br") == 0 || strcmp(tag, "p") == 0 ||
                strcmp(tag, "div") == 0 || strcmp(tag, "li") == 0 ||
                strcmp(tag, "tr") == 0 || strcmp(tag, "h1") == 0 ||
                strcmp(tag, "h2") == 0 || strcmp(tag, "h3") == 0 ||
                strcmp(tag, "h4") == 0 || strcmp(tag, "h5") == 0 ||
                strcmp(tag, "h6") == 0 || strcmp(tag, "pre") == 0 ||
                strcmp(tag, "table") == 0 || strcmp(tag, "ul") == 0 ||
                strcmp(tag, "ol") == 0 || strcmp(tag, "blockquote") == 0 ||
                strcmp(tag, "section") == 0 || strcmp(tag, "article") == 0 ||
                strcmp(tag, "hr") == 0 || strcmp(tag, "dt") == 0 ||
                strcmp(tag, "dd") == 0 || strcmp(tag, "td") == 0 ||
                strcmp(tag, "th") == 0 || strcmp(tag, "figure") == 0 ||
                strcmp(tag, "figcaption") == 0 || strcmp(tag, "summary") == 0 ||
                strcmp(tag, "details") == 0 || strcmp(tag, "address") == 0 ||
                strcmp(tag, "fieldset") == 0 || strcmp(tag, "legend") == 0 ||
                strcmp(tag, "caption") == 0 || strcmp(tag, "header") == 0 ||
                strcmp(tag, "footer") == 0 || strcmp(tag, "main") == 0 ||
                strcmp(tag, "nav") == 0 || strcmp(tag, "aside") == 0) {
                if (last && last != '\n') { *o++ = '\n'; last = '\n'; }
            }
            continue;
        }
        if (c == '&') {
            struct { const char *e; const char *r; } ents[] = {
                {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
                {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"},
                {"&#34;", "\""}, {"&#x27;", "'"}, {"&nbsp;", " "},
                {"&ndash;", "-"}, {"&mdash;", "-"}, {"&hellip;", "..."},
                {"&copy;", "(c)"}, {"&reg;", "(R)"}, {"&trade;", "(TM)"},
                {"&middot;", "."}, {"&bull;", "*"}, {"&deg;", " deg"},
                {"&plusmn;", "+/-"}, {"&times;", "x"}, {"&divide;", "/"},
                {"&eacute;", "e"}, {"&egrave;", "e"}, {"&ecirc;", "e"},
                {"&euml;", "e"}, {"&agrave;", "a"}, {"&acirc;", "a"},
                {"&auml;", "a"}, {"&igrave;", "i"}, {"&iuml;", "i"},
                {"&ograve;", "o"}, {"&ouml;", "o"}, {"&ugrave;", "u"},
                {"&uuml;", "u"}, {"&ccedil;", "c"}, {"&ntilde;", "n"},
                {"&szlig;", "ss"}, {"&iexcl;", "!"}, {"&iquest;", "?"},
            };
            int done = 0;
            for (int i = 0; i < (int)ARRAY_LEN(ents); i++) {
                if (strncmp(p, ents[i].e, strlen(ents[i].e)) == 0) {
                    const char *r = ents[i].r;
                    while (*r && o < oend) *o++ = *r++;
                    last = (o > out) ? o[-1] : 0;
                    p += strlen(ents[i].e);
                    done = 1;
                    break;
                }
            }
            if (done) continue;
            if (p[1] == '#') {
                const char *q = p + 2;
                int isHex = (*q == 'x' || *q == 'X');
                if (isHex) q++;
                const char *s = q;
                while (*q && *q != ';') q++;
                if (*q == ';') {
                    char num[16];
                    int nl = (int)(q - s);
                    if (nl > 0 && nl < 16) {
                        strncpy(num, s, nl);
                        num[nl] = 0;
                        int code = (int)strtol(num, NULL, isHex ? 16 : 10);
                        if (code >= 32 && code <= 126) *o++ = (char)code;
                        else if (code == 160) *o++ = ' ';
                        else *o++ = '?';
                        last = (o > out) ? o[-1] : 0;
                        p = q + 1;
                        continue;
                    }
                }
            }
            *o++ = '&';
            last = '&';
            p++;
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (last && last != '\n') { *o++ = '\n'; last = '\n'; }
            p++;
            continue;
        }
        if (c == '\t') c = ' ';
        if (c == ' ') {
            if (last && last != ' ' && last != '\n') { *o++ = ' '; last = ' '; }
            p++;
            continue;
        }
        *o++ = c;
        last = c;
        p++;
    }
    if (o > out && o[-1] != '\n') *o++ = '\n';
    *o = 0;
    return (int)(o - out);
}

/* Read a JSON string value for a key, decoding escapes. */
static void JsStr(const char *js, const char *key, char *out, int outSize) {
    out[0] = 0;
    char pat[128];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *f = strstr(js, pat);
    if (!f) return;
    f += strlen(pat);
    while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') f++;
    if (*f != ':') return;
    f++;
    while (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') f++;
    if (*f != '"') return;
    f++;
    char *o = out;
    char *oend = out + outSize - 1;
    while (*f && *f != '"' && o < oend) {
        if (*f == '\\') {
            f++;
            switch (*f) {
                case 'n': *o++ = '\n'; break;
                case 't': *o++ = '\t'; break;
                case 'r': break;
                case '"': *o++ = '"'; break;
                case '\\': *o++ = '\\'; break;
                case '/': *o++ = '/'; break;
                case 'u': {
                    int code = 0;
                    for (int k = 0; k < 4 && f[1 + k]; k++) {
                        char ch = f[1 + k];
                        code <<= 4;
                        if (ch >= '0' && ch <= '9') code += ch - '0';
                        else if (ch >= 'a' && ch <= 'f') code += ch - 'a' + 10;
                        else if (ch >= 'A' && ch <= 'F') code += ch - 'A' + 10;
                    }
                    if (code >= 32 && code < 127) *o++ = (char)code;
                    else if (code == 0x20AC) *o++ = (char)0x80;
                    else if (code >= 0xA0 && code <= 0xFF) *o++ = (char)code;
                    else *o++ = '?';
                    f += 4;
                    break;
                }
                default: *o++ = *f ? *f : '?'; break;
            }
            if (*f) f++;
        } else {
            *o++ = *f++;
        }
    }
    *o = 0;
}

static void ExtractTitle(const char *html, char *out, int outSize) {
    out[0] = 0;
    char tmp[600];
    const char *t = strstr(html, "<title");
    if (!t) return;
    const char *gt = strchr(t, '>');
    if (!gt) return;
    const char *end = strstr(gt + 1, "</title>");
    if (!end) return;
    size_t n = (size_t)(end - (gt + 1));
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    memcpy(tmp, gt + 1, n);
    tmp[n] = 0;
    HtmlToText(tmp, out, outSize);
}

static void ExtractMain(const char *html, char *out, int outSize) {
    out[0] = 0;
    const char *src = html;
    size_t n = strlen(html);
    const char *m = strstr(html, "<main");
    if (m) {
        const char *end = strstr(m, "</main>");
        const char *stop = end ? end : m + strlen(m);
        src = m;
        n = (size_t)(stop - m);
        if (n > 262144) n = 262144;
    }
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return;
    memcpy(tmp, src, n);
    tmp[n] = 0;
    HtmlToText(tmp, out, outSize);
    free(tmp);
}

/* Fetch an official Microsoft Learn page and extract title + readable text.
   Returns 1 on success. */
static int DocsFetchPage(const char *path, char *titleOut, int titleSize,
                         char *textOut, int textSize) {
    if (!path || !*path) return 0;
    const char *p = path;
    if (*p == '/') p++;
    if (strstr(p, "..")) return 0;
    char enc[1536];
    UrlEncode(p, enc, sizeof enc);
    char full[2048];
    snprintf(full, sizeof full, "/en-us/%s", enc);
    char *buf = (char *)malloc(420000);
    if (!buf) return 0;
    int ok = HttpGetTextUA("learn.microsoft.com", full, buf, 420000,
                           L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                           L"AppleWebKit/537.36 (KHTML, like Gecko) "
                           L"Chrome/120.0 Safari/537.36");
    if (ok) {
        ExtractTitle(buf, titleOut, titleSize);
        ExtractMain(buf, textOut, textSize);
        if (!textOut[0]) ok = 0;
    }
    free(buf);
    return ok;
}

static void DocsShowPage(const char *path) {
    char *title = (char *)malloc(600);
    char *text = (char *)malloc(250000);
    if (!title || !text) { free(title); free(text); Notify("Out of memory."); return; }
    ReportClear();
    ReportAdd("Microsoft Learn - official Windows documentation");
    ReportAdd("https://learn.microsoft.com/en-us/%s", path[0] == '/' ? path + 1 : path);
    ReportAdd("------------------------------------------------------------------");
    if (!DocsFetchPage(path, title, 600, text, 250000)) {
        ReportAdd("");
        ReportAdd("ERROR: could not download this page from learn.microsoft.com.");
        ReportAdd("Possible causes: no internet connection, page not found, or");
        ReportAdd("the site is blocking this client. Try again later.");
        ReportAdd("");
        ReportAdd("Offline alternative: use the built-in manual, e.g.");
        ReportAdd("  manual-overview / manual-settings / manual-update ...");
        ShowReport();
        free(title); free(text);
        return;
    }
    if (title[0]) {
        ReportAdd("");
        ReportAdd("ARTICLE: %s", title);
        ReportAdd("------------------------------------------------------------------");
    }
    ReportAdd("%s", text);
    ReportAdd("------------------------------------------------------------------");
    ReportAdd("Source: official Microsoft Learn documentation (learn.microsoft.com)");
    free(title); free(text);
    ShowReport();
}

/* Official Microsoft Learn search API (learn.microsoft.com/api/search). */
static void DocsSearchApi(const char *query) {
    char qenc[1024];
    UrlEncode(query, qenc, sizeof qenc);
    char path[2048];
    snprintf(path, sizeof path, "/api/search?search=%s&locale=en-us&$top=12", qenc);
    char *buf = (char *)malloc(300000);
    if (!buf) { Notify("Out of memory."); return; }
    ReportClear();
    ReportAdd("Microsoft Learn - official documentation search");
    ReportAdd("Query: %s   (API: learn.microsoft.com/api/search)", query);
    ReportAdd("------------------------------------------------------------------");
    int ok = HttpGetTextUA("learn.microsoft.com", path, buf, 300000,
                           L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                           L"AppleWebKit/537.36 (KHTML, like Gecko) "
                           L"Chrome/120.0 Safari/537.36");
    if (!ok) {
        ReportAdd("");
        ReportAdd("ERROR: search failed. No internet connection?");
        ShowReport();
        free(buf);
        return;
    }
    const char *res = strstr(buf, "\"results\"");
    const char *arr = res ? strchr(res, '[') : NULL;
    if (!arr) {
        ReportAdd("No results found.");
        ShowReport();
        free(buf);
        return;
    }
    const char *obj = arr;
    int shown = 0;
    while (shown < 12) {
        obj = strchr(obj, '{');
        if (!obj) break;
        const char *objEnd = strchr(obj, '}');
        if (!objEnd) break;
        char title[600], url[900], desc[2600];
        JsStr(obj, "title", title, sizeof title);
        JsStr(obj, "url", url, sizeof url);
        JsStr(obj, "description", desc, sizeof desc);
        if (!title[0] && !url[0] && !desc[0]) break;
        shown++;
        ReportAdd("");
        ReportAdd("[%d] %s", shown, title[0] ? title : "(untitled)");
        if (url[0]) ReportAdd("    %s", url);
        if (desc[0]) ReportAdd("    %s", desc);
        obj = objEnd + 1;
    }
    if (!shown) {
        ReportAdd("No results found for '%s'.", query);
    } else {
        ReportAdd("");
        ReportAdd("Next steps:  'docs-open <topic>'   opens a result in your browser");
        ReportAdd("             'docs-article <path>' shows the full article text here");
    }
    ShowReport();
    free(buf);
}

/* Fetch a topic page; if that fails, fall back to the official search API. */
static void DocsTopic(const char *path, const char *fallbackQuery) {
    char *title = (char *)malloc(600);
    char *text = (char *)malloc(250000);
    if (!title || !text) { free(title); free(text); Notify("Out of memory."); return; }
    int ok = DocsFetchPage(path, title, 600, text, 250000);
    if (ok) {
        ReportClear();
        ReportAdd("Microsoft Learn - official Windows documentation");
        ReportAdd("https://learn.microsoft.com/en-us/%s", path[0] == '/' ? path + 1 : path);
        ReportAdd("------------------------------------------------------------------");
        if (title[0]) {
            ReportAdd("");
            ReportAdd("ARTICLE: %s", title);
            ReportAdd("------------------------------------------------------------------");
        }
        ReportAdd("%s", text);
        ReportAdd("------------------------------------------------------------------");
        ReportAdd("Source: official Microsoft Learn documentation (learn.microsoft.com)");
        free(title); free(text);
        ShowReport();
        return;
    }
    free(title); free(text);
    DocsSearchApi(fallbackQuery);
}

static void DocsSearchCmd(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: docs-search <query>");
        ReportAdd("Example: docs-search \"change wallpaper\"");
        ReportAdd("Searches the official Microsoft Learn documentation (API:");
        ReportAdd("learn.microsoft.com/api/search) and lists the top matches.");
        ShowReport();
        return;
    }
    DocsSearchApi(arg);
}

static void DocsOpenCmd(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: docs-open <topic>");
        ReportAdd("Examples: docs-open windows/settings");
        ReportAdd("          docs-open windows/security/operating-system-security/virus-and-threat-protection");
        ReportAdd("          docs-open https://learn.microsoft.com/en-us/windows/terminal/");
        ReportAdd("Opens the official Microsoft Learn documentation page in your browser.");
        ShowReport();
        return;
    }
    char url[2300];
    if (strnicmp(arg, "http://", 7) == 0 || strnicmp(arg, "https://", 8) == 0) {
        snprintf(url, sizeof url, "%s", arg);
    } else {
        const char *p = arg;
        if (*p == '/') p++;
        char enc[1700];
        UrlEncode(p, enc, sizeof enc);
        snprintf(url, sizeof url, "https://learn.microsoft.com/en-us/%s", enc);
    }
    RunURI(url);
    NotifyF("Opened in your browser:\n%s", url);
}

static void DocsArticleCmd(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: docs-article <path>");
        ReportAdd("Example: docs-article windows/windows-update");
        ReportAdd("Fetches any official Microsoft Learn article and shows its");
        ReportAdd("readable text here (no browser needed).");
        ShowReport();
        return;
    }
    DocsShowPage(arg);
}

static void DocsWin32ApiCmd(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: docs-win32-api <api-function>");
        ReportAdd("Example: docs-win32-api CreateProcessW");
        ReportAdd("Searches the official Microsoft Win32 API reference docs.");
        ShowReport();
        return;
    }
    char q[700];
    snprintf(q, sizeof q, "%s Win32 API reference learn.microsoft.com", arg);
    DocsSearchApi(q);
}

static void DocsPsCmd(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: docs-powershell-cmd <cmdlet>");
        ReportAdd("Example: docs-powershell-cmd Get-Process");
        ReportAdd("Searches the official Microsoft PowerShell documentation.");
        ShowReport();
        return;
    }
    char q[700];
    snprintf(q, sizeof q, "%s PowerShell cmdlet reference", arg);
    DocsSearchApi(q);
}

static void ManualArticle(const char *title, const char *enc, size_t len) {
    char *buf = (char *)malloc(len + 1);
    if (!buf) { ReportClear(); ReportAdd("(out of memory)"); ShowReport(); return; }
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)(enc[i] ^ (0x5A ^ (i & 0x3F)));
    buf[len] = 0;
    ReportClear();
    ReportAdd("Windows Manual - %s", title);
    ReportAdd("=================================================================");
    ReportAdd("%s", buf);
    ReportAdd("=================================================================");
    ReportAdd("Tip: 'docs-search <query>' searches the official Microsoft docs");
    ReportAdd("     'docs-open <topic>' opens an official page in your browser");
    ShowReport();
    free(buf);
}

#define MAN(id, title, body) \
static void OnManual_##id(const char *a) { (void)a; ManualArticle(title, body, sizeof(body) - 1); }

MAN(overview, "Complete Overview",
"\x0D\x3E\x34\x3A\x31\x32\x39\x7D\x26\x3C\x70\x25\x3E\x32\x74\x02\x23\x25\x2C\x26\x39\x3C\x6C\x00\x23\x2D\x35\x20\x2A\x69\x64\x11\x12\x12\x0B\x59\x19\x0A\x15\x19\x17\x53\x15\x09\x06\x1B\x15\x1C\x04\x18\x48\x1D\x06\x0A\x4C\x00\x0D\x10\x14\x41\x0F\x0A\x14\x0A\x28\x2F\x39\x37\x2A\x55\x2C\x3C\x20\x27\x23"
"\x71\x39\x31\x74\x02\x23\x25\x2C\x26\x39\x3C\x76\x6D\x36\x2B\x25\x61\x22\x22\x37\x2E\x0E\x14\x08\x55\x5E\x0C\x19\x09\x06\x1A\x1E\x16\x05\x5B\x54\x13\x03\x07\x0D\x1A\x42\x4F\x02\x08\x16\x14\x0F\x13\x0D\x0E\x0A\x02\x76\x7B\x2B\x3C\x3D\x2A\x2E\x34\x26\x2A\x7C\x5B\x3B\x36\x3D\x3B\x3E\x2E\x26\x28\x20\x2C"
"\x29\x6D\x23\x2D\x24\x61\x34\x22\x27\x2A\x0C\x1E\x0A\x00\x50\x5F\x39\x0B\x17\x01\x09\x05\x1E\x1E\x1A\x12\x4A\x02\x1B\x49\x0C\x1A\x05\x01\x16\x43\x09\x0F\x46\x4A\x44\x0B\x35\x7B\x31\x37\x2A\x3A\x2E\x33\x37\x27\x70\x3F\x33\x32\x30\x30\x2E\x65\x42\x43\x19\x07\x0D\x19\x62\x0A\x13\x61\x11\x0E\x0A\x01\x35"
"\x2C\x2B\x46\x74\x28\x15\x13\x16\x1C\x07\x02\x56\x1E\x07\x55\x1E\x03\x0D\x49\x01\x1F\x09\x1F\x03\x17\x09\x0F\x01\x47\x17\x1C\x29\x2F\x3D\x34\x7E\x32\x3D\x39\x37\x73\x32\x28\x76\x1A\x3D\x36\x38\x24\x3B\x26\x28\x3B\x62\x6D\x0B\x37\x60\x33\x33\x29\x37\x65\x03\x14\x0D\x0B\x5E\x0F\x0E\x12\x15\x01\x11\x1C"
"\x05\x5B\x7E\x18\x0B\x05\x09\x0E\x0B\x1C\x4C\x14\x0D\x16\x12\x41\x00\x0E\x08\x00\x29\x77\x78\x3A\x31\x31\x32\x38\x31\x27\x23\x71\x2F\x38\x21\x75\x3E\x24\x68\x3D\x26\x2A\x6C\x24\x2C\x37\x25\x33\x28\x22\x30\x65\x1B\x15\x1C\x59\x1D\x10\x12\x09\x00\x1C\x1C\x02\x56\x0E\x1B\x00\x18\x4B\x00\x08\x1C\x0B\x1B"
"\x0C\x10\x06\x6A\x49\x15\x04\x16\x00\x3F\x35\x74\x79\x35\x3A\x25\x3F\x3D\x32\x22\x35\x7A\x77\x39\x3A\x3F\x38\x2D\x65\x6E\x3F\x3E\x24\x2C\x37\x25\x33\x35\x6B\x64\x26\x1B\x16\x1D\x0B\x1F\x0C\x52\x53\x5C\x5A\x5E\x51\x22\x1F\x11\x55\x09\x1E\x1A\x1B\x0B\x01\x18\x4D\x14\x06\x12\x12\x0F\x08\x0A\x16\x7A\x3A"
"\x2A\x3C\x54\x08\x35\x33\x36\x3C\x27\x22\x76\x66\x65\x75\x2B\x25\x2C\x69\x19\x26\x22\x29\x2D\x34\x33\x61\x77\x77\x6A\x4F\x70\x2F\x30\x3C\x5E\x39\x33\x28\x20\x53\x33\x3E\x24\x39\x31\x27\x39\x4B\x27\x2F\x4E\x38\x25\x23\x26\x2C\x37\x32\x6C\x56\x4D\x45\x1E\x3E\x2B\x32\x2A\x30\x2C\x7D\x7F\x73\x24\x39\x33"
"\x77\x39\x34\x23\x25\x68\x3A\x2D\x3D\x29\x28\x2C\x63\x37\x28\x32\x2F\x64\x31\x12\x1E\x58\x0D\x1F\x0C\x17\x1F\x13\x01\x50\x10\x02\x57\x00\x1D\x0F\x4B\x0A\x06\x1A\x1B\x03\x00\x4C\x69\x52\x48\x46\x34\x10\x04\x28\x2F\x78\x34\x3B\x31\x29\x7D\x7F\x73\x20\x23\x33\x24\x27\x75\x3E\x23\x2D\x69\x19\x26\x22\x29"
"\x2D\x34\x33\x61\x2D\x22\x3D\x65\x52\x2C\x11\x17\x57\x5F\x08\x12\x52\x1C\x00\x14\x18\x57\x1D\x01\x50\x4B\x1B\x0C\x0F\x1D\x0F\x05\x4E\x43\x10\x08\x08\x09\x01\x01\x50\x7B\x78\x79\x3F\x2F\x2C\x2E\x7E\x73\x20\x3E\x21\x32\x26\x75\x25\x3B\x3C\x20\x21\x21\x3F\x6D\x6A\x10\x2C\x24\x23\x37\x64\x6A\x5A\x28\x10"
"\x0C\x0A\x5F\x18\x12\x05\x1D\x50\x5E\x56\x25\x11\x06\x1E\x0A\x1A\x1D\x47\x41\x66\x5E\x4B\x43\x33\x04\x12\x13\x0D\x0B\x3D\x28\x78\x74\x7E\x08\x35\x33\x79\x1A\x70\x3E\x26\x32\x3A\x26\x6A\x3F\x20\x2C\x6E\x1C\x29\x39\x36\x2A\x2E\x26\x35\x67\x25\x35\x0A\x5B\x0F\x11\x1B\x0D\x19\x5D\x13\x1F\x1D\x1E\x05\x03"
"\x54\x10\x1C\x0E\x1A\x10\x1A\x07\x05\x03\x05\x43\x03\x00\x08\x47\x06\x00\x50\x7B\x78\x79\x3D\x30\x32\x3B\x3B\x34\x25\x23\x33\x33\x7A\x75\x1E\x23\x2D\x69\x2D\x23\x2D\x3E\x31\x2A\x23\x61\x05\x28\x2A\x31\x08\x14\x14\x59\x2E\x1E\x12\x18\x1E\x53\x03\x05\x1F\x1B\x18\x55\x0F\x13\x01\x1A\x1A\x1C\x4C\x19\x0D"
"\x0C\x4E\x6B\x52\x4E\x44\x23\x33\x37\x3D\x79\x1B\x27\x2C\x31\x3D\x21\x35\x23\x76\x7A\x74\x02\x23\x25\x63\x0C\x6E\x3C\x24\x22\x35\x30\x60\x38\x29\x32\x36\x65\x1C\x12\x14\x1C\x0D\x5F\x1D\x13\x16\x53\x16\x1E\x1A\x13\x11\x07\x19\x45\x62\x63\x2A\x2E\x25\x21\x3B\x43\x32\x2E\x33\x33\x2D\x2B\x1F\x51\x75\x79"
"\x11\x2F\x39\x33\x72\x32\x70\x21\x24\x38\x33\x27\x2B\x26\x72\x69\x19\x26\x22\x61\x62\x37\x39\x31\x23\x67\x2D\x31\x09\x5B\x16\x18\x13\x1A\x50\x5D\x02\x01\x15\x02\x05\x57\x31\x1B\x1E\x0E\x1A\x47\x64\x42\x4C\x3E\x15\x0A\x14\x02\x0E\x47\x06\x00\x2E\x2C\x3D\x3C\x30\x7F\x2C\x2F\x3D\x34\x22\x30\x3B\x24\x6E"
"\x75\x0B\x27\x3C\x62\x1A\x2E\x2E\x63\x48\x6E\x60\x02\x29\x37\x3D\x65\x1B\x15\x1C\x59\x0E\x1E\x0F\x09\x17\x49\x50\x32\x02\x05\x18\x5E\x29\x47\x48\x2A\x1A\x1D\x00\x46\x34\x43\x48\x22\x13\x13\x5E\x45\x19\x2F\x2A\x35\x75\x07\x75\x73\x58\x7E\x70\x05\x37\x3C\x31\x75\x2B\x6B\x3B\x2A\x3C\x2A\x29\x23\x31\x2B"
"\x2F\x35\x7C\x67\x13\x2C\x14\x50\x2B\x11\x17\x19\x08\x56\x21\x53\x58\x1E\x04\x57\x24\x07\x03\x05\x1C\x49\x3D\x0C\x1E\x08\x07\x0D\x49\x4F\x6C\x4A\x44\x29\x35\x38\x33\x79\x27\x30\x29\x2F\x72\x03\x13\x6B\x76\x00\x3D\x3B\x61\x07\x66\x43\x44\x04\x09\x08\x12\x63\x17\x08\x08\x03\x0B\x12\x29\x5B\x30\x3C\x3F"
"\x33\x28\x35\x2B\x79\x5D\x51\x3A\x12\x00\x55\x3D\x02\x06\x0D\x01\x18\x1F\x4D\x37\x13\x04\x00\x12\x02\x44\x0C\x34\x28\x2C\x38\x32\x33\x7C\x28\x22\x37\x31\x25\x33\x24\x74\x27\x2F\x2C\x3D\x25\x2F\x3D\x20\x34\x62\x6B\x13\x24\x32\x33\x2D\x2B\x1D\x08\x58\x47\x5E\x28\x15\x13\x16\x1C\x07\x02\x56\x22\x04\x11"
"\x0B\x1F\x0D\x40\x40\x65\x41\x4D\x2F\x02\x0B\x04\x46\x05\x05\x06\x31\x2E\x28\x2A\x7E\x30\x3A\x7D\x3B\x3E\x20\x3E\x24\x23\x35\x3B\x3E\x6B\x2E\x20\x22\x2A\x3F\x6D\x6A\x05\x29\x2D\x23\x67\x0C\x2C\x09\x0F\x17\x0B\x07\x5F\x53\x5D\x3D\x1D\x15\x35\x04\x1E\x02\x10\x4A\x44\x48\x1D\x06\x06\x1F\x4D\x12\x11\x0F"
"\x06\x14\x06\x09\x42\x29\x51\x78\x79\x3C\x3E\x3F\x36\x27\x23\x70\x25\x39\x38\x38\x26\x63\x65\x42\x64\x6E\x1D\x39\x23\x62\x37\x28\x28\x35\x67\x34\x37\x15\x1C\x0A\x18\x13\x58\x0F\x5D\x01\x10\x11\x1F\x05\x57\x1A\x1A\x1D\x4B\x09\x07\x0A\x4F\x18\x05\x07\x0D\x5A\x41\x02\x0E\x17\x0E\x7A\x38\x30\x3C\x3D\x34"
"\x70\x7D\x21\x35\x33\x7C\x25\x34\x35\x3B\x24\x24\x3F\x65\x6E\x0B\x05\x1E\x0F\x6D\x4A\x6C\x66\x03\x2B\x65\x14\x14\x0C\x59\x1A\x16\x0F\x1C\x10\x1F\x15\x51\x0F\x18\x01\x07\x4A\x0A\x06\x1D\x07\x19\x05\x1F\x17\x10\x40\x49\x2B\x0E\x07\x17\x35\x28\x37\x3F\x2A\x7F\x18\x38\x34\x36\x3E\x35\x33\x25\x7D\x75\x3D"
"\x22\x3C\x21\x21\x3A\x38\x6D\x23\x63\x32\x24\x27\x34\x2B\x2B\x54\x71\x72\x2E\x36\x3A\x32\x5D\x21\x3C\x3D\x34\x22\x3F\x3D\x3B\x2D\x4B\x2A\x3B\x2B\x2E\x27\x3E\x68\x52\x49\x41\x34\x02\x17\x11\x3B\x29\x2C\x79\x2A\x37\x39\x7D\x02\x10\x70\x7C\x76\x31\x3D\x2D\x2F\x38\x68\x24\x21\x3C\x38\x6D\x31\x2E\x21\x2D"
"\x2A\x67\x34\x37\x15\x19\x14\x1C\x13\x0C\x52\x77\x40\x5A\x50\x24\x05\x12\x54\x01\x02\x0E\x48\x0B\x1B\x06\x00\x19\x4F\x0A\x0E\x41\x12\x15\x0B\x10\x38\x37\x3D\x2A\x36\x30\x33\x29\x37\x21\x23\x71\x7E\x04\x31\x21\x3E\x22\x26\x2E\x3D\x6F\x72\x6D\x11\x3A\x33\x35\x23\x2A\x64\x7B\x5A\x2F\x0A\x16\x0B\x1D\x10"
"\x18\x01\x1B\x1F\x1E\x02\x5E\x5A\x7F\x59\x42\x48\x2A\x06\x0A\x0F\x06\x42\x17\x08\x04\x46\x22\x12\x00\x34\x2F\x78\x0F\x37\x3A\x2B\x38\x20\x73\x36\x3E\x24\x77\x31\x27\x38\x24\x3A\x69\x2A\x2A\x38\x2C\x2B\x2F\x33\x61\x6E\x33\x2C\x2C\x09\x5B\x08\x0B\x11\x18\x0E\x1C\x1F\x49\x50\x14\x00\x12\x1A\x01\x47\x1D"
"\x01\x0C\x19\x0A\x1E\x44\x4C\x69\x54\x48\x46\x35\x11\x0B\x7A\x28\x3E\x3A\x73\x2C\x3F\x3C\x3C\x3D\x3F\x26\x76\x36\x3A\x31\x6A\x2F\x21\x3A\x23\x62\x2F\x25\x27\x20\x2B\x61\x6E\x33\x2C\x2C\x09\x5B\x08\x0B\x11\x18\x0E\x1C\x1F\x5F\x50\x3C\x17\x1E\x1A\x01\x0F\x05\x09\x07\x0D\x0A\x4C\x4B\x42\x31\x05\x11\x07"
"\x0E\x16\x4C\x74\x51\x6D\x70\x7E\x0A\x2F\x38\x72\x00\x29\x22\x22\x32\x39\x75\x18\x2E\x3B\x3D\x21\x3D\x29\x6D\x2D\x31\x60\x13\x23\x34\x21\x31\x5A\x0F\x10\x10\x0D\x5F\x2C\x3E\x52\x1A\x16\x51\x02\x1F\x11\x55\x19\x12\x1B\x1D\x0B\x02\x4C\x04\x11\x43\x02\x00\x02\x0B\x1D\x45\x3E\x3A\x35\x38\x39\x3A\x38\x73"
"\x58\x59\x11\x13\x19\x02\x00\x75\x1E\x03\x01\x1A\x6E\x02\x0D\x03\x17\x02\x0C\x4B\x03\x31\x21\x37\x03\x5B\x17\x0D\x16\x1A\x0E\x5D\x55\x1E\x11\x1F\x03\x16\x18\x58\x40\x4C\x48\x0A\x01\x02\x01\x0C\x0C\x07\x40\x02\x09\x11\x01\x17\x29\x7B\x37\x37\x3B\x7F\x28\x32\x22\x3A\x33\x71\x3F\x39\x74\x31\x2F\x3F\x29"
"\x20\x22\x61\x6C\x19\x2A\x26\x60\x66\x22\x28\x27\x36\x57\x51\x5F\x73\x1D\x10\x11\x10\x13\x1D\x14\x02\x56\x11\x11\x01\x09\x03\x48\x1D\x06\x0A\x4C\x22\x24\x25\x29\x22\x2F\x26\x28\x45\x17\x32\x3B\x2B\x31\x2C\x33\x3B\x26\x73\x34\x3E\x35\x22\x39\x30\x24\x3F\x29\x3D\x27\x20\x22\x6D\x2E\x2A\x36\x24\x66\x21"
"\x36\x2A\x17\x71\x14\x1C\x1F\x0D\x12\x53\x1F\x1A\x13\x03\x19\x04\x1B\x13\x1E\x45\x0B\x06\x03\x4F\x1B\x05\x07\x0D\x40\x18\x09\x12\x44\x04\x28\x3E\x78\x36\x30\x33\x35\x33\x37\x69\x70\x35\x39\x34\x27\x78\x39\x2E\x29\x3B\x2D\x27\x60\x6D\x26\x2C\x23\x32\x6B\x28\x34\x20\x14\x57\x72\x1D\x11\x1C\x0F\x50\x13"
"\x01\x04\x18\x15\x1B\x11\x55\x0B\x05\x0C\x49\x1A\x07\x09\x4D\x10\x06\x01\x05\x1F\x4A\x09\x04\x3E\x3E\x78\x2D\x31\x2F\x35\x3E\x72\x30\x3F\x3C\x3B\x36\x3A\x31\x39\x6B\x60\x2D\x21\x2C\x3F\x60\x37\x33\x24\x20\x32\x22\x68\x65\x1E\x14\x1B\x0A\x53\x08\x0F\x11\x5C\x5D\x5E\x58\x58"
);

MAN(desktop, "Desktop, Start Menu & Taskbar",
"\x0E\x13\x1D\x79\x1A\x1A\x0F\x16\x06\x1C\x00\x5B\x0F\x38\x21\x27\x6A\x26\x29\x20\x20\x6F\x3F\x2E\x30\x26\x25\x2F\x68\x67\x16\x2C\x1D\x13\x0C\x54\x1D\x13\x15\x1E\x19\x53\x19\x05\x56\x03\x1B\x55\x1A\x0E\x1A\x1A\x01\x01\x0D\x01\x0B\x19\x05\x5B\x46\x24\x0C\x04\x34\x3C\x3D\x79\x3C\x3E\x3F\x36\x35\x21\x3F"
"\x24\x38\x33\x78\x5F\x1A\x2E\x3A\x3A\x21\x21\x2D\x21\x2B\x39\x25\x6D\x66\x03\x2D\x36\x0A\x17\x19\x00\x5E\x0C\x19\x09\x06\x1A\x1E\x16\x05\x5B\x54\x3B\x0F\x1C\x48\x57\x4E\x29\x03\x01\x06\x06\x12\x4E\x35\x0F\x0B\x17\x2E\x38\x2D\x2D\x70\x7F\x18\x38\x21\x38\x24\x3E\x26\x77\x3D\x36\x25\x25\x3B\x69\x2D\x2E"
"\x22\x6D\x20\x26\x4A\x32\x2E\x28\x33\x2B\x5A\x14\x0A\x59\x16\x16\x18\x19\x17\x1D\x50\x59\x25\x12\x00\x01\x03\x05\x0F\x1A\x4E\x51\x4C\x3D\x07\x11\x13\x0E\x08\x06\x08\x0C\x20\x3A\x2C\x30\x31\x31\x7C\x63\x72\x07\x38\x34\x3B\x32\x27\x75\x74\x6B\x0C\x2C\x3D\x24\x38\x22\x32\x63\x29\x22\x29\x29\x4E\x36\x1F"
"\x0F\x0C\x10\x10\x18\x0F\x46\x52\x07\x18\x18\x05\x57\x04\x07\x05\x0C\x1A\x08\x03\x4F\x04\x0C\x11\x43\x14\x16\x03\x06\x0F\x16\x7A\x3D\x37\x2B\x7E\x0B\x34\x34\x21\x73\x00\x12\x7A\x77\x06\x30\x29\x32\x2B\x25\x2B\x6F\x0E\x24\x2C\x63\x21\x2F\x22\x67\x29\x2A\x08\x1E\x51\x57\x74\x75\x28\x35\x37\x53\x23\x25"
"\x37\x25\x20\x55\x27\x2E\x26\x3C\x4E\x47\x3B\x04\x0C\x43\x0B\x04\x1F\x4E\x6E\x48\x7A\x0B\x31\x37\x30\x3A\x38\x7D\x33\x23\x20\x22\x76\x36\x20\x75\x3E\x23\x2D\x69\x3A\x20\x3C\x61\x62\x11\x25\x22\x29\x2A\x29\x20\x14\x1F\x1D\x1D\x5E\x19\x15\x11\x17\x00\x50\x13\x13\x1B\x1B\x02\x44\x61\x45\x49\x3A\x16\x1C"
"\x08\x42\x02\x0E\x18\x12\x0F\x0D\x0B\x3D\x7B\x2C\x36\x7E\x2C\x39\x3C\x20\x30\x38\x71\x37\x27\x24\x26\x66\x6B\x2E\x20\x22\x2A\x3F\x61\x62\x30\x25\x35\x32\x2E\x2A\x22\x09\x5B\x19\x17\x1A\x5F\x08\x15\x17\x53\x07\x14\x14\x59\x7E\x58\x4A\x39\x01\x0E\x06\x1B\x41\x0E\x0E\x0A\x03\x0A\x46\x06\x0A\x1C\x7A\x3A"
"\x28\x29\x7E\x2B\x33\x7D\x22\x3A\x3E\x7E\x23\x39\x24\x3C\x24\x67\x68\x26\x3C\x6F\x28\x3F\x23\x24\x60\x28\x32\x67\x2B\x2B\x0E\x14\x58\x0D\x16\x1A\x5C\x09\x13\x00\x1B\x13\x17\x05\x5A\x7F\x47\x4B\x3A\x00\x09\x07\x18\x40\x01\x0C\x12\x0F\x03\x15\x44\x15\x35\x2C\x3D\x2B\x7E\x3D\x29\x29\x26\x3C\x3E\x6B\x76"
"\x04\x38\x30\x2F\x3B\x68\x66\x6E\x1C\x24\x38\x36\x63\x24\x2E\x31\x29\x64\x6A\x5A\x29\x1D\x0A\x0A\x1E\x0E\x09\x5C\x79\x7A\x25\x3E\x32\x54\x21\x2B\x38\x23\x2B\x2F\x3D\x66\x40\x42\x2F\x05\x07\x12\x5D\x44\x36\x2E\x3A\x2A\x2D\x7E\x3D\x29\x29\x26\x3C\x3E\x7D\x76\x24\x31\x34\x38\x28\x20\x65\x6E\x3B\x2D\x3E"
"\x29\x63\x36\x28\x23\x30\x68\x65\x0D\x12\x1C\x1E\x1B\x0B\x0F\x5D\x5A\x24\x19\x1F\x12\x18\x03\x06\x4A\x5A\x59\x40\x40\x65\x41\x4D\x2F\x0A\x04\x05\x0A\x02\x5E\x45\x2A\x32\x36\x37\x3B\x3B\x7C\x3C\x3C\x37\x70\x23\x23\x39\x3A\x3C\x24\x2C\x68\x28\x3E\x3F\x3F\x63\x62\x00\x2C\x28\x25\x2C\x64\x31\x15\x5B\x0B"
"\x0E\x17\x0B\x1F\x15\x5E\x53\x02\x18\x11\x1F\x00\x58\x09\x07\x01\x0A\x05\x4F\x0A\x02\x10\x43\x01\x6B\x46\x47\x0E\x10\x37\x2B\x78\x35\x37\x2C\x28\x7D\x7A\x21\x35\x32\x33\x39\x20\x75\x2C\x22\x24\x2C\x3D\x66\x60\x6D\x21\x2F\x29\x22\x2D\x67\x30\x2D\x1F\x5B\x19\x09\x0E\x5F\x1D\x1A\x13\x1A\x1E\x51\x02\x18"
"\x54\x18\x03\x05\x01\x04\x07\x15\x09\x42\x10\x06\x13\x15\x09\x15\x01\x4B\x50\x76\x78\x0B\x37\x38\x34\x29\x68\x73\x23\x28\x25\x23\x31\x38\x6A\x3F\x3A\x28\x37\x6F\x61\x6D\x21\x2F\x2F\x22\x2D\x6B\x64\x33\x15\x17\x0D\x14\x1B\x53\x5C\x13\x17\x07\x07\x1E\x04\x1C\x58\x55\x08\x0A\x1C\x1D\x0B\x1D\x15\x4D\x03"
"\x0D\x04\x41\x0E\x0E\x00\x01\x3F\x35\x78\x30\x3D\x30\x32\x2E\x58\x73\x70\x79\x22\x3F\x31\x75\x39\x26\x29\x25\x22\x6F\x39\x3D\x6F\x22\x32\x33\x29\x30\x64\x36\x12\x14\x0F\x0A\x5E\x0B\x14\x18\x1F\x5A\x5E\x7B\x7C\x23\x35\x26\x21\x29\x29\x3B\x4E\x3B\x25\x3D\x31\x69\x4D\x41\x36\x0E\x0A\x45\x3B\x7B\x2A\x2C"
"\x30\x31\x35\x33\x35\x73\x31\x21\x26\x6D\x74\x27\x23\x2C\x20\x3D\x63\x2C\x20\x24\x21\x28\x60\x28\x32\x34\x64\x2C\x19\x14\x16\x59\x40\x5F\x2C\x14\x1C\x53\x04\x1E\x56\x03\x15\x06\x01\x09\x09\x1B\x40\x65\x41\x4D\x2F\x0C\x16\x04\x46\x13\x0C\x00\x7A\x2F\x39\x2A\x35\x3D\x3D\x2F\x68\x73\x22\x38\x31\x3F\x20"
"\x78\x29\x27\x21\x2A\x25\x6F\x25\x39\x62\x7D\x60\x15\x27\x34\x2F\x27\x1B\x09\x58\x0A\x1B\x0B\x08\x14\x1C\x14\x03\x51\x5E\x18\x06\x55\x1F\x05\x04\x06\x0D\x04\x4C\x0C\x0C\x07\x40\x05\x14\x06\x03\x5E\x50\x7B\x78\x3A\x32\x3E\x2F\x2E\x3B\x30\x70\x33\x33\x3F\x35\x23\x23\x24\x3A\x69\x27\x21\x6C\x1E\x27\x37"
"\x34\x28\x28\x20\x37\x65\x44\x5B\x28\x1C\x0C\x0C\x13\x13\x13\x1F\x19\x0B\x17\x03\x1D\x1A\x04\x4B\x56\x49\x3A\x0E\x1F\x06\x00\x02\x12\x48\x48\x6D\x49\x45\x09\x33\x37\x2E\x7E\x2C\x39\x3E\x3D\x3D\x34\x22\x76\x3E\x3A\x75\x3E\x23\x2D\x69\x2D\x23\x23\x2E\x29\x6F\x60\x32\x2B\x26\x28\x29\x5A\x12\x1B\x16\x10"
"\x0C\x50\x5D\x11\x1C\x1D\x13\x1F\x19\x11\x55\x08\x1E\x1C\x1D\x01\x01\x1F\x57\x42\x02\x0C\x0D\x46\x06\x12\x04\x33\x37\x39\x3B\x32\x3A\x56\x7D\x72\x3A\x3E\x71\x22\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x62\x36\x2E\x25\x23\x35\x64\x16\x03\x08\x0C\x1C\x13\x5F\x28\x0A\x17\x12\x1B\x02\x58\x7D\x59\x55"
"\x3D\x02\x06\x0D\x01\x18\x1F\x4D\x53\x52\x5A\x41\x12\x06\x17\x0E\x38\x3A\x2A\x79\x3D\x30\x2E\x33\x37\x21\x70\x3E\x20\x32\x26\x33\x26\x24\x3F\x65\x6E\x3C\x29\x2C\x30\x20\x28\x61\x24\x28\x3C\x65\x09\x0F\x01\x15\x1B\x0C\x5C\x1C\x1C\x17\x50\x06\x1F\x13\x13\x10\x1E\x61\x48\x49\x0C\x1A\x18\x19\x0D\x0D\x13"
"\x41\x07\x15\x01\x45\x3B\x37\x34\x79\x3D\x30\x32\x3B\x3B\x34\x25\x23\x37\x35\x38\x30\x6A\x22\x26\x69\x3A\x2E\x3F\x26\x20\x22\x32\x61\x35\x22\x30\x31\x13\x15\x1F\x0A\x50\x75\x76\x33\x3D\x27\x39\x37\x3F\x34\x35\x21\x23\x24\x26\x3A\x4E\x49\x4C\x3C\x37\x2A\x23\x2A\x46\x34\x21\x31\x0E\x12\x16\x1E\x0D\x55"
"\x71\x7D\x05\x3A\x3E\x7A\x18\x6D\x74\x3B\x25\x3F\x21\x2F\x27\x2C\x2D\x39\x2B\x2C\x2E\x32\x66\x6F\x0A\x2A\x0E\x12\x1E\x10\x1D\x1E\x08\x14\x1D\x1D\x50\x32\x13\x19\x00\x10\x18\x42\x46\x63\x43\x4F\x3B\x04\x0C\x48\x21\x5B\x46\x36\x11\x0C\x39\x30\x78\x0A\x3B\x2B\x28\x34\x3C\x34\x23\x71\x7E\x00\x3D\x78\x0C"
"\x22\x64\x69\x0C\x23\x39\x28\x36\x2C\x2F\x35\x2E\x6B\x64\x27\x08\x12\x1F\x11\x0A\x11\x19\x0E\x01\x5F\x50\x07\x19\x1B\x01\x18\x0F\x47\x48\x0B\x0F\x1B\x18\x08\x10\x1A\x6A\x41\x46\x14\x05\x13\x3F\x29\x74\x79\x38\x30\x3F\x28\x21\x73\x31\x22\x25\x3E\x27\x21\x66\x6B\x26\x20\x29\x27\x38\x6D\x2E\x2A\x27\x29"
"\x32\x69\x6A\x6B\x53\x55\x72\x73\x29\x36\x32\x39\x3D\x24\x50\x22\x38\x36\x24\x25\x23\x25\x2F\x63\x43\x4F\x28\x1F\x03\x04\x40\x00\x46\x10\x0D\x0B\x3E\x34\x2F\x79\x2A\x30\x7C\x3C\x72\x20\x33\x23\x33\x32\x3A\x75\x2F\x2F\x2F\x2C\x6E\x3B\x23\x6D\x31\x2D\x21\x31\x66\x2E\x30\x65\x12\x1A\x14\x1F\x53\x0C\x1F"
"\x0F\x17\x16\x1E\x4A\x56\x1F\x1B\x03\x0F\x19\x48\x1D\x06\x0A\x66\x4D\x42\x0E\x01\x19\x0F\x0A\x0D\x1F\x3F\x7B\x3A\x2C\x2A\x2B\x33\x33\x72\x35\x3F\x23\x76\x04\x3A\x34\x3A\x6B\x04\x28\x37\x20\x39\x39\x31\x63\x68\x16\x2F\x29\x75\x74\x53\x55\x58\x2E\x17\x11\x57\x3C\x00\x01\x1F\x06\x56\x1C\x11\x0C\x19\x4B"
"\x1B\x07\x0F\x1F\x4C\x19\x0D\x0C\x4E\x6B\x4B\x47\x37\x0B\x3B\x2B\x78\x35\x3F\x26\x33\x28\x26\x20\x70\x30\x38\x33\x74\x26\x24\x2A\x38\x69\x2F\x3C\x3F\x24\x31\x37\x60\x22\x27\x29\x64\x27\x1F\x5B\x0C\x0C\x10\x1A\x18\x5D\x1B\x1D\x50\x22\x13\x03\x00\x1C\x04\x0C\x1B\x49\x50\x4F\x3F\x14\x11\x17\x05\x0C\x46"
"\x59\x6E\x45\x7A\x16\x2D\x35\x2A\x36\x28\x3C\x21\x38\x39\x3F\x31\x77\x7C\x21\x22\x22\x3B\x69\x3E\x3D\x23\x2A\x30\x22\x2D\x7B\x66\x34\x2A\x24\x0A\x56\x0F\x10\x10\x1B\x13\x0A\x01\x5E\x1F\x1F\x56\x58\x54\x18\x1F\x07\x1C\x00\x1A\x0E\x1F\x06\x0B\x0D\x07\x48\x48\x6D\x6E\x28\x0F\x17\x0C\x10\x0E\x13\x19\x7D"
"\x16\x16\x03\x1A\x02\x18\x04\x06\x6A\x63\x1F\x20\x20\x64\x0F\x39\x30\x2F\x6B\x05\x6F\x4D\x69\x65\x2C\x12\x0A\x0D\x0B\x1E\x10\x5D\x16\x16\x03\x1A\x02\x18\x04\x06\x4A\x00\x0D\x0C\x1E\x4F\x08\x04\x04\x05\x05\x13\x03\x09\x10\x45\x29\x3E\x2C\x2A\x7E\x30\x3A\x7D\x25\x3A\x3E\x35\x39\x20\x27\x7B\x6A\x1C\x21"
"\x27\x65\x0C\x38\x3F\x2E\x68\x0C\x24\x20\x33\x6B\x17\x13\x1C\x10\x0D\x74\x5F\x5C\x0E\x05\x1A\x04\x12\x1E\x12\x07\x59\x4A\x3C\x01\x07\x45\x2C\x18\x1F\x0E\x48\x26\x55\x46\x04\x08\x0A\x29\x3E\x2B\x79\x2A\x37\x39\x7D\x31\x26\x22\x23\x33\x39\x20\x75\x25\x25\x2D\x67\x6E\x1C\x29\x28\x62\x2E\x21\x2F\x33\x26"
"\x28\x68\x1E\x1E\x0B\x12\x0A\x10\x0C\x0E\x5C\x79\x7A\x3D\x3F\x21\x31\x55\x3D\x2A\x24\x25\x3E\x2E\x3C\x28\x30\x43\x4F\x41\x32\x2F\x21\x28\x1F\x08\x78\x76\x7E\x1C\x13\x11\x1D\x01\x03\x5B\x7B\x77\x07\x30\x3E\x3F\x21\x27\x29\x3C\x6C\x73\x62\x13\x25\x33\x35\x28\x2A\x24\x16\x12\x02\x18\x0A\x16\x13\x13\x48"
"\x53\x12\x10\x15\x1C\x13\x07\x05\x1E\x06\x0D\x42\x4F\x0F\x02\x0E\x0C\x12\x12\x4A\x47\x05\x06\x39\x3E\x36\x2D\x7E\x3C\x33\x31\x3D\x21\x7C\x71\x22\x3F\x31\x38\x2F\x38\x64\x43\x6E\x6F\x20\x22\x21\x28\x60\x32\x25\x35\x21\x20\x14\x57\x58\x1F\x11\x11\x08\x0E\x5C\x53\x24\x19\x1F\x04\x54\x05\x18\x04\x0F\x1B"
"\x0F\x02\x4C\x05\x03\x10\x40\x0D\x0F\x00\x0C\x11\x75\x3F\x39\x2B\x35\x7F\x28\x35\x37\x3E\x35\x71\x37\x39\x30\x75\x2B\x28\x2B\x2C\x20\x3B\x6C\x39\x35\x26\x21\x2A\x35\x69\x4E\x68\x5A\x3A\x58\x0A\x12\x16\x18\x18\x01\x1B\x1F\x06\x56\x18\x12\x55\x03\x06\x09\x0E\x0B\x1C\x4C\x0E\x03\x0D\x40\x03\x03\x47\x11"
"\x16\x3F\x3F\x78\x38\x2D\x7F\x3E\x3C\x31\x38\x37\x23\x39\x22\x3A\x31\x64"
);

MAN(settings, "Settings App Tour",
"\x09\x1E\x0C\x0D\x17\x11\x1B\x0E\x72\x7B\x07\x38\x38\x7C\x1D\x7C\x6A\x22\x3B\x69\x3A\x27\x29\x6D\x2F\x22\x29\x2F\x66\x24\x2B\x2B\x1C\x12\x1F\x0C\x0C\x1E\x08\x14\x1D\x1D\x50\x10\x06\x07\x54\x1A\x0C\x4B\x3F\x00\x00\x0B\x03\x1A\x11\x4D\x40\x29\x03\x15\x01\x45\x33\x28\x78\x38\x7E\x2B\x33\x28\x20\x7D\x5A"
"\x5B\x67\x7E\x74\x06\x13\x18\x1C\x0C\x03\x45\x08\x24\x31\x33\x2C\x20\x3F\x67\x6C\x37\x1F\x08\x17\x15\x0B\x0B\x15\x12\x1C\x5F\x50\x13\x04\x1E\x13\x1D\x1E\x05\x0D\x1A\x1D\x43\x4C\x03\x0B\x04\x08\x15\x46\x0B\x0D\x02\x32\x2F\x74\x79\x16\x1B\x0E\x71\x72\x3E\x25\x3D\x22\x3E\x24\x39\x2F\x6B\x25\x26\x20\x26"
"\x38\x22\x30\x30\x69\x6D\x4C\x14\x2B\x30\x14\x1F\x54\x59\x30\x10\x08\x14\x14\x1A\x13\x10\x02\x1E\x1B\x1B\x19\x47\x48\x2F\x01\x0C\x19\x1E\x4E\x43\x30\x0E\x11\x02\x16\x45\x7C\x7B\x3A\x38\x2A\x2B\x39\x2F\x2B\x7F\x70\x02\x22\x38\x26\x34\x2D\x2E\x68\x61\x1D\x3B\x23\x3F\x23\x24\x25\x61\x15\x22\x2A\x36\x1F"
"\x57\x72\x1A\x12\x1A\x1D\x13\x07\x03\x59\x5D\x56\x39\x11\x14\x18\x09\x11\x49\x1D\x07\x0D\x1F\x0B\x0D\x07\x4D\x46\x2A\x11\x09\x2E\x32\x2C\x38\x2D\x34\x35\x33\x35\x7F\x70\x01\x24\x38\x3E\x30\x29\x3F\x64\x69\x1C\x2A\x21\x22\x36\x26\x60\x05\x23\x34\x2F\x31\x15\x0B\x54\x73\x3D\x13\x15\x0D\x10\x1C\x11\x03"
"\x12\x5B\x54\x34\x08\x04\x1D\x1D\x4E\x47\x08\x08\x14\x0A\x03\x04\x46\x14\x14\x00\x39\x28\x74\x79\x2C\x3A\x32\x3C\x3F\x36\x70\x01\x15\x7B\x74\x02\x23\x25\x2C\x26\x39\x3C\x6C\x2C\x21\x37\x29\x37\x27\x33\x2D\x2A\x14\x52\x56\x73\x74\x4D\x55\x5D\x30\x3F\x25\x34\x22\x38\x3B\x21\x22\x4B\x4E\x49\x2A\x2A\x3A"
"\x24\x21\x26\x33\x6B\x24\x0B\x11\x00\x2E\x34\x37\x2D\x36\x73\x7C\x0D\x20\x3A\x3E\x25\x33\x25\x27\x75\x6C\x6B\x3B\x2A\x2F\x21\x22\x28\x30\x30\x6C\x61\x0B\x28\x31\x36\x1F\x57\x58\x2D\x11\x0A\x1F\x15\x02\x12\x14\x5D\x56\x23\x0D\x05\x03\x05\x0F\x45\x4E\x3F\x09\x03\x4E\x43\x34\x0E\x13\x04\x0C\x49\x50\x18"
"\x39\x34\x3B\x2D\x3D\x2E\x7E\x73\x05\x02\x14\x7B\x74\x05\x22\x24\x26\x2C\x6E\x03\x25\x23\x29\x6F\x60\x00\x33\x33\x2B\x15\x16\x1A\x01\x57\x74\x75\x4F\x54\x52\x3D\x35\x25\x21\x38\x26\x3E\x4A\x4D\x48\x20\x20\x3B\x29\x3F\x2C\x26\x34\x6B\x31\x0E\x49\x23\x33\x77\x78\x1C\x2A\x37\x39\x2F\x3C\x36\x24\x7D\x76"
"\x01\x04\x1B\x66\x6B\x05\x26\x2C\x26\x20\x28\x62\x2B\x2F\x35\x35\x37\x2B\x31\x56\x5B\x39\x10\x0C\x0F\x10\x1C\x1C\x16\x50\x1C\x19\x13\x11\x59\x4A\x3B\x1A\x06\x16\x16\x40\x4D\x26\x02\x14\x00\x46\x12\x17\x04\x3D\x3E\x74\x53\x1F\x3B\x2A\x3C\x3C\x30\x35\x35\x76\x39\x31\x21\x3D\x24\x3A\x22\x6E\x3C\x29\x39"
"\x36\x2A\x2E\x26\x35\x67\x6C\x24\x1E\x1A\x08\x0D\x1B\x0D\x5C\x12\x02\x07\x19\x1E\x18\x04\x58\x55\x2E\x25\x3B\x45\x4E\x1D\x09\x1E\x07\x17\x49\x4F\x6C\x6D\x50\x4C\x7A\x0B\x1D\x0B\x0D\x10\x12\x1C\x1E\x1A\x0A\x10\x02\x1E\x1B\x1B\x40\x09\x29\x2A\x25\x28\x3E\x22\x37\x2D\x24\x6D\x66\x04\x2B\x29\x15\x09\x0B"
"\x55\x5E\x2B\x14\x18\x1F\x16\x03\x5D\x56\x3B\x1B\x16\x01\x4B\x1B\x0A\x1C\x0A\x09\x03\x4E\x43\x33\x15\x07\x15\x10\x49\x7A\x0F\x39\x2A\x35\x3D\x3D\x2F\x7E\x73\x16\x3E\x38\x23\x27\x79\x6A\x0F\x2D\x3A\x25\x3B\x23\x3D\x48\x2A\x23\x2E\x28\x67\x37\x20\x0E\x0F\x11\x17\x19\x0C\x52\x77\x78\x46\x59\x51\x37\x27"
"\x24\x26\x60\x22\x06\x1A\x1A\x0E\x00\x01\x07\x07\x40\x00\x16\x17\x17\x49\x7A\x1F\x3D\x3F\x3F\x2A\x30\x29\x72\x32\x20\x21\x25\x7B\x74\x06\x3E\x2A\x3A\x3D\x3B\x3F\x60\x6D\x0D\x33\x34\x28\x29\x29\x25\x29\x5A\x1D\x1D\x18\x0A\x0A\x0E\x18\x01\x5F\x50\x30\x12\x01\x15\x1B\x09\x0E\x0C\x49\x0F\x1F\x1C\x67\x11"
"\x06\x14\x15\x0F\x09\x03\x16\x7A\x73\x39\x29\x2E\x7F\x39\x25\x37\x30\x25\x25\x3F\x38\x3A\x75\x2B\x27\x21\x28\x3D\x2A\x3F\x61\x62\x35\x29\x33\x32\x32\x25\x29\x5A\x16\x19\x1A\x16\x16\x12\x18\x52\x03\x1C\x10\x02\x11\x1B\x07\x07\x45\x46\x47\x47\x41\x66\x67\x54\x4A\x40\x20\x25\x24\x2B\x30\x14\x0F\x0B\x53"
"\x07\x30\x29\x2F\x72\x3A\x3E\x37\x39\x7B\x74\x06\x23\x2C\x26\x64\x27\x21\x6C\x22\x32\x37\x29\x2E\x28\x34\x64\x6D\x2D\x12\x16\x1D\x11\x08\x0F\x5D\x3A\x16\x1C\x1D\x19\x4D\x54\x13\x0B\x08\x0D\x45\x4E\x09\x05\x03\x05\x06\x12\x11\x14\x0E\x0A\x11\x76\x7B\x08\x10\x10\x76\x70\x57\x17\x3E\x31\x38\x3A\x77\x72"
"\x75\x2B\x28\x2B\x26\x3B\x21\x38\x3E\x6E\x63\x0F\x35\x2E\x22\x36\x65\x0F\x08\x1D\x0B\x0D\x53\x5C\x2A\x1B\x1D\x14\x1E\x01\x04\x54\x17\x0B\x08\x03\x1C\x1E\x43\x4C\x3E\x1B\x0D\x03\x41\x1F\x08\x11\x17\x7A\x28\x3D\x2D\x2A\x36\x32\x3A\x21\x7D\x5A\x5B\x61\x7E\x74\x01\x03\x06\x0D\x69\x68\x6F\x00\x0C\x0C\x04"
"\x15\x00\x01\x02\x4E\x01\x1B\x0F\x1D\x59\x58\x5F\x08\x14\x1F\x16\x5C\x51\x3A\x16\x1A\x12\x1F\x0A\x0F\x0C\x4E\x49\x4C\x1F\x07\x04\x09\x0E\x08\x4B\x44\x31\x23\x2B\x31\x37\x39\x73\x7C\x0E\x22\x36\x35\x32\x3E\x79\x5E\x5F\x72\x62\x68\x0E\x0F\x02\x05\x03\x05\x49\x07\x20\x2B\x22\x64\x07\x1B\x09\x54\x59\x39"
"\x1E\x11\x18\x52\x3E\x1F\x15\x13\x5B\x54\x36\x0B\x1B\x1C\x1C\x1C\x0A\x1F\x41\x42\x3B\x02\x0E\x1E\x47\x0A\x00\x2E\x2C\x37\x2B\x35\x36\x32\x3A\x7C\x59\x5A\x68\x7F\x77\x15\x16\x09\x0E\x1B\x1A\x07\x0D\x05\x01\x0B\x17\x19\x4B\x10\x2E\x37\x2C\x15\x15\x58\x51\x0A\x1A\x04\x09\x52\x00\x19\x0B\x13\x5B\x54\x18"
"\x0B\x0C\x06\x00\x08\x06\x09\x1F\x4E\x43\x03\x0E\x0A\x08\x16\x45\x3C\x32\x34\x2D\x3B\x2D\x2F\x71\x72\x30\x3F\x3F\x22\x25\x35\x26\x3E\x62\x64\x69\x06\x2A\x2D\x3F\x2B\x2D\x27\x61\x6E\x26\x31\x21\x13\x14\x54\x73\x1D\x1E\x0C\x09\x1B\x1C\x1E\x02\x5F\x5B\x54\x3C\x04\x1F\x0D\x1B\x0F\x0C\x18\x04\x0D\x0D\x40"
"\x49\x0D\x02\x1D\x07\x35\x3A\x2A\x3D\x72\x7F\x31\x32\x27\x20\x35\x7D\x76\x32\x2D\x30\x6A\x28\x27\x27\x3A\x3D\x23\x21\x6E\x63\x36\x2E\x2F\x24\x21\x65\x1B\x18\x1B\x1C\x0D\x0C\x55\x53\x78\x79\x41\x41\x5F\x57\x24\x27\x23\x3D\x29\x2A\x37\x4F\x4A\x4D\x31\x26\x23\x34\x34\x2E\x30\x3C\x50\x0C\x31\x37\x3A\x30"
"\x2B\x2E\x72\x00\x35\x32\x23\x25\x3D\x21\x33\x6B\x60\x28\x20\x3B\x25\x3B\x2B\x31\x35\x32\x6A\x67\x22\x2C\x08\x1E\x0F\x18\x12\x13\x52\x53\x5C\x5A\x5C\x51\x30\x1E\x1A\x11\x4A\x06\x11\x49\x0A\x0A\x1A\x04\x01\x06\x4C\x41\x07\x09\x00\x45\x37\x3A\x36\x20\x54\x2F\x39\x2F\x7F\x32\x20\x21\x76\x27\x31\x27\x27"
"\x22\x3B\x3A\x27\x20\x22\x6D\x31\x26\x34\x35\x2F\x29\x23\x36\x5A\x53\x14\x16\x1D\x1E\x08\x14\x1D\x1D\x5C\x51\x15\x16\x19\x10\x18\x0A\x44\x49\x03\x06\x0F\x1F\x0D\x13\x08\x0E\x08\x02\x48\x45\x3C\x32\x34\x3C\x2D\x71\x72\x73\x7B\x7D\x5A\x5B\x67\x66\x7D\x75\x1D\x02\x06\x0D\x01\x18\x1F\x6D\x17\x13\x04\x00"
"\x12\x02\x4E\x06\x12\x1E\x1B\x12\x5E\x19\x13\x0F\x52\x06\x00\x15\x17\x03\x11\x06\x46\x4B\x18\x08\x1B\x1C\x09\x41\x42\x02\x03\x15\x0F\x11\x01\x45\x32\x34\x2D\x2B\x2D\x73\x7C\x32\x22\x27\x39\x3E\x38\x36\x38\x75\x3F\x3B\x2C\x28\x3A\x2A\x3F\x61\x62\x36\x30\x25\x27\x33\x21\x65\x12\x12\x0B\x0D\x11\x0D\x05"
"\x51\x78\x12\x14\x07\x17\x19\x17\x10\x0E\x4B\x07\x19\x1A\x06\x03\x03\x11\x4F\x40\x36\x0F\x09\x00\x0A\x2D\x28\x78\x10\x30\x2C\x35\x39\x37\x21\x70\x01\x24\x38\x33\x27\x2B\x26\x66\x43\x44\x1E\x19\x04\x01\x08\x60\x00\x05\x04\x01\x16\x29\x5B\x2C\x30\x2E\x2C\x76\x50\x52\x27\x18\x18\x05\x57\x04\x07\x05\x0C"
"\x1A\x08\x03\x4F\x03\x1D\x07\x0D\x13\x41\x27\x29\x3D\x45\x29\x3E\x2C\x2D\x37\x31\x3B\x2E\x72\x23\x31\x36\x33\x77\x30\x3C\x38\x2E\x2B\x3D\x22\x36\x6C\x3A\x2B\x37\x28\x61\x29\x29\x21\x65\x19\x17\x11\x1A\x15\x5F\x54\x1E\x13\x07\x15\x16\x19\x05\x0D\x7F\x4A\x4B\x4F\x3E\x07\x01\x08\x02\x15\x10\x40\x32\x03"
"\x13\x10\x0C\x34\x3C\x2B\x7E\x7E\x72\x7C\x69\x64\x63\x70\x21\x37\x30\x31\x26\x63\x65\x42\x64\x6E\x1C\x29\x2C\x30\x20\x28\x61\x2F\x29\x37\x2C\x1E\x1E\x58\x2A\x1B\x0B\x08\x14\x1C\x14\x03\x4B\x56\x14\x18\x1C\x09\x00\x48\x1D\x06\x0A\x4C\x1E\x07\x02\x12\x02\x0E\x47\x06\x0A\x22\x7B\x39\x37\x3A\x7F\x28\x24"
"\x22\x36\x7C\x71\x33\x79\x33\x7B\x6A\x6C\x2A\x25\x3B\x2A\x38\x22\x2D\x37\x28\x66\x68\x4D\x69\x65\x28\x12\x1F\x11\x0A\x52\x1F\x11\x1B\x10\x1B\x51\x02\x1F\x11\x55\x39\x1F\x09\x1B\x1A\x4F\x0E\x18\x16\x17\x0F\x0F\x46\x4F\x33\x0C\x34\x70\x00\x70\x7E\x39\x33\x2F\x72\x27\x38\x34\x76\x27\x3B\x22\x2F\x39\x68"
"\x3C\x3D\x2A\x3E\x6D\x2F\x26\x2E\x34\x7C\x67\x00\x20\x0C\x12\x1B\x1C\x74\x5F\x5C\x30\x13\x1D\x11\x16\x13\x05\x58\x55\x2E\x02\x1B\x02\x4E\x22\x0D\x03\x03\x04\x05\x0C\x03\x09\x10\x49\x7A\x0F\x3D\x2B\x33\x36\x32\x3C\x3E\x7F\x70\x05\x37\x24\x3F\x75\x07\x2A\x26\x28\x29\x2A\x3E\x6D\x23\x2D\x24\x61\x2B\x28"
"\x36\x20\x54"
);

MAN(shortcuts, "Keyboard Shortcuts Master List",
"\x0D\x12\x16\x1D\x11\x08\x0F\x7D\x19\x16\x09\x71\x7E\x00\x3D\x3B\x63\x6B\x1B\x01\x01\x1D\x18\x0E\x17\x17\x13\x4B\x11\x2E\x2A\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x26\x1E\x0A\x02\x42\x01\x0F\x0F\x12\x03\x47\x37\x11\x3B\x29\x2C\x79\x33\x3A\x32\x28\x58\x04\x39"
"\x3F\x7D\x16\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x2B\x0E\x11\x1A\x15\x5F\x2F\x18\x06\x07\x19\x1F\x11\x04\x7E\x22\x03\x05\x43\x2B\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x1A\x32\x31\x26\x23\x71\x22\x3F\x31\x75\x39\x32\x3B\x3D\x2B\x22"
"\x6C\x39\x30\x22\x39\x4B\x11\x2E\x2A\x6E\x39\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x2A\x06\x0E\x18\x42\x21\x0C\x10\x08\x0A\x08\x10\x45\x72\x0C\x31\x37\x3A\x30\x2B\x2E\x72\x62\x61\x78\x5C\x00\x3D\x3B\x61\x0F\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A"
"\x5B\x58\x59\x2D\x17\x13\x0A\x5D\x1B\x19\x15\x13\x57\x00\x1D\x0F\x4B\x0C\x0C\x1D\x04\x18\x02\x12\x69\x37\x08\x08\x4C\x21\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x0D\x21\x25\x2B\x6F\x09\x35\x32\x2F\x2F\x33\x23\x35\x4E\x12\x13\x15\x53\x3F\x5E\x5F\x5C\x5D\x52\x53\x50\x51"
"\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x23\x1D\x07\x0D\x40\x27\x03\x02\x00\x07\x3B\x38\x33\x79\x16\x2A\x3E\x57\x05\x3A\x3E\x7A\x11\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x02\x1B\x16\x1D\x59\x3C\x1E\x0E\x77\x25\x1A\x1E\x5A\x3E\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C"
"\x4D\x42\x43\x40\x41\x46\x47\x44\x33\x35\x32\x3B\x3C\x7E\x2B\x25\x2D\x3B\x3D\x37\x5B\x01\x3E\x3A\x7E\x03\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x2A\x1B\x0B\x08\x14\x1C\x14\x03\x7B\x21\x1E\x1A\x5E\x21\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B"
"\x78\x1A\x3F\x2C\x28\x7D\x7D\x73\x13\x3E\x38\x39\x31\x36\x3E\x41\x1F\x20\x20\x64\x00\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x3F\x1F\x12\x1D\x57\x00\x1D\x0F\x4B\x38\x2A\x64\x38\x05\x03\x49\x2E\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x1B"
"\x3E\x3A\x3C\x27\x22\x32\x2C\x6E\x2E\x20\x21\x62\x34\x29\x2F\x22\x28\x33\x36\x70\x2C\x11\x17\x55\x2F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x32\x11\x0F\x0B\x03\x04\x10\x45\x75\x7B\x28\x2B\x3B\x2C\x39\x33\x26\x32\x24\x38\x39\x39\x74\x31\x23\x38\x38\x25\x2F\x36\x6C\x20"
"\x2D\x27\x25\x32\x4C\x10\x2D\x2B\x51\x29\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x3C\x1A\x02\x4D\x06\x0A\x01\x0D\x09\x00\x6E\x32\x33\x35\x73\x0A\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x1F\x28\x23\x31\x23\x29\x4C\x10\x2D\x2B\x51\x2F\x58"
"\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x2D\x16\x0F\x01\x07\x43\x14\x00\x15\x0C\x06\x04\x28\x7B\x39\x29\x2E\x2C\x56\x0A\x3B\x3D\x7B\x04\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x05\x26\x19\x1E\x0B\x0A\x17\x1D\x15\x11\x1B\x07\x09\x51\x05\x12"
"\x00\x01\x03\x05\x0F\x1A\x64\x38\x05\x03\x49\x35\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x15\x3B\x3D\x25\x28\x24\x29\x3B\x2A\x6F\x24\x24\x31\x37\x2F\x33\x3F\x4D\x13\x2C\x14\x50\x2F\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x38\x05\x09\x05"
"\x06\x14\x12\x46\x4F\x33\x0C\x34\x3F\x37\x2E\x2D\x7F\x6D\x6C\x7B\x59\x07\x38\x38\x7C\x0C\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x2B\x17\x0E\x1B\x0D\x5C\x08\x01\x16\x02\x51\x1B\x12\x1A\x00\x4A\x43\x3F\x00\x00\x4F\x07\x08\x1B\x43\x4B\x41\x3E\x4E\x6E\x32\x33\x35\x73\x00"
"\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x1F\x3A\x2B\x37\x23\x29\x66\x25\x21\x31\x0D\x1E\x1D\x17\x5E\x1B\x19\x0E\x19\x07\x1F\x01\x56\x16\x1A\x11\x4A\x26\x01\x11\x0B\x0B\x4C\x3F\x07\x02\x0C\x08\x12\x1E\x6E\x32\x33\x35\x73\x03\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74"
"\x75\x6A\x6B\x68\x69\x6E\x6F\x1F\x23\x23\x33\x60\x2D\x27\x3E\x2B\x30\x0E\x08\x58\x51\x29\x16\x12\x19\x1D\x04\x03\x51\x47\x46\x5D\x7F\x3D\x02\x06\x42\x42\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x0D\x37\x36\x3B\x71\x37\x23\x74\x21\x22\x2E\x68\x2D\x2B\x3C\x27\x39\x2D\x33"
"\x4A\x16\x2F\x29\x6F\x6B\x5A\x14\x0A\x59\x29\x16\x12\x56\x49\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x2D\x04\x01\x05\x05\x4D\x12\x02\x0E\x04\x0A\x6D\x33\x0C\x34\x70\x08\x38\x2B\x2C\x39\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x1C\x35\x3E\x36\x26\x2D\x61\x36\x35\x2B\x35\x1F\x09\x0C\x10\x1B"
"\x0C\x76\x2A\x1B\x1D\x5B\x22\x1E\x1E\x12\x01\x41\x38\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x37\x06\x28\x3E\x3D\x37\x2D\x37\x33\x29\x72\x7B\x03\x3F\x3F\x27\x74\x73\x6A\x18\x23\x2C\x3A\x2C\x24\x64\x48\x14\x29\x2F\x6D\x14\x2C\x2C\x1C\x0F\x53\x35\x1B\x19\x08\x52\x20\x1A\x17\x19\x02\x57\x54\x55"
"\x27\x04\x1E\x0C\x4E\x18\x05\x03\x06\x0C\x17\x41\x12\x08\x44\x04\x34\x34\x2C\x31\x3B\x2D\x7C\x30\x3D\x3D\x39\x25\x39\x25\x5E\x02\x23\x25\x63\x05\x2B\x29\x38\x62\x10\x2A\x27\x29\x32\x68\x11\x35\x55\x3F\x17\x0E\x10\x5F\x2F\x13\x13\x03\x50\x06\x1F\x19\x10\x1A\x1D\x61\x3F\x00\x00\x44\x2D\x01\x16\x48\x24"
"\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x10\x31\x3D\x33\x39\x30\x34\x38\x6B\x2E\x25\x37\x20\x39\x39\x48\x14\x29\x2F\x6D\x04\x30\x37\x16\x50\x3C\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x24\x0E\x1F\x49\x18\x06\x1E\x19\x17\x02\x0C\x41\x02\x02\x17\x0E\x2E\x34\x28\x53\x09\x36"
"\x32\x76\x11\x27\x22\x3D\x7D\x1B\x31\x33\x3E\x64\x1A\x20\x29\x27\x38\x6D\x62\x63\x60\x12\x31\x2E\x30\x26\x12\x5B\x0E\x10\x0C\x0B\x09\x1C\x1E\x53\x14\x14\x05\x1C\x00\x1A\x1A\x61\x3F\x00\x00\x44\x2F\x19\x10\x0F\x4B\x27\x52\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x10\x3C\x3E\x25\x32\x74\x23\x23"
"\x39\x3C\x3C\x2F\x23\x6C\x29\x27\x30\x2B\x35\x29\x37\x4E\x12\x13\x15\x53\x37\x0B\x12\x1E\x18\x00\x53\x58\x40\x5B\x4E\x5D\x55\x4A\x4B\x48\x49\x4E\x4F\x23\x1D\x07\x0D\x40\x15\x0E\x02\x44\x2B\x2E\x33\x78\x2D\x3F\x2C\x37\x3F\x33\x21\x70\x30\x26\x27\x5E\x02\x23\x25\x63\x19\x22\x3A\x3F\x62\x0F\x2A\x2E\x34"
"\x35\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x31\x1C\x15\x1D\x19\x17\x1F\x12\x06\x55\x10\x04\x07\x04\x64\x38\x05\x03\x49\x26\x13\x02\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x15\x3B\x3B\x26\x2F\x6B\x05\x28\x29\x21\x25\x2B\x2B\x26\x32\x4B\x4C\x00\x01\x0B\x3F\x29\x39\x35\x5E\x2C\x34"
"\x32\x20\x27\x33\x24\x22\x24\x7E\x36\x1E\x19\x04\x42\x2D\x4F\x43\x4D\x3A\x43\x4F\x41\x30\x47\x4B\x45\x00\x7B\x78\x79\x7E\x7F\x1F\x32\x22\x2A\x70\x7E\x76\x14\x21\x21\x6A\x64\x68\x19\x2F\x3C\x38\x28\x62\x6C\x60\x14\x28\x23\x2B\x4F\x39\x0F\x0A\x15\x55\x3E\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B"
"\x48\x49\x4E\x4F\x4C\x3E\x07\x0F\x05\x02\x12\x47\x05\x09\x36\x51\x1B\x2D\x2C\x33\x77\x1B\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x05\x29\x2F\x22\x4D\x07\x31\x08\x17\x53\x2A\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x3C\x0D\x1B\x07\x69\x23\x15\x14"
"\x0B\x4F\x2A\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x04\x38\x2C\x20\x45\x0F\x39\x30\x2F\x6B\x0F\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x39\x11\x02\x4A\x1C\x01\x07\x0A\x00\x1B\x67\x21\x17\x12\x0D\x4D\x30\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D"
"\x72\x73\x70\x71\x76\x77\x74\x16\x26\x24\x3B\x2C\x6E\x3B\x2D\x2F\x6D\x34\x29\x2F\x22\x28\x33\x4F\x39\x0F\x0A\x15\x55\x2C\x14\x14\x14\x07\x5B\x34\x05\x14\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x39\x03\x10\x0B\x41\x2B\x06\x0A\x04\x3D\x3E\x2A\x53\x1F\x33\x28\x76\x06\x32\x32\x71\x76\x77\x74\x75\x6A\x6B\x68"
"\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x12\x31\x2E\x30\x26\x12\x5B\x19\x09\x0E\x0C\x76\x3C\x1E\x07\x5B\x37\x42\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x27\x09\x35\x28\x3D\x79\x29\x36\x32\x39\x3D\x24\x70\x7E\x76\x24\x3C\x20\x3E\x2F\x27\x3E\x20\x6F\x28\x24\x23\x2F\x2F\x26\x4C\x06"
"\x28\x31\x51\x3E\x16\x0D\x1B\x0D\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x3E\x1D\x03\x1D\x07\x11\x14\x08\x03\x14\x44\x0A\x3C\x7B\x2C\x31\x3B\x7F\x2F\x38\x3E\x36\x33\x25\x33\x33\x74\x3C\x3E\x2E\x25\x43\x0F\x23\x38\x66\x0E\x26\x26\x35\x69\x15\x2D\x22\x12\x0F\x58\x59\x5E\x5F\x5C\x5D\x52"
"\x53\x50\x33\x17\x14\x1F\x55\x45\x4B\x2E\x06\x1C\x18\x0D\x1F\x06\x43\x48\x24\x1E\x17\x08\x0A\x28\x3E\x2A\x75\x7E\x3D\x2E\x32\x25\x20\x35\x23\x25\x7E\x5E\x14\x26\x3F\x63\x1C\x3E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x29\x0D\x52\x1C\x1E\x14\x56\x11\x1B\x19\x0E\x0E\x1A\x49"
"\x46\x2A\x14\x1D\x0E\x0C\x12\x04\x14\x4E\x6E\x36\x32\x32\x3E\x2D\x75\x1B\x39\x31\x37\x27\x35\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x08\x28\x2E\x26\x34\x24\x66\x30\x2D\x31\x12\x14\x0D\x0D\x5E\x2D\x19\x1E\x0B\x10\x1C\x14\x56\x35\x1D\x1B\x60\x28\x1C\x1B\x02\x44\x3F\x05\x0B\x05\x14\x4A\x23\x09\x10"
"\x00\x28\x7B\x78\x79\x7E\x7F\x7C\x7D\x00\x26\x3E\x71\x37\x24\x74\x34\x2E\x26\x21\x27\x27\x3C\x38\x3F\x23\x37\x2F\x33\x66\x6F\x22\x37\x15\x16\x58\x0A\x1B\x1E\x0E\x1E\x1A\x5C\x02\x04\x18\x5E\x7E\x25\x18\x02\x06\x1D\x4E\x3C\x0F\x1F\x07\x06\x0E\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x1A\x28\x3E\x3F"
"\x70\x22\x35\x25\x31\x30\x24\x38\x20\x26\x3A\x6F\x38\x22\x62\x13\x29\x22\x32\x32\x36\x20\x09\x27\x2B\x1A\x0C\x1A\x19\x13\x01\x1B\x1F\x05\x05\x7D\x32\x47\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x0D\x39\x33\x33\x3E\x35\x5B\x10\x62\x74\x75\x6A\x6B\x68\x69\x6E"
"\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x2B\x1B\x19\x0E\x18\x01\x1B\x7A\x37\x47\x46\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x1E\x2C\x32\x33\x2F\x3E\x20\x36\x35\x3F\x5C\x5D\x00\x10\x12\x1F\x68\x0C\x0A\x06\x18\x04\x0C\x04\x4A\x02\x32\x35\x28\x6E"
"\x36\x1E\x1E\x0D\x51\x2D\x15\x1A\x1A\x07\x50\x51\x56\x57\x54\x55\x4A\x4B\x22\x1C\x03\x1F\x4C\x1A\x0D\x11\x04\x41\x04\x1E\x44\x12\x35\x29\x3C\x53\x1D\x2B\x2E\x31\x79\x1B\x3F\x3C\x33\x78\x11\x3B\x2E\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x0B\x33\x2A\x34\x65\x0E\x14\x58\x0A\x0A\x1E\x0E\x09\x5D\x16\x1E"
"\x15\x56\x18\x12\x55\x0E\x04\x0B\x1C\x03\x0A\x02\x19\x68\x30\x08\x08\x00\x13\x4F\x2D\x35\x36\x3D\x76\x1B\x31\x38\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x19\x2E\x24\x2C\x2D\x3B\x6C\x39\x2D\x63\x2C\x28\x28\x22\x64\x36\x0E\x1A\x0A\x0D\x51\x1A\x12\x19\x78\x30\x04\x03\x1A\x5C\x27\x1D\x03\x0D\x1C\x42\x22\x0A"
"\x0A\x19\x4D\x31\x09\x06\x0E\x13\x44\x45\x09\x3E\x34\x3C\x3D\x2B\x7C\x2A\x3D\x21\x34\x71\x34\x2E\x74\x22\x25\x39\x2C\x43\x06\x20\x21\x28\x62\x6C\x60\x04\x28\x23\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x3D\x1F\x19\x11\x55\x19\x1F\x09\x1B\x1A\x4F\x43\x4D\x07\x0D\x04\x6B\x36\x06\x03\x00\x7A"
"\x0E\x28\x79\x71\x7F\x0C\x3C\x35\x36\x70\x15\x39\x20\x3A\x75\x6A\x6B\x68\x1A\x2D\x3D\x23\x21\x2E\x49\x4A\x05\x0F\x06\x08\x0A\x3D\x5B\x3A\x36\x26\x5F\x37\x38\x2B\x20\x7A\x25\x17\x15\x54\x5A\x4A\x38\x00\x00\x08\x1B\x47\x39\x03\x01\x40\x41\x46\x47\x44\x45\x7A\x7B\x15\x36\x28\x3A\x7C\x3F\x37\x27\x27\x34"
"\x33\x39\x74\x36\x25\x25\x3C\x3B\x21\x23\x3F\x47\x07\x2D\x34\x24\x34\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x3A\x21\x4B\x47\x49\x0D\x00\x02\x0B\x0B\x11\x0D\x6B\x23\x14\x07\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x0A\x2F\x21\x2F"
"\x28\x2E\x49\x13\x31\x27\x24\x21\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x3F\x07\x0E\x09\x03\x09\x4D\x01\x0B\x05\x02\x0D\x05\x0B\x1D\x3F\x28\x52\x53\x18\x16\x10\x18\x72\x16\x08\x01\x1A\x18\x06\x10\x18\x6B\x03\x0C\x17\x1C\x46\x0E\x36\x31\x2C\x6A\x15\x2F\x2D\x23\x0E\x50"
"\x36\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x3A\x10\x1D\x4B\x0E\x06\x02\x0B\x09\x1F\x68\x20\x14\x13\x0A\x4C\x28\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x0C\x24\x2B\x3C\x3D\x6F\x38\x25\x27\x63\x21\x25\x22\x35\x21\x36\x09\x5B\x1A\x18\x0C\x75\x3F\x09\x00\x1F\x5B\x34\x56"
"\x58\x54\x36\x1E\x19\x04\x42\x28\x4F\x4C\x4D\x42\x43\x40\x41\x46\x34\x01\x04\x28\x38\x30\x79\x3C\x30\x24\x57\x13\x3F\x24\x7A\x06\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x15\x08\x1E\x0E\x10\x1B\x08\x5C\x0D\x13\x1D\x15\x7B\x37\x1B\x00\x5E\x39\x03\x01\x0F\x1A\x44\x3C\x4D"
"\x42\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x1D\x3B\x2B\x3D\x34\x3E\x20\x70\x21\x37\x39\x31\x6A\x6A\x25\x27\x69\x63\x6F\x0D\x21\x36\x68\x13\x29\x2F\x21\x30\x6E\x2A\x5B\x17\x09\x1B\x11\x0F\x5D\x16\x16\x04\x10\x1F\x1B\x07\x4A\x4A\x43\x1D\x1A\x0B\x4F\x2D\x01\x16\x48\x33\x09\x0F\x01\x10\x4E\x0E\x3A\x3A"
"\x70\x54\x19\x68\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x02\x35\x2B\x35\x57\x1F\x17\x0E\x10\x5F\x08\x15\x17\x53\x11\x15\x12\x05\x11\x06\x19\x4B\x0A\x08\x1C\x65\x2E\x0C\x01\x08\x13\x11\x07\x04\x01\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x10"
"\x3B\x75\x3F\x3B\x68\x26\x20\x2A\x6C\x2B\x2D\x2F\x24\x24\x34"
);

MAN(cmd, "Command Prompt (CMD) Reference",
"\x15\x0B\x1D\x17\x17\x11\x1B\x7D\x11\x1C\x1D\x1C\x17\x19\x10\x75\x1A\x19\x07\x04\x1E\x1B\x46\x60\x62\x14\x29\x2F\x6D\x15\x68\x65\x0E\x02\x08\x1C\x5E\x1C\x11\x19\x5E\x53\x35\x1F\x02\x12\x06\x5B\x60\x46\x48\x2F\x01\x1D\x4C\x0C\x06\x0E\x09\x0F\x46\x15\x0D\x02\x32\x2F\x2B\x63\x7E\x08\x35\x33\x79\x0B\x70"
"\x6F\x76\x03\x31\x27\x27\x22\x26\x28\x22\x6F\x64\x0C\x26\x2E\x29\x2F\x6F\x6B\x64\x2A\x08\x5B\x3B\x0D\x0C\x13\x57\x2E\x1A\x1A\x16\x05\x5D\x32\x1A\x01\x0F\x19\x48\x06\x00\x4F\x18\x05\x07\x69\x40\x41\x05\x0A\x00\x45\x29\x3E\x39\x2B\x3D\x37\x7C\x2F\x37\x20\x25\x3D\x22\x79\x74\x01\x22\x22\x3B\x69\x3E\x3D"
"\x23\x2A\x30\x22\x2D\x7B\x66\x60\x27\x28\x1E\x5C\x58\x18\x10\x1B\x5C\x5A\x11\x1E\x14\x5C\x17\x13\x19\x1C\x04\x4C\x48\x0A\x01\x02\x01\x0C\x0C\x07\x13\x4F\x6C\x6D\x2A\x24\x0C\x12\x1F\x18\x0A\x16\x13\x13\x58\x30\x34\x71\x6A\x31\x3B\x39\x2E\x2E\x3A\x77\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x05\x2F\x25\x2B\x1D"
"\x1E\x58\x1D\x17\x0D\x19\x1E\x06\x1C\x02\x08\x56\x5F\x17\x11\x4A\x45\x46\x49\x53\x4F\x19\x1D\x4E\x43\x03\x05\x46\x48\x00\x45\x1E\x61\x04\x79\x63\x7F\x3F\x35\x33\x3D\x37\x34\x76\x33\x26\x3C\x3C\x2E\x61\x43\x2A\x26\x3E\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x31\x1B\x00\x04\x51"
"\x10\x1E\x18\x10\x19\x4B\x09\x07\x0A\x4F\x0A\x02\x0E\x07\x05\x13\x15\x47\x4C\x01\x33\x29\x78\x76\x3F\x7F\x2F\x35\x3D\x24\x23\x71\x3E\x3E\x30\x31\x2F\x25\x61\x43\x2D\x23\x3F\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x3E\x1E\x16\x11\x03\x56\x03\x1C\x10\x4A\x18\x0B\x1B\x0B\x0A\x02"
"\x67\x07\x00\x08\x0E\x46\x5B\x10\x00\x22\x2F\x66\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x01\x24\x3E\x3A\x21\x6A\x3F\x2D\x31\x3A\x6F\x64\x28\x21\x2B\x2F\x61\x29\x21\x22\x65\x12\x12\x1C\x1C\x0D\x5F\x1F\x12\x1F\x1E\x11\x1F\x12\x04\x54\x1C\x04\x4B\x0A\x08\x1A\x0C\x04\x4D\x04\x0A\x0C\x04\x15\x4E\x6E\x0D\x3F\x37"
"\x28\x79\x62\x3C\x33\x30\x3F\x32\x3E\x35\x68\x77\x74\x75\x6A\x6B\x1B\x21\x21\x38\x6C\x25\x27\x2F\x30\x61\x20\x28\x36\x65\x1B\x5B\x1B\x16\x13\x12\x1D\x13\x16\x79\x07\x19\x13\x05\x11\x55\x56\x1B\x1A\x06\x09\x1D\x0D\x00\x5C\x43\x40\x41\x46\x21\x0D\x0B\x3E\x7B\x2F\x31\x3B\x2D\x39\x7D\x33\x73\x20\x23\x39"
"\x30\x26\x34\x27\x6B\x21\x3A\x6E\x26\x22\x3E\x36\x22\x2C\x2D\x23\x23\x4E\x31\x08\x1E\x1D\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x3B\x01\x01\x18\x4C\x0C\x42\x05\x0F\x0D\x02\x02\x16\x45\x2E\x29\x3D\x3C\x54\x2C\x28\x3C\x20\x27\x70\x6D\x30\x3E\x38\x30\x74\x6B\x68\x69\x6E\x6F\x6C\x6D"
"\x0D\x33\x25\x2F\x66\x26\x64\x23\x13\x17\x1D\x56\x18\x10\x10\x19\x17\x01\x5F\x24\x24\x3B\x54\x1C\x04\x4B\x01\x1D\x1D\x4F\x08\x08\x04\x02\x15\x0D\x12\x47\x05\x15\x2A\x51\x52\x1F\x17\x13\x19\x0E\x72\x75\x70\x17\x19\x1B\x10\x10\x18\x18\x42\x24\x2A\x6F\x70\x23\x23\x2E\x25\x7F\x66\x67\x64\x65\x5A\x5B\x58"
"\x59\x5E\x5F\x31\x1C\x19\x16\x50\x15\x1F\x05\x11\x16\x1E\x04\x1A\x10\x64\x1D\x08\x4D\x5E\x0D\x01\x0C\x03\x59\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x00\x36\x3D\x3E\x20\x32\x74\x31\x23\x39\x2D\x2A\x3A\x20\x3E\x34\x62\x6B\x32\x25\x66\x68\x37\x65\x55\x0A\x58\x44\x5E\x19\x13\x0F\x11\x16\x59\x7B\x15\x18"
"\x04\x0C\x4A\x57\x09\x57\x4E\x53\x0E\x53\x42\x43\x40\x41\x46\x47\x44\x26\x35\x2B\x21\x79\x38\x36\x30\x38\x21\x59\x3D\x3E\x20\x32\x74\x69\x2B\x75\x68\x75\x2C\x71\x6C\x6D\x62\x63\x60\x61\x66\x0A\x2B\x33\x1F\x5B\x17\x0B\x5E\x0D\x19\x13\x13\x1E\x15\x7B\x12\x12\x18\x55\x56\x0D\x01\x05\x0B\x51\x4C\x4D\x42"
"\x43\x40\x41\x46\x47\x44\x21\x3F\x37\x3D\x2D\x3B\x7F\x74\x39\x37\x3F\x70\x7E\x30\x77\x7B\x24\x6A\x64\x29\x69\x28\x20\x3E\x2E\x27\x30\x60\x33\x23\x26\x20\x68\x15\x15\x14\x00\x55\x17\x15\x19\x16\x16\x1E\x58\x7C\x05\x11\x1B\x4A\x57\x09\x57\x4E\x53\x0E\x53\x42\x43\x40\x41\x46\x47\x44\x45\x08\x3E\x36\x38"
"\x33\x3A\x56\x29\x2B\x23\x35\x71\x6A\x31\x3D\x39\x2F\x75\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x10\x33\x2F\x29\x30\x65\x1B\x5B\x0C\x1C\x06\x0B\x5C\x1B\x1B\x1F\x15\x7B\x1B\x18\x06\x10\x4A\x57\x0E\x00\x02\x0A\x52\x4D\x42\x43\x40\x41\x46\x47\x44\x35\x28\x32\x36\x2D\x7E\x3E\x7C\x29\x37\x2B\x24\x71\x30\x3E\x38"
"\x30\x6A\x24\x26\x2C\x6E\x3C\x2F\x3F\x27\x26\x2E\x61\x27\x33\x64\x24\x5A\x0F\x11\x14\x1B\x75\x1D\x09\x06\x01\x19\x13\x56\x4B\x12\x1C\x06\x0E\x56\x49\x4E\x4F\x4C\x4D\x42\x30\x08\x0E\x11\x48\x07\x0D\x3B\x35\x3F\x3C\x7E\x39\x35\x31\x37\x73\x31\x25\x22\x25\x3D\x37\x3F\x3F\x2D\x3A\x6E\x67\x67\x3F\x6E\x63"
"\x6D\x29\x6A\x67\x6F\x36\x54\x55\x56\x50\x74\x07\x1F\x12\x02\x0A\x50\x5E\x56\x05\x1B\x17\x05\x08\x07\x19\x17\x4F\x4C\x4D\x23\x07\x16\x00\x08\x04\x01\x01\x7A\x38\x37\x29\x27\x36\x32\x3A\x72\x7B\x22\x3E\x34\x38\x37\x3A\x3A\x32\x68\x20\x3D\x6F\x3C\x3F\x27\x25\x25\x33\x34\x22\x20\x6C\x70\x1D\x1B\x59\x42"
"\x1E\x42\x5D\x4E\x11\x4E\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x2D\x00\x01\x1D\x03\x11\x05\x41\x12\x10\x0B\x45\x3C\x32\x34\x3C\x2D\x55\x3A\x34\x3C\x37\x23\x25\x24\x77\x68\x21\x2F\x33\x3C\x77\x6E\x73\x2A\x73\x62\x10\x25\x20\x34\x24\x2C\x65\x13\x15\x0B\x10\x1A\x1A\x5C\x1B\x1B\x1F\x15\x02\x7C\x14\x1D\x05"
"\x02\x0E\x1A\x49\x41\x0B\x4C\x4D\x42\x43\x40\x41\x46\x47\x44\x45\x1E\x3E\x3B\x2B\x27\x2F\x28\x7D\x34\x3A\x3C\x34\x25\x77\x7C\x10\x0C\x18\x61\x43\x2D\x20\x21\x3D\x62\x7F\x21\x7F\x66\x7B\x26\x7B\x5A\x5B\x58\x59\x5E\x5F\x5C\x3E\x1D\x1E\x00\x10\x04\x12\x54\x01\x1D\x04\x48\x0F\x07\x03\x09\x1E\x42\x01\x19"
"\x15\x03\x47\x06\x1C\x7A\x39\x21\x2D\x3B\x55\x3F\x32\x3F\x23\x31\x32\x22\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x10\x28\x2E\x31\x68\x27\x2D\x1B\x15\x1F\x1C\x5E\x31\x28\x3B\x21\x53\x13\x1E\x1B\x07\x06\x10\x19\x18\x01\x06\x00\x65\x66\x29\x2B\x30\x2B\x32\x6C\x04\x0C\x0E\x3E\x28\x33\x79\x62\x3B"
"\x2E\x34\x24\x36\x6E\x71\x76\x77\x74\x75\x09\x23\x2D\x2A\x25\x6F\x38\x25\x27\x63\x24\x28\x35\x2C\x64\x23\x15\x09\x58\x1C\x0C\x0D\x13\x0F\x01\x53\x58\x12\x1E\x1C\x10\x06\x01\x4B\x2B\x53\x4E\x40\x0A\x4D\x4D\x11\x40\x13\x03\x17\x05\x0C\x28\x28\x71\x53\x3A\x36\x2F\x36\x22\x32\x22\x25\x76\x77\x74\x75\x6A"
"\x6B\x68\x69\x6E\x6F\x6C\x1D\x23\x31\x34\x28\x32\x2E\x2B\x2B\x5A\x16\x19\x17\x1F\x18\x19\x0F\x52\x5B\x14\x18\x05\x1C\x04\x14\x18\x1F\x48\x57\x4E\x03\x05\x1E\x16\x43\x04\x08\x15\x0C\x44\x5B\x7A\x28\x3D\x35\x3B\x3C\x28\x7D\x6C\x73\x33\x3D\x33\x36\x3A\x7C\x40\x2F\x2D\x2F\x3C\x2E\x2B\x6D\x7E\x27\x32\x28"
"\x30\x22\x7A\x65\x5A\x5B\x58\x59\x3A\x1A\x1A\x0F\x13\x14\x1D\x14\x18\x03\x54\x5A\x4A\x3F\x3A\x20\x23\x4F\x44\x09\x07\x05\x12\x00\x01\x47\x27\x5F\x7A\x74\x17\x70\x54\x39\x33\x2F\x3F\x32\x24\x71\x6A\x33\x26\x3C\x3C\x2E\x76\x69\x6E\x6F\x6C\x6D\x04\x2C\x32\x2C\x27\x33\x64\x24\x5A\x0D\x17\x15\x0B\x12\x19"
"\x5D\x5A\x15\x1F\x03\x1B\x16\x00\x55\x2F\x51\x48\x46\x28\x3C\x56\x23\x36\x25\x33\x48\x6C\x10\x09\x0C\x39\x7B\x34\x36\x39\x36\x3F\x3C\x3E\x37\x39\x22\x3D\x77\x74\x75\x06\x22\x3B\x3D\x6E\x2B\x25\x3E\x29\x63\x29\x2F\x20\x28\x64\x6D\x0D\x16\x11\x1A\x5E\x13\x13\x1A\x1B\x10\x11\x1D\x12\x1E\x07\x1E\x4A\x0C"
"\x0D\x1D\x4E\x01\x0D\x00\x07\x4F\x13\x08\x1C\x02\x48\x03\x28\x3E\x3D\x2A\x2E\x3E\x3F\x38\x7B\x59\x5A\x1F\x13\x03\x03\x1A\x18\x00\x42\x20\x3E\x2C\x23\x23\x24\x2A\x27\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x2F\x15\x1D\x04\x50\x38\x26\x57\x17\x1A\x04\x0D\x01\x0E\x1B\x1D\x0D\x19\x0B\x0C\x0E\x41\x4E"
"\x0E\x14\x06\x35\x35\x3E\x30\x39\x7F\x73\x3C\x3E\x3F\x7C\x71\x79\x31\x38\x20\x39\x23\x2C\x27\x3D\x63\x6C\x62\x30\x26\x2C\x24\x27\x34\x21\x69\x70\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x41\x1D\x09\x03\x07\x14\x4C\x41\x49\x15\x01\x02\x33\x28\x2C\x3C\x2C\x3B\x32\x2E"
"\x7B\x59\x20\x38\x38\x30\x74\x69\x22\x24\x3B\x3D\x70\x6F\x6C\x6D\x62\x63\x60\x61\x66\x13\x21\x36\x0E\x5B\x1B\x16\x10\x11\x19\x1E\x06\x1A\x06\x18\x02\x0E\x54\x5D\x1A\x02\x06\x0E\x4E\x42\x18\x4D\x12\x0A\x0E\x06\x15\x47\x02\x0A\x28\x3E\x2E\x3C\x2C\x76\x56\x29\x20\x32\x33\x34\x24\x23\x74\x69\x22\x24\x3B"
"\x3D\x70\x6F\x6C\x6D\x62\x63\x14\x33\x27\x24\x21\x65\x0E\x13\x1D\x59\x0C\x10\x09\x09\x17\x53\x04\x1E\x56\x16\x54\x1D\x05\x18\x1C\x63\x1E\x0E\x18\x05\x12\x0A\x0E\x06\x46\x5B\x0C\x0A\x29\x2F\x66\x79\x7E\x7F\x7C\x0E\x3E\x3C\x27\x71\x34\x22\x20\x75\x2E\x2E\x3C\x28\x27\x23\x29\x29\x62\x31\x2F\x34\x32\x22"
"\x64\x24\x14\x1A\x14\x00\x0D\x16\x0F\x77\x1C\x00\x1C\x1E\x19\x1C\x01\x05\x4A\x57\x06\x08\x03\x0A\x52\x4D\x42\x43\x40\x25\x28\x34\x44\x09\x35\x34\x33\x2C\x2E\x55\x32\x38\x26\x20\x24\x30\x22\x77\x79\x34\x24\x24\x68\x69\x6E\x6F\x6C\x6D\x62\x10\x28\x2E\x31\x67\x34\x2A\x08\x0F\x0B\x59\x1F\x11\x18\x5D\x11"
"\x1C\x1E\x1F\x13\x14\x00\x1C\x05\x05\x1B\x49\x46\x42\x03\x4D\x11\x0B\x0F\x16\x15\x47\x0B\x12\x34\x32\x36\x3E\x7E\x0F\x15\x19\x7B\x59\x3E\x34\x22\x77\x21\x26\x2F\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x0A\x25\x35\x55\x0E\x16\x14\x1F\x0F\x5C\x13\x17\x07\x07\x1E\x04\x1C\x54\x11\x18\x02\x1E\x0C"
"\x1D\x65\x02\x08\x16\x43\x13\x09\x07\x15\x01\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x72\x1F\x39\x22\x22\x77\x27\x3D\x2B\x39\x2D\x2D\x6E\x29\x23\x21\x26\x26\x32\x32\x4C\x29\x21\x31\x5A\x0E\x0B\x1C\x0C\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x27\x0A\x06\x08\x09\x0A\x4C\x18\x11\x06\x12\x12\x46\x4F\x0A"
"\x00\x2E\x7B\x2D\x2A\x3B\x2D\x7C\x61\x3C\x32\x3D\x34\x68\x77\x68\x25\x2B\x38\x3B\x77\x6E\x60\x2D\x29\x26\x6A\x4A\x2F\x23\x33\x37\x2D\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x3A\x10\x1E\x1C\x07\x1B\x05\x4F\x1F\x05\x07\x0F\x0C\x5B\x46\x09\x01\x11\x29\x33\x78\x2E\x32\x3E\x32\x7D\x21\x3B"
"\x3F\x26\x76\x27\x26\x3A\x2C\x22\x24\x2C\x3D\x63\x6C\x23\x27\x37\x33\x29\x66\x21\x2D\x37\x1F\x0C\x19\x15\x12\x51\x52\x53\x78\x79\x23\x28\x25\x23\x31\x38\x60\x18\x11\x1A\x1A\x0A\x01\x04\x0C\x05\x0F\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x18\x2A\x30\x31\x72\x20\x29\x22\x22\x32\x39\x75\x39\x3E\x25\x24\x2F"
"\x3D\x35\x47\x36\x22\x33\x2A\x2A\x2E\x37\x31\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x3D\x1F\x04\x00\x55\x1A\x19\x07\x0A\x0B\x1C\x1F\x08\x11\x43\x48\x15\x07\x14\x0F\x09\x33\x28\x2C\x79\x71\x2C\x2A\x3E\x7B\x59\x24\x30\x25\x3C\x3F\x3C\x26\x27\x68\x66\x1E\x06\x08\x6D\x2C\x63\x60\x61\x66\x0C\x2D\x29"
"\x16\x5B\x19\x59\x0E\x0D\x13\x1E\x17\x00\x03\x51\x5E\x03\x15\x06\x01\x00\x01\x05\x02\x4F\x43\x0B\x42\x4C\x09\x0C\x46\x09\x05\x08\x3F\x75\x3D\x21\x3B\x76\x56\x2E\x3A\x26\x24\x35\x39\x20\x3A\x75\x65\x38\x68\x66\x3A\x6F\x7A\x7D\x62\x63\x13\x29\x33\x33\x20\x2A\x0D\x15\x58\x10\x10\x5F\x4A\x4D\x01\x53\x58"
"\x02\x1E\x02\x00\x11\x05\x1C\x06\x49\x41\x0E\x4C\x0C\x00\x0C\x12\x15\x15\x4B\x44\x4A\x28\x7B\x2A\x3C\x2D\x2B\x3D\x2F\x26\x20\x79\x5B\x25\x31\x37\x75\x65\x38\x2B\x28\x20\x21\x23\x3A\x62\x63\x60\x61\x66\x67\x64\x17\x1F\x0B\x19\x10\x0C\x5F\x0F\x04\x01\x07\x15\x1C\x56\x11\x1D\x19\x0F\x18\x62\x0D\x07\x1C"
"\x01\x4D\x4D\x0C\x0E\x0D\x0F\x09\x01\x45\x75\x38\x34\x3C\x3F\x31\x29\x2D\x7F\x3A\x3D\x30\x31\x32\x74\x7A\x38\x2E\x3B\x3D\x21\x3D\x29\x25\x27\x22\x2C\x35\x2E\x67\x64\x65\x28\x1E\x08\x18\x17\x0D\x5C\x09\x1A\x16\x50\x02\x0F\x04\x00\x10\x07\x4B\x01\x04\x0F\x08\x09\x67\x12\x0C\x17\x04\x14\x04\x02\x02\x7A"
"\x74\x39\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x02\x3E\x38\x23\x75\x39\x27\x2D\x2C\x3E\x6F\x3F\x39\x23\x37\x25\x32\x4C\x37\x2B\x32\x1F\x09\x1B\x1F\x19\x5F\x53\x1F\x13\x07\x04\x14\x04\x0E\x06\x10\x1A\x04\x1A\x1D\x4E\x4F\x4C\x4D\x42\x21\x01\x15\x12\x02\x16\x1C\x7A\x29\x3D\x29\x31\x2D\x28\x57\x36\x21\x39\x27"
"\x33\x25\x25\x20\x2F\x39\x31\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x0D\x2F\x34\x30\x65\x1E\x09\x11\x0F\x1B\x0D\x0F\x77\x1F\x00\x19\x1F\x10\x18\x47\x47\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x32\x1F\x14\x10\x00\x37\x7B\x11\x37\x38\x30\x2E\x30\x33\x27\x39\x3E\x38\x77\x20\x3A\x25\x27\x42\x3B\x2B\x28\x6C"
"\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x2E\x18\x15\x1A\x03\x05\x04\x0E\x54\x16\x05\x05\x1B\x06\x02\x0A\x4C\x45\x10\x06\x07\x41\x17\x12\x01\x17\x23\x77\x78\x2B\x3B\x38\x7C\x3C\x36\x37\x7C\x71\x24\x32\x33\x75\x2E\x2E\x24\x2C\x3A\x2A\x65\x47\x48\x10\x05\x13\x10\x0E\x07\x00\x29\x5B"
"\x5E\x59\x2A\x3E\x2F\x36\x21\x79\x03\x12\x56\x06\x01\x10\x18\x12\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x2B\x0D\x16\x2E\x7B\x2B\x3C\x2C\x29\x35\x3E\x37\x20\x70\x79\x25\x34\x74\x24\x3F\x2E\x3A\x30\x6E\x3C\x3C\x22\x2D\x2F\x25\x33\x6F\x4D\x37\x26\x5A\x08\x0C\x18\x0C\x0B\x53\x0E\x06\x1C\x00\x51\x4A"
"\x04\x02\x16\x54\x4B\x48\x49\x3D\x1B\x0D\x1F\x16\x43\x0F\x13\x46\x14\x10\x0A\x2A\x7B\x39\x79\x2D\x3A\x2E\x2B\x3B\x30\x35\x5B\x25\x34\x74\x36\x25\x25\x2E\x20\x29\x6F\x70\x3E\x34\x20\x7E\x61\x35\x33\x25\x37\x0E\x46\x58\x18\x0B\x0B\x13\x01\x1F\x12\x1E\x04\x17\x1B\x08\x11\x03\x18\x09\x0B\x02\x0A\x08\x4D"
"\x42\x43\x23\x09\x07\x09\x03\x00\x7A\x28\x2C\x38\x2C\x2B\x7C\x29\x2B\x23\x35\x5B\x25\x34\x3C\x21\x2B\x38\x23\x3A\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x11\x1B\x08\x13\x59\x2D\x1C\x14\x18\x16\x06\x1C\x14\x04\x57\x17\x1A\x04\x18\x07\x05\x0B\x4F\x44\x1E\x01\x0B\x14\x00\x15\x0C\x17\x45\x75\x38\x2A"
"\x3C\x3F\x2B\x39\x7D\x7D\x27\x3E\x71\x38\x36\x39\x30\x6A\x64\x3C\x3B\x6E\x2C\x21\x29\x48\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x59\x04\x17\x55\x0E\x0A\x01\x05\x17\x4F\x43\x1E\x16\x43\x50\x58\x5C\x57\x54\x4C\x50\x51\x1A\x18\x0A\x1C\x14\x7D\x14\x1A\x1C\x14\x05\x5D"
"\x14\x30\x29\x23\x27\x69\x21\x29\x2A\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x33\x11\x1D\x1B\x5F\x1F\x12\x1F\x1E\x11\x1F\x12\x04\x54\x13\x18\x04\x05\x49\x01\x1A\x18\x1D\x17\x17\x6A\x12\x03\x13\x44\x33\x1B\x09\x65\x2F\x3F\x33\x29\x38\x72\x73\x70\x71\x76\x77\x07\x30\x3E\x6B\x29\x69\x38\x2E\x3E\x24\x23"
"\x21\x2C\x24\x66\x6F\x61\x13\x3B\x29\x5D\x59\x0C\x1A\x1D\x19\x01\x53\x19\x05\x5F\x7D\x1D\x13\x4A\x44\x48\x0F\x01\x1D\x4C\x42\x42\x04\x0F\x15\x09\x47\x4B\x45\x39\x3A\x34\x35\x7E\x7F\x7C\x1E\x3D\x3D\x24\x23\x39\x3B\x74\x33\x26\x24\x3F\x43\x2D\x27\x23\x24\x21\x26\x60\x6E\x66\x34\x21\x31\x5A\x54\x08\x59"
"\x5E\x5F\x5C\x3C\x01\x18\x50\x05\x1E\x12\x54\x00\x19\x0E\x1A\x49\x08\x00\x1E\x4D\x0B\x0D\x10\x14\x12\x6D\x14\x04\x2F\x28\x3D\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x1C\x29\x20\x3A\x6F\x2A\x22\x30\x63\x21\x61\x2D\x22\x3D\x4F\x0E\x12\x15\x1C\x11\x0A\x08\x5D\x5D\x07\x50\x44\x56\x57\x54"
"\x55\x4A\x4B\x48\x3E\x0F\x06\x18\x4D\x57\x43\x13\x04\x05\x08\x0A\x01\x29\x51\x7D\x0C\x0D\x1A\x0E\x0D\x00\x1C\x16\x18\x1A\x12\x71\x79\x6A\x6E\x1C\x0C\x03\x1F\x69\x61\x62\x66\x13\x38\x35\x33\x21\x28\x28\x14\x17\x0D\x5B\x5F\x5C\x5D\x27\x00\x15\x17\x03\x1B\x54\x17\x1F\x02\x04\x1D\x43\x06\x02\x4D\x14\x02"
"\x12\x08\x07\x05\x08\x00\x29"
);

MAN(powershell, "PowerShell Reference",
"\x0D\x13\x19\x0D\x7E\x16\x0F\x7D\x02\x1C\x07\x14\x04\x04\x1C\x10\x06\x07\x77\x43\x0F\x6F\x21\x22\x30\x26\x60\x31\x29\x30\x21\x37\x1C\x0E\x14\x59\x1D\x10\x11\x10\x13\x1D\x14\x51\x05\x1F\x11\x19\x06\x4B\x0A\x1C\x07\x03\x18\x4D\x0D\x0D\x40\x4F\x28\x22\x30\x4B\x7A\x02\x37\x2C\x7E\x3C\x3D\x33\x72\x21\x25"
"\x3F\x76\x36\x38\x39\x6A\x08\x05\x0D\x6E\x2C\x23\x20\x2F\x22\x2E\x25\x35\x4D\x34\x29\x0F\x08\x58\x0D\x16\x10\x09\x0E\x13\x1D\x14\x02\x56\x18\x12\x55\x09\x06\x0C\x05\x0B\x1B\x1F\x4D\x4A\x35\x05\x13\x04\x4A\x2A\x0A\x2F\x35\x78\x3A\x31\x32\x31\x3C\x3C\x37\x23\x71\x22\x3F\x35\x21\x6A\x39\x2D\x3D\x3B\x3D"
"\x22\x6D\x2D\x21\x2A\x24\x25\x33\x37\x6C\x54\x71\x2F\x10\x10\x1B\x13\x0A\x01\x53\x41\x40\x56\x04\x1C\x1C\x1A\x18\x48\x39\x01\x18\x09\x1F\x31\x0B\x05\x0D\x0A\x47\x51\x4B\x6B\x7B\x39\x37\x3A\x7F\x28\x35\x37\x73\x3D\x3E\x32\x32\x26\x3B\x6A\x1B\x27\x3E\x2B\x3D\x1F\x25\x27\x2F\x2C\x61\x71\x67\x6C\x35\x0D"
"\x08\x10\x50\x5E\x09\x15\x1C\x52\x07\x18\x14\x7C\x24\x00\x1A\x18\x0E\x46\x49\x39\x06\x02\x09\x0D\x14\x13\x41\x32\x02\x16\x08\x33\x35\x39\x35\x7E\x37\x33\x2E\x26\x20\x70\x33\x39\x23\x3C\x7B\x40\x41\x07\x19\x0B\x01\x05\x03\x05\x63\x09\x15\x4C\x6A\x64\x12\x13\x15\x53\x21\x5E\x41\x5C\x29\x17\x01\x1D\x18"
"\x18\x16\x18\x55\x42\x2A\x0C\x04\x07\x01\x4C\x1F\x07\x00\x0F\x0C\x0B\x02\x0A\x01\x3F\x3F\x78\x3F\x31\x2D\x7C\x2E\x2B\x20\x24\x34\x3B\x77\x20\x34\x39\x20\x3B\x60\x60\x45\x61\x6D\x16\x2B\x29\x32\x66\x37\x36\x2A\x1D\x09\x19\x14\x44\x5F\x5B\x0D\x1D\x04\x15\x03\x05\x1F\x11\x19\x06\x4C\x48\x08\x00\x0B\x4C"
"\x19\x0A\x06\x40\x15\x03\x15\x09\x0C\x34\x3A\x34\x79\x3D\x30\x31\x30\x33\x3D\x34\x7F\x5C\x5D\x12\x1C\x18\x18\x1C\x69\x0D\x00\x01\x00\x03\x0D\x04\x12\x4C\x00\x21\x31\x57\x38\x17\x14\x13\x1E\x12\x19\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x22\x06\x1F\x19\x42\x02\x0C\x0D\x46\x06\x12\x04\x33\x37"
"\x39\x3B\x32\x3A\x7C\x3E\x3D\x3E\x3D\x30\x38\x33\x27\x5F\x0D\x2E\x3C\x64\x06\x2A\x20\x3D\x62\x04\x25\x35\x6B\x17\x36\x2A\x19\x1E\x0B\x0A\x5E\x5F\x5C\x35\x17\x1F\x00\x51\x10\x18\x06\x55\x0B\x05\x11\x49\x0D\x02\x08\x01\x07\x17\x40\x49\x21\x02\x10\x48\x12\x3E\x34\x29\x7E\x72\x13\x33\x3E\x3A\x3E\x34\x76"
"\x38\x24\x30\x24\x38\x68\x2D\x21\x2C\x3F\x64\x48\x04\x25\x35\x6B\x17\x36\x2A\x19\x1E\x0B\x0A\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x38\x1E\x06\x07\x07\x01\x0B\x4D\x12\x11\x0F\x02\x03\x14\x17\x00\x29\x51\x1F\x3C\x2A\x72\x0F\x38\x20\x25\x39\x32\x33\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D"
"\x62\x10\x25\x33\x30\x2E\x27\x20\x09\x71\x3F\x1C\x0A\x52\x3F\x15\x1B\x1F\x14\x38\x02\x12\x19\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x2F\x09\x12\x12\x47\x02\x0C\x36\x3E\x2B\x79\x76\x3E\x30\x34\x33\x20\x6A\x71\x32\x3E\x26\x75\x65\x6B\x24\x3A\x67\x45\x1F\x28\x36\x6E\x0C\x2E\x25\x26\x30\x2C\x15\x15\x58"
"\x3A\x44\x23\x5C\x5D\x52\x53\x50\x51\x56\x34\x1C\x14\x04\x0C\x0D\x49\x08\x00\x00\x09\x07\x11\x40\x49\x07\x0B\x0D\x04\x29\x61\x78\x3A\x3A\x76\x56\x1A\x37\x27\x7D\x12\x39\x39\x20\x30\x24\x3F\x68\x2F\x27\x23\x29\x63\x36\x3B\x34\x61\x66\x67\x14\x37\x13\x15\x0C\x59\x1F\x5F\x1A\x14\x1E\x16\x50\x59\x17\x1B"
"\x1D\x14\x19\x51\x48\x0A\x0F\x1B\x4C\x42\x42\x17\x19\x11\x03\x4E\x6E\x22\x3F\x2F\x75\x1D\x3F\x2B\x39\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x0F\x38\x30\x31\x25\x2F\x32\x67\x20\x24\x0E\x1E\x57\x0D\x17\x12\x19\x77\x35\x16\x04\x5C\x3A\x18\x17\x14\x1E\x02\x07\x07\x4E\x4F\x4C\x4D\x42"
"\x43\x40\x41\x46\x47\x44\x26\x2F\x29\x2A\x3C\x30\x2B\x7C\x3B\x3D\x3F\x34\x34\x24\x77\x7C\x34\x26\x22\x29\x3A\x74\x6F\x3C\x3A\x26\x6A\x4A\x4B\x12\x0F\x01\x65\x2A\x32\x28\x3C\x32\x36\x32\x38\x52\x0F\x7A\x3E\x14\x1D\x11\x16\x1E\x18\x48\x0F\x02\x00\x1B\x4D\x16\x0B\x12\x0E\x13\x00\x0C\x45\x2E\x33\x3D\x79"
"\x2E\x36\x2C\x38\x3E\x3A\x3E\x34\x6C\x5D\x74\x75\x0D\x2E\x3C\x64\x1E\x3D\x23\x2E\x27\x30\x33\x61\x3A\x67\x17\x2A\x08\x0F\x55\x36\x1C\x15\x19\x1E\x06\x53\x33\x21\x23\x57\x59\x31\x0F\x18\x0B\x0C\x00\x0B\x05\x03\x05\x43\x1C\x41\x35\x02\x08\x00\x39\x2F\x75\x16\x3C\x35\x39\x3E\x26\x73\x7D\x17\x3F\x25\x27"
"\x21\x6A\x7E\x42\x69\x6E\x08\x29\x39\x6F\x10\x25\x33\x30\x2E\x27\x20\x5A\x07\x58\x2E\x16\x1A\x0E\x18\x5F\x3C\x12\x1B\x13\x14\x00\x55\x39\x1F\x09\x1D\x1B\x1C\x4C\x40\x07\x12\x40\x46\x34\x12\x0A\x0B\x33\x35\x3F\x7E\x7E\x23\x7C\x0E\x37\x3F\x35\x32\x22\x77\x1A\x34\x27\x2E\x64\x0D\x27\x3C\x3C\x21\x23\x3A"
"\x0E\x20\x2B\x22\x4E\x65\x5A\x3C\x1D\x0D\x53\x2F\x0E\x12\x11\x16\x03\x02\x56\x0B\x54\x30\x12\x1B\x07\x1B\x1A\x42\x2F\x1E\x14\x43\x10\x13\x09\x04\x01\x16\x29\x3E\x2B\x77\x3D\x2C\x2A\x57\x58\x06\x03\x14\x10\x02\x18\x75\x09\x06\x0C\x05\x0B\x1B\x1F\x47\x11\x37\x2F\x31\x6B\x17\x36\x2A\x19\x1E\x0B\x0A\x5E"
"\x52\x32\x1C\x1F\x16\x50\x1F\x19\x03\x11\x05\x0B\x0F\x48\x49\x4E\x4F\x27\x04\x0E\x0F\x40\x00\x46\x17\x16\x0A\x39\x3E\x2B\x2A\x54\x0C\x28\x3C\x20\x27\x7D\x02\x33\x25\x22\x3C\x29\x2E\x68\x3A\x3E\x20\x23\x21\x27\x31\x60\x6E\x66\x14\x30\x2A\x0A\x56\x2B\x1C\x0C\x09\x15\x1E\x17\x53\x5F\x51\x24\x12\x07\x01"
"\x0B\x19\x1C\x44\x3D\x0A\x1E\x1B\x0B\x00\x05\x41\x49\x47\x37\x00\x2E\x76\x0B\x3C\x2C\x29\x35\x3E\x37\x59\x17\x34\x22\x7A\x1D\x21\x2F\x26\x68\x66\x6E\x1D\x29\x20\x2D\x35\x25\x6C\x0F\x33\x21\x28\x5A\x54\x58\x3A\x11\x0F\x05\x50\x3B\x07\x15\x1C\x56\x58\x54\x38\x05\x1D\x0D\x44\x27\x1B\x09\x00\x42\x4C\x40"
"\x2F\x03\x10\x49\x2C\x2E\x3E\x35\x79\x71\x7F\x0E\x38\x3C\x32\x3D\x34\x7B\x1E\x20\x30\x27\x41\x1C\x2C\x3D\x3B\x61\x0E\x2D\x2D\x2E\x24\x25\x33\x2D\x2A\x14\x5B\x1F\x16\x11\x18\x10\x18\x5C\x10\x1F\x1C\x56\x57\x54\x55\x26\x02\x03\x0C\x4E\x1F\x05\x03\x05\x69\x32\x04\x15\x08\x08\x13\x3F\x76\x1C\x37\x2D\x11"
"\x3D\x30\x37\x73\x35\x29\x37\x3A\x24\x39\x2F\x65\x2B\x26\x23\x6F\x6C\x6D\x06\x0D\x13\x61\x2A\x28\x2B\x2E\x0F\x0B\x72\x3E\x1B\x0B\x51\x33\x17\x07\x39\x21\x37\x13\x10\x07\x0F\x18\x1B\x49\x41\x4F\x2B\x08\x16\x4E\x2E\x04\x12\x26\x00\x04\x2A\x2F\x3D\x2B\x7E\x7F\x7C\x13\x37\x27\x27\x3E\x24\x3C\x74\x3C\x24"
"\x2D\x27\x43\x09\x2A\x38\x60\x15\x2A\x2E\x25\x29\x30\x37\x10\x0A\x1F\x19\x0D\x1B\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x40\x07\x0B\x0A\x08\x1E\x42\x33\x33\x36\x0F\x09\x00\x0A\x2D\x28\x0D\x29\x3A\x3E\x28\x38\x72\x3E\x3F\x35\x23\x3B\x31\x7C\x40\x0C\x2D\x3D\x63\x07\x23\x39\x04\x2A\x38\x61"
"\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x3C\x04\x18\x1C\x08\x02\x03\x09\x09\x42\x16\x10\x05\x07\x13\x01\x16\x50\x1C\x3D\x2D\x73\x1C\x33\x30\x22\x26\x24\x34\x24\x1E\x3A\x33\x25\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x16\x03\x08\x0C\x1C\x13\x5F\x0F"
"\x08\x1F\x1E\x11\x03\x0F\x7D\x33\x10\x1E\x46\x2B\x00\x03\x26\x02\x1E\x16\x02\x0E\x02\x03\x47\x33\x0C\x34\x68\x6A\x06\x0E\x2D\x33\x3E\x37\x20\x23\x3E\x24\x77\x74\x75\x02\x2A\x3A\x2D\x39\x2E\x3E\x28\x62\x2A\x2E\x27\x29\x67\x32\x2C\x1B\x5B\x2F\x34\x37\x50\x3F\x34\x3F\x79\x23\x14\x02\x5A\x31\x0D\x0F\x08"
"\x1D\x1D\x07\x00\x02\x3D\x0D\x0F\x09\x02\x1F\x47\x44\x45\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x11\x3B\x31\x3F\x31\x32\x74\x26\x29\x39\x21\x39\x3A\x6F\x3C\x22\x2E\x2A\x23\x38\x4C\x00\x21\x31\x57\x38\x17\x17\x0A\x1A\x12\x09\x52\x0F\x50\x22\x13\x03\x59\x36\x05\x05\x1C\x0C\x00\x1B\x4C\x4D\x42\x43\x40\x33\x03"
"\x06\x00\x45\x75\x7B\x2F\x2B\x37\x2B\x39\x7D\x34\x3A\x3C\x34\x25\x5D\x17\x3A\x24\x3D\x2D\x3B\x3A\x1B\x23\x60\x08\x30\x2F\x2F\x66\x68\x64\x06\x15\x15\x0E\x1C\x0C\x0B\x3A\x0F\x1D\x1E\x5D\x3B\x05\x18\x1A\x55\x4A\x4B\x22\x3A\x21\x21\x4C\x1E\x17\x13\x10\x0E\x14\x13\x6E\x2C\x34\x2D\x37\x32\x3B\x72\x0B\x38"
"\x30\x01\x35\x20\x23\x32\x27\x21\x6A\x1E\x1A\x05\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x03\x2B\x32\x14\x17\x17\x18\x1A\x5F\x1D\x5D\x05\x16\x12\x51\x06\x16\x13\x10\x4A\x43\x09\x05\x07\x0E\x1F\x57\x42\x0A\x17\x13\x4F\x6D\x23\x00\x2E\x76\x0E\x36\x32\x2A\x31\x38\x72\x7C\x70\x16\x33\x23\x79\x11\x23\x38\x23"
"\x69\x61\x6F\x0B\x28\x36\x6E\x10\x20\x34\x33\x2D\x31\x13\x14\x16\x59\x5E\x5F\x2F\x09\x1D\x01\x11\x16\x13\x7D\x7E\x34\x26\x22\x29\x3A\x2B\x3C\x66\x0E\x0F\x07\x40\x00\x0A\x0E\x05\x16\x3F\x28\x78\x2E\x31\x2D\x37\x7D\x26\x3C\x3F\x6B\x76\x34\x30\x79\x6A\x2F\x21\x3B\x62\x6F\x20\x3E\x6E\x63\x23\x20\x32\x6B"
"\x64\x26\x0A\x57\x58\x14\x08\x53\x5C\x0F\x1F\x5F\x50\x01\x01\x13\x58\x55\x0F\x08\x00\x06\x42\x4F\x0F\x01\x11\x4D\x4E\x4F\x6C\x20\x01\x11\x77\x1A\x34\x30\x3F\x2C\x7C\x31\x3B\x20\x24\x22\x76\x36\x38\x39\x6A\x24\x2E\x69\x3A\x27\x29\x20\x6C\x49\x4A\x11\x14\x08\x02\x0C\x36\x3E\x2B\x59\x58\x5F\x2F\x3E\x20"
"\x3A\x20\x25\x25\x7D\x59\x55\x4E\x3B\x3A\x26\x28\x26\x20\x28\x42\x10\x08\x0E\x11\x14\x44\x1C\x35\x2E\x2A\x79\x2E\x2D\x33\x3B\x3B\x3F\x35\x71\x25\x34\x26\x3C\x3A\x3F\x68\x39\x2F\x3B\x24\x6D\x6A\x31\x35\x2F\x35\x67\x25\x31\x5A\x1E\x0E\x1C\x0C\x06\x5C\x0E\x06\x12\x02\x05\x5F\x59\x7E\x58\x4A\x38\x09\x1F"
"\x0B\x4F\x1F\x0E\x10\x0A\x10\x15\x15\x47\x05\x16\x7A\x75\x28\x2A\x6F\x71\x7C\x0F\x27\x3D\x70\x25\x3E\x32\x39\x75\x3D\x22\x3C\x21\x74\x6F\x62\x11\x31\x20\x32\x28\x36\x33\x6A\x35\x09\x4A\x72\x54\x5E\x3A\x04\x18\x11\x06\x04\x18\x19\x19\x54\x05\x05\x07\x01\x0A\x17\x55\x4C\x3E\x07\x17\x4D\x24\x1E\x02\x07"
"\x10\x2E\x32\x37\x37\x0E\x30\x30\x34\x31\x2A\x70\x03\x33\x3A\x3B\x21\x2F\x18\x21\x2E\x20\x2A\x28\x6D\x6F\x10\x23\x2E\x36\x22\x64\x06\x0F\x09\x0A\x1C\x10\x0B\x29\x0E\x17\x01\x7A\x7B\x33\x3B\x31\x23\x2B\x3F\x21\x26\x20\x65\x3F\x19\x03\x11\x14\x4C\x36\x15\x0B\x06\x3F\x28\x2B\x79\x2E\x30\x2B\x38\x20\x20"
"\x38\x34\x3A\x3B\x74\x78\x1C\x2E\x3A\x2B\x6E\x1D\x39\x23\x03\x30\x60\x61\x66\x67\x16\x30\x14\x5B\x19\x59\x10\x1A\x0B\x5D\x17\x1F\x15\x07\x17\x03\x11\x11\x4A\x18\x00\x0C\x02\x03\x66\x3E\x16\x02\x12\x15\x4B\x37\x16\x0A\x39\x3E\x2B\x2A\x7E\x72\x0A\x38\x20\x31\x70\x03\x23\x39\x15\x26\x6A\x25\x27\x3D\x2B"
"\x3F\x2D\x29\x62\x63\x60\x61\x66\x67\x64\x17\x0F\x15\x58\x18\x10\x06\x5C\x1C\x02\x03\x50\x10\x05\x57\x15\x11\x07\x02\x06\x63\x64\x23\x29\x2C\x30\x2D\x29\x2F\x21\x47\x29\x2A\x08\x1E\x52\x0D\x27\x2F\x39\x7D\x75\x37\x3F\x32\x25\x7A\x24\x3A\x3D\x2E\x3A\x3A\x26\x2A\x20\x21\x6F\x20\x2D\x25\x66\x00\x21\x31"
"\x57\x2B\x0A\x16\x1D\x1A\x0F\x0E\x55\x53\x19\x1F\x56\x03\x1C\x1C\x19\x4B\x18\x1B\x01\x08\x1E\x0C\x0F\x43\x14\x0E\x46\x14\x01\x04\x28\x38\x30\x79\x2A\x37\x39\x57\x3D\x35\x36\x38\x35\x3E\x35\x39\x6A\x06\x21\x2A\x3C\x20\x3F\x22\x24\x37\x60\x11\x29\x30\x21\x37\x29\x13\x1D\x15\x12\x5F\x18\x12\x11\x06\x1D"
"\x14\x18\x03\x15\x01\x03\x04\x06\x45\x4E\x00\x1E\x4D\x45\x07\x0F\x02\x15\x4A\x14\x0A\x2D\x3E\x2A\x2A\x36\x3A\x30\x31\x75\x73\x24\x3E\x76\x25\x31\x34\x2E\x41\x3C\x21\x2B\x6F\x28\x22\x21\x30\x60\x2D\x27\x29\x20\x2C\x14\x1C\x58\x09\x1F\x18\x19\x53"
);

MAN(files, "File Explorer Guide",
"\x15\x0B\x1D\x17\x17\x11\x1B\x7D\x1B\x07\x5A\x7C\x76\x00\x3D\x3B\x61\x0E\x66\x69\x1E\x26\x22\x23\x27\x27\x60\x28\x28\x67\x30\x2D\x1F\x5B\x0C\x18\x0D\x14\x1E\x1C\x00\x53\x12\x08\x56\x13\x11\x13\x0B\x1E\x04\x1D\x40\x4F\x38\x05\x0B\x10\x40\x11\x14\x08\x03\x17\x3B\x36\x62\x79\x79\x39\x35\x31\x37\x20\x77"
"\x7F\x5C\x5D\x00\x1D\x0F\x6B\x04\x08\x17\x00\x19\x19\x48\x6E\x60\x0F\x27\x31\x2D\x22\x1B\x0F\x11\x16\x10\x5F\x0C\x1C\x1C\x16\x50\x59\x1A\x12\x12\x01\x43\x51\x48\x38\x1B\x06\x0F\x06\x42\x02\x03\x02\x03\x14\x17\x49\x7A\x14\x36\x3C\x1A\x2D\x35\x2B\x37\x7F\x70\x05\x3E\x3E\x27\x75\x1A\x08\x64\x69\x00\x2A"
"\x38\x3A\x2D\x31\x2B\x6F\x4C\x6A\x64\x04\x1E\x1F\x0A\x1C\x0D\x0C\x5C\x1F\x13\x01\x4A\x51\x15\x1B\x1D\x16\x01\x4B\x1C\x06\x4E\x1B\x15\x1D\x07\x43\x01\x41\x16\x06\x10\x0D\x7A\x73\x1B\x63\x02\x0A\x2F\x38\x20\x20\x0C\x08\x39\x22\x08\x11\x25\x28\x3D\x24\x2B\x21\x38\x3E\x6B\x6D\x4A\x6C\x66\x15\x2D\x27\x18"
"\x14\x16\x56\x0A\x10\x13\x11\x10\x12\x02\x4B\x56\x39\x11\x02\x46\x4B\x2B\x1C\x1A\x40\x2F\x02\x12\x1A\x4F\x31\x07\x14\x10\x00\x76\x7B\x0A\x3C\x30\x3E\x31\x38\x7E\x73\x03\x39\x37\x25\x31\x79\x6A\x18\x27\x3B\x3A\x63\x6C\x1B\x2B\x26\x37\x6F\x68\x69\x4E\x68\x5A\x28\x0C\x18\x0A\x0A\x0F\x5D\x10\x12\x02\x51"
"\x5E\x15\x1B\x01\x1E\x04\x05\x40\x54\x4F\x05\x19\x07\x0E\x40\x02\x09\x12\x0A\x11\x7A\x3A\x36\x3D\x7E\x2B\x33\x29\x33\x3F\x70\x22\x3F\x2D\x31\x75\x3D\x23\x2D\x27\x6E\x36\x23\x38\x62\x30\x25\x2D\x23\x24\x30\x65\x1C\x12\x14\x1C\x0D\x51\x76\x77\x23\x26\x39\x32\x3D\x57\x35\x36\x29\x2E\x3B\x3A\x64\x42\x4C"
"\x39\x0A\x06\x40\x15\x09\x17\x44\x16\x3F\x38\x2C\x30\x31\x31\x7C\x32\x34\x73\x24\x39\x33\x77\x3A\x34\x3C\x22\x2F\x28\x3A\x26\x23\x23\x62\x33\x21\x2F\x23\x67\x34\x2C\x14\x08\x58\x00\x11\x0A\x0E\x5D\x1F\x1C\x03\x05\x5B\x02\x07\x10\x0E\x4B\x0E\x06\x02\x0B\x09\x1F\x11\x4D\x6A\x4C\x46\x37\x0D\x0B\x7A\x3A"
"\x36\x20\x7E\x39\x33\x31\x36\x36\x22\x6B\x76\x25\x3D\x32\x22\x3F\x65\x2A\x22\x26\x2F\x26\x62\x2A\x34\x61\x78\x67\x14\x2C\x14\x5B\x0C\x16\x5E\x2E\x09\x14\x11\x18\x50\x10\x15\x14\x11\x06\x19\x45\x62\x44\x4E\x38\x05\x03\x06\x0C\x17\x12\x46\x56\x55\x45\x31\x3E\x3D\x29\x2D\x7F\x2E\x38\x31\x36\x3E\x25\x76"
"\x31\x3D\x39\x2F\x38\x68\x20\x20\x6F\x1D\x38\x2B\x20\x2B\x61\x27\x24\x27\x20\x09\x08\x43\x59\x07\x10\x09\x5D\x11\x12\x1E\x51\x05\x1F\x1B\x02\x4A\x0D\x1D\x05\x02\x4F\x0A\x02\x0E\x07\x05\x13\x15\x6D\x44\x45\x2C\x32\x39\x79\x11\x2F\x28\x34\x3D\x3D\x23\x71\x7E\x04\x31\x30\x6A\x26\x27\x3B\x2B\x6F\x72\x6D"
"\x0D\x33\x34\x28\x29\x29\x37\x65\x44\x5B\x28\x0B\x17\x09\x1D\x1E\x0B\x53\x03\x14\x15\x03\x1D\x1A\x04\x42\x46\x63\x64\x3B\x24\x24\x31\x43\x30\x22\x6C\x4A\x44\x36\x32\x34\x2F\x2A\x7E\x3E\x30\x31\x72\x37\x22\x38\x20\x32\x27\x75\x2B\x25\x2C\x69\x37\x20\x39\x3F\x62\x36\x33\x24\x34\x67\x22\x2A\x16\x1F\x1D"
"\x0B\x0D\x5F\x54\x39\x17\x00\x1B\x05\x19\x07\x58\x55\x2E\x04\x0B\x1C\x03\x0A\x02\x19\x11\x4F\x40\x25\x09\x10\x0A\x09\x35\x3A\x3C\x2A\x72\x55\x7C\x7D\x1F\x26\x23\x38\x35\x7B\x74\x05\x23\x28\x3C\x3C\x3C\x2A\x3F\x61\x62\x15\x29\x25\x23\x28\x37\x6C\x54\x71\x55\x59\x2C\x16\x1B\x15\x06\x5E\x13\x1D\x1F\x14"
"\x1F\x55\x0B\x4B\x0C\x1B\x07\x19\x09\x4D\x5C\x43\x30\x13\x09\x17\x01\x17\x2E\x32\x3D\x2A\x64\x7F\x29\x2E\x37\x37\x7F\x37\x24\x32\x31\x75\x39\x3B\x29\x2A\x2B\x63\x6C\x19\x2D\x2C\x2C\x32\x66\x33\x25\x27\x5A\x53\x1D\x0B\x0C\x10\x0E\x77\x52\x53\x13\x19\x13\x14\x1F\x1C\x04\x0C\x44\x49\x01\x1F\x18\x04\x0F"
"\x0A\x1A\x00\x12\x0E\x0B\x0B\x73\x77\x78\x31\x3F\x2D\x38\x2A\x33\x21\x35\x7F\x5C\x5D\x07\x10\x0B\x19\x0B\x01\x44\x62\x6C\x0E\x36\x31\x2C\x6A\x00\x67\x2B\x37\x5A\x0F\x10\x1C\x5E\x0C\x19\x1C\x00\x10\x18\x51\x14\x18\x0C\x5B\x4A\x38\x0D\x08\x1C\x0C\x04\x4D\x00\x1A\x40\x0F\x07\x0A\x01\x49\x7A\x34\x2A\x79"
"\x29\x36\x28\x35\x72\x20\x29\x3F\x22\x36\x2C\x6F\x40\x6B\x68\x63\x60\x25\x3C\x2A\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x18\x12\x13\x5C\x17\x02\x14\x50\x17\x1F\x1B\x11\x06\x60\x4B\x48\x1A\x07\x15\x09\x57\x5C\x56\x50\x51\x2B\x25\x44\x45\x7A\x7B\x78\x35\x3F\x2D\x3B\x38\x72\x35\x39\x3D\x33\x24\x5E"
"\x75\x6A\x2F\x29\x3D\x2B\x75\x38\x25\x2B\x30\x60\x36\x23\x22\x2F\x65\x5A\x09\x1D\x1A\x1B\x11\x08\x5D\x14\x1A\x1C\x14\x05\x7D\x54\x55\x01\x02\x06\x0D\x54\x06\x01\x0C\x05\x06\x40\x41\x46\x47\x44\x45\x35\x35\x34\x20\x7E\x2F\x35\x3E\x26\x26\x22\x34\x25\x5D\x79\x75\x19\x2E\x29\x3B\x2D\x27\x6C\x24\x2C\x30"
"\x29\x25\x23\x67\x25\x65\x1C\x14\x14\x1D\x1B\x0D\x5C\x0E\x17\x12\x02\x12\x1E\x12\x07\x55\x1E\x03\x09\x1D\x4E\x09\x03\x01\x06\x06\x12\x41\x07\x09\x00\x45\x3B\x37\x34\x79\x2D\x2A\x3E\x3B\x3D\x3F\x34\x34\x24\x24\x7A\x5F\x40\x1D\x01\x0C\x19\x1C\x6C\x6B\x62\x10\x0F\x13\x12\x0E\x0A\x02\x70\x56\x58\x2F\x17"
"\x1A\x0B\x5D\x1F\x16\x1E\x04\x4C\x57\x1D\x16\x05\x05\x1B\x49\x02\x0E\x1E\x0A\x07\x4C\x0D\x04\x02\x0E\x11\x08\x75\x28\x35\x38\x32\x33\x70\x7D\x3E\x3A\x23\x25\x7A\x77\x30\x30\x3E\x2A\x21\x25\x3D\x63\x6C\x39\x2B\x2F\x25\x32\x68\x4D\x69\x65\x3E\x1E\x0C\x18\x17\x13\x0F\x5D\x04\x1A\x15\x06\x4C\x57\x17\x19"
"\x03\x08\x03\x49\x0D\x00\x00\x18\x0F\x0D\x40\x09\x03\x06\x00\x00\x28\x28\x78\x2D\x31\x7F\x2F\x32\x20\x27\x6B\x71\x24\x3E\x33\x3D\x3E\x66\x2B\x25\x27\x2C\x27\x6D\x2A\x26\x21\x25\x23\x35\x37\x65\x0E\x14\x58\x18\x1A\x1B\x76\x5D\x52\x10\x1F\x1D\x03\x1A\x1A\x06\x4A\x43\x2C\x08\x1A\x0A\x4C\x00\x0D\x07\x09"
"\x07\x0F\x02\x00\x49\x7A\x0F\x21\x29\x3B\x73\x7C\x0E\x3B\x29\x35\x7F\x78\x79\x7D\x7B\x40\x66\x68\x0E\x3C\x20\x39\x3D\x62\x21\x39\x6D\x66\x34\x2B\x37\x0E\x5B\x1A\x00\x5E\x1E\x12\x19\x52\x03\x02\x14\x00\x1E\x11\x02\x4A\x1B\x09\x07\x0B\x4F\x0D\x1F\x07\x43\x09\x0F\x46\x13\x0C\x00\x7A\x0D\x31\x3C\x29\x7F"
"\x31\x38\x3C\x26\x7E\x5B\x5C\x1F\x1D\x11\x0E\x0E\x06\x69\x08\x06\x00\x08\x11\x49\x6D\x61\x10\x2E\x21\x32\x5A\x45\x58\x2A\x16\x10\x0B\x5D\x4C\x53\x38\x18\x12\x13\x11\x1B\x4A\x02\x1C\x0C\x03\x1C\x40\x4D\x0D\x11\x40\x27\x09\x0B\x00\x00\x28\x7B\x37\x29\x2A\x36\x33\x33\x21\x73\x6E\x71\x00\x3E\x31\x22\x6A"
"\x75\x68\x1A\x26\x20\x3B\x6D\x2A\x2A\x24\x25\x23\x29\x64\x23\x13\x17\x1D\x0A\x50\x75\x51\x5D\x21\x06\x00\x14\x04\x5A\x1C\x1C\x0E\x0F\x0D\x07\x4E\x1C\x15\x1E\x16\x06\x0D\x41\x00\x0E\x08\x00\x29\x61\x78\x1F\x31\x33\x38\x38\x20\x73\x3F\x21\x22\x3E\x3B\x3B\x39\x6B\x76\x69\x3B\x21\x2F\x25\x27\x20\x2B\x61"
"\x61\x0F\x2D\x21\x1F\x5B\x08\x0B\x11\x0B\x19\x1E\x06\x16\x14\x7B\x56\x57\x1B\x05\x0F\x19\x09\x1D\x07\x01\x0B\x4D\x11\x1A\x13\x15\x03\x0A\x44\x03\x33\x37\x3D\x2A\x79\x71\x7C\x75\x06\x3B\x39\x22\x76\x27\x26\x3A\x2D\x39\x29\x24\x74\x6F\x24\x24\x26\x27\x25\x2F\x6B\x21\x2D\x29\x1F\x08\x55\x16\x10\x53\x5C"
"\x0E\x1A\x1C\x07\x5C\x05\x02\x04\x10\x18\x46\x00\x00\x0A\x0B\x09\x03\x4C\x4A\x6A\x6B\x3C\x2E\x34\x45\x1C\x14\x14\x1D\x1B\x0D\x0F\x57\x7F\x73\x03\x34\x3A\x32\x37\x21\x6A\x2D\x21\x25\x2B\x3C\x6C\x73\x62\x31\x29\x26\x2E\x33\x69\x26\x16\x12\x1B\x12\x5E\x41\x5C\x3E\x1D\x1E\x00\x03\x13\x04\x07\x55\x1E\x04"
"\x48\x33\x27\x3F\x4C\x0B\x0B\x0F\x05\x41\x4E\x30\x0D\x0B\x6B\x6A\x62\x79\x79\x1C\x33\x30\x22\x21\x35\x22\x25\x77\x20\x3A\x6D\x41\x68\x69\x3A\x27\x29\x23\x62\x19\x09\x11\x66\x21\x2D\x29\x1F\x52\x56\x59\x3A\x10\x09\x1F\x1E\x16\x5D\x12\x1A\x1E\x17\x1E\x4A\x0A\x48\x33\x27\x3F\x4C\x19\x0D\x43\x0F\x11\x03"
"\x09\x44\x0C\x2E\x60\x78\x3C\x26\x2B\x2E\x3C\x31\x27\x70\x27\x3F\x36\x74\x27\x23\x2C\x20\x3D\x63\x2C\x20\x24\x21\x28\x6E\x4B\x6B\x67\x10\x2D\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x50\x12\x17\x19\x54\x0F\x03\x1B\x48\x08\x00\x0B\x4C\x18\x0C\x19\x09\x11\x46\x01\x16\x0A\x37\x7B\x2C\x31\x3B\x7F\x3F\x32"
"\x3F\x3E\x31\x3F\x32\x77\x38\x3C\x24\x2E\x68\x61\x34\x26\x3C\x60\x21\x31\x25\x20\x32\x22\x6B\x3F\x13\x0B\x55\x1C\x06\x0B\x0E\x1C\x11\x07\x59\x5F\x7C\x7D\x3A\x30\x3E\x3C\x27\x3B\x25\x4F\x28\x3F\x2B\x35\x25\x32\x6C\x4A\x44\x28\x3B\x2B\x78\x38\x7E\x2C\x34\x3C\x20\x36\x34\x71\x30\x38\x38\x31\x2F\x39\x72"
"\x69\x3C\x26\x2B\x25\x36\x6E\x23\x2D\x2F\x24\x2F\x65\x34\x1E\x0C\x0E\x11\x0D\x17\x5D\x4C\x53\x3D\x10\x06\x57\x1A\x10\x1E\x1C\x07\x1B\x05\x4F\x08\x1F\x0B\x15\x05\x4D\x46\x08\x16\x6F\x7A\x7B\x7F\x37\x3B\x2B\x7C\x28\x21\x36\x70\x0B\x6C\x77\x08\x09\x39\x2E\x3A\x3F\x2B\x3D\x10\x3E\x2A\x22\x32\x24\x61\x69"
"\x64\x11\x12\x12\x0B\x59\x0E\x0D\x13\x1A\x00\x12\x1D\x4B\x56\x19\x11\x01\x1D\x04\x1A\x02\x43\x09\x03\x01\x06\x06\x12\x4F\x6C\x6D\x22\x2C\x16\x1E\x78\x09\x1B\x0D\x11\x14\x01\x00\x19\x1E\x18\x04\x5E\x78\x6A\x19\x21\x2E\x26\x3B\x61\x2E\x2E\x2A\x23\x2A\x66\x26\x64\x23\x13\x17\x1D\x59\x40\x5F\x2C\x0F\x1D"
"\x03\x15\x03\x02\x1E\x11\x06\x4A\x55\x48\x3A\x0B\x0C\x19\x1F\x0B\x17\x19\x41\x12\x06\x06\x45\x29\x33\x37\x2E\x2D\x7F\x3D\x3E\x31\x36\x23\x22\x76\x25\x3D\x32\x22\x3F\x3B\x43\x6E\x6F\x64\x3F\x27\x22\x24\x6E\x31\x35\x2D\x31\x1F\x5B\x1E\x16\x0C\x5F\x19\x1C\x11\x1B\x50\x04\x05\x12\x06\x5C\x44\x4B\x26\x3D"
"\x28\x3C\x4C\x1D\x07\x11\x0D\x08\x15\x14\x0D\x0A\x34\x28\x78\x38\x30\x3B\x7C\x2E\x3A\x32\x22\x34\x76\x27\x31\x27\x27\x22\x3B\x3A\x27\x20\x22\x3E\x62\x22\x32\x24\x4C\x67\x64\x26\x15\x16\x1A\x10\x10\x1A\x18\x5D\x5F\x53\x04\x19\x13\x57\x07\x01\x18\x02\x0B\x1D\x0B\x1D\x4C\x02\x0C\x06\x40\x16\x0F\x09\x17"
"\x4B\x50\x51\x0B\x11\x1F\x0D\x19\x7D\x7D\x73\x03\x14\x18\x13\x5E\x78\x6A\x19\x21\x2E\x26\x3B\x61\x2E\x2E\x2A\x23\x2A\x66\x79\x64\x16\x12\x1A\x0A\x1C\x44\x5F\x12\x18\x13\x01\x12\x08\x56\x04\x1C\x14\x18\x02\x06\x0E\x4E\x47\x2E\x01\x17\x06\x14\x0E\x09\x13\x0C\x4A\x0D\x32\x75\x1F\x37\x7F\x28\x32\x72\x3D"
"\x35\x30\x24\x35\x2D\x75\x1A\x08\x3B\x60\x6E\x20\x3E\x47\x62\x63\x34\x29\x23\x67\x27\x29\x1B\x08\x0B\x10\x1D\x5F\x5B\x2E\x1A\x12\x02\x14\x56\x00\x1D\x01\x02\x4C\x46\x49\x20\x0A\x0D\x1F\x00\x1A\x40\x12\x0E\x06\x16\x0C\x34\x3C\x62\x79\x0D\x3A\x28\x29\x3B\x3D\x37\x22\x76\x69\x74\x06\x33\x38\x3C\x2C\x23"
"\x6F\x72\x6D\x0C\x26\x21\x33\x24\x3E\x4E\x65\x5A\x08\x10\x18\x0C\x16\x12\x1A\x5C\x53\x24\x19\x1F\x04\x54\x05\x18\x04\x0F\x1B\x0F\x02\x56\x4D\x45\x0D\x05\x00\x14\x05\x1D\x42\x74\x51\x52\x0D\x1F\x1D\x0F\x7D\x7A\x04\x39\x3F\x32\x38\x23\x26\x6A\x7A\x79\x60\x44\x62\x6C\x0B\x2B\x2F\x25\x61\x03\x3F\x34\x29"
"\x15\x09\x1D\x0B\x5E\x17\x1D\x0E\x52\x11\x02\x1E\x01\x04\x11\x07\x47\x18\x1C\x10\x02\x0A\x4C\x19\x03\x01\x13\x4F\x46\x24\x10\x17\x36\x70\x0C\x79\x30\x3A\x2B\x7D\x26\x32\x32\x7D\x76\x14\x20\x27\x26\x60\x1F\x69\x2D\x23\x23\x3E\x27\x6F\x4A\x61\x66\x2A\x2D\x21\x1E\x17\x1D\x54\x1D\x13\x15\x1E\x19\x53\x11"
"\x51\x10\x18\x18\x11\x0F\x19\x48\x1D\x01\x4F\x03\x1D\x07\x0D\x40\x08\x12\x47\x0D\x0B\x7A\x3A\x78\x37\x3B\x28\x7C\x29\x33\x31\x7E"
);

MAN(network, "Networking Guide",
"\x12\x14\x0F\x79\x10\x1A\x08\x0A\x1D\x01\x1B\x18\x18\x10\x74\x02\x05\x19\x03\x1A\x44\x62\x6C\x08\x34\x26\x32\x38\x66\x23\x21\x33\x13\x18\x1D\x59\x19\x1A\x08\x0E\x52\x12\x1E\x51\x3F\x27\x54\x14\x0E\x0F\x1A\x0C\x1D\x1C\x4C\x45\x07\x4D\x07\x4F\x46\x56\x5D\x57\x74\x6A\x6E\x61\x70\x6E\x72\x6C\x62\x7A\x70"
"\x7C\x76\x3E\x20\x26\x6A\x2A\x2C\x2D\x3C\x2A\x3F\x3E\x62\x2C\x2E\x61\x32\x2F\x21\x4F\x5A\x5B\x16\x1C\x0A\x08\x13\x0F\x19\x5D\x50\x23\x19\x02\x00\x10\x18\x18\x48\x01\x0F\x01\x08\x4D\x0D\x16\x14\x41\x07\x03\x00\x17\x3F\x28\x2B\x3C\x2D\x7F\x3D\x28\x26\x3C\x3D\x30\x22\x3E\x37\x34\x26\x27\x31\x69\x38\x26"
"\x2D\x6D\x06\x0B\x03\x11\x68\x4D\x69\x65\x3E\x35\x2B\x59\x0A\x0D\x1D\x13\x01\x1F\x11\x05\x13\x04\x54\x1B\x0B\x06\x0D\x1A\x4E\x47\x00\x08\x03\x11\x0E\x4F\x0B\x0E\x07\x17\x35\x28\x37\x3F\x2A\x71\x3F\x32\x3F\x7A\x70\x38\x38\x23\x3B\x75\x03\x1B\x68\x28\x2A\x2B\x3E\x28\x31\x30\x25\x32\x68\x4D\x69\x65\x23"
"\x14\x0D\x0B\x5E\x13\x13\x1E\x13\x1F\x50\x1F\x13\x03\x03\x1A\x18\x00\x48\x00\x1D\x4F\x0E\x08\x0A\x0A\x0E\x05\x46\x06\x44\x17\x35\x2E\x2C\x3C\x2C\x64\x7C\x29\x3A\x36\x70\x23\x39\x22\x20\x30\x38\x6C\x3B\x69\x19\x0E\x02\x6D\x31\x2A\x24\x24\x66\x20\x21\x31\x09\x5B\x19\x73\x5E\x5F\x0C\x08\x10\x1F\x19\x12"
"\x56\x3E\x24\x55\x0C\x19\x07\x04\x4E\x16\x03\x18\x10\x43\x29\x32\x36\x49\x6E\x6F\x0C\x12\x1D\x0E\x7E\x06\x13\x08\x00\x73\x13\x1E\x18\x11\x1D\x12\x1F\x19\x09\x1D\x07\x00\x02\x47\x6F\x63\x29\x31\x25\x28\x2A\x23\x13\x1C\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x23\x19\x19\x00\x07\x55\x13\x04\x1D\x1B\x4E\x26\x3C"
"\x1B\x56\x4C\x29\x31\x10\x51\x48\x45\x3D\x3A\x2C\x3C\x29\x3E\x25\x7D\x7A\x21\x3F\x24\x22\x32\x26\x7C\x6A\x2A\x26\x2D\x6E\x0B\x02\x1E\x62\x30\x25\x33\x30\x22\x36\x36\x54\x71\x55\x59\x17\x0F\x1F\x12\x1C\x15\x19\x16\x56\x58\x15\x19\x06\x4B\x48\x49\x3D\x07\x03\x1A\x11\x43\x0D\x0E\x14\x02\x5E\x45\x17\x1A"
"\x1B\x79\x3F\x3B\x38\x2F\x37\x20\x23\x7D\x76\x13\x1C\x16\x1A\x6B\x3B\x2C\x3C\x39\x29\x3F\x6E\x63\x2C\x24\x27\x34\x21\x65\x0E\x12\x15\x1C\x50\x75\x51\x5D\x26\x1B\x19\x02\x56\x07\x06\x1A\x0D\x19\x09\x04\x54\x4F\x4B\x04\x12\x00\x0F\x0F\x00\x0E\x03\x42\x7A\x3A\x36\x3D\x7E\x78\x3D\x39\x33\x23\x24\x34\x24"
"\x7A\x38\x3C\x39\x3F\x6F\x67\x44\x45\x0F\x05\x03\x0D\x07\x04\x66\x14\x01\x11\x2E\x32\x36\x3E\x2D\x75\x2F\x18\x06\x07\x19\x1F\x11\x04\x54\x4B\x4A\x25\x0D\x1D\x19\x00\x1E\x06\x42\x45\x40\x08\x08\x13\x01\x17\x34\x3E\x2C\x79\x60\x7F\x1D\x39\x24\x32\x3E\x32\x33\x33\x74\x3B\x2F\x3F\x3F\x26\x3C\x24\x6C\x3E"
"\x27\x37\x34\x28\x28\x20\x37\x65\x44\x5B\x35\x16\x0C\x1A\x5C\x13\x17\x07\x07\x1E\x04\x1C\x7E\x14\x0E\x0A\x18\x1D\x0B\x1D\x4C\x02\x12\x17\x09\x0E\x08\x14\x44\x5B\x7A\x29\x31\x3E\x36\x2B\x71\x3E\x3E\x3A\x33\x3A\x76\x36\x30\x34\x3A\x3F\x2D\x3B\x6E\x71\x6C\x1D\x30\x2C\x30\x24\x34\x33\x2D\x20\x09\x41\x72"
"\x54\x5E\x36\x2C\x0B\x46\x49\x50\x1E\x14\x03\x15\x1C\x04\x4B\x09\x1C\x1A\x00\x01\x0C\x16\x0A\x03\x00\x0A\x0B\x1D\x45\x72\x1F\x10\x1A\x0E\x76\x7C\x32\x20\x73\x23\x34\x22\x77\x27\x21\x2B\x3F\x21\x2A\x6E\x06\x1C\x61\x62\x2E\x21\x32\x2D\x6B\x64\x22\x1B\x0F\x1D\x0E\x1F\x06\x50\x5D\x36\x3D\x23\x5F\x7C\x5A"
"\x54\x3C\x3A\x1D\x5E\x53\x4E\x1A\x1F\x18\x03\x0F\x0C\x18\x46\x06\x11\x11\x35\x36\x39\x2D\x37\x3C\x7C\x75\x01\x1F\x11\x10\x15\x78\x10\x1D\x09\x1B\x3E\x7F\x67\x61\x46\x60\x62\x07\x0E\x12\x7C\x67\x75\x6B\x4B\x55\x49\x57\x4F\x5F\x54\x3E\x1E\x1C\x05\x15\x10\x1B\x15\x07\x0F\x42\x44\x49\x56\x41\x54\x43\x5A"
"\x4D\x58\x41\x4E\x20\x0B\x0A\x3D\x37\x3D\x70\x72\x7F\x65\x73\x6B\x7D\x69\x7F\x6F\x77\x7C\x04\x3F\x2A\x2C\x70\x67\x61\x6C\x19\x2A\x2A\x33\x4B\x66\x67\x34\x37\x15\x1C\x0A\x18\x13\x5F\x14\x1C\x01\x53\x1F\x1F\x13\x5A\x17\x19\x03\x08\x03\x49\x2A\x21\x3F\x4D\x11\x14\x09\x15\x05\x0F\x01\x16\x74\x51\x52\x17"
"\x1B\x0B\x0B\x12\x00\x18\x70\x01\x04\x18\x12\x1C\x06\x0E\x1B\x43\x63\x6F\x1C\x38\x20\x2F\x29\x22\x66\x31\x37\x65\x2A\x09\x11\x0F\x1F\x0B\x19\x53\x52\x23\x05\x13\x1A\x1E\x17\x55\x03\x18\x48\x1A\x0F\x09\x09\x1F\x42\x4B\x0E\x0E\x46\x01\x0D\x09\x3F\x7B\x2B\x31\x3F\x2D\x35\x33\x35\x7A\x6B\x71\x06\x25\x3D"
"\x23\x2B\x3F\x2D\x69\x2B\x21\x2D\x2F\x2E\x26\x33\x4B\x66\x67\x20\x2C\x09\x18\x17\x0F\x1B\x0D\x05\x5D\x13\x1D\x14\x51\x05\x1F\x15\x07\x03\x05\x0F\x47\x4E\x2C\x04\x0C\x0C\x04\x05\x41\x0F\x13\x5E\x45\x3B\x3F\x39\x29\x2A\x3A\x2E\x7D\x6C\x73\x00\x23\x39\x27\x31\x27\x3E\x22\x2D\x3A\x6E\x71\x6C\x03\x27\x37"
"\x37\x2E\x34\x2C\x64\x35\x08\x14\x1E\x10\x12\x1A\x52\x77\x78\x20\x38\x30\x24\x3E\x3A\x32\x4A\x2D\x21\x25\x2B\x3C\x66\x40\x42\x22\x04\x17\x07\x09\x07\x00\x3E\x7B\x2B\x31\x3F\x2D\x35\x33\x35\x73\x23\x34\x22\x23\x3D\x3B\x2D\x38\x68\x61\x1D\x2A\x38\x39\x2B\x2D\x27\x32\x66\x79\x64\x0B\x1F\x0F\x0F\x16\x0C"
"\x14\x5C\x43\x52\x32\x14\x07\x17\x19\x17\x10\x0E\x4B\x06\x0C\x1A\x18\x03\x1F\x09\x69\x40\x41\x15\x02\x10\x11\x33\x35\x3F\x2A\x7E\x61\x7C\x1C\x36\x25\x31\x3F\x35\x32\x30\x75\x39\x23\x29\x3B\x27\x21\x2B\x6D\x31\x26\x34\x35\x2F\x29\x23\x36\x53\x5B\x1B\x16\x10\x0B\x0E\x12\x1E\x53\x14\x18\x05\x14\x1B\x03"
"\x0F\x19\x11\x49\x0F\x01\x08\x4D\x04\x0A\x0C\x04\x46\x14\x0C\x04\x28\x32\x36\x3E\x70\x55\x71\x7D\x01\x3B\x31\x23\x33\x77\x35\x75\x2C\x24\x24\x2D\x2B\x3D\x76\x6D\x30\x2A\x27\x29\x32\x6A\x27\x29\x13\x18\x13\x59\x40\x5F\x2C\x0F\x1D\x03\x15\x03\x02\x1E\x11\x06\x4A\x55\x48\x3A\x06\x0E\x1E\x04\x0C\x04\x40"
"\x5F\x46\x34\x0C\x04\x28\x3E\x76\x53\x73\x7F\x08\x35\x3B\x20\x70\x21\x24\x38\x33\x27\x2B\x26\x72\x69\x69\x2E\x28\x3B\x23\x2D\x23\x24\x22\x6A\x37\x2D\x1B\x09\x11\x17\x19\x58\x5C\x1C\x1C\x17\x50\x56\x05\x1F\x15\x07\x0F\x18\x45\x05\x07\x1C\x18\x4A\x4C\x69\x6A\x37\x36\x29\x6E\x48\x7A\x08\x3D\x2D\x2A\x36"
"\x32\x3A\x21\x73\x6E\x71\x18\x32\x20\x22\x25\x39\x23\x69\x68\x6F\x25\x23\x36\x26\x32\x2F\x23\x33\x64\x7B\x5A\x2D\x28\x37\x5E\x41\x5C\x3C\x16\x17\x50\x10\x56\x21\x24\x3B\x44\x4B\x25\x08\x00\x16\x4C\x1A\x0D\x11\x0B\x41\x30\x37\x2A\x16\x7A\x2E\x2B\x3C\x54\x7F\x7C\x3F\x27\x3A\x3C\x25\x7B\x3E\x3A\x75\x1D"
"\x22\x26\x2D\x21\x38\x3F\x6D\x14\x13\x0E\x61\x6E\x0E\x0F\x00\x0C\x49\x54\x59\x2D\x2C\x28\x2D\x5E\x53\x3C\x43\x22\x27\x58\x55\x3A\x3B\x3C\x39\x47\x4F\x03\x1F\x42\x34\x09\x13\x03\x20\x11\x04\x28\x3F\x77\x16\x2E\x3A\x32\x0B\x02\x1D\x70\x30\x26\x27\x27\x7B\x40\x41\x18\x1B\x01\x17\x15\x47\x6F\x63\x13\x24"
"\x32\x33\x2D\x2B\x1D\x08\x58\x47\x5E\x31\x19\x09\x05\x1C\x02\x1A\x56\x51\x54\x1C\x04\x1F\x0D\x1B\x00\x0A\x18\x4D\x5C\x43\x30\x13\x09\x1F\x1D\x4B\x7A\x1A\x2D\x2D\x31\x72\x38\x38\x26\x36\x33\x25\x76\x38\x26\x75\x27\x2A\x26\x3C\x2F\x23\x6C\x3D\x30\x2C\x38\x38\x7D\x4D\x64\x65\x2A\x3A\x3B\x59\x18\x16\x10"
"\x18\x52\x00\x13\x03\x1F\x07\x00\x06\x4A\x08\x09\x07\x4E\x0D\x09\x4D\x01\x0C\x0E\x07\x0F\x00\x11\x17\x3F\x3F\x78\x2D\x36\x3A\x2E\x38\x72\x27\x3F\x3E\x78\x77\x00\x3D\x23\x38\x68\x39\x3C\x20\x2B\x3F\x23\x2E\x7A\x61\x61\x37\x36\x2A\x02\x02\x5F\x57\x74\x75\x3A\x34\x20\x36\x27\x30\x3A\x3B\x7E\x58\x4A\x3C"
"\x01\x07\x0A\x00\x1B\x1E\x42\x27\x05\x07\x03\x09\x00\x00\x28\x7B\x1E\x30\x2C\x3A\x2B\x3C\x3E\x3F\x70\x37\x3F\x3B\x20\x30\x38\x38\x68\x20\x20\x2C\x23\x20\x2B\x2D\x27\x61\x32\x35\x25\x23\x1C\x12\x1B\x59\x0E\x1A\x0E\x5D\x13\x03\x00\x5E\x06\x18\x06\x01\x44\x61\x45\x49\x2F\x03\x00\x02\x15\x43\x01\x41\x16"
"\x15\x0B\x02\x28\x3A\x35\x63\x7E\x0C\x39\x29\x26\x3A\x3E\x36\x25\x77\x6A\x75\x1D\x22\x26\x2D\x21\x38\x3F\x6D\x11\x26\x23\x34\x34\x2E\x30\x3C\x5A\x45\x58\x3F\x17\x0D\x19\x0A\x13\x1F\x1C\x51\x50\x57\x1A\x10\x1E\x1C\x07\x1B\x05\x65\x4C\x4D\x12\x11\x0F\x15\x03\x04\x10\x0C\x35\x35\x78\x67\x7E\x1E\x30\x31"
"\x3D\x24\x70\x30\x38\x77\x35\x25\x3A\x6B\x3C\x21\x3C\x20\x39\x2A\x2A\x63\x26\x28\x34\x22\x33\x24\x16\x17\x56\x73\x53\x5F\x28\x15\x1B\x00\x50\x01\x04\x18\x13\x07\x0B\x06\x48\x0A\x0F\x01\x4C\x0C\x06\x07\x4F\x13\x03\x0A\x0B\x13\x3F\x7B\x28\x36\x2C\x2B\x7C\x2F\x27\x3F\x35\x22\x76\x36\x3A\x31\x6A\x3F\x27"
"\x2E\x29\x23\x29\x6D\x36\x2B\x25\x61\x20\x2E\x36\x20\x0D\x1A\x14\x15\x50\x75\x76\x29\x37\x20\x24\x38\x38\x30\x54\x53\x4A\x2D\x21\x31\x27\x21\x2B\x67\x4F\x43\x10\x08\x08\x00\x44\x5D\x74\x63\x76\x61\x70\x67\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x03\x38\x68\x3D\x26\x2A\x6C\x24\x2C\x37\x25\x33\x28\x22"
"\x30\x65\x08\x1E\x19\x1A\x16\x1E\x1E\x11\x17\x4C\x7A\x5C\x56\x07\x1D\x1B\x0D\x4B\x1A\x06\x1B\x1B\x09\x1F\x42\x2A\x30\x41\x46\x47\x44\x45\x7A\x7B\x78\x10\x2D\x7F\x28\x35\x37\x73\x3C\x3E\x35\x36\x38\x75\x24\x2E\x3C\x3E\x21\x3D\x27\x6D\x0D\x08\x7F\x4B\x6B\x67\x2A\x36\x16\x14\x17\x12\x0B\x0F\x5C\x1A\x1D"
"\x1C\x17\x1D\x13\x59\x17\x1A\x07\x4B\x48\x49\x27\x1C\x4C\x29\x2C\x30\x40\x16\x09\x15\x0F\x0C\x34\x3C\x67\x53\x73\x7F\x28\x2F\x33\x30\x35\x23\x22\x77\x33\x3A\x25\x2C\x24\x2C\x60\x2C\x23\x20\x62\x63\x60\x61\x11\x2F\x21\x37\x1F\x5B\x1C\x16\x1B\x0C\x5C\x09\x1A\x16\x50\x01\x17\x03\x1C\x55\x08\x19\x0D\x08"
"\x05\x50\x66\x40\x42\x0D\x05\x15\x15\x13\x05\x11\x7A\x76\x39\x37\x31\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x02\x22\x2A\x3C\x69\x2D\x20\x22\x23\x27\x20\x34\x28\x29\x29\x37\x65\x1F\x03\x11\x0A\x0A\x40\x76\x50\x52\x1A\x00\x12\x19\x19\x12\x1C\x0D\x4B\x47\x0F\x02\x1A\x1F\x05\x06\x0D\x13\x41\x46\x47\x44"
"\x26\x36\x3E\x39\x2B\x7E\x2B\x34\x38\x72\x17\x1E\x02\x76\x34\x35\x36\x22\x2E\x68\x61\x3A\x27\x25\x3E\x62\x33\x32\x2E\x21\x35\x25\x28\x40\x5B\x1E\x15\x0B\x0C\x14\x50\x16\x1D\x03\x58\x58\x7D\x59\x55\x03\x1B\x0B\x06\x00\x09\x05\x0A\x42\x4C\x12\x04\x0A\x02\x05\x16\x3F\x7B\x7E\x7F\x7E\x36\x2C\x3E\x3D\x3D"
"\x36\x38\x31\x77\x7B\x27\x2F\x25\x2D\x3E\x6E\x6F\x6C\x0A\x27\x37\x60\x20\x66\x21\x36\x20\x09\x13\x58\x3D\x36\x3C\x2C\x5D\x13\x17\x14\x03\x13\x04\x07\x5B\x60\x46\x48\x3E\x07\x01\x08\x02\x15\x10\x40\x0F\x03\x13\x13\x0A\x28\x30\x78\x2B\x3B\x2C\x39\x29\x68\x73\x03\x34\x22\x23\x3D\x3B\x2D\x38\x68\x77\x6E"
"\x01\x29\x39\x35\x2C\x32\x2A\x66\x79\x64\x04\x1E\x0D\x19\x17\x1D\x1A\x18\x5D\x4C\x53\x3E\x14\x02\x00\x1B\x07\x01\x4B\x1A\x0C\x1D\x0A\x18\x43\x68\x4E\x40\x35\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E\x31\x67\x72\x74\x3E\x34\x22\x20\x3B\x27\x21\x66\x3A\x2C\x3D\x2A\x38\x6A\x6E\x63\x67\x33\x23\x34\x21\x31"
"\x57\x0C\x11\x17\x0D\x10\x1F\x16\x55\x5F\x50\x56\x15\x1F\x11\x16\x01\x46\x18\x06\x1C\x1B\x4B\x41\x68\x43\x40\x46\x0E\x13\x10\x15\x77\x38\x30\x3C\x3D\x34\x7B\x71\x72\x74\x25\x23\x3A\x7A\x30\x3A\x3D\x25\x24\x26\x2F\x2B\x6B\x63\x48\x49\x13\x11\x03\x02\x00\x65\x39\x33\x3D\x3A\x35\x75\x51\x5D\x26\x16\x03"
"\x05\x56\x00\x1D\x01\x02\x4B\x09\x49\x1D\x1F\x09\x08\x06\x43\x14\x04\x15\x13\x44\x12\x3F\x39\x2B\x30\x2A\x3A\x72\x7D\x01\x3F\x3F\x26\x69\x77\x17\x3D\x2F\x28\x23\x69\x19\x26\x61\x0B\x2B\x63\x33\x28\x21\x29\x25\x29\x56\x5B\x17\x0D\x16\x1A\x0E\x5D\x16\x16\x06\x18\x15\x12\x07\x7F\x4A\x4B\x07\x07\x4E\x1B"
"\x04\x08\x42\x0D\x05\x15\x11\x08\x16\x0E\x76\x7B\x3A\x38\x3D\x34\x3B\x2F\x3D\x26\x3E\x35\x76\x33\x3B\x22\x24\x27\x27\x28\x2A\x3C\x60\x6D\x23\x2D\x24\x61\x32\x2F\x21\x65\x08\x14\x0D\x0D\x1B\x0D\x5C\x55\x00\x16\x03\x05\x17\x05\x00\x55\x03\x1F\x41\x47"
);

MAN(wifi, "Wi-Fi Guide",
"\x19\x14\x16\x17\x1B\x1C\x08\x14\x1C\x14\x5A\x7C\x76\x00\x3D\x3B\x61\x0A\x68\x61\x1F\x3A\x25\x2E\x29\x63\x13\x24\x32\x33\x2D\x2B\x1D\x08\x51\x59\x40\x5F\x2B\x14\x5F\x35\x19\x51\x1F\x14\x1B\x1B\x4A\x55\x48\x0A\x06\x00\x03\x1E\x07\x43\x01\x41\x08\x02\x10\x12\x35\x29\x33\x79\x60\x7F\x1F\x32\x3C\x3D\x35"
"\x32\x22\x79\x5E\x78\x6A\x0D\x21\x3B\x3D\x3B\x6C\x39\x2B\x2E\x25\x7B\x66\x22\x2A\x31\x1F\x09\x58\x0D\x16\x1A\x5C\x0D\x13\x00\x03\x06\x19\x05\x10\x55\x42\x3C\x38\x28\x5C\x40\x3B\x3D\x23\x50\x49\x4F\x46\x24\x0C\x00\x39\x30\x78\x7E\x1D\x30\x32\x33\x37\x30\x24\x71\x37\x22\x20\x3A\x27\x2A\x3C\x20\x2D\x2E"
"\x20\x21\x3B\x64\x6E\x4B\x6B\x67\x01\x2B\x0E\x1E\x0A\x09\x0C\x16\x0F\x18\x52\x1D\x15\x05\x01\x18\x06\x1E\x19\x4B\x40\x1E\x01\x1D\x07\x42\x11\x00\x08\x0E\x09\x0B\x4D\x45\x37\x3A\x21\x79\x3F\x2C\x37\x7D\x34\x3C\x22\x71\x37\x77\x21\x26\x2F\x39\x26\x28\x23\x2A\x63\x3D\x23\x30\x33\x36\x29\x35\x20\x65\x15"
"\x09\x58\x18\x74\x5F\x5C\x1E\x17\x01\x04\x18\x10\x1E\x17\x14\x1E\x0E\x48\x44\x4E\x0C\x04\x02\x0D\x10\x05\x41\x12\x0F\x01\x45\x2A\x29\x37\x3F\x37\x33\x39\x7D\x2B\x3C\x25\x23\x76\x1E\x00\x75\x3A\x39\x27\x3F\x27\x2B\x29\x3E\x6C\x49\x4A\x07\x09\x15\x03\x00\x2E\x2F\x31\x37\x39\x5F\x3D\x5D\x3C\x36\x24\x26"
"\x39\x25\x3F\x7F\x39\x0E\x1C\x1D\x07\x01\x0B\x1E\x42\x5D\x40\x2F\x03\x13\x13\x0A\x28\x30\x78\x7F\x7E\x36\x32\x29\x37\x21\x3E\x34\x22\x77\x6A\x75\x1D\x22\x65\x0F\x27\x6F\x72\x6D\x0F\x22\x2E\x20\x21\x22\x64\x2E\x14\x14\x0F\x17\x5E\x11\x19\x09\x05\x1C\x02\x1A\x05\x57\x4A\x55\x19\x0E\x04\x0C\x0D\x1B\x4C"
"\x53\x68\x25\x0F\x13\x01\x02\x10\x4B\x7A\x09\x3D\x3A\x31\x31\x32\x38\x31\x27\x70\x3D\x37\x23\x31\x27\x6A\x3C\x21\x3D\x26\x6F\x2D\x6D\x24\x31\x25\x32\x2E\x67\x34\x24\x09\x08\x0F\x16\x0C\x1B\x52\x77\x78\x24\x39\x23\x33\x3B\x31\x26\x39\x4B\x3B\x2C\x2D\x3A\x3E\x24\x36\x3A\x40\x35\x3F\x37\x21\x36\x50\x76"
"\x78\x0E\x1B\x0F\x66\x7D\x3D\x3F\x34\x71\x37\x39\x30\x75\x23\x25\x3B\x2C\x2D\x3A\x3E\x28\x62\x6E\x60\x20\x30\x28\x2D\x21\x54\x71\x55\x59\x29\x2F\x3D\x5D\x5D\x53\x27\x21\x37\x45\x4E\x55\x19\x0E\x0B\x1C\x1C\x0A\x42\x4D\x35\x33\x21\x53\x4B\x37\x37\x2E\x7A\x73\x19\x1C\x0D\x76\x7C\x34\x21\x73\x24\x39\x33"
"\x77\x39\x3C\x24\x22\x25\x3C\x23\x6F\x35\x22\x37\x63\x33\x29\x29\x32\x28\x21\x5A\x0E\x0B\x1C\x50\x75\x51\x5D\x25\x23\x31\x42\x4C\x57\x1A\x10\x1D\x0E\x1B\x1D\x4E\x1C\x18\x0C\x0C\x07\x01\x13\x02\x47\x4C\x32\x0A\x1A\x6B\x74\x0E\x3A\x2E\x2E\x3D\x3D\x31\x3D\x76\x38\x3A\x75\x27\x24\x2C\x2C\x3C\x21\x6C\x3F"
"\x2D\x36\x34\x24\x34\x34\x6D\x6B\x70\x56\x58\x36\x0E\x1A\x12\x5D\x1C\x16\x04\x06\x19\x05\x1F\x06\x50\x4B\x06\x06\x4E\x1F\x0D\x1E\x11\x14\x0F\x13\x02\x47\x49\x45\x3E\x3A\x2C\x38\x7E\x3C\x3D\x33\x72\x31\x35\x71\x24\x32\x35\x31\x71\x6B\x3D\x3A\x2B\x6F\x04\x19\x16\x13\x13\x6E\x10\x17\x0A\x6B\x70\x56\x58"
"\x20\x11\x0A\x0E\x5D\x00\x1C\x05\x05\x13\x05\x54\x06\x0F\x1F\x1B\x49\x1A\x07\x05\x1E\x59\x43\x0C\x0E\x01\x47\x0D\x0B\x2E\x34\x78\x2D\x36\x3A\x7C\x2F\x3D\x26\x24\x34\x24\x77\x20\x3A\x6A\x28\x20\x28\x20\x28\x29\x6D\x2B\x37\x6E\x4B\x4C\x0F\x0B\x11\x29\x2B\x37\x2D\x5E\x59\x5C\x29\x37\x27\x38\x34\x24\x3E"
"\x3A\x32\x60\x46\x48\x24\x01\x0D\x05\x01\x07\x43\x08\x0E\x12\x14\x14\x0A\x2E\x61\x78\x0A\x3B\x2B\x28\x34\x3C\x34\x23\x71\x68\x77\x1A\x30\x3E\x3C\x27\x3B\x25\x6F\x6A\x6D\x2B\x2D\x34\x24\x34\x29\x21\x31\x5A\x45\x58\x34\x11\x1D\x15\x11\x17\x53\x18\x1E\x02\x04\x04\x1A\x1E\x4B\x45\x49\x1D\x07\x0D\x1F\x07"
"\x69\x40\x41\x1F\x08\x11\x17\x7A\x38\x37\x37\x30\x3A\x3F\x29\x3B\x3C\x3E\x71\x7E\x00\x3D\x78\x0C\x22\x68\x26\x3C\x6F\x19\x1E\x00\x6A\x60\x36\x2F\x33\x2C\x65\x15\x0F\x10\x1C\x0C\x5F\x18\x18\x04\x1A\x13\x14\x05\x59\x54\x21\x02\x02\x1B\x49\x1E\x1D\x03\x0A\x10\x02\x0D\x5B\x6C\x47\x44\x42\x37\x34\x3A\x30"
"\x32\x3A\x71\x35\x3D\x27\x23\x21\x39\x23\x73\x7B\x40\x66\x68\x19\x26\x20\x22\x28\x62\x37\x25\x35\x2E\x22\x36\x2C\x14\x1C\x42\x59\x1B\x11\x1D\x1F\x1E\x16\x50\x19\x19\x03\x07\x05\x05\x1F\x48\x06\x00\x4F\x18\x05\x07\x43\x10\x09\x09\x09\x01\x45\x3B\x35\x3C\x79\x34\x30\x35\x33\x72\x3A\x24\x71\x30\x25\x3B"
"\x38\x6A\x1C\x21\x27\x2A\x20\x3B\x3E\x6C\x49\x4A\x00\x0F\x15\x14\x09\x3B\x35\x3D\x59\x33\x30\x38\x38\x78\x24\x19\x1F\x5D\x36\x54\x4B\x4A\x2A\x01\x1B\x1E\x03\x0D\x03\x07\x43\x0D\x0E\x02\x02\x44\x48\x7A\x2F\x2D\x2B\x30\x2C\x7C\x32\x34\x35\x70\x06\x3F\x7A\x12\x3C\x66\x6B\x0A\x25\x3B\x2A\x38\x22\x2D\x37"
"\x28\x61\x27\x29\x20\x65\x19\x1E\x14\x15\x0B\x13\x1D\x0F\x5C\x79\x24\x1E\x11\x10\x18\x1C\x04\x0C\x48\x00\x1A\x4F\x03\x0B\x04\x4C\x0F\x0F\x46\x04\x05\x0B\x7A\x3D\x31\x21\x7E\x2C\x28\x28\x31\x38\x70\x23\x37\x33\x3D\x3A\x39\x65\x68\x1D\x26\x26\x3F\x6D\x32\x31\x2F\x26\x34\x26\x29\x7F\x5A\x5C\x19\x10\x0C"
"\x0F\x10\x1C\x1C\x16\x5D\x1C\x19\x13\x11\x52\x44\x61\x62\x28\x2A\x39\x2D\x23\x21\x26\x24\x41\x27\x23\x25\x35\x0E\x1E\x0A\x79\x0D\x1A\x08\x09\x1B\x1D\x17\x02\x5C\x16\x30\x34\x3A\x3F\x2D\x3B\x6E\x71\x6C\x1D\x30\x2C\x30\x24\x34\x33\x2D\x20\x09\x5B\x46\x59\x3D\x10\x12\x1B\x1B\x14\x05\x03\x13\x57\x4A\x55"
"\x2B\x0F\x1E\x08\x00\x0C\x09\x09\x42\x17\x01\x03\x5C\x6D\x49\x45\x0A\x29\x3D\x3F\x3B\x2D\x2E\x38\x36\x73\x32\x30\x38\x33\x74\x7D\x78\x65\x7C\x66\x7B\x60\x7A\x6D\x05\x0B\x3A\x68\x6A\x67\x27\x2D\x1B\x15\x16\x1C\x12\x5F\x0B\x14\x16\x07\x18\x5D\x56\x07\x1B\x02\x0F\x19\x48\x1A\x0F\x19\x05\x03\x05\x43\x0D"
"\x0E\x02\x02\x48\x6F\x7A\x7B\x2A\x36\x3F\x32\x35\x33\x35\x73\x31\x36\x31\x25\x31\x26\x39\x22\x3E\x2C\x20\x2A\x3F\x3E\x6C\x49\x6D\x61\x73\x67\x03\x0D\x00\x5B\x19\x17\x1A\x5F\x4A\x5D\x35\x3B\x0A\x51\x17\x05\x11\x55\x0C\x0A\x1B\x1D\x0B\x1D\x4C\x0F\x17\x17\x40\x12\x0E\x08\x16\x11\x3F\x29\x78\x2B\x3F\x31"
"\x3B\x38\x69\x73\x62\x7F\x62\x77\x13\x1D\x30\x6B\x2F\x26\x2B\x3C\x6C\x2B\x37\x31\x34\x29\x23\x35\x6A\x4F\x70\x38\x37\x34\x33\x3E\x32\x39\x52\x3F\x39\x3F\x33\x57\x5C\x1B\x0F\x1F\x1B\x01\x4E\x18\x00\x0C\x0C\x4A\x6A\x4C\x46\x09\x01\x11\x29\x33\x78\x2E\x32\x3E\x32\x7D\x21\x3B\x3F\x26\x76\x27\x26\x3A\x2C"
"\x22\x24\x2C\x3D\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x3E\x10\x11\x52\x00\x11\x07\x13\x13\x54\x1B\x0F\x1F\x1F\x06\x1C\x04\x1F\x67\x4F\x43\x0E\x04\x12\x14\x0C\x45\x2D\x37\x39\x37\x7E\x2C\x34\x32\x25\x73\x20\x23\x39\x31\x3D\x39\x2F\x38\x68\x75\x20\x2E\x21\x28\x7C\x63\x2B\x24"
"\x3F\x7A\x27\x29\x1F\x1A\x0A\x59\x5E\x5F\x2F\x15\x1D\x04\x50\x59\x17\x19\x10\x55\x18\x0E\x1E\x0C\x0F\x03\x45\x4D\x16\x0B\x05\x41\x0D\x02\x1D\x6F\x77\x7B\x36\x3C\x2A\x2C\x34\x7D\x25\x3F\x31\x3F\x76\x24\x3C\x3A\x3D\x6B\x21\x27\x3A\x2A\x3E\x2B\x23\x20\x25\x32\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C"
"\x5D\x52\x53\x33\x04\x04\x05\x11\x1B\x1E\x4B\x0B\x06\x00\x01\x09\x0E\x16\x0A\x0F\x0F\x46\x0E\x0A\x03\x35\x51\x75\x79\x30\x3A\x28\x2E\x3A\x73\x27\x3D\x37\x39\x74\x36\x25\x25\x26\x2C\x2D\x3B\x6C\x23\x23\x2E\x25\x7C\x7A\x37\x36\x2A\x1C\x12\x14\x1C\x40\x5F\x5C\x5D\x52\x53\x50\x51\x35\x18\x1A\x1B\x0F\x08"
"\x1C\x49\x1A\x00\x4C\x0C\x42\x10\x01\x17\x03\x03\x44\x15\x28\x34\x3E\x30\x32\x3A\x56\x70\x72\x07\x38\x38\x25\x77\x24\x27\x25\x2C\x3A\x28\x23\x75\x6C\x6A\x35\x2A\x26\x28\x6B\x37\x36\x2A\x1C\x12\x14\x1C\x0D\x58\x50\x5D\x55\x04\x19\x17\x1F\x5A\x04\x14\x19\x18\x1F\x06\x1C\x0B\x4B\x41\x42\x44\x17\x08\x00"
"\x0E\x49\x06\x35\x35\x36\x3C\x3D\x2B\x7B\x73\x58\x59\x07\x18\x10\x1E\x74\x1B\x05\x1F\x68\x1E\x01\x1D\x07\x04\x0C\x04\x7F\x61\x05\x0F\x01\x06\x31\x5B\x34\x30\x2D\x2B\x76\x4C\x5B\x53\x39\x02\x56\x16\x1D\x07\x1A\x07\x09\x07\x0B\x4F\x01\x02\x06\x06\x40\x0E\x00\x01\x5B\x6F\x68\x72\x78\x1F\x31\x2D\x3B\x38"
"\x26\x73\x24\x39\x33\x77\x3A\x30\x3E\x3C\x27\x3B\x25\x6F\x2D\x23\x26\x63\x32\x24\x25\x28\x2A\x2B\x1F\x18\x0C\x59\x09\x16\x08\x15\x52\x07\x18\x14\x56\x07\x15\x06\x19\x1C\x07\x1B\x0A\x41\x66\x5E\x4B\x43\x32\x04\x15\x13\x05\x17\x2E\x7B\x2C\x31\x3B\x7F\x2E\x32\x27\x27\x35\x23\x76\x16\x1A\x11\x6A\x3F\x20"
"\x2C\x6E\x1F\x0F\x63\x48\x77\x69\x61\x13\x37\x20\x24\x0E\x1E\x58\x0D\x16\x1A\x5C\x2A\x1B\x5E\x36\x18\x56\x13\x06\x1C\x1C\x0E\x1A\x49\x46\x2B\x09\x1B\x0B\x00\x05\x41\x2B\x06\x0A\x04\x3D\x3E\x2A\x79\x60\x7F\x12\x38\x26\x24\x3F\x23\x3D\x77\x35\x31\x2B\x3B\x3C\x2C\x3C\x3C\x65\x63\x48\x76\x69\x61\x14\x32"
"\x2A\x65\x0E\x13\x1D\x59\x10\x1A\x08\x0A\x1D\x01\x1B\x51\x02\x05\x1B\x00\x08\x07\x0D\x1A\x06\x00\x03\x19\x07\x11\x40\x49\x35\x02\x10\x11\x33\x35\x3F\x2A\x7E\x61\x7C\x0E\x2B\x20\x24\x34\x3B\x77\x6A\x75\x1E\x39\x27\x3C\x2C\x23\x29\x3E\x2A\x2C\x2F\x35\x6F\x69\x4E\x73\x53\x5B\x2A\x1C\x0D\x1A\x08\x5D\x06"
"\x1B\x15\x51\x18\x12\x00\x02\x05\x19\x03\x49\x1D\x1B\x0D\x0E\x09\x43\x48\x15\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E\x31\x67\x72\x3D\x35\x25\x21\x38\x26\x3E\x67\x39\x2D\x3A\x2B\x3B\x6C\x62\x62\x31\x25\x32\x23\x33\x69\x32\x13\x15\x0B\x16\x1D\x14\x55\x53\x78\x44\x59\x51\x35\x1F\x11\x16\x01\x4B\x01\x0F"
"\x4E\x00\x18\x05\x07\x11\x40\x05\x03\x11\x0D\x06\x3F\x28\x78\x3A\x31\x31\x32\x38\x31\x27\x70\x7C\x76\x3E\x32\x75\x24\x24\x3C\x65\x6E\x3B\x24\x28\x62\x31\x2F\x34\x32\x22\x36\x6A\x33\x28\x28\x59\x17\x0C\x5C\x09\x1A\x16\x50\x18\x05\x04\x01\x10\x44"
);

MAN(security, "Security Guide",
"\x0E\x13\x1D\x79\x1A\x1A\x1A\x18\x1C\x17\x15\x03\x76\x04\x01\x1C\x1E\x0E\x42\x1E\x27\x21\x28\x22\x35\x30\x60\x70\x76\x68\x75\x74\x5A\x12\x16\x1A\x12\x0A\x18\x18\x01\x53\x3D\x18\x15\x05\x1B\x06\x05\x0D\x1C\x49\x2A\x0A\x0A\x08\x0C\x07\x05\x13\x5C\x47\x05\x0B\x2E\x32\x2E\x30\x2C\x2A\x2F\x71\x72\x35\x39"
"\x23\x33\x20\x35\x39\x26\x67\x68\x28\x2D\x2C\x23\x38\x2C\x37\x4A\x31\x34\x28\x30\x20\x19\x0F\x11\x16\x10\x53\x5C\x1C\x02\x03\x50\x57\x56\x15\x06\x1A\x1D\x18\x0D\x1B\x4E\x0C\x03\x03\x16\x11\x0F\x0D\x4A\x47\x00\x00\x2C\x32\x3B\x3C\x7E\x2C\x39\x3E\x27\x21\x39\x25\x2F\x7B\x74\x25\x2F\x39\x2E\x26\x3C\x22"
"\x2D\x23\x21\x26\x60\x67\x66\x2F\x21\x24\x16\x0F\x10\x57\x74\x30\x0C\x18\x1C\x53\x19\x05\x56\x00\x1D\x01\x02\x4B\x1C\x01\x07\x1C\x4C\x1D\x10\x0C\x07\x13\x07\x0A\x43\x16\x7A\x7C\x2F\x30\x30\x3B\x33\x2A\x21\x7E\x23\x34\x35\x22\x26\x3C\x3E\x32\x6F\x69\x2D\x20\x21\x20\x23\x2D\x24\x61\x29\x35\x64\x16\x1F"
"\x0F\x0C\x10\x10\x18\x0F\x5D\x4C\x79\x20\x03\x1F\x01\x15\x16\x13\x4B\x4E\x49\x1D\x0A\x0F\x18\x10\x0A\x14\x18\x46\x59\x44\x32\x33\x35\x3C\x36\x29\x2C\x7C\x0E\x37\x30\x25\x23\x3F\x23\x2D\x7B\x40\x41\x11\x06\x1B\x1D\x6C\x0C\x01\x00\x0F\x14\x08\x13\x64\x0C\x29\x5B\x2C\x31\x3B\x5F\x3A\x34\x20\x20\x24\x51"
"\x3A\x3E\x3A\x30\x4A\x24\x2E\x49\x2A\x2A\x2A\x28\x2C\x30\x25\x6B\x4B\x47\x31\x16\x3F\x7B\x39\x79\x2D\x2B\x2E\x32\x3C\x34\x70\x21\x37\x24\x27\x22\x25\x39\x2C\x69\x21\x3D\x6C\x65\x20\x26\x34\x35\x23\x35\x6D\x65\x2D\x12\x16\x1D\x11\x08\x0F\x5D\x3A\x16\x1C\x1D\x19\x4D\x54\x25\x23\x25\x44\x49\x08\x0E\x0F"
"\x08\x42\x0C\x12\x6B\x46\x47\x02\x0C\x34\x3C\x3D\x2B\x2E\x2D\x35\x33\x26\x73\x7D\x71\x05\x32\x20\x21\x23\x25\x2F\x3A\x6E\x71\x6C\x0C\x21\x20\x2F\x34\x28\x33\x37\x65\x44\x5B\x2B\x10\x19\x11\x51\x14\x1C\x53\x1F\x01\x02\x1E\x1B\x1B\x19\x45\x62\x44\x4E\x2E\x4C\x3D\x2B\x2D\x40\x08\x15\x47\x08\x0A\x39\x3A"
"\x34\x79\x2A\x30\x7C\x24\x3D\x26\x22\x71\x06\x14\x74\x34\x24\x2F\x68\x20\x3D\x6F\x3F\x39\x2D\x31\x25\x25\x66\x26\x37\x65\x1B\x5B\x10\x18\x0D\x17\x5C\x50\x52\x06\x03\x14\x56\x16\x54\x38\x03\x08\x1A\x06\x1D\x00\x0A\x19\x68\x43\x40\x00\x05\x04\x0B\x10\x34\x2F\x78\x2D\x31\x7F\x37\x38\x37\x23\x70\x23\x33"
"\x34\x3B\x23\x2F\x39\x31\x69\x21\x3F\x38\x24\x2D\x2D\x33\x6F\x4C\x6A\x64\x00\x14\x1A\x1A\x15\x1B\x5F\x18\x04\x1C\x12\x1D\x18\x15\x57\x18\x1A\x09\x00\x52\x49\x1D\x06\x0B\x03\x42\x0A\x0E\x41\x11\x0E\x10\x0D\x7A\x22\x37\x2C\x2C\x7F\x2C\x35\x3D\x3D\x35\x71\x38\x32\x35\x27\x28\x32\x68\x61\x1D\x2A\x38\x39"
"\x2B\x2D\x27\x32\x66\x79\x64\x04\x19\x18\x17\x0C\x10\x0B\x0F\x77\x52\x53\x4E\x51\x25\x1E\x13\x1B\x47\x02\x06\x49\x01\x1F\x18\x04\x0D\x0D\x13\x41\x58\x47\x20\x1C\x34\x3A\x35\x30\x3D\x7F\x30\x32\x31\x38\x79\x7F\x5C\x7A\x74\x72\x0C\x22\x26\x2D\x6E\x22\x35\x6D\x26\x26\x36\x28\x25\x22\x63\x65\x19\x1A\x16"
"\x59\x12\x10\x1F\x1C\x06\x16\x50\x10\x56\x04\x00\x1A\x06\x0E\x06\x49\x02\x0E\x1C\x19\x0D\x13\x40\x49\x35\x02\x10\x11\x33\x35\x3F\x2A\x7E\x61\x7C\x0D\x20\x3A\x26\x30\x35\x2E\x74\x73\x40\x6B\x68\x3A\x2B\x2C\x39\x3F\x2B\x37\x39\x61\x78\x67\x02\x2C\x14\x1F\x58\x14\x07\x5F\x18\x18\x04\x1A\x13\x14\x5F\x57"
"\x59\x55\x18\x0E\x19\x1C\x07\x1D\x09\x1E\x42\x02\x40\x2C\x0F\x04\x16\x0A\x29\x34\x3E\x2D\x7E\x3E\x3F\x3E\x3D\x26\x3E\x25\x78\x5D\x5E\x06\x07\x0A\x1A\x1D\x0B\x1D\x1F\x0E\x10\x06\x05\x0F\x66\x61\x64\x10\x3B\x38\x72\x54\x5E\x2C\x11\x1C\x00\x07\x23\x12\x04\x12\x11\x1B\x4A\x43\x29\x19\x1E\x4F\x4A\x4D\x00"
"\x11\x0F\x16\x15\x02\x16\x45\x39\x34\x36\x2D\x2C\x30\x30\x74\x72\x31\x3C\x3E\x35\x3C\x27\x75\x3F\x25\x23\x27\x21\x38\x22\x6D\x26\x2C\x37\x2F\x2A\x28\x25\x21\x1F\x1F\x58\x18\x0E\x0F\x0F\x46\x78\x53\x50\x1A\x13\x12\x04\x55\x03\x1F\x48\x06\x00\x41\x66\x40\x42\x36\x13\x04\x14\x47\x25\x06\x39\x34\x2D\x37"
"\x2A\x7F\x1F\x32\x3C\x27\x22\x3E\x3A\x77\x7C\x00\x0B\x08\x61\x69\x2F\x3C\x27\x3E\x62\x21\x25\x27\x29\x35\x21\x65\x0A\x09\x17\x1E\x0C\x1E\x11\x0E\x52\x10\x18\x10\x18\x10\x11\x55\x19\x12\x1B\x1D\x0B\x02\x4C\x1E\x07\x17\x14\x08\x08\x00\x17\x4B\x50\x7B\x78\x12\x3B\x3A\x2C\x7D\x26\x3B\x35\x71\x32\x32\x32"
"\x34\x3F\x27\x3C\x69\x22\x2A\x3A\x28\x2E\x63\x6D\x61\x28\x22\x32\x20\x08\x5B\x0B\x1C\x0A\x5F\x15\x09\x52\x07\x1F\x51\x51\x39\x11\x03\x0F\x19\x48\x07\x01\x1B\x05\x0B\x1B\x44\x4E\x6B\x6C\x35\x25\x2B\x09\x14\x15\x0E\x1F\x0D\x19\x7D\x02\x01\x1F\x05\x13\x14\x00\x1C\x05\x05\x42\x64\x6E\x0C\x23\x23\x36\x31"
"\x2F\x2D\x2A\x22\x20\x65\x1C\x14\x14\x1D\x1B\x0D\x5C\x1C\x11\x10\x15\x02\x05\x57\x5C\x22\x03\x05\x0C\x06\x19\x1C\x4C\x3E\x07\x00\x15\x13\x0F\x13\x1D\x45\x64\x7B\x0E\x30\x2C\x2A\x2F\x7D\x74\x73\x24\x39\x24\x32\x35\x21\x6A\x3B\x3A\x26\x3A\x2A\x2F\x39\x2B\x2C\x2E\x4B\x66\x67\x7A\x65\x28\x1A\x16\x0A\x11"
"\x12\x0B\x1C\x00\x16\x50\x01\x04\x18\x00\x10\x09\x1F\x01\x06\x00\x46\x4C\x0F\x0E\x0C\x03\x0A\x15\x47\x11\x0B\x31\x35\x37\x2E\x30\x7F\x3D\x2D\x22\x20\x70\x37\x24\x38\x39\x75\x29\x23\x29\x27\x29\x26\x22\x2A\x62\x3A\x2F\x34\x34\x4D\x64\x65\x3E\x14\x1B\x0C\x13\x1A\x12\x09\x01\x5C\x20\x18\x15\x03\x01\x07"
"\x0F\x18\x48\x0F\x01\x03\x08\x08\x10\x10\x4E\x41\x32\x12\x16\x0B\x7A\x32\x2C\x79\x31\x31\x67\x7D\x33\x37\x34\x71\x33\x2F\x37\x30\x3A\x3F\x21\x26\x20\x3C\x6C\x2B\x2D\x31\x60\x35\x34\x32\x37\x31\x1F\x1F\x58\x18\x0E\x0F\x0F\x53\x78\x5E\x50\x33\x17\x14\x1F\x00\x1A\x18\x48\x42\x4E\x20\x02\x08\x26\x11\x09"
"\x17\x03\x47\x4F\x45\x39\x34\x36\x2D\x2C\x30\x30\x31\x37\x37\x70\x37\x39\x3B\x30\x30\x38\x6B\x29\x2A\x2D\x2A\x3F\x3E\x62\x2A\x33\x61\x27\x67\x37\x31\x08\x14\x16\x1E\x5E\x1C\x13\x10\x10\x1C\x5E\x7B\x7C\x31\x3D\x27\x2F\x3C\x29\x25\x22\x65\x41\x4D\x29\x06\x05\x11\x46\x13\x0C\x00\x7A\x3D\x31\x2B\x3B\x28"
"\x3D\x31\x3E\x73\x3F\x3F\x76\x31\x3B\x27\x6A\x2A\x24\x25\x6E\x3B\x24\x3F\x27\x26\x60\x31\x34\x28\x22\x2C\x16\x1E\x0B\x59\x56\x3B\x13\x10\x13\x1A\x1E\x5D\x56\x27\x06\x1C\x1C\x0A\x1C\x0C\x42\x4F\x3C\x18\x00\x0F\x09\x02\x4F\x49\x6E\x48\x7A\x0B\x2A\x3C\x38\x3A\x2E\x7D\x75\x12\x3C\x3D\x39\x20\x74\x34\x24"
"\x6B\x29\x39\x3E\x68\x6C\x22\x34\x26\x32\x61\x29\x37\x21\x2B\x13\x15\x1F\x59\x0C\x1E\x0B\x5D\x02\x1C\x02\x05\x05\x59\x7E\x58\x4A\x3F\x00\x00\x1D\x4F\x1C\x1F\x0D\x04\x12\x00\x0B\x5D\x44\x42\x3C\x32\x2A\x3C\x29\x3E\x30\x31\x75\x73\x23\x34\x22\x23\x3D\x3B\x2D\x38\x68\x28\x20\x2B\x6C\x3D\x2D\x31\x34\x61"
"\x34\x32\x28\x20\x09\x55\x72\x73\x2E\x2D\x35\x2B\x33\x30\x29\x51\x25\x32\x20\x21\x23\x25\x2F\x3A\x64\x42\x4C\x3E\x07\x17\x14\x08\x08\x00\x17\x45\x64\x7B\x08\x2B\x37\x29\x3D\x3E\x2B\x73\x76\x71\x25\x32\x37\x20\x38\x22\x3C\x30\x74\x6F\x3E\x28\x34\x2A\x25\x36\x66\x24\x25\x28\x1F\x09\x19\x55\x5E\x12\x15"
"\x1E\x00\x1C\x00\x19\x19\x19\x11\x59\x4A\x07\x07\x0A\x0F\x1B\x05\x02\x0C\x4F\x6A\x41\x46\x09\x0B\x11\x33\x3D\x31\x3A\x3F\x2B\x35\x32\x3C\x20\x70\x30\x38\x33\x74\x33\x23\x27\x2D\x69\x3E\x2A\x3E\x20\x2B\x30\x33\x28\x29\x29\x37\x65\x0A\x1E\x0A\x59\x1F\x0F\x0C\x53\x78\x5E\x50\x30\x12\x01\x11\x07\x1E\x02"
"\x1B\x00\x00\x08\x4C\x24\x26\x4F\x40\x00\x05\x13\x0D\x13\x33\x2F\x21\x79\x36\x36\x2F\x29\x3D\x21\x29\x71\x37\x39\x30\x75\x2E\x22\x29\x2E\x20\x20\x3F\x39\x2B\x20\x60\x25\x27\x33\x25\x65\x1B\x09\x1D\x59\x1D\x10\x12\x1B\x1B\x14\x05\x03\x17\x15\x18\x10\x60\x4B\x48\x41\x1A\x07\x05\x1E\x42\x13\x12\x0E\x01"
"\x15\x05\x08\x7A\x33\x39\x2A\x7E\x2B\x2B\x38\x33\x38\x23\x71\x30\x38\x26\x75\x39\x2E\x3E\x2C\x3C\x2E\x20\x6D\x2D\x25\x60\x35\x2E\x22\x29\x6C\x54\x71\x72\x2A\x3B\x3C\x29\x2F\x3B\x27\x29\x51\x3D\x32\x2D\x26\x60\x46\x48\x3A\x0B\x0C\x19\x1F\x0B\x17\x19\x41\x0D\x02\x1D\x16\x7A\x73\x1E\x10\x1A\x10\x6E\x7D"
"\x07\x00\x12\x7E\x18\x11\x17\x75\x21\x2E\x31\x3A\x67\x6F\x2F\x2C\x2C\x63\x32\x24\x36\x2B\x25\x26\x1F\x5B\x08\x18\x0D\x0C\x0B\x12\x00\x17\x03\x51\x5B\x57\x27\x10\x1E\x1F\x01\x07\x09\x1C\x4C\x53\x68\x43\x40\x20\x05\x04\x0B\x10\x34\x2F\x2B\x79\x60\x7F\x0F\x34\x35\x3D\x7D\x38\x38\x77\x3B\x25\x3E\x22\x27"
"\x27\x3D\x6F\x72\x6D\x11\x26\x23\x34\x34\x2E\x30\x3C\x5A\x10\x1D\x00\x50\x75\x51\x5D\x25\x1A\x1E\x15\x19\x00\x07\x55\x22\x0E\x04\x05\x01\x4F\x0A\x02\x10\x43\x22\x14\x15\x0E\x0A\x00\x29\x28\x78\x76\x7E\x2F\x3D\x2E\x21\x38\x35\x28\x25\x6D\x74\x21\x22\x2E\x68\x3A\x2F\x22\x29\x6D\x65\x33\x21\x32\x35\x2C"
"\x21\x3C\x09\x5C\x58\x0A\x07\x0C\x08\x18\x1F\x53\x07\x1E\x04\x1C\x07\x7F\x4A\x4B\x01\x07\x4E\x2A\x08\x0A\x07\x43\x01\x0F\x02\x47\x0B\x0B\x7A\x2F\x30\x3C\x7E\x28\x39\x3F\x7C\x59\x5A\x16\x19\x18\x10\x75\x02\x0A\x0A\x00\x1A\x1C\x46\x60\x62\x08\x25\x24\x36\x67\x13\x2C\x14\x1F\x17\x0E\x0D\x5F\x29\x0D\x16"
"\x12\x04\x14\x56\x16\x1A\x11\x4A\x2F\x0D\x0F\x0B\x01\x08\x08\x10\x43\x13\x08\x01\x09\x05\x11\x2F\x29\x3D\x2A\x7E\x3C\x29\x2F\x20\x36\x3E\x25\x78\x5D\x79\x75\x1F\x38\x2D\x69\x2F\x6F\x3C\x2C\x31\x30\x37\x2E\x34\x23\x64\x28\x1B\x15\x19\x1E\x1B\x0D\x47\x5D\x1C\x16\x06\x14\x04\x57\x06\x10\x1F\x18\x0D\x49"
"\x1E\x0E\x1F\x1E\x15\x0C\x12\x05\x15\x49\x6E\x48\x7A\x19\x3D\x2E\x3F\x2D\x39\x7D\x33\x27\x24\x30\x35\x3F\x39\x30\x24\x3F\x3B\x69\x2F\x21\x28\x6D\x2E\x2A\x2E\x2A\x35\x67\x2D\x2B\x5A\x0E\x16\x1C\x06\x0F\x19\x1E\x06\x16\x14\x51\x13\x1A\x15\x1C\x06\x18\x46\x63\x43\x4F\x20\x02\x01\x08\x40\x15\x0E\x02\x44"
"\x35\x19\x7B\x70\x0E\x37\x31\x77\x11\x7B\x73\x27\x39\x33\x39\x74\x2C\x25\x3E\x68\x25\x2B\x2E\x3A\x28\x62\x2A\x34\x6F\x4C\x6A\x64\x01\x15\x5B\x16\x16\x0A\x5F\x18\x14\x01\x12\x12\x1D\x13\x57\x00\x14\x07\x1B\x0D\x1B\x4E\x1F\x1E\x02\x16\x06\x03\x15\x0F\x08\x0A\x45\x35\x29\x78\x2B\x3B\x3E\x30\x70\x26\x3A"
"\x3D\x34\x76\x27\x26\x3A\x3E\x2E\x2B\x3D\x27\x20\x22\x6D\x37\x2D\x2C\x24\x35\x34\x64\x3C\x15\x0E\x72\x59\x5E\x14\x12\x12\x05\x53\x15\x09\x17\x14\x00\x19\x13\x4B\x1F\x01\x0F\x1B\x4C\x14\x0D\x16\x40\x00\x14\x02\x44\x01\x35\x32\x36\x3E\x70"
);

MAN(update, "Windows Update Guide",
"\x12\x14\x0F\x79\x0B\x0F\x18\x1C\x06\x16\x03\x71\x01\x18\x06\x1E\x40\x66\x68\x1E\x27\x21\x28\x22\x35\x30\x60\x14\x36\x23\x25\x31\x1F\x5B\x1C\x1C\x12\x16\x0A\x18\x00\x00\x4A\x51\x05\x12\x17\x00\x18\x02\x1C\x10\x4E\x09\x05\x15\x07\x10\x40\x49\x16\x06\x10\x06\x32\x7B\x0C\x2C\x3B\x2C\x38\x3C\x2B\x73\x6D"
"\x71\x64\x39\x30\x75\x1E\x3E\x2D\x3A\x2A\x2E\x35\x6D\x2D\x25\x4A\x61\x66\x33\x2C\x20\x5A\x16\x17\x17\x0A\x17\x55\x51\x52\x15\x15\x10\x02\x02\x06\x10\x4A\x1E\x18\x0D\x0F\x1B\x09\x1E\x42\x4B\x0F\x0F\x05\x02\x44\x04\x7A\x22\x3D\x38\x2C\x7F\x3A\x32\x20\x73\x07\x38\x38\x33\x3B\x22\x39\x6B\x79\x78\x67\x63"
"\x6C\x29\x30\x2A\x36\x24\x34\x4D\x64\x65\x0F\x0B\x1C\x18\x0A\x1A\x0F\x51\x52\x15\x19\x03\x1B\x00\x15\x07\x0F\x4B\x40\x3C\x2B\x29\x25\x42\x20\x2A\x2F\x32\x4A\x47\x37\x36\x1E\x72\x78\x38\x30\x3B\x7C\x73\x1C\x16\x04\x71\x23\x27\x30\x34\x3E\x2E\x3B\x67\x44\x62\x6C\x1C\x37\x22\x2C\x28\x32\x3E\x64\x30\x0A"
"\x1F\x19\x0D\x1B\x0C\x5C\x14\x1C\x00\x04\x10\x1A\x1B\x54\x14\x1F\x1F\x07\x04\x0F\x1B\x05\x0E\x03\x0F\x0C\x18\x5D\x47\x16\x00\x29\x2F\x39\x2B\x2A\x7F\x31\x3C\x2B\x73\x32\x34\x76\x25\x31\x24\x3F\x22\x3A\x2C\x2A\x61\x46\x47\x01\x0B\x05\x02\x0D\x67\x62\x65\x33\x35\x2B\x2D\x3F\x33\x30\x77\x21\x16\x04\x05"
"\x1F\x19\x13\x06\x4A\x55\x48\x3E\x07\x01\x08\x02\x15\x10\x40\x34\x16\x03\x05\x11\x3F\x7B\x66\x79\x1D\x37\x39\x3E\x39\x73\x36\x3E\x24\x77\x21\x25\x2E\x2A\x3C\x2C\x3D\x61\x6C\x02\x30\x63\x34\x29\x2F\x34\x64\x35\x08\x14\x1F\x0B\x1F\x12\x46\x77\x55\x06\x00\x15\x17\x03\x11\x58\x09\x03\x0D\x0A\x05\x48\x4C"
"\x42\x42\x44\x17\x08\x08\x03\x0B\x12\x29\x76\x2D\x29\x3A\x3E\x28\x38\x75\x73\x20\x30\x31\x32\x7A\x5F\x40\x1B\x09\x1C\x1D\x06\x02\x0A\x48\x6E\x60\x11\x27\x32\x37\x20\x5A\x0E\x08\x1D\x1F\x0B\x19\x0E\x52\x15\x1F\x03\x56\x02\x04\x55\x1E\x04\x48\x5C\x4E\x18\x09\x08\x09\x10\x5A\x41\x31\x0E\x0A\x01\x35\x2C"
"\x2B\x79\x0B\x2F\x38\x3C\x26\x36\x70\x6F\x76\x07\x35\x20\x39\x2E\x68\x3C\x3E\x2B\x2D\x39\x27\x30\x6E\x4B\x6B\x67\x14\x24\x0F\x08\x11\x17\x19\x5F\x15\x0E\x52\x15\x1F\x03\x56\x12\x19\x10\x18\x0C\x0D\x07\x0D\x06\x09\x1E\x42\x0C\x0E\x0D\x1F\x47\x49\x45\x23\x34\x2D\x2B\x7E\x0F\x1F\x7D\x21\x27\x31\x28\x25"
"\x77\x21\x3B\x3A\x39\x27\x3D\x2B\x2C\x38\x28\x26\x63\x2D\x24\x27\x29\x33\x2D\x13\x17\x1D\x57\x74\x75\x3D\x3E\x26\x3A\x26\x34\x56\x3F\x3B\x20\x38\x38\x62\x44\x4E\x38\x05\x03\x06\x0C\x17\x12\x46\x32\x14\x01\x3B\x2F\x3D\x79\x60\x7F\x1D\x39\x24\x32\x3E\x32\x33\x33\x74\x3A\x3A\x3F\x21\x26\x20\x3C\x6C\x73"
"\x62\x02\x23\x35\x2F\x31\x21\x65\x12\x14\x0D\x0B\x0D\x45\x5C\x2A\x1B\x1D\x14\x1E\x01\x04\x54\x02\x03\x07\x04\x49\x00\x00\x18\x67\x42\x43\x12\x04\x15\x13\x05\x17\x2E\x7B\x3C\x2C\x2C\x36\x32\x3A\x72\x27\x38\x3E\x25\x32\x74\x3D\x25\x3E\x3A\x3A\x60\x45\x61\x6D\x65\x11\x25\x32\x32\x26\x36\x31\x5A\x14\x08"
"\x0D\x17\x10\x12\x0E\x55\x53\x1C\x14\x02\x04\x54\x0C\x05\x1E\x48\x1A\x0D\x07\x09\x09\x17\x0F\x05\x41\x07\x47\x14\x17\x3F\x38\x31\x2A\x3B\x7F\x2E\x38\x21\x27\x31\x23\x22\x77\x20\x3C\x27\x2E\x66\x43\x44\x00\x1C\x19\x0B\x0C\x0E\x00\x0A\x67\x11\x15\x3E\x3A\x2C\x3C\x2D\x75\x51\x5D\x33\x17\x06\x10\x18\x14"
"\x11\x11\x4A\x04\x18\x1D\x07\x00\x02\x1E\x42\x5D\x40\x2E\x16\x13\x0D\x0A\x34\x3A\x34\x79\x2B\x2F\x38\x3C\x26\x36\x23\x6B\x76\x33\x26\x3C\x3C\x2E\x3A\x69\x2F\x21\x28\x6D\x24\x2A\x32\x2C\x31\x26\x36\x20\x5A\x0E\x08\x1D\x1F\x0B\x19\x0E\x52\x07\x18\x10\x02\x7D\x54\x55\x0B\x19\x0D\x49\x00\x00\x18\x4D\x0B"
"\x0D\x13\x15\x07\x0B\x08\x00\x3E\x7B\x39\x2C\x2A\x30\x31\x3C\x26\x3A\x33\x30\x3A\x3B\x2D\x7B\x6A\x02\x26\x3A\x3A\x2E\x20\x21\x62\x37\x28\x24\x2B\x67\x33\x2D\x1F\x15\x58\x18\x5E\x1B\x19\x0B\x1B\x10\x15\x51\x1B\x1E\x07\x17\x0F\x03\x09\x1F\x0B\x1C\x42\x67\x68\x36\x30\x25\x27\x33\x21\x45\x12\x12\x0B\x0D"
"\x11\x0D\x05\x57\x7F\x73\x07\x38\x38\x33\x3B\x22\x39\x6B\x1D\x39\x2A\x2E\x38\x28\x62\x7D\x60\x14\x36\x23\x25\x31\x1F\x5B\x10\x10\x0D\x0B\x13\x0F\x0B\x49\x50\x1D\x1F\x04\x00\x55\x05\x0D\x48\x00\x00\x1C\x18\x0C\x0E\x0F\x05\x05\x46\x12\x14\x01\x3B\x2F\x3D\x2A\x7E\x28\x35\x29\x3A\x73\x1B\x13\x5C\x77\x74"
"\x3B\x3F\x26\x2A\x2C\x3C\x3C\x60\x6D\x26\x22\x34\x24\x35\x6B\x64\x24\x14\x1F\x58\x0C\x10\x16\x12\x0E\x06\x12\x1C\x1D\x56\x18\x04\x01\x03\x04\x06\x1A\x4E\x09\x03\x1F\x42\x10\x0F\x0C\x03\x49\x6E\x48\x7A\x0F\x30\x30\x2D\x7F\x2C\x2F\x3D\x34\x22\x30\x3B\x6D\x74\x72\x3F\x3B\x2C\x28\x3A\x2A\x61\x25\x2B\x30"
"\x34\x2E\x34\x3E\x63\x6B\x70\x71\x3C\x2B\x37\x29\x39\x2F\x21\x53\x26\x38\x37\x57\x23\x3C\x24\x2F\x27\x3E\x3D\x4F\x39\x3D\x26\x22\x34\x24\x6C\x4A\x44\x21\x28\x32\x2E\x3C\x2C\x2C\x7C\x3E\x3D\x3E\x35\x71\x22\x3F\x26\x3A\x3F\x2C\x20\x69\x19\x26\x22\x29\x2D\x34\x33\x61\x13\x37\x20\x24\x0E\x1E\x58\x18\x10"
"\x1B\x5C\x32\x37\x3E\x50\x10\x06\x07\x07\x5B\x4A\x22\x0E\x49\x0F\x4F\x08\x08\x14\x0A\x03\x04\x46\x01\x05\x0C\x36\x28\x52\x79\x7E\x3E\x3A\x29\x37\x21\x70\x30\x76\x33\x26\x3C\x3C\x2E\x3A\x69\x3B\x3F\x28\x2C\x36\x26\x7A\x61\x02\x22\x32\x2C\x19\x1E\x58\x34\x1F\x11\x1D\x1A\x17\x01\x50\x4F\x56\x13\x11\x03"
"\x03\x08\x0D\x49\x50\x4F\x3C\x1F\x0D\x13\x05\x13\x12\x0E\x01\x16\x7A\x65\x78\x1D\x2C\x36\x2A\x38\x20\x73\x6E\x5B\x76\x77\x06\x3A\x26\x27\x68\x2B\x2F\x2C\x27\x6D\x26\x31\x29\x37\x23\x35\x6A\x4F\x70\x3F\x3D\x35\x37\x29\x39\x2F\x2B\x53\x3F\x21\x22\x3E\x39\x3C\x30\x2A\x3C\x20\x21\x21\x66\x40\x42\x22\x04"
"\x17\x07\x09\x07\x00\x3E\x7B\x37\x29\x2A\x36\x33\x33\x21\x73\x6E\x71\x12\x32\x38\x3C\x3C\x2E\x3A\x30\x6E\x20\x3C\x39\x2B\x2E\x29\x3B\x27\x33\x2D\x2A\x14\x41\x58\x1D\x11\x08\x12\x11\x1D\x12\x14\x02\x56\x02\x04\x11\x0B\x1F\x0D\x1A\x4E\x09\x1E\x02\x0F\x43\x0F\x15\x0E\x02\x16\x6F\x7A\x7B\x08\x1A\x2D\x7F"
"\x33\x33\x72\x2A\x3F\x24\x24\x77\x3A\x30\x3E\x3C\x27\x3B\x25\x6F\x64\x2C\x2C\x27\x60\x2E\x36\x33\x2D\x2A\x14\x1A\x14\x15\x07\x5F\x08\x15\x17\x53\x19\x1F\x02\x12\x06\x1B\x0F\x1F\x41\x47\x4E\x3D\x09\x09\x17\x00\x05\x12\x46\x05\x05\x0B\x3E\x2C\x31\x3D\x2A\x37\x7C\x70\x58\x73\x70\x39\x37\x39\x30\x2C\x6A"
"\x24\x26\x69\x3D\x23\x23\x3A\x62\x20\x2F\x2F\x28\x22\x27\x31\x13\x14\x16\x0A\x50\x5F\x28\x15\x1B\x00\x50\x01\x04\x18\x13\x07\x0B\x06\x52\x49\x49\x0B\x09\x01\x0B\x15\x05\x13\x1F\x4A\x0B\x15\x2E\x32\x35\x30\x24\x3E\x28\x34\x3D\x3D\x77\x7F\x5C\x5D\x12\x14\x03\x07\x0D\x0D\x6E\x1A\x1C\x09\x03\x17\x05\x12"
"\x79\x67\x10\x17\x23\x5B\x2C\x31\x37\x2C\x5C\x32\x20\x37\x35\x23\x7C\x46\x5D\x55\x38\x1E\x06\x49\x1A\x07\x09\x4D\x35\x0A\x0E\x05\x09\x10\x17\x45\x0F\x2B\x3C\x38\x2A\x3A\x7C\x09\x20\x3C\x25\x33\x3A\x32\x27\x3D\x25\x24\x3C\x2C\x3C\x6F\x64\x1E\x27\x37\x34\x28\x28\x20\x37\x65\x44\x5B\x2B\x00\x0D\x0B\x19"
"\x10\x52\x4D\x50\x25\x04\x18\x01\x17\x06\x0E\x1B\x01\x01\x00\x18\x67\x42\x43\x40\x5F\x46\x28\x10\x0D\x3F\x29\x78\x2D\x2C\x30\x29\x3F\x3E\x36\x23\x39\x39\x38\x20\x30\x38\x38\x68\x77\x6E\x18\x25\x23\x26\x2C\x37\x32\x66\x12\x34\x21\x1B\x0F\x1D\x50\x50\x75\x4E\x54\x52\x21\x15\x02\x02\x16\x06\x01\x4A\x1F"
"\x00\x0C\x4E\x3F\x2F\x4D\x03\x0D\x04\x41\x14\x02\x10\x17\x23\x75\x52\x6A\x77\x7F\x1A\x2F\x37\x36\x70\x35\x3F\x24\x3F\x75\x39\x3B\x29\x2A\x2B\x6F\x64\x38\x32\x27\x21\x35\x23\x34\x64\x2B\x1F\x1E\x1C\x59\x0C\x10\x13\x10\x5B\x5D\x7A\x45\x5F\x57\x26\x10\x04\x0A\x05\x0C\x4E\x1B\x04\x08\x42\x30\x0F\x07\x12"
"\x10\x05\x17\x3F\x1F\x31\x2A\x2A\x2D\x35\x3F\x27\x27\x39\x3E\x38\x77\x32\x3A\x26\x2F\x2D\x3B\x6E\x67\x2D\x29\x2F\x2A\x2E\x68\x7C\x4D\x64\x65\x5A\x5B\x58\x17\x1B\x0B\x5C\x0E\x06\x1C\x00\x51\x01\x02\x15\x00\x19\x0E\x1A\x1F\x64\x4F\x4C\x4D\x42\x43\x12\x04\x08\x47\x27\x5F\x06\x0C\x31\x37\x3A\x30\x2B\x2E"
"\x0E\x00\x3F\x37\x22\x20\x35\x27\x2F\x0F\x21\x3A\x3A\x3D\x25\x2F\x37\x37\x29\x2E\x28\x67\x17\x2A\x1C\x0F\x0F\x18\x0C\x1A\x38\x14\x01\x07\x02\x18\x14\x02\x00\x1C\x05\x05\x46\x06\x02\x0B\x66\x4D\x42\x43\x40\x41\x08\x02\x10\x45\x29\x2F\x39\x2B\x2A\x7F\x2B\x28\x33\x26\x23\x34\x24\x21\x5E\x60\x63\x6B\x1A"
"\x2C\x3D\x2A\x38\x6D\x15\x2A\x2E\x25\x29\x30\x37\x65\x2F\x0B\x1C\x18\x0A\x1A\x5C\x1E\x1D\x1E\x00\x1E\x18\x12\x1A\x01\x19\x4B\x40\x1D\x06\x06\x1F\x4D\x12\x11\x0F\x06\x14\x06\x09\x5F\x7A\x7C\x2A\x3C\x2D\x3A\x28\x70\x25\x3A\x3E\x35\x39\x20\x27\x78\x3F\x3B\x2C\x28\x3A\x2A\x6B\x64\x6C\x49\x76\x68\x66\x04"
"\x2C\x20\x19\x10\x58\x0D\x16\x1A\x5C\x18\x00\x01\x1F\x03\x56\x14\x1B\x11\x0F\x4B\x01\x07\x4E\x02\x0D\x03\x17\x02\x0C\x4C\x03\x15\x16\x0A\x28\x28\x76\x53\x54\x16\x12\x0E\x1B\x17\x15\x03\x76\x07\x06\x1A\x0D\x19\x09\x04\x44\x62\x6C\x1D\x30\x26\x36\x28\x23\x30\x64\x27\x0F\x12\x14\x1D\x0D\x45\x5C\x2E\x17"
"\x07\x04\x18\x18\x10\x07\x55\x54\x4B\x3F\x00\x00\x0B\x03\x1A\x11\x43\x35\x11\x02\x06\x10\x00\x7A\x65\x78\x0E\x37\x31\x38\x32\x25\x20\x70\x18\x38\x24\x3D\x31\x2F\x39\x68\x19\x3C\x20\x2B\x3F\x23\x2E\x6E\x4B\x6B\x67\x14\x37\x1F\x56\x0A\x1C\x12\x1A\x1D\x0E\x17\x53\x03\x1E\x10\x03\x03\x14\x18\x0E\x48\x0A"
"\x0F\x01\x4C\x0F\x10\x06\x01\x0A\x46\x13\x0C\x0C\x34\x3C\x2B\x79\x73\x7F\x29\x2E\x37\x73\x31\x71\x34\x36\x37\x3E\x3F\x3B\x68\x26\x3C\x6F\x2D\x6D\x14\x0E\x6E\x4B\x4C\x10\x0C\x00\x34\x5B\x2F\x30\x30\x3B\x33\x2A\x21\x53\x23\x24\x26\x27\x3B\x27\x3E\x4B\x2D\x27\x2A\x3C\x66\x40\x42\x34\x09\x0F\x02\x08\x13"
"\x16\x7A\x6A\x68\x79\x2C\x3A\x3D\x3E\x3A\x36\x34\x71\x33\x39\x30\x75\x25\x2D\x68\x3A\x3B\x3F\x3C\x22\x30\x37\x60\x28\x28\x67\x0B\x26\x0E\x14\x1A\x1C\x0C\x5F\x4E\x4D\x40\x46\x50\x59\x33\x24\x21\x55\x0B\x1D\x09\x00\x02\x0E\x0E\x01\x07\x43\x06\x0E\x14\x6D\x44\x45\x29\x34\x35\x3C\x7E\x3A\x38\x34\x26\x3A"
"\x3F\x3F\x25\x7E\x7A\x75\x1F\x3B\x2F\x3B\x2F\x2B\x25\x23\x25\x63\x34\x2E\x66\x10\x2D\x2B\x1E\x14\x0F\x0A\x5E\x4E\x4D\x5D\x1B\x00\x50\x03\x13\x14\x1B\x18\x07\x0E\x06\x0D\x0B\x0B\x4C\x0B\x0D\x11\x40\x12\x03\x04\x11\x17\x33\x2F\x21\x77"
);

MAN(accounts, "User Accounts Guide",
"\x16\x14\x1B\x18\x12\x7F\x0A\x0E\x72\x1E\x19\x12\x04\x18\x07\x1A\x0C\x1F\x68\x08\x0D\x0C\x03\x18\x0C\x17\x4A\x6C\x66\x0A\x2D\x26\x08\x14\x0B\x16\x18\x0B\x5C\x1C\x11\x10\x1F\x04\x18\x03\x4E\x55\x19\x12\x06\x0A\x1D\x4F\x1F\x08\x16\x17\x09\x0F\x01\x14\x48\x45\x15\x35\x3D\x1D\x2C\x36\x2A\x38\x7E\x73\x03"
"\x25\x39\x25\x31\x75\x3A\x3E\x3A\x2A\x26\x2E\x3F\x28\x31\x6F\x60\x33\x23\x24\x2B\x33\x1F\x09\x01\x73\x5E\x5F\x13\x0D\x06\x1A\x1F\x1F\x05\x5B\x54\x14\x04\x0F\x48\x1E\x01\x1D\x07\x1E\x42\x06\x16\x04\x14\x1E\x13\x0D\x3F\x29\x3D\x77\x7E\x0D\x39\x3E\x3D\x3E\x3D\x34\x38\x33\x31\x31\x6A\x2D\x27\x3B\x6E\x22"
"\x23\x3E\x36\x63\x30\x24\x29\x37\x28\x20\x54\x71\x55\x59\x32\x10\x1F\x1C\x1E\x53\x11\x12\x15\x18\x01\x1B\x1E\x51\x48\x1E\x01\x1D\x07\x1E\x42\x0C\x06\x07\x0A\x0E\x0A\x00\x76\x7B\x3D\x2F\x3B\x2D\x25\x29\x3A\x3A\x3E\x36\x76\x24\x20\x34\x33\x38\x68\x26\x20\x6F\x38\x25\x27\x63\x10\x02\x68\x4D\x69\x65\x29"
"\x0C\x11\x0D\x1D\x17\x46\x5D\x21\x16\x04\x05\x1F\x19\x13\x06\x4A\x55\x48\x28\x0D\x0C\x03\x18\x0C\x17\x13\x41\x58\x47\x3D\x0A\x2F\x29\x78\x30\x30\x39\x33\x7D\x6C\x73\x77\x02\x3F\x30\x3A\x75\x23\x25\x68\x3E\x27\x3B\x24\x6D\x23\x63\x2C\x2E\x25\x26\x28\x65\x1B\x18\x1B\x16\x0B\x11\x08\x5A\x78\x53\x50\x1E"
"\x04\x57\x53\x26\x03\x0C\x06\x49\x07\x01\x4C\x1A\x0B\x17\x08\x41\x07\x47\x29\x0C\x39\x29\x37\x2A\x31\x39\x28\x7D\x33\x30\x33\x3E\x23\x39\x20\x72\x64\x41\x42\x08\x0A\x02\x05\x03\x0B\x10\x14\x13\x07\x13\x0B\x17\x5A\x2D\x2B\x59\x2D\x2B\x3D\x33\x36\x32\x22\x35\x7C\x5A\x54\x34\x0E\x06\x01\x07\x07\x1C\x18"
"\x1F\x03\x17\x0F\x13\x15\x47\x07\x04\x34\x7B\x3B\x31\x3F\x31\x3B\x38\x72\x20\x29\x22\x22\x32\x39\x75\x39\x2E\x3C\x3D\x27\x21\x2B\x3E\x62\x22\x2E\x25\x66\x2E\x2A\x36\x0E\x1A\x14\x15\x5E\x0C\x13\x1B\x06\x04\x11\x03\x13\x59\x7E\x58\x4A\x38\x1C\x08\x00\x0B\x0D\x1F\x06\x43\x15\x12\x03\x15\x17\x45\x39\x3A"
"\x36\x37\x31\x2B\x7C\x70\x72\x27\x38\x30\x22\x77\x24\x27\x25\x3F\x2D\x2A\x3A\x3C\x6C\x39\x2A\x26\x60\x32\x3F\x34\x30\x20\x17\x5B\x50\x2C\x3F\x3C\x55\x53\x78\x5E\x50\x32\x1E\x16\x1A\x12\x0F\x4B\x09\x49\x1B\x1C\x09\x1F\x45\x10\x40\x0D\x03\x11\x01\x09\x60\x7B\x0B\x3C\x2A\x2B\x35\x33\x35\x20\x70\x6F\x76"
"\x16\x37\x36\x25\x3E\x26\x3D\x3D\x6F\x72\x6D\x0D\x37\x28\x24\x34\x67\x31\x36\x1F\x09\x0B\x59\x40\x5F\x3D\x1E\x11\x1C\x05\x1F\x02\x57\x00\x0C\x1A\x0E\x46\x63\x43\x4F\x21\x0C\x09\x06\x40\x18\x09\x12\x16\x16\x3F\x37\x3E\x79\x3F\x3B\x31\x34\x3C\x73\x26\x38\x37\x77\x27\x34\x2C\x2E\x68\x24\x21\x2B\x29\x6A"
"\x31\x63\x22\x34\x2F\x2B\x30\x68\x13\x15\x58\x38\x1A\x12\x15\x13\x1B\x00\x04\x03\x17\x03\x1B\x07\x4A\x02\x0E\x49\x02\x00\x0F\x06\x07\x07\x40\x0E\x13\x13\x6E\x45\x7A\x73\x39\x3D\x28\x3E\x32\x3E\x37\x37\x70\x23\x33\x34\x3B\x23\x2F\x39\x31\x69\x2D\x20\x22\x3E\x2D\x2F\x25\x68\x68\x4D\x4E\x16\x33\x3C\x36"
"\x54\x37\x31\x5C\x32\x22\x27\x39\x3E\x38\x24\x54\x5D\x3D\x02\x06\x0D\x01\x18\x1F\x4D\x2A\x06\x0C\x0D\x09\x4E\x6E\x48\x7A\x0B\x11\x17\x64\x7F\x3A\x3C\x21\x27\x35\x22\x22\x77\x35\x3B\x2E\x6B\x3B\x28\x28\x2A\x6C\x65\x26\x26\x36\x28\x25\x22\x69\x27\x15\x0E\x16\x1D\x57\x51\x76\x50\x52\x35\x11\x12\x13\x57"
"\x5C\x3C\x38\x4B\x0B\x08\x03\x0A\x1E\x0C\x4B\x43\x01\x0F\x02\x47\x22\x0C\x34\x3C\x3D\x2B\x2E\x2D\x35\x33\x26\x69\x70\x33\x3F\x38\x39\x30\x3E\x39\x21\x2A\x6E\x07\x29\x21\x2E\x2C\x6E\x4B\x6B\x67\x14\x24\x09\x08\x0F\x16\x0C\x1B\x5C\x52\x52\x23\x19\x12\x02\x02\x06\x10\x4A\x1B\x09\x1A\x1D\x18\x03\x1F\x06"
"\x59\x40\x07\x07\x0B\x08\x07\x3B\x38\x33\x2A\x70\x55\x71\x7D\x16\x2A\x3E\x30\x3B\x3E\x37\x75\x26\x24\x2B\x22\x74\x6F\x20\x22\x21\x28\x60\x20\x33\x33\x2B\x28\x1B\x0F\x11\x1A\x1F\x13\x10\x04\x52\x04\x18\x14\x18\x57\x0D\x1A\x1F\x19\x48\x19\x06\x00\x02\x08\x42\x0F\x05\x00\x10\x02\x17\x4B\x50\x76\x78\x0A"
"\x3B\x2B\x7C\x28\x22\x73\x39\x3F\x76\x04\x31\x21\x3E\x22\x26\x2E\x3D\x6F\x72\x6D\x03\x20\x23\x2E\x33\x29\x30\x36\x5A\x45\x58\x2A\x17\x18\x12\x50\x1B\x1D\x50\x1E\x06\x03\x1D\x1A\x04\x18\x46\x63\x64\x29\x2D\x20\x2B\x2F\x39\x41\x40\x47\x2B\x31\x12\x1E\x0A\x79\x0B\x0C\x19\x0F\x01\x59\x7D\x71\x19\x23\x3C"
"\x30\x38\x6B\x3D\x3A\x2B\x3D\x3F\x6D\x7C\x63\x01\x25\x22\x67\x25\x26\x19\x14\x0D\x17\x0A\x45\x5C\x1B\x13\x1E\x19\x1D\x0F\x57\x19\x10\x07\x09\x0D\x1B\x1D\x4F\x44\x20\x0B\x00\x12\x0E\x15\x08\x02\x11\x7A\x3D\x39\x34\x37\x33\x25\x7D\x21\x32\x36\x34\x22\x2E\x6E\x5F\x6A\x6B\x3B\x2A\x3C\x2A\x29\x23\x62\x37"
"\x29\x2C\x23\x6B\x64\x26\x15\x15\x0C\x1C\x10\x0B\x5C\x1B\x1B\x1F\x04\x14\x04\x04\x58\x55\x0B\x08\x1C\x00\x18\x06\x18\x14\x42\x11\x05\x11\x09\x15\x10\x0C\x34\x3C\x71\x79\x31\x2D\x7C\x32\x26\x3B\x35\x23\x76\x36\x37\x36\x25\x3E\x26\x3D\x3D\x61\x46\x60\x62\x10\x34\x20\x28\x23\x25\x37\x1E\x5B\x0D\x0A\x1B"
"\x0D\x0F\x5D\x10\x0A\x50\x15\x13\x11\x15\x00\x06\x1F\x53\x49\x0D\x07\x05\x01\x06\x43\x01\x02\x05\x08\x11\x0B\x2E\x28\x78\x2C\x30\x3B\x39\x2F\x72\x35\x31\x3C\x3F\x3B\x2D\x75\x39\x2A\x2E\x2C\x3A\x36\x62\x47\x48\x14\x0F\x13\x0D\x67\x62\x65\x29\x38\x30\x36\x31\x33\x5C\x3C\x31\x30\x3F\x24\x38\x23\x27\x7F"
"\x47\x4B\x3B\x0C\x1A\x1B\x05\x03\x05\x10\x40\x5F\x46\x26\x07\x06\x35\x2E\x36\x2D\x2D\x7F\x62\x7D\x13\x30\x33\x34\x25\x24\x74\x22\x25\x39\x23\x69\x21\x3D\x6C\x3E\x21\x2B\x2F\x2E\x2A\x7D\x64\x2F\x15\x12\x16\x59\x3F\x05\x09\x0F\x17\x53\x31\x35\x59\x32\x1A\x01\x18\x0A\x48\x20\x2A\x43\x66\x4D\x42\x2E\x24"
"\x2C\x46\x4F\x2D\x0B\x2E\x2E\x36\x3C\x77\x73\x7C\x32\x20\x73\x23\x38\x31\x39\x74\x3C\x24\x6B\x3C\x26\x6E\x36\x23\x38\x30\x63\x2F\x33\x21\x26\x2A\x2C\x00\x1A\x0C\x10\x11\x11\x5B\x0E\x52\x12\x00\x01\x05\x59\x7E\x7F\x3F\x38\x2D\x3B\x4E\x3F\x3E\x22\x24\x2A\x2C\x24\x35\x47\x42\x45\x1C\x14\x14\x1D\x1B\x0D"
"\x0F\x57\x7F\x73\x15\x30\x35\x3F\x74\x20\x39\x2E\x3A\x69\x26\x2E\x3F\x6D\x01\x79\x1C\x14\x35\x22\x36\x36\x26\x47\x16\x18\x13\x1A\x42\x5D\x05\x1A\x04\x19\x56\x33\x11\x06\x01\x1F\x07\x19\x42\x4F\x28\x02\x01\x16\x0D\x04\x08\x13\x17\x49\x7A\x1F\x37\x2E\x30\x33\x33\x3C\x36\x20\x7C\x5B\x76\x77\x04\x3C\x29"
"\x3F\x3D\x3B\x2B\x3C\x60\x6D\x0F\x36\x33\x28\x25\x6B\x64\x13\x13\x1F\x1D\x16\x0D\x51\x76\x50\x52\x3E\x1F\x07\x1F\x19\x13\x55\x1F\x18\x0D\x1B\x4E\x09\x03\x01\x06\x06\x12\x12\x5C\x47\x16\x0C\x3D\x33\x2C\x74\x3D\x33\x35\x3E\x39\x73\x36\x3E\x3A\x33\x31\x27\x6A\x75\x68\x19\x3C\x20\x3C\x28\x30\x37\x29\x24"
"\x35\x67\x7A\x65\x36\x14\x1B\x18\x0A\x16\x13\x13\x52\x4D\x50\x3C\x19\x01\x11\x7F\x4A\x4B\x40\x26\x00\x0A\x28\x1F\x0B\x15\x05\x41\x0B\x06\x1D\x45\x37\x3A\x36\x38\x39\x3A\x7C\x29\x3A\x36\x3D\x71\x3F\x39\x27\x21\x2F\x2A\x2C\x60\x60\x45\x46\x0E\x0E\x02\x13\x12\x0F\x04\x64\x11\x35\x34\x34\x2A\x74\x52\x5C"
"\x13\x17\x07\x00\x1D\x01\x1E\x0E\x4F\x4A\x0A\x0C\x1F\x0F\x01\x0F\x08\x06\x43\x15\x12\x03\x15\x44\x09\x33\x28\x2C\x79\x76\x3E\x29\x29\x3D\x7E\x3C\x3E\x31\x3E\x3A\x75\x29\x23\x2D\x2A\x25\x2D\x23\x35\x6B\x6D\x4A\x6C\x66\x2B\x31\x36\x08\x16\x1F\x0B\x50\x12\x0F\x1E\x48\x53\x1C\x1E\x15\x16\x18\x55\x1F\x18"
"\x0D\x1B\x1D\x4F\x0D\x03\x06\x43\x07\x13\x09\x12\x14\x16\x7A\x73\x08\x2B\x31\x70\x19\x33\x26\x36\x22\x21\x24\x3E\x27\x30\x63\x65\x42\x64\x6E\x1B\x24\x24\x31\x63\x30\x33\x29\x20\x36\x24\x17\x41\x58\x5E\x0B\x0C\x19\x0F\x5F\x12\x13\x12\x19\x02\x1A\x01\x19\x4C\x44\x49\x49\x03\x19\x1E\x10\x0E\x07\x13\x41"
"\x4B\x44\x04\x39\x38\x37\x2C\x30\x2B\x71\x3E\x20\x36\x31\x25\x33\x78\x31\x3B\x2B\x29\x24\x2C\x61\x2B\x25\x3E\x23\x21\x2C\x24\x69\x4D\x64\x65\x1E\x1E\x14\x1C\x0A\x1A\x53\x1C\x16\x1E\x19\x1F\x59\x07\x15\x06\x19\x1C\x07\x1B\x0A\x42\x1F\x08\x16\x43\x03\x0E\x0B\x0A\x05\x0B\x3E\x28\x76\x53\x54\x0D\x19\x0E"
"\x17\x07\x70\x10\x76\x11\x1B\x07\x0D\x04\x1C\x1D\x0B\x01\x6C\x1D\x03\x10\x13\x16\x09\x15\x00\x4F\x57\x5B\x35\x10\x1D\x0D\x13\x0E\x1D\x15\x04\x51\x17\x14\x17\x1A\x1F\x05\x1C\x53\x4E\x1D\x09\x1E\x07\x17\x40\x0E\x08\x0B\x0D\x0B\x3F\x7B\x70\x38\x3D\x3C\x33\x28\x3C\x27\x7E\x3D\x3F\x21\x31\x7B\x29\x24\x25"
"\x66\x3E\x2E\x3F\x3E\x35\x2C\x32\x25\x69\x35\x21\x36\x1F\x0F\x51\x57\x74\x52\x5C\x31\x1D\x10\x11\x1D\x56\x16\x17\x16\x05\x1E\x06\x1D\x4E\x18\x05\x19\x0A\x43\x13\x04\x05\x12\x16\x0C\x2E\x22\x78\x28\x2B\x3A\x2F\x29\x3B\x3C\x3E\x22\x6C\x77\x21\x26\x2F\x6B\x3C\x21\x2B\x22\x6C\x2C\x36\x63\x34\x29\x23\x67"
"\x28\x2A\x19\x10\x58\x0A\x1D\x0D\x19\x18\x1C\x5D\x7A\x5C\x56\x3B\x1B\x16\x0B\x07\x48\x08\x0D\x0C\x03\x18\x0C\x17\x40\x16\x0F\x13\x0C\x0A\x2F\x2F\x78\x28\x2B\x3A\x2F\x29\x3B\x3C\x3E\x22\x6C\x77\x26\x30\x29\x24\x3E\x2C\x3C\x36\x6C\x22\x32\x37\x29\x2E\x28\x34\x64\x6D\x08\x1E\x0B\x1C\x0A\x5F\x08\x15\x1B"
"\x00\x50\x21\x35\x57\x59\x7F\x4A\x4B\x03\x0C\x0B\x1F\x4C\x0B\x0B\x0F\x05\x12\x5D\x47\x0B\x17\x7A\x2E\x2B\x3C\x7E\x2B\x34\x38\x72\x31\x25\x38\x3A\x23\x79\x3C\x24\x6B\x09\x2D\x23\x26\x22\x24\x31\x37\x32\x20\x32\x28\x36\x65\x13\x15\x58\x0A\x1F\x19\x19\x5D\x1F\x1C\x14\x14\x5F\x59\x7E\x7F\x3F\x2A\x2B\x49"
"\x22\x2A\x3A\x28\x2E\x30\x6A\x4C\x46\x33\x0C\x0C\x29\x7B\x28\x2B\x31\x38\x2E\x3C\x3F\x69\x70\x76\x23\x36\x37\x78\x26\x2E\x3E\x2C\x22\x62\x3F\x28\x36\x64\x60\x69\x76\x6A\x70\x6C\x54\x5B\x34\x1C\x08\x1A\x10\x5D\x41\x53\x58\x15\x13\x11\x15\x00\x06\x1F\x41\x49\x0C\x0E\x00\x0C\x0C\x00\x05\x12\x46\x17\x16"
"\x0A\x37\x2B\x2C\x2A\x54\x7F\x7C\x2A\x3B\x27\x38\x71\x25\x32\x37\x20\x38\x22\x3C\x30\x60\x6F\x02\x28\x34\x26\x32\x61\x33\x34\x21\x65\x16\x1E\x0E\x1C\x12\x5F\x4C\x5D\x5A\x1D\x1F\x51\x06\x05\x1B\x18\x1A\x1F\x1B\x40\x40\x65\x66\x38\x31\x26\x32\x41\x36\x35\x2B\x23\x13\x17\x1D\x79\x0C\x1A\x0C\x1C\x1B\x01"
"\x5A\x7C\x76\x14\x3B\x27\x38\x3E\x38\x3D\x6E\x3F\x3E\x22\x24\x2A\x2C\x24\x79\x67\x07\x37\x1F\x1A\x0C\x1C\x5E\x1E\x5C\x13\x17\x04\x50\x04\x05\x12\x06\x59\x4A\x08\x07\x19\x17\x4F\x0A\x04\x0E\x06\x13\x4D\x46\x13\x0C\x00\x34\x7B\x3C\x3C\x32\x3A\x28\x38\x72\x27\x38\x34\x76\x38\x38\x31\x40\x6B\x68\x39\x3C"
"\x20\x2A\x24\x2E\x26\x6E\x61\x09\x35\x64\x30\x09\x1E\x58\x2B\x1B\x18\x15\x0E\x06\x01\x09\x51\x33\x0F\x04\x19\x05\x19\x0D\x1B\x54\x4F\x24\x26\x2E\x2E\x3C\x32\x09\x01\x10\x12\x3B\x29\x3D\x05\x13\x36\x3F\x2F\x3D\x20\x3F\x37\x22\x0B\x03\x3C\x24\x2F\x27\x3E\x3D\x45\x6C\x6D\x0C\x17\x1C\x02\x33\x35\x36\x20"
"\x14\x0F\x2E\x1C\x0C\x0C\x15\x12\x1C\x2F\x20\x03\x19\x11\x1D\x19\x0F\x27\x01\x1A\x1A\x41"
);

MAN(backup, "Backup Guide",
"\x0E\x13\x1D\x79\x19\x10\x10\x19\x17\x1D\x70\x03\x03\x1B\x11\x6F\x6A\x78\x65\x7B\x63\x7E\x46\x7E\x62\x20\x2F\x31\x2F\x22\x37\x65\x15\x1D\x58\x00\x11\x0A\x0E\x5D\x16\x12\x04\x10\x5A\x57\x1B\x1B\x4A\x59\x48\x0D\x07\x09\x0A\x08\x10\x06\x0E\x15\x46\x0A\x01\x01\x33\x3A\x78\x2D\x27\x2F\x39\x2E\x7E\x73\x61"
"\x71\x35\x38\x24\x2C\x6A\x24\x2E\x2F\x63\x3C\x25\x39\x27\x6D\x4A\x4B\x09\x09\x01\x65\x3E\x29\x31\x2F\x3B\x5F\x54\x3E\x3E\x3C\x25\x35\x5F\x7D\x59\x55\x28\x1E\x01\x05\x1A\x4F\x05\x03\x16\x0C\x40\x36\x0F\x09\x00\x0A\x2D\x28\x76\x79\x07\x30\x29\x2F\x72\x17\x35\x22\x3D\x23\x3B\x25\x65\x0F\x27\x2A\x3B\x22"
"\x29\x23\x36\x30\x6F\x11\x2F\x24\x30\x30\x08\x1E\x0B\x59\x1D\x1E\x12\x5D\x01\x0A\x1E\x12\x56\x03\x1B\x55\x1E\x03\x0D\x63\x4E\x4F\x0F\x01\x0D\x16\x04\x41\x07\x12\x10\x0A\x37\x3A\x2C\x30\x3D\x3E\x30\x31\x2B\x73\x78\x02\x33\x23\x20\x3C\x24\x2C\x3B\x69\x70\x6F\x0D\x2E\x21\x2C\x35\x2F\x32\x34\x64\x7B\x5A"
"\x2C\x11\x17\x1A\x10\x0B\x0E\x52\x11\x11\x12\x1D\x02\x04\x55\x05\x19\x48\x26\x00\x0A\x28\x1F\x0B\x15\x05\x6B\x46\x47\x05\x15\x2A\x7B\x66\x79\x1C\x3E\x3F\x36\x27\x23\x70\x25\x37\x35\x74\x6B\x6A\x06\x29\x27\x2F\x28\x29\x6D\x20\x22\x23\x2A\x33\x37\x6D\x6B\x70\x56\x58\x2F\x1B\x0D\x0F\x14\x1D\x1D\x50\x19"
"\x1F\x04\x00\x1A\x18\x12\x52\x49\x1C\x06\x0B\x05\x16\x4E\x03\x0D\x0F\x04\x0F\x45\x3B\x7B\x17\x37\x3B\x1B\x2E\x34\x24\x36\x70\x37\x3F\x3B\x31\x75\x74\x6B\x1E\x2C\x3C\x3C\x25\x22\x2C\x63\x28\x28\x35\x33\x2B\x37\x03\x55\x72\x54\x5E\x30\x12\x18\x36\x01\x19\x07\x13\x57\x15\x19\x19\x04\x48\x01\x01\x03\x08"
"\x1E\x42\x1A\x0F\x14\x14\x47\x33\x0C\x34\x3F\x37\x2E\x2D\x7F\x2F\x38\x26\x27\x39\x3F\x31\x24\x74\x37\x2B\x28\x23\x3C\x3E\x6F\x2D\x23\x26\x63\x02\x28\x32\x0B\x2B\x26\x11\x1E\x0A\x59\x0C\x1A\x1F\x12\x04\x16\x02\x08\x7C\x57\x54\x1E\x0F\x12\x1B\x49\x46\x06\x02\x4D\x1B\x0C\x15\x13\x46\x06\x07\x06\x35\x2E"
"\x36\x2D\x79\x2C\x7C\x2E\x37\x30\x25\x23\x3F\x23\x2D\x75\x23\x25\x2E\x26\x67\x61\x46\x47\x04\x0A\x0C\x04\x66\x0F\x0D\x16\x2E\x34\x2A\x20\x5E\x57\x3F\x31\x33\x20\x23\x38\x35\x5E\x7E\x58\x4A\x28\x07\x07\x1A\x1D\x03\x01\x42\x33\x01\x0F\x03\x0B\x44\x5B\x7A\x1D\x31\x35\x3B\x7F\x14\x34\x21\x27\x3F\x23\x2F"
"\x6D\x74\x34\x3F\x3F\x27\x24\x2F\x3B\x25\x2E\x62\x2B\x2F\x34\x34\x2B\x3D\x65\x19\x14\x08\x10\x1B\x0C\x5C\x12\x14\x53\x09\x1E\x03\x05\x54\x05\x0F\x19\x1B\x06\x00\x0E\x00\x67\x42\x43\x06\x0E\x0A\x03\x01\x17\x29\x7B\x2C\x36\x7E\x3E\x32\x7D\x37\x2B\x24\x34\x24\x39\x35\x39\x6A\x2F\x3A\x20\x38\x2A\x6C\x22"
"\x30\x63\x2E\x24\x32\x30\x2B\x37\x11\x5B\x0B\x11\x1F\x0D\x19\x53\x78\x5E\x50\x23\x13\x04\x00\x1A\x18\x0E\x52\x49\x28\x06\x00\x08\x42\x2B\x09\x12\x12\x08\x16\x1C\x7A\x65\x78\x0B\x3B\x2C\x28\x32\x20\x36\x70\x21\x33\x25\x27\x3A\x24\x2A\x24\x69\x28\x26\x20\x28\x31\x63\x68\x37\x23\x35\x37\x2C\x15\x15\x58"
"\x09\x17\x1C\x17\x18\x00\x5A\x5E\x7B\x5B\x57\x20\x00\x18\x05\x48\x00\x1A\x4F\x03\x03\x42\x01\x05\x07\x09\x15\x01\x45\x23\x34\x2D\x79\x30\x3A\x39\x39\x72\x3A\x24\x70\x5C\x5D\x03\x1C\x04\x0F\x07\x1E\x1D\x6F\x0E\x0C\x01\x08\x15\x11\x66\x6F\x13\x2C\x14\x1F\x17\x0E\x0D\x5F\x4D\x4C\x5B\x79\x5D\x51\x25\x12"
"\x00\x01\x03\x05\x0F\x1A\x4E\x51\x4C\x2C\x01\x00\x0F\x14\x08\x13\x17\x45\x64\x7B\x0F\x30\x30\x3B\x33\x2A\x21\x73\x32\x30\x35\x3C\x21\x25\x70\x6B\x3B\x30\x20\x2C\x6C\x3E\x27\x37\x34\x28\x28\x20\x37\x69\x5A\x1A\x08\x09\x0D\x5F\x1D\x13\x16\x79\x50\x51\x15\x05\x11\x11\x0F\x05\x1C\x00\x0F\x03\x1F\x4D\x16"
"\x0C\x40\x18\x09\x12\x16\x45\x17\x32\x3B\x2B\x31\x2C\x33\x3B\x26\x73\x31\x32\x35\x38\x21\x3B\x3E\x65\x42\x64\x6E\x1D\x29\x3E\x36\x2C\x32\x24\x66\x28\x2A\x65\x1B\x5B\x16\x1C\x09\x5F\x2C\x3E\x48\x53\x03\x18\x11\x19\x54\x1C\x04\x4B\x1F\x00\x1A\x07\x4C\x19\x0A\x06\x40\x12\x07\x0A\x01\x45\x3B\x38\x3B\x36"
"\x2B\x31\x28\x7D\x36\x26\x22\x38\x38\x30\x74\x26\x2F\x3F\x3D\x39\x60\x45\x46\x1E\x1B\x10\x14\x04\x0B\x67\x0D\x08\x3B\x3C\x3D\x59\x56\x28\x34\x32\x3E\x36\x50\x35\x3F\x24\x3F\x5C\x60\x46\x48\x2A\x01\x01\x18\x1F\x0D\x0F\x40\x31\x07\x09\x01\x09\x7A\x65\x78\x1B\x3F\x3C\x37\x28\x22\x73\x31\x3F\x32\x77\x06"
"\x30\x39\x3F\x27\x3B\x2B\x6F\x64\x1A\x2B\x2D\x24\x2E\x31\x34\x64\x72\x53\x5B\x46\x59\x3D\x0D\x19\x1C\x06\x16\x50\x10\x56\x04\x0D\x06\x1E\x0E\x05\x49\x07\x02\x0D\x0A\x07\x59\x6A\x41\x46\x06\x44\x03\x2F\x37\x34\x79\x2D\x31\x3D\x2D\x21\x3B\x3F\x25\x76\x38\x32\x75\x3E\x23\x2D\x69\x3D\x36\x3F\x39\x27\x2E"
"\x60\x25\x34\x2E\x32\x20\x56\x5B\x0A\x1C\x0D\x0B\x13\x0F\x13\x11\x1C\x14\x56\x00\x1D\x01\x02\x04\x1D\x1D\x4E\x1D\x09\x04\x0C\x10\x14\x00\x0A\x0B\x0D\x0B\x3D\x75\x52\x74\x7E\x11\x39\x38\x36\x20\x70\x30\x76\x33\x26\x3C\x3C\x2E\x68\x2B\x27\x28\x6C\x28\x2C\x2C\x35\x26\x2E\x67\x22\x2A\x08\x5B\x0C\x11\x1B"
"\x5F\x0B\x15\x1D\x1F\x15\x51\x05\x0E\x07\x01\x0F\x06\x48\x0D\x07\x1C\x07\x43\x68\x4E\x40\x35\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E\x31\x67\x72\x74\x23\x28\x25\x23\x31\x38\x67\x22\x25\x28\x29\x2A\x6B\x63\x48\x49\x12\x04\x15\x13\x0B\x17\x3F\x5B\x28\x36\x37\x31\x28\x2E\x52\x5B\x23\x28\x25\x23\x31\x38"
"\x4A\x24\x26\x25\x37\x46\x66\x40\x42\x30\x19\x12\x12\x02\x09\x45\x08\x3E\x2B\x2D\x31\x2D\x39\x7D\x21\x3D\x31\x21\x25\x3F\x3B\x21\x39\x6B\x3B\x30\x3D\x3B\x29\x20\x62\x25\x29\x2D\x23\x34\x64\x24\x14\x1F\x58\x0D\x16\x1A\x5C\x0F\x17\x14\x19\x02\x02\x05\x0D\x55\x47\x4B\x06\x06\x1A\x4F\x15\x02\x17\x11\x6A"
"\x41\x46\x03\x0B\x06\x2F\x36\x3D\x37\x2A\x2C\x72\x7D\x13\x26\x24\x3E\x7B\x34\x26\x30\x2B\x3F\x2D\x2D\x6E\x2D\x29\x2B\x2D\x31\x25\x61\x33\x37\x20\x24\x0E\x1E\x0B\x42\x5E\x1E\x10\x0E\x1D\x53\x13\x03\x13\x16\x00\x10\x4A\x06\x09\x07\x1B\x0E\x00\x01\x1B\x69\x40\x41\x4E\x13\x0C\x0C\x29\x7B\x28\x2B\x31\x38"
"\x2E\x3C\x3F\x69\x70\x76\x25\x2E\x27\x21\x2F\x26\x65\x3B\x2B\x3C\x38\x22\x30\x26\x6D\x22\x34\x22\x25\x31\x1F\x5C\x51\x57\x74\x75\x2E\x38\x35\x3A\x23\x25\x24\x2E\x54\x37\x2B\x28\x23\x3C\x3E\x3C\x66\x40\x42\x37\x08\x08\x15\x47\x14\x17\x35\x3C\x2A\x38\x33\x7F\x3F\x3C\x3C\x73\x35\x29\x26\x38\x26\x21\x6A"
"\x3F\x20\x2C\x6E\x29\x39\x21\x2E\x63\x32\x24\x21\x2E\x37\x31\x08\x02\x58\x51\x0C\x1A\x1B\x14\x01\x07\x02\x08\x5B\x15\x15\x16\x01\x1E\x18\x40\x4E\x1B\x03\x4D\x03\x43\x4E\x13\x03\x00\x6E\x45\x7A\x3D\x31\x35\x3B\x7F\x33\x33\x72\x2A\x3F\x24\x24\x77\x30\x30\x39\x20\x3C\x26\x3E\x6F\x61\x6D\x2A\x22\x2E\x25"
"\x3F\x67\x26\x20\x1C\x14\x0A\x1C\x5E\x1A\x04\x0D\x17\x01\x19\x1C\x13\x19\x00\x06\x44\x61\x62\x3B\x2B\x2C\x23\x3B\x27\x31\x39\x41\x22\x35\x2D\x33\x1F\x7B\x77\x79\x0B\x0C\x1E\x57\x7F\x73\x03\x34\x22\x23\x3D\x3B\x2D\x38\x68\x77\x6E\x1C\x35\x3E\x36\x26\x2D\x61\x78\x67\x16\x20\x19\x14\x0E\x1C\x0C\x06\x5C"
"\x43\x52\x32\x14\x07\x17\x19\x17\x10\x0E\x4B\x1B\x1D\x0F\x1D\x18\x18\x12\x43\x5E\x41\x25\x15\x01\x04\x2E\x3E\x78\x38\x7E\x2D\x39\x3E\x3D\x25\x35\x23\x2F\x5D\x74\x75\x2E\x39\x21\x3F\x2B\x6F\x64\x22\x30\x63\x34\x29\x2F\x34\x64\x35\x08\x14\x1F\x0B\x1F\x12\x46\x5D\x55\x01\x15\x12\x19\x01\x11\x07\x13\x46"
"\x0C\x1B\x07\x19\x09\x4A\x4B\x59\x40\x00\x46\x05\x0B\x0A\x2E\x3A\x3A\x35\x3B\x7F\x09\x0E\x10\x73\x27\x38\x22\x3F\x74\x21\x25\x24\x24\x3A\x6E\x3B\x23\x47\x62\x63\x32\x24\x36\x26\x2D\x37\x56\x5B\x0A\x1C\x0D\x1A\x08\x5D\x1D\x01\x50\x03\x13\x1E\x1A\x06\x1E\x0A\x04\x05\x4E\x38\x05\x03\x06\x0C\x17\x12\x48"
"\x47\x29\x04\x31\x3E\x78\x36\x30\x3A\x7C\x29\x3D\x37\x31\x28\x77\x5D\x5E\x01\x02\x02\x1A\x0D\x63\x1F\x0D\x1F\x16\x1A\x60\x15\x09\x08\x08\x16\x70\x56\x58\x34\x11\x0C\x08\x5D\x10\x12\x13\x1A\x03\x07\x54\x06\x1F\x02\x1C\x0C\x1D\x4F\x44\x2C\x01\x11\x0F\x0F\x0F\x14\x48\x45\x17\x3A\x3B\x2B\x37\x2A\x31\x71"
"\x72\x05\x35\x34\x37\x3A\x7A\x7B\x64\x62\x68\x2D\x21\x6F\x25\x20\x23\x24\x25\x61\x6D\x67\x22\x2C\x16\x1E\x58\x1B\x1F\x1C\x17\x08\x02\x48\x7A\x51\x56\x03\x1C\x10\x13\x4B\x1F\x06\x1C\x04\x4C\x0C\x0E\x0C\x0E\x06\x15\x0E\x00\x00\x7A\x2F\x30\x3C\x7E\x3D\x29\x34\x3E\x27\x7D\x38\x38\x77\x20\x3A\x25\x27\x3B"
"\x67\x6E\x18\x24\x2C\x36\x26\x36\x24\x34\x67\x3D\x2A\x0F\x5B\x0D\x0A\x1B\x5F\x51\x5D\x26\x36\x23\x25\x56\x16\x7E\x55\x4A\x19\x0D\x1A\x1A\x00\x1E\x08\x42\x0D\x0F\x16\x46\x06\x0A\x01\x7A\x2F\x30\x3C\x30\x71\x56\x57\x05\x1B\x11\x05\x76\x03\x1B\x75\x08\x0A\x0B\x02\x6E\x1A\x1C\x47\x06\x2C\x23\x34\x2B\x22"
"\x2A\x31\x09\x57\x58\x29\x17\x1C\x08\x08\x00\x16\x03\x5D\x56\x3A\x01\x06\x03\x08\x44\x49\x38\x06\x08\x08\x0D\x10\x4C\x41\x22\x08\x13\x0B\x36\x34\x39\x3D\x2D\x73\x7C\x19\x37\x20\x3B\x25\x39\x27\x78\x75\x28\x39\x27\x3E\x3D\x2A\x3E\x47\x32\x22\x33\x32\x31\x28\x36\x21\x09\x54\x1A\x16\x11\x14\x11\x1C\x00"
"\x18\x03\x5D\x56\x1A\x15\x1C\x06\x47\x48\x0E\x0F\x02\x09\x4D\x11\x02\x16\x04\x15\x4B\x44\x15\x28\x34\x32\x3C\x3D\x2B\x7C\x3B\x3D\x3F\x34\x34\x24\x24\x78\x75\x2B\x25\x2C\x69\x2F\x21\x35\x39\x2A\x2A\x2E\x26\x66\x2E\x2A\x4F\x3B\x0B\x08\x3D\x1F\x0B\x1D\x5D\x0B\x1C\x05\x51\x15\x16\x06\x10\x4A\x0A\x0A\x06"
"\x1B\x1B\x42\x4D\x45\x37\x08\x08\x15\x47\x14\x17\x35\x3C\x2A\x38\x33\x65\x7C\x3B\x3B\x3F\x35\x7C\x35\x38\x24\x2C\x6A\x64\x68\x33\x27\x3F\x61\x2E\x30\x26\x21\x35\x23\x60\x64\x26\x1B\x15\x58\x18\x0C\x1C\x14\x14\x04\x16\x7A\x17\x19\x1B\x10\x10\x18\x18\x48\x0F\x1C\x00\x01\x4D\x16\x0B\x05\x41\x05\x08\x09"
"\x08\x3B\x35\x3C\x79\x32\x36\x32\x38\x7C"
);

MAN(recovery, "Recovery & Repair Guide",
"\x0E\x09\x01\x79\x0A\x17\x19\x7D\x01\x1A\x1D\x01\x1A\x12\x74\x01\x02\x02\x06\x0E\x1D\x6F\x0A\x04\x10\x10\x14\x4B\x77\x6E\x64\x17\x1F\x08\x0C\x18\x0C\x0B\x52\x5D\x40\x5A\x50\x26\x1F\x19\x10\x1A\x1D\x18\x48\x3C\x1E\x0B\x0D\x19\x07\x43\x14\x13\x09\x12\x06\x09\x3F\x28\x30\x36\x31\x2B\x39\x2F\x21\x7D\x70"
"\x62\x7F\x77\x27\x33\x29\x6B\x67\x3A\x2D\x2E\x22\x23\x2D\x34\x60\x20\x28\x23\x4E\x21\x13\x08\x15\x59\x51\x10\x12\x11\x1B\x1D\x15\x51\x59\x14\x18\x10\x0B\x05\x1D\x19\x43\x06\x01\x0C\x05\x06\x40\x4E\x14\x02\x17\x11\x35\x29\x3D\x31\x3B\x3E\x30\x29\x3A\x73\x78\x25\x3E\x3E\x27\x75\x3A\x39\x27\x2E\x3C\x2E"
"\x21\x77\x62\x64\x33\x27\x25\x6A\x37\x26\x1B\x15\x16\x16\x09\x58\x50\x77\x55\x17\x19\x02\x1B\x5A\x17\x1D\x0F\x08\x03\x4E\x42\x4F\x4B\x09\x0B\x10\x0D\x4C\x14\x02\x17\x11\x35\x29\x3D\x7E\x77\x71\x7C\x69\x7B\x73\x03\x28\x25\x23\x31\x38\x6A\x19\x2D\x3A\x3A\x20\x3E\x28\x6C\x63\x0F\x2F\x2A\x3E\x64\x31\x12"
"\x1E\x16\x59\x13\x10\x0A\x18\x52\x1C\x1E\x5F\x7C\x7D\x26\x30\x39\x2E\x3C\x49\x3A\x27\x25\x3E\x42\x33\x23\x41\x4E\x35\x21\x2C\x14\x08\x0C\x18\x12\x13\x7C\x0A\x1B\x07\x18\x1E\x03\x03\x74\x18\x0F\x0F\x01\x08\x67\x45\x1F\x28\x36\x37\x29\x2F\x21\x34\x64\x7B\x5A\x28\x01\x0A\x0A\x1A\x11\x5D\x4C\x53\x22\x14"
"\x15\x18\x02\x10\x18\x12\x48\x57\x4E\x3D\x09\x1E\x07\x17\x40\x15\x0E\x0E\x17\x45\x0A\x18\x62\x53\x73\x7F\x7B\x16\x37\x36\x20\x71\x3B\x2E\x74\x33\x23\x27\x2D\x3A\x69\x6F\x3E\x28\x6F\x2A\x2E\x32\x32\x26\x28\x29\x09\x5B\x2F\x10\x10\x1B\x13\x0A\x01\x5F\x50\x1A\x13\x12\x04\x06\x4A\x1B\x0D\x1B\x1D\x00\x02"
"\x0C\x0E\x43\x06\x08\x0A\x02\x17\x45\x3B\x35\x3C\x79\x33\x30\x2F\x29\x58\x73\x70\x22\x33\x23\x20\x3C\x24\x2C\x3B\x65\x6E\x3D\x29\x20\x2D\x35\x25\x32\x66\x26\x34\x35\x09\x55\x58\x5E\x2C\x1A\x11\x12\x04\x16\x50\x14\x00\x12\x06\x0C\x1E\x03\x01\x07\x09\x48\x4C\x04\x11\x43\x01\x41\x00\x12\x08\x09\x7A\x2C"
"\x31\x29\x3B\x7F\x74\x28\x21\x36\x36\x24\x3A\x5D\x74\x75\x28\x2E\x2E\x26\x3C\x2A\x6C\x3E\x27\x2F\x2C\x28\x28\x20\x64\x24\x5A\x2B\x3B\x59\x53\x5F\x1F\x15\x1D\x1C\x03\x14\x56\x50\x17\x19\x0F\x0A\x06\x49\x0A\x1D\x05\x1B\x07\x10\x47\x41\x12\x08\x0B\x4C\x74\x51\x75\x79\x79\x1C\x30\x32\x27\x37\x70\x35\x39"
"\x20\x3A\x39\x25\x2A\x2C\x6E\x6E\x29\x29\x39\x21\x2B\x25\x32\x66\x26\x64\x23\x08\x1E\x0B\x11\x5E\x28\x15\x13\x16\x1C\x07\x02\x56\x1E\x19\x14\x0D\x0E\x53\x49\x49\x23\x03\x0E\x03\x0F\x40\x13\x03\x0E\x0A\x16\x2E\x3A\x34\x35\x79\x7F\x29\x2E\x37\x20\x5A\x71\x76\x23\x3C\x30\x6A\x24\x26\x2C\x6E\x2E\x20\x3F"
"\x27\x22\x24\x38\x66\x28\x2A\x65\x0E\x13\x1D\x59\x1A\x16\x0F\x16\x5C\x53\x24\x19\x1F\x04\x54\x05\x18\x04\x0F\x1B\x0F\x02\x56\x4D\x45\x11\x05\x12\x03\x13\x49\x15\x39\x7C\x76\x53\x54\x1E\x18\x0B\x13\x1D\x13\x14\x12\x77\x07\x01\x0B\x19\x1C\x1C\x1E\x6F\x64\x0F\x0D\x0C\x14\x61\x0B\x02\x0A\x10\x53\x71\x2B"
"\x1C\x0A\x0B\x15\x13\x15\x00\x50\x4F\x56\x24\x0D\x06\x1E\x0E\x05\x49\x50\x4F\x3E\x08\x01\x0C\x16\x04\x14\x1E\x44\x5B\x7A\x1A\x3C\x2F\x3F\x31\x3F\x38\x36\x73\x23\x25\x37\x25\x20\x20\x3A\x6B\x76\x69\x1C\x2A\x3F\x39\x23\x31\x34\x61\x28\x28\x33\x65\x57\x5B\x17\x0B\x5E\x17\x13\x11\x16\x79\x23\x19\x1F\x11"
"\x00\x55\x1D\x03\x01\x05\x0B\x4F\x0F\x01\x0B\x00\x0B\x08\x08\x00\x44\x37\x3F\x28\x2C\x38\x2C\x2B\x72\x7D\x1D\x23\x24\x38\x39\x39\x27\x75\x3E\x23\x2D\x3B\x2B\x75\x46\x60\x62\x10\x34\x20\x34\x33\x31\x35\x5A\x29\x1D\x09\x1F\x16\x0E\x47\x52\x15\x19\x09\x13\x04\x54\x17\x05\x04\x1C\x49\x1E\x1D\x03\x0F\x0E"
"\x06\x0D\x12\x46\x06\x11\x11\x35\x36\x39\x2D\x37\x3C\x3D\x31\x3E\x2A\x7E\x5B\x7B\x77\x07\x21\x2B\x39\x3C\x3C\x3E\x6F\x1F\x28\x36\x37\x29\x2F\x21\x34\x7E\x65\x09\x1A\x1E\x1C\x5E\x12\x13\x19\x17\x5F\x50\x1D\x19\x00\x59\x07\x0F\x18\x07\x05\x1B\x1B\x05\x02\x0C\x4F\x40\x0D\x09\x00\x03\x0C\x34\x3C\x76\x77"
"\x70\x55\x71\x7D\x11\x3C\x3D\x3C\x37\x39\x30\x75\x1A\x39\x27\x24\x3E\x3B\x76\x6D\x23\x27\x36\x20\x28\x24\x21\x21\x5A\x09\x1D\x09\x1F\x16\x0E\x5D\x11\x1C\x1D\x1C\x17\x19\x10\x06\x44\x61\x45\x49\x3B\x2A\x2A\x24\x42\x25\x09\x13\x0B\x10\x05\x17\x3F\x7B\x0B\x3C\x2A\x2B\x35\x33\x35\x20\x6A\x71\x33\x39\x20"
"\x30\x38\x6B\x3C\x21\x2B\x6F\x0E\x04\x0D\x10\x6F\x14\x03\x01\x0D\x6B\x70\x56\x58\x2A\x07\x0C\x08\x18\x1F\x53\x39\x1C\x17\x10\x11\x55\x38\x0E\x0B\x06\x18\x0A\x1E\x14\x58\x43\x12\x04\x15\x13\x0B\x17\x3F\x7B\x39\x79\x2D\x26\x2F\x29\x37\x3E\x70\x38\x3B\x36\x33\x30\x6A\x29\x29\x2A\x25\x3A\x3C\x63\x48\x6E"
"\x60\x14\x28\x2E\x2A\x36\x0E\x1A\x14\x15\x5E\x2A\x0C\x19\x13\x07\x15\x02\x4C\x57\x06\x10\x07\x04\x1E\x0C\x4E\x0E\x4C\x0F\x10\x06\x01\x0A\x0F\x09\x03\x45\x2F\x2B\x3C\x38\x2A\x3A\x72\x57\x58\x00\x11\x17\x13\x77\x19\x1A\x0E\x0E\x42\x1A\x2F\x29\x29\x6D\x2F\x2C\x24\x24\x66\x2B\x2B\x24\x1E\x08\x58\x16\x10"
"\x13\x05\x5D\x17\x00\x03\x14\x18\x03\x1D\x14\x06\x4B\x0C\x1B\x07\x19\x09\x1F\x11\x43\x4D\x41\x16\x02\x16\x03\x3F\x38\x2C\x79\x38\x30\x2E\x7D\x20\x36\x3D\x3E\x20\x3E\x3A\x32\x6A\x29\x29\x2D\x6E\x2B\x3E\x24\x34\x26\x32\x32\x4C\x28\x36\x65\x17\x1A\x14\x0E\x1F\x0D\x19\x47\x78\x5E\x50\x33\x19\x18\x00\x55"
"\x0C\x19\x07\x04\x4E\x2E\x08\x1B\x03\x0D\x03\x04\x02\x47\x17\x11\x3B\x29\x2C\x2C\x2E\x7F\x62\x7D\x01\x27\x31\x23\x22\x22\x24\x75\x19\x2E\x3C\x3D\x27\x21\x2B\x3E\x62\x7D\x60\x13\x23\x34\x30\x24\x08\x0F\x58\x47\x5E\x0F\x0E\x18\x01\x00\x50\x45\x59\x42\x5A\x7F\x47\x4B\x27\x1B\x4E\x02\x1F\x0E\x0D\x0D\x06"
"\x08\x01\x47\x5A\x45\x18\x34\x37\x2D\x7E\x2B\x3D\x3F\x72\x6D\x70\x02\x37\x31\x31\x75\x28\x24\x27\x3D\x6E\x71\x6C\x00\x2B\x2D\x29\x2C\x27\x2B\x64\x6D\x0F\x15\x1B\x11\x1B\x1C\x17\x5D\x13\x15\x04\x14\x04\x00\x15\x07\x0E\x18\x49\x40\x40\x65\x41\x4D\x2D\x11\x40\x15\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E"
"\x31\x7A\x21\x73\x77\x22\x37\x31\x31\x78\x27\x24\x2C\x2C\x69\x6F\x2F\x22\x2F\x2E\x21\x2F\x22\x67\x6C\x37\x1F\x08\x0C\x18\x0C\x0B\x5C\x0F\x17\x02\x05\x18\x04\x12\x10\x5C\x44\x61\x62\x3A\x37\x3C\x38\x28\x2F\x43\x32\x24\x35\x33\x2B\x37\x1F\x51\x75\x79\x0C\x3A\x2F\x29\x3D\x21\x35\x71\x06\x14\x74\x21\x25"
"\x6B\x29\x27\x6E\x2A\x2D\x3F\x2E\x2A\x25\x33\x66\x34\x30\x24\x0E\x1E\x58\x51\x0D\x06\x0F\x09\x17\x1E\x50\x17\x1F\x1B\x11\x06\x4A\x40\x48\x1B\x0B\x08\x05\x1E\x16\x11\x19\x41\x09\x09\x08\x1C\x73\x75\x52\x74\x7E\x0D\x29\x33\x72\x35\x22\x3E\x3B\x6D\x74\x16\x25\x25\x3C\x3B\x21\x23\x6C\x1D\x23\x2D\x25\x2D"
"\x66\x79\x64\x17\x1F\x18\x17\x0F\x1B\x0D\x05\x5D\x4C\x53\x3F\x01\x13\x19\x54\x26\x13\x18\x1C\x0C\x03\x4F\x3E\x08\x11\x17\x0F\x13\x03\x4B\x44\x0A\x28\x7B\x19\x3D\x28\x3E\x32\x3E\x37\x37\x5A\x71\x76\x24\x20\x34\x38\x3F\x3D\x39\x6E\x71\x6C\x19\x30\x2C\x35\x23\x2A\x22\x37\x2D\x15\x14\x0C\x59\x40\x5F\x3D"
"\x19\x04\x12\x1E\x12\x13\x13\x54\x4B\x4A\x38\x11\x1A\x1A\x0A\x01\x4D\x30\x06\x13\x15\x09\x15\x01\x4B\x50\x76\x78\x0D\x36\x36\x2F\x7D\x22\x21\x3F\x36\x24\x36\x39\x6F\x6A\x6C\x3B\x30\x3D\x3B\x29\x20\x6F\x31\x25\x32\x32\x28\x36\x20\x5D\x57\x58\x5E\x0D\x06\x0F\x09\x17\x1E\x5D\x03\x13\x04\x00\x1A\x18\x0E"
"\x45\x0A\x1C\x0A\x0D\x19\x07\x44\x4E\x6B\x6C\x35\x21\x26\x15\x0D\x1D\x0B\x07\x7F\x18\x0F\x1B\x05\x15\x71\x79\x77\x1D\x1B\x19\x1F\x09\x05\x02\x6F\x01\x08\x06\x0A\x01\x4B\x6B\x67\x0D\x23\x5A\x2C\x11\x17\x1A\x10\x0B\x0E\x52\x10\x11\x1F\x18\x18\x00\x55\x08\x04\x07\x1D\x4E\x0E\x18\x4D\x03\x0F\x0C\x5B\x46"
"\x05\x0B\x0A\x2E\x7B\x3E\x2B\x31\x32\x7C\x3C\x72\x21\x35\x32\x39\x21\x31\x27\x33\x6B\x2C\x3B\x27\x39\x29\x6D\x2D\x31\x60\x14\x15\x05\x4E\x65\x5A\x12\x16\x0A\x0A\x1E\x10\x11\x52\x1E\x15\x15\x1F\x16\x54\x14\x04\x0F\x48\x19\x07\x0C\x07\x4D\x45\x31\x05\x11\x07\x0E\x16\x45\x23\x34\x2D\x2B\x7E\x3C\x33\x30"
"\x22\x26\x24\x34\x24\x70\x7A\x5F\x67\x6B\x0B\x3B\x2B\x2E\x38\x28\x62\x2C\x2E\x24\x7C\x67\x30\x2D\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x50\x56\x04\x12\x17\x1A\x1C\x0E\x1A\x10\x43\x0B\x1E\x04\x14\x06\x47\x5A\x46\x2A\x0D\x06\x28\x34\x2B\x36\x38\x2B\x7B\x2E\x72\x1E\x35\x35\x3F\x36\x74\x16\x38\x2E\x29"
"\x3D\x27\x20\x22\x47\x62\x63\x14\x2E\x29\x2B\x64\x28\x1B\x10\x1D\x0A\x5E\x16\x12\x0E\x06\x12\x1C\x1D\x56\x22\x27\x37\x19\x45\x62\x63\x2D\x20\x21\x20\x23\x2D\x24\x4C\x2A\x2E\x2A\x20\x7A\x09\x1D\x09\x1F\x16\x0E\x0E\x72\x7B\x36\x23\x39\x3A\x74\x21\x22\x2E\x68\x2B\x21\x20\x38\x6D\x2F\x26\x2E\x34\x66\x04"
"\x2B\x28\x17\x1A\x16\x1D\x5E\x2F\x0E\x12\x1F\x03\x04\x58\x7C\x5A\x54\x17\x05\x04\x1C\x1B\x0B\x0C\x4C\x42\x04\x0A\x18\x0C\x04\x15\x48\x45\x75\x3D\x31\x21\x3C\x30\x33\x29\x7E\x73\x7F\x23\x33\x35\x21\x3C\x26\x2F\x2A\x2A\x2A\x6F\x6C\x6D\x04\x2A\x38\x61\x24\x28\x2B\x31\x5A\x09\x1D\x1A\x11\x0D\x18\x0E\x5C"
"\x79\x5D\x51\x05\x11\x17\x55\x45\x18\x0B\x08\x00\x01\x03\x1A\x42\x45\x40\x05\x0F\x14\x09\x45\x74\x75\x76\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x1E\x28\x32\x22\x29\x33\x66\x34\x3D\x36\x0E\x1E\x15\x59\x18\x16\x10\x18\x01\x5D\x7A\x5C\x56\x34\x1B\x05\x13\x4B\x1C\x01"
"\x0B\x4F\x1E\x08\x05\x0A\x13\x15\x14\x1E\x44\x0D\x33\x2D\x3D\x2A\x7E\x32\x3D\x33\x27\x32\x3C\x3D\x2F\x77\x32\x27\x25\x26\x68\x1E\x27\x21\x28\x22\x35\x30\x1C\x12\x3F\x34\x30\x20\x17\x48\x4A\x25\x1D\x10\x12\x1B\x1B\x14\x2C\x23\x13\x10\x36\x14\x09\x00\x62\x49\x4E\x47\x3B\x04\x0C\x07\x0F\x16\x15\x47\x55"
"\x55\x73\x7B\x31\x3F\x7E\x2B\x34\x38\x72\x21\x35\x36\x3F\x24\x20\x27\x33\x6B\x21\x3A\x6E\x2B\x2D\x20\x23\x24\x25\x25\x68\x4D\x4E\x07\x36\x2E\x3D\x59\x2D\x3C\x2E\x38\x37\x3D\x50\x59\x34\x24\x3B\x31\x43\x61\x45\x49\x20\x00\x18\x08\x42\x17\x08\x04\x46\x14\x10\x0A\x2A\x7B\x3B\x36\x3A\x3A\x7C\x75\x37\x7D"
"\x37\x7F\x76\x14\x06\x1C\x1E\x02\x0B\x08\x02\x10\x1C\x1F\x0D\x00\x05\x12\x15\x18\x00\x0C\x3F\x3F\x54\x59\x33\x3A\x31\x32\x20\x2A\x2F\x3C\x37\x39\x35\x32\x2F\x26\x2D\x27\x3A\x46\x42\x67\x4F\x43\x23\x09\x03\x04\x0F\x45\x1F\x2D\x3D\x37\x2A\x7F\x0A\x34\x37\x24\x35\x23\x7A\x77\x26\x20\x24\x6B\x25\x2C\x23"
"\x20\x3E\x34\x62\x27\x29\x20\x21\x29\x2B\x36\x0E\x12\x1B\x0A\x5E\x1E\x12\x19\x52\x17\x19\x02\x1D\x57\x17\x1D\x0F\x08\x03\x1A\x42\x4F\x1E\x08\x0F\x0C\x16\x04\x6C\x47\x44\x17\x3F\x38\x3D\x37\x2A\x33\x25\x7D\x3B\x3D\x23\x25\x37\x3B\x38\x30\x2E\x6B\x20\x28\x3C\x2B\x3B\x2C\x30\x26\x6F\x25\x34\x2E\x32\x20"
"\x08\x08\x54\x59\x1D\x17\x19\x1E\x19\x53\x04\x14\x1B\x07\x11\x07\x0B\x1F\x1D\x1B\x0B\x1C\x42\x67\x4F\x43\x33\x04\x07\x15\x07\x0D\x7A\x2F\x30\x3C\x7E\x3C\x33\x39\x37\x73\x3F\x3F\x76\x23\x3C\x30\x6A\x24\x2E\x2F\x27\x2C\x25\x2C\x2E\x63\x24\x2E\x25\x34\x7E\x65\x5D\x1F\x17\x1A\x0D\x52\x0F\x18\x13\x01\x13"
"\x19\x56\x4B\x17\x1A\x0E\x0E\x56\x4E\x40\x65\x41\x4D\x36\x0B\x09\x12\x46\x17\x16\x0A\x3D\x29\x39\x34\x64\x7F\x7B\x30\x37\x3E\x3F\x23\x2F\x7A\x30\x3C\x2B\x2C\x26\x26\x3D\x3B\x25\x2E\x65\x6F\x60\x66\x22\x2E\x37\x2E\x57\x18\x10\x1C\x1D\x14\x5B\x51\x52\x54\x15\x07\x13\x19\x00\x58\x1C\x02\x0D\x1E\x0B\x1D"
"\x4B\x43\x68\x69\x22\x28\x32\x2B\x2B\x26\x11\x1E\x0A\x79\x0C\x1A\x1F\x12\x04\x16\x02\x08\x5C\x7A\x74\x1C\x2C\x6B\x0A\x20\x3A\x03\x23\x2E\x29\x26\x32\x61\x27\x34\x2F\x36\x5A\x1D\x17\x0B\x5E\x1E\x5C\x0F\x17\x10\x1F\x07\x13\x05\x0D\x55\x01\x0E\x11\x53\x4E\x09\x05\x03\x06\x43\x09\x15\x46\x0E\x0A\x45\x23"
"\x34\x2D\x2B\x7E\x12\x35\x3E\x20\x3C\x23\x3E\x30\x23\x74\x34\x29\x28\x27\x3C\x20\x3B\x46\x6D\x62\x6B\x21\x22\x25\x28\x31\x2B\x0E\x55\x15\x10\x1D\x0D\x13\x0E\x1D\x15\x04\x5F\x15\x18\x19\x5A\x0E\x0E\x1E\x00\x0D\x0A\x1F\x42\x10\x06\x03\x0E\x10\x02\x16\x1C\x31\x3E\x21\x70\x7E\x30\x2E\x7D\x25\x3B\x35\x23"
"\x33\x77\x2D\x3A\x3F\x6B\x3B\x28\x38\x2A\x28\x6D\x2B\x37\x60\x36\x2E\x22\x2A\x4F\x5A\x5B\x1D\x17\x1F\x1D\x10\x14\x1C\x14\x50\x33\x1F\x03\x38\x1A\x09\x00\x0D\x1B\x40\x4F\x22\x08\x14\x06\x12\x41\x0A\x08\x17\x00\x7A\x32\x2C\x78\x54\x55\x1F\x11\x17\x12\x1E\x71\x1F\x19\x07\x01\x0B\x07\x04\x69\x66\x01\x19"
"\x0E\x0E\x06\x01\x13\x66\x08\x14\x11\x33\x34\x36\x50\x74\x52\x5C\x3F\x13\x10\x1B\x51\x03\x07\x54\x13\x03\x19\x1B\x1D\x4F\x4F\x38\x05\x07\x0D\x40\x13\x03\x0E\x0A\x16\x2E\x3A\x34\x35\x7E\x39\x2E\x32\x3F\x73\x05\x02\x14\x77\x79\x75\x39\x2E\x2D\x69\x23\x2E\x22\x38\x23\x2F\x6D\x22\x2A\x22\x25\x2B\x57\x12"
"\x16\x0A\x0A\x1E\x10\x11\x5C"
);

MAN(storage, "Storage & Disks Guide",
"\x1E\x09\x11\x0F\x1B\x0C\x70\x7D\x02\x12\x02\x05\x1F\x03\x1D\x1A\x04\x18\x68\x6F\x6E\x19\x03\x01\x17\x0E\x05\x12\x4C\x6A\x64\x04\x5A\x0B\x10\x00\x0D\x16\x1F\x1C\x1E\x53\x14\x18\x05\x1C\x54\x16\x0B\x05\x48\x0A\x01\x01\x18\x0C\x0B\x0D\x40\x12\x03\x11\x01\x17\x3B\x37\x78\x29\x3F\x2D\x28\x34\x26\x3A\x3F"
"\x3F\x25\x77\x7C\x26\x33\x38\x3C\x2C\x23\x63\x6C\x3F\x27\x20\x2F\x37\x23\x35\x3D\x69\x70\x5B\x58\x1D\x1F\x0B\x1D\x53\x5C\x5D\x59\x5F\x56\x32\x15\x16\x02\x4B\x18\x08\x1C\x1B\x05\x19\x0B\x0C\x0E\x41\x11\x0E\x10\x0D\x7A\x3A\x78\x3D\x2C\x36\x2A\x38\x72\x3F\x35\x25\x22\x32\x26\x75\x23\x38\x68\x28\x6E\x68"
"\x3A\x22\x2E\x36\x2D\x24\x61\x69\x4E\x68\x5A\x3F\x11\x0A\x15\x5F\x31\x1C\x1C\x12\x17\x14\x1B\x12\x1A\x01\x4A\x43\x1C\x01\x07\x1C\x4C\x1D\x10\x0C\x07\x13\x07\x0A\x5E\x45\x7D\x3F\x31\x2A\x35\x72\x31\x3C\x3C\x32\x37\x34\x3B\x32\x3A\x21\x6D\x62\x68\x3A\x26\x20\x3B\x3E\x62\x22\x2C\x2D\x66\x23\x2D\x36\x11"
"\x08\x54\x73\x5E\x5F\x0C\x1C\x00\x07\x19\x05\x1F\x18\x1A\x06\x4A\x0A\x06\x0D\x4E\x0B\x1E\x04\x14\x06\x40\x0D\x03\x13\x10\x00\x28\x28\x76\x79\x0D\x37\x2E\x34\x3C\x38\x7F\x14\x2E\x23\x31\x3B\x2E\x64\x0E\x26\x3C\x22\x2D\x39\x6D\x00\x28\x20\x28\x20\x21\x65\x16\x1E\x0C\x0D\x1B\x0D\x5C\x09\x1A\x16\x02\x14"
"\x58\x7D\x59\x55\x29\x04\x05\x04\x0F\x01\x08\x4D\x0E\x0A\x0E\x04\x5C\x47\x00\x0C\x29\x30\x28\x38\x2C\x2B\x72\x7D\x17\x2B\x31\x3C\x26\x3B\x31\x6F\x6A\x2F\x21\x3A\x25\x3F\x2D\x3F\x36\x63\x7E\x61\x2A\x2E\x37\x31\x5A\x1F\x11\x0A\x15\x5F\x42\x5D\x01\x16\x1C\x14\x15\x03\x54\x11\x03\x18\x03\x49\x5F\x4F\x52"
"\x67\x42\x43\x03\x0D\x03\x06\x0A\x45\x64\x7B\x3B\x2B\x3B\x3E\x28\x38\x72\x23\x31\x23\x22\x3E\x20\x3C\x25\x25\x68\x39\x3C\x26\x21\x2C\x30\x3A\x60\x7F\x66\x21\x2B\x37\x17\x1A\x0C\x59\x18\x0C\x41\x13\x06\x15\x03\x51\x07\x02\x1D\x16\x01\x45\x62\x63\x28\x26\x20\x28\x42\x30\x39\x32\x32\x22\x29\x36\x50\x76"
"\x78\x17\x0A\x19\x0F\x67\x72\x37\x35\x37\x37\x22\x38\x21\x6A\x2D\x27\x3B\x6E\x18\x25\x23\x26\x2C\x37\x32\x68\x67\x14\x20\x08\x16\x11\x0A\x0D\x16\x13\x13\x01\x5F\x50\x14\x18\x14\x06\x0C\x1A\x1F\x01\x06\x00\x4F\x44\x28\x24\x30\x49\x4D\x46\x04\x0B\x08\x2A\x29\x3D\x2A\x2D\x36\x33\x33\x7E\x59\x70\x71\x27"
"\x22\x3B\x21\x2B\x38\x64\x69\x26\x3A\x2B\x28\x62\x25\x29\x2D\x23\x34\x6A\x65\x2F\x08\x1D\x59\x18\x10\x0E\x5D\x01\x0A\x03\x05\x13\x1A\x54\x14\x04\x0F\x48\x0D\x0F\x1B\x0D\x4D\x06\x11\x09\x17\x03\x14\x4A\x6F\x77\x7B\x3D\x21\x18\x1E\x08\x67\x72\x31\x35\x22\x22\x77\x32\x3A\x38\x6B\x1D\x1A\x0C\x6F\x3F\x39"
"\x2B\x20\x2B\x32\x66\x26\x2A\x21\x5A\x16\x1D\x14\x11\x0D\x05\x5D\x11\x12\x02\x15\x05\x57\x07\x1D\x0B\x19\x0D\x0D\x4E\x18\x05\x19\x0A\x43\x0F\x15\x0E\x02\x16\x45\x3E\x3E\x2E\x30\x3D\x3A\x2F\x73\x58\x7E\x70\x17\x17\x03\x67\x67\x70\x6B\x27\x25\x2A\x2A\x3F\x39\x6E\x63\x2D\x20\x3E\x67\x70\x65\x3D\x39\x58"
"\x1F\x17\x13\x19\x5D\x01\x1A\x0A\x14\x56\x5A\x54\x14\x1C\x04\x01\x0D\x4E\x0A\x14\x0E\x07\x13\x14\x41\x00\x08\x16\x45\x2E\x32\x36\x20\x7E\x32\x39\x39\x3B\x32\x7E\x5B\x7B\x77\x06\x30\x0C\x18\x72\x69\x3B\x3C\x29\x29\x62\x21\x39\x61\x02\x22\x32\x65\x3E\x09\x11\x0F\x1B\x5F\x1D\x13\x16\x53\x23\x05\x19\x05"
"\x15\x12\x0F\x4B\x3B\x19\x0F\x0C\x09\x1E\x59\x43\x12\x04\x15\x0E\x08\x0C\x3F\x35\x3B\x3C\x73\x39\x33\x3E\x27\x20\x35\x35\x78\x5D\x5E\x06\x1E\x04\x1A\x08\x09\x0A\x6C\x1E\x07\x0D\x13\x04\x66\x61\x64\x06\x36\x3E\x39\x37\x2B\x2F\x76\x50\x52\x20\x04\x1E\x04\x16\x13\x10\x4A\x38\x0D\x07\x1D\x0A\x4C\x45\x31"
"\x06\x14\x15\x0F\x09\x03\x16\x7A\x65\x78\x0A\x27\x2C\x28\x38\x3F\x73\x6E\x71\x05\x23\x3B\x27\x2B\x2C\x2D\x60\x6E\x2E\x39\x39\x2D\x6E\x24\x24\x2A\x22\x30\x20\x09\x5B\x0C\x1C\x13\x0F\x5C\x1B\x1B\x1F\x15\x02\x56\x16\x1A\x11\x60\x4B\x48\x0C\x03\x1F\x18\x04\x07\x10\x40\x15\x0E\x02\x44\x37\x3F\x38\x21\x3A"
"\x32\x3A\x7C\x1F\x3B\x3D\x70\x3E\x38\x77\x35\x75\x39\x28\x20\x2C\x2A\x3A\x20\x28\x6C\x63\x14\x29\x2F\x34\x64\x35\x08\x14\x1F\x0B\x1F\x12\x46\x5D\x55\x00\x04\x1E\x04\x16\x13\x10\x47\x18\x0D\x07\x1D\x0A\x4B\x43\x68\x4E\x40\x25\x0F\x14\x0F\x45\x19\x37\x3D\x38\x30\x2A\x2C\x7D\x7A\x30\x3C\x34\x37\x39\x39"
"\x32\x38\x62\x68\x66\x6E\x1C\x38\x22\x30\x22\x27\x24\x66\x79\x64\x11\x1F\x16\x08\x16\x0C\x1E\x0E\x04\x52\x15\x19\x1D\x13\x04\x4E\x55\x07\x0A\x06\x1C\x0F\x03\x4C\x0E\x0E\x06\x01\x0F\x13\x17\x48\x6F\x7A\x7B\x31\x37\x3D\x33\x29\x39\x3B\x3D\x37\x71\x71\x00\x3D\x3B\x2E\x24\x3F\x3A\x6E\x1A\x3C\x29\x23\x37"
"\x25\x61\x05\x2B\x21\x24\x14\x0E\x08\x5E\x5E\x57\x1F\x1C\x1C\x53\x16\x03\x13\x12\x54\x18\x0B\x05\x11\x49\x29\x2D\x45\x43\x68\x4E\x40\x35\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E\x31\x67\x72\x74\x34\x38\x25\x3C\x79\x36\x26\x2E\x29\x27\x3B\x3F\x6B\x61\x62\x64\x23\x2D\x23\x26\x2A\x68\x0E\x1E\x15\x09\x59"
"\x53\x5C\x5A\x11\x1F\x15\x10\x18\x5A\x04\x07\x0F\x0D\x0D\x1D\x0D\x07\x4B\x41\x68\x43\x40\x46\x05\x0B\x01\x04\x34\x76\x2C\x31\x2B\x32\x3E\x3E\x33\x30\x38\x34\x71\x7B\x74\x72\x29\x27\x2D\x28\x3C\x62\x3B\x24\x2C\x27\x2F\x36\x35\x6A\x31\x35\x1E\x1A\x0C\x1C\x53\x1C\x1D\x1E\x1A\x16\x57\x5F\x7C\x7D\x30\x30"
"\x2C\x39\x29\x2E\x23\x2A\x22\x39\x42\x45\x40\x35\x34\x2E\x29\x6F\x77\x7B\x10\x1D\x1A\x2C\x7C\x33\x37\x36\x34\x71\x32\x32\x32\x27\x2B\x2C\x25\x2C\x20\x3B\x2D\x39\x2B\x2C\x2E\x7A\x66\x14\x17\x01\x09\x5B\x16\x1C\x1B\x1B\x5C\x29\x20\x3A\x3D\x51\x5E\x16\x01\x01\x05\x06\x09\x1D\x07\x0C\x45\x43\x68\x4E\x40"
"\x2E\x16\x13\x0D\x08\x33\x21\x3D\x79\x1A\x2D\x35\x2B\x37\x20\x70\x79\x32\x31\x26\x32\x3F\x22\x61\x69\x21\x3D\x6C\x39\x2A\x2A\x33\x61\x36\x35\x2B\x22\x08\x1A\x15\x43\x5E\x58\x18\x14\x01\x18\x5D\x15\x13\x11\x06\x14\x0D\x4C\x46\x63\x43\x4F\x22\x08\x14\x06\x12\x41\x02\x02\x02\x17\x3B\x3C\x78\x38\x30\x7F"
"\x0F\x0E\x16\x73\x7D\x71\x3F\x23\x74\x3C\x39\x6B\x3D\x27\x20\x2A\x2F\x28\x31\x30\x21\x33\x3F\x67\x25\x2B\x1E\x5B\x0F\x1C\x1F\x0D\x0F\x5D\x1B\x07\x50\x1E\x03\x03\x54\x5D\x3D\x02\x06\x0D\x01\x18\x1F\x67\x42\x43\x08\x00\x08\x03\x08\x00\x29\x7B\x2C\x31\x37\x2C\x7C\x3C\x27\x27\x3F\x3C\x37\x23\x3D\x36\x2B"
"\x27\x24\x30\x67\x61\x46\x47\x01\x0B\x05\x02\x0D\x0E\x0A\x02\x5A\x3D\x37\x2B\x5E\x3A\x2E\x2F\x3D\x21\x23\x7B\x5B\x57\x17\x1D\x01\x0F\x1B\x02\x4E\x2C\x56\x4D\x4D\x05\x40\x41\x4E\x06\x00\x08\x33\x35\x71\x79\x3D\x37\x39\x3E\x39\x20\x70\x30\x38\x33\x74\x33\x23\x33\x2D\x3A\x6E\x3B\x24\x28\x62\x30\x39\x32"
"\x32\x22\x29\x65\x1E\x09\x11\x0F\x1B\x5F\x13\x13\x52\x1D\x15\x09\x02\x57\x16\x1A\x05\x1F\x46\x63\x43\x4F\x38\x05\x0B\x10\x40\x11\x14\x08\x03\x17\x3B\x36\x62\x79\x79\x3B\x35\x2E\x39\x7E\x33\x39\x33\x34\x3F\x72\x6A\x63\x2B\x21\x25\x2B\x3F\x26\x6B\x63\x21\x2F\x22\x67\x63\x21\x08\x12\x0E\x1C\x53\x16\x12"
"\x1B\x1D\x54\x50\x59\x1E\x12\x15\x19\x1E\x03\x47\x3A\x23\x2E\x3E\x39\x4B\x4D\x6A\x6B\x24\x2E\x30\x29\x15\x18\x13\x1C\x0C\x7F\x19\x13\x11\x01\x09\x01\x02\x1E\x1B\x1B\x40\x66\x68\x0C\x20\x2C\x3E\x34\x32\x37\x60\x36\x2E\x28\x28\x20\x5A\x1F\x0A\x10\x08\x1A\x0F\x5D\x5A\x00\x09\x02\x02\x12\x19\x55\x0E\x19"
"\x01\x1F\x0B\x4F\x19\x1E\x07\x10\x40\x35\x36\x2A\x5F\x45\x28\x3E\x35\x36\x28\x3E\x3E\x31\x37\x73\x34\x23\x3F\x21\x31\x26\x6A\x28\x29\x27\x6E\x3A\x3F\x28\x48\x63\x60\x31\x27\x34\x37\x32\x15\x09\x1C\x50\x50\x5F\x2F\x18\x06\x07\x19\x1F\x11\x04\x54\x4B\x4A\x3B\x1A\x00\x18\x0E\x0F\x14\x42\x45\x40\x12\x03"
"\x04\x11\x17\x33\x2F\x21\x79\x60\x7F\x18\x38\x24\x3A\x33\x34\x76\x32\x3A\x36\x38\x32\x38\x3D\x27\x20\x22\x61\x62\x2C\x32\x4B\x66\x67\x07\x2A\x14\x0F\x0A\x16\x12\x5F\x2C\x1C\x1C\x16\x1C\x51\x48\x57\x36\x1C\x1E\x27\x07\x0A\x05\x0A\x1E\x4D\x26\x11\x09\x17\x03\x47\x21\x0B\x39\x29\x21\x29\x2A\x36\x33\x33"
"\x7C\x59\x7D\x71\x02\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x78\x63\x67\x23\x2F\x33\x28\x2A\x19\x10\x1D\x0B\x59\x5F\x1F\x12\x1F\x1E\x11\x1F\x12\x04\x5A\x55\x28\x0A\x0B\x02\x4E\x1A\x1C\x4D\x1B\x0C\x15\x13\x46\x15\x01\x06\x35\x2D\x3D\x2B\x27\x7F\x37\x38\x2B\x72\x5A\x5B\x05\x03\x1B\x07\x0B\x0C\x0D"
"\x69\x1D\x1F\x0D\x0E\x07\x10\x4A\x6C\x66\x17\x2B\x2A\x16\x5B\x0B\x1C\x08\x1A\x0E\x1C\x1E\x53\x14\x03\x1F\x01\x11\x06\x4A\x02\x06\x1D\x01\x4F\x03\x03\x07\x43\x16\x08\x14\x13\x11\x04\x36\x7B\x2E\x36\x32\x2A\x31\x38\x72\x24\x39\x25\x3E\x77\x26\x30\x2E\x3E\x26\x2D\x2F\x21\x2F\x34\x62\x6B\x2D\x28\x34\x35"
"\x2B\x37\x5A\x14\x0A\x73\x5E\x5F\x0C\x1C\x00\x1A\x04\x08\x5F\x59\x54\x36\x05\x05\x1C\x1B\x01\x03\x4C\x3D\x03\x0D\x05\x0D\x46\x59\x44\x36\x2E\x34\x2A\x38\x39\x3A\x7C\x0E\x22\x32\x33\x34\x25\x79\x5E\x5F\x0E\x0E\x1E\x69\x0A\x1D\x05\x1B\x07\x63\x68\x16\x2F\x29\x20\x2A\x0D\x08\x58\x48\x4F\x56\x76\x50\x52"
"\x32\x50\x17\x17\x04\x00\x55\x38\x0E\x2E\x3A\x4E\x19\x03\x01\x17\x0E\x05\x41\x00\x08\x16\x45\x3E\x3E\x2E\x3C\x32\x30\x2C\x38\x20\x73\x27\x3E\x24\x3C\x38\x3A\x2B\x2F\x3B\x69\x66\x2D\x39\x24\x2E\x27\x60\x22\x27\x24\x2C\x20\x09\x57\x58\x2F\x33\x0C\x50\x77\x52\x53\x02\x14\x06\x18\x07\x1C\x1E\x04\x1A\x00"
"\x0B\x1C\x45\x43\x42\x30\x05\x15\x12\x0E\x0A\x02\x29\x7B\x66\x79\x0D\x26\x2F\x29\x37\x3E\x70\x6F\x76\x04\x20\x3A\x38\x2A\x2F\x2C\x6E\x71\x6C\x0C\x26\x35\x21\x2F\x25\x22\x20\x65\x44\x5B\x3C\x10\x0D\x14\x0F\x5D\x54\x79\x50\x51\x00\x18\x18\x00\x07\x0E\x1B\x49\x50\x4F\x2F\x1F\x07\x02\x14\x04\x46\x23\x01"
"\x13\x7A\x1F\x2A\x30\x28\x3A\x72\x7D\x1D\x35\x36\x38\x35\x3E\x35\x39\x6A\x2F\x27\x2A\x3D\x75\x6C\x6A\x26\x2C\x23\x32\x6B\x23\x21\x33\x57\x1F\x0A\x10\x08\x1A\x5B\x53\x78\x79\x36\x23\x33\x32\x3D\x3B\x2D\x4B\x3B\x39\x2F\x2C\x29\x4D\x21\x2B\x25\x20\x32\x47\x37\x2D\x1F\x1E\x0C\x53\x6F\x76\x7C\x0E\x26\x3C"
"\x22\x30\x31\x32\x74\x6B\x6A\x1F\x2D\x24\x3E\x20\x3E\x2C\x30\x3A\x60\x27\x2F\x2B\x21\x36\x54\x71\x4A\x50\x5E\x3B\x15\x0E\x19\x53\x33\x1D\x13\x16\x1A\x00\x1A\x4B\x56\x49\x39\x06\x02\x09\x0D\x14\x13\x41\x33\x17\x00\x04\x2E\x3E\x78\x1A\x32\x3A\x3D\x33\x27\x23\x7E\x5B\x65\x7E\x74\x18\x25\x3D\x2D\x69\x01"
"\x21\x29\x09\x30\x2A\x36\x24\x66\x21\x2D\x29\x1F\x08\x58\x0D\x11\x5F\x5B\x3B\x00\x16\x15\x51\x03\x07\x54\x06\x1A\x0A\x0B\x0C\x49\x4F\x44\x0E\x0E\x0C\x15\x05\x4B\x08\x0A\x09\x23\x72\x76\x53\x6A\x76\x7C\x08\x3C\x3A\x3E\x22\x22\x36\x38\x39\x6A\x3E\x26\x3C\x3D\x2A\x28\x6D\x23\x33\x30\x32\x66\x6F\x30\x2D"
"\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x4A\x51\x51\x16\x04\x05\x47\x1E\x06\x00\x00\x1C\x18\x0C\x0E\x0F\x47\x48\x48\x6D\x51\x4C\x7A\x1F\x31\x2A\x3F\x3D\x30\x38\x72\x3B\x39\x33\x33\x25\x3A\x34\x3E\x22\x27\x27\x74\x6F\x3C\x22\x35\x26\x32\x22\x20\x20\x64\x6A\x12\x5B\x17\x1F\x18\x5F\x54\x09\x1A\x1A\x03"
"\x51\x06\x05\x1B\x12\x18\x0A\x05\x53\x4E\x48\x04\x04\x00\x06\x12\x0F\x07\x13\x0D\x0A\x34\x76\x37\x3F\x38\x78\x75\x73\x58\x65\x79\x71\x10\x3E\x3A\x31\x6A\x27\x29\x3B\x29\x2A\x6C\x2B\x2B\x2F\x25\x32\x7C\x67\x30\x2D\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x50\x56\x1A\x16\x06\x12\x0F\x46\x0E\x00\x02\x0A"
"\x1F\x4A\x4C\x69\x6A\x32\x27\x21\x21\x45\x08\x1E\x15\x16\x08\x1E\x10\x57\x7F\x73\x05\x02\x14\x77\x30\x27\x23\x3D\x2D\x3A\x74\x6F\x2F\x21\x2B\x20\x2B\x61\x32\x2F\x21\x65\x0E\x09\x19\x00\x5E\x16\x1F\x12\x1C\x53\x4E\x51\x25\x16\x12\x10\x06\x12\x48\x1B\x0B\x02\x03\x1B\x07\x4D\x40\x35\x0E\x0E\x17\x45\x2A"
"\x29\x37\x3E\x2C\x3E\x31\x67\x58\x73\x70\x76\x25\x36\x32\x30\x67\x39\x2D\x24\x21\x39\x29\x6A\x6C"
);

MAN(performance, "Performance Guide",
"\x09\x0F\x19\x0B\x0A\x0A\x0C\x7D\x13\x03\x00\x71\x15\x18\x1A\x01\x18\x04\x04\x43\x63\x6F\x18\x22\x2D\x63\x2D\x20\x28\x3E\x64\x36\x0E\x1A\x0A\x0D\x0B\x0F\x5C\x1C\x02\x03\x03\x51\x4B\x57\x07\x19\x05\x1C\x48\x0B\x01\x00\x18\x43\x42\x30\x05\x15\x12\x0E\x0A\x02\x29\x7B\x66\x79\x1F\x2F\x2C\x2E\x72\x6D\x70"
"\x02\x22\x36\x26\x21\x3F\x3B\x64\x69\x21\x3D\x46\x6D\x62\x17\x21\x32\x2D\x67\x09\x24\x14\x1A\x1F\x1C\x0C\x5F\x42\x5D\x21\x07\x11\x03\x02\x02\x04\x55\x0B\x1B\x18\x1A\x4E\x1B\x0D\x0F\x4E\x43\x0F\x13\x46\x13\x0C\x0C\x29\x7B\x28\x2B\x31\x38\x2E\x3C\x3F\x69\x70\x76\x25\x23\x35\x27\x3E\x3E\x38\x64\x2F\x3F"
"\x3C\x3E\x65\x6D\x4A\x6C\x66\x03\x2D\x36\x1B\x19\x14\x1C\x5E\x1A\x0A\x18\x00\x0A\x04\x19\x1F\x19\x13\x55\x13\x04\x1D\x49\x0A\x00\x4C\x03\x0D\x17\x40\x0F\x03\x02\x00\x45\x3B\x2F\x78\x35\x31\x38\x35\x33\x7C\x59\x5A\x13\x17\x14\x1F\x12\x18\x04\x1D\x07\x0A\x6F\x0D\x1D\x12\x10\x4A\x6C\x66\x14\x21\x31\x0E"
"\x12\x16\x1E\x0D\x5F\x42\x5D\x21\x0A\x03\x05\x13\x1A\x54\x4B\x4A\x3B\x07\x1E\x0B\x1D\x4C\x4B\x42\x01\x01\x15\x12\x02\x16\x1C\x7A\x65\x78\x1B\x3F\x2B\x28\x38\x20\x2A\x70\x24\x25\x36\x33\x30\x6A\x63\x38\x2C\x3C\x6F\x2D\x3D\x32\x6A\x60\x20\x28\x23\x4E\x65\x5A\x28\x1D\x0D\x0A\x16\x12\x1A\x01\x53\x4E\x51"
"\x37\x07\x04\x06\x4A\x55\x48\x28\x0A\x19\x0D\x03\x01\x06\x04\x41\x07\x17\x14\x45\x29\x3E\x2C\x2D\x37\x31\x3B\x2E\x72\x6D\x70\x13\x37\x34\x3F\x32\x38\x24\x3D\x27\x2A\x6F\x2D\x3D\x32\x63\x30\x24\x34\x2A\x2D\x36\x09\x12\x17\x17\x0D\x51\x76\x50\x52\x27\x18\x18\x05\x57\x04\x07\x05\x0C\x1A\x08\x03\x55\x4C"
"\x4A\x00\x02\x03\x0A\x01\x15\x0B\x10\x34\x3F\x75\x38\x2E\x2F\x2F\x7A\x72\x23\x31\x36\x33\x79\x5E\x5F\x1A\x04\x1F\x0C\x1C\x6F\x1C\x01\x03\x0D\x4A\x6C\x66\x04\x2B\x2B\x0E\x09\x17\x15\x5E\x2F\x1D\x13\x17\x1F\x50\x4F\x56\x27\x1B\x02\x0F\x19\x48\x26\x1E\x1B\x05\x02\x0C\x10\x40\x49\x12\x0F\x0D\x16\x7A\x2B"
"\x2A\x36\x39\x2D\x3D\x30\x68\x73\x77\x21\x39\x20\x31\x27\x67\x24\x38\x3D\x27\x20\x22\x3E\x65\x6A\x7A\x4B\x66\x67\x06\x24\x16\x1A\x16\x1A\x1B\x1B\x5C\x14\x01\x53\x04\x19\x13\x57\x06\x1C\x0D\x03\x1C\x49\x0D\x07\x03\x04\x01\x06\x40\x07\x09\x15\x44\x08\x35\x28\x2C\x77\x7E\x78\x14\x34\x35\x3B\x70\x21\x33"
"\x25\x32\x3A\x38\x26\x29\x27\x2D\x2A\x6B\x6D\x30\x22\x32\x24\x2A\x3E\x64\x2D\x1F\x17\x08\x0A\x74\x5F\x5C\x12\x1C\x53\x1D\x1E\x12\x12\x06\x1B\x4A\x28\x38\x3C\x1D\x4F\x0D\x03\x06\x43\x17\x00\x15\x13\x01\x16\x7A\x39\x39\x2D\x2A\x3A\x2E\x24\x7C\x59\x7D\x71\x03\x3B\x20\x3C\x27\x2A\x3C\x2C\x6E\x1F\x29\x3F"
"\x24\x2C\x32\x2C\x27\x29\x27\x20\x5A\x0B\x14\x18\x10\x5F\x15\x0E\x52\x1B\x19\x15\x12\x12\x1A\x55\x47\x4B\x1C\x01\x07\x1C\x4C\x1D\x10\x0C\x07\x13\x07\x0A\x44\x06\x3B\x35\x78\x3C\x30\x3E\x3E\x31\x37\x73\x39\x25\x78\x5D\x5E\x12\x0B\x06\x0D\x69\x03\x00\x08\x08\x62\x65\x60\x06\x07\x0A\x01\x65\x38\x3A\x2A"
"\x73\x53\x5F\x3B\x1C\x1F\x16\x50\x3C\x19\x13\x11\x55\x42\x38\x0D\x1D\x1A\x06\x02\x0A\x11\x43\x5E\x41\x21\x06\x09\x0C\x34\x3C\x71\x79\x2E\x2D\x35\x32\x20\x3A\x24\x38\x2C\x32\x27\x75\x2D\x2A\x25\x2C\x3D\x75\x6C\x26\x27\x26\x30\x32\x66\x25\x25\x26\x11\x1C\x0A\x16\x0B\x11\x18\x77\x52\x53\x05\x01\x12\x16"
"\x00\x10\x19\x4B\x09\x07\x0A\x4F\x02\x02\x16\x0A\x06\x08\x05\x06\x10\x0C\x35\x35\x2B\x79\x2F\x2A\x35\x38\x26\x7D\x70\x05\x3E\x3E\x27\x75\x3A\x39\x27\x2E\x3C\x2E\x21\x77\x62\x64\x27\x20\x2B\x22\x69\x28\x15\x1F\x1D\x54\x11\x11\x5B\x53\x78\x79\x26\x38\x25\x22\x35\x39\x4A\x2E\x2E\x2F\x2B\x2C\x38\x3E\x68"
"\x4E\x40\x31\x03\x15\x02\x0A\x28\x36\x39\x37\x3D\x3A\x7C\x12\x22\x27\x39\x3E\x38\x24\x74\x7D\x3E\x23\x21\x3A\x6E\x3F\x3E\x22\x25\x31\x21\x2C\x7C\x67\x63\x35\x1F\x09\x1E\x16\x0C\x12\x1D\x13\x11\x16\x5D\x1E\x06\x03\x1D\x1A\x04\x18\x4F\x40\x54\x4F\x1A\x04\x11\x16\x01\x0D\x6C\x47\x44\x00\x3C\x3D\x3D\x3A"
"\x2A\x2C\x7C\x2E\x3E\x3A\x34\x34\x24\x79\x74\x72\x0B\x2F\x22\x3C\x3D\x3B\x6C\x2B\x2D\x31\x60\x23\x23\x34\x30\x65\x0A\x1E\x0A\x1F\x11\x0D\x11\x1C\x1C\x10\x15\x56\x56\x13\x1D\x06\x0B\x09\x04\x0C\x1D\x4F\x0D\x03\x0B\x0E\x01\x15\x0F\x08\x0A\x16\x7A\x3A\x36\x3D\x54\x7F\x7C\x29\x20\x32\x3E\x22\x26\x36\x26"
"\x30\x24\x28\x31\x69\x63\x6F\x21\x28\x23\x2D\x29\x2F\x21\x21\x31\x29\x5A\x14\x16\x59\x09\x1A\x1D\x16\x52\x1B\x11\x03\x12\x00\x15\x07\x0F\x45\x62\x44\x4E\x20\x02\x4D\x0F\x0C\x04\x04\x14\x09\x44\x35\x19\x28\x78\x2D\x36\x3A\x7C\x39\x3B\x35\x36\x34\x24\x32\x3A\x36\x2F\x6B\x21\x3A\x6E\x2C\x23\x3E\x2F\x26"
"\x34\x28\x25\x67\x69\x65\x16\x1E\x19\x0F\x1B\x5F\x15\x09\x52\x1C\x1E\x51\x12\x12\x12\x14\x1F\x07\x1C\x47\x64\x65\x3A\x24\x30\x37\x35\x20\x2A\x47\x29\x20\x17\x14\x0A\x00\x7E\x77\x0C\x1C\x15\x16\x16\x18\x1A\x12\x7D\x5F\x67\x6B\x1F\x20\x20\x2B\x23\x3A\x31\x63\x2D\x20\x28\x26\x23\x20\x09\x5B\x0C\x11\x1B"
"\x5F\x0C\x1C\x15\x16\x16\x18\x1A\x12\x54\x14\x1F\x1F\x07\x04\x0F\x1B\x05\x0E\x03\x0F\x0C\x18\x46\x4A\x44\x09\x3F\x3A\x2E\x3C\x7E\x36\x28\x7D\x33\x3F\x3F\x3F\x33\x77\x21\x3B\x26\x2E\x3B\x3A\x6E\x36\x23\x38\x48\x63\x60\x2A\x28\x28\x33\x65\x0D\x13\x19\x0D\x5E\x06\x13\x08\x52\x12\x02\x14\x56\x13\x1B\x1C"
"\x04\x0C\x46\x49\x3E\x0A\x1E\x0B\x0D\x11\x0D\x00\x08\x04\x01\x45\x15\x2B\x2C\x30\x31\x31\x2F\x7D\x6C\x73\x11\x35\x20\x36\x3A\x36\x2F\x2F\x68\x77\x6E\x19\x25\x3F\x36\x36\x21\x2D\x66\x2A\x21\x28\x15\x09\x01\x57\x74\x52\x5C\x3B\x07\x1F\x1C\x51\x12\x1E\x07\x1E\x4A\x56\x48\x3E\x07\x01\x08\x02\x15\x10\x40"
"\x02\x07\x09\x43\x11\x7A\x3C\x2A\x36\x29\x7F\x28\x35\x37\x73\x20\x30\x31\x32\x32\x3C\x26\x2E\x66\x69\x05\x2A\x29\x3D\x62\x72\x70\x6C\x74\x77\x64\x02\x38\x5B\x1E\x0B\x1B\x1A\x52\x77\x78\x20\x29\x22\x3B\x36\x3D\x3B\x4A\x44\x48\x3A\x3B\x3F\x29\x3F\x24\x26\x34\x22\x2E\x6D\x49\x45\x09\x22\x2B\x14\x3F\x36"
"\x32\x7D\x22\x21\x35\x3D\x39\x36\x30\x26\x6A\x2D\x3A\x2C\x3F\x3A\x29\x23\x36\x2F\x39\x61\x33\x34\x21\x21\x5A\x1A\x08\x09\x0D\x5F\x15\x13\x06\x1C\x50\x23\x37\x3A\x5A\x55\x23\x1F\x48\x01\x0B\x03\x1C\x1E\x42\x0E\x0F\x13\x03\x47\x10\x0D\x3B\x35\x78\x30\x2A\x55\x7C\x7D\x3A\x26\x22\x25\x25\x77\x79\x75\x26"
"\x2E\x29\x3F\x2B\x6F\x25\x39\x62\x2C\x2E\x6F\x66\x13\x2C\x2C\x09\x5B\x08\x0B\x11\x18\x0E\x1C\x1F\x49\x50\x56\x05\x0E\x07\x18\x0B\x02\x06\x44\x01\x09\x0A\x42\x0D\x0D\x47\x41\x0F\x01\x44\x1C\x35\x2E\x78\x34\x2B\x2C\x28\x73\x58\x59\x03\x14\x17\x05\x17\x1D\x6A\x02\x06\x0D\x0B\x17\x05\x03\x05\x49\x6D\x61"
"\x11\x2E\x2A\x21\x15\x0C\x0B\x59\x2D\x1A\x1D\x0F\x11\x1B\x50\x18\x18\x13\x11\x0D\x0F\x18\x48\x10\x01\x1A\x1E\x4D\x04\x0A\x0C\x04\x15\x47\x02\x0A\x28\x7B\x3E\x38\x2D\x2B\x7C\x2F\x37\x20\x25\x3D\x22\x24\x7A\x75\x03\x25\x2C\x2C\x36\x26\x22\x2A\x62\x0C\x30\x35\x2F\x28\x2A\x36\x70\x5B\x58\x51\x0A\x17\x15"
"\x0E\x52\x03\x02\x1E\x11\x05\x15\x18\x50\x4B\x4F\x00\x00\x0B\x09\x15\x0B\x0D\x07\x4C\x09\x17\x10\x0C\x35\x35\x2B\x7E\x77\x7F\x3F\x3C\x3C\x73\x35\x29\x35\x3B\x21\x31\x2F\x6B\x20\x3C\x29\x2A\x6C\x2B\x2D\x2F\x24\x24\x34\x34\x6A\x4F\x57\x5B\x3C\x10\x0D\x1E\x1E\x11\x1B\x1D\x17\x51\x1F\x19\x10\x10\x12\x02"
"\x06\x0E\x4E\x1C\x0D\x1B\x07\x10\x40\x0D\x0F\x13\x10\x09\x3F\x7B\x39\x37\x3A\x7F\x2F\x31\x3D\x24\x23\x71\x25\x32\x35\x27\x29\x23\x66\x43\x44\x1C\x18\x02\x10\x02\x07\x04\x66\x0F\x01\x04\x36\x2F\x30\x73\x53\x5F\x3A\x08\x1E\x1F\x50\x1E\x04\x57\x12\x14\x03\x07\x01\x07\x09\x4F\x08\x04\x11\x08\x13\x41\x15"
"\x0B\x0B\x12\x7A\x3E\x2E\x3C\x2C\x26\x28\x35\x3B\x3D\x37\x7F\x76\x70\x30\x27\x23\x3D\x2D\x64\x27\x21\x2A\x22\x65\x63\x33\x29\x29\x30\x37\x65\x29\x36\x39\x2B\x2A\x5F\x14\x18\x13\x1F\x04\x19\x58\x7D\x59\x55\x3E\x19\x01\x04\x4E\x3C\x3F\x29\x11\x43\x48\x00\x13\x13\x0B\x08\x3B\x2F\x31\x3A\x77\x73\x7C\x39"
"\x37\x35\x22\x30\x31\x77\x1C\x11\x0E\x38\x68\x26\x2D\x2C\x2D\x3E\x2B\x2C\x2E\x20\x2A\x2B\x3D\x6B\x70\x71\x3C\x2B\x37\x29\x39\x2F\x21\x53\x56\x51\x30\x3E\x26\x38\x3D\x2A\x3A\x2C\x64\x42\x4C\x22\x17\x17\x04\x00\x12\x02\x00\x45\x1D\x0B\x0D\x76\x3D\x37\x35\x2D\x21\x36\x24\x71\x32\x25\x3D\x23\x2F\x39\x3B"
"\x69\x2D\x2E\x39\x3E\x27\x63\x33\x35\x33\x33\x30\x20\x08\x5B\x19\x17\x1A\x5F\x1F\x0F\x13\x00\x18\x14\x05\x59\x54\x20\x1A\x0F\x09\x1D\x0B\x4F\x1A\x04\x03\x69\x40\x41\x31\x0E\x0A\x01\x35\x2C\x2B\x79\x0B\x2F\x38\x3C\x26\x36\x70\x3E\x26\x23\x3D\x3A\x24\x2A\x24\x69\x3B\x3F\x28\x2C\x36\x26\x33\x6D\x66\x08"
"\x01\x08\x5A\x1A\x08\x09\x0D\x53\x5C\x12\x00\x53\x04\x19\x13\x57\x19\x14\x04\x1E\x0E\x08\x0D\x1B\x19\x1F\x07\x11\x40\x12\x0F\x13\x01\x4B\x50\x76\x78\x0D\x36\x36\x2F\x7D\x22\x21\x3F\x36\x24\x36\x39\x6F\x6A\x6C\x3D\x39\x2A\x2E\x38\x28\x6F\x20\x28\x24\x25\x2C\x63\x69\x5A\x5C\x1C\x1C\x08\x16\x1F\x18\x5F"
"\x1E\x11\x1F\x17\x10\x11\x07\x4D\x45\x62\x63\x23\x2A\x2D\x3E\x37\x31\x25\x41\x40\x47\x20\x2C\x1B\x1C\x16\x16\x0D\x1A\x56\x70\x72\x07\x31\x22\x3D\x77\x19\x34\x24\x2A\x2F\x2C\x3C\x6F\x64\x0E\x36\x31\x2C\x6A\x15\x2F\x2D\x23\x0E\x50\x3D\x0A\x1D\x56\x46\x5D\x01\x1C\x02\x05\x56\x07\x06\x1A\x09\x0E\x1B\x1A"
"\x0B\x1C\x4C\x0F\x1B\x43\x23\x31\x33\x48\x36\x24\x17\x74\x1C\x30\x2D\x34\x73\x13\x37\x27\x27\x3E\x24\x3C\x7A\x5F\x67\x6B\x1A\x2C\x3D\x20\x39\x3F\x21\x26\x60\x0C\x29\x29\x2D\x31\x15\x09\x42\x59\x59\x0D\x19\x0E\x1D\x06\x02\x12\x13\x5A\x19\x1A\x04\x02\x1C\x06\x1C\x48\x42\x67\x4F\x43\x30\x04\x14\x01\x0B"
"\x17\x37\x3A\x36\x3A\x3B\x7F\x11\x32\x3C\x3A\x24\x3E\x24\x6D\x74\x72\x3A\x2E\x3A\x2F\x21\x3D\x21\x2C\x2C\x20\x25\x6C\x2B\x28\x2A\x2C\x0E\x14\x0A\x5E\x50\x75\x51\x5D\x20\x16\x1C\x18\x17\x15\x1D\x19\x03\x1F\x11\x49\x23\x00\x02\x04\x16\x0C\x12\x5B\x46\x40\x16\x00\x36\x32\x39\x3B\x37\x33\x35\x29\x2B\x7E"
"\x3D\x3E\x38\x3E\x20\x3A\x38\x6C\x68\x64\x6E\x2E\x6C\x39\x2B\x2E\x25\x2D\x2F\x29\x21\x65\x15\x1D\x58\x1A\x0C\x1E\x0F\x15\x17\x00\x5E\x7B\x5B\x57\x36\x14\x1E\x1F\x0D\x1B\x17\x4F\x1E\x08\x12\x0C\x12\x15\x5C\x47\x43\x07\x3B\x2F\x2C\x3C\x2C\x26\x71\x2F\x37\x23\x3F\x23\x22\x70\x74\x7D\x3A\x24\x3F\x2C\x3C"
"\x2C\x2A\x2A\x62\x6C\x22\x20\x32\x33\x21\x37\x03\x09\x1D\x09\x11\x0D\x08\x54\x5C\x79\x5D\x51\x33\x19\x11\x07\x0D\x12\x48\x1B\x0B\x1F\x03\x1F\x16\x59\x40\x46\x03\x09\x01\x17\x3D\x22\x75\x2B\x3B\x2F\x33\x2F\x26\x74\x70\x79\x26\x38\x23\x30\x38\x28\x2E\x2E\x6E\x60\x29\x23\x27\x31\x27\x38\x6F\x69\x4E\x4F"
"\x39\x37\x3D\x38\x30\x5F\x3E\x32\x3D\x27\x50\x59\x30\x3E\x3A\x31\x4A\x3F\x20\x2C\x4E\x2C\x39\x21\x32\x31\x29\x35\x4F\x6D\x49\x45\x37\x28\x3B\x36\x30\x39\x35\x3A\x72\x6D\x70\x02\x33\x25\x22\x3C\x29\x2E\x3B\x69\x70\x6F\x04\x24\x26\x26\x60\x20\x2A\x2B\x64\x08\x13\x18\x0A\x16\x0D\x10\x1A\x09\x52\x00\x15"
"\x03\x00\x1E\x17\x10\x19\x4B\x56\x49\x2A\x06\x1F\x0C\x00\x0F\x05\x41\x07\x0B\x08\x5E\x7A\x2F\x30\x3C\x30\x55\x7C\x7D\x06\x32\x23\x3A\x76\x1A\x35\x3B\x2B\x2C\x2D\x3B\x6E\x71\x6C\x1E\x36\x22\x32\x35\x33\x37\x64\x7B\x5A\x1F\x11\x0A\x1F\x1D\x10\x18\x52\x12\x1C\x1D\x58\x57\x26\x10\x08\x04\x07\x1D\x40\x4F"
"\x25\x0B\x42\x05\x01\x12\x12\x4B\x44\x17\x3F\x76\x3D\x37\x3F\x3D\x30\x38\x72\x3B\x31\x3D\x30\x5D\x74\x75\x2B\x3F\x68\x28\x6E\x3B\x25\x20\x27\x63\x34\x2E\x66\x21\x2D\x2B\x1E\x5B\x0C\x11\x1B\x5F\x13\x1B\x14\x16\x1E\x15\x13\x05\x5A\x55\x3E\x03\x01\x1A\x4E\x1F\x1E\x02\x05\x11\x01\x0C\x5C\x47\x43\x08\x29"
"\x38\x37\x37\x38\x36\x3B\x7A\x7C\x59\x5A\x1C\x0F\x03\x1C\x06\x40\x66\x68\x6E\x1C\x0E\x01\x6D\x21\x2F\x25\x20\x28\x22\x36\x36\x5D\x5B\x1C\x16\x5E\x11\x13\x09\x52\x1B\x15\x1D\x06\x4C\x54\x13\x18\x0E\x0D\x49\x3C\x2E\x21\x4D\x0B\x10\x40\x16\x07\x14\x10\x00\x3E\x7B\x0A\x18\x13\x71\x56\x70\x72\x10\x3C\x3E"
"\x25\x3E\x3A\x32\x6A\x3F\x3A\x28\x37\x6F\x25\x2E\x2D\x2D\x33\x61\x34\x26\x36\x20\x16\x02\x58\x14\x1F\x0B\x08\x18\x00\x00\x5E\x7B\x5B\x57\x30\x10\x0C\x19\x09\x0E\x09\x06\x02\x0A\x42\x30\x33\x25\x15\x47\x0C\x10\x28\x2F\x2B\x77\x54\x72\x7C\x7A\x10\x3C\x3F\x22\x22\x32\x26\x26\x6D\x6B\x25\x26\x3D\x3B\x20"
"\x34\x62\x29\x35\x32\x32\x67\x20\x2C\x09\x1A\x1A\x15\x1B\x5F\x0C\x0F\x1D\x07\x15\x12\x02\x1E\x1B\x1B\x19\x4B\x45\x49\x0F\x19\x03\x04\x06\x43\x14\x09\x03\x0A\x4A\x6F\x50\x09\x1D\x18\x12\x7F\x0B\x14\x1C\x1D\x15\x03\x05\x5D\x07\x06\x0E\x6B\x63\x69\x2B\x21\x23\x38\x25\x2B\x60\x13\x07\x0A\x64\x6E\x5A\x0E"
"\x08\x1D\x1F\x0B\x19\x19\x52\x17\x02\x18\x00\x12\x06\x06\x4A\x40\x48\x0F\x0B\x18\x4C\x1E\x16\x02\x12\x15\x13\x17\x44\x04\x2A\x2B\x2B\x79\x75\x7F\x2C\x31\x37\x3D\x24\x28\x76\x38\x32\x75\x2C\x39\x2D\x2C\x44\x2B\x25\x3E\x29\x63\x33\x31\x27\x24\x21\x65\x51\x5B\x1B\x16\x11\x13\x5C\x09\x17\x1E\x00\x14\x04"
"\x16\x00\x00\x18\x0E\x1B\x47\x4E\x2A\x1A\x08\x10\x1A\x14\x09\x0F\x09\x03\x45\x3F\x37\x2B\x3C\x7E\x36\x2F\x7D\x35\x32\x22\x3F\x3F\x24\x3C\x7B"
);

MAN(gaming, "Gaming Guide",
"\x1D\x1A\x15\x1C\x7E\x1D\x1D\x0F\x72\x7B\x07\x38\x38\x7C\x13\x7C\x40\x66\x68\x06\x38\x2A\x3E\x21\x23\x3A\x7A\x61\x36\x22\x36\x23\x15\x09\x15\x18\x10\x1C\x19\x5D\x05\x1A\x14\x16\x13\x03\x07\x55\x42\x2D\x38\x3A\x42\x4F\x2F\x3D\x37\x4F\x40\x26\x36\x32\x48\x45\x08\x1A\x15\x70\x72\x7F\x3F\x3C\x22\x27\x25"
"\x23\x33\x24\x78\x5F\x6A\x6B\x2A\x3B\x21\x2E\x28\x2E\x23\x30\x34\x28\x28\x20\x68\x65\x1B\x0E\x1C\x10\x11\x53\x5C\x25\x10\x1C\x08\x51\x05\x18\x17\x1C\x0B\x07\x46\x63\x43\x4F\x3F\x05\x0D\x11\x14\x02\x13\x13\x17\x5F\x7A\x0C\x31\x37\x75\x1E\x30\x29\x79\x01\x70\x23\x33\x34\x3B\x27\x2E\x67\x68\x1E\x27\x21"
"\x67\x0C\x2E\x37\x6B\x06\x66\x35\x21\x26\x15\x09\x1C\x59\x12\x1E\x0F\x09\x52\x40\x40\x02\x5A\x57\x23\x1C\x04\x40\x29\x05\x1A\x44\x3C\x1F\x0B\x0D\x14\x6B\x46\x47\x17\x06\x28\x3E\x3D\x37\x2D\x37\x33\x29\x7E\x73\x07\x38\x38\x7C\x15\x39\x3E\x60\x05\x69\x23\x3A\x38\x28\x62\x2E\x29\x22\x34\x28\x34\x2D\x15"
"\x15\x1D\x57\x74\x52\x5C\x2E\x17\x07\x04\x18\x18\x10\x07\x4F\x4A\x38\x0D\x1D\x1A\x06\x02\x0A\x11\x43\x5E\x41\x21\x06\x09\x0C\x34\x3C\x78\x67\x7E\x18\x3D\x30\x37\x73\x12\x30\x24\x77\x7C\x21\x25\x2C\x2F\x25\x2B\x63\x6C\x26\x27\x3A\x22\x2E\x27\x35\x20\x65\x09\x13\x17\x0B\x0A\x1C\x09\x09\x01\x5A\x5E\x7B"
"\x7C\x30\x35\x38\x2F\x4B\x25\x26\x2A\x2A\x66\x40\x42\x30\x05\x15\x12\x0E\x0A\x02\x29\x7B\x66\x79\x19\x3E\x31\x34\x3C\x34\x70\x6F\x76\x10\x35\x38\x2F\x6B\x05\x26\x2A\x2A\x76\x6D\x15\x2A\x2E\x25\x29\x30\x37\x65\x0A\x09\x11\x16\x0C\x16\x08\x14\x08\x16\x03\x51\x0F\x18\x01\x07\x4A\x0C\x09\x04\x0B\x4F\x41"
"\x4D\x12\x02\x15\x12\x03\x14\x6E\x45\x7A\x2E\x28\x3D\x3F\x2B\x39\x2E\x7E\x73\x34\x34\x30\x25\x35\x32\x6A\x2A\x26\x2D\x6E\x21\x23\x39\x2B\x25\x29\x22\x27\x33\x2D\x2A\x14\x08\x56\x59\x35\x1A\x19\x0D\x52\x1A\x04\x51\x39\x39\x5A\x7F\x60\x28\x29\x39\x3A\x3A\x3E\x28\x31\x43\x46\x41\x24\x35\x2B\x24\x1E\x18"
"\x19\x0A\x0A\x16\x12\x1A\x58\x7E\x70\x12\x37\x27\x20\x20\x38\x2E\x3B\x69\x28\x20\x20\x29\x27\x31\x7A\x61\x10\x2E\x20\x20\x15\x08\x24\x3A\x1F\x0F\x08\x08\x00\x16\x03\x51\x5E\x14\x1C\x14\x04\x0C\x0D\x49\x07\x01\x4C\x2A\x03\x0E\x05\x41\x24\x06\x16\x45\x2D\x32\x3C\x3E\x3B\x2B\x7C\x2E\x37\x27\x24\x38\x38"
"\x30\x27\x7C\x64\x41\x65\x69\x0C\x3D\x23\x2C\x26\x20\x21\x32\x32\x2E\x2A\x22\x40\x5B\x14\x10\x08\x1A\x5C\x09\x1D\x53\x24\x06\x1F\x03\x17\x1D\x4A\x43\x1B\x00\x09\x01\x4C\x04\x0C\x43\x17\x08\x12\x0F\x44\x1C\x35\x2E\x2A\x79\x3F\x3C\x3F\x32\x27\x3D\x24\x78\x78\x5D\x79\x75\x08\x2A\x2B\x22\x29\x3D\x23\x38"
"\x2C\x27\x60\x33\x23\x24\x2B\x37\x1E\x12\x16\x1E\x44\x5F\x3B\x1C\x1F\x16\x50\x33\x17\x05\x54\x4B\x4A\x38\x0D\x1D\x1A\x06\x02\x0A\x11\x43\x12\x04\x05\x08\x16\x01\x29\x7B\x31\x37\x7E\x2B\x34\x38\x72\x31\x31\x32\x3D\x30\x26\x3A\x3F\x25\x2C\x69\x3D\x20\x46\x6D\x62\x3A\x2F\x34\x66\x24\x25\x2B\x5A\x08\x19"
"\x0F\x1B\x5F\x08\x15\x17\x53\x1C\x10\x05\x03\x54\x18\x05\x06\x0D\x07\x1A\x1C\x4C\x40\x42\x00\x0F\x12\x12\x14\x44\x16\x35\x36\x3D\x79\x3A\x36\x2F\x36\x72\x20\x20\x30\x35\x32\x7A\x5F\x40\x13\x0A\x06\x16\x6F\x05\x03\x16\x06\x07\x13\x07\x13\x0D\x0A\x34\x71\x55\x59\x26\x1D\x13\x05\x52\x12\x00\x01\x4C\x57"
"\x33\x14\x07\x0E\x48\x39\x0F\x1C\x1F\x4D\x01\x02\x14\x00\x0A\x08\x03\x49\x7A\x38\x34\x36\x2B\x3B\x7C\x3A\x33\x3E\x39\x3F\x31\x7B\x74\x26\x25\x28\x21\x28\x22\x61\x46\x60\x62\x1B\x22\x2E\x3E\x67\x2A\x20\x0E\x0C\x17\x0B\x15\x16\x12\x1A\x52\x5B\x23\x14\x02\x03\x1D\x1B\x0D\x18\x48\x57\x4E\x28\x0D\x00\x0B"
"\x0D\x07\x41\x58\x47\x3C\x07\x35\x23\x78\x37\x3B\x2B\x2B\x32\x20\x38\x39\x3F\x31\x7E\x6E\x75\x3E\x2E\x3B\x3D\x3D\x6F\x02\x0C\x16\x63\x21\x2F\x22\x4D\x64\x65\x16\x1A\x0C\x1C\x10\x1C\x05\x5D\x14\x1C\x02\x51\x1B\x02\x18\x01\x03\x1B\x04\x08\x17\x0A\x1E\x43\x42\x44\x34\x04\x14\x02\x00\x0A\x7D\x7B\x31\x2A"
"\x2D\x2A\x39\x2E\x72\x30\x31\x3F\x76\x35\x38\x3A\x29\x20\x68\x39\x2F\x3D\x38\x34\x62\x20\x28\x20\x32\x69\x4E\x4F\x3E\x32\x2B\x29\x32\x3E\x25\x5D\x26\x36\x33\x39\x7C\x5A\x54\x3D\x2E\x39\x52\x49\x3D\x0A\x18\x19\x0B\x0D\x07\x12\x46\x59\x44\x36\x23\x28\x2C\x3C\x33\x7F\x62\x7D\x16\x3A\x23\x21\x3A\x36\x2D"
"\x75\x74\x6B\x00\x0D\x1C\x6F\x64\x05\x06\x11\x6D\x22\x27\x37\x25\x27\x16\x1E\x58\x14\x11\x11\x15\x09\x1D\x01\x03\x58\x58\x7D\x59\x55\x2B\x1E\x1C\x06\x4E\x27\x28\x3F\x42\x4B\x37\x08\x08\x56\x55\x4C\x60\x7B\x31\x34\x2E\x2D\x33\x2B\x37\x20\x70\x02\x12\x05\x74\x32\x2B\x26\x2D\x3A\x6E\x20\x22\x6D\x0A\x07"
"\x12\x61\x22\x2E\x37\x35\x16\x1A\x01\x0A\x50\x75\x51\x5D\x24\x12\x02\x18\x17\x15\x18\x10\x4A\x39\x0D\x0F\x1C\x0A\x1F\x05\x42\x31\x01\x15\x03\x47\x4B\x45\x0C\x09\x0A\x79\x76\x18\x71\x0E\x2B\x3D\x33\x7D\x76\x11\x26\x30\x2F\x18\x31\x27\x2D\x66\x76\x6D\x11\x26\x34\x35\x2F\x29\x23\x36\x5A\x45\x58\x3D\x17"
"\x0C\x0C\x11\x13\x0A\x50\x4F\x7C\x57\x54\x34\x0E\x1D\x09\x07\x0D\x0A\x08\x4D\x06\x0A\x13\x11\x0A\x06\x1D\x4B\x50\x76\x78\x0B\x3B\x39\x2E\x38\x21\x3B\x70\x23\x37\x23\x31\x6F\x6A\x38\x2D\x25\x2B\x2C\x38\x6D\x36\x2B\x25\x61\x2E\x2E\x23\x2D\x1F\x08\x0C\x59\x07\x10\x09\x0F\x52\x1E\x1F\x1F\x1F\x03\x1B\x07"
"\x4A\x18\x1D\x19\x1E\x00\x1E\x19\x11\x43\x14\x09\x03\x15\x01\x45\x2E\x34\x37\x77\x54\x72\x7C\x1A\x02\x06\x70\x21\x24\x32\x32\x30\x38\x2E\x26\x2A\x2B\x3C\x76\x6D\x11\x26\x34\x35\x2F\x29\x23\x36\x5A\x45\x58\x3D\x17\x0C\x0C\x11\x13\x0A\x50\x4F\x56\x30\x06\x14\x1A\x03\x01\x0A\x1D\x4F\x41\x4D\x12\x06\x12"
"\x4C\x07\x17\x14\x45\x7D\x13\x31\x3E\x36\x55\x7C\x7D\x22\x36\x22\x37\x39\x25\x39\x34\x24\x28\x2D\x6E\x6E\x08\x1C\x18\x62\x6B\x26\x2E\x34\x67\x28\x24\x0A\x0F\x17\x09\x0D\x5F\x0B\x14\x06\x1B\x50\x15\x03\x16\x18\x55\x2D\x3B\x3D\x1A\x47\x41\x66\x67\x32\x26\x32\x27\x29\x35\x29\x24\x14\x18\x1D\x53\x73\x7F"
"\x1B\x3C\x3F\x36\x70\x1C\x39\x33\x31\x75\x61\x6B\x3D\x39\x2A\x2E\x38\x28\x26\x63\x07\x11\x13\x67\x20\x37\x13\x0D\x1D\x0B\x5E\x54\x5C\x3A\x13\x1E\x15\x51\x34\x16\x06\x55\x4D\x0F\x07\x49\x00\x00\x18\x4D\x10\x06\x03\x0E\x14\x03\x44\x12\x32\x32\x34\x3C\x7E\x16\x7B\x30\x58\x73\x70\x21\x3A\x36\x2D\x3C\x24"
"\x2C\x6F\x69\x65\x6F\x2F\x21\x2D\x30\x29\x2F\x21\x67\x26\x37\x15\x0C\x0B\x1C\x0C\x5F\x08\x1C\x10\x00\x50\x19\x13\x1B\x04\x06\x4A\x0D\x1A\x08\x03\x0A\x4C\x1F\x03\x17\x05\x12\x48\x6D\x49\x45\x0E\x33\x31\x2A\x7E\x2F\x2E\x32\x35\x21\x31\x3C\x6C\x77\x73\x32\x2B\x26\x2D\x64\x23\x20\x28\x28\x6F\x2C\x2E\x66"
"\x6A\x67\x63\x22\x1B\x16\x1D\x54\x1A\x09\x0E\x50\x1D\x1D\x57\x5D\x56\x50\x04\x10\x18\x0D\x07\x1B\x03\x0E\x02\x0E\x07\x4E\x0F\x11\x12\x0E\x0B\x0B\x29\x7C\x76\x53\x54\x1C\x13\x13\x06\x01\x1F\x1D\x1A\x12\x06\x06\x40\x66\x68\x11\x2C\x20\x34\x6D\x21\x2C\x2E\x35\x34\x28\x28\x29\x1F\x09\x0B\x43\x5E\x0F\x10"
"\x08\x15\x53\x19\x1F\x56\x18\x06\x55\x28\x07\x1D\x0C\x1A\x00\x03\x19\x0A\x43\x4D\x41\x11\x08\x16\x0E\x29\x7B\x31\x37\x2D\x2B\x3D\x33\x26\x3F\x29\x7F\x5C\x7A\x74\x06\x2F\x3F\x68\x3C\x3E\x6F\x2F\x38\x31\x37\x2F\x2C\x66\x37\x36\x2A\x1C\x12\x14\x1C\x0D\x5F\x15\x13\x52\x07\x18\x14\x56\x2F\x16\x1A\x12\x4B"
"\x29\x0A\x0D\x0A\x1F\x1E\x0D\x11\x09\x04\x15\x47\x05\x15\x2A\x75\x52\x74\x7E\x0B\x34\x34\x21\x73\x20\x23\x39\x30\x26\x34\x27\x71\x68\x6E\x29\x2E\x21\x28\x6F\x20\x2F\x2F\x32\x35\x2B\x29\x16\x1E\x0A\x0A\x59\x5F\x54\x17\x1D\x0A\x5E\x12\x06\x1B\x5D\x5B\x60\x61\x2F\x28\x23\x2A\x4C\x3D\x23\x30\x33\x41\x49"
"\x47\x37\x31\x15\x09\x1D\x79\x19\x1E\x11\x18\x01\x59\x7D\x71\x11\x36\x39\x30\x39\x6B\x21\x27\x3D\x3B\x2D\x21\x2E\x63\x36\x28\x27\x67\x30\x2D\x1F\x5B\x2B\x0D\x11\x0D\x19\x52\x2A\x11\x1F\x09\x56\x16\x04\x05\x4A\x43\x25\x3A\x27\x37\x45\x43\x42\x31\x05\x11\x07\x0E\x16\x4A\x28\x3E\x2B\x3C\x2A\x65\x56\x7D"
"\x72\x00\x35\x25\x22\x3E\x3A\x32\x39\x6B\x76\x69\x0F\x3F\x3C\x3E\x62\x7D\x60\x26\x27\x2A\x21\x65\x44\x5B\x39\x1D\x08\x1E\x12\x1E\x17\x17\x50\x1E\x06\x03\x1D\x1A\x04\x18\x46\x63\x43\x4F\x21\x02\x06\x10\x40\x07\x09\x15\x44\x22\x3B\x36\x3D\x79\x0E\x3E\x2F\x2E\x72\x34\x31\x3C\x33\x24\x74\x39\x23\x3D\x2D"
"\x69\x27\x21\x6C\x1A\x2B\x2D\x24\x2E\x31\x34\x05\x35\x0A\x08\x58\x51\x0E\x0D\x13\x09\x17\x10\x04\x14\x12\x5E\x54\x58\x4A\x0C\x09\x04\x0B\x65\x4C\x4D\x06\x0A\x12\x04\x05\x13\x0B\x17\x33\x3E\x2B\x79\x3F\x2D\x39\x7D\x37\x3F\x23\x34\x21\x3F\x31\x27\x2F\x70\x68\x2C\x20\x2E\x2E\x21\x27\x63\x04\x24\x30\x22"
"\x28\x2A\x0A\x1E\x0A\x59\x33\x10\x18\x18\x52\x07\x1F\x51\x14\x05\x1B\x02\x19\x0E\x48\x1D\x06\x0A\x01\x43\x68\x69\x33\x35\x27\x25\x2D\x29\x13\x0F\x01\x53\x73\x7F\x1F\x2F\x33\x20\x38\x34\x25\x6D\x74\x20\x3A\x2F\x29\x3D\x2B\x6F\x28\x3F\x2B\x35\x25\x33\x35\x6B\x64\x33\x1F\x09\x11\x1F\x07\x5F\x08\x15\x17"
"\x53\x17\x10\x1B\x12\x54\x13\x03\x07\x0D\x1A\x4E\x47\x3F\x19\x0D\x11\x05\x5B\x46\x35\x01\x15\x3B\x32\x2A\x62\x54\x7F\x7C\x0E\x26\x36\x31\x3C\x6C\x77\x02\x30\x38\x22\x2E\x30\x67\x63\x6C\x2E\x2A\x26\x23\x2A\x66\x33\x21\x28\x0A\x1E\x0A\x18\x0A\x0A\x0E\x18\x01\x5F\x50\x03\x03\x19\x54\x18\x0F\x06\x07\x1B"
"\x17\x4F\x08\x04\x03\x04\x0E\x0E\x15\x13\x0D\x06\x29\x75\x52\x74\x7E\x10\x2A\x38\x20\x3F\x31\x28\x25\x77\x7C\x11\x23\x38\x2B\x26\x3C\x2B\x6C\x28\x36\x20\x6E\x68\x66\x28\x27\x26\x1B\x08\x11\x16\x10\x1E\x10\x11\x0B\x53\x13\x1E\x18\x11\x18\x1C\x09\x1F\x48\x44\x4E\x0B\x05\x1E\x03\x01\x0C\x04\x46\x08\x0A"
"\x00\x7A\x3A\x2C\x79\x3F\x7F\x28\x34\x3F\x36\x7E\x5B\x7B\x77\x03\x3C\x24\x2F\x27\x3E\x3D\x6F\x19\x3D\x26\x22\x34\x24\x66\x26\x2A\x21\x5A\x18\x10\x10\x0E\x0C\x19\x09\x52\x17\x02\x18\x00\x12\x06\x06\x4A\x06\x09\x1D\x1A\x0A\x1E\x4D\x04\x0C\x12\x41\x15\x13\x11\x11\x2E\x3E\x2A\x74\x38\x2D\x39\x38\x72\x34"
"\x31\x3C\x3F\x39\x33\x7B"
);

MAN(troubleshoot, "Troubleshooting Guide",
"\x17\x1E\x0C\x11\x11\x1B\x66\x7D\x1D\x11\x03\x14\x04\x01\x11\x75\x74\x6B\x01\x1A\x01\x03\x0D\x19\x07\x63\x7E\x61\x00\x0E\x1C\x65\x44\x5B\x2E\x3C\x2C\x36\x3A\x24\x78\x42\x59\x51\x21\x05\x1D\x01\x0F\x4B\x0C\x06\x19\x01\x4C\x08\x1A\x02\x03\x15\x0A\x1E\x44\x12\x32\x3A\x2C\x79\x36\x3E\x2C\x2D\x37\x3D\x23"
"\x71\x7E\x32\x26\x27\x25\x39\x68\x3D\x2B\x37\x38\x61\x62\x34\x28\x24\x28\x6B\x64\x32\x12\x1A\x0C\x59\x1D\x17\x1D\x13\x15\x16\x14\x58\x58\x7D\x46\x5C\x4A\x3C\x00\x08\x1A\x4F\x0F\x05\x03\x0D\x07\x04\x02\x47\x16\x00\x39\x3E\x36\x2D\x32\x26\x63\x7D\x07\x23\x34\x30\x22\x32\x78\x75\x2E\x39\x21\x3F\x2B\x3D"
"\x60\x6D\x2C\x26\x37\x61\x35\x28\x22\x31\x0D\x1A\x0A\x1C\x52\x5F\x12\x18\x05\x53\x18\x10\x04\x13\x03\x14\x18\x0E\x57\x63\x5D\x46\x4C\x2E\x0A\x02\x0E\x06\x03\x47\x2B\x2B\x1F\x7B\x2C\x31\x37\x31\x3B\x71\x72\x27\x35\x22\x22\x7B\x74\x27\x2F\x3B\x2D\x28\x3A\x61\x6C\x03\x27\x35\x25\x33\x66\x24\x2C\x24\x14"
"\x1C\x1D\x59\x0D\x1A\x0A\x18\x00\x12\x1C\x51\x02\x1F\x1D\x1B\x0D\x18\x48\x08\x1A\x4F\x03\x03\x01\x06\x4E\x6B\x6C\x35\x21\x36\x0E\x1A\x0A\x0D\x72\x7F\x08\x15\x17\x73\x05\x1F\x12\x12\x06\x07\x0B\x1F\x0D\x0D\x6E\x09\x05\x15\x48\x6E\x60\x13\x23\x34\x30\x24\x08\x0F\x58\x1A\x12\x1A\x1D\x0F\x01\x53\x03\x05"
"\x03\x14\x1F\x55\x1A\x19\x07\x0A\x0B\x1C\x1F\x08\x11\x4F\x40\x05\x14\x0E\x12\x00\x28\x28\x78\x38\x30\x3B\x7C\x30\x37\x3E\x3F\x23\x2F\x77\x38\x30\x2B\x20\x3B\x67\x6E\x0B\x23\x6D\x2B\x37\x60\x23\x23\x21\x2B\x37\x1F\x71\x58\x59\x1A\x1A\x19\x0D\x52\x17\x15\x13\x03\x10\x13\x1C\x04\x0C\x46\x49\x2F\x4F\x0A"
"\x18\x0E\x0F\x40\x12\x0E\x12\x10\x01\x35\x2C\x36\x79\x76\x08\x35\x33\x79\x0B\x70\x6F\x76\x04\x3C\x20\x3E\x6B\x2C\x26\x39\x21\x65\x6D\x23\x2F\x33\x2E\x66\x24\x28\x20\x1B\x09\x0B\x59\x0A\x17\x19\x77\x52\x53\x16\x10\x05\x03\x59\x06\x1E\x0A\x1A\x1D\x1B\x1F\x4C\x1E\x07\x10\x13\x08\x09\x09\x4A\x6F\x50\x19"
"\x0D\x10\x12\x0B\x71\x14\x1C\x73\x04\x03\x19\x02\x16\x19\x0F\x18\x00\x06\x01\x1B\x09\x1F\x11\x49\x6D\x61\x15\x22\x30\x31\x13\x15\x1F\x0A\x5E\x41\x5C\x2E\x0B\x00\x04\x14\x1B\x57\x4A\x55\x3E\x19\x07\x1C\x0C\x03\x09\x1E\x0A\x0C\x0F\x15\x46\x59\x44\x2A\x2E\x33\x3D\x2B\x7E\x2B\x2E\x32\x27\x31\x3C\x34\x25"
"\x3F\x3B\x3A\x3E\x2E\x3A\x3A\x74\x6F\x05\x23\x36\x26\x32\x2F\x23\x33\x4E\x65\x5A\x18\x17\x17\x10\x1A\x1F\x09\x1B\x1C\x1E\x02\x5A\x57\x15\x00\x0E\x02\x07\x45\x4E\x1F\x1E\x04\x0C\x17\x05\x13\x4A\x47\x33\x0C\x34\x3F\x37\x2E\x2D\x7F\x09\x2D\x36\x32\x24\x34\x7A\x77\x16\x39\x3F\x2E\x3C\x26\x21\x3B\x24\x61"
"\x62\x28\x25\x38\x24\x28\x25\x37\x1E\x55\x56\x57\x74\x52\x5C\x29\x1A\x1A\x03\x51\x06\x05\x1B\x12\x18\x0A\x05\x53\x4E\x48\x18\x1F\x0D\x16\x02\x0D\x03\x14\x0C\x0A\x35\x2F\x7F\x79\x2E\x3E\x3B\x38\x72\x78\x70\x22\x26\x32\x37\x3C\x2C\x22\x2B\x69\x39\x26\x36\x2C\x30\x27\x33\x6F\x4C\x4D\x01\x13\x3F\x35\x2C"
"\x59\x28\x36\x39\x2A\x37\x21\x50\x5C\x56\x23\x3C\x30\x4A\x27\x27\x2E\x3D\x65\x41\x4D\x07\x15\x05\x0F\x12\x11\x13\x17\x74\x36\x2B\x3A\x7E\x77\x28\x35\x3B\x20\x70\x21\x24\x38\x33\x27\x2B\x26\x72\x69\x69\x2A\x3A\x28\x2C\x37\x6D\x37\x2F\x22\x33\x20\x08\x5C\x51\x43\x74\x5F\x5C\x2A\x1B\x1D\x14\x1E\x01\x04"
"\x54\x39\x05\x0C\x1B\x49\x50\x4F\x3F\x14\x11\x17\x05\x0C\x46\x06\x0A\x01\x7A\x1A\x28\x29\x32\x36\x3F\x3C\x26\x3A\x3F\x3F\x78\x77\x06\x30\x2E\x6B\x75\x69\x2B\x3D\x3E\x22\x30\x6F\x60\x38\x23\x2B\x28\x2A\x0D\x5B\x45\x59\x09\x1E\x0E\x13\x1B\x1D\x17\x5F\x7C\x5A\x54\x33\x03\x07\x1C\x0C\x1C\x4F\x0E\x14\x42"
"\x0F\x05\x17\x03\x0B\x44\x04\x34\x3F\x78\x3B\x27\x7F\x28\x34\x3F\x36\x7E\x71\x02\x3F\x31\x75\x39\x24\x3D\x3B\x2D\x2A\x6C\x23\x23\x2E\x25\x61\x6D\x67\x21\x33\x1F\x15\x0C\x59\x37\x3B\x5C\x56\x52\x17\x15\x02\x15\x05\x1D\x05\x1E\x02\x07\x07\x64\x4F\x4C\x0C\x10\x06\x40\x18\x09\x12\x16\x45\x29\x3E\x39\x2B"
"\x3D\x37\x7C\x29\x37\x21\x3D\x22\x6C\x77\x73\x31\x25\x28\x3B\x64\x3D\x2A\x2D\x3F\x21\x2B\x60\x7D\x23\x31\x21\x2B\x0E\x5B\x11\x1D\x40\x58\x5C\x1B\x1B\x1D\x14\x02\x56\x18\x12\x13\x03\x08\x01\x08\x02\x4F\x0D\x03\x11\x14\x05\x13\x15\x49\x6E\x6F\x08\x1E\x14\x10\x1F\x1D\x15\x11\x1B\x07\x09\x71\x1B\x18\x1A"
"\x1C\x1E\x04\x1A\x43\x63\x6F\x0D\x6D\x24\x31\x29\x24\x28\x23\x28\x3C\x5A\x0F\x11\x14\x1B\x13\x15\x13\x17\x53\x1F\x17\x56\x14\x06\x14\x19\x03\x0D\x1A\x4E\x0E\x02\x09\x42\x14\x01\x13\x08\x0E\x0A\x02\x29\x7B\x2F\x30\x2A\x37\x7C\x39\x37\x27\x31\x38\x3A\x24\x74\x7D\x3E\x23\x21\x3A\x6E\x3F\x3E\x22\x25\x31"
"\x21\x2C\x7C\x4D\x64\x65\x5D\x09\x1D\x15\x17\x1E\x1E\x14\x1E\x1A\x04\x08\x5B\x1A\x1B\x1B\x03\x1F\x07\x1B\x49\x46\x42\x67\x68\x37\x21\x32\x2D\x47\x29\x24\x14\x1A\x1F\x1C\x0C\x55\x71\x7D\x11\x27\x22\x3D\x7D\x04\x3C\x3C\x2C\x3F\x63\x0C\x3D\x2C\x62\x6D\x12\x26\x32\x27\x29\x35\x29\x24\x14\x18\x1D\x59\x0A"
"\x1E\x1E\x47\x52\x30\x20\x24\x59\x25\x35\x38\x45\x2F\x01\x1A\x05\x40\x2B\x3D\x37\x43\x15\x12\x07\x00\x01\x45\x3D\x29\x39\x29\x36\x2C\x72\x57\x7F\x73\x14\x34\x22\x36\x3D\x39\x39\x6B\x3C\x28\x2C\x75\x6C\x2B\x2B\x2D\x24\x61\x32\x2F\x21\x65\x0A\x09\x17\x1A\x1B\x0C\x0F\x5D\x17\x12\x04\x18\x18\x10\x54\x07"
"\x0F\x18\x07\x1C\x1C\x0C\x09\x1E\x59\x43\x12\x08\x01\x0F\x10\x48\x39\x37\x31\x3A\x35\x7F\x62\x7D\x17\x3D\x34\x71\x22\x36\x27\x3E\x64\x41\x65\x69\x1D\x3B\x2D\x3F\x36\x36\x30\x61\x32\x26\x26\x7F\x5A\x1F\x11\x0A\x1F\x1D\x10\x18\x52\x00\x1C\x1E\x01\x57\x07\x01\x0B\x19\x1C\x0C\x1C\x1C\x42\x67\x68\x30\x39"
"\x32\x32\x22\x29\x45\x1C\x12\x14\x1C\x7E\x0D\x19\x0D\x13\x1A\x02\x71\x7E\x3E\x3A\x75\x25\x39\x2C\x2C\x3C\x66\x46\x60\x62\x30\x26\x22\x66\x68\x37\x26\x1B\x15\x16\x16\x09\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x48\x15\x0E\x0E\x17\x45\x2A\x29\x37\x3E\x2C\x3E"
"\x31\x67\x72\x74\x23\x37\x35\x7A\x27\x36\x2B\x25\x26\x26\x39\x68\x65\x47\x6F\x63\x24\x28\x35\x2A\x64\x6A\x15\x15\x14\x10\x10\x1A\x5C\x52\x11\x1F\x15\x10\x18\x02\x04\x58\x03\x06\x09\x0E\x0B\x4F\x43\x1F\x07\x10\x14\x0E\x14\x02\x0C\x00\x3B\x37\x2C\x31\x7E\x7F\x7C\x75\x75\x37\x39\x22\x3B\x7A\x26\x30\x39"
"\x3F\x27\x3B\x2B\x68\x65\x47\x6F\x63\x23\x29\x2D\x23\x37\x2E\x5A\x38\x42\x59\x51\x19\x5C\x5D\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x48\x46\x02\x0E\x17\x0E\x77\x38\x30\x3C\x3D\x34\x7B\x74\x58\x7E\x70\x02\x2F\x24\x20\x30\x27\x6B\x1A\x2C\x3D\x3B\x23\x3F\x27\x63\x60\x61"
"\x66\x67\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x5E\x50\x07\x0C\x19\x1F\x0D\x04\x43\x1D\x09\x1E\x16\x0C\x12\x04\x41\x4E\x6E\x6F\x19\x17\x1D\x18\x10\x7F\x1E\x12\x1D\x07\x5A\x7C\x76\x13\x3D\x26\x2B\x29\x24\x2C\x6E\x2E\x20\x21\x62\x2D\x2F\x2F\x6B\x0A\x2D\x26\x08\x14\x0B\x16\x18\x0B\x5C"
"\x0E\x17\x01\x06\x18\x15\x12\x07\x55\x0B\x05\x0C\x49\x1D\x1B\x0D\x1F\x16\x16\x10\x41\x07\x17\x14\x16\x7A\x73\x35\x2A\x3D\x30\x32\x3B\x3B\x34\x70\x7A\x76\x03\x35\x26\x21\x41\x68\x69\x03\x2E\x22\x2C\x25\x26\x32\x68\x68\x67\x0D\x23\x5A\x0F\x10\x1C\x5E\x0F\x0E\x12\x10\x1F\x15\x1C\x56\x01\x15\x1B\x03\x18"
"\x00\x0C\x1D\x43\x4C\x1F\x07\x4E\x05\x0F\x07\x05\x08\x00\x7A\x33\x39\x35\x38\x7F\x3D\x29\x72\x32\x70\x25\x3F\x3A\x31\x7B\x6A\x1F\x20\x20\x3D\x45\x6C\x6D\x32\x31\x2F\x26\x34\x26\x29\x7F\x5A\x5C\x15\x0A\x1D\x10\x12\x1B\x1B\x14\x57\x5F\x7C\x7D\x27\x34\x2C\x2E\x48\x24\x21\x2B\x29\x67\x4F\x43\x22\x0E\x09"
"\x13\x17\x45\x2D\x32\x2C\x31\x7E\x32\x35\x33\x3B\x3E\x31\x3D\x76\x33\x26\x3C\x3C\x2E\x3A\x3A\x6E\x62\x6C\x24\x31\x2C\x2C\x20\x32\x22\x64\x21\x08\x12\x0E\x1C\x0C\x5F\x0A\x0E\x52\x00\x1F\x17\x02\x00\x15\x07\x0F\x4B\x01\x1A\x1D\x1A\x09\x1E\x4C\x43\x34\x09\x0F\x14\x6E\x45\x7A\x2B\x2A\x36\x39\x2D\x3D\x30"
"\x68\x73\x77\x22\x37\x31\x31\x78\x27\x24\x2C\x2C\x69\x6F\x64\x3F\x27\x30\x34\x20\x34\x33\x64\x2B\x1F\x1E\x1C\x1C\x1A\x56\x52\x77\x78\x3D\x3F\x51\x25\x38\x21\x3B\x2E\x61\x59\x40\x4E\x39\x03\x01\x17\x0E\x05\x41\x0B\x0E\x1C\x00\x28\x7B\x75\x79\x37\x2C\x7C\x29\x3A\x36\x70\x30\x26\x27\x74\x38\x3F\x3F\x2D"
"\x2D\x71\x6F\x64\x39\x2A\x2A\x33\x61\x36\x35\x2B\x22\x08\x1A\x15\x43\x5E\x58\x0A\x12\x1E\x06\x1D\x14\x5B\x1A\x1D\x0D\x0F\x19\x4F\x40\x64\x5D\x45\x4D\x26\x06\x06\x00\x13\x0B\x10\x45\x2A\x37\x39\x20\x3C\x3E\x3F\x36\x72\x37\x35\x27\x3F\x34\x31\x6A\x6A\x63\x1B\x2C\x3A\x3B\x25\x23\x25\x30\x60\x7F\x66\x14"
"\x3D\x36\x0E\x1E\x15\x59\x40\x5F\x2F\x12\x07\x1D\x14\x51\x48\x57\x3B\x00\x1E\x1B\x1D\x1D\x47\x65\x5F\x44\x42\x22\x15\x05\x0F\x08\x44\x11\x28\x34\x2D\x3B\x32\x3A\x2F\x35\x3D\x3C\x24\x34\x24\x79\x74\x61\x63\x6B\x1D\x39\x2A\x2E\x38\x28\x6D\x31\x25\x28\x28\x34\x30\x24\x16\x17\x58\x18\x0B\x1B\x15\x12\x52"
"\x17\x02\x18\x00\x12\x06\x5B\x4A\x5E\x41\x49\x2D\x07\x09\x0E\x09\x43\x14\x09\x03\x6D\x44\x45\x3E\x3E\x2E\x30\x3D\x3A\x7C\x34\x3C\x73\x14\x34\x20\x3E\x37\x30\x6A\x06\x29\x27\x2F\x28\x29\x3F\x62\x6B\x25\x39\x25\x2B\x25\x28\x1B\x0F\x11\x16\x10\x5F\x11\x1C\x00\x18\x4F\x58\x58\x7D\x7E\x3B\x25\x4B\x21\x27"
"\x3A\x2A\x3E\x23\x27\x37\x6A\x4C\x46\x34\x01\x00\x7A\x36\x39\x37\x2B\x3E\x30\x70\x3C\x36\x24\x26\x39\x25\x3F\x6F\x6A\x3B\x21\x27\x29\x6F\x3E\x22\x37\x37\x25\x33\x6A\x67\x27\x2D\x1F\x18\x13\x59\x3A\x31\x2F\x51\x52\x1A\x00\x12\x19\x19\x12\x1C\x0D\x4B\x47\x0F\x02\x1A\x1F\x05\x06\x0D\x13\x4D\x6C\x47\x44"
"\x0B\x3F\x2F\x2F\x36\x2C\x34\x7C\x2F\x37\x20\x35\x25\x78\x77\x00\x3D\x23\x38\x68\x39\x3C\x20\x2B\x3F\x23\x2E\x7A\x61\x61\x29\x21\x31\x0D\x14\x0A\x12\x53\x0D\x19\x0E\x17\x07\x57\x5D\x56\x50\x06\x10\x19\x0E\x1C\x44\x19\x06\x02\x1E\x0D\x00\x0B\x46\x48\x6D\x6E\x36\x16\x14\x0F\x79\x0E\x1C\x56\x70\x72\x00"
"\x35\x34\x76\x3A\x35\x3B\x3F\x2A\x24\x64\x3E\x2A\x3E\x2B\x2D\x31\x2D\x20\x28\x24\x21\x6B\x5A\x2F\x17\x09\x5E\x1C\x1D\x08\x01\x16\x03\x4B\x56\x11\x01\x19\x06\x4B\x0C\x00\x1D\x04\x40\x4D\x0F\x02\x0E\x18\x46\x14\x10\x04\x28\x2F\x2D\x29\x7E\x3E\x2C\x2D\x21\x7F\x5A\x71\x76\x35\x35\x36\x21\x2C\x3A\x26\x3B"
"\x21\x28\x6D\x37\x33\x24\x20\x32\x22\x37\x69\x5A\x16\x19\x15\x09\x1E\x0E\x18\x5E\x53\x1C\x1E\x01\x57\x26\x34\x27\x47\x48\x06\x18\x0A\x1E\x05\x07\x02\x14\x08\x08\x00\x4A\x6F\x50\x19\x14\x0C\x1B\x7F\x0F\x1E\x00\x16\x15\x1F\x5C\x7A\x74\x06\x2F\x2E\x68\x24\x2F\x21\x39\x2C\x2E\x6E\x32\x24\x25\x28\x32\x20"
"\x08\x02\x42\x59\x10\x10\x08\x18\x52\x07\x18\x14\x56\x04\x00\x1A\x1A\x4B\x0B\x06\x0A\x0A\x40\x4D\x01\x0B\x05\x02\x0D\x47\x08\x0A\x3D\x28\x78\x38\x30\x3B\x7C\x30\x37\x3E\x3F\x23\x2F\x7B\x5E\x75\x6A\x38\x2D\x28\x3C\x2C\x24\x6D\x36\x2B\x25\x61\x25\x28\x20\x20\x5A\x14\x16\x59\x0A\x17\x19\x5D\x1D\x15\x16"
"\x18\x15\x1E\x15\x19\x4A\x0F\x07\x0A\x1D\x4F\x44\x4A\x06\x0C\x03\x12\x4B\x14\x01\x04\x28\x38\x30\x7E\x77\x71\x56\x57\x1A\x12\x02\x15\x01\x16\x06\x10\x6A\x0F\x01\x08\x09\x01\x03\x1E\x16\x0A\x03\x12\x4C\x6A\x64\x08\x1F\x16\x17\x0B\x07\x45\x5C\x2A\x1B\x1D\x14\x1E\x01\x04\x54\x38\x0F\x06\x07\x1B\x17\x4F"
"\x28\x04\x03\x04\x0E\x0E\x15\x13\x0D\x06\x7A\x73\x2C\x31\x37\x2C\x7C\x2D\x20\x3C\x37\x23\x37\x3A\x6E\x75\x6D\x26\x2D\x24\x21\x3D\x35\x60\x26\x2A\x21\x26\x28\x28\x37\x31\x13\x18\x5F\x50\x50\x75\x51\x5D\x36\x1A\x03\x1A\x05\x4D\x54\x52\x0E\x02\x1B\x02\x43\x0C\x04\x08\x01\x08\x47\x41\x49\x47\x43\x01\x28"
"\x32\x2E\x3C\x73\x36\x32\x3B\x3D\x74\x70\x79\x05\x1A\x15\x07\x1E\x62\x66\x43\x63\x6F\x0B\x1D\x17\x63\x33\x35\x34\x22\x37\x36\x40\x5B\x5F\x1D\x06\x1B\x15\x1C\x15\x54\x50\x5E\x56\x30\x24\x20\x4A\x1D\x0D\x07\x0A\x00\x1E\x4D\x16\x0C\x0F\x0D\x15\x49\x6E\x6F\x09\x0F\x11\x15\x12\x7F\x0F\x09\x07\x10\x1B\x6E"
"\x5C\x7A\x74\x06\x2F\x2A\x3A\x2A\x26\x6F\x38\x25\x27\x63\x25\x39\x27\x24\x30\x65\x1F\x09\x0A\x16\x0C\x5F\x08\x18\x0A\x07\x50\x1E\x18\x1B\x1D\x1B\x0F\x4B\x40\x4E\x0A\x00\x0F\x1E\x4F\x10\x05\x00\x14\x04\x0C\x45\x66\x3E\x2A\x2B\x31\x2D\x62\x7A\x7B\x7D\x5A\x7C\x76\x16\x27\x3E\x6A\x2A\x68\x2F\x3C\x26\x29"
"\x23\x26\x63\x2F\x33\x66\x26\x64\x23\x15\x09\x0D\x14\x5E\x52\x5C\x0A\x1B\x07\x18\x51\x02\x1F\x11\x55\x0F\x19\x1A\x06\x1C\x4F\x18\x08\x1A\x17\x40\x00\x08\x03\x44\x20\x2C\x3E\x36\x2D\x7E\x09\x35\x38\x25\x36\x22\x71\x32\x36\x20\x34\x64\x41\x65\x69\x1C\x2A\x3F\x28\x36\x63\x34\x29\x2F\x34\x64\x15\x39\x5B"
"\x19\x0A\x5E\x1E\x5C\x11\x13\x00\x04\x51\x04\x12\x07\x1A\x18\x1F\x48\x44\x4E\x06\x18\x4D\x0B\x10\x40\x07\x07\x14\x10\x00\x28\x7B\x2C\x31\x3F\x31\x7C\x24\x3D\x26\x70\x25\x3E\x3E\x3A\x3E\x64"
);

MAN(devices, "Devices & Drivers Guide",
"\x1E\x1E\x0E\x10\x1D\x1A\x7C\x10\x13\x1D\x11\x16\x13\x05\x5E\x78\x6A\x2F\x2D\x3F\x23\x28\x21\x39\x6C\x2E\x33\x22\x66\x6F\x30\x2D\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x4A\x51\x51\x13\x11\x03\x03\x08\x0D\x44\x03\x0E\x02\x0C\x05\x06\x12\x46\x4F\x49\x44\x31\x32\x3E\x78\x34\x3F\x2C\x28\x38\x20\x73\x3C"
"\x38\x25\x23\x74\x3A\x2C\x6B\x31\x26\x3B\x3D\x46\x6D\x62\x2B\x21\x33\x22\x30\x25\x37\x1F\x55\x58\x38\x5E\x06\x19\x11\x1E\x1C\x07\x51\x02\x05\x1D\x14\x04\x0C\x04\x0C\x4E\x52\x4C\x1D\x10\x0C\x02\x0D\x03\x0A\x5F\x45\x28\x3E\x3C\x79\x06\x7F\x61\x7D\x36\x3A\x23\x30\x34\x3B\x31\x31\x64\x41\x42\x1B\x0B\x0E"
"\x08\x04\x0C\x04\x60\x15\x0E\x02\x64\x16\x2E\x3A\x2C\x2C\x2D\x75\x51\x5D\x20\x1A\x17\x19\x02\x5A\x17\x19\x03\x08\x03\x49\x0F\x4F\x08\x08\x14\x0A\x03\x04\x46\x59\x44\x35\x28\x34\x28\x3C\x2C\x2B\x35\x38\x21\x73\x6E\x71\x11\x32\x3A\x30\x38\x2A\x24\x73\x6E\x3C\x38\x2C\x36\x36\x33\x61\x32\x22\x3C\x31\x5A"
"\x50\x58\x1C\x0C\x0D\x13\x0F\x52\x10\x1F\x15\x13\x7D\x54\x55\x42\x28\x07\x0D\x0B\x4F\x5D\x41\x42\x20\x0F\x05\x03\x47\x55\x55\x76\x7B\x1B\x36\x3A\x3A\x7C\x6C\x6B\x7D\x7E\x7F\x7F\x79\x5E\x78\x6A\x6C\x1C\x21\x27\x3C\x6C\x29\x27\x35\x29\x22\x23\x67\x27\x24\x14\x15\x17\x0D\x5E\x0C\x08\x1C\x00\x07\x50\x59"
"\x35\x18\x10\x10\x4A\x5A\x58\x40\x49\x4F\x19\x1E\x17\x02\x0C\x0D\x1F\x47\x09\x00\x3B\x35\x2B\x79\x3F\x7F\x38\x2F\x3B\x25\x35\x23\x76\x27\x26\x3A\x28\x27\x2D\x24\x6E\x20\x3E\x47\x62\x63\x28\x20\x34\x23\x33\x24\x08\x1E\x58\x1F\x1F\x16\x10\x08\x00\x16\x5E\x51\x25\x12\x15\x07\x09\x03\x48\x1D\x06\x0A\x4C"
"\x0E\x0D\x07\x05\x41\x09\x09\x44\x11\x32\x3E\x78\x3D\x31\x3C\x2F\x7D\x7A\x74\x34\x3E\x35\x24\x79\x26\x2F\x2A\x3A\x2A\x26\x68\x65\x63\x48\x49\x15\x11\x02\x06\x10\x0C\x34\x3C\x58\x3D\x2C\x36\x2A\x38\x20\x20\x7A\x5C\x56\x25\x1D\x12\x02\x1F\x45\x0A\x02\x06\x0F\x06\x42\x07\x05\x17\x0F\x04\x01\x45\x64\x7B"
"\x0D\x29\x3A\x3E\x28\x38\x72\x37\x22\x38\x20\x32\x26\x75\x74\x6B\x6F\x1A\x2B\x2E\x3E\x2E\x2A\x63\x21\x34\x32\x28\x29\x24\x0E\x12\x1B\x18\x12\x13\x05\x5A\x52\x5B\x05\x02\x13\x04\x7E\x55\x4A\x3C\x01\x07\x0A\x00\x1B\x1E\x42\x36\x10\x05\x07\x13\x01\x45\x71\x7B\x34\x36\x3D\x3E\x30\x74\x7C\x59\x7D\x71\x71"
"\x15\x26\x3A\x3D\x38\x2D\x69\x23\x36\x6C\x2E\x2D\x2E\x30\x34\x32\x22\x36\x62\x40\x5B\x1E\x16\x0C\x5F\x11\x1C\x1C\x06\x11\x1D\x1A\x0E\x54\x11\x05\x1C\x06\x05\x01\x0E\x08\x08\x06\x43\x04\x13\x0F\x11\x01\x17\x7A\x2B\x39\x3A\x35\x3E\x3B\x38\x21\x7D\x5A\x7C\x76\x1A\x35\x3B\x3F\x2D\x29\x2A\x3A\x3A\x3E\x28"
"\x30\x63\x33\x28\x32\x22\x37\x65\x52\x3C\x28\x2C\x44\x5F\x32\x2B\x3B\x37\x39\x30\x59\x36\x39\x31\x45\x22\x06\x1D\x0B\x03\x45\x4D\x0D\x05\x06\x04\x14\x47\x10\x0D\x3F\x7B\x36\x3C\x29\x3A\x2F\x29\x72\x37\x22\x38\x20\x32\x26\x26\x6A\x66\x42\x69\x6E\x3A\x3F\x28\x24\x36\x2C\x61\x20\x28\x36\x65\x1D\x1A\x15"
"\x10\x10\x18\x5C\x1C\x1C\x17\x50\x02\x06\x12\x17\x1C\x0C\x02\x0B\x49\x08\x06\x14\x08\x11\x4D\x6A\x6B\x34\x28\x28\x29\x7A\x19\x19\x1A\x15\x55\x71\x7D\x16\x36\x26\x38\x35\x32\x74\x6B\x6A\x1B\x3A\x26\x3E\x2A\x3E\x39\x2B\x26\x33\x61\x78\x67\x00\x37\x13\x0D\x1D\x0B\x5E\x41\x5C\x2F\x1D\x1F\x1C\x51\x34\x16"
"\x17\x1E\x4A\x2F\x1A\x00\x18\x0A\x1E\x57\x42\x11\x05\x17\x03\x15\x10\x45\x2E\x33\x3D\x79\x32\x3E\x2F\x29\x58\x73\x70\x24\x26\x33\x35\x21\x2F\x6B\x21\x2F\x6E\x2E\x6C\x29\x27\x35\x29\x22\x23\x67\x26\x37\x15\x10\x1D\x59\x1F\x19\x08\x18\x00\x53\x11\x1F\x56\x02\x04\x11\x0B\x1F\x0D\x47\x64\x65\x28\x24\x31"
"\x22\x22\x2D\x23\x47\x4B\x45\x1F\x15\x19\x1B\x12\x1A\x7C\x72\x72\x06\x1E\x18\x18\x04\x00\x14\x06\x07\x42\x64\x6E\x0B\x25\x3E\x23\x21\x2C\x24\x7C\x67\x30\x2D\x1F\x5B\x1C\x1C\x08\x16\x1F\x18\x52\x00\x04\x10\x0F\x04\x54\x1C\x04\x18\x1C\x08\x02\x03\x09\x09\x42\x01\x15\x15\x46\x03\x0B\x00\x29\x7B\x36\x36"
"\x2A\x7F\x2B\x32\x20\x38\x70\x7C\x76\x3F\x35\x3B\x2E\x32\x68\x2F\x21\x3D\x46\x6D\x62\x20\x2F\x2F\x20\x2B\x2D\x26\x0E\x12\x16\x1E\x5E\x1E\x18\x1C\x02\x07\x15\x03\x05\x59\x7E\x58\x4A\x3E\x06\x00\x00\x1C\x18\x0C\x0E\x0F\x5A\x41\x14\x02\x09\x0A\x2C\x3E\x2B\x79\x2A\x37\x39\x7D\x36\x21\x39\x27\x33\x25\x7A"
"\x75\x6D\x18\x2B\x28\x20\x6F\x2A\x22\x30\x63\x28\x20\x34\x23\x33\x24\x08\x1E\x58\x1A\x16\x1E\x12\x1A\x17\x00\x57\x51\x04\x12\x59\x11\x0F\x1F\x0D\x0A\x1A\x1C\x66\x4D\x42\x17\x08\x04\x46\x03\x01\x13\x33\x38\x3D\x79\x3F\x31\x38\x7D\x20\x36\x39\x3F\x25\x23\x35\x39\x26\x38\x68\x3D\x26\x2A\x6C\x29\x27\x25"
"\x21\x34\x2A\x33\x64\x21\x08\x12\x0E\x1C\x0C\x5F\x51\x5D\x13\x53\x13\x1D\x17\x04\x07\x1C\x09\x4B\x0E\x00\x16\x41\x66\x67\x31\x20\x21\x2F\x46\x21\x2B\x37\x7A\x13\x19\x0B\x1A\x08\x1D\x0F\x17\x73\x13\x19\x17\x19\x13\x10\x19\x41\x65\x69\x0F\x2C\x38\x24\x2D\x2D\x60\x2C\x23\x29\x31\x65\x44\x5B\x2B\x1A\x1F"
"\x11\x5C\x1B\x1D\x01\x50\x19\x17\x05\x10\x02\x0B\x19\x0D\x49\x0D\x07\x0D\x03\x05\x06\x13\x41\x4B\x47\x10\x00\x36\x37\x2B\x79\x09\x36\x32\x39\x3D\x24\x23\x71\x22\x38\x74\x39\x25\x24\x23\x69\x28\x20\x3E\x47\x62\x63\x2E\x24\x31\x67\x2B\x37\x5A\x16\x11\x0A\x0D\x16\x12\x1A\x52\x17\x15\x07\x1F\x14\x11\x06"
"\x4A\x0A\x0F\x08\x07\x01\x42\x67\x68\x30\x28\x2E\x31\x47\x2C\x2C\x1E\x1F\x1D\x17\x7E\x1B\x19\x0B\x1B\x10\x15\x02\x5C\x7A\x74\x03\x23\x2E\x3F\x69\x70\x6F\x1F\x25\x2D\x34\x60\x29\x2F\x23\x20\x20\x14\x5B\x1C\x1C\x08\x16\x1F\x18\x01\x49\x50\x02\x1E\x18\x03\x06\x4A\x0C\x00\x06\x1D\x1B\x4C\x09\x07\x15\x09"
"\x02\x03\x14\x44\x4D\x2F\x35\x28\x35\x2B\x38\x3B\x38\x36\x73\x05\x02\x14\x5D\x74\x75\x29\x24\x26\x3D\x3C\x20\x20\x21\x27\x31\x33\x61\x23\x33\x27\x6B\x53\x55\x58\x2B\x1B\x12\x13\x0B\x1B\x1D\x17\x51\x11\x1F\x1B\x06\x1E\x18\x48\x0A\x0F\x01\x4C\x0B\x0B\x1B\x40\x02\x09\x09\x02\x09\x33\x38\x2C\x2A\x70\x55"
"\x56\x19\x00\x1A\x06\x14\x04\x77\x10\x10\x1E\x0A\x01\x05\x1D\x45\x61\x6D\x06\x26\x36\x28\x25\x22\x64\x7B\x5A\x2B\x0A\x16\x0E\x1A\x0E\x09\x1B\x16\x03\x51\x48\x57\x30\x07\x03\x1D\x0D\x1B\x4E\x51\x4C\x29\x10\x0A\x16\x04\x14\x47\x20\x00\x2E\x3A\x31\x35\x2D\x65\x7C\x2A\x3A\x3A\x33\x39\x76\x79\x27\x2C\x39"
"\x6B\x2E\x20\x22\x2A\x3F\x6D\x20\x22\x23\x2A\x4C\x67\x64\x31\x12\x12\x0B\x59\x1A\x1A\x0A\x14\x11\x16\x5E\x7B\x5B\x57\x37\x1A\x07\x06\x09\x07\x0A\x4F\x00\x04\x0C\x06\x5A\x41\x02\x15\x0D\x13\x3F\x29\x29\x2C\x3B\x2D\x25\x7D\x7F\x25\x70\x79\x22\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x78\x63\x67\x25"
"\x34\x2E\x32\x20\x08\x56\x14\x10\x0D\x0B\x5B\x54\x5C\x79\x5D\x51\x06\x19\x04\x00\x1E\x02\x04\x49\x41\x0A\x02\x18\x0F\x4E\x04\x13\x0F\x11\x01\x17\x29\x7B\x34\x30\x2D\x2B\x2F\x7D\x26\x3B\x35\x71\x32\x25\x3D\x23\x2F\x39\x68\x3A\x3A\x20\x3E\x28\x6C\x49\x4A\x05\x14\x0E\x12\x00\x28\x5B\x2B\x30\x39\x31\x35"
"\x33\x35\x79\x5D\x51\x21\x1E\x1A\x11\x05\x1C\x1B\x49\x1C\x0A\x1D\x18\x0B\x11\x05\x12\x46\x14\x0D\x02\x34\x3E\x3C\x79\x3A\x2D\x35\x2B\x37\x21\x23\x7F\x76\x02\x3A\x26\x23\x2C\x26\x2C\x2A\x6F\x28\x3F\x2B\x35\x25\x33\x35\x67\x22\x24\x13\x17\x58\x0D\x11\x5F\x15\x13\x01\x07\x11\x1D\x1A\x4C\x7E\x55\x4A\x12"
"\x07\x1C\x4E\x0C\x0D\x03\x42\x05\x0F\x13\x05\x02\x49\x09\x35\x3A\x3C\x79\x2A\x37\x39\x30\x72\x3C\x3E\x3D\x2F\x77\x22\x3C\x2B\x6B\x29\x2D\x38\x2E\x22\x2E\x27\x27\x60\x23\x29\x28\x30\x65\x15\x0B\x0C\x10\x11\x11\x0F\x5D\x5F\x53\x11\x07\x19\x1E\x10\x5B\x60\x61\x3D\x3A\x2C\x4F\x4A\x4D\x20\x2F\x35\x24\x32"
"\x28\x2B\x31\x12\x7B\x11\x0A\x0D\x0A\x19\x0E\x58\x7E\x70\x05\x24\x2E\x74\x34\x24\x24\x3C\x21\x2B\x3D\x6C\x3D\x2D\x31\x34\x7A\x66\x32\x34\x21\x1B\x0F\x1D\x59\x1D\x17\x15\x0D\x01\x16\x04\x5E\x23\x24\x36\x55\x0E\x19\x01\x1F\x0B\x1D\x1F\x56\x42\x11\x05\x0C\x09\x11\x01\x45\x3D\x33\x37\x2A\x2A\x7F\x38\x38"
"\x24\x3A\x33\x34\x25\x6C\x5E\x75\x6A\x3E\x26\x20\x20\x3C\x38\x2C\x2E\x2F\x60\x35\x2E\x22\x64\x21\x1F\x0D\x11\x1A\x1B\x5F\x1D\x13\x16\x53\x02\x14\x05\x14\x15\x1B\x44\x61\x45\x49\x2C\x03\x19\x08\x16\x0C\x0F\x15\x0E\x5D\x44\x36\x3F\x2F\x2C\x30\x30\x38\x2F\x7D\x6C\x73\x12\x3D\x23\x32\x20\x3A\x25\x3F\x20"
"\x69\x68\x6F\x28\x28\x34\x2A\x23\x24\x35\x7C\x64\x37\x1F\x16\x17\x0F\x1B\x5F\x57\x5D\x00\x16\x5D\x01\x17\x1E\x06\x55\x1E\x03\x0D\x49\x0A\x0A\x1A\x04\x01\x06\x5B\x6B\x46\x47\x09\x04\x31\x3E\x78\x2A\x2B\x2D\x39\x7D\x33\x3A\x22\x21\x3A\x36\x3A\x30\x6A\x26\x27\x2D\x2B\x6F\x25\x3E\x62\x2C\x26\x27\x68\x4D"
"\x4E\x15\x28\x32\x36\x2D\x3B\x2D\x2F\x5D\x54\x53\x23\x32\x37\x39\x3A\x30\x38\x38\x62\x44\x4E\x3C\x09\x19\x16\x0A\x0E\x06\x15\x47\x5A\x45\x18\x37\x2D\x3C\x2A\x30\x33\x29\x3A\x73\x76\x71\x32\x32\x22\x3C\x29\x2E\x3B\x69\x70\x6F\x1C\x3F\x2B\x2D\x34\x24\x34\x34\x64\x63\x5A\x08\x1B\x18\x10\x11\x19\x0F\x01"
"\x53\x4E\x51\x37\x13\x10\x55\x0E\x0E\x1E\x00\x0D\x0A\x42\x67\x4F\x43\x34\x13\x09\x12\x06\x09\x3F\x28\x30\x36\x31\x2B\x66\x7D\x20\x36\x23\x25\x37\x25\x20\x75\x3E\x23\x2D\x69\x3D\x3F\x23\x22\x2E\x26\x32\x61\x35\x22\x36\x33\x13\x18\x1D\x59\x56\x0B\x14\x14\x01\x53\x00\x03\x19\x10\x06\x14\x07\x51\x62\x49"
"\x4E\x48\x1F\x08\x10\x15\x09\x02\x03\x14\x49\x17\x3F\x28\x2C\x38\x2C\x2B\x7C\x2E\x22\x3C\x3F\x3D\x33\x25\x73\x7C\x66\x6B\x2B\x25\x2B\x2E\x3E\x6D\x36\x2B\x25\x61\x35\x37\x2B\x2A\x16\x1E\x0A\x59\x0F\x0A\x19\x08\x17\x53\x58\x56\x15\x1B\x11\x14\x18\x46\x18\x1B\x07\x01\x18\x40\x11\x13\x0F\x0E\x0A\x02\x16"
"\x42\x73\x77\x52\x79\x7E\x2D\x39\x34\x3C\x20\x24\x30\x3A\x3B\x74\x21\x22\x2E\x68\x2D\x3C\x26\x3A\x28\x30\x6F\x60\x22\x2E\x22\x27\x2E\x5A\x0F\x10\x1C\x5E\x0F\x0E\x14\x1C\x07\x15\x03\x51\x04\x54\x1A\x1D\x05\x48\x1A\x1A\x0E\x18\x18\x11\x43\x10\x00\x01\x02\x4A\x6F\x50\x16\x17\x17\x17\x0B\x13\x0F\x01\x59"
"\x7D\x71\x12\x3E\x27\x25\x26\x2A\x31\x69\x3D\x2A\x38\x39\x2B\x2D\x27\x32\x7C\x67\x36\x20\x09\x14\x14\x0C\x0A\x16\x13\x13\x5E\x53\x02\x14\x10\x05\x11\x06\x02\x4B\x1A\x08\x1A\x0A\x40\x4D\x11\x00\x01\x0D\x0F\x09\x03\x49\x7A\x13\x1C\x0B\x72\x7F\x33\x2F\x3B\x36\x3E\x25\x37\x23\x3D\x3A\x24\x67\x42\x69\x6E"
"\x22\x39\x21\x36\x2A\x30\x2D\x23\x67\x29\x2A\x14\x12\x0C\x16\x0C\x0C\x5C\x55\x06\x1B\x19\x02\x56\x07\x06\x1A\x0D\x19\x09\x04\x54\x4F\x4B\x00\x0D\x0D\x09\x15\x09\x15\x17\x48\x36\x32\x2B\x2D\x79\x73\x7C\x7A\x3F\x26\x3C\x25\x3F\x27\x38\x30\x67\x2F\x21\x3A\x3E\x23\x2D\x34\x31\x64\x69\x6F\x4C\x6A\x64\x12"
"\x1F\x1A\x13\x56\x10\x10\x5C\x0E\x1B\x14\x1E\x10\x1A\x4D\x54\x16\x02\x0E\x0B\x02\x4E\x0C\x0D\x0F\x0E\x06\x13\x4D\x46\x13\x16\x1C\x7A\x3A\x36\x36\x2A\x37\x39\x2F\x72\x23\x3F\x23\x22\x7B\x74\x27\x2F\x38\x2D\x28\x3A\x6F\x38\x25\x27\x63\x07\x11\x13\x69"
);

MAN(boot, "Boot, UEFI & BIOS Guide",
"\x1C\x12\x0A\x14\x09\x1E\x0E\x18\x72\x11\x11\x02\x1F\x14\x07\x5F\x67\x6B\x1D\x0C\x08\x06\x6C\x65\x2F\x2C\x24\x24\x34\x29\x68\x65\x09\x12\x16\x1A\x1B\x5F\x02\x4F\x42\x42\x42\x58\x56\x05\x11\x05\x06\x0A\x0B\x0C\x0A\x4F\x18\x05\x07\x43\x0F\x0D\x02\x47\x26\x2C\x15\x08\x76\x79\x17\x2B\x7C\x31\x3B\x25\x35"
"\x22\x76\x38\x3A\x75\x3E\x23\x2D\x43\x6E\x6F\x21\x22\x36\x2B\x25\x33\x24\x28\x25\x37\x1E\x57\x58\x10\x10\x16\x08\x14\x13\x1F\x19\x0B\x13\x04\x54\x1D\x0B\x19\x0C\x1E\x0F\x1D\x09\x4D\x03\x0D\x04\x41\x0A\x08\x05\x01\x29\x7B\x0F\x30\x30\x3B\x33\x2A\x21\x7D\x5A\x7C\x76\x04\x31\x36\x3F\x39\x2D\x69\x0C\x20"
"\x23\x39\x78\x63\x2F\x2F\x2A\x3E\x64\x36\x13\x1C\x16\x1C\x1A\x5F\x1E\x12\x1D\x07\x50\x1D\x19\x16\x10\x10\x18\x18\x48\x08\x1C\x0A\x4C\x0C\x0E\x0F\x0F\x16\x03\x03\x44\x48\x7A\x30\x3D\x3C\x2E\x2C\x7C\x2F\x3D\x3C\x24\x3A\x3F\x23\x27\x75\x25\x3E\x3C\x67\x44\x62\x6C\x19\x12\x0E\x60\x73\x68\x77\x7E\x65\x1B"
"\x5B\x0B\x1C\x1D\x0A\x0E\x14\x06\x0A\x50\x12\x1E\x1E\x04\x55\x18\x0E\x19\x1C\x07\x1D\x09\x09\x42\x01\x19\x41\x31\x0E\x0A\x01\x35\x2C\x2B\x79\x6F\x6E\x67\x7D\x27\x20\x35\x35\x76\x31\x3B\x27\x6A\x09\x21\x3D\x02\x20\x2F\x26\x27\x31\x6C\x4B\x66\x67\x13\x2C\x14\x1F\x17\x0E\x0D\x5F\x34\x18\x1E\x1F\x1F\x51"
"\x17\x19\x10\x55\x0B\x1F\x1C\x0C\x1D\x1B\x0D\x19\x0B\x0C\x0E\x4F\x6C\x6D\x21\x2B\x0E\x1E\x0A\x10\x10\x18\x7C\x09\x1A\x16\x70\x17\x1F\x05\x19\x02\x0B\x19\x0D\x69\x66\x0D\x05\x02\x11\x6C\x15\x04\x00\x0E\x6D\x4F\x57\x5B\x3E\x0B\x11\x12\x5C\x2A\x1B\x1D\x14\x1E\x01\x04\x4E\x55\x39\x0E\x1C\x1D\x07\x01\x0B"
"\x1E\x42\x5D\x40\x32\x1F\x14\x10\x00\x37\x7B\x66\x79\x0C\x3A\x3F\x32\x24\x36\x22\x28\x76\x69\x74\x14\x2E\x3D\x29\x27\x2D\x2A\x28\x6D\x31\x37\x21\x33\x32\x32\x34\x65\x44\x5B\x2A\x1C\x0D\x0B\x1D\x0F\x06\x79\x50\x51\x18\x18\x03\x55\x54\x4B\x3C\x1B\x01\x1A\x0E\x01\x07\x10\x08\x0E\x09\x13\x44\x5B\x7A\x1A"
"\x3C\x2F\x3F\x31\x3F\x38\x36\x73\x6E\x71\x03\x12\x12\x1C\x6A\x0D\x21\x3B\x23\x38\x2D\x3F\x27\x63\x13\x24\x32\x33\x2D\x2B\x1D\x08\x56\x59\x2A\x17\x15\x0E\x52\x03\x02\x1E\x11\x05\x15\x18\x50\x61\x48\x49\x49\x0E\x08\x1B\x03\x0D\x03\x04\x02\x4A\x17\x11\x3B\x29\x2C\x2C\x2E\x78\x72\x57\x7F\x73\x16\x23\x39"
"\x3A\x74\x36\x25\x27\x2C\x69\x2C\x20\x23\x39\x78\x63\x30\x33\x23\x34\x37\x65\x3E\x1E\x14\x56\x38\x4D\x53\x3B\x43\x43\x5F\x37\x47\x45\x54\x11\x0F\x1B\x0D\x07\x0A\x06\x02\x0A\x42\x0C\x0E\x41\x12\x0F\x01\x45\x38\x29\x39\x37\x3A\x7F\x71\x7D\x25\x32\x24\x32\x3E\x77\x20\x3D\x2F\x41\x68\x69\x3D\x2C\x3E\x28"
"\x27\x2D\x60\x27\x29\x35\x64\x31\x12\x1E\x58\x09\x0C\x10\x11\x0D\x06\x5D\x7A\x7B\x21\x3F\x35\x21\x4A\x32\x27\x3C\x4E\x2C\x2D\x23\x42\x20\x28\x20\x28\x20\x21\x45\x0E\x13\x1D\x0B\x1B\x55\x71\x7D\x10\x3C\x3F\x25\x76\x38\x26\x31\x2F\x39\x68\x61\x1B\x1C\x0E\x6D\x24\x2A\x32\x32\x32\x67\x30\x2A\x5A\x12\x16"
"\x0A\x0A\x1E\x10\x11\x52\x15\x02\x1E\x1B\x57\x15\x55\x19\x1F\x01\x0A\x05\x54\x4C\x19\x0A\x06\x0E\x41\x04\x06\x07\x0E\x7A\x2F\x37\x79\x3A\x36\x2F\x36\x7B\x7D\x5A\x7C\x76\x04\x31\x36\x3F\x39\x2D\x69\x0C\x20\x23\x39\x62\x2C\x2E\x6E\x29\x21\x22\x7E\x5A\x2F\x28\x34\x5E\x0C\x08\x1C\x06\x06\x03\x4A\x56\x01"
"\x1D\x07\x1E\x1E\x09\x05\x07\x15\x0D\x19\x0B\x0C\x0E\x41\x4E\x31\x30\x48\x22\x74\x19\x14\x1A\x72\x0A\x7D\x7F\x73\x3E\x34\x33\x33\x31\x31\x40\x6B\x68\x2F\x21\x3D\x6C\x05\x3B\x33\x25\x33\x6B\x11\x68\x65\x2D\x28\x34\x4B\x52\x5F\x0F\x1C\x1C\x17\x12\x1E\x0E\x12\x07\x5C\x51\x4B\x30\x24\x3E\x40\x29\x35\x32"
"\x2C\x40\x0C\x03\x0A\x0B\x17\x23\x7B\x28\x2B\x31\x39\x35\x31\x37\x20\x6B\x71\x30\x36\x3A\x75\x29\x3E\x3A\x3F\x2B\x3C\x77\x47\x62\x63\x37\x20\x2D\x22\x69\x2A\x14\x56\x34\x38\x30\x51\x76\x50\x52\x26\x00\x15\x17\x03\x11\x55\x0C\x02\x1A\x04\x19\x0E\x1E\x08\x42\x15\x09\x00\x46\x13\x0C\x00\x7A\x36\x37\x2D"
"\x36\x3A\x2E\x3F\x3D\x32\x22\x35\x76\x23\x3B\x3A\x26\x6B\x27\x3B\x6E\x2E\x6C\x18\x11\x01\x60\x32\x32\x2E\x27\x2E\x5A\x56\x58\x1F\x11\x13\x10\x12\x05\x53\x04\x19\x13\x7D\x54\x55\x07\x0A\x03\x0C\x1C\x48\x1F\x4D\x0B\x0D\x13\x15\x14\x12\x07\x11\x33\x34\x36\x2A\x7E\x3A\x24\x3C\x31\x27\x3C\x28\x6D\x77\x30"
"\x3A\x6A\x25\x27\x3D\x6E\x26\x22\x39\x27\x31\x32\x34\x36\x33\x64\x31\x12\x1E\x58\x09\x0C\x10\x1F\x18\x01\x00\x5E\x7B\x7C\x35\x3B\x3A\x3E\x4B\x25\x2C\x20\x3A\x4C\x45\x23\x27\x36\x20\x28\x24\x21\x21\x7A\x08\x0C\x18\x0C\x0B\x09\x0D\x7B\x59\x7D\x71\x05\x23\x35\x27\x3E\x3E\x38\x69\x1C\x2A\x3C\x2C\x2B\x31"
"\x6C\x61\x15\x33\x25\x37\x0E\x0E\x08\x59\x2D\x1A\x08\x09\x1B\x1D\x17\x02\x56\x5F\x07\x14\x0C\x0E\x48\x04\x01\x0B\x09\x44\x4E\x43\x23\x0E\x0B\x0A\x05\x0B\x3E\x7B\x08\x2B\x31\x32\x2C\x29\x7E\x73\x05\x14\x10\x1E\x5E\x75\x6A\x38\x2D\x3D\x3A\x26\x22\x2A\x31\x6F\x60\x12\x3F\x34\x30\x20\x17\x5B\x31\x14\x1F"
"\x18\x19\x5D\x20\x16\x13\x1E\x00\x12\x06\x0C\x46\x4B\x3D\x07\x07\x01\x1F\x19\x03\x0F\x0C\x41\x33\x17\x00\x04\x2E\x3E\x2B\x77\x7E\x1B\x39\x29\x33\x3A\x3C\x22\x76\x3E\x3A\x5F\x6A\x6B\x25\x28\x20\x3A\x2D\x21\x6F\x31\x25\x22\x29\x31\x21\x37\x03\x55\x72\x73\x3C\x30\x33\x29\x52\x30\x3F\x3F\x30\x3E\x33\x20"
"\x38\x2A\x3C\x20\x21\x21\x4C\x39\x2D\x2C\x2C\x32\x6C\x4A\x44\x08\x29\x38\x37\x37\x38\x36\x3B\x7D\x6C\x73\x12\x3E\x39\x23\x74\x21\x2B\x29\x72\x69\x39\x27\x25\x2E\x2A\x63\x0F\x12\x66\x25\x2B\x2A\x0E\x08\x54\x59\x0A\x16\x11\x18\x1D\x06\x04\x5D\x56\x04\x15\x13\x0F\x4B\x0A\x06\x01\x1B\x4C\x02\x12\x17\x09"
"\x0E\x08\x14\x48\x6F\x7A\x7B\x36\x2C\x33\x3D\x39\x2F\x72\x3C\x36\x71\x26\x25\x3B\x36\x2F\x38\x3B\x26\x3C\x3C\x60\x6D\x2F\x22\x38\x28\x2B\x32\x29\x65\x17\x1E\x15\x16\x0C\x06\x5C\x55\x06\x1B\x19\x02\x56\x07\x06\x1A\x0D\x19\x09\x04\x54\x4F\x4B\x00\x11\x00\x0F\x0F\x00\x0E\x03\x42\x73\x75\x52\x74\x7E\x3D"
"\x3F\x39\x37\x37\x39\x25\x76\x7F\x35\x31\x27\x22\x26\x60\x74\x6F\x2D\x29\x34\x22\x2E\x22\x23\x23\x64\x27\x15\x14\x0C\x59\x12\x10\x1D\x19\x17\x01\x50\x12\x19\x19\x12\x1C\x0D\x4B\x40\x0B\x0D\x0B\x09\x09\x0B\x17\x40\x4E\x03\x09\x11\x08\x73\x75\x52\x74\x7E\x3D\x33\x32\x26\x21\x35\x32\x76\x78\x32\x3C\x32"
"\x26\x2A\x3B\x62\x6F\x63\x2B\x2B\x3B\x22\x2E\x29\x33\x68\x65\x55\x09\x1D\x1B\x0B\x16\x10\x19\x10\x10\x14\x4B\x56\x05\x11\x05\x0B\x02\x1A\x49\x0C\x00\x03\x19\x42\x11\x05\x02\x09\x15\x00\x16\x7A\x3D\x2A\x36\x33\x7F\x28\x35\x37\x59\x70\x71\x24\x32\x37\x3A\x3C\x2E\x3A\x30\x6E\x0C\x23\x20\x2F\x22\x2E\x25"
"\x66\x17\x36\x2A\x17\x0B\x0C\x57\x74\x75\x38\x28\x33\x3F\x50\x33\x39\x38\x20\x7F\x47\x4B\x21\x07\x1D\x1B\x0D\x01\x0E\x43\x01\x0F\x09\x13\x0C\x00\x28\x7B\x17\x0A\x7E\x30\x32\x7D\x33\x73\x23\x34\x26\x36\x26\x34\x3E\x2E\x68\x39\x2F\x3D\x38\x24\x36\x2A\x2F\x2F\x66\x28\x36\x65\x1E\x09\x11\x0F\x1B\x51\x5C"
"\x2A\x1B\x1D\x14\x1E\x01\x04\x54\x26\x0F\x1F\x1D\x19\x4E\x00\x1E\x67\x42\x43\x14\x09\x03\x47\x2B\x36\x7A\x32\x36\x2A\x2A\x3E\x30\x31\x37\x21\x70\x3C\x37\x39\x35\x32\x2F\x38\x68\x3D\x26\x2A\x6C\x2F\x2D\x2C\x34\x61\x2B\x22\x2A\x30\x5A\x53\x3F\x2B\x2B\x3D\x53\x3F\x1D\x1C\x04\x51\x3B\x16\x1A\x14\x0D\x0E"
"\x1A\x40\x40\x65\x41\x4D\x35\x0A\x0E\x05\x09\x10\x17\x45\x18\x34\x37\x2D\x7E\x12\x3D\x33\x33\x34\x35\x23\x76\x3B\x3D\x26\x3E\x38\x68\x2B\x21\x3B\x24\x6D\x31\x3A\x33\x35\x23\x2A\x37\x7E\x5A\x1F\x1D\x1F\x1F\x0A\x10\x09\x52\x58\x50\x05\x1F\x1A\x11\x1A\x1F\x1F\x48\x0A\x01\x01\x0A\x04\x05\x16\x12\x00\x04"
"\x0B\x01\x6F\x7A\x7B\x31\x37\x7E\x32\x2F\x3E\x3D\x3D\x36\x38\x31\x77\x3B\x27\x6A\x29\x2B\x2D\x2B\x2B\x25\x39\x6C\x49\x4A\x03\x09\x08\x10\x65\x2A\x29\x37\x3B\x32\x3A\x31\x2E\x52\x5E\x50\x37\x3F\x25\x27\x21\x4A\x2A\x21\x2D\x64\x42\x4C\x4A\x2C\x0C\x40\x03\x09\x08\x10\x04\x38\x37\x3D\x79\x3A\x3A\x2A\x34"
"\x31\x36\x77\x6B\x76\x34\x3C\x30\x29\x20\x68\x2B\x21\x20\x38\x6D\x2D\x31\x24\x24\x34\x6B\x64\x26\x1B\x19\x14\x1C\x52\x5F\x18\x14\x01\x18\x50\x19\x13\x16\x18\x01\x02\x4B\x40\x1D\x06\x06\x1F\x67\x42\x43\x10\x13\x09\x00\x16\x04\x37\x61\x78\x7E\x3A\x2D\x35\x2B\x37\x7E\x39\x3F\x30\x38\x73\x7C\x64\x41\x65"
"\x69\x0C\x20\x23\x39\x62\x2F\x2F\x2E\x36\x7D\x64\x04\x1E\x0D\x19\x17\x1D\x1A\x18\x5D\x01\x07\x11\x03\x02\x02\x04\x55\x54\x4B\x3B\x1D\x0F\x1D\x18\x18\x12\x43\x32\x04\x16\x06\x0D\x17\x61\x7B\x2C\x31\x3B\x31\x7C\x2E\x33\x35\x35\x71\x3B\x38\x30\x30\x71\x6B\x3C\x21\x2B\x21\x46\x6D\x62\x30\x26\x22\x69\x23"
"\x2D\x36\x17\x40\x58\x0D\x16\x1A\x12\x5D\x21\x0A\x03\x05\x13\x1A\x54\x27\x0F\x18\x1C\x06\x1C\x0A\x57\x4D\x16\x0B\x05\x0F\x46\x35\x01\x16\x3F\x2F\x78\x2D\x36\x36\x2F\x7D\x02\x10\x7E\x5B\x7B\x77\x16\x39\x2B\x28\x23\x69\x3D\x2C\x3E\x28\x27\x2D\x60\x20\x20\x33\x21\x37\x5A\x0E\x08\x1D\x1F\x0B\x19\x47\x52"
"\x01\x1F\x1D\x1A\x57\x16\x14\x09\x00\x48\x1D\x06\x0A\x4C\x18\x12\x07\x01\x15\x03\x47\x02\x17\x35\x36\x78\x18\x3A\x29\x3D\x33\x31\x36\x34\x71\x25\x23\x35\x27\x3E\x3E\x38\x43\x6E\x6F\x72\x6D\x17\x2D\x29\x2F\x35\x33\x25\x29\x16\x5B\x2D\x09\x1A\x1E\x08\x18\x01\x5D\x7A\x7B\x30\x36\x27\x21\x4A\x38\x3C\x28"
"\x3C\x3B\x39\x3D\x68\x4E\x40\x29\x1F\x05\x16\x0C\x3E\x7B\x3A\x36\x31\x2B\x66\x7D\x21\x3B\x25\x25\x32\x38\x23\x3B\x6A\x38\x29\x3F\x2B\x3C\x6C\x39\x2A\x26\x60\x2A\x23\x35\x2A\x20\x16\x5B\x0C\x16\x5E\x1B\x15\x0E\x19\x53\x16\x1E\x04\x57\x12\x14\x19\x1F\x0D\x1B\x4E\x1C\x18\x0C\x10\x17\x13\x4F\x6C\x4A\x44"
"\x2C\x3C\x7B\x21\x36\x2B\x7F\x38\x28\x33\x3F\x70\x33\x39\x38\x20\x75\x25\x39\x68\x3A\x2B\x2A\x6C\x22\x26\x27\x60\x32\x2E\x32\x30\x21\x15\x0C\x16\x59\x1C\x1A\x14\x1C\x04\x1A\x1F\x03\x5A\x57\x10\x1C\x19\x0A\x0A\x05\x0B\x4F\x05\x19\x42\x4B\x14\x09\x0F\x14\x6E\x45\x7A\x2B\x2A\x36\x39\x2D\x3D\x30\x68\x73"
"\x77\x37\x37\x24\x20\x78\x39\x3F\x29\x3B\x3A\x3A\x3C\x60\x2D\x25\x26\x66\x6F\x69\x4E\x68\x5A\x3A\x58\x0B\x1B\x0C\x08\x1C\x00\x07\x50\x59\x18\x18\x00\x55\x19\x03\x1D\x1D\x0A\x00\x1B\x03\x4B\x43\x01\x0D\x11\x06\x1D\x16\x7A\x3F\x37\x3C\x2D\x7F\x3D\x7D\x34\x26\x3C\x3D\x76\x35\x3B\x3A\x3E\x65\x42\x43\x08"
"\x06\x14\x04\x0C\x04\x60\x00\x66\x10\x0D\x0B\x3E\x34\x2F\x2A\x5E\x2B\x34\x3C\x26\x53\x27\x3E\x38\x50\x20\x55\x28\x24\x27\x3D\x4E\x2E\x38\x4D\x23\x2F\x2C\x6B\x24\x08\x0B\x11\x7A\x3D\x2A\x36\x33\x7F\x3D\x7D\x20\x36\x33\x3E\x20\x32\x26\x2C\x6A\x2F\x3A\x20\x38\x2A\x6C\x62\x62\x2A\x2E\x32\x32\x26\x28\x29"
"\x5A\x2E\x2B\x3B\x5E\x41\x5C\x2F\x17\x03\x11\x18\x04\x57\x0D\x1A\x1F\x19\x48\x0A\x01\x02\x1C\x18\x16\x06\x12\x41\x58\x6D\x30\x17\x35\x2E\x3A\x35\x3B\x2C\x34\x32\x3D\x27\x7E\x71\x05\x32\x31\x75\x27\x2A\x26\x3C\x2F\x23\x61\x3F\x27\x20\x2F\x37\x23\x35\x3D\x65\x1B\x15\x1C\x59\x13\x1E\x12\x08\x13\x1F\x5D"
"\x12\x1A\x12\x15\x1B\x47\x02\x06\x1A\x1A\x0E\x00\x01\x4C"
);

MAN(power, "Power & Battery Guide",
"\x0A\x14\x0F\x1C\x0C\x7F\x0F\x09\x13\x07\x15\x02\x5C\x7A\x74\x06\x26\x2E\x2D\x39\x74\x6F\x29\x3B\x27\x31\x39\x35\x2E\x2E\x2A\x22\x5A\x0B\x19\x0C\x0D\x1A\x0F\x51\x52\x21\x31\x3C\x56\x04\x00\x14\x13\x18\x48\x19\x01\x18\x09\x1F\x07\x07\x40\x4C\x46\x15\x01\x16\x2F\x36\x3D\x79\x37\x31\x7C\x2E\x37\x30\x3F"
"\x3F\x32\x24\x7A\x5F\x67\x6B\x00\x20\x2C\x2A\x3E\x23\x23\x37\x25\x7B\x66\x15\x05\x08\x5A\x08\x19\x0F\x1B\x1B\x5C\x09\x1D\x53\x14\x18\x05\x1C\x58\x55\x3A\x28\x48\x0F\x1B\x03\x00\x14\x42\x0C\x06\x07\x46\x4A\x44\x17\x3F\x28\x2D\x34\x3B\x7F\x35\x33\x72\x2D\x61\x61\x25\x7B\x74\x20\x39\x2E\x3B\x69\x20\x20"
"\x46\x6D\x62\x21\x21\x35\x32\x22\x36\x3C\x54\x71\x55\x59\x38\x1E\x0F\x09\x52\x00\x04\x10\x04\x03\x01\x05\x50\x4B\x1B\x01\x1B\x1B\x08\x02\x15\x0D\x40\x4A\x46\x01\x05\x16\x2E\x7B\x2A\x3C\x2D\x2A\x31\x38\x72\x25\x39\x30\x76\x24\x35\x23\x2F\x2F\x68\x22\x2B\x3D\x22\x28\x2E\x63\x68\x32\x23\x22\x64\x28\x1B"
"\x15\x0D\x18\x12\x52\x1E\x12\x1D\x07\x59\x5F\x7C\x5A\x54\x26\x02\x1E\x1C\x49\x0A\x00\x1B\x03\x42\x4C\x40\x33\x03\x14\x10\x04\x28\x2F\x62\x79\x38\x2A\x30\x31\x72\x30\x29\x32\x3A\x32\x27\x7B\x40\x41\x1F\x01\x0B\x1D\x09\x6D\x16\x0C\x60\x02\x09\x09\x10\x17\x35\x37\x58\x30\x2A\x75\x51\x5D\x21\x16\x04\x05"
"\x1F\x19\x13\x06\x4A\x55\x48\x3A\x17\x1C\x18\x08\x0F\x43\x5E\x41\x36\x08\x13\x00\x28\x7B\x7E\x79\x3C\x3E\x28\x29\x37\x21\x29\x71\x7E\x00\x3D\x3B\x7B\x7A\x61\x69\x21\x3D\x6C\x0E\x2D\x2D\x34\x33\x29\x2B\x64\x15\x1B\x15\x1D\x15\x5E\x41\x5C\x2D\x1D\x04\x15\x03\x7C\x57\x54\x3A\x1A\x1F\x01\x06\x00\x1C\x4C"
"\x45\x01\x0F\x01\x12\x15\x0E\x07\x4C\x74\x7B\x0C\x31\x37\x2C\x7C\x2D\x20\x3C\x37\x23\x37\x3A\x6E\x75\x6D\x3B\x27\x3E\x2B\x3D\x61\x22\x32\x37\x29\x2E\x28\x34\x63\x69\x5A\x5C\x1A\x18\x0A\x0B\x19\x0F\x0B\x5E\x03\x10\x00\x12\x06\x52\x44\x61\x45\x49\x3D\x0C\x1E\x08\x07\x0D\x40\x00\x08\x03\x44\x16\x36\x3E"
"\x3D\x29\x7E\x2B\x35\x30\x37\x3C\x25\x25\x25\x6C\x74\x25\x25\x3C\x2D\x3B\x6E\x2D\x39\x39\x36\x2C\x2E\x61\x69\x67\x28\x2C\x1E\x5B\x19\x1A\x0A\x16\x13\x13\x49\x53\x07\x10\x1D\x12\x54\x01\x03\x06\x0D\x1B\x1D\x54\x66\x4D\x42\x0B\x19\x03\x14\x0E\x00\x45\x29\x37\x3D\x3C\x2E\x7F\x71\x7D\x33\x3F\x3C\x71\x22"
"\x3F\x31\x27\x2F\x65\x42\x43\x1E\x00\x1B\x08\x10\x63\x10\x0D\x07\x09\x17\x4F\x57\x5B\x3A\x18\x12\x1E\x12\x1E\x17\x17\x4A\x51\x12\x12\x12\x14\x1F\x07\x1C\x49\x0F\x01\x08\x4D\x00\x06\x13\x15\x46\x01\x0B\x17\x7A\x36\x37\x2A\x2A\x7F\x29\x2E\x37\x21\x23\x7F\x5C\x7A\x74\x1D\x23\x2C\x20\x69\x3E\x2A\x3E\x2B"
"\x2D\x31\x2D\x20\x28\x24\x21\x65\x55\x5B\x2D\x15\x0A\x16\x11\x1C\x06\x16\x50\x21\x13\x05\x12\x1A\x18\x06\x09\x07\x0D\x0A\x56\x4D\x11\x16\x02\x15\x0A\x02\x44\x02\x3B\x32\x36\x2A\x72\x7F\x31\x32\x20\x36\x70\x34\x38\x32\x26\x32\x33\x6B\x65\x69\x6E\x45\x6C\x6D\x30\x22\x32\x24\x2A\x3E\x64\x32\x15\x09\x0C"
"\x11\x5E\x16\x08\x5D\x1D\x1D\x50\x1C\x19\x13\x11\x07\x04\x4B\x2B\x39\x3B\x1C\x42\x67\x4F\x43\x30\x0E\x11\x02\x16\x45\x29\x3A\x2E\x3C\x2C\x7F\x74\x32\x3E\x37\x35\x23\x76\x35\x21\x3C\x26\x2F\x3B\x60\x74\x6F\x2A\x22\x30\x63\x25\x2C\x23\x35\x23\x20\x14\x18\x01\x59\x1C\x1E\x08\x09\x17\x01\x09\x51\x1A\x1E"
"\x12\x10\x44\x61\x45\x49\x3A\x07\x05\x1E\x42\x13\x12\x0E\x01\x15\x05\x08\x60\x7B\x7F\x29\x32\x3E\x32\x70\x30\x32\x3C\x30\x38\x34\x31\x31\x6D\x67\x68\x6E\x3E\x23\x2D\x23\x6F\x2B\x29\x26\x2E\x6A\x34\x20\x08\x1D\x17\x0B\x13\x1E\x12\x1E\x17\x54\x5C\x7B\x56\x57\x53\x05\x05\x1C\x0D\x1B\x43\x1F\x00\x0C\x0C"
"\x4E\x15\x0D\x12\x0E\x09\x04\x2E\x3E\x7F\x75\x7E\x78\x2C\x32\x25\x36\x22\x7C\x25\x36\x22\x30\x38\x6C\x66\x43\x44\x0D\x0D\x19\x16\x06\x12\x18\x4C\x6A\x64\x07\x1B\x0F\x0C\x1C\x0C\x06\x5C\x0E\x13\x05\x15\x03\x56\x5F\x27\x10\x1E\x1F\x01\x07\x09\x1C\x4C\x53\x42\x33\x0F\x16\x03\x15\x44\x43\x7A\x39\x39\x2D"
"\x2A\x3A\x2E\x24\x72\x6D\x70\x13\x37\x23\x20\x30\x38\x32\x68\x3A\x2F\x39\x29\x3F\x6B\x79\x60\x33\x23\x23\x31\x26\x1F\x08\x72\x59\x5E\x1D\x1D\x1E\x19\x14\x02\x1E\x03\x19\x10\x55\x0B\x08\x1C\x00\x18\x06\x18\x14\x42\x01\x05\x0D\x09\x10\x44\x04\x7A\x2F\x30\x2B\x3B\x2C\x34\x32\x3E\x37\x70\x79\x32\x32\x32"
"\x34\x3F\x27\x3C\x69\x7C\x7F\x69\x64\x6C\x49\x6D\x61\x04\x26\x30\x31\x1F\x09\x01\x59\x0B\x0C\x1D\x1A\x17\x49\x50\x06\x1E\x1E\x17\x1D\x4A\x0A\x18\x19\x1D\x4F\x08\x1F\x03\x0A\x0E\x04\x02\x47\x10\x0D\x3F\x7B\x3A\x38\x2A\x2B\x39\x2F\x2B\x73\x7D\x71\x22\x22\x3A\x30\x6A\x3F\x20\x2C\x23\x61\x46\x60\x62\x01"
"\x21\x35\x32\x22\x36\x3C\x5A\x09\x1D\x09\x11\x0D\x08\x47\x52\x03\x1F\x06\x13\x05\x17\x13\x0D\x4B\x47\x0B\x0F\x1B\x18\x08\x10\x1A\x12\x04\x16\x08\x16\x11\x7A\x76\x78\x3B\x3F\x2B\x28\x38\x20\x2A\x70\x39\x33\x36\x38\x21\x22\x67\x68\x2D\x2B\x3C\x25\x2A\x2C\x63\x36\x32\x4C\x67\x64\x23\x0F\x17\x14\x59\x1D"
"\x17\x1D\x0F\x15\x16\x50\x12\x17\x07\x15\x16\x03\x1F\x11\x45\x4E\x1A\x1F\x0C\x05\x06\x40\x09\x0F\x14\x10\x0A\x28\x22\x78\x71\x2A\x37\x35\x2E\x72\x23\x22\x3E\x31\x25\x35\x38\x70\x6B\x6F\x2B\x2F\x3B\x38\x28\x30\x3A\x6D\x33\x23\x37\x2B\x37\x0E\x5C\x51\x57\x74\x52\x5C\x38\x1C\x16\x02\x16\x0F\x57\x06\x10"
"\x1A\x04\x1A\x1D\x54\x4F\x1C\x02\x15\x06\x12\x02\x00\x00\x44\x4A\x3F\x35\x3D\x2B\x39\x26\x7C\x70\x72\x37\x35\x25\x33\x34\x20\x26\x6A\x22\x26\x2C\x28\x29\x25\x2E\x2B\x26\x2E\x35\x66\x34\x21\x31\x0E\x12\x16\x1E\x0D\x51\x76\x50\x52\x20\x1C\x14\x13\x07\x54\x06\x1E\x0A\x1C\x0C\x1D\x55\x4C\x1D\x0D\x14\x05"
"\x13\x05\x01\x03\x45\x75\x3A\x78\x74\x7E\x2B\x39\x31\x3E\x20\x70\x28\x39\x22\x74\x22\x22\x32\x68\x3A\x22\x2A\x29\x3D\x62\x2A\x33\x61\x33\x29\x25\x33\x1B\x12\x14\x18\x1C\x13\x19\x53\x78\x79\x32\x30\x22\x23\x31\x27\x33\x4B\x3C\x20\x3E\x3C\x66\x40\x42\x2F\x0F\x16\x03\x15\x44\x07\x28\x32\x3F\x31\x2A\x31"
"\x39\x2E\x21\x68\x70\x34\x38\x36\x36\x39\x2F\x6B\x2A\x28\x3A\x3B\x29\x3F\x3B\x63\x33\x20\x30\x22\x36\x7E\x5A\x18\x14\x16\x0D\x1A\x5C\x15\x17\x12\x06\x08\x56\x15\x15\x16\x01\x0C\x1A\x06\x1B\x01\x08\x4D\x03\x13\x10\x12\x5D\x6D\x44\x45\x2F\x28\x3D\x79\x09\x36\x71\x1B\x3B\x73\x3F\x27\x33\x25\x74\x36\x2F"
"\x27\x24\x3C\x22\x2E\x3E\x76\x62\x36\x2E\x31\x2A\x32\x23\x65\x0A\x1E\x0A\x10\x0E\x17\x19\x0F\x13\x1F\x03\x4A\x56\x1C\x11\x10\x1A\x4B\x01\x1D\x4E\x0C\x03\x02\x0E\x4D\x6A\x4C\x46\x24\x05\x09\x33\x39\x2A\x38\x2A\x3A\x7C\x32\x31\x30\x31\x22\x3F\x38\x3A\x34\x26\x27\x31\x73\x6E\x2C\x24\x2C\x30\x24\x25\x61"
"\x32\x28\x64\x74\x4A\x4B\x5D\x55\x5E\x0A\x0F\x18\x52\x07\x1F\x51\x08\x45\x44\x50\x46\x4B\x0B\x01\x0F\x1D\x0B\x08\x42\x02\x07\x00\x0F\x09\x4A\x6F\x77\x7B\x0A\x3C\x2E\x33\x3D\x3E\x37\x73\x27\x39\x33\x39\x74\x36\x2B\x3B\x29\x2A\x27\x3B\x35\x6D\x26\x26\x27\x33\x27\x23\x21\x36\x5A\x14\x0A\x59\x0A\x17\x19"
"\x5D\x10\x12\x04\x05\x13\x05\x0D\x55\x19\x1C\x0D\x05\x02\x1C\x4C\x40\x42\x0D\x05\x17\x03\x15\x44\x10\x29\x3E\x78\x38\x54\x7F\x7C\x2E\x25\x3C\x3C\x3D\x33\x39\x74\x37\x2B\x3F\x3C\x2C\x3C\x36\x62\x47\x48\x14\x01\x0A\x03\x67\x10\x0C\x37\x3E\x2A\x2A\x74\x52\x5C\x5A\x33\x1F\x1C\x1E\x01\x57\x03\x14\x01\x0E"
"\x48\x1D\x07\x02\x09\x1F\x11\x44\x40\x0D\x03\x13\x17\x45\x29\x38\x30\x3C\x3A\x2A\x30\x38\x36\x73\x24\x30\x25\x3C\x27\x75\x62\x3E\x38\x2D\x2F\x3B\x29\x3E\x6E\x63\x24\x24\x20\x35\x25\x22\x53\x5B\x0F\x18\x15\x1A\x5C\x09\x1A\x16\x50\x21\x35\x59\x7E\x58\x4A\x3C\x01\x07\x45\x3D\x4C\x53\x42\x13\x0F\x16\x03"
"\x15\x07\x03\x3D\x7B\x77\x35\x3F\x2C\x28\x2A\x33\x38\x35\x71\x25\x3F\x3B\x22\x39\x6B\x3F\x21\x2F\x3B\x6C\x3A\x2D\x28\x25\x61\x32\x2F\x21\x65\x2A\x38\x58\x15\x1F\x0C\x08\x5D\x06\x1A\x1D\x14\x58\x7D\x59\x55\x3A\x04\x1F\x0C\x1C\x0C\x0A\x0A\x42\x4C\x17\x00\x0D\x02\x10\x0C\x37\x3E\x2A\x2A\x7E\x33\x35\x2E"
"\x26\x20\x70\x22\x35\x3F\x31\x31\x3F\x27\x2D\x2D\x6E\x38\x2D\x26\x27\x30\x6E\x4B\x4C\x0A\x0B\x0B\x33\x2F\x37\x2B\x5E\x50\x5C\x35\x36\x37\x50\x25\x3F\x3A\x31\x3A\x3F\x3F\x3B\x63\x43\x4F\x3F\x0E\x10\x06\x05\x0F\x46\x13\x0D\x08\x3F\x34\x2D\x2D\x7E\x2C\x3D\x2B\x37\x20\x70\x25\x3E\x32\x74\x38\x25\x38\x3C"
"\x67\x6E\x1C\x29\x39\x62\x76\x6D\x70\x73\x67\x29\x2C\x14\x5B\x1E\x16\x0C\x5F\x10\x1C\x02\x07\x1F\x01\x05\x5B\x54\x44\x5F\x46\x5B\x59\x4E\x09\x03\x1F\x68\x43\x40\x05\x03\x14\x0F\x11\x35\x2B\x2B\x77\x7E\x0B\x34\x34\x21\x73\x20\x23\x39\x30\x26\x34\x27\x71\x68\x6E\x2A\x26\x3F\x3D\x2E\x22\x39\x6C\x32\x2E"
"\x29\x20\x15\x0E\x0C\x5E\x5E\x0B\x0B\x18\x13\x18\x5E\x7B\x7C\x3F\x2D\x37\x38\x22\x2C\x49\x3D\x23\x29\x28\x32\x43\x4F\x41\x2E\x2E\x26\x20\x08\x15\x19\x0D\x1B\x7F\x1A\x14\x1E\x16\x5A\x7C\x76\x1F\x2D\x37\x38\x22\x2C\x69\x3D\x23\x29\x28\x32\x63\x7D\x61\x35\x2B\x21\x20\x0A\x5B\x53\x59\x16\x16\x1E\x18\x00"
"\x1D\x11\x05\x13\x57\x12\x1C\x06\x0E\x48\x0F\x01\x1D\x4C\x0E\x10\x02\x13\x09\x46\x15\x01\x06\x35\x2D\x3D\x2B\x27\x71\x56\x70\x72\x23\x3F\x26\x33\x25\x37\x33\x2D\x6B\x67\x21\x6E\x20\x2A\x2B\x62\x31\x25\x2C\x29\x31\x21\x36\x5A\x13\x11\x1B\x1B\x0D\x1A\x14\x1E\x5D\x03\x08\x05\x57\x5C\x06\x0B\x1D\x0D\x1A"
"\x4E\x0B\x05\x1E\x09\x43\x13\x11\x07\x04\x01\x49\x7A\x37\x37\x2A\x3B\x2C\x56\x7D\x72\x3B\x39\x33\x33\x25\x3A\x34\x3E\x2E\x63\x2F\x2F\x3C\x38\x6D\x31\x37\x21\x33\x32\x32\x34\x6C\x54\x5B\x2C\x11\x17\x0C\x5C\x0D\x00\x1C\x17\x03\x17\x1A\x4E\x55\x4D\x03\x01\x0B\x0B\x1D\x02\x0C\x16\x0A\x0F\x0F\x4B\x08\x02"
"\x03\x7D\x75\x52\x53\x0E\x0D\x19\x0E\x17\x1D\x04\x10\x02\x1E\x1B\x1B\x6A\x06\x07\x0D\x0B\x45\x61\x6D\x16\x2B\x29\x32\x66\x37\x36\x2A\x1D\x09\x19\x14\x44\x5F\x5B\x0D\x00\x16\x03\x14\x18\x03\x15\x01\x03\x04\x06\x44\x03\x00\x08\x08\x45\x43\x4D\x41\x08\x08\x44\x16\x36\x3E\x3D\x29\x7E\x28\x34\x34\x3E\x36"
"\x70\x21\x24\x38\x3E\x30\x29\x3F\x21\x27\x29\x61\x46\x47\x11\x0F\x05\x04\x16\x67\x0A\x0A\x2D\x5B\x57\x59\x36\x36\x3E\x38\x20\x3D\x31\x25\x33\x57\x3A\x3A\x3D\x61\x45\x49\x3A\x07\x05\x1E\x42\x13\x12\x0E\x01\x15\x05\x08\x60\x7B\x7F\x2A\x32\x3A\x39\x2D\x75\x73\x31\x3F\x32\x77\x73\x3D\x23\x29\x2D\x3B\x20"
"\x2E\x38\x28\x65\x63\x23\x2E\x2B\x2A\x25\x2B\x1E\x08\x58\x1D\x11\x5F\x15\x09\x52\x1A\x1E\x02\x02\x16\x1A\x01\x06\x12\x46"
);

MAN(registry, "Registry Guide",
"\x0D\x13\x19\x0D\x7E\x0B\x14\x18\x72\x01\x15\x16\x1F\x04\x00\x07\x13\x6B\x01\x1A\x44\x0E\x6C\x25\x2B\x26\x32\x20\x34\x24\x2C\x2C\x19\x1A\x14\x59\x1A\x1E\x08\x1C\x10\x12\x03\x14\x56\x18\x12\x55\x19\x0E\x1C\x1D\x07\x01\x0B\x1E\x42\x05\x0F\x13\x46\x30\x0D\x0B\x3E\x34\x2F\x2A\x7E\x3E\x32\x39\x72\x32\x20"
"\x21\x25\x79\x74\x06\x2F\x3F\x3C\x20\x20\x28\x3F\x6D\x23\x33\x30\x32\x4C\x26\x2A\x21\x5A\x0F\x0F\x1C\x1F\x14\x5C\x09\x1D\x1C\x1C\x02\x56\x5F\x18\x1C\x01\x0E\x48\x1D\x06\x06\x1F\x4D\x0D\x0D\x05\x48\x46\x0D\x11\x16\x2E\x7B\x2F\x2B\x37\x2B\x39\x7D\x24\x32\x3C\x24\x33\x24\x74\x21\x22\x2E\x3A\x2C\x60\x6F"
"\x0E\x28\x2B\x2D\x27\x61\x25\x26\x36\x20\x1C\x0E\x14\x55\x74\x06\x13\x08\x52\x10\x11\x1F\x56\x12\x10\x1C\x1E\x4B\x09\x05\x03\x00\x1F\x19\x42\x02\x0E\x18\x12\x0F\x0D\x0B\x3D\x7B\x75\x79\x3C\x3A\x35\x33\x35\x73\x33\x30\x24\x32\x38\x30\x39\x38\x64\x69\x37\x20\x39\x6D\x21\x22\x2E\x61\x24\x35\x21\x24\x11"
"\x5B\x2F\x10\x10\x1B\x13\x0A\x01\x5D\x7A\x7B\x22\x3F\x31\x55\x2C\x22\x3E\x2C\x4E\x27\x25\x3B\x27\x30\x40\x49\x34\x28\x2B\x31\x7A\x10\x1D\x00\x0D\x76\x56\x70\x72\x1B\x1B\x12\x04\x77\x74\x1D\x01\x0E\x11\x16\x0D\x03\x0D\x1E\x11\x06\x13\x1E\x14\x08\x0B\x11\x5A\x56\x58\x1F\x17\x13\x19\x5D\x13\x00\x03\x1E"
"\x15\x1E\x15\x01\x03\x04\x06\x1A\x42\x4F\x2F\x22\x2F\x43\x03\x0D\x07\x14\x17\x00\x29\x75\x52\x74\x7E\x17\x17\x1E\x07\x73\x70\x19\x1D\x12\x0D\x0A\x09\x1E\x1A\x1B\x0B\x01\x18\x12\x17\x10\x05\x13\x66\x6A\x64\x36\x1F\x0F\x0C\x10\x10\x18\x0F\x5D\x14\x1C\x02\x51\x02\x1F\x11\x55\x06\x04\x0F\x0E\x0B\x0B\x41"
"\x04\x0C\x43\x15\x12\x03\x15\x4A\x6F\x77\x7B\x10\x12\x12\x12\x7C\x7D\x1A\x18\x15\x08\x09\x1B\x1B\x16\x0B\x07\x17\x04\x0F\x0C\x04\x04\x0C\x06\x60\x6C\x66\x2A\x25\x26\x12\x12\x16\x1C\x53\x08\x15\x19\x17\x53\x03\x14\x02\x03\x1D\x1B\x0D\x18\x48\x41\x0F\x0B\x01\x04\x0C\x43\x0E\x04\x03\x03\x01\x01\x73\x75"
"\x52\x74\x7E\x17\x17\x08\x72\x73\x70\x19\x1D\x12\x0D\x0A\x1F\x18\x0D\x1B\x1D\x6F\x61\x6D\x32\x31\x2F\x27\x2F\x2B\x21\x36\x5A\x14\x1E\x59\x1F\x13\x10\x5D\x07\x00\x15\x03\x05\x59\x7E\x58\x4A\x23\x23\x2A\x2D\x4F\x4C\x25\x29\x26\x39\x3E\x25\x32\x36\x37\x1F\x15\x0C\x06\x1D\x10\x12\x1B\x1B\x14\x70\x7C\x76"
"\x34\x21\x27\x38\x2E\x26\x3D\x6E\x27\x2D\x3F\x26\x34\x21\x33\x23\x67\x34\x37\x15\x1D\x11\x15\x1B\x51\x76\x77\x24\x32\x3C\x24\x33\x57\x20\x2C\x3A\x2E\x3B\x63\x43\x4F\x3E\x28\x25\x3C\x33\x3B\x5C\x47\x17\x11\x28\x32\x36\x3E\x70\x7F\x0E\x18\x15\x0C\x14\x06\x19\x05\x10\x6F\x6A\x78\x7A\x64\x2C\x26\x38\x6D"
"\x2C\x36\x2D\x23\x23\x35\x64\x6D\x4A\x5B\x17\x0B\x5E\x4E\x5C\x40\x52\x00\x07\x18\x02\x14\x1C\x10\x19\x42\x46\x63\x43\x4F\x3E\x28\x25\x3C\x31\x36\x29\x35\x20\x5F\x7A\x6D\x6C\x74\x3C\x36\x28\x7D\x3C\x26\x3D\x33\x33\x25\x7A\x75\x18\x0E\x0F\x16\x0C\x06\x02\x0C\x10\x1A\x7A\x61\x34\x26\x33\x65\x18\x02\x0C"
"\x1C\x0D\x51\x76\x50\x52\x21\x35\x36\x29\x32\x2C\x25\x2B\x25\x2C\x36\x3D\x35\x56\x4D\x11\x17\x12\x08\x08\x00\x44\x12\x33\x2F\x30\x79\x7B\x09\x1D\x0F\x1B\x12\x12\x1D\x13\x04\x71\x7B\x40\x41\x1D\x1A\x0B\x09\x19\x01\x62\x08\x05\x18\x15\x4D\x69\x65\x32\x30\x3B\x2C\x22\x2C\x13\x1B\x06\x04\x11\x03\x13\x2B"
"\x39\x1C\x09\x19\x07\x1A\x01\x09\x18\x31\x35\x0A\x0E\x05\x09\x10\x17\x39\x19\x2E\x2A\x2B\x3B\x31\x28\x0B\x37\x21\x23\x38\x39\x39\x08\x07\x3F\x25\x68\x69\x6E\x67\x3F\x39\x23\x31\x34\x34\x36\x67\x25\x35\x0A\x08\x51\x73\x53\x5F\x34\x36\x3E\x3E\x2C\x5F\x58\x59\x28\x27\x1F\x05\x48\x44\x4E\x1C\x18\x0C\x10"
"\x17\x15\x11\x46\x01\x0B\x17\x7A\x3A\x34\x35\x7E\x2A\x2F\x38\x20\x20\x70\x79\x22\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x78\x63\x33\x35\x27\x35\x30\x30\x0A\x5B\x15\x18\x10\x1E\x1B\x18\x00\x5A\x5E\x7B\x5B\x57\x3C\x3E\x26\x26\x34\x3A\x01\x09\x18\x1A\x03\x11\x05\x3D\x2B\x0E\x07\x17\x35\x28\x37\x3F"
"\x2A\x03\x0B\x34\x3C\x37\x3F\x26\x25\x0B\x17\x20\x38\x39\x2D\x27\x3A\x19\x29\x3F\x31\x2A\x2F\x2F\x1A\x17\x2B\x29\x13\x18\x11\x1C\x0D\x5F\x51\x5D\x02\x1C\x1C\x18\x15\x1E\x11\x06\x44\x61\x45\x49\x26\x24\x2F\x38\x3E\x20\x0F\x0F\x12\x15\x0B\x09\x7A\x0B\x39\x37\x3B\x33\x00\x19\x37\x20\x3B\x25\x39\x27\x74"
"\x78\x6A\x2F\x2D\x3A\x25\x3B\x23\x3D\x62\x21\x25\x29\x27\x31\x2D\x2A\x08\x5B\x50\x0E\x1F\x13\x10\x0D\x13\x03\x15\x03\x5A\x57\x19\x1A\x1F\x18\x0D\x40\x40\x65\x41\x4D\x2A\x28\x23\x34\x3A\x34\x0B\x03\x2E\x2C\x39\x2B\x3B\x03\x11\x34\x31\x21\x3F\x22\x39\x31\x20\x09\x1D\x22\x26\x2D\x21\x38\x3F\x11\x01\x36"
"\x32\x33\x23\x29\x30\x13\x1F\x09\x0B\x10\x11\x11\x20\x29\x1A\x16\x1D\x14\x05\x2B\x24\x10\x18\x18\x07\x07\x0F\x03\x05\x17\x07\x69\x40\x41\x4B\x47\x25\x15\x2A\x28\x0D\x2A\x3B\x13\x35\x3A\x3A\x27\x04\x39\x33\x3A\x31\x75\x2F\x3F\x2B\x67\x6E\x67\x38\x25\x2B\x30\x60\x31\x34\x28\x23\x37\x1B\x16\x5F\x0A\x5E"
"\x0B\x14\x18\x1F\x16\x50\x05\x01\x12\x15\x1E\x19\x42\x46\x63\x64\x3D\x29\x2A\x27\x27\x29\x35\x6C\x4A\x44\x17\x3F\x3C\x3D\x3D\x37\x2B\x72\x38\x2A\x36\x70\x79\x22\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x78\x63\x67\x33\x23\x20\x2D\x36\x0E\x09\x01\x54\x11\x0F\x19\x13\x5F\x16\x14\x18\x02\x18\x06\x52"
"\x43\x45\x62\x44\x4E\x29\x5F\x4D\x11\x06\x01\x13\x05\x0F\x01\x16\x74\x7B\x1D\x21\x2E\x30\x2E\x29\x7D\x1A\x3D\x21\x39\x25\x20\x75\x23\x25\x68\x0F\x27\x23\x29\x6D\x2F\x26\x2E\x34\x66\x6F\x26\x24\x19\x10\x0D\x09\x5E\x1D\x19\x1B\x1D\x01\x15\x51\x13\x13\x1D\x01\x03\x05\x0F\x48\x47\x41\x66\x40\x42\x4D\x12"
"\x04\x01\x47\x02\x0C\x36\x3E\x2B\x79\x3F\x2D\x39\x7D\x26\x36\x28\x25\x6C\x77\x30\x3A\x3F\x29\x24\x2C\x63\x2C\x20\x24\x21\x28\x60\x2C\x23\x35\x23\x20\x09\x5B\x0C\x11\x1B\x12\x47\x5D\x00\x1A\x17\x19\x02\x5A\x17\x19\x03\x08\x03\x49\x50\x4F\x29\x09\x0B\x17\x40\x15\x09\x6D\x44\x45\x33\x35\x2B\x29\x3B\x3C"
"\x28\x7D\x34\x3A\x22\x22\x22\x79\x74\x01\x22\x22\x3B\x69\x3E\x3D\x23\x2A\x30\x22\x2D\x7B\x66\x60\x36\x20\x1D\x12\x0B\x0D\x0C\x06\x51\x14\x1F\x03\x1F\x03\x02\x50\x54\x5A\x4A\x4C\x1A\x0C\x09\x06\x1F\x19\x10\x1A\x4D\x03\x07\x04\x0F\x10\x2A\x7C\x76\x53\x54\x0D\x19\x1A\x1B\x00\x04\x03\x0F\x77\x1B\x1B\x6A"
"\x1F\x00\x0C\x6E\x0C\x03\x00\x0F\x02\x0E\x05\x66\x0B\x0D\x0B\x3F\x5B\x50\x0B\x1B\x18\x55\x77\x5F\x53\x02\x14\x11\x57\x05\x00\x0F\x19\x11\x49\x52\x04\x09\x14\x5C\x43\x40\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x0D\x39\x3C\x36\x73\x26\x30\x3A\x22\x31\x26\x40\x66\x68\x3B\x2B\x28\x6C\x2C\x26\x27\x60\x7D"
"\x2D\x22\x3D\x7B\x5A\x54\x0E\x59\x42\x11\x1D\x10\x17\x4D\x50\x5E\x02\x57\x26\x30\x2D\x34\x2C\x3E\x21\x3D\x28\x4D\x4D\x07\x40\x50\x46\x48\x02\x45\x7A\x7B\x0F\x2B\x37\x2B\x39\x57\x7F\x73\x22\x34\x31\x77\x30\x30\x26\x2E\x3C\x2C\x6E\x73\x27\x28\x3B\x7D\x60\x6E\x20\x67\x64\x65\x5A\x5B\x58\x59\x5E\x3B\x19"
"\x11\x17\x07\x15\x51\x17\x57\x1F\x10\x13\x61\x45\x49\x1C\x0A\x0B\x4D\x07\x1B\x10\x0E\x14\x13\x44\x59\x31\x3E\x21\x67\x7E\x39\x35\x31\x37\x7D\x22\x34\x31\x77\x74\x17\x2B\x28\x23\x3C\x3E\x45\x61\x6D\x16\x2B\x29\x32\x66\x37\x36\x2A\x1D\x09\x19\x14\x44\x5F\x0E\x18\x15\x1A\x03\x05\x04\x0E\x59\x07\x0F\x0A"
"\x0C\x46\x19\x1D\x05\x19\x07\x4C\x04\x04\x0A\x02\x10\x00\x75\x28\x3D\x38\x2C\x3C\x34\x72\x30\x32\x33\x3A\x23\x27\x7B\x3C\x27\x3B\x27\x3B\x3A\x6F\x67\x47\x62\x63\x21\x61\x20\x32\x28\x29\x5A\x29\x1D\x1E\x17\x0C\x08\x0F\x0B\x53\x35\x09\x06\x1B\x1B\x07\x0F\x19\x48\x0A\x0F\x1B\x09\x0A\x0D\x11\x19\x4F\x6C"
"\x6D\x37\x24\x1C\x1E\x0C\x00\x7E\x0D\x09\x11\x17\x00\x5A\x60\x7F\x77\x15\x39\x3D\x2A\x31\x3A\x6E\x2A\x34\x3D\x2D\x31\x34\x61\x24\x22\x22\x2A\x08\x1E\x58\x1C\x1A\x16\x08\x14\x1C\x14\x50\x10\x18\x0E\x00\x1D\x03\x05\x0F\x47\x64\x5D\x45\x4D\x21\x0B\x01\x0F\x01\x02\x44\x0A\x34\x3E\x78\x2F\x3F\x33\x29\x38"
"\x72\x32\x24\x71\x37\x77\x20\x3C\x27\x2E\x73\x69\x3A\x2A\x3F\x39\x79\x63\x32\x24\x30\x22\x36\x31\x5A\x12\x1E\x59\x09\x0D\x13\x13\x15\x5D\x7A\x42\x5F\x57\x3A\x10\x1C\x0E\x1A\x49\x0A\x0A\x00\x08\x16\x06\x40\x0A\x03\x1E\x17\x45\x23\x34\x2D\x79\x3A\x30\x7C\x33\x3D\x27\x70\x24\x38\x33\x31\x27\x39\x3F\x29"
"\x27\x2A\x61\x46\x79\x6B\x63\x04\x2E\x66\x29\x2B\x31\x5A\x0F\x0A\x0C\x0D\x0B\x5C\x0F\x13\x1D\x14\x1E\x1B\x57\x53\x07\x0F\x0C\x01\x1A\x1A\x1D\x15\x4D\x01\x0F\x05\x00\x08\x02\x16\x42\x7A\x3A\x28\x29\x2D\x7F\x71\x7D\x26\x3B\x35\x28\x76\x34\x35\x20\x39\x2E\x68\x2F\x2F\x3D\x6C\x20\x2D\x31\x25\x4B\x66\x67"
"\x64\x21\x1B\x16\x19\x1E\x1B\x5F\x08\x15\x13\x1D\x50\x05\x1E\x12\x0D\x55\x0C\x02\x10\x47\x64\x5A\x45\x4D\x37\x10\x05\x41\x35\x1E\x17\x11\x3F\x36\x78\x0B\x3B\x2C\x28\x32\x20\x36\x70\x30\x25\x77\x35\x3B\x6A\x2E\x30\x3D\x3C\x2E\x6C\x3E\x23\x25\x25\x35\x3F\x67\x2A\x20\x0E\x5B\x50\x0D\x16\x16\x0F\x5D\x02"
"\x01\x1F\x16\x04\x16\x19\x4F\x60\x4B\x48\x49\x49\x1C\x15\x1E\x16\x06\x0D\x4C\x14\x02\x17\x11\x35\x29\x3D\x74\x3D\x2D\x39\x3C\x26\x36\x77\x78\x78\x5D\x5E\x07\x0F\x08\x07\x1F\x0B\x1D\x15\x47\x6F\x63\x05\x39\x36\x28\x36\x31\x5A\x33\x33\x35\x33\x23\x2F\x04\x01\x07\x15\x1C\x2A\x34\x01\x07\x18\x0E\x06\x1D"
"\x2D\x00\x02\x19\x10\x0C\x0C\x32\x03\x13\x38\x36\x3F\x29\x2E\x30\x3D\x3A\x2F\x7D\x34\x3C\x22\x71\x32\x25\x3D\x23\x2F\x39\x68\x3A\x3B\x3D\x2B\x28\x30\x3A\x6E\x4B\x6B\x67\x0D\x23\x5A\x0F\x10\x1C\x5E\x0D\x19\x1A\x1B\x00\x04\x03\x0F\x57\x1D\x06\x4A\x0F\x09\x04\x0F\x08\x09\x09\x58\x43\x02\x0E\x09\x13\x44"
"\x11\x35\x7B\x2A\x3C\x3D\x30\x2A\x38\x20\x2A\x70\x12\x39\x3A\x39\x34\x24\x2F\x68\x19\x3C\x20\x21\x3D\x36\x63\x21\x2F\x22\x67\x31\x36\x1F\x71\x58\x59\x0C\x1A\x1B\x18\x16\x1A\x04\x51\x02\x1F\x11\x07\x0F\x47\x48\x06\x1C\x4F\x1E\x08\x11\x17\x0F\x13\x03\x47\x02\x17\x35\x36\x78\x38\x7E\x2D\x39\x2E\x26\x3C"
"\x22\x34\x76\x27\x3B\x3C\x24\x3F\x68\x66\x6E\x2D\x2D\x2E\x29\x36\x30\x6F"
);

MAN(services, "Windows Services Guide",
"\x0D\x13\x19\x0D\x7E\x0C\x19\x0F\x04\x1A\x13\x14\x05\x77\x15\x07\x0F\x41\x0A\x28\x2D\x24\x2B\x3F\x2D\x36\x2E\x25\x66\x37\x36\x2A\x1D\x09\x19\x14\x0D\x5F\x08\x15\x13\x07\x50\x03\x03\x19\x54\x02\x03\x1F\x00\x06\x1B\x1B\x4C\x0C\x42\x14\x09\x0F\x02\x08\x13\x5F\x7A\x2E\x28\x3D\x3F\x2B\x35\x33\x35\x7F\x70"
"\x21\x24\x3E\x3A\x21\x23\x25\x2F\x65\x44\x6F\x6C\x23\x27\x37\x37\x2E\x34\x2C\x2D\x2B\x1D\x57\x58\x0A\x1B\x1C\x09\x0F\x1B\x07\x09\x5D\x56\x03\x1D\x18\x0F\x4B\x1B\x10\x00\x0C\x42\x43\x4C\x43\x34\x09\x03\x47\x37\x00\x28\x2D\x31\x3A\x3B\x2C\x7C\x3E\x3D\x3D\x23\x3E\x3A\x32\x5E\x7D\x39\x2E\x3A\x3F\x27\x2C"
"\x29\x3E\x6C\x2E\x33\x22\x7D\x67\x30\x2D\x13\x08\x58\x09\x0C\x10\x1B\x0F\x13\x1E\x4A\x51\x51\x04\x11\x07\x1C\x02\x0B\x0C\x1D\x42\x0F\x02\x0C\x10\x0F\x0D\x03\x40\x44\x04\x34\x3F\x78\x2D\x36\x3A\x7C\x0E\x37\x21\x26\x38\x35\x32\x27\x75\x07\x2A\x26\x28\x29\x2A\x3E\x47\x21\x22\x34\x24\x21\x28\x36\x3C\x53"
"\x5B\x0B\x11\x11\x08\x0F\x5D\x06\x1B\x15\x1C\x56\x16\x18\x19\x44\x61\x62\x2A\x21\x23\x39\x20\x2C\x30\x40\x47\x46\x34\x30\x24\x0E\x0E\x0B\x53\x73\x7F\x0F\x29\x33\x27\x25\x22\x6C\x77\x06\x20\x24\x25\x21\x27\x29\x6F\x63\x6D\x11\x37\x2F\x31\x36\x22\x20\x6B\x70\x56\x58\x2A\x0A\x1E\x0E\x09\x07\x03\x50\x05"
"\x0F\x07\x11\x4F\x4A\x2A\x1D\x1D\x01\x02\x0D\x19\x0B\x00\x4C\x41\x27\x12\x10\x0A\x37\x3A\x2C\x30\x3D\x7F\x74\x19\x37\x3F\x31\x28\x33\x33\x7D\x79\x6A\x06\x29\x27\x3B\x2E\x20\x61\x62\x07\x29\x32\x27\x25\x28\x20\x1E\x55\x72\x54\x5E\x58\x30\x12\x15\x53\x1F\x1F\x56\x16\x07\x52\x50\x4B\x24\x06\x0D\x0E\x00"
"\x4D\x31\x1A\x13\x15\x03\x0A\x48\x45\x14\x3E\x2C\x2E\x31\x2D\x37\x7D\x01\x36\x22\x27\x3F\x34\x31\x79\x6A\x07\x27\x2A\x2F\x23\x6C\x1E\x27\x31\x36\x28\x25\x22\x68\x65\x15\x09\x58\x18\x5E\x0A\x0F\x18\x00\x5D\x7A\x5C\x56\x25\x1D\x12\x02\x1F\x45\x0A\x02\x06\x0F\x06\x42\x02\x40\x12\x03\x15\x12\x0C\x39\x3E"
"\x78\x67\x7E\x0F\x2E\x32\x22\x36\x22\x25\x3F\x32\x27\x6F\x6A\x2F\x2D\x3A\x2D\x3D\x25\x3D\x36\x2A\x2F\x2F\x6A\x67\x20\x20\x0A\x1E\x16\x1D\x1B\x11\x1F\x14\x17\x00\x5C\x51\x04\x12\x17\x1A\x1C\x0E\x1A\x10\x64\x4F\x4C\x0C\x01\x17\x09\x0E\x08\x14\x44\x4D\x28\x3E\x2B\x2D\x3F\x2D\x28\x72\x20\x36\x32\x3E\x39"
"\x23\x74\x3A\x24\x6B\x2E\x28\x27\x23\x39\x3F\x27\x6A\x6C\x61\x32\x2F\x21\x65\x1F\x03\x1D\x1A\x0B\x0B\x1D\x1F\x1E\x16\x50\x13\x13\x1F\x1D\x1B\x0E\x4B\x01\x1D\x40\x65\x66\x22\x32\x26\x32\x20\x32\x2E\x2B\x2B\x09\x51\x75\x79\x0D\x2B\x3D\x2F\x26\x73\x7F\x71\x05\x23\x3B\x25\x6A\x64\x68\x1B\x2B\x3C\x38\x2C"
"\x30\x37\x60\x6E\x66\x17\x25\x30\x09\x1E\x58\x56\x5E\x2D\x19\x0E\x07\x1E\x15\x51\x5E\x27\x06\x1A\x1A\x0E\x1A\x1D\x07\x0A\x1F\x4D\x0D\x11\x40\x15\x09\x08\x08\x07\x3B\x29\x71\x77\x54\x72\x7C\x0E\x26\x32\x22\x25\x76\x23\x2D\x25\x2F\x6B\x2B\x21\x2F\x21\x2B\x28\x62\x2D\x25\x24\x22\x34\x64\x24\x1E\x16\x11"
"\x17\x5E\x57\x13\x0F\x52\x07\x18\x18\x05\x57\x04\x07\x05\x0C\x1A\x08\x03\x48\x1F\x4D\x01\x0C\x0D\x0C\x07\x09\x00\x16\x73\x75\x52\x74\x7E\x0B\x34\x34\x21\x73\x20\x23\x39\x30\x26\x34\x27\x71\x68\x3A\x2B\x3D\x3A\x24\x21\x26\x33\x6C\x35\x33\x25\x37\x0E\x54\x0B\x0D\x11\x0F\x53\x0F\x17\x00\x04\x10\x04\x03"
"\x5B\x10\x04\x0A\x0A\x05\x0B\x40\x08\x04\x11\x02\x02\x0D\x03\x48\x15\x10\x3F\x29\x21\x79\x62\x31\x3D\x30\x37\x6D\x7E\x5B\x5C\x14\x1B\x18\x07\x04\x06\x69\x1D\x0A\x1E\x1B\x0B\x00\x05\x12\x4C\x6A\x64\x12\x13\x15\x1C\x16\x09\x0C\x5C\x28\x02\x17\x11\x05\x13\x57\x5C\x02\x1F\x0A\x1D\x1A\x0B\x1D\x1A\x44\x42"
"\x4E\x40\x0A\x03\x02\x14\x45\x33\x2F\x78\x36\x30\x71\x56\x70\x72\x03\x22\x38\x38\x23\x74\x06\x3A\x24\x27\x25\x2B\x3D\x6C\x65\x31\x33\x2F\x2E\x2A\x22\x36\x6C\x5A\x56\x58\x09\x0C\x16\x12\x09\x1B\x1D\x17\x4A\x56\x05\x11\x06\x1E\x0A\x1A\x1D\x4E\x1B\x03\x4D\x04\x0A\x18\x41\x15\x13\x11\x06\x31\x7B\x28\x2B"
"\x37\x31\x28\x7D\x38\x3C\x32\x22\x78\x5D\x79\x75\x1D\x22\x26\x2D\x21\x38\x3F\x6D\x16\x2A\x2D\x24\x66\x6F\x13\x76\x48\x2F\x11\x14\x1B\x56\x5C\x50\x52\x10\x1C\x1E\x15\x1C\x54\x06\x13\x05\x0B\x47\x64\x42\x4C\x3A\x0B\x0D\x04\x0E\x11\x14\x44\x21\x3F\x3D\x3D\x37\x3A\x3A\x2E\x7D\x21\x36\x22\x27\x3F\x34\x31"
"\x26\x6A\x66\x68\x28\x20\x3B\x25\x3B\x2B\x31\x35\x32\x7D\x67\x28\x20\x1B\x0D\x1D\x59\x11\x11\x52\x77\x5F\x53\x23\x08\x05\x3A\x15\x1C\x04\x4B\x45\x49\x0F\x1F\x1C\x4D\x12\x11\x05\x0D\x09\x06\x00\x0C\x34\x3C\x78\x71\x2D\x3A\x39\x7D\x3F\x32\x3E\x24\x37\x3B\x79\x25\x2F\x39\x2E\x26\x3C\x22\x2D\x23\x21\x26"
"\x69\x6F\x4C\x6A\x64\x07\x16\x0E\x1D\x0D\x11\x10\x08\x15\x52\x20\x05\x01\x06\x18\x06\x01\x4A\x38\x0D\x1B\x18\x06\x0F\x08\x42\x4B\x02\x15\x0E\x14\x01\x17\x2C\x72\x78\x74\x7E\x1D\x30\x28\x37\x27\x3F\x3E\x22\x3F\x7A\x5F\x67\x6B\x1A\x2C\x23\x20\x38\x28\x62\x11\x25\x26\x2F\x34\x30\x37\x03\x5B\x55\x59\x31"
"\x39\x3A\x5D\x10\x0A\x50\x15\x13\x11\x15\x00\x06\x1F\x48\x0F\x01\x1D\x4C\x1E\x07\x00\x15\x13\x0F\x13\x1D\x5E\x7A\x30\x3D\x3C\x2E\x7F\x35\x29\x72\x3C\x36\x37\x78\x5D\x5E\x11\x03\x18\x09\x0B\x02\x06\x02\x0A\x62\x10\x05\x13\x10\x0E\x07\x00\x29\x5B\x55\x59\x39\x3A\x32\x38\x20\x32\x3C\x51\x24\x22\x38\x30"
"\x39\x61\x45\x49\x39\x07\x09\x03\x42\x0A\x0E\x41\x02\x08\x11\x07\x2E\x77\x78\x3D\x31\x7F\x12\x12\x06\x73\x34\x38\x25\x36\x36\x39\x2F\x65\x68\x08\x3B\x3B\x23\x20\x23\x37\x29\x22\x66\x34\x21\x37\x0C\x12\x1B\x1C\x0D\x5F\x08\x15\x13\x07\x50\x10\x04\x12\x54\x06\x1E\x04\x18\x19\x0B\x0B\x4C\x0C\x10\x06\x6A"
"\x41\x46\x06\x08\x17\x3F\x3A\x3C\x20\x7E\x3B\x33\x34\x3C\x34\x70\x3F\x39\x23\x3C\x3C\x24\x2C\x73\x69\x2A\x26\x3F\x2C\x20\x2F\x29\x2F\x21\x67\x27\x24\x14\x5B\x1A\x0B\x1B\x1E\x17\x5D\x14\x16\x11\x05\x03\x05\x11\x06\x4A\x18\x1D\x0B\x1A\x03\x15\x43\x68\x4E\x40\x2E\x08\x0B\x1D\x45\x3E\x32\x2B\x38\x3C\x33"
"\x39\x7D\x25\x3B\x31\x25\x76\x3E\x27\x75\x29\x27\x2D\x28\x3C\x23\x35\x6D\x37\x2D\x35\x32\x23\x23\x64\x04\x34\x3F\x58\x00\x11\x0A\x5C\x16\x1C\x1C\x07\x51\x01\x1F\x15\x01\x4A\x02\x1C\x49\x07\x1C\x42\x67\x4F\x43\x37\x13\x0F\x13\x01\x45\x3E\x34\x2F\x37\x7E\x28\x34\x3C\x26\x73\x29\x3E\x23\x77\x37\x3D\x2B"
"\x25\x2F\x2C\x6E\x3C\x23\x6D\x3B\x2C\x35\x61\x25\x26\x2A\x65\x08\x1E\x0E\x1C\x0C\x0B\x52\x77\x5F\x53\x57\x35\x1F\x04\x15\x17\x06\x0E\x4F\x49\x0F\x4F\x1F\x08\x10\x15\x09\x02\x03\x4B\x44\x0B\x35\x2F\x78\x2D\x36\x3A\x7C\x39\x20\x3A\x26\x34\x24\x77\x3D\x21\x6A\x25\x2D\x2C\x2A\x3C\x62\x47\x6F\x63\x12\x24"
"\x6B\x22\x2A\x24\x18\x17\x1D\x43\x5E\x0C\x19\x09\x52\x32\x05\x05\x19\x1A\x15\x01\x03\x08\x48\x08\x00\x0B\x4C\x1F\x07\x10\x14\x00\x14\x13\x44\x11\x32\x3E\x78\x09\x1D\x71\x56\x57\x06\x1B\x15\x71\x25\x34\x74\x16\x05\x06\x05\x08\x00\x0B\x46\x60\x62\x30\x23\x61\x37\x32\x21\x37\x03\x5B\x44\x17\x1F\x12\x19"
"\x43\x52\x53\x50\x51\x56\x57\x54\x55\x4A\x4B\x3B\x1D\x0F\x1B\x19\x1E\x68\x4E\x40\x12\x05\x47\x07\x0A\x34\x3D\x31\x3E\x7E\x63\x32\x3C\x3F\x36\x6E\x71\x25\x23\x35\x27\x3E\x76\x68\x2D\x27\x3C\x2D\x2F\x2E\x26\x24\x61\x66\x67\x07\x2D\x1B\x15\x1F\x1C\x5E\x0C\x08\x1C\x00\x07\x50\x05\x0F\x07\x11\x7F\x47\x4B"
"\x1B\x0A\x4E\x1C\x18\x0C\x10\x17\x4F\x12\x12\x08\x14\x45\x66\x35\x39\x34\x3B\x61\x56\x70\x72\x20\x33\x71\x27\x34\x74\x69\x24\x2A\x25\x2C\x70\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x5A\x5B\x29\x0C\x1B\x0D\x05\x5D\x11\x1C\x1E\x17\x1F\x10\x01\x07\x0B\x1F\x01\x06\x00\x65\x41\x4D\x11\x00\x40\x10\x13"
"\x02\x16\x1C\x3F\x23\x78\x65\x30\x3E\x31\x38\x6C\x73\x70\x71\x76\x77\x74\x75\x6A\x02\x26\x2A\x22\x3A\x28\x28\x62\x13\x09\x05\x4C\x6A\x64\x11\x12\x12\x0B\x59\x0E\x0D\x13\x1A\x00\x12\x1D\x51\x15\x16\x1A\x55\x1B\x1E\x0D\x1B\x17\x40\x1F\x19\x03\x11\x14\x4E\x15\x13\x0B\x15\x75\x3E\x36\x38\x3C\x33\x39\x72"
"\x36\x3A\x23\x30\x34\x3B\x31\x75\x2B\x25\x31\x69\x3D\x2A\x3E\x3B\x2B\x20\x25\x61\x24\x3E\x64\x2B\x1B\x16\x1D\x57\x74\x75\x38\x38\x22\x36\x3E\x35\x33\x39\x37\x3C\x2F\x38\x62\x44\x4E\x3F\x1E\x02\x12\x06\x12\x15\x0F\x02\x17\x45\x64\x7B\x1C\x3C\x2E\x3A\x32\x39\x37\x3D\x33\x38\x33\x24\x74\x26\x22\x24\x3F"
"\x3A\x6E\x38\x24\x2C\x36\x63\x21\x61\x35\x22\x36\x33\x13\x18\x1D\x59\x10\x1A\x19\x19\x01\x53\x11\x1F\x12\x57\x03\x1D\x05\x4B\x06\x0C\x0B\x0B\x1F\x4D\x0B\x17\x4E\x6B\x4B\x47\x37\x11\x35\x2B\x28\x30\x30\x38\x7C\x3C\x72\x20\x35\x23\x20\x3E\x37\x30\x6A\x2A\x3D\x3D\x21\x62\x3F\x39\x2D\x33\x33\x61\x2F\x33"
"\x37\x65\x1E\x1E\x08\x1C\x10\x1B\x19\x13\x06\x00\x50\x59\x14\x12\x54\x16\x0B\x19\x0D\x0F\x1B\x03\x4D\x44\x4C\x69\x6A\x32\x23\x35\x32\x2C\x19\x1E\x0B\x79\x08\x16\x1D\x7D\x06\x12\x03\x1A\x76\x1A\x15\x1B\x0B\x0C\x0D\x1B\x44\x62\x6C\x19\x23\x30\x2B\x61\x0B\x26\x2A\x24\x1D\x1E\x0A\x59\x40\x5F\x2F\x18\x00"
"\x05\x19\x12\x13\x04\x54\x01\x0B\x09\x52\x49\x1F\x1A\x05\x0E\x09\x43\x13\x15\x07\x15\x10\x4A\x29\x2F\x37\x29\x7E\x28\x35\x29\x3A\x73\x24\x39\x33\x77\x27\x30\x38\x3D\x21\x2A\x2B\x6F\x22\x2C\x2F\x26\x4A\x61\x66\x6F\x36\x2C\x1D\x13\x0C\x54\x1D\x13\x15\x1E\x19\x53\x57\x36\x19\x57\x00\x1A\x4A\x0F\x0D\x1D"
"\x0F\x06\x00\x1E\x45\x43\x13\x09\x09\x10\x17\x45\x2E\x33\x3D\x79\x31\x28\x32\x34\x3C\x34\x70\x21\x24\x38\x37\x30\x39\x38\x61\x67\x44\x45\x18\x1F\x0D\x16\x02\x0D\x03\x14\x0C\x0A\x35\x2F\x31\x37\x39\x75\x51\x5D\x21\x16\x02\x07\x1F\x14\x11\x55\x0C\x0A\x01\x05\x1D\x4F\x18\x02\x42\x10\x14\x00\x14\x13\x5E"
"\x45\x1F\x2D\x3D\x37\x2A\x7F\x0A\x34\x37\x24\x35\x23\x76\x7F\x07\x2C\x39\x3F\x2D\x24\x6E\x23\x23\x2A\x6B\x63\x33\x29\x29\x30\x37\x65\x0E\x13\x1D\x59\x1B\x0D\x0E\x12\x00\x48\x7A\x51\x56\x14\x1C\x10\x09\x00\x48\x1D\x06\x0A\x4C\x1E\x16\x02\x12\x15\x46\x13\x1D\x15\x3F\x77\x78\x3D\x3B\x2F\x39\x33\x36\x36"
"\x3E\x32\x3F\x32\x27\x79\x6A\x2A\x26\x2D\x6E\x3B\x24\x28\x62\x30\x25\x33\x30\x2E\x27\x20\x5A\x1E\x00\x1C\x1D\x0A\x08\x1C\x10\x1F\x15\x51\x06\x16\x00\x1D\x60\x4B\x48\x41\x3E\x1D\x03\x1D\x07\x11\x14\x08\x03\x14\x44\x5B\x7A\x0B\x39\x2D\x36\x7F\x28\x32\x72\x36\x28\x34\x35\x22\x20\x34\x28\x27\x2D\x69\x63"
"\x6F\x28\x22\x27\x30\x60\x35\x2E\x22\x64\x23\x13\x17\x1D\x59\x1B\x07\x15\x0E\x06\x4C\x59\x5F\x7C\x5A\x54\x34\x0C\x1F\x0D\x1B\x4E\x02\x0D\x01\x15\x02\x12\x04\x5C\x47\x07\x0D\x3F\x38\x33\x79\x38\x30\x2E\x7D\x21\x26\x23\x21\x3F\x34\x3D\x3A\x3F\x38\x68\x3A\x2B\x3D\x3A\x24\x21\x26\x33\x61\x2F\x29\x64\x31"
"\x12\x1E\x58\x15\x17\x0C\x08\x53"
);

MAN(scheduler, "Task Scheduler Guide",
"\x0D\x13\x19\x0D\x7E\x16\x08\x7D\x16\x1C\x15\x02\x5C\x03\x35\x26\x21\x6B\x1B\x2A\x26\x2A\x28\x38\x2E\x26\x32\x61\x34\x32\x2A\x36\x5A\x0B\x0A\x16\x19\x0D\x1D\x10\x01\x53\x11\x05\x56\x03\x1D\x18\x0F\x18\x48\x06\x1C\x4F\x03\x03\x42\x06\x16\x04\x08\x13\x17\x5F\x7A\x3A\x2C\x79\x32\x30\x3B\x32\x3C\x7F\x70"
"\x35\x37\x3E\x38\x2C\x66\x41\x68\x69\x39\x2A\x29\x26\x2E\x3A\x6C\x61\x29\x29\x64\x2C\x1E\x17\x1D\x55\x5E\x10\x12\x5D\x13\x53\x03\x01\x13\x14\x1D\x13\x03\x08\x48\x0C\x18\x0A\x02\x19\x4E\x43\x0F\x13\x46\x10\x0C\x00\x34\x7B\x2C\x31\x3B\x7F\x0C\x1E\x72\x3A\x23\x71\x3A\x38\x37\x3E\x2F\x2F\x66\x43\x19\x26"
"\x22\x29\x2D\x34\x33\x61\x2F\x33\x37\x20\x16\x1D\x58\x0C\x0D\x1A\x0F\x5D\x1B\x07\x50\x19\x13\x16\x02\x1C\x06\x12\x48\x41\x03\x0E\x05\x03\x16\x06\x0E\x00\x08\x04\x01\x49\x7A\x2E\x28\x3D\x3F\x2B\x39\x2E\x7E\x73\x34\x34\x30\x25\x35\x32\x63\x65\x42\x43\x01\x1F\x09\x03\x62\x0A\x14\x4B\x6B\x67\x30\x24\x09"
"\x10\x0B\x1A\x16\x1B\x52\x10\x01\x10\x50\x59\x02\x1F\x1D\x06\x4A\x1B\x1A\x06\x09\x1D\x0D\x00\x58\x43\x47\x15\x07\x14\x0F\x48\x29\x38\x30\x3C\x3A\x2A\x30\x38\x20\x74\x79\x7F\x76\x05\x3D\x32\x22\x3F\x68\x3A\x27\x2B\x29\x77\x62\x00\x32\x24\x27\x33\x21\x4F\x5A\x5B\x3A\x18\x0D\x16\x1F\x5D\x26\x12\x03\x1A"
"\x56\x5F\x03\x1C\x10\x0A\x1A\x0D\x47\x4F\x0D\x03\x06\x43\x23\x13\x03\x06\x10\x00\x7A\x0F\x39\x2A\x35\x7F\x74\x3B\x27\x3F\x3C\x71\x35\x38\x3A\x21\x38\x24\x24\x60\x60\x45\x46\x1D\x03\x11\x14\x12\x66\x08\x02\x65\x3B\x5B\x2C\x38\x2D\x34\x76\x50\x52\x34\x15\x1F\x13\x05\x15\x19\x50\x4B\x06\x08\x03\x0A\x40"
"\x4D\x06\x06\x13\x02\x14\x0E\x14\x11\x33\x34\x36\x75\x7E\x2A\x2F\x38\x20\x73\x24\x3E\x76\x25\x21\x3B\x6A\x2A\x3B\x65\x6E\x68\x1E\x38\x2C\x63\x37\x29\x23\x33\x2C\x20\x08\x5B\x0D\x0A\x1B\x0D\x5C\x14\x01\x79\x50\x51\x1A\x18\x13\x12\x0F\x0F\x48\x06\x00\x4F\x03\x1F\x42\x0D\x0F\x15\x41\x4B\x44\x42\x08\x2E"
"\x36\x79\x29\x36\x28\x35\x72\x3B\x39\x36\x3E\x32\x27\x21\x6A\x3B\x3A\x20\x38\x26\x20\x28\x25\x26\x33\x66\x66\x6F\x25\x21\x17\x12\x16\x50\x50\x75\x51\x5D\x26\x01\x19\x16\x11\x12\x06\x06\x50\x4B\x1F\x01\x0B\x01\x4C\x04\x16\x43\x12\x14\x08\x14\x44\x4D\x2E\x32\x35\x3C\x72\x7F\x30\x32\x35\x3C\x3E\x7D\x76"
"\x32\x22\x30\x24\x3F\x64\x69\x27\x2B\x20\x28\x6C\x6D\x6E\x68\x68\x67\x09\x30\x16\x0F\x11\x09\x12\x1A\x5C\x1C\x1E\x1F\x1F\x06\x13\x13\x5A\x7F\x47\x4B\x29\x0A\x1A\x06\x03\x03\x11\x59\x40\x16\x0E\x06\x10\x45\x28\x2E\x36\x2A\x7E\x77\x2C\x2F\x3D\x34\x22\x30\x3B\x7B\x74\x26\x29\x39\x21\x39\x3A\x63\x6C\x28"
"\x2F\x22\x29\x2D\x6A\x67\x29\x20\x09\x08\x19\x1E\x1B\x56\x52\x77\x5F\x53\x33\x1E\x18\x13\x1D\x01\x03\x04\x06\x1A\x54\x4F\x03\x03\x0E\x1A\x40\x08\x00\x47\x0B\x0B\x7A\x1A\x1B\x79\x2E\x30\x2B\x38\x20\x7F\x70\x3E\x38\x3B\x2D\x75\x23\x2D\x68\x20\x2A\x23\x29\x61\x62\x2C\x2E\x2D\x3F\x67\x2D\x23\x5A\x15\x1D"
"\x0D\x09\x10\x0E\x16\x5C\x5D\x5E\x7B\x5B\x57\x27\x10\x1E\x1F\x01\x07\x09\x1C\x56\x4D\x11\x17\x0F\x11\x46\x0E\x02\x45\x28\x2E\x36\x2A\x7E\x2B\x33\x32\x72\x3F\x3F\x3F\x31\x7B\x74\x27\x2F\x3F\x3A\x30\x6E\x20\x22\x6D\x24\x22\x29\x2D\x33\x35\x21\x69\x5A\x09\x0D\x17\x5E\x10\x12\x5D\x16\x16\x1D\x10\x18\x13"
"\x54\x1A\x04\x07\x11\x47\x64\x65\x2F\x3F\x27\x22\x34\x28\x28\x20\x44\x24\x7A\x0F\x19\x0A\x15\x55\x6D\x74\x72\x10\x22\x34\x37\x23\x31\x75\x1E\x2A\x3B\x22\x6E\x71\x6C\x0A\x27\x2D\x25\x33\x27\x2B\x7E\x65\x14\x1A\x15\x1C\x52\x5F\x5B\x2F\x07\x1D\x50\x06\x1F\x03\x1C\x55\x02\x02\x0F\x01\x0B\x1C\x18\x4D\x12"
"\x11\x09\x17\x0F\x0B\x01\x02\x3F\x28\x7F\x79\x37\x39\x7C\x3C\x36\x3E\x39\x3F\x5C\x77\x74\x75\x24\x2E\x2D\x2D\x2B\x2B\x62\x47\x70\x6A\x60\x15\x34\x2E\x23\x22\x1F\x09\x0B\x59\x40\x5F\x32\x18\x05\x49\x50\x14\x58\x10\x5A\x55\x2B\x1F\x48\x05\x01\x08\x4C\x02\x0C\x4F\x40\x0E\x14\x47\x20\x04\x33\x37\x21\x79"
"\x3F\x2B\x7C\x6D\x6B\x69\x60\x61\x78\x5D\x67\x7C\x6A\x0A\x2B\x3D\x27\x20\x22\x3E\x62\x7D\x60\x0F\x23\x30\x7E\x65\x29\x0F\x19\x0B\x0A\x5F\x1D\x5D\x02\x01\x1F\x16\x04\x16\x19\x55\x54\x4B\x18\x06\x07\x01\x18\x4D\x16\x0C\x40\x15\x0E\x02\x44\x00\x22\x3E\x78\x71\x3F\x2D\x3B\x28\x3F\x36\x3E\x25\x25\x77\x3B"
"\x25\x3E\x22\x27\x27\x2F\x23\x65\x63\x48\x77\x69\x61\x05\x28\x2A\x21\x13\x0F\x11\x16\x10\x0C\x53\x2E\x17\x07\x04\x18\x18\x10\x07\x4F\x4A\x18\x0D\x07\x1D\x06\x0E\x01\x07\x43\x04\x04\x00\x06\x11\x09\x2E\x28\x76\x53\x6B\x76\x7C\x12\x19\x7D\x70\x05\x33\x24\x20\x6F\x6A\x39\x21\x2E\x26\x3B\x61\x2E\x2E\x2A"
"\x23\x2A\x66\x79\x64\x17\x0F\x15\x56\x73\x74\x3C\x33\x30\x3F\x32\x3E\x35\x56\x3B\x3D\x3B\x2F\x4B\x40\x1A\x0D\x07\x18\x0C\x11\x08\x13\x48\x6C\x4A\x44\x16\x39\x33\x2C\x38\x2D\x34\x2F\x7D\x7D\x30\x22\x34\x37\x23\x31\x75\x65\x3F\x26\x69\x03\x36\x18\x2C\x31\x28\x60\x6E\x32\x35\x64\x67\x14\x14\x0C\x1C\x0E"
"\x1E\x18\x53\x17\x0B\x15\x53\x56\x58\x07\x16\x4A\x0F\x09\x00\x02\x16\x4C\x42\x11\x17\x40\x51\x5F\x5D\x54\x55\x50\x76\x78\x2A\x3D\x37\x28\x3C\x21\x38\x23\x71\x79\x25\x21\x3B\x6A\x64\x3C\x27\x6E\x02\x35\x19\x23\x30\x2B\x4B\x6B\x67\x37\x26\x12\x0F\x19\x0A\x15\x0C\x5C\x52\x03\x06\x15\x03\x0F\x57\x5B\x01"
"\x04\x4B\x25\x10\x3A\x0E\x1F\x06\x42\x4C\x16\x6B\x4B\x47\x17\x06\x32\x2F\x39\x2A\x35\x2C\x7C\x72\x36\x36\x3C\x34\x22\x32\x74\x7A\x3E\x25\x68\x04\x37\x1B\x2D\x3E\x29\x63\x6F\x27\x4C\x6A\x64\x11\x12\x12\x0B\x59\x0E\x0D\x13\x1A\x00\x12\x1D\x4B\x56\x03\x15\x06\x01\x46\x0B\x1B\x0B\x0E\x18\x08\x42\x4C\x40"
"\x15\x07\x14\x0F\x48\x28\x2E\x36\x79\x71\x7F\x28\x3C\x21\x38\x7D\x35\x33\x3B\x31\x21\x2F\x6B\x67\x69\x3A\x2E\x3F\x26\x6F\x32\x35\x24\x34\x3E\x6A\x4F\x70\x29\x2D\x37\x30\x36\x32\x3A\x52\x32\x23\x51\x37\x33\x39\x3C\x24\x4B\x2E\x3B\x21\x22\x4C\x3E\x21\x2B\x25\x25\x33\x2B\x21\x37\x50\x76\x78\x7E\x0C\x2A"
"\x32\x7D\x25\x3A\x24\x39\x76\x3F\x3D\x32\x22\x2E\x3B\x3D\x6E\x3F\x3E\x24\x34\x2A\x2C\x24\x21\x22\x37\x62\x5A\x50\x58\x1A\x16\x1A\x1F\x16\x52\x07\x18\x14\x56\x03\x06\x1C\x0D\x0C\x0D\x1B\x49\x1C\x4C\x4A\x27\x0D\x01\x03\x0A\x02\x00\x42\x74\x7B\x11\x3F\x7E\x3E\x56\x7D\x72\x27\x31\x22\x3D\x77\x30\x3A\x2F"
"\x38\x68\x27\x21\x3B\x24\x24\x2C\x24\x7A\x61\x32\x2F\x21\x65\x3B\x18\x0C\x10\x11\x11\x5C\x2D\x00\x1C\x17\x03\x17\x1A\x54\x05\x0B\x1F\x00\x49\x07\x1C\x4C\x1A\x10\x0C\x0E\x06\x46\x08\x16\x45\x2E\x33\x3D\x79\x2B\x2C\x39\x2F\x72\x3F\x31\x32\x3D\x24\x5E\x75\x6A\x39\x21\x2E\x26\x3B\x3F\x63\x48\x49\x03\x0E"
"\x0B\x0A\x0B\x0B\x5A\x2E\x2B\x3C\x2D\x75\x51\x5D\x33\x06\x04\x1E\x56\x04\x00\x14\x18\x1F\x1D\x19\x4E\x00\x0A\x4D\x16\x0C\x0F\x0D\x15\x47\x4C\x11\x32\x34\x2D\x3E\x36\x7F\x0F\x29\x33\x21\x24\x24\x26\x77\x32\x3A\x26\x2F\x2D\x3B\x61\x1D\x39\x23\x62\x28\x25\x38\x66\x2E\x37\x65\x09\x12\x15\x09\x12\x1A\x0E"
"\x54\x5C\x79\x5D\x51\x34\x16\x17\x1E\x1F\x1B\x48\x1A\x0D\x1D\x05\x1D\x16\x10\x40\x00\x12\x47\x0A\x0C\x3D\x33\x2C\x79\x76\x28\x35\x29\x3A\x73\x77\x26\x37\x3C\x31\x75\x3E\x23\x2D\x69\x2D\x20\x21\x3D\x37\x37\x25\x33\x66\x33\x2B\x65\x08\x0E\x16\x59\x0A\x17\x15\x0E\x52\x07\x11\x02\x1D\x50\x5D\x5B\x60\x46"
"\x48\x2A\x02\x0A\x0D\x03\x17\x13\x40\x12\x05\x15\x0D\x15\x2E\x28\x78\x36\x30\x7F\x30\x32\x35\x3C\x3E\x7F\x5C\x7A\x74\x02\x2B\x3F\x2B\x21\x27\x21\x2B\x6D\x23\x63\x33\x24\x34\x31\x2D\x26\x1F\x5B\x19\x17\x1A\x5F\x0E\x18\x01\x07\x11\x03\x02\x1E\x1A\x12\x4A\x02\x1C\x49\x46\x1B\x0D\x1E\x09\x43\x14\x13\x0F"
"\x00\x03\x00\x28\x3E\x3C\x79\x3C\x26\x7C\x38\x24\x36\x3E\x25\x76\x1E\x10\x7C\x64\x41\x42\x1A\x0B\x0C\x19\x1F\x0B\x17\x19\x4B\x6B\x67\x10\x24\x09\x10\x0B\x59\x0C\x0A\x12\x5D\x07\x1D\x14\x14\x04\x57\x00\x1D\x0F\x4B\x0B\x06\x00\x09\x05\x0A\x17\x11\x05\x05\x46\x06\x07\x06\x35\x2E\x36\x2D\x7E\x72\x7C\x28"
"\x21\x36\x70\x25\x3E\x32\x74\x39\x2F\x2A\x3B\x3D\x6E\x3F\x3E\x24\x34\x2A\x2C\x24\x21\x22\x4E\x65\x5A\x15\x1D\x1C\x1A\x1A\x18\x53\x78\x5E\x50\x3C\x17\x1B\x03\x14\x18\x0E\x48\x06\x08\x1B\x09\x03\x42\x0B\x09\x05\x03\x14\x44\x11\x3B\x28\x33\x2A\x64\x7F\x2E\x38\x24\x3A\x35\x26\x76\x03\x35\x26\x21\x6B\x1B"
"\x2A\x26\x2A\x28\x38\x2E\x26\x32\x61\x0A\x2E\x26\x37\x1B\x09\x01\x59\x0C\x1A\x1B\x08\x1E\x12\x02\x1D\x0F\x59\x7E\x58\x4A\x2F\x01\x1A\x0F\x0D\x00\x08\x42\x17\x01\x12\x0D\x14\x44\x1C\x35\x2E\x78\x3D\x31\x7F\x32\x32\x26\x73\x22\x34\x35\x38\x33\x3B\x23\x31\x2D\x69\x66\x3C\x29\x2C\x30\x20\x28\x61\x32\x2F"
"\x21\x65\x14\x1A\x15\x1C\x5E\x10\x12\x11\x1B\x1D\x15\x51\x10\x1E\x06\x06\x1E\x42\x46\x63\x64\x3B\x3E\x22\x37\x21\x2C\x24\x35\x2F\x2B\x2A\x0E\x12\x16\x1E\x54\x72\x7C\x11\x33\x20\x24\x71\x04\x22\x3A\x75\x18\x2E\x3B\x3C\x22\x3B\x6C\x2E\x2D\x2F\x35\x2C\x28\x67\x37\x2D\x15\x0C\x0B\x59\x1B\x07\x15\x09\x52"
"\x10\x1F\x15\x13\x04\x54\x5D\x5A\x4B\x55\x49\x1D\x1A\x0F\x0E\x07\x10\x13\x48\x48\x6D\x49\x45\x08\x32\x3F\x31\x2A\x72\x3F\x31\x3B\x30\x3B\x71\x68\x77\x06\x20\x24\x6B\x3C\x2C\x3D\x3B\x3F\x6D\x36\x2B\x25\x61\x32\x26\x37\x2E\x5A\x12\x15\x14\x1B\x1B\x15\x1C\x06\x16\x1C\x08\x58\x7D\x59\x55\x4D\x23\x01\x1A"
"\x1A\x00\x1E\x14\x45\x43\x14\x00\x04\x47\x17\x0D\x35\x2C\x2B\x79\x3B\x29\x39\x2F\x2B\x73\x35\x29\x33\x34\x21\x21\x23\x24\x26\x69\x2F\x3B\x38\x28\x2F\x33\x34\x61\x31\x2E\x30\x2D\x5A\x1F\x1D\x0D\x1F\x16\x10\x0E\x5C"
);

MAN(desktops, "Virtual Desktops Guide",
"\x0D\x13\x19\x0D\x7E\x0B\x14\x18\x0B\x73\x11\x03\x13\x5D\x19\x20\x26\x3F\x21\x39\x22\x2A\x6C\x3A\x2D\x31\x2B\x32\x36\x26\x27\x20\x09\x5B\x17\x17\x5E\x10\x12\x18\x52\x1E\x1F\x1F\x1F\x03\x1B\x07\x44\x4B\x23\x0C\x0B\x1F\x4C\x1A\x0D\x11\x0B\x41\x09\x09\x44\x21\x3F\x28\x33\x2D\x31\x2F\x7C\x6C\x7E\x73\x3D"
"\x34\x32\x3E\x35\x75\x25\x25\x42\x0D\x2B\x3C\x27\x39\x2D\x33\x60\x73\x6A\x67\x25\x2B\x1E\x5B\x1E\x15\x17\x0F\x5C\x1F\x17\x07\x07\x14\x13\x19\x54\x01\x02\x0E\x05\x49\x43\x4F\x09\x0C\x01\x0B\x40\x09\x07\x14\x44\x0C\x2E\x28\x78\x36\x29\x31\x7C\x32\x22\x36\x3E\x71\x21\x3E\x3A\x31\x25\x3C\x3B\x67\x44\x45"
"\x1F\x05\x0D\x11\x14\x02\x13\x13\x17\x4F\x57\x5B\x2F\x10\x10\x54\x3F\x09\x00\x1F\x5B\x35\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x22\x08\x15\x43\x16\x08\x14\x13\x11\x04\x36\x7B\x3C\x3C\x2D\x34\x28\x32\x22\x59\x7D\x71\x01\x3E\x3A\x7E\x09\x3F\x3A\x25\x65\x03\x29\x2B\x36\x6C\x12\x28\x21\x2F\x30\x65\x29"
"\x0C\x11\x0D\x1D\x17\x5C\x19\x17\x00\x1B\x05\x19\x07\x7E\x58\x4A\x3C\x01\x07\x45\x2C\x18\x1F\x0E\x48\x26\x55\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x1C\x30\x32\x21\x36\x70\x25\x3E\x32\x74\x36\x3F\x39\x3A\x2C\x20\x3B\x6C\x29\x27\x30\x2B\x35\x29\x37\x4E\x68\x5A\x2C\x11\x17\x55\x2B\x1D\x1F\x52\x53\x50\x51"
"\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x3B\x0D\x1E\x09\x43\x16\x08\x03\x10\x44\x4D\x29\x3E\x3D\x79\x3F\x33\x30\x7D\x36\x36\x23\x3A\x22\x38\x24\x26\x6A\x2A\x3C\x69\x3A\x27\x29\x6D\x36\x2C\x30\x68\x4C\x4D\x10\x04\x29\x30\x58\x2F\x37\x3A\x2B\x77\x5F\x53\x27\x18\x18\x5C\x20\x14\x08\x4B\x07\x1B\x4E\x1B\x04"
"\x08\x42\x37\x01\x12\x0D\x47\x12\x0C\x3F\x2C\x78\x3B\x2B\x2B\x28\x32\x3C\x73\x3F\x3F\x76\x23\x3C\x30\x6A\x3F\x29\x3A\x25\x2D\x2D\x3F\x6C\x49\x6D\x61\x12\x28\x34\x7F\x5A\x02\x17\x0C\x0C\x5F\x18\x18\x01\x18\x04\x1E\x06\x04\x5A\x55\x22\x04\x1E\x0C\x1C\x4F\x0D\x4D\x06\x06\x13\x0A\x12\x08\x14\x45\x2E\x34"
"\x78\x2A\x3B\x3A\x7C\x34\x26\x20\x70\x26\x3F\x39\x30\x3A\x3D\x38\x73\x69\x2D\x23\x25\x2E\x29\x63\x34\x2E\x66\x34\x33\x2C\x0E\x18\x10\x57\x74\x52\x5C\x5A\x59\x54\x50\x12\x04\x12\x15\x01\x0F\x18\x48\x08\x4E\x01\x09\x1A\x42\x0C\x0E\x04\x5D\x47\x10\x0D\x3F\x7B\x00\x79\x3D\x33\x33\x2E\x37\x20\x70\x3E\x38"
"\x32\x74\x7D\x23\x3F\x3B\x69\x39\x26\x22\x29\x2D\x34\x33\x61\x2B\x28\x32\x20\x5A\x0F\x17\x59\x0A\x17\x19\x77\x52\x53\x00\x03\x13\x01\x1D\x1A\x1F\x18\x48\x0D\x0B\x1C\x07\x19\x0D\x13\x49\x4F\x6C\x4A\x44\x21\x28\x3A\x3F\x79\x3F\x7F\x2B\x34\x3C\x37\x3F\x26\x76\x23\x3C\x20\x27\x29\x26\x28\x27\x23\x6C\x22"
"\x2C\x37\x2F\x61\x27\x29\x2B\x31\x12\x1E\x0A\x59\x1A\x1A\x0F\x16\x06\x1C\x00\x51\x02\x18\x54\x18\x05\x1D\x0D\x49\x07\x1B\x42\x67\x68\x31\x25\x2F\x27\x2A\x21\x45\x7C\x7B\x0A\x1C\x11\x0D\x18\x18\x00\x59\x7D\x71\x1F\x39\x74\x01\x2B\x38\x23\x69\x38\x26\x29\x3A\x6E\x63\x23\x2D\x2F\x24\x2F\x65\x1B\x5B\x1C"
"\x1C\x0D\x14\x08\x12\x02\x54\x03\x51\x18\x16\x19\x10\x4A\x1F\x07\x49\x1C\x0A\x02\x0C\x0F\x06\x40\x08\x12\x47\x4C\x00\x74\x3C\x76\x79\x79\x08\x33\x2F\x39\x74\x7C\x5B\x76\x77\x73\x12\x2B\x26\x21\x27\x29\x68\x65\x63\x62\x07\x32\x20\x21\x67\x30\x2D\x0F\x16\x1A\x17\x1F\x16\x10\x0E\x52\x07\x1F\x51\x04\x12"
"\x1B\x07\x0E\x0E\x1A\x47\x64\x42\x4C\x3A\x0B\x0D\x04\x0E\x11\x14\x44\x54\x6B\x61\x78\x2B\x37\x38\x34\x29\x7F\x30\x3C\x38\x35\x3C\x74\x34\x6A\x2F\x2D\x3A\x25\x3B\x23\x3D\x62\x7D\x60\x13\x23\x29\x25\x28\x1F\x5B\x57\x59\x33\x10\x0A\x18\x52\x1F\x15\x17\x02\x57\x5B\x55\x27\x04\x1E\x0C\x4E\x1D\x05\x0A\x0A"
"\x17\x4E\x6B\x6C\x37\x21\x37\x77\x1F\x1D\x0A\x15\x0B\x13\x0D\x72\x04\x11\x1D\x1A\x07\x15\x05\x0F\x19\x68\x61\x19\x26\x22\x29\x2D\x34\x33\x61\x77\x76\x6D\x4F\x57\x5B\x2B\x1C\x0A\x0B\x15\x13\x15\x00\x50\x4F\x56\x27\x11\x07\x19\x04\x06\x08\x02\x06\x16\x0C\x16\x0A\x0F\x0F\x46\x59\x44\x27\x3B\x38\x33\x3E"
"\x2C\x30\x29\x33\x36\x73\x6E\x71\x24\x3E\x33\x3D\x3E\x66\x2B\x25\x27\x2C\x27\x6D\x23\x63\x22\x20\x25\x2C\x23\x37\x15\x0E\x16\x1D\x5E\x41\x76\x5D\x52\x54\x23\x14\x02\x57\x12\x1A\x18\x4B\x0C\x0C\x1D\x04\x18\x02\x12\x43\x51\x4E\x54\x49\x4A\x4B\x7D\x7B\x37\x2B\x7E\x3C\x34\x32\x3D\x20\x35\x71\x71\x04\x31"
"\x21\x6A\x2D\x27\x3B\x6E\x2E\x20\x21\x62\x27\x25\x32\x2D\x33\x2B\x35\x09\x5C\x56\x73\x53\x5F\x39\x1C\x11\x1B\x50\x15\x13\x04\x1F\x01\x05\x1B\x48\x0A\x0F\x01\x4C\x1E\x0A\x0C\x17\x41\x07\x47\x00\x0C\x3C\x3D\x3D\x2B\x3B\x31\x28\x7D\x22\x3A\x33\x25\x23\x25\x31\x75\x25\x39\x68\x3A\x22\x26\x28\x28\x31\x2B"
"\x2F\x36\x68\x4D\x4E\x11\x33\x2B\x2B\x73\x53\x5F\x3D\x11\x06\x58\x24\x10\x14\x57\x17\x0C\x09\x07\x0D\x1A\x4E\x18\x05\x03\x06\x0C\x17\x12\x46\x08\x02\x45\x2E\x33\x3D\x79\x1D\x0A\x0E\x0F\x17\x1D\x04\x71\x32\x32\x27\x3E\x3E\x24\x38\x69\x21\x21\x20\x34\x6C\x49\x6D\x61\x61\x14\x2C\x2A\x0D\x5B\x0F\x10\x10"
"\x1B\x13\x0A\x01\x53\x16\x03\x19\x1A\x54\x14\x06\x07\x48\x0D\x0B\x1C\x07\x19\x0D\x13\x13\x46\x46\x0E\x0A\x45\x0E\x3A\x2B\x32\x7E\x29\x35\x38\x25\x73\x23\x34\x22\x23\x3D\x3B\x2D\x38\x68\x61\x1D\x2A\x38\x39\x2B\x2D\x27\x32\x66\x79\x4E\x65\x5A\x28\x01\x0A\x0A\x1A\x11\x5D\x4C\x53\x3D\x04\x1A\x03\x1D\x01"
"\x0B\x18\x03\x00\x00\x08\x4C\x53\x42\x27\x05\x12\x0D\x13\x0B\x15\x29\x72\x78\x3A\x36\x3E\x32\x3A\x37\x20\x70\x10\x3A\x23\x7F\x01\x2B\x29\x68\x2B\x2B\x27\x2D\x3B\x2B\x2C\x32\x6F\x4C\x6A\x64\x16\x14\x1A\x08\x59\x12\x1E\x05\x12\x07\x07\x03\x51\x01\x18\x06\x1E\x4A\x1B\x0D\x1B\x4E\x0B\x09\x1E\x09\x17\x0F"
"\x11\x48\x6D\x49\x45\x1B\x7B\x28\x36\x29\x3A\x2E\x70\x27\x20\x35\x23\x76\x20\x3B\x27\x21\x2D\x24\x26\x39\x75\x6C\x09\x27\x30\x2B\x35\x29\x37\x64\x74\x5A\x46\x58\x1A\x11\x12\x11\x08\x1C\x1A\x13\x10\x02\x1E\x1B\x1B\x4A\x43\x05\x08\x07\x03\x43\x0E\x0A\x02\x14\x48\x4A\x6D\x44\x45\x1E\x3E\x2B\x32\x2A\x30"
"\x2C\x7D\x60\x73\x6D\x71\x21\x38\x26\x3E\x66\x6B\x0C\x2C\x3D\x24\x38\x22\x32\x63\x73\x61\x7B\x67\x29\x20\x1E\x12\x19\x57\x74\x75\x2B\x35\x2B\x53\x25\x22\x33\x57\x20\x3D\x2F\x26\x62\x44\x4E\x29\x09\x1A\x07\x11\x40\x16\x0F\x09\x00\x0A\x2D\x28\x78\x29\x3B\x2D\x7C\x39\x37\x20\x3B\x25\x39\x27\x74\x68\x6A"
"\x27\x2D\x3A\x3D\x6F\x28\x24\x31\x37\x32\x20\x25\x33\x2D\x2A\x14\x57\x58\x1C\x1F\x0C\x15\x18\x00\x53\x31\x1D\x02\x5C\x20\x14\x08\x45\x62\x44\x4E\x2C\x00\x08\x03\x0D\x40\x15\x07\x14\x0F\x07\x3B\x29\x78\x29\x3B\x2D\x7C\x2D\x20\x3C\x3A\x34\x35\x23\x7A\x5F\x67\x6B\x1C\x21\x27\x3C\x6C\x3D\x30\x2C\x27\x33"
"\x27\x2A\x7E\x65\x5D\x0D\x11\x0B\x0A\x0A\x1D\x11\x5F\x17\x15\x02\x1D\x03\x1B\x05\x47\x05\x0D\x1E\x49\x4F\x43\x4D\x0C\x06\x18\x15\x46\x48\x44\x15\x28\x3E\x2E\x30\x31\x2A\x2F\x7D\x7D\x73\x33\x3D\x39\x24\x31\x7B"
);

MAN(accessibility, "Accessibility Guide",
"\x0C\x12\x0B\x10\x11\x11\x56\x70\x72\x07\x35\x29\x22\x77\x27\x3C\x30\x2E\x72\x69\x1D\x2A\x38\x39\x2B\x2D\x27\x32\x66\x79\x64\x04\x19\x18\x1D\x0A\x0D\x16\x1E\x14\x1E\x1A\x04\x08\x56\x49\x54\x21\x0F\x13\x1C\x49\x1D\x06\x16\x08\x42\x4E\x40\x12\x0A\x0E\x00\x00\x28\x7B\x28\x2B\x3B\x29\x35\x38\x25\x7D\x5A"
"\x7C\x76\x1A\x35\x32\x24\x22\x2E\x20\x2B\x3D\x76\x6D\x15\x2A\x2E\x6A\x16\x2B\x31\x36\x5A\x01\x17\x16\x13\x0C\x5C\x14\x1C\x5F\x50\x26\x1F\x19\x5F\x38\x03\x05\x1D\x1A\x4E\x00\x19\x19\x4E\x43\x37\x08\x08\x4C\x21\x16\x39\x7B\x3B\x35\x31\x2C\x39\x2E\x7C\x73\x03\x34\x22\x23\x3D\x3B\x2D\x38\x72\x43\x6E\x6F"
"\x36\x22\x2D\x2E\x60\x2D\x23\x31\x21\x29\x56\x5B\x1E\x16\x12\x13\x13\x0A\x52\x1E\x1F\x04\x05\x12\x5B\x1E\x0F\x12\x0A\x06\x0F\x1D\x08\x41\x42\x05\x15\x0D\x0A\x47\x17\x06\x28\x3E\x3D\x37\x7E\x30\x2E\x7D\x3E\x36\x3E\x22\x76\x3A\x3B\x31\x2F\x65\x42\x64\x6E\x0C\x23\x21\x2D\x31\x60\x27\x2F\x2B\x30\x20\x08"
"\x08\x42\x59\x19\x0D\x19\x04\x01\x10\x11\x1D\x13\x5B\x54\x16\x05\x07\x07\x1B\x43\x0D\x00\x04\x0C\x07\x40\x07\x0F\x0B\x10\x00\x28\x28\x78\x71\x09\x36\x32\x76\x11\x27\x22\x3D\x7D\x14\x74\x21\x25\x2C\x2F\x25\x2B\x3C\x65\x63\x48\x6E\x60\x02\x29\x29\x30\x37\x1B\x08\x0C\x59\x0A\x17\x19\x10\x17\x00\x4A\x51"
"\x1E\x1E\x13\x1D\x4A\x08\x07\x07\x1A\x1D\x0D\x1E\x16\x43\x06\x0E\x14\x47\x01\x04\x29\x32\x3D\x2B\x7E\x2D\x39\x3C\x36\x3A\x3E\x36\x78\x5D\x79\x75\x09\x3E\x3A\x3A\x21\x3D\x6C\x6B\x62\x33\x2F\x28\x28\x33\x21\x37\x40\x5B\x0B\x10\x04\x1A\x50\x5D\x11\x1C\x1C\x1E\x04\x5B\x54\x05\x05\x02\x06\x1D\x0B\x1D\x4C"
"\x1E\x16\x1A\x0C\x04\x46\x4F\x37\x00\x2E\x2F\x31\x37\x39\x2C\x7C\x63\x72\x12\x33\x32\x33\x24\x27\x3C\x28\x22\x24\x20\x3A\x36\x46\x6D\x62\x7D\x60\x0C\x29\x32\x37\x20\x5A\x0B\x17\x10\x10\x0B\x19\x0F\x52\x12\x1E\x15\x56\x03\x1B\x00\x09\x03\x41\x47\x64\x42\x4C\x39\x07\x1B\x14\x41\x05\x12\x16\x16\x35\x29"
"\x62\x79\x2A\x37\x35\x3E\x39\x3D\x35\x22\x25\x7B\x74\x3C\x24\x2F\x21\x2A\x2F\x3B\x23\x3F\x62\x20\x2F\x2D\x29\x35\x6A\x4F\x57\x5B\x36\x18\x0C\x0D\x1D\x09\x1D\x01\x4A\x51\x05\x14\x06\x10\x0F\x05\x48\x1B\x0B\x0E\x08\x08\x10\x4F\x40\x36\x0F\x09\x4F\x26\x2E\x29\x34\x72\x1B\x31\x28\x38\x20\x7D\x70\x03\x33"
"\x36\x30\x26\x6A\x38\x2B\x3B\x2B\x2A\x22\x3E\x6E\x63\x2E\x20\x30\x2E\x23\x24\x0E\x1E\x0B\x59\x09\x16\x08\x15\x78\x53\x50\x1A\x13\x0E\x16\x1A\x0B\x19\x0C\x47\x4E\x29\x19\x01\x0E\x43\x03\x0E\x0B\x0A\x05\x0B\x3E\x7B\x2B\x3C\x2A\x7F\x35\x33\x72\x1D\x31\x23\x24\x36\x20\x3A\x38\x6B\x3B\x2C\x3A\x3B\x25\x23"
"\x25\x30\x6E\x4B\x4C\x0F\x01\x04\x28\x32\x36\x3E\x74\x52\x5C\x3C\x07\x17\x19\x1E\x4C\x57\x19\x1A\x04\x04\x48\x08\x1B\x0B\x05\x02\x4E\x43\x06\x0D\x07\x14\x0C\x45\x39\x3A\x28\x2D\x37\x30\x32\x2E\x72\x3C\x3E\x71\x37\x22\x30\x3C\x25\x6B\x26\x26\x3A\x26\x2A\x24\x21\x22\x34\x28\x29\x29\x37\x6B\x70\x56\x58"
"\x3A\x1F\x0F\x08\x14\x1D\x1D\x03\x4B\x56\x04\x00\x0C\x06\x0E\x48\x06\x08\x4F\x0F\x01\x0D\x10\x05\x05\x46\x04\x05\x15\x2E\x32\x37\x37\x2D\x7F\x39\x2B\x37\x21\x29\x26\x3E\x32\x26\x30\x6A\x63\x3B\x20\x34\x2A\x60\x6D\x21\x2C\x2C\x2E\x34\x6B\x64\x2A\x0A\x1A\x1B\x10\x0A\x06\x55\x53\x78\x5E\x50\x3D\x1F\x01"
"\x11\x55\x09\x0A\x18\x1D\x07\x00\x02\x1E\x42\x4B\x37\x08\x08\x56\x55\x4C\x60\x7B\x2A\x3C\x3F\x33\x71\x29\x3B\x3E\x35\x71\x35\x36\x24\x21\x23\x24\x26\x3A\x6E\x20\x2A\x6D\x23\x2D\x39\x61\x27\x32\x20\x2C\x15\x5B\x17\x17\x5E\x0B\x14\x18\x52\x23\x33\x5F\x7C\x7D\x3D\x3B\x3E\x2E\x3A\x28\x2D\x3B\x25\x22\x2C"
"\x69\x4D\x41\x30\x08\x0D\x06\x3F\x7B\x39\x3A\x3D\x3A\x2F\x2E\x72\x7B\x07\x38\x38\x66\x65\x7C\x70\x6B\x2B\x26\x20\x3B\x3E\x22\x2E\x63\x34\x29\x23\x67\x33\x2D\x15\x17\x1D\x59\x2E\x3C\x5C\x1F\x0B\x53\x06\x1E\x1F\x14\x11\x55\x47\x4B\x3F\x00\x00\x44\x2F\x19\x10\x0F\x4B\x32\x46\x14\x10\x04\x28\x2F\x2B\x53"
"\x7E\x7F\x35\x29\x7C\x73\x13\x3E\x3B\x3A\x35\x3B\x2E\x38\x68\x25\x27\x24\x29\x6D\x65\x2C\x30\x24\x28\x67\x01\x21\x1D\x1E\x5F\x55\x5E\x58\x1F\x11\x1B\x10\x1B\x51\x25\x03\x15\x07\x1E\x4C\x44\x49\x49\x1C\x0F\x1F\x0D\x0F\x0C\x41\x02\x08\x13\x0B\x7D\x75\x52\x74\x7E\x1A\x25\x38\x72\x30\x3F\x3F\x22\x25\x3B"
"\x39\x70\x6B\x2B\x26\x20\x3B\x3E\x22\x2E\x63\x34\x29\x23\x67\x29\x2A\x0F\x08\x1D\x59\x09\x16\x08\x15\x52\x0A\x1F\x04\x04\x57\x11\x0C\x0F\x18\x48\x41\x0B\x16\x09\x4D\x16\x11\x01\x02\x0D\x02\x16\x16\x76\x7B\x3D\x77\x39\x71\x56\x7D\x72\x07\x3F\x33\x3F\x3E\x7D\x7B\x40\x66\x68\x02\x2B\x36\x2E\x22\x23\x31"
"\x24\x7B\x66\x08\x2A\x68\x09\x18\x0A\x1C\x1B\x11\x5C\x16\x17\x0A\x12\x1E\x17\x05\x10\x59\x4A\x18\x1C\x00\x0D\x04\x15\x4D\x09\x06\x19\x12\x46\x4F\x14\x17\x3F\x28\x2B\x79\x31\x31\x39\x7D\x39\x36\x29\x71\x37\x23\x74\x34\x6A\x3F\x21\x24\x2B\x66\x60\x47\x62\x63\x26\x28\x2A\x33\x21\x37\x5A\x10\x1D\x00\x0D"
"\x5F\x54\x14\x15\x1D\x1F\x03\x13\x57\x15\x16\x09\x02\x0C\x0C\x00\x1B\x0D\x01\x42\x11\x05\x11\x03\x06\x10\x16\x73\x77\x78\x2D\x31\x38\x3B\x31\x37\x73\x3B\x34\x2F\x24\x74\x7D\x39\x24\x3D\x27\x2A\x6F\x23\x23\x62\x2F\x2F\x22\x2D\x4D\x64\x65\x11\x1E\x01\x0A\x57\x51\x5C\x29\x1A\x1A\x03\x51\x06\x05\x1B\x12"
"\x18\x0A\x05\x53\x4E\x1C\x18\x04\x01\x08\x19\x4E\x12\x08\x03\x02\x36\x3E\x77\x3F\x37\x33\x28\x38\x20\x7C\x3D\x3E\x23\x24\x31\x75\x21\x2E\x31\x3A\x6E\x3B\x23\x2A\x25\x2F\x25\x32\x68\x4D\x69\x65\x37\x14\x0D\x0A\x1B\x5F\x17\x18\x0B\x00\x4A\x51\x15\x18\x1A\x01\x18\x04\x04\x49\x1A\x07\x09\x4D\x12\x0C\x09"
"\x0F\x12\x02\x16\x45\x2D\x32\x2C\x31\x7E\x2B\x34\x38\x72\x3D\x25\x3C\x33\x25\x3D\x36\x6A\x20\x2D\x30\x3E\x2E\x28\x63\x48\x6E\x60\x0C\x29\x32\x37\x20\x40\x5B\x08\x16\x17\x11\x08\x18\x00\x53\x03\x18\x0C\x12\x5B\x16\x05\x07\x07\x1B\x42\x4F\x01\x02\x17\x10\x05\x41\x0D\x02\x1D\x16\x7A\x28\x28\x3C\x3B\x3B"
"\x72\x57\x7F\x73\x03\x21\x33\x32\x37\x3D\x70\x6B\x2C\x20\x2D\x3B\x2D\x39\x2B\x2C\x2E\x61\x6E\x10\x2D\x2B\x51\x33\x51\x59\x0A\x06\x0C\x18\x01\x53\x07\x19\x17\x03\x54\x0C\x05\x1E\x48\x1A\x0F\x16\x42\x67\x68\x34\x28\x24\x34\x22\x44\x2C\x0E\x7B\x19\x15\x12\x7F\x10\x14\x04\x16\x03\x5B\x05\x32\x20\x21\x23"
"\x25\x2F\x3A\x6E\x71\x6C\x0C\x21\x20\x25\x32\x35\x2E\x26\x2C\x16\x12\x0C\x00\x50\x5F\x33\x11\x16\x16\x02\x51\x13\x19\x00\x07\x03\x0E\x1B\x49\x0F\x03\x1F\x02\x42\x06\x18\x08\x15\x13\x44\x10\x34\x3F\x3D\x2B\x7E\x1A\x3D\x2E\x37\x73\x3F\x37\x76\x16\x37\x36\x2F\x38\x3B\x43\x66\x3B\x24\x24\x31\x63\x30\x33"
"\x29\x20\x36\x24\x17\x41\x58\x5E\x1F\x1C\x1F\x18\x01\x00\x19\x13\x1F\x1B\x1D\x01\x13\x4C\x48\x19\x0F\x08\x09\x1E\x4E\x43\x47\x04\x07\x14\x01\x0A\x3C\x3A\x3B\x3A\x3B\x2C\x2F\x70\x78\x74\x79\x7F\x5C\x5D\x05\x00\x03\x08\x03\x69\x1D\x1B\x0D\x1F\x16\x10\x4A\x6C\x66\x10\x2D\x2B\x51\x2E\x58\x16\x0E\x1A\x12"
"\x0E\x52\x32\x13\x12\x13\x04\x07\x1C\x08\x02\x04\x00\x1A\x16\x4C\x1E\x07\x17\x14\x08\x08\x00\x17\x45\x3E\x32\x2A\x3C\x3D\x2B\x30\x24\x7C\x59\x7D\x71\x02\x3F\x3D\x26\x6A\x3B\x3A\x26\x29\x3D\x2D\x20\x78\x63\x67\x2F\x27\x35\x36\x24\x0E\x14\x0A\x5E\x52\x5F\x5B\x10\x13\x14\x1E\x18\x10\x1E\x11\x07\x4D\x47"
"\x48\x4E\x06\x06\x0B\x05\x4F\x00\x0F\x0F\x12\x15\x05\x16\x2E\x7C\x74\x53\x7E\x7F\x7B\x3C\x31\x30\x35\x22\x25\x3E\x36\x3C\x26\x22\x3C\x30\x63\x3B\x29\x35\x36\x6E\x33\x28\x3C\x22\x63\x69\x5A\x5C\x1B\x15\x11\x0C\x19\x19\x5F\x10\x11\x01\x02\x1E\x1B\x1B\x19\x4C\x48\x08\x00\x0B\x4C\x00\x0D\x11\x05\x4F"
);

MAN(clean_install, "Clean Install Guide",
"\x0D\x13\x1D\x17\x7E\x0B\x13\x7D\x16\x1C\x70\x18\x02\x5D\x79\x75\x1A\x2E\x3A\x3A\x27\x3C\x38\x28\x2C\x37\x60\x22\x29\x35\x36\x30\x0A\x0F\x11\x16\x10\x5F\x08\x15\x13\x07\x50\x03\x13\x07\x15\x1C\x18\x18\x48\x0A\x0F\x01\x02\x02\x16\x43\x06\x08\x1E\x49\x6E\x48\x7A\x08\x3D\x35\x32\x36\x32\x3A\x7D\x34\x39"
"\x27\x3F\x39\x33\x75\x2B\x3C\x29\x30\x6E\x3B\x24\x28\x62\x13\x03\x61\x6E\x26\x28\x36\x15\x41\x58\x5E\x2C\x1A\x0F\x18\x06\x53\x04\x19\x1F\x04\x54\x25\x29\x4B\x45\x49\x3C\x0A\x01\x02\x14\x06\x40\x04\x10\x02\x16\x1C\x2E\x33\x31\x37\x39\x7F\x71\x57\x72\x73\x13\x3D\x33\x36\x3A\x75\x2E\x39\x21\x3F\x2B\x3C"
"\x6B\x6D\x26\x2C\x25\x32\x66\x33\x2C\x2C\x09\x5B\x0F\x10\x0A\x17\x13\x08\x06\x53\x25\x22\x34\x5E\x5A\x7F\x47\x4B\x3A\x0C\x08\x1D\x09\x1E\x0A\x0A\x0E\x06\x46\x06\x02\x11\x3F\x29\x78\x20\x3B\x3E\x2E\x2E\x72\x3C\x36\x71\x35\x25\x21\x33\x3E\x6B\x65\x69\x2F\x6F\x2F\x21\x27\x22\x2E\x61\x2F\x29\x37\x31\x1B"
"\x17\x14\x59\x11\x19\x5C\x2A\x1B\x1D\x14\x1E\x01\x04\x54\x44\x5B\x45\x62\x63\x3D\x3B\x29\x3D\x42\x52\x40\x4C\x46\x25\x25\x26\x11\x7B\x0D\x09\x54\x72\x7C\x04\x3D\x26\x22\x71\x30\x3E\x38\x30\x39\x67\x68\x2B\x3C\x20\x3B\x3E\x27\x31\x60\x25\x27\x33\x25\x69\x5A\x0B\x0A\x16\x1A\x0A\x1F\x09\x52\x18\x15\x08"
"\x05\x5B\x54\x14\x04\x0F\x48\x08\x00\x16\x18\x05\x0B\x0D\x07\x41\x0F\x09\x44\x24\x2A\x2B\x1C\x38\x2A\x3E\x72\x57\x7F\x73\x12\x30\x35\x3C\x74\x20\x3A\x6B\x3C\x21\x2B\x6F\x0E\x24\x36\x0F\x2F\x22\x2D\x22\x36\x65\x08\x1E\x1B\x16\x08\x1A\x0E\x04\x52\x18\x15\x08\x56\x16\x1A\x11\x4A\x1C\x1A\x00\x1A\x0A\x4C"
"\x09\x0D\x14\x0E\x41\x12\x0F\x01\x45\x17\x32\x3B\x2B\x31\x2C\x33\x3B\x26\x73\x31\x32\x35\x38\x21\x3B\x3E\x41\x68\x69\x66\x36\x23\x38\x62\x34\x29\x2D\x2A\x67\x2A\x20\x1F\x1F\x58\x10\x0A\x5F\x1D\x09\x52\x00\x19\x16\x18\x5A\x1D\x1B\x43\x45\x62\x63\x3D\x3B\x29\x3D\x42\x51\x40\x4C\x46\x20\x21\x31\x7A\x0C"
"\x11\x17\x1A\x10\x0B\x0E\x58\x7E\x70\x1C\x3F\x34\x26\x3A\x39\x24\x2E\x3D\x69\x3C\x6C\x00\x27\x27\x29\x20\x66\x04\x36\x20\x1B\x0F\x11\x16\x10\x5F\x28\x12\x1D\x1F\x50\x59\x19\x11\x12\x1C\x09\x02\x09\x05\x47\x55\x4C\x09\x0D\x14\x0E\x0D\x09\x06\x00\x16\x7A\x0C\x31\x37\x3A\x30\x2B\x2E\x72\x62\x61\x71\x37"
"\x39\x30\x5F\x6A\x6B\x3F\x3B\x27\x3B\x29\x3E\x62\x22\x60\x23\x29\x28\x30\x24\x18\x17\x1D\x59\x2B\x2C\x3E\x5D\x5A\x4B\x50\x36\x34\x5C\x5D\x5B\x60\x46\x48\x26\x1C\x4F\x08\x02\x15\x0D\x0C\x0E\x07\x03\x44\x11\x32\x3E\x78\x10\x0D\x10\x7C\x3B\x20\x3C\x3D\x71\x1B\x3E\x37\x27\x25\x38\x27\x2F\x3A\x6F\x2D\x23"
"\x26\x63\x22\x34\x34\x29\x6B\x32\x08\x12\x0C\x1C\x5E\x16\x08\x5D\x06\x1C\x50\x24\x25\x35\x5A\x7F\x47\x4B\x3C\x01\x07\x1C\x4C\x1D\x10\x0C\x07\x13\x07\x0A\x5E\x45\x7D\x36\x3D\x3D\x37\x3E\x71\x3E\x20\x36\x31\x25\x3F\x38\x3A\x78\x3E\x24\x27\x25\x69\x6F\x23\x3D\x27\x2D\x33\x61\x32\x2F\x21\x65\x15\x1D\x1E"
"\x10\x1D\x16\x1D\x11\x52\x17\x1F\x06\x18\x1B\x1B\x14\x0E\x4B\x18\x08\x09\x0A\x42\x67\x68\x30\x34\x24\x36\x47\x57\x45\x77\x7B\x1A\x16\x11\x0B\x7C\x1B\x00\x1C\x1D\x71\x02\x1F\x11\x75\x1F\x18\x0A\x43\x63\x6F\x05\x23\x31\x26\x32\x35\x66\x12\x17\x07\x5A\x45\x58\x0B\x1B\x0C\x08\x1C\x00\x07\x50\x4F\x56\x15"
"\x1B\x1A\x1E\x4B\x05\x0C\x00\x1A\x4C\x45\x24\x52\x52\x4E\x20\x56\x55\x4A\x1F\x28\x3B\x75\x7E\x3B\x39\x2D\x37\x3D\x34\x22\x76\x38\x3A\x75\x28\x39\x29\x27\x2A\x66\x6C\x73\x48\x63\x60\x32\x23\x2B\x21\x26\x0E\x5B\x0C\x11\x1B\x5F\x29\x2E\x30\x5D\x50\x3E\x04\x4D\x54\x26\x0F\x1F\x1C\x00\x00\x08\x1F\x4D\x5C"
"\x43\x33\x18\x15\x13\x01\x08\x7A\x65\x78\x0B\x3B\x3C\x33\x2B\x37\x21\x29\x71\x68\x77\x15\x31\x3C\x2A\x26\x2A\x2B\x2B\x6C\x3E\x36\x22\x32\x35\x33\x37\x64\x7B\x70\x5B\x58\x2C\x0D\x1A\x5C\x1C\x52\x17\x15\x07\x1F\x14\x11\x55\x54\x4B\x3D\x3A\x2C\x41\x66\x40\x42\x2E\x01\x0A\x03\x47\x17\x10\x28\x3E\x78\x0A"
"\x3B\x3C\x29\x2F\x37\x73\x12\x3E\x39\x23\x74\x38\x2B\x3F\x2B\x21\x2B\x3C\x6C\x39\x2A\x26\x60\x28\x28\x34\x30\x24\x16\x17\x1D\x0B\x5E\x57\x18\x18\x14\x12\x05\x1D\x02\x57\x03\x1A\x18\x00\x1B\x40\x40\x65\x66\x3E\x36\x26\x30\x41\x52\x47\x49\x45\x13\x15\x0B\x0D\x1F\x13\x10\x57\x7F\x73\x1C\x30\x38\x30\x21"
"\x34\x2D\x2E\x68\x77\x6E\x06\x22\x3E\x36\x22\x2C\x2D\x66\x29\x2B\x32\x5A\x45\x58\x5E\x37\x5F\x18\x12\x1C\x54\x04\x51\x1E\x16\x02\x10\x4A\x0A\x48\x19\x1C\x00\x08\x18\x01\x17\x40\x0A\x03\x1E\x43\x45\x72\x3A\x3B\x2D\x37\x29\x3D\x29\x3B\x3C\x3E\x71\x22\x3E\x31\x26\x40\x6B\x68\x3D\x21\x6F\x35\x22\x37\x31"
"\x60\x0C\x2F\x24\x36\x2A\x09\x14\x1E\x0D\x5E\x1E\x1F\x1E\x1D\x06\x1E\x05\x59\x13\x1D\x12\x03\x1F\x09\x05\x4E\x03\x05\x0E\x07\x0D\x13\x04\x4F\x49\x6E\x48\x7A\x7C\x1B\x2C\x2D\x2B\x33\x30\x68\x73\x19\x3F\x25\x23\x35\x39\x26\x6B\x1F\x20\x20\x2B\x23\x3A\x31\x63\x2F\x2F\x2A\x3E\x63\x65\x44\x5B\x1C\x1C\x12"
"\x1A\x08\x18\x52\x32\x3C\x3D\x56\x07\x15\x07\x1E\x02\x1C\x00\x01\x01\x1F\x4D\x0D\x0D\x40\x15\x0E\x02\x44\x11\x3B\x29\x3F\x3C\x2A\x55\x7C\x7D\x36\x3A\x23\x3A\x76\x7F\x3B\x27\x6A\x24\x26\x25\x37\x6F\x38\x25\x27\x63\x17\x28\x28\x23\x2B\x32\x09\x5B\x17\x17\x1B\x0C\x55\x5D\x4C\x53\x3E\x14\x0E\x03\x54\x1A"
"\x04\x4B\x1C\x01\x0B\x4F\x19\x03\x03\x0F\x0C\x0E\x05\x06\x10\x00\x3E\x7B\x2B\x29\x3F\x3C\x39\x73\x58\x7E\x70\x06\x3F\x39\x30\x3A\x3D\x38\x68\x2A\x21\x3F\x25\x28\x31\x63\x26\x28\x2A\x22\x37\x65\x1B\x15\x1C\x59\x0C\x1A\x0F\x09\x13\x01\x04\x02\x56\x04\x11\x03\x0F\x19\x09\x05\x4E\x1B\x05\x00\x07\x10\x40"
"\x4C\x46\x03\x0B\x45\x34\x34\x2C\x79\x2B\x31\x2C\x31\x27\x34\x70\x25\x3E\x32\x74\x00\x19\x09\x42\x69\x6E\x3A\x22\x39\x2B\x2F\x60\x31\x34\x28\x29\x35\x0E\x1E\x1C\x57\x74\x75\x2F\x29\x37\x23\x50\x44\x56\x5A\x54\x33\x23\x39\x3B\x3D\x4E\x3D\x39\x23\x68\x4E\x40\x32\x0F\x00\x0A\x45\x33\x35\x78\x2E\x37\x2B"
"\x34\x7D\x2B\x3C\x25\x23\x76\x1A\x3D\x36\x38\x24\x3B\x26\x28\x3B\x6C\x2C\x21\x20\x2F\x34\x28\x33\x7F\x65\x03\x14\x0D\x0B\x5E\x0C\x19\x09\x06\x1A\x1E\x16\x05\x57\x15\x1B\x0E\x4B\x27\x07\x0B\x2B\x1E\x04\x14\x06\x40\x12\x1F\x09\x07\x6F\x7A\x7B\x3A\x38\x3D\x34\x72\x57\x7F\x73\x19\x3F\x25\x23\x35\x39\x26"
"\x6B\x2C\x3B\x27\x39\x29\x3F\x31\x63\x36\x28\x27\x67\x13\x2C\x14\x1F\x17\x0E\x0D\x5F\x29\x0D\x16\x12\x04\x14\x56\x5F\x1B\x05\x1E\x02\x07\x07\x0F\x03\x4C\x18\x12\x07\x01\x15\x03\x14\x4D\x45\x71\x7B\x17\x1C\x13\x7F\x3D\x2D\x22\x20\x7E\x5B\x7B\x77\x06\x30\x23\x25\x3B\x3D\x2F\x23\x20\x6D\x23\x33\x30\x32"
"\x7D\x67\x36\x20\x09\x0F\x17\x0B\x1B\x5F\x1A\x14\x1E\x16\x03\x51\x10\x05\x1B\x18\x4A\x09\x09\x0A\x05\x1A\x1C\x43\x68\x69\x21\x27\x32\x22\x36\x32\x1B\x09\x1C\x0A\x54\x72\x7C\x1E\x3A\x36\x33\x3A\x76\x00\x3D\x3B\x2E\x24\x3F\x3A\x6E\x1A\x3C\x29\x23\x37\x25\x6D\x66\x22\x2A\x24\x18\x17\x1D\x59\x3C\x16\x08"
"\x31\x1D\x10\x1B\x14\x04\x5B\x54\x06\x0F\x1F\x48\x1C\x1E\x4F\x0E\x0C\x01\x08\x15\x11\x15\x47\x4C\x08\x3B\x35\x2D\x38\x32\x72\x3E\x3C\x31\x38\x25\x21\x7F\x79\x5E\x78\x6A\x1F\x20\x20\x3D\x6F\x3C\x3F\x2D\x24\x32\x20\x2B\x67\x27\x24\x14\x5B\x1A\x1C\x5E\x0D\x19\x14\x1C\x00\x04\x10\x1A\x1B\x11\x11\x4A\x02"
"\x06\x49\x1D\x0A\x0F\x02\x0C\x07\x13\x41\x4E\x4A\x49\x0C\x34\x28\x2C\x38\x32\x33\x75\x7D\x7F\x73\x31\x3F\x32\x77\x35\x39\x26\x6B\x21\x3D\x3D\x45\x6C\x6D\x36\x34\x25\x20\x2D\x34\x64\x29\x1F\x0F\x58\x00\x11\x0A\x5C\x0F\x17\x00\x04\x1E\x04\x12\x54\x0C\x05\x1E\x1A\x49\x1E\x1D\x09\x0B\x07\x11\x05\x0F\x05"
"\x02\x17\x45\x2B\x2E\x31\x3A\x35\x33\x25\x73\x58\x59\x11\x1D\x02\x12\x06\x1B\x0B\x1F\x01\x1F\x0B\x1C\x6C\x19\x0D\x63\x01\x61\x05\x0B\x01\x04\x34\x5B\x31\x37\x2D\x2B\x3D\x31\x3E\x79\x5D\x51\x24\x12\x07\x10\x1E\x4B\x1C\x01\x07\x1C\x4C\x3D\x21\x43\x48\x2A\x03\x02\x14\x45\x37\x22\x78\x3F\x37\x33\x39\x2E"
"\x7B\x73\x7D\x71\x30\x36\x27\x21\x2F\x39\x64\x69\x25\x2A\x29\x3D\x31\x63\x24\x20\x32\x26\x6A\x4F\x57\x5B\x31\x17\x53\x0F\x10\x1C\x11\x16\x50\x04\x06\x10\x06\x14\x0E\x0E\x52\x49\x1C\x1A\x02\x4D\x11\x06\x14\x14\x16\x49\x01\x1D\x3F\x7B\x3E\x2B\x31\x32\x7C\x29\x3A\x36\x70\x06\x3F\x39\x30\x3A\x3D\x38\x68"
"\x78\x7F\x6F\x05\x1E\x0D\x63\x6D\x61\x34\x22\x34\x24\x13\x09\x0B\x59\x1F\x11\x18\x77\x52\x53\x05\x01\x12\x16\x00\x10\x19\x4B\x1F\x00\x1A\x07\x03\x18\x16\x43\x0C\x0E\x15\x0E\x0A\x02\x7A\x3A\x28\x29\x2D\x7F\x33\x2F\x72\x35\x39\x3D\x33\x24\x7A\x5F\x67\x6B\x3B\x2F\x2D\x60\x28\x24\x31\x2E\x6F\x33\x23\x34"
"\x30\x2A\x08\x1E\x58\x09\x11\x16\x12\x09\x52\x15\x1F\x03\x56\x1B\x1D\x12\x02\x1F\x48\x0D\x0F\x02\x0D\x0A\x07\x4D\x6A\x6B\x2A\x22\x23\x24\x16\x7B\x16\x16\x0A\x1A\x56\x08\x21\x36\x70\x3E\x30\x31\x3D\x36\x23\x2A\x24\x69\x03\x26\x2F\x3F\x2D\x30\x2F\x27\x32\x67\x29\x20\x1E\x12\x19\x59\x11\x11\x10\x04\x5C"
"\x53\x31\x51\x12\x1E\x13\x1C\x1E\x0A\x04\x49\x02\x06\x0F\x08\x0C\x10\x05\x41\x12\x0E\x01\x01\x7A\x2F\x37\x79\x27\x30\x29\x2F\x58\x1E\x39\x32\x24\x38\x27\x3A\x2C\x3F\x68\x28\x2D\x2C\x23\x38\x2C\x37\x60\x33\x23\x26\x27\x31\x13\x0D\x19\x0D\x1B\x0C\x5C\x1C\x07\x07\x1F\x1C\x17\x03\x1D\x16\x0B\x07\x04\x10"
"\x4E\x0E\x0A\x19\x07\x11\x40\x00\x46\x04\x08\x00\x3B\x35\x78\x30\x30\x2C\x28\x3C\x3E\x3F\x7E"
);

MAN(defender, "Microsoft Defender Deep Dive",
"\x0D\x13\x19\x0D\x7E\x16\x08\x7D\x11\x1C\x06\x14\x04\x04\x5E\x02\x23\x25\x2C\x26\x39\x3C\x6C\x1E\x27\x20\x35\x33\x2F\x33\x3D\x65\x52\x0F\x10\x10\x0D\x5F\x0C\x0F\x1D\x14\x02\x10\x1B\x4D\x54\x52\x1D\x02\x06\x0D\x01\x18\x1F\x40\x11\x06\x03\x14\x14\x0E\x10\x1C\x7D\x72\x78\x3B\x2B\x31\x38\x31\x37\x20\x6A"
"\x5B\x7B\x77\x02\x3C\x38\x3E\x3B\x69\x68\x6F\x38\x25\x30\x26\x21\x35\x66\x37\x36\x2A\x0E\x1E\x1B\x0D\x17\x10\x12\x5D\x5A\x12\x1E\x05\x1F\x01\x1D\x07\x1F\x18\x41\x63\x43\x4F\x2D\x0E\x01\x0C\x15\x0F\x12\x47\x14\x17\x35\x2F\x3D\x3A\x2A\x36\x33\x33\x72\x7B\x07\x38\x38\x33\x3B\x22\x39\x6B\x00\x2C\x22\x23"
"\x23\x61\x62\x27\x39\x2F\x27\x2A\x2D\x26\x5A\x17\x17\x1A\x15\x56\x76\x50\x52\x35\x19\x03\x13\x00\x15\x19\x06\x4B\x4E\x49\x00\x0A\x18\x1A\x0D\x11\x0B\x41\x16\x15\x0B\x11\x3F\x38\x2C\x30\x31\x31\x56\x70\x72\x12\x20\x21\x76\x71\x74\x37\x38\x24\x3F\x3A\x2B\x3D\x6C\x2E\x2D\x2D\x34\x33\x29\x2B\x64\x6D\x29"
"\x16\x19\x0B\x0A\x2C\x1F\x0F\x17\x16\x1E\x58\x7C\x5A\x54\x31\x0F\x1D\x01\x0A\x0B\x4F\x1F\x08\x01\x16\x12\x08\x12\x1E\x44\x4D\x39\x34\x2A\x3C\x7E\x36\x2F\x32\x3E\x32\x24\x38\x39\x39\x78\x75\x1E\x1B\x05\x65\x6E\x1C\x29\x2E\x37\x31\x25\x61\x04\x28\x2B\x31\x53\x71\x55\x59\x3A\x1A\x0A\x14\x11\x16\x50\x01"
"\x13\x05\x12\x1A\x18\x06\x09\x07\x0D\x0A\x4C\x4B\x42\x0B\x05\x00\x0A\x13\x0C\x6F\x50\x08\x1B\x18\x10\x7F\x08\x04\x02\x16\x03\x5B\x7B\x77\x05\x20\x23\x28\x23\x69\x3D\x2C\x2D\x23\x78\x63\x23\x2E\x2B\x2A\x2B\x2B\x5A\x16\x19\x15\x09\x1E\x0E\x18\x52\x1F\x1F\x12\x17\x03\x1D\x1A\x04\x18\x48\x44\x4E\x1D\x19"
"\x03\x42\x0A\x14\x41\x14\x02\x03\x10\x36\x3A\x2A\x35\x27\x71\x56\x70\x72\x15\x25\x3D\x3A\x77\x27\x36\x2B\x25\x72\x69\x2B\x39\x29\x3F\x3B\x37\x28\x28\x28\x20\x64\x68\x5A\x08\x14\x16\x09\x5F\x1E\x08\x06\x53\x04\x19\x19\x05\x1B\x00\x0D\x03\x53\x49\x1C\x1A\x02\x4D\x0F\x0C\x0E\x15\x0E\x0B\x1D\x4B\x50\x76"
"\x78\x1A\x2B\x2C\x28\x32\x3F\x73\x23\x32\x37\x39\x6E\x75\x3A\x22\x2B\x22\x6E\x2E\x6C\x2B\x2D\x2F\x24\x24\x34\x68\x20\x37\x13\x0D\x1D\x57\x74\x52\x5C\x30\x1B\x10\x02\x1E\x05\x18\x12\x01\x4A\x2F\x0D\x0F\x0B\x01\x08\x08\x10\x43\x2F\x07\x00\x0B\x0D\x0B\x3F\x7B\x2B\x3A\x3F\x31\x66\x7D\x20\x36\x32\x3E\x39"
"\x23\x27\x75\x23\x25\x3C\x26\x6E\x2E\x6C\x3D\x30\x26\x6D\x16\x2F\x29\x20\x2A\x0D\x08\x58\x0A\x1D\x1E\x12\x13\x17\x01\x7A\x51\x56\x03\x1C\x14\x1E\x4B\x0B\x08\x00\x4F\x1E\x08\x0F\x0C\x16\x04\x46\x14\x10\x10\x38\x39\x37\x2B\x30\x7F\x31\x3C\x3E\x24\x31\x23\x33\x79\x74\x02\x23\x25\x2C\x26\x39\x3C\x6C\x1E"
"\x27\x20\x35\x33\x2F\x33\x3D\x65\x44\x5B\x2E\x10\x0C\x0A\x0F\x5D\x54\x53\x04\x19\x04\x12\x15\x01\x60\x4B\x48\x19\x1C\x00\x18\x08\x01\x17\x09\x0E\x08\x47\x5A\x45\x09\x38\x39\x37\x7E\x30\x2C\x29\x3B\x3C\x3E\x22\x78\x5D\x79\x75\x1E\x23\x21\x3A\x6E\x3F\x3E\x22\x25\x31\x21\x2C\x7C\x67\x63\x21\x1F\x1D\x1D"
"\x17\x1A\x1A\x0E\x50\x03\x06\x19\x12\x1D\x5A\x07\x16\x0B\x05\x4F\x45\x4E\x48\x08\x08\x04\x06\x0E\x05\x03\x15\x49\x03\x2F\x37\x34\x74\x2D\x3C\x3D\x33\x75\x7F\x5A\x71\x76\x70\x30\x30\x2C\x2E\x26\x2D\x2B\x3D\x61\x22\x24\x25\x2C\x28\x28\x22\x69\x36\x19\x1A\x16\x5E\x52\x5F\x5B\x19\x17\x15\x15\x1F\x12\x12"
"\x06\x58\x1E\x03\x1A\x0C\x0F\x1B\x41\x05\x0B\x10\x14\x0E\x14\x1E\x43\x4B\x50\x51\x0A\x1C\x1F\x13\x71\x09\x1B\x1E\x15\x71\x06\x05\x1B\x01\x0F\x08\x1C\x00\x01\x01\x46\x60\x62\x0C\x2E\x61\x24\x3E\x64\x21\x1F\x1D\x19\x0C\x12\x0B\x52\x5D\x34\x1A\x1C\x14\x05\x57\x15\x07\x0F\x4B\x1B\x0A\x0F\x01\x02\x08\x06"
"\x43\x01\x12\x46\x13\x0C\x00\x23\x7B\x37\x29\x3B\x31\x73\x39\x3D\x24\x3E\x3D\x39\x36\x30\x7B\x40\x66\x68\x6E\x1A\x2E\x21\x3D\x27\x31\x60\x11\x34\x28\x30\x20\x19\x0F\x11\x16\x10\x58\x5C\x55\x1D\x1D\x59\x51\x05\x03\x1B\x05\x19\x4B\x05\x08\x02\x18\x0D\x1F\x07\x43\x06\x13\x09\x0A\x44\x01\x33\x28\x39\x3B"
"\x32\x36\x32\x3A\x72\x17\x35\x37\x33\x39\x30\x30\x38\x6B\x65\x69\x22\x2A\x2D\x3B\x27\x49\x60\x61\x2F\x33\x64\x0A\x34\x55\x72\x73\x3D\x33\x33\x28\x36\x53\x20\x23\x39\x23\x31\x36\x3E\x22\x27\x27\x4E\x49\x4C\x3E\x23\x2E\x30\x2D\x23\x34\x6E\x48\x7A\x18\x34\x36\x2B\x3B\x71\x39\x37\x3F\x39\x27\x33\x25\x31"
"\x31\x6A\x3B\x3A\x26\x3A\x2A\x2F\x39\x2B\x2C\x2E\x7B\x66\x2E\x2A\x36\x0E\x1A\x16\x0D\x5E\x1C\x10\x12\x07\x17\x50\x12\x1E\x12\x17\x1E\x19\x4B\x07\x0F\x4E\x01\x09\x1A\x42\x17\x08\x13\x03\x06\x10\x16\x74\x51\x75\x79\x1F\x2A\x28\x32\x3F\x32\x24\x38\x35\x77\x27\x34\x27\x3B\x24\x2C\x6E\x3C\x39\x2F\x2F\x2A"
"\x33\x32\x2F\x28\x2A\x7F\x5A\x08\x0D\x0A\x0E\x16\x1F\x14\x1D\x06\x03\x51\x10\x1E\x18\x10\x19\x4B\x09\x1B\x0B\x4F\x1F\x08\x0C\x17\x40\x15\x09\x47\x29\x0C\x39\x29\x37\x2A\x31\x39\x28\x57\x72\x73\x36\x3E\x24\x77\x35\x3B\x2B\x27\x31\x3A\x27\x3C\x62\x6D\x00\x2C\x34\x29\x66\x34\x2C\x2A\x0F\x17\x1C\x59\x0D"
"\x0B\x1D\x04\x52\x1C\x1E\x5F\x7C\x7D\x31\x2D\x29\x27\x3D\x3A\x27\x20\x22\x3E\x68\x4E\x40\x20\x02\x03\x44\x00\x22\x38\x34\x2C\x2D\x36\x33\x33\x21\x73\x3F\x3F\x3A\x2E\x74\x33\x25\x39\x68\x2F\x27\x23\x29\x3E\x6D\x25\x2F\x2D\x22\x22\x36\x36\x5A\x02\x17\x0C\x5E\x1E\x0E\x18\x52\x42\x40\x41\x53\x57\x07\x00"
"\x18\x0E\x48\x08\x0C\x00\x19\x19\x68\x43\x40\x49\x03\x49\x03\x4B\x7A\x37\x39\x2B\x39\x3A\x7C\x39\x37\x25\x70\x32\x37\x34\x3C\x30\x39\x6B\x27\x3B\x6E\x20\x20\x29\x62\x30\x2F\x27\x32\x30\x25\x37\x1F\x52\x56\x59\x3F\x0B\x08\x1C\x11\x18\x15\x03\x05\x57\x11\x0D\x1A\x07\x07\x00\x1A\x4F\x09\x15\x01\x0F\x15"
"\x12\x0F\x08\x0A\x16\x74\x51\x75\x79\x0A\x37\x35\x2E\x72\x23\x22\x3E\x31\x25\x35\x38\x70\x6B\x6F\x2D\x2B\x29\x29\x23\x26\x26\x32\x6C\x23\x3F\x27\x29\x0F\x08\x11\x16\x10\x0C\x5B\x5D\x1D\x03\x15\x1F\x05\x57\x00\x1D\x0F\x4B\x18\x08\x09\x0A\x42\x67\x68\x20\x2F\x2F\x32\x35\x2B\x29\x16\x1E\x1C\x79\x18\x10"
"\x10\x19\x17\x01\x70\x10\x15\x14\x11\x06\x19\x41\x65\x69\x1E\x2E\x3E\x39\x62\x2C\x26\x61\x34\x26\x2A\x36\x15\x16\x0F\x18\x0C\x1A\x5C\x0D\x00\x1C\x04\x14\x15\x03\x1D\x1A\x04\x51\x48\x06\x00\x03\x15\x4D\x03\x0F\x0C\x0E\x11\x02\x00\x45\x3B\x2B\x28\x2A\x7E\x32\x3D\x24\x72\x24\x22\x38\x22\x32\x74\x21\x25"
"\x6B\x31\x26\x3B\x3D\x46\x6D\x62\x07\x2F\x22\x33\x2A\x21\x2B\x0E\x08\x58\x1C\x0A\x1C\x52\x5D\x21\x16\x15\x51\x1B\x16\x1A\x00\x0B\x07\x45\x1A\x0B\x0C\x19\x1F\x0B\x17\x19\x4F\x6C\x6D\x21\x3D\x0A\x17\x17\x10\x0A\x7F\x0C\x0F\x1D\x07\x15\x12\x02\x1E\x1B\x1B\x40\x66\x68\x1A\x37\x3C\x38\x28\x2F\x6E\x2C\x24"
"\x30\x22\x28\x65\x17\x12\x0C\x10\x19\x1E\x08\x14\x1D\x1D\x03\x51\x5E\x33\x31\x25\x46\x4B\x29\x3A\x22\x3D\x40\x4D\x21\x25\x27\x4F\x48\x49\x4D\x45\x3C\x34\x2A\x79\x3F\x2F\x2C\x2E\x7C\x73\x14\x34\x30\x36\x21\x39\x3E\x38\x68\x28\x3C\x2A\x46\x6D\x62\x24\x2F\x2E\x22\x7C\x64\x31\x12\x1E\x0A\x1C\x5E\x16\x0F"
"\x5D\x13\x53\x37\x24\x3F\x57\x01\x1B\x0E\x0E\x1A\x49\x2F\x1F\x1C\x4D\x44\x43\x02\x13\x09\x10\x17\x00\x28\x7B\x3B\x36\x30\x2B\x2E\x32\x3E\x73\x6E\x71\x13\x2F\x24\x39\x25\x22\x3C\x69\x3E\x3D\x23\x39\x27\x20\x34\x28\x29\x29\x6A\x4F\x70\x3D\x31\x2B\x3B\x28\x3D\x31\x3E\x79\x5D\x51\x22\x1F\x06\x10\x0F\x4B"
"\x18\x1B\x01\x09\x05\x01\x07\x10\x5A\x41\x22\x08\x09\x04\x33\x35\x74\x79\x0E\x2D\x35\x2B\x33\x27\x35\x7D\x76\x07\x21\x37\x26\x22\x2B\x67\x6E\x1D\x39\x21\x27\x30\x60\x31\x23\x35\x64\x24\x0A\x0B\x58\x18\x10\x1B\x5C\x0D\x1D\x01\x04\x5F\x7C\x5A\x54\x21\x02\x02\x1B\x49\x1E\x1D\x03\x0A\x10\x02\x0D\x5B\x46"
"\x40\x02\x0C\x28\x3E\x2F\x38\x32\x33\x7B\x7D\x31\x32\x24\x34\x31\x38\x26\x2C\x6A\x3F\x27\x26\x22\x3C\x6C\x65\x2D\x2D\x6F\x2E\x20\x21\x68\x65\x1B\x17\x14\x16\x09\x50\x1E\x11\x1D\x10\x1B\x51\x06\x18\x06\x01\x43\x45\x62\x63\x20\x20\x38\x24\x24\x2A\x23\x20\x32\x2E\x2B\x2B\x09\x7B\x7E\x79\x0E\x1A\x0E\x1B"
"\x1D\x01\x1D\x10\x18\x14\x11\x5F\x67\x6B\x0C\x2C\x28\x2A\x22\x29\x27\x31\x60\x33\x33\x29\x37\x65\x0B\x0E\x11\x1C\x0A\x13\x05\x46\x52\x1B\x15\x10\x00\x0E\x54\x06\x09\x0A\x06\x1A\x4E\x0C\x0D\x03\x42\x01\x05\x41\x15\x04\x0C\x00\x3E\x2E\x34\x3C\x3A\x7F\x35\x33\x72\x07\x31\x22\x3D\x77\x07\x36\x22\x2E\x2C"
"\x3C\x22\x2A\x3E\x47\x62\x63\x68\x15\x27\x34\x2F\x65\x29\x18\x10\x1C\x1A\x0A\x10\x18\x00\x53\x4E\x51\x3B\x1E\x17\x07\x05\x18\x07\x0F\x1A\x4F\x52\x4D\x35\x0A\x0E\x05\x09\x10\x17\x45\x64\x7B\x0F\x30\x30\x3B\x33\x2A\x21\x73\x14\x34\x30\x32\x3A\x31\x2F\x39\x61\x67\x44\x62\x6C\x04\x24\x63\x04\x24\x20\x22"
"\x2A\x21\x1F\x09\x58\x1F\x12\x1E\x1B\x0E\x52\x12\x50\x17\x1F\x1B\x11\x55\x13\x04\x1D\x49\x1A\x1D\x19\x1E\x16\x4F\x40\x12\x13\x05\x09\x0C\x2E\x7B\x31\x2D\x7E\x2B\x33\x7D\x1F\x3A\x33\x23\x39\x24\x3B\x33\x3E\x6B\x2E\x26\x3C\x45\x6C\x6D\x30\x26\x6D\x20\x28\x26\x28\x3C\x09\x12\x0B\x59\x17\x11\x0F\x09\x17"
"\x12\x14\x51\x19\x11\x54\x1F\x1F\x18\x1C\x49\x0B\x17\x0F\x01\x17\x07\x09\x0F\x01\x47\x0D\x11\x74\x51\x52\x09\x1F\x16\x0E\x14\x1C\x14\x70\x06\x1F\x03\x1C\x75\x05\x1F\x00\x0C\x1C\x6F\x0D\x03\x16\x0A\x16\x08\x14\x12\x17\x4F\x57\x5B\x2C\x11\x17\x0D\x18\x50\x02\x12\x02\x05\x0F\x57\x15\x1B\x1E\x02\x1E\x00"
"\x1C\x1A\x1F\x4D\x4A\x34\x09\x0F\x02\x08\x13\x16\x7A\x6A\x69\x79\x3B\x2D\x3D\x74\x72\x21\x35\x36\x3F\x24\x20\x30\x38\x38\x68\x3E\x27\x3B\x24\x6D\x15\x2A\x2E\x25\x29\x30\x37\x65\x29\x1E\x1B\x0C\x0C\x16\x08\x04\x49\x79\x50\x51\x32\x12\x12\x10\x04\x0F\x0D\x1B\x49\x1C\x4C\x1F\x07\x02\x0C\x4C\x12\x0E\x09"
"\x00\x7A\x2B\x2A\x36\x2A\x3A\x3F\x29\x3B\x3C\x3E\x71\x26\x36\x21\x26\x2F\x38\x68\x28\x3B\x3B\x23\x20\x23\x37\x29\x22\x27\x2B\x28\x3C\x54\x5B\x2A\x0C\x10\x5F\x0F\x1E\x13\x1D\x03\x51\x01\x1E\x00\x1D\x60\x4B\x48\x06\x00\x03\x15\x4D\x0D\x0D\x05\x41\x27\x31\x44\x04\x2E\x7B\x39\x79\x2A\x36\x31\x38\x7C\x59"
"\x5A\x18\x10\x77\x1D\x1B\x0C\x0E\x0B\x1D\x0B\x0B\x46\x7C\x6B\x63\x12\x34\x28\x67\x25\x65\x1C\x0E\x14\x15\x5E\x0C\x1F\x1C\x1C\x5F\x50\x05\x1E\x12\x1A\x55\x0B\x05\x48\x06\x08\x09\x00\x04\x0C\x06\x40\x12\x05\x06\x0A\x4B\x50\x69\x71\x79\x0C\x3A\x31\x32\x24\x36\x70\x22\x23\x24\x24\x3C\x29\x22\x27\x3C\x3D"
"\x6F\x3F\x39\x23\x31\x34\x34\x36\x67\x21\x2B\x0E\x09\x11\x1C\x0D\x5F\x54\x09\x1A\x1A\x03\x51\x06\x05\x1B\x12\x18\x0A\x05\x53\x4E\x48\x1F\x19\x03\x11\x14\x14\x16\x4A\x05\x15\x2A\x28\x7F\x70\x70\x55\x6F\x74\x72\x10\x38\x34\x35\x3C\x74\x34\x29\x28\x27\x3C\x20\x3B\x3F\x6D\x23\x2D\x24\x61\x23\x29\x25\x27"
"\x16\x1E\x58\x0D\x09\x10\x51\x0E\x06\x16\x00\x51\x00\x12\x06\x1C\x0C\x02\x0B\x08\x1A\x06\x03\x03\x4C\x69\x54\x48\x46\x35\x01\x16\x3F\x2F\x78\x3B\x2C\x30\x2B\x2E\x37\x21\x70\x22\x33\x23\x20\x3C\x24\x2C\x3B\x69\x2F\x21\x28\x6D\x21\x2B\x25\x22\x2D\x67\x37\x24\x0C\x1E\x1C\x59\x0E\x1E\x0F\x0E\x05\x1C\x02"
"\x15\x05\x59\x7E\x40\x43\x4B\x3F\x06\x1C\x1C\x18\x4D\x01\x02\x13\x04\x5C\x47\x16\x00\x29\x3E\x2C\x79\x2A\x37\x35\x2E\x72\x03\x13\x71\x7E\x3C\x31\x30\x3A\x6B\x2E\x20\x22\x2A\x3F\x64\x6E\x63\x34\x29\x23\x29\x64\x37\x1F\x08\x0C\x16\x0C\x1A\x5C\x19\x13\x07\x11\x51\x10\x05\x1B\x18\x60\x4B\x48\x49\x0D\x03"
"\x09\x0C\x0C\x43\x02\x00\x05\x0C\x11\x15\x29\x7B\x37\x37\x32\x26\x72"
);

MAN(wsl, "Windows Subsystem for Linux (WSL)",
"\x0D\x13\x19\x0D\x7E\x08\x0F\x11\x72\x1A\x03\x5B\x04\x22\x3A\x75\x2B\x6B\x3A\x2C\x2F\x23\x6C\x01\x2B\x2D\x35\x39\x66\x23\x2D\x36\x0E\x09\x11\x1B\x0B\x0B\x15\x12\x1C\x53\x58\x24\x14\x02\x1A\x01\x1F\x47\x48\x2D\x0B\x0D\x05\x0C\x0C\x4F\x40\x2A\x07\x0B\x0D\x49\x7A\x34\x28\x3C\x30\x0C\x09\x0E\x17\x7F\x70"
"\x17\x33\x33\x3B\x27\x2B\x67\x42\x08\x22\x3F\x25\x23\x27\x6D\x6E\x6F\x6F\x67\x2D\x2B\x09\x12\x1C\x1C\x5E\x28\x15\x13\x16\x1C\x07\x02\x56\x5A\x54\x01\x0F\x19\x05\x00\x00\x0E\x00\x41\x42\x17\x0F\x0E\x0A\x14\x48\x45\x3F\x2D\x3D\x37\x7E\x18\x09\x14\x72\x32\x20\x21\x25\x77\x35\x3B\x2E\x6B\x0B\x1C\x0A\x0E"
"\x46\x0A\x12\x16\x60\x36\x29\x35\x2F\x6B\x5A\x35\x17\x59\x1A\x0A\x1D\x11\x52\x11\x1F\x1E\x02\x5B\x54\x1B\x05\x4B\x3E\x24\x4E\x1C\x09\x19\x17\x13\x4E\x6B\x6C\x2E\x2A\x36\x0E\x1A\x14\x15\x7E\x77\x33\x33\x37\x73\x33\x3E\x3B\x3A\x35\x3B\x2E\x62\x42\x64\x6E\x00\x3C\x28\x2C\x63\x14\x24\x34\x2A\x2D\x2B\x1B"
"\x17\x58\x18\x0D\x5F\x1D\x19\x1F\x1A\x1E\x51\x17\x19\x10\x55\x18\x1E\x06\x53\x4E\x4F\x1B\x1E\x0E\x43\x4D\x4C\x0F\x09\x17\x11\x3B\x37\x34\x53\x73\x7F\x15\x29\x72\x36\x3E\x30\x34\x3B\x31\x26\x6A\x1C\x1B\x05\x7C\x63\x6C\x39\x2A\x26\x60\x17\x2F\x35\x30\x30\x1B\x17\x58\x34\x1F\x1C\x14\x14\x1C\x16\x50\x21"
"\x1A\x16\x00\x13\x05\x19\x05\x45\x4E\x0E\x02\x09\x42\x0A\x0E\x12\x12\x06\x08\x09\x29\x7B\x0D\x3B\x2B\x31\x28\x28\x72\x31\x29\x5B\x76\x77\x30\x30\x2C\x2A\x3D\x25\x3A\x61\x6C\x1F\x27\x30\x34\x20\x34\x33\x68\x65\x0E\x13\x1D\x17\x5E\x19\x15\x13\x1B\x00\x18\x51\x02\x1F\x11\x55\x26\x02\x06\x1C\x16\x4F\x19"
"\x1E\x07\x11\x40\x12\x03\x13\x11\x15\x74\x51\x75\x79\x09\x36\x32\x39\x3D\x24\x23\x71\x67\x66\x74\x67\x78\x03\x7A\x62\x6E\x2A\x3A\x28\x2C\x63\x24\x2E\x23\x34\x64\x62\x0D\x08\x14\x59\x53\x52\x15\x13\x01\x07\x11\x1D\x1A\x50\x54\x02\x03\x1F\x00\x06\x1B\x1B\x4C\x0C\x06\x0E\x09\x0F\x46\x01\x0B\x17\x7A\x35"
"\x3D\x2E\x54\x7F\x7C\x34\x3C\x20\x24\x30\x3A\x3B\x27\x75\x62\x1C\x1B\x05\x6E\x26\x22\x6D\x11\x37\x2F\x33\x23\x6E\x6A\x4F\x70\x36\x39\x37\x3F\x38\x35\x33\x35\x53\x34\x38\x25\x23\x26\x3A\x39\x61\x45\x49\x19\x1C\x00\x4D\x4F\x4E\x0C\x08\x15\x13\x44\x48\x77\x34\x36\x35\x37\x31\x39\x7D\x72\x73\x70\x71\x76"
"\x77\x74\x14\x3C\x2A\x21\x25\x2F\x2D\x20\x28\x62\x27\x29\x32\x32\x35\x2B\x36\x70\x56\x58\x0E\x0D\x13\x5C\x50\x5F\x1A\x1E\x02\x02\x16\x18\x19\x4A\x46\x0C\x49\x3B\x0D\x19\x03\x16\x16\x40\x41\x46\x47\x2D\x0B\x29\x2F\x39\x35\x32\x7F\x3D\x7D\x21\x23\x35\x32\x3F\x31\x3D\x36\x6A\x2F\x21\x3A\x3A\x3D\x23\x47"
"\x6F\x63\x37\x32\x2A\x67\x69\x68\x16\x12\x0B\x0D\x5E\x52\x51\x0B\x17\x01\x12\x1E\x05\x12\x54\x55\x4A\x4B\x48\x49\x4E\x26\x02\x1E\x16\x02\x0C\x0D\x03\x03\x44\x01\x33\x28\x2C\x2B\x31\x2C\x7C\x76\x72\x04\x03\x1D\x76\x21\x31\x27\x39\x22\x27\x27\x6E\x67\x7D\x6D\x2D\x31\x60\x73\x6F\x4D\x69\x65\x0D\x08\x14"
"\x59\x53\x52\x0F\x18\x06\x5E\x06\x14\x04\x04\x1D\x1A\x04\x4B\x54\x07\x0F\x02\x09\x53\x42\x51\x40\x41\x46\x47\x37\x12\x33\x2F\x3B\x31\x7E\x3E\x7C\x39\x3B\x20\x24\x23\x39\x77\x20\x3A\x6A\x1C\x1B\x05\x7C\x45\x61\x6D\x35\x30\x2C\x61\x6B\x6A\x37\x20\x0E\x56\x1C\x1C\x18\x1E\x09\x11\x06\x53\x4C\x1F\x17\x1A"
"\x11\x4B\x4A\x4B\x48\x2D\x0B\x09\x0D\x18\x0E\x17\x40\x05\x0F\x14\x10\x17\x35\x51\x75\x79\x29\x2C\x30\x7D\x7F\x7E\x25\x3F\x24\x32\x33\x3C\x39\x3F\x2D\x3B\x6E\x73\x22\x2C\x2F\x26\x7E\x61\x66\x67\x64\x01\x1F\x17\x1D\x0D\x1B\x5F\x1D\x5D\x16\x1A\x03\x05\x04\x18\x54\x5D\x03\x1F\x1B\x49\x08\x06\x00\x08\x11"
"\x43\x07\x0E\x46\x13\x0B\x0A\x7B\x72\x52\x74\x7E\x28\x2F\x31\x72\x7E\x7D\x24\x26\x33\x35\x21\x2F\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x2F\x0B\x1C\x18\x0A\x1A\x5C\x2A\x21\x3F\x50\x18\x02\x04\x11\x19\x0C\x61\x45\x49\x19\x1C\x00\x4D\x4F\x4E\x13\x15\x07\x13\x11\x16\x7A\x74\x78\x2E"
"\x2D\x33\x7C\x70\x7F\x25\x35\x23\x25\x3E\x3B\x3B\x6A\x6B\x68\x01\x2B\x2E\x20\x39\x2A\x63\x23\x29\x23\x24\x2F\x4F\x57\x5B\x0F\x0A\x12\x5F\x51\x50\x01\x1B\x05\x05\x12\x18\x03\x1B\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x34\x10\x0A\x2A\x7B\x39\x35\x32\x7F\x0B\x0E\x1E\x73\x39\x3F\x25\x23\x35"
"\x3B\x29\x2E\x3B\x43\x44\x1D\x19\x03\x0C\x0A\x0E\x06\x66\x0B\x0D\x0B\x2F\x23\x72\x54\x5E\x30\x0C\x18\x1C\x53\x09\x1E\x03\x05\x54\x11\x03\x18\x1C\x1B\x01\x4F\x0A\x1F\x0D\x0E\x40\x15\x0E\x02\x44\x36\x2E\x3A\x2A\x2D\x7E\x32\x39\x33\x27\x7F\x70\x3E\x24\x77\x20\x2C\x3A\x2E\x68\x6E\x39\x3C\x20\x6A\x62\x2A"
"\x2E\x61\x27\x67\x30\x20\x08\x16\x11\x17\x1F\x13\x52\x77\x5F\x53\x22\x04\x18\x57\x1B\x1B\x0F\x4B\x0B\x06\x03\x02\x0D\x03\x06\x59\x40\x41\x11\x14\x08\x45\x77\x3E\x78\x65\x3D\x30\x31\x30\x33\x3D\x34\x6F\x7A\x77\x31\x7B\x2D\x65\x68\x3E\x3D\x23\x6C\x60\x27\x63\x2C\x32\x66\x6A\x28\x24\x70\x56\x58\x2E\x17"
"\x11\x18\x12\x05\x00\x50\x10\x06\x07\x07\x55\x09\x0A\x06\x49\x0D\x0E\x00\x01\x42\x34\x33\x2D\x5C\x47\x44\x12\x29\x37\x76\x3C\x26\x3A\x7C\x3A\x20\x36\x20\x71\x26\x36\x20\x21\x2F\x39\x26\x69\x28\x26\x20\x28\x48\x49\x06\x08\x0A\x02\x17\x4F\x57\x5B\x34\x10\x10\x0A\x04\x5D\x14\x1A\x1C\x14\x05\x57\x18\x1C"
"\x1C\x0E\x48\x00\x00\x1C\x05\x09\x07\x43\x14\x09\x03\x47\x00\x0C\x29\x2F\x2A\x36\x79\x2C\x7C\x2B\x3B\x21\x24\x24\x37\x3B\x74\x31\x23\x38\x23\x72\x6E\x2D\x3E\x22\x35\x30\x25\x61\x32\x2F\x21\x28\x5A\x1D\x0A\x16\x13\x75\x5C\x5D\x25\x1A\x1E\x15\x19\x00\x07\x55\x0B\x1F\x48\x35\x32\x18\x1F\x01\x46\x3F\x5C"
"\x05\x0F\x14\x10\x17\x35\x65\x78\x36\x2C\x7F\x00\x01\x25\x20\x3C\x7F\x3A\x38\x37\x34\x26\x23\x27\x3A\x3A\x13\x70\x29\x2B\x30\x34\x33\x29\x79\x64\x6D\x3F\x03\x08\x15\x11\x0D\x19\x0F\x5B\x5D\x7A\x5C\x56\x24\x1C\x14\x18\x0E\x48\x0F\x01\x03\x08\x08\x10\x10\x40\x03\x09\x13\x0C\x45\x2D\x3A\x21\x2A\x65\x7F"
"\x3D\x31\x25\x32\x29\x22\x76\x32\x30\x3C\x3E\x6B\x04\x20\x20\x3A\x34\x6D\x24\x2A\x2C\x24\x35\x67\x33\x2C\x0E\x13\x58\x35\x17\x11\x09\x05\x52\x07\x1F\x1E\x1A\x04\x54\x01\x05\x61\x48\x49\x0F\x19\x03\x04\x06\x43\x10\x04\x14\x0A\x0D\x16\x29\x32\x37\x37\x71\x33\x35\x33\x37\x7E\x35\x3F\x32\x3E\x3A\x32\x6A"
"\x22\x3B\x3A\x3B\x2A\x3F\x63\x48\x6E\x60\x03\x27\x24\x2F\x65\x0F\x0B\x58\x18\x5E\x1B\x15\x0E\x06\x01\x1F\x4B\x56\x00\x07\x19\x4A\x46\x45\x0C\x16\x1F\x03\x1F\x16\x43\x5C\x0F\x07\x0A\x01\x5B\x7A\x39\x39\x3A\x35\x2A\x2C\x73\x26\x32\x22\x7D\x76\x25\x31\x26\x3E\x24\x3A\x2C\x6E\x38\x25\x39\x2A\x49\x60\x61"
"\x31\x34\x28\x65\x57\x56\x11\x14\x0E\x10\x0E\x09\x52\x4F\x1E\x10\x1B\x12\x4A\x55\x0C\x04\x04\x0D\x0B\x1D\x4C\x0F\x03\x00\x0B\x14\x16\x49\x10\x04\x28\x75\x52\x53\x09\x0C\x10\x6F\x72\x25\x23\x71\x01\x04\x18\x64\x40\x66\x68\x1E\x1D\x03\x7E\x77\x62\x31\x25\x20\x2A\x67\x08\x2C\x14\x0E\x00\x59\x15\x1A\x0E"
"\x13\x17\x1F\x50\x18\x18\x57\x15\x55\x06\x02\x0F\x01\x1A\x18\x09\x04\x05\x0B\x14\x41\x30\x2A\x44\x48\x7A\x3D\x2D\x35\x32\x7F\x3F\x32\x3F\x23\x31\x25\x3F\x35\x3D\x39\x23\x3F\x31\x65\x44\x6F\x6C\x2B\x23\x30\x34\x61\x0F\x68\x0B\x65\x13\x15\x0B\x10\x1A\x1A\x5C\x31\x1B\x1D\x05\x09\x5A\x57\x30\x1A\x09\x00"
"\x0D\x1B\x4E\x1C\x19\x1D\x12\x0C\x12\x15\x48\x47\x31\x16\x3F\x7B\x31\x2D\x70\x55\x71\x7D\x05\x00\x1C\x60\x6C\x77\x20\x27\x2B\x25\x3B\x25\x2F\x3B\x25\x22\x2C\x63\x2C\x20\x3F\x22\x36\x65\x57\x5B\x17\x17\x12\x06\x5C\x1B\x1D\x01\x50\x02\x06\x12\x17\x1C\x0B\x07\x48\x0A\x0F\x1C\x09\x1E\x42\x4B\x0F\x0D\x02"
"\x02\x16\x45\x32\x3A\x2A\x3D\x29\x3E\x2E\x38\x7B\x7D\x5A\x5B\x13\x0F\x00\x07\x0B\x18\x42\x64\x6E\x08\x19\x04\x62\x22\x30\x31\x35\x67\x6C\x12\x29\x37\x1F\x50\x44\x5F\x1B\x0F\x13\x03\x18\x18\x15\x16\x18\x55\x26\x02\x06\x1C\x16\x4F\x0D\x1D\x12\x10\x40\x00\x16\x17\x01\x04\x28\x7B\x37\x37\x7E\x26\x33\x28"
"\x20\x73\x34\x34\x25\x3C\x20\x3A\x3A\x65\x42\x64\x6E\x3C\x35\x3E\x36\x26\x2D\x25\x7C\x67\x21\x2B\x1B\x19\x14\x1C\x1A\x5F\x1E\x04\x52\x17\x15\x17\x17\x02\x18\x01\x4A\x02\x06\x49\x1C\x0A\x0F\x08\x0C\x17\x40\x36\x35\x2B\x44\x48\x7A\x28\x3D\x2B\x28\x36\x3F\x38\x21\x73\x3C\x38\x3D\x32\x74\x31\x25\x28\x23"
"\x2C\x3C\x45\x6C\x6D\x23\x2D\x24\x61\x35\x34\x2C\x21\x5A\x0C\x17\x0B\x15\x5F\x12\x12\x00\x1E\x11\x1D\x1A\x0E\x5A\x7F\x47\x4B\x2F\x39\x3B\x55\x4C\x2E\x37\x27\x21\x41\x07\x09\x00\x45\x15\x2B\x3D\x37\x19\x13\x7C\x2A\x3D\x21\x3B\x71\x22\x3F\x26\x3A\x3F\x2C\x20\x69\x3A\x27\x29\x6D\x2A\x2C\x33\x35\x66\x00"
"\x14\x10\x54\x71\x55\x59\x3A\x10\x1F\x16\x17\x01\x4A\x51\x51\x33\x1B\x16\x01\x0E\x1A\x49\x2A\x0A\x1F\x06\x16\x0C\x10\x41\x11\x0E\x10\x0D\x7A\x0C\x0B\x15\x6C\x7F\x3E\x3C\x31\x38\x35\x3F\x32\x70\x74\x3C\x39\x6B\x3C\x21\x2B\x6F\x3F\x20\x2D\x2C\x34\x29\x23\x34\x30\x65\x0D\x1A\x01\x57\x74\x75\x33\x3B\x34"
"\x3A\x33\x38\x37\x3B\x54\x31\x25\x28\x3B\x63\x43\x4F\x38\x05\x0B\x10\x40\x11\x14\x08\x03\x17\x3B\x36\x62\x79\x79\x3B\x33\x3E\x21\x7E\x27\x22\x3A\x70\x74\x33\x2F\x3F\x2B\x21\x2B\x3C\x6C\x39\x2A\x26\x60\x2E\x20\x21\x2D\x26\x13\x1A\x14\x59\x33\x16\x1F\x0F\x1D\x00\x1F\x17\x02\x57\x23\x26\x26\x61\x48\x49"
"\x0A\x00\x0F\x18\x0F\x06\x0E\x15\x07\x13\x0D\x0A\x34\x7B\x34\x30\x28\x3A\x72"
);

MAN(glossary, "Glossary",
"\x1B\x0B\x08\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x10\x26\x27\x38\x3C\x29\x2A\x3C\x20\x21\x21\x6C\x60\x62\x22\x60\x31\x34\x28\x23\x37\x1B\x16\x58\x51\x29\x10\x0E\x19\x5E\x53\x33\x19\x04\x18\x19\x10\x46\x4B\x0F\x08\x03\x0A\x1F\x43\x4C\x4D\x49\x4F\x6C\x26\x34\x2C\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x13\x23\x20"
"\x3D\x3F\x34\x35\x21\x23\x24\x26\x69\x1E\x3D\x23\x2A\x30\x22\x2D\x2C\x2F\x29\x23\x65\x33\x15\x0C\x1C\x0C\x19\x1D\x1E\x17\x53\x5D\x51\x1E\x18\x03\x55\x1A\x19\x07\x0E\x1C\x0E\x01\x1E\x42\x17\x01\x0D\x0D\x47\x10\x0A\x7A\x3E\x39\x3A\x36\x55\x7C\x7D\x72\x73\x70\x71\x76\x77\x74\x75\x6A\x24\x3C\x21\x2B\x3D"
"\x6C\x62\x62\x37\x2F\x61\x32\x2F\x21\x65\x35\x28\x56\x59\x2A\x17\x19\x5D\x25\x1A\x1E\x42\x44\x57\x35\x25\x23\x4B\x01\x1A\x4E\x38\x05\x03\x06\x0C\x17\x12\x41\x47\x0A\x04\x2E\x32\x2E\x3C\x7E\x30\x32\x38\x7C\x59\x12\x18\x19\x04\x74\x75\x6A\x6B\x68\x69\x6E\x03\x29\x2A\x23\x20\x39\x61\x20\x2E\x36\x28\x0D"
"\x1A\x0A\x1C\x5E\x0B\x14\x1C\x06\x53\x03\x05\x17\x05\x00\x06\x4A\x1F\x00\x0C\x4E\x3F\x2F\x4D\x4A\x10\x05\x04\x46\x32\x21\x23\x13\x72\x76\x53\x1C\x36\x28\x7D\x7D\x73\x12\x28\x22\x32\x74\x00\x24\x22\x3C\x3A\x74\x6F\x74\x6D\x20\x2A\x34\x32\x66\x7A\x64\x74\x5A\x19\x01\x0D\x1B\x44\x5C\x36\x30\x5F\x50\x3C"
"\x34\x5B\x54\x32\x28\x47\x48\x3D\x2C\x4F\x51\x4D\x53\x53\x52\x55\x46\x14\x10\x00\x2A\x28\x76\x53\x1C\x36\x28\x11\x3D\x30\x3B\x34\x24\x77\x74\x02\x23\x25\x2C\x26\x39\x3C\x6C\x2B\x37\x2F\x2C\x6C\x22\x2E\x37\x2E\x5A\x1E\x16\x1A\x0C\x06\x0C\x09\x1B\x1C\x1E\x5F\x7C\x35\x18\x00\x0F\x1F\x07\x06\x1A\x07\x4C"
"\x4D\x31\x0B\x0F\x13\x12\x4A\x16\x04\x34\x3C\x3D\x79\x29\x36\x2E\x38\x3E\x36\x23\x22\x76\x31\x3B\x27\x6A\x26\x21\x2A\x2B\x63\x6C\x2C\x37\x27\x29\x2E\x6A\x67\x34\x2D\x15\x15\x1D\x0A\x50\x75\x3E\x2E\x3D\x37\x50\x51\x56\x57\x54\x55\x4A\x29\x04\x1C\x0B\x4F\x3F\x0E\x10\x06\x05\x0F\x46\x08\x02\x45\x1E\x3E"
"\x39\x2D\x36\x7F\x71\x7D\x34\x32\x24\x30\x3A\x77\x27\x2C\x39\x3F\x2D\x24\x6E\x2A\x3E\x3F\x2D\x31\x60\x32\x25\x35\x21\x20\x14\x55\x72\x3A\x1F\x1C\x14\x18\x52\x53\x50\x51\x56\x57\x20\x10\x07\x1B\x07\x1B\x0F\x1D\x15\x4D\x11\x17\x0F\x13\x07\x00\x01\x45\x3C\x34\x2A\x79\x2D\x2F\x39\x38\x36\x73\x78\x12\x06"
"\x02\x78\x75\x2E\x22\x3B\x22\x62\x6F\x3B\x28\x20\x6F\x60\x35\x2E\x32\x29\x27\x14\x1A\x11\x15\x0D\x56\x52\x77\x31\x3E\x34\x51\x56\x57\x54\x55\x4A\x4B\x48\x2A\x01\x02\x01\x0C\x0C\x07\x40\x31\x14\x08\x09\x15\x2E\x7B\x75\x79\x2A\x37\x39\x7D\x31\x3F\x31\x22\x25\x3E\x37\x75\x1D\x22\x26\x2D\x21\x38\x3F\x6D"
"\x21\x2C\x2D\x2C\x27\x29\x20\x65\x09\x13\x1D\x15\x12\x51\x76\x3E\x22\x26\x50\x51\x56\x57\x54\x55\x4A\x4B\x2B\x0C\x00\x1B\x1E\x0C\x0E\x43\x30\x13\x09\x04\x01\x16\x29\x32\x36\x3E\x7E\x0A\x32\x34\x26\x73\x7D\x71\x22\x3F\x31\x75\x28\x39\x29\x20\x20\x6F\x23\x2B\x62\x37\x28\x24\x66\x17\x07\x6B\x70\x3F\x30"
"\x3A\x2E\x5F\x5C\x5D\x52\x53\x50\x51\x37\x02\x00\x1A\x07\x0A\x1C\x00\x0D\x4F\x25\x3D\x42\x02\x04\x05\x14\x02\x17\x16\x7A\x3A\x2B\x2A\x37\x38\x32\x30\x37\x3D\x24\x71\x7E\x31\x26\x3A\x27\x6B\x3C\x21\x2B\x6F\x3E\x22\x37\x37\x25\x33\x6F\x69\x4E\x01\x36\x37\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x34\x08\x18\x16"
"\x19\x1C\x09\x4B\x24\x00\x00\x04\x4C\x21\x0B\x01\x12\x00\x14\x1E\x44\x48\x7A\x28\x30\x38\x2C\x3A\x38\x7D\x31\x3C\x34\x34\x76\x22\x27\x30\x2E\x6B\x2A\x30\x6E\x3F\x3E\x22\x25\x31\x21\x2C\x35\x69\x4E\x01\x34\x28\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x34\x1E\x1B\x16\x1D\x1B\x4A\x25\x09\x04\x0B\x4F\x3F\x14\x11"
"\x17\x05\x0C\x46\x4A\x44\x11\x28\x3A\x36\x2A\x32\x3E\x28\x38\x21\x73\x3E\x30\x3B\x32\x27\x75\x3E\x24\x68\x00\x1E\x6F\x2D\x29\x26\x31\x25\x32\x35\x22\x37\x6B\x70\x3F\x28\x30\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x32\x18\x00\x06\x4A\x3B\x0D\x1B\x4E\x26\x02\x0E\x0A\x43\x4D\x41\x02\x0E\x17\x15\x36\x3A\x21\x79"
"\x2C\x3A\x2F\x32\x3E\x26\x24\x38\x39\x39\x74\x31\x2F\x25\x3B\x20\x3A\x36\x6C\x62\x62\x2E\x2F\x34\x35\x22\x64\x36\x0A\x1E\x1D\x1D\x50\x75\x38\x0F\x1B\x05\x15\x03\x56\x57\x54\x55\x4A\x38\x07\x0F\x1A\x18\x0D\x1F\x07\x43\x14\x09\x07\x13\x44\x09\x3F\x2F\x2B\x79\x09\x36\x32\x39\x3D\x24\x23\x71\x23\x24\x31"
"\x75\x2B\x6B\x20\x28\x3C\x2B\x3B\x2C\x30\x26\x60\x25\x23\x31\x2D\x26\x1F\x55\x72\x3C\x38\x36\x53\x28\x37\x35\x39\x51\x56\x57\x39\x1A\x0E\x0E\x1A\x07\x4E\x09\x05\x1F\x0F\x14\x01\x13\x03\x5C\x44\x30\x1F\x1D\x11\x79\x2C\x3A\x2C\x31\x33\x30\x35\x35\x76\x15\x1D\x1A\x19\x65\x42\x2C\x36\x09\x0D\x19\x62\x63"
"\x60\x61\x66\x67\x02\x2C\x16\x1E\x58\x0A\x07\x0C\x08\x18\x1F\x53\x16\x1E\x04\x57\x21\x26\x28\x4B\x1B\x1D\x07\x0C\x07\x1E\x4D\x0E\x05\x05\x0F\x06\x44\x4D\x34\x34\x78\x6D\x7E\x18\x1E\x7D\x34\x3A\x3C\x34\x76\x3B\x3D\x38\x23\x3F\x61\x67\x44\x0A\x34\x3D\x2E\x2C\x32\x24\x34\x67\x64\x65\x3C\x12\x14\x1C\x5E"
"\x3A\x04\x0D\x1E\x1C\x02\x14\x04\x57\x59\x55\x1E\x03\x0D\x49\x08\x06\x00\x08\x42\x0E\x01\x0F\x07\x00\x01\x17\x7A\x73\x0F\x30\x30\x74\x19\x74\x7C\x59\x16\x10\x02\x64\x66\x75\x6A\x6B\x68\x69\x6E\x00\x20\x29\x62\x25\x29\x2D\x23\x67\x37\x3C\x09\x0F\x1D\x14\x5E\x08\x15\x09\x1A\x53\x11\x51\x42\x57\x33\x37"
"\x4A\x0D\x01\x05\x0B\x4F\x1F\x04\x18\x06\x40\x0D\x0F\x0A\x0D\x11\x74\x51\x1E\x30\x2C\x3A\x2B\x3C\x3E\x3F\x70\x71\x76\x11\x3D\x39\x3E\x2E\x3A\x3A\x6E\x21\x29\x39\x35\x2C\x32\x2A\x66\x33\x36\x24\x1C\x1D\x11\x1A\x5E\x1D\x05\x5D\x00\x06\x1C\x14\x05\x59\x7E\x33\x3A\x38\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x26"
"\x13\x07\x0A\x01\x16\x7A\x2B\x3D\x2B\x7E\x2C\x39\x3E\x3D\x3D\x34\x71\x7B\x77\x33\x34\x27\x2E\x68\x3A\x23\x20\x23\x39\x2A\x2D\x25\x32\x35\x69\x4E\x02\x2A\x2E\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x37\x03\x17\x07\x1C\x1C\x09\x18\x48\x39\x1C\x00\x0F\x08\x11\x10\x09\x0F\x01\x47\x31\x0B\x33\x2F\x78\x74\x7E\x2D"
"\x39\x33\x36\x36\x22\x22\x76\x3E\x39\x34\x2D\x2E\x3B\x66\x38\x26\x28\x28\x2D\x6D\x4A\x09\x02\x15\x64\x65\x5A\x5B\x58\x59\x5E\x5F\x34\x14\x15\x1B\x50\x35\x0F\x19\x15\x18\x03\x08\x48\x3B\x0F\x01\x0B\x08\x42\x4E\x40\x16\x0F\x03\x01\x17\x7A\x38\x37\x35\x31\x2D\x73\x3F\x20\x3A\x37\x39\x22\x39\x31\x26\x39"
"\x6B\x3A\x28\x20\x28\x29\x63\x48\x0B\x04\x05\x66\x67\x64\x65\x5A\x5B\x58\x59\x36\x1E\x0E\x19\x52\x37\x19\x02\x1D\x57\x30\x07\x03\x1D\x0D\x49\x43\x4F\x01\x08\x01\x0B\x01\x0F\x0F\x04\x05\x09\x7A\x28\x2C\x36\x2C\x3E\x3B\x38\x7C\x59\x18\x28\x26\x32\x26\x78\x1C\x6B\x68\x69\x6E\x02\x25\x2E\x30\x2C\x33\x2E"
"\x20\x33\x63\x36\x5A\x19\x0D\x10\x12\x0B\x51\x14\x1C\x53\x06\x18\x04\x03\x01\x14\x06\x4B\x05\x08\x0D\x07\x05\x03\x07\x43\x10\x0D\x07\x13\x02\x0A\x28\x36\x76\x53\x17\x0F\x7C\x3C\x36\x37\x22\x34\x25\x24\x74\x1B\x3F\x26\x2D\x3B\x27\x2C\x6C\x2C\x26\x27\x32\x24\x35\x34\x64\x2A\x1C\x5B\x19\x59\x1A\x1A\x0A"
"\x14\x11\x16\x50\x1E\x18\x57\x15\x55\x04\x0E\x1C\x1E\x01\x1D\x07\x43\x68\x2A\x33\x2E\x46\x47\x44\x45\x7A\x7B\x78\x79\x1F\x7F\x2F\x34\x3C\x34\x3C\x34\x7B\x31\x3D\x39\x2F\x6B\x21\x24\x2F\x28\x29\x6D\x2D\x25\x60\x20\x28\x67\x2B\x35\x0E\x12\x1B\x18\x12\x5F\x18\x14\x01\x10\x50\x5E\x56\x1E\x1A\x06\x1E\x0A"
"\x04\x05\x0B\x1D\x42\x67\x2E\x22\x2E\x41\x46\x47\x44\x45\x7A\x7B\x78\x15\x31\x3C\x3D\x31\x72\x12\x22\x34\x37\x77\x1A\x30\x3E\x3C\x27\x3B\x25\x6F\x61\x6D\x3B\x2C\x35\x33\x66\x2F\x2B\x28\x1F\x54\x17\x1F\x18\x16\x1F\x18\x52\x1D\x15\x05\x01\x18\x06\x1E\x44\x61\x24\x08\x1A\x0A\x02\x0E\x1B\x43\x40\x41\x46"
"\x23\x01\x09\x3B\x22\x78\x71\x33\x2C\x75\x7D\x7F\x73\x3D\x30\x22\x23\x31\x27\x39\x6B\x2E\x26\x3C\x6F\x23\x23\x2E\x2A\x2E\x24\x66\x20\x25\x28\x13\x15\x1F\x57\x74\x32\x3E\x2F\x5D\x34\x20\x25\x56\x57\x54\x55\x25\x07\x0C\x46\x00\x0A\x1B\x4D\x06\x0A\x13\x0A\x46\x17\x05\x17\x2E\x32\x2C\x30\x31\x31\x35\x33"
"\x35\x73\x23\x32\x3E\x32\x39\x30\x39\x65\x42\x24\x3D\x2C\x23\x23\x24\x2A\x27\x61\x66\x67\x17\x3C\x09\x0F\x1D\x14\x5E\x3C\x13\x13\x14\x1A\x17\x04\x04\x16\x00\x1C\x05\x05\x48\x1D\x01\x00\x00\x4D\x4A\x01\x0F\x0E\x12\x47\x0B\x15\x2E\x32\x37\x37\x2D\x73\x7C\x2E\x37\x21\x26\x38\x35\x32\x27\x7C\x64\x41\x05"
"\x1A\x07\x6F\x6C\x6D\x62\x63\x60\x61\x66\x10\x2D\x2B\x1E\x14\x0F\x0A\x5E\x36\x12\x0E\x06\x12\x1C\x1D\x13\x05\x54\x05\x0B\x08\x03\x08\x09\x0A\x4C\x45\x4C\x0E\x13\x08\x46\x01\x0D\x09\x3F\x28\x71\x77\x54\x11\x08\x1B\x01\x73\x70\x71\x76\x77\x74\x75\x0E\x2E\x2E\x28\x3B\x23\x38\x6D\x15\x2A\x2E\x25\x29\x30"
"\x37\x65\x1C\x12\x14\x1C\x5E\x0C\x05\x0E\x06\x16\x1D\x51\x5E\x07\x11\x07\x07\x02\x1B\x1A\x07\x00\x02\x1E\x4E\x43\x05\x0F\x05\x15\x1D\x15\x2E\x32\x37\x37\x77\x71\x56\x13\x04\x1E\x35\x71\x76\x77\x74\x75\x6A\x6B\x0E\x28\x3D\x3B\x6C\x1E\x11\x07\x60\x28\x28\x33\x21\x37\x1C\x1A\x1B\x1C\x5E\x57\x31\x53\x40"
"\x53\x14\x03\x1F\x01\x11\x06\x43\x45\x62\x26\x00\x0A\x28\x1F\x0B\x15\x05\x41\x46\x47\x29\x0C\x39\x29\x37\x2A\x31\x39\x28\x7A\x21\x73\x33\x3D\x39\x22\x30\x75\x39\x3F\x27\x3B\x2F\x28\x29\x61\x62\x21\x35\x28\x2A\x33\x64\x2C\x14\x0F\x17\x59\x29\x16\x12\x19\x1D\x04\x03\x5F\x7C\x38\x27\x55\x4A\x4B\x48\x49"
"\x4E\x4F\x4C\x4D\x2D\x13\x05\x13\x07\x13\x0D\x0B\x3D\x7B\x0B\x20\x2D\x2B\x39\x30\x72\x7E\x70\x06\x3F\x39\x30\x3A\x3D\x38\x64\x69\x02\x26\x22\x38\x3A\x6F\x60\x2C\x27\x24\x0B\x16\x54\x71\x28\x18\x0A\x1C\x14\x5D\x52\x53\x50\x51\x56\x36\x54\x06\x05\x0D\x1C\x1E\x0F\x1D\x09\x4D\x04\x0A\x18\x5A\x46\x40\x34"
"\x04\x2E\x38\x30\x79\x0A\x2A\x39\x2E\x36\x32\x29\x76\x76\x6A\x74\x38\x25\x25\x3C\x21\x22\x36\x6C\x3E\x27\x20\x35\x33\x2F\x33\x3D\x65\x0F\x0B\x1C\x18\x0A\x1A\x0F\x53\x78\x23\x39\x35\x56\x57\x54\x55\x4A\x4B\x48\x49\x3E\x1D\x03\x0E\x07\x10\x13\x41\x2F\x03\x01\x0B\x2E\x32\x3E\x30\x3B\x2D\x7C\x70\x72\x3D"
"\x25\x3C\x34\x32\x26\x75\x25\x2D\x68\x28\x6E\x3D\x39\x23\x2C\x2A\x2E\x26\x66\x37\x36\x2A\x19\x1E\x0B\x0A\x50\x75\x2C\x34\x3C\x53\x50\x51\x56\x57\x54\x55\x4A\x2D\x09\x1A\x1A\x4F\x00\x02\x01\x02\x0C\x41\x15\x0E\x03\x0B\x77\x32\x36\x79\x3D\x30\x38\x38\x72\x7B\x07\x38\x38\x33\x3B\x22\x39\x6B\x00\x2C\x22"
"\x23\x23\x64\x6C\x49\x10\x2E\x31\x22\x36\x16\x12\x1E\x14\x15\x5E\x3E\x18\x0B\x13\x1D\x13\x14\x12\x57\x17\x1A\x07\x06\x09\x07\x0A\x4F\x1F\x05\x07\x0F\x0C\x41\x4E\x14\x01\x00\x7A\x36\x39\x37\x2B\x3E\x30\x70\x22\x3C\x27\x34\x24\x24\x3C\x30\x26\x27\x61\x67\x44\x1D\x0D\x04\x06\x63\x60\x61\x66\x67\x64\x65"
"\x39\x14\x15\x1B\x17\x11\x15\x13\x15\x53\x14\x18\x05\x1C\x07\x55\x0C\x04\x1A\x49\x1D\x1F\x09\x08\x06\x43\x0F\x13\x46\x15\x01\x01\x2F\x35\x3C\x38\x30\x3C\x25\x73\x58\x01\x11\x1C\x76\x77\x74\x75\x6A\x6B\x68\x69\x1C\x2E\x22\x29\x2D\x2E\x60\x00\x25\x24\x21\x36\x09\x5B\x35\x1C\x13\x10\x0E\x04\x52\x5E\x50"
"\x17\x17\x04\x00\x55\x1D\x04\x1A\x02\x07\x01\x0B\x4D\x0F\x06\x0D\x0E\x14\x1E\x4A\x6F\x08\x3E\x1E\x0A\x7E\x7F\x7C\x7D\x72\x73\x70\x03\x33\x24\x3D\x39\x23\x2E\x26\x3D\x6E\x29\x25\x21\x27\x63\x33\x38\x35\x33\x21\x28\x5A\x53\x3C\x1C\x08\x5F\x38\x0F\x1B\x05\x15\x5D\x56\x24\x00\x1A\x18\x0A\x0F\x0C\x4E\x3C"
"\x1C\x0C\x01\x06\x13\x48\x48\x6D\x36\x00\x3D\x32\x2B\x2D\x2C\x26\x7C\x7D\x72\x04\x39\x3F\x32\x38\x23\x26\x6D\x6B\x3B\x2C\x3A\x3B\x25\x23\x25\x30\x60\x25\x27\x33\x25\x27\x1B\x08\x1D\x59\x56\x0C\x19\x18\x52\x1E\x11\x1F\x03\x16\x18\x58\x18\x0E\x0F\x00\x1D\x1B\x1E\x14\x4B\x4D\x6A\x33\x09\x12\x10\x00\x28"
"\x7B\x78\x79\x7E\x7F\x18\x38\x24\x3A\x33\x34\x76\x34\x3B\x3B\x24\x2E\x2B\x3D\x27\x21\x2B\x6D\x3B\x2C\x35\x33\x66\x0B\x05\x0B\x5A\x0F\x17\x59\x0A\x17\x19\x5D\x1B\x1D\x04\x14\x04\x19\x11\x01\x44\x61\x3B\x08\x00\x0B\x0E\x02\x1A\x43\x40\x41\x46\x2E\x17\x0A\x36\x3A\x2C\x3C\x3A\x7F\x39\x33\x24\x3A\x22\x3E"
"\x38\x3A\x31\x3B\x3E\x6B\x3C\x26\x6E\x3D\x39\x23\x62\x36\x2E\x35\x34\x32\x37\x31\x1F\x1F\x58\x0A\x11\x19\x08\x0A\x13\x01\x15\x5F\x7C\x24\x27\x31\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x31\x0C\x0C\x08\x02\x47\x37\x11\x3B\x2F\x3D\x79\x1A\x2D\x35\x2B\x37\x73\x7D\x71\x30\x36\x27\x21\x6A\x38\x3C\x26\x3C\x2E\x2B"
"\x28\x6E\x63\x2E\x2E\x66\x2A\x2B\x33\x13\x15\x1F\x59\x0E\x1E\x0E\x09\x01\x5D\x7A\x25\x26\x3A\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x38\x1F\x17\x10\x14\x04\x02\x47\x34\x09\x3B\x2F\x3E\x36\x2C\x32\x7C\x10\x3D\x37\x25\x3D\x33\x77\x79\x75\x39\x2E\x2B\x3C\x3C\x26\x38\x34\x62\x20\x28\x28\x36\x69\x4E\x10\x3B\x38"
"\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x25\x02\x13\x05\x54\x34\x09\x08\x07\x1C\x00\x1B\x4C\x2E\x0D\x0D\x14\x13\x09\x0B\x44\x48\x7A\x3A\x3C\x34\x37\x31\x7C\x2D\x37\x21\x3D\x38\x25\x24\x3D\x3A\x24\x6B\x38\x3B\x21\x22\x3C\x39\x31\x6D\x4A\x14\x03\x01\x0D\x65\x5A\x5B\x58\x59\x5E\x5F\x2F\x18\x17\x53\x25\x34\x30"
"\x3E\x54\x14\x08\x04\x1E\x0C\x4E\x47\x01\x02\x06\x06\x12\x0F\x46\x01\x0D\x17\x37\x2C\x39\x2B\x3B\x76\x72\x57\x07\x00\x12\x71\x76\x77\x74\x75\x6A\x6B\x68\x1C\x20\x26\x3A\x28\x30\x30\x21\x2D\x66\x14\x21\x37\x13\x1A\x14\x59\x3C\x0A\x0F\x5D\x5F\x53\x04\x19\x13\x57\x19\x1A\x19\x1F\x48\x0A\x01\x02\x01\x02"
"\x0C\x43\x10\x0E\x14\x13\x4A\x6F\x0C\x0B\x16\x79\x7E\x7F\x7C\x7D\x72\x73\x70\x07\x3F\x25\x20\x20\x2B\x27\x68\x19\x3C\x26\x3A\x2C\x36\x26\x60\x0F\x23\x33\x33\x2A\x08\x10\x58\x54\x5E\x1A\x12\x1E\x00\x0A\x00\x05\x13\x13\x54\x01\x1F\x05\x06\x0C\x02\x41\x66\x3A\x23\x2D\x40\x41\x46\x47\x44\x45\x7A\x7B\x0F"
"\x30\x3A\x3A\x7C\x1C\x20\x36\x31\x71\x18\x32\x20\x22\x25\x39\x23\x69\x63\x6F\x38\x25\x27\x63\x29\x2F\x32\x22\x36\x2B\x1F\x0F\x58\x0A\x17\x1B\x19\x5D\x1D\x15\x50\x05\x1E\x12\x54\x07\x05\x1E\x1C\x0C\x1C\x41\x66\x3A\x2E\x22\x2E\x41\x46\x47\x44\x45\x7A\x7B\x0F\x30\x2C\x3A\x30\x38\x21\x20\x70\x1D\x17\x19"
"\x74\x78\x6A\x1C\x21\x64\x08\x26\x62\x47\x15\x13\x01\x73\x69\x10\x14\x04\x49\x5B\x58\x2E\x17\x52\x3A\x14\x52\x00\x15\x12\x03\x05\x1D\x01\x13\x4B\x1B\x1D\x0F\x01\x08\x0C\x10\x07\x13\x4F\x6C\x30\x37\x29\x7A\x7B\x78\x79\x7E\x7F\x7C\x7D\x05\x3A\x3E\x35\x39\x20\x27\x75\x19\x3E\x2A\x3A\x37\x3C\x38\x28\x2F"
"\x63\x26\x2E\x34\x67\x08\x2C\x14\x0E\x00\x59\x56\x0C\x19\x18\x52\x1E\x11\x1F\x03\x16\x18\x58\x1D\x18\x04\x40\x40\x65\x14\x55\x54\x4C\x18\x57\x52\x47\x44\x45\x7A\x68\x6A\x74\x3C\x36\x28\x7D\x7D\x73\x66\x65\x7B\x35\x3D\x21\x6A\x3B\x3A\x26\x2D\x2A\x3F\x3E\x2D\x31\x60\x20\x34\x24\x2C\x2C\x0E\x1E\x1B\x0D"
"\x0B\x0D\x19\x0E\x5C\x79\x2A\x18\x06\x57\x54\x55\x4A\x4B\x48\x49\x4E\x2C\x03\x00\x12\x11\x05\x12\x15\x02\x00\x45\x3B\x29\x3B\x31\x37\x29\x39\x7D\x7A\x7D\x2A\x38\x26\x7E\x7A"
);

MAN(errors, "Common Error Codes",
"\x0D\x13\x19\x0D\x7E\x1A\x0E\x0F\x1D\x01\x70\x12\x19\x13\x11\x06\x6A\x06\x0D\x08\x00\x45\x1B\x24\x2C\x27\x2F\x36\x35\x67\x21\x37\x08\x14\x0A\x0A\x5E\x1E\x0E\x18\x52\x1B\x15\x09\x56\x19\x01\x18\x08\x0E\x1A\x1A\x40\x4F\x3F\x08\x03\x11\x03\x09\x46\x13\x0C\x00\x7A\x3E\x20\x38\x3D\x2B\x7C\x3E\x3D\x37\x35"
"\x71\x39\x39\x74\x21\x22\x2E\x68\x26\x28\x29\x25\x2E\x2B\x22\x2C\x4B\x22\x28\x27\x36\x5A\x53\x5F\x1D\x11\x1C\x0F\x50\x01\x16\x11\x03\x15\x1F\x54\x49\x09\x04\x0C\x0C\x50\x48\x45\x4D\x0D\x11\x40\x08\x08\x47\x10\x0D\x3F\x7B\x1D\x2F\x3B\x31\x28\x7D\x04\x3A\x35\x26\x33\x25\x74\x36\x25\x25\x3C\x2C\x36\x3B"
"\x62\x6D\x00\x26\x2C\x2E\x31\x67\x25\x37\x1F\x71\x0C\x11\x1B\x5F\x11\x12\x01\x07\x50\x12\x19\x1A\x19\x1A\x04\x4B\x07\x07\x0B\x1C\x4C\x0C\x0C\x07\x40\x15\x0E\x02\x0D\x17\x7A\x3D\x31\x21\x3B\x2C\x72\x57\x58\x04\x19\x1F\x12\x18\x03\x06\x6A\x1E\x18\x0D\x0F\x1B\x09\x47\x72\x3B\x78\x71\x76\x70\x74\x75\x4A"
"\x49\x58\x59\x38\x16\x10\x18\x01\x53\x1D\x18\x05\x04\x1D\x1B\x0D\x44\x0B\x06\x1C\x1D\x19\x1D\x16\x43\x4D\x41\x14\x02\x17\x11\x3B\x29\x2C\x75\x7E\x2D\x29\x33\x72\x27\x38\x34\x76\x02\x24\x31\x2B\x3F\x2D\x43\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x2E\x09\x17\x0C\x1C\x13\x19\x0E\x1A\x1C\x1F\x05"
"\x13\x05\x58\x55\x1E\x03\x0D\x07\x4E\x1C\x0A\x0E\x42\x4C\x13\x02\x07\x09\x0A\x0A\x2D\x7B\x39\x37\x3A\x7F\x38\x34\x21\x3E\x7D\x23\x33\x24\x20\x3A\x38\x2E\x66\x43\x7E\x37\x74\x7D\x72\x74\x70\x71\x76\x72\x64\x65\x3B\x18\x1B\x1C\x0D\x0C\x5C\x19\x17\x1D\x19\x14\x12\x57\x59\x55\x18\x1E\x06\x49\x0F\x1C\x4C"
"\x0C\x06\x0E\x09\x0F\x4A\x47\x16\x00\x29\x3E\x2C\x79\x0B\x2F\x38\x3C\x26\x36\x70\x32\x39\x3A\x24\x3A\x24\x2E\x26\x3D\x3D\x61\x46\x7D\x3A\x7B\x70\x71\x71\x77\x74\x77\x4A\x5B\x58\x3F\x17\x13\x19\x5D\x1B\x1D\x50\x04\x05\x12\x54\x58\x4A\x18\x1C\x06\x1E\x4F\x3B\x04\x0C\x07\x0F\x16\x15\x47\x31\x15\x3E\x3A"
"\x2C\x3C\x7E\x2C\x39\x2F\x24\x3A\x33\x34\x7A\x77\x37\x39\x2F\x2A\x3A\x43\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x65\x29\x14\x1E\x0D\x09\x1E\x0E\x18\x36\x1A\x03\x05\x04\x1E\x16\x00\x1E\x02\x07\x07\x42\x4F\x1E\x08\x11\x17\x01\x13\x12\x49\x6E\x55\x22\x63\x68\x69\x69\x6F\x6C\x68\x65\x73\x70\x18\x38"
"\x21\x35\x39\x23\x2F\x68\x39\x2F\x3D\x2D\x20\x27\x37\x25\x33\x66\x6A\x64\x37\x1F\x08\x1D\x0D\x5E\x28\x15\x13\x16\x1C\x07\x02\x56\x22\x04\x11\x0B\x1F\x0D\x49\x0D\x00\x01\x1D\x0D\x0D\x05\x0F\x12\x14\x4A\x6F\x6A\x23\x60\x69\x6E\x68\x6C\x69\x60\x61\x70\x71\x05\x32\x26\x23\x23\x28\x2D\x69\x2A\x26\x3F\x2C"
"\x20\x2F\x25\x25\x66\x6A\x64\x36\x1F\x0F\x58\x0E\x0B\x1E\x09\x0E\x17\x01\x06\x51\x02\x18\x54\x34\x1F\x1F\x07\x04\x0F\x1B\x05\x0E\x42\x02\x0E\x05\x46\x14\x10\x04\x28\x2F\x78\x30\x2A\x71\x56\x6D\x2A\x6B\x60\x61\x61\x67\x62\x61\x79\x6B\x68\x00\x20\x3C\x38\x2C\x2E\x2F\x25\x33\x66\x21\x25\x2C\x16\x0E\x0A"
"\x1C\x5E\x52\x5C\x1B\x00\x16\x15\x51\x12\x1E\x07\x1E\x4A\x18\x18\x08\x0D\x0A\x40\x4D\x10\x06\x10\x00\x0F\x15\x44\x4B\x14\x1E\x0C\x75\x7E\x2D\x39\x29\x20\x2A\x7E\x5B\x66\x2F\x6C\x65\x7A\x7C\x78\x7F\x0C\x0E\x6C\x6D\x10\x13\x03\x61\x33\x29\x25\x33\x1B\x12\x14\x18\x1C\x13\x19\x5D\x5F\x53\x13\x19\x13\x14"
"\x1F\x55\x04\x0E\x1C\x1E\x01\x1D\x07\x42\x04\x0A\x12\x04\x11\x06\x08\x09\x76\x7B\x2A\x3C\x2D\x2B\x3D\x2F\x26\x73\x23\x34\x24\x21\x3D\x36\x2F\x38\x66\x43\x7E\x37\x74\x7D\x72\x74\x72\x04\x00\x03\x64\x65\x39\x14\x16\x17\x1B\x1C\x08\x14\x1D\x1D\x50\x17\x17\x1E\x18\x10\x0E\x4B\x45\x49\x0D\x07\x09\x0E\x09"
"\x43\x09\x0F\x12\x02\x16\x0B\x3F\x2F\x77\x3F\x37\x2D\x39\x2A\x33\x3F\x3C\x7E\x26\x25\x3B\x2D\x33\x65\x42\x79\x36\x77\x7C\x7D\x75\x71\x05\x04\x74\x67\x64\x11\x13\x16\x1D\x16\x0B\x0B\x5C\x50\x52\x01\x15\x05\x04\x0E\x54\x19\x0B\x1F\x0D\x1B\x55\x4F\x0F\x05\x07\x00\x0B\x41\x0F\x09\x10\x00\x28\x35\x3D\x2D"
"\x70\x55\x6C\x25\x6A\x63\x62\x65\x66\x67\x67\x61\x6A\x6B\x01\x27\x3D\x3B\x2D\x21\x2E\x26\x32\x61\x24\x32\x37\x3C\x5A\x56\x58\x0B\x1B\x0C\x08\x1C\x00\x07\x50\x10\x18\x13\x54\x07\x0F\x1F\x1A\x10\x40\x65\x5C\x15\x5A\x53\x52\x55\x54\x57\x54\x27\x7A\x7B\x0B\x3C\x2C\x29\x35\x3E\x37\x73\x3E\x34\x33\x33\x31"
"\x31\x6A\x66\x68\x3A\x3A\x2E\x3E\x39\x62\x34\x35\x20\x33\x34\x21\x37\x0C\x57\x58\x3B\x37\x2B\x2F\x51\x52\x10\x02\x08\x06\x03\x07\x03\x09\x45\x62\x59\x16\x57\x5C\x5F\x56\x55\x50\x51\x51\x47\x44\x21\x35\x2C\x36\x35\x31\x3E\x38\x7D\x34\x32\x39\x3D\x33\x33\x74\x78\x6A\x28\x20\x2C\x2D\x24\x6C\x29\x2B\x30"
"\x2B\x61\x35\x37\x25\x26\x1F\x5B\x19\x17\x1A\x5F\x12\x18\x06\x04\x1F\x03\x1D\x59\x7E\x45\x12\x53\x58\x59\x59\x5C\x5B\x5C\x50\x43\x40\x31\x07\x04\x0F\x04\x3D\x3E\x78\x3D\x3F\x32\x3D\x3A\x37\x37\x70\x7C\x76\x25\x21\x3B\x6A\x0F\x01\x1A\x03\x6F\x3E\x28\x31\x37\x2F\x33\x23\x2F\x21\x24\x16\x0F\x10\x55\x5E"
"\x0B\x14\x18\x1C\x53\x02\x14\x02\x05\x0D\x5B\x60\x5B\x10\x51\x5E\x5F\x2A\x5D\x5A\x52\x26\x41\x46\x23\x2D\x36\x17\x7B\x2B\x36\x2B\x2D\x3F\x38\x72\x3E\x39\x22\x25\x3E\x3A\x32\x6A\x66\x68\x3C\x3D\x2A\x6C\x62\x31\x2C\x35\x33\x25\x22\x64\x32\x13\x0F\x10\x59\x17\x11\x0F\x09\x13\x1F\x1C\x51\x1B\x12\x10\x1C"
"\x0B\x45\x62\x59\x16\x57\x5C\x5D\x24\x53\x59\x51\x50\x48\x54\x1D\x62\x6B\x68\x1F\x6E\x66\x6E\x6F\x72\x73\x03\x3E\x23\x25\x37\x30\x6A\x3E\x26\x28\x38\x2E\x25\x21\x23\x21\x2C\x24\x66\x6A\x64\x37\x1F\x0F\x0A\x00\x52\x5F\x13\x0F\x52\x03\x1F\x18\x18\x03\x54\x31\x23\x38\x25\x49\x0F\x1B\x4C\x0C\x68\x43\x40"
"\x41\x46\x47\x44\x45\x7A\x7B\x78\x79\x7E\x08\x35\x33\x36\x3C\x27\x22\x76\x1E\x07\x1A\x6A\x63\x25\x26\x3B\x21\x38\x6D\x2B\x37\x60\x20\x28\x23\x64\x30\x09\x1E\x58\x56\x0D\x10\x09\x0F\x11\x16\x59\x5F\x7C\x47\x0C\x36\x5B\x52\x58\x59\x5F\x5F\x5D\x4D\x42\x27\x12\x08\x10\x02\x16\x4A\x28\x34\x34\x35\x3C\x3E"
"\x3F\x36\x72\x37\x25\x23\x3F\x39\x33\x75\x3F\x3B\x2F\x3B\x2F\x2B\x29\x6D\x6F\x63\x35\x31\x22\x26\x30\x20\x5A\x1F\x0A\x10\x08\x1A\x0E\x0E\x5E\x53\x05\x1F\x06\x1B\x01\x12\x60\x4B\x48\x49\x4E\x4F\x4C\x4D\x42\x43\x40\x41\x46\x17\x01\x17\x33\x2B\x30\x3C\x2C\x3E\x30\x2E\x7E\x73\x22\x34\x22\x25\x2D\x75\x3E"
"\x23\x2D\x69\x3B\x3F\x2B\x3F\x23\x27\x25\x6F\x4C\x77\x3C\x06\x4B\x42\x48\x49\x4C\x4F\x4C\x52\x42\x0B\x42\x41\x46\x32\x54\x55\x24\x04\x1C\x49\x0B\x01\x03\x18\x05\x0B\x40\x12\x16\x06\x07\x00\x7A\x34\x2A\x79\x36\x3E\x2E\x39\x25\x32\x22\x34\x76\x7A\x74\x33\x38\x2E\x2D\x69\x3D\x3F\x2D\x2E\x27\x63\x6F\x61"
"\x25\x2F\x21\x26\x11\x71\x58\x59\x5E\x5F\x5C\x5D\x52\x53\x50\x51\x56\x57\x06\x10\x1B\x1E\x01\x1B\x0B\x02\x09\x03\x16\x10\x4E\x6B\x56\x1F\x27\x54\x63\x6B\x68\x6B\x6F\x6F\x7C\x7D\x07\x23\x34\x30\x22\x32\x74\x37\x26\x24\x2B\x22\x2B\x2B\x6C\x2F\x3B\x63\x33\x38\x35\x33\x21\x28\x5A\x08\x0C\x18\x0A\x1A\x5C"
"\x50\x52\x01\x05\x1F\x56\x03\x1C\x10\x4A\x1F\x1A\x06\x1B\x0D\x00\x08\x11\x0B\x0F\x0E\x12\x02\x16\x4B\x50\x51\x19\x09\x0E\x7F\x10\x1C\x07\x1D\x13\x19\x5C\x67\x2C\x16\x7A\x7B\x78\x79\x7E\x78\x0E\x6D\x62\x01\x21\x25\x66\x2E\x29\x24\x1D\x1E\x58\x54\x5E\x0B\x14\x18\x52\x12\x00\x01\x56\x19\x11\x10\x0E\x18"
"\x48\x08\x4E\x59\x58\x40\x00\x0A\x14\x41\x14\x12\x0A\x11\x33\x36\x3D\x79\x31\x2D\x7C\x29\x3A\x36\x70\x37\x3F\x3B\x31\x75\x23\x38\x42\x69\x6E\x6F\x6C\x6D\x62\x63\x60\x61\x66\x67\x64\x26\x15\x09\x0A\x0C\x0E\x0B\x47\x5D\x00\x16\x19\x1F\x05\x03\x15\x19\x06\x4B\x1C\x01\x0B\x4F\x0D\x1D\x12\x4D\x6A\x51\x1E"
"\x24\x54\x55\x6A\x6B\x69\x6A\x6B\x7F\x7C\x10\x3B\x20\x23\x38\x38\x30\x74\x11\x06\x07\x68\x64\x6E\x3D\x29\x24\x2C\x30\x34\x20\x2A\x2B\x64\x31\x12\x1E\x58\x18\x0E\x0F\x5C\x12\x00\x53\x04\x19\x13\x57\x19\x1C\x19\x18\x01\x07\x09\x4F\x1E\x18\x0C\x17\x09\x0C\x03\x49\x6E\x55\x22\x18\x68\x69\x6E\x6F\x6D\x69"
"\x60\x73\x70\x15\x1A\x1B\x74\x3C\x24\x22\x3C\x69\x28\x2E\x25\x21\x27\x27\x60\x6C\x66\x35\x21\x2C\x14\x08\x0C\x18\x12\x13\x47\x5D\x07\x03\x14\x10\x02\x12\x54\x12\x18\x0A\x18\x01\x07\x0C\x1F\x4D\x06\x11\x09\x17\x03\x15\x17\x4B\x50\x6B\x20\x1A\x6E\x6F\x6C\x6D\x66\x63\x69\x71\x76\x14\x26\x34\x39\x23\x68"
"\x64\x6E\x2E\x3C\x3D\x62\x21\x35\x26\x7D\x67\x31\x35\x1E\x1A\x0C\x1C\x5E\x10\x0E\x5D\x00\x16\x19\x1F\x05\x03\x15\x19\x06\x45\x62\x59\x16\x57\x5C\x5D\x52\x57\x50\x51\x53\x47\x44\x30\x34\x28\x28\x3C\x3D\x36\x3A\x34\x37\x37\x70\x34\x24\x25\x3B\x27\x6A\x66\x68\x3F\x2B\x3D\x35\x6D\x25\x26\x2E\x24\x34\x2E"
"\x27\x7E\x5A\x18\x10\x1C\x1D\x14\x5C\x11\x1D\x14\x03\x5F\x7C\x47\x0C\x4D\x5A\x5B\x5F\x59\x5E\x58\x29\x4D\x42\x2E\x0F\x05\x13\x0B\x01\x45\x34\x34\x2C\x79\x38\x30\x29\x33\x36\x73\x7D\x71\x24\x32\x3D\x3B\x39\x3F\x29\x25\x22\x6F\x38\x25\x27\x63\x30\x33\x29\x20\x36\x24\x17\x55\x72\x49\x06\x47\x4C\x4D\x45"
"\x43\x45\x46\x46\x57\x54\x36\x05\x19\x1A\x1C\x1E\x1B\x4C\x0B\x0B\x0F\x05\x4E\x0F\x09\x17\x11\x3B\x37\x34\x79\x73\x7F\x2E\x38\x36\x3C\x27\x3F\x3A\x38\x35\x31\x65\x39\x2D\x20\x20\x3C\x38\x2C\x2E\x2F\x7B\x61\x25\x2F\x21\x26\x11\x5B\x1C\x10\x0D\x14\x52\x77\x78\x37\x35\x27\x3F\x34\x31\x26\x60\x28\x07\x0D"
"\x0B\x4F\x5D\x4D\x42\x43\x40\x41\x46\x23\x01\x13\x33\x38\x3D\x79\x30\x3A\x39\x39\x21\x73\x33\x3E\x38\x31\x3D\x32\x3F\x39\x29\x3D\x27\x20\x22\x6D\x6F\x63\x35\x31\x22\x26\x30\x20\x5A\x1F\x0A\x10\x08\x1A\x0E\x53\x78\x30\x1F\x15\x13\x57\x45\x45\x4A\x4B\x48\x49\x4E\x2C\x0D\x03\x0C\x0C\x14\x41\x15\x13\x05"
"\x17\x2E\x7B\x75\x79\x3A\x2D\x35\x2B\x37\x21\x70\x21\x24\x38\x36\x39\x2F\x26\x73\x69\x3C\x2A\x25\x23\x31\x37\x21\x2D\x2A\x67\x20\x37\x13\x0D\x1D\x0B\x52\x5F\x1F\x15\x17\x10\x1B\x7B\x56\x57\x54\x55\x4A\x4B\x48\x49\x4E\x4F\x4C\x4D\x0A\x02\x12\x05\x11\x06\x16\x00\x74\x51\x1B\x36\x3A\x3A\x7C\x6C\x6B\x73"
"\x70\x71\x76\x77\x06\x30\x2D\x22\x3B\x3D\x3C\x36\x6C\x24\x2C\x20\x2F\x2C\x36\x2B\x21\x31\x1F\x5B\x55\x59\x0B\x11\x15\x13\x01\x07\x11\x1D\x1A\x57\x15\x1B\x0E\x4B\x1A\x0C\x1D\x0C\x0D\x03\x42\x07\x05\x17\x0F\x04\x01\x4B\x50\x18\x37\x3D\x3B\x7F\x6E\x65\x72\x73\x70\x71\x76\x13\x26\x3C\x3C\x2E\x3A\x69\x20"
"\x20\x38\x6D\x2B\x2D\x33\x35\x27\x2B\x28\x20\x1E\x5B\x55\x59\x0D\x1A\x1D\x0F\x11\x1B\x50\x10\x03\x03\x1B\x18\x0B\x1F\x01\x0A\x0F\x03\x00\x14\x42\x05\x0F\x13\x46\x03\x16\x0C\x2C\x3E\x2A\x2A\x70\x55\x1F\x32\x36\x36\x70\x62\x67\x77\x74\x75\x6A\x6B\x1F\x3B\x21\x21\x2B\x6D\x26\x31\x29\x37\x23\x35\x64\x68"
"\x5A\x09\x1D\x10\x10\x0C\x08\x1C\x1E\x1F\x50\x05\x1E\x12\x54\x16\x05\x19\x1A\x0C\x0D\x1B\x4C\x09\x10\x0A\x16\x04\x14\x49\x6E\x26\x35\x3F\x3D\x79\x6A\x6C\x7C\x7D\x72\x73\x70\x15\x33\x21\x3D\x36\x2F\x6B\x3A\x2C\x3E\x20\x3E\x39\x27\x27\x60\x27\x27\x2E\x28\x30\x08\x1E\x58\x54\x5E\x17\x1D\x0F\x16\x04\x11"
"\x03\x13\x58\x10\x07\x03\x1D\x0D\x1B\x55\x4F\x18\x1F\x1B\x43\x01\x0F\x09\x13\x0C\x00\x28\x7B\x28\x36\x2C\x2B\x72\x57\x11\x3C\x34\x34\x76\x63\x61\x7A\x7E\x7C\x68\x69\x0A\x2A\x3A\x24\x21\x26\x60\x2F\x29\x33\x64\x26\x15\x15\x16\x1C\x1D\x0B\x19\x19\x52\x1C\x02\x51\x04\x12\x19\x1A\x1C\x0E\x0C\x49\x43\x4F"
"\x1E\x08\x01\x0C\x0E\x0F\x03\x04\x10\x4B\x50\x51\x16\x1C\x0A\x08\x13\x0F\x19\x59\x60\x29\x6E\x67\x64\x62\x7A\x7F\x7A\x7D\x6E\x6F\x1F\x28\x30\x35\x29\x22\x23\x67\x29\x2C\x09\x08\x11\x17\x19\x5F\x51\x5D\x00\x16\x19\x1F\x05\x03\x15\x19\x06\x4B\x1C\x01\x0B\x4F\x1F\x08\x10\x15\x09\x02\x03\x47\x4C\x32\x16"
"\x1A\x16\x76\x29\x2A\x3D\x28\x21\x36\x22\x27\x7F\x79\x5E\x65\x32\x73\x78\x79\x79\x7F\x7A\x09\x7B\x63\x60\x07\x2F\x35\x21\x32\x1B\x17\x14\x59\x0D\x1A\x0E\x0B\x1B\x10\x15\x51\x05\x03\x1B\x05\x1A\x0E\x0C\x49\x43\x4F\x1F\x19\x03\x11\x14\x41\x0F\x13\x44\x4D\x2E\x33\x31\x2A\x7E\x2F\x2E\x32\x35\x21\x31\x3C"
"\x6C\x5D\x74\x75\x6A\x6B\x68\x69\x6E\x6F\x6C\x6D\x62\x63\x33\x24\x34\x31\x2D\x26\x1F\x08\x55\x0A\x0A\x1E\x0E\x09\x52\x1E\x00\x02\x05\x01\x17\x5C\x44\x61\x2D\x3B\x3C\x20\x3E\x4D\x54\x56\x51\x4E\x50\x50\x5C\x4A\x6C\x62\x69\x79\x7E\x0F\x0C\x0D\x72\x30\x3F\x3F\x38\x32\x37\x21\x23\x24\x26\x69\x28\x2E\x25"
"\x21\x27\x27\x60\x6C\x66\x0E\x17\x15\x55\x18\x0A\x1C\x1A\x1A\x12\x09\x1B\x12\x1C\x02\x56\x1E\x07\x06\x1F\x0E\x46\x63\x39\x3C\x2D\x28\x23\x20\x23\x24\x35\x47\x4C\x54\x6A\x6B\x69\x6A\x77\x7F\x7C\x0D\x3D\x21\x24\x71\x34\x3B\x3B\x36\x21\x2E\x2C\x69\x63\x6F\x2F\x25\x27\x20\x2B\x61\x20\x2E\x36\x20\x0D\x1A"
"\x14\x15\x51\x1E\x12\x09\x1B\x05\x19\x03\x03\x04\x5A\x7F\x3D\x38\x29\x2C\x3A\x26\x21\x28\x26\x2C\x35\x35\x46\x4F\x55\x55\x6A\x6D\x68\x70\x7E\x7F\x1F\x32\x3C\x3D\x35\x32\x22\x3E\x3B\x3B\x6A\x3F\x21\x24\x2B\x20\x39\x39\x62\x6E\x60\x29\x29\x34\x30\x6A\x1C\x12\x0A\x1C\x09\x1E\x10\x11\x52\x03\x02\x1E\x14"
"\x1B\x11\x18\x44\x61\x62\x3A\x3A\x20\x3E\x2C\x25\x26\x6A\x51\x1E\x5F\x54\x55\x6D\x6B\x68\x6A\x6C\x7F\x7C\x08\x3C\x20\x25\x21\x26\x38\x26\x21\x2F\x2F\x68\x2F\x27\x23\x29\x6D\x31\x3A\x33\x35\x23\x2A\x64\x68\x5A\x1D\x17\x0B\x13\x1E\x08\x5D\x06\x1B\x15\x51\x00\x18\x18\x00\x07\x0E\x48\x08\x1E\x1F\x1E\x02"
"\x12\x11\x09\x00\x12\x02\x08\x1C\x74\x51\x68\x21\x66\x6F\x6C\x6A\x62\x63\x63\x64\x76\x77\x1A\x30\x3E\x3C\x27\x3B\x25\x6F\x3C\x2C\x36\x2B\x60\x2F\x29\x33\x64\x23\x15\x0E\x16\x1D\x5E\x52\x5C\x1E\x1A\x16\x13\x1A\x56\x04\x1C\x14\x18\x0E\x48\x07\x0F\x02\x09\x42\x04\x0A\x12\x04\x11\x06\x08\x09\x74\x51\x68"
"\x21\x66\x6F\x6C\x6A\x62\x63\x65\x66\x76\x77\x1D\x3B\x3C\x2A\x24\x20\x2A\x6F\x3C\x2C\x30\x22\x2D\x24\x32\x22\x36\x65\x57\x5B\x0A\x0C\x10\x5F\x1F\x15\x19\x17\x03\x1A\x4D\x57\x06\x10\x0E\x04\x48\x1D\x06\x0A\x4C\x02\x12\x06\x12\x00\x12\x0E\x0B\x0B\x74\x51\x68\x21\x66\x6F\x6C\x6A\x63\x12\x13\x62\x76\x77"
"\x02\x3A\x26\x3E\x25\x2C\x6E\x2B\x25\x3F\x36\x3A\x60\x6C\x66\x35\x31\x2B\x5A\x18\x10\x12\x1A\x0C\x17\x5D\x5D\x15\x50\x10\x18\x13\x54\x07\x0F\x09\x07\x06\x1A\x41\x66\x67\x24\x2A\x38\x41\x34\x22\x27\x2C\x0A\x1E\x0B\x53\x73\x7F\x1B\x38\x3C\x36\x22\x38\x35\x6D\x74\x27\x2F\x38\x3C\x28\x3C\x3B\x6C\x73\x62"
"\x36\x30\x25\x27\x33\x21\x65\x44\x5B\x0B\x1F\x1D\x5F\x42\x5D\x16\x1A\x03\x1C\x56\x49\x54\x07\x0F\x18\x1C\x06\x1C\x0A\x4C\x1D\x0D\x0A\x0E\x15\x46\x59\x44\x17\x3F\x28\x3D\x2D\x70\x55\x71\x7D\x06\x3B\x39\x22\x76\x27\x26\x3A\x2D\x39\x29\x24\x74\x6F\x6B\x3F\x27\x30\x25\x35\x6B\x30\x2D\x2B\x1E\x14\x0F\x0A"
"\x53\x0A\x0C\x19\x13\x07\x15\x56\x5A\x57\x53\x06\x0C\x08\x45\x1A\x0D\x0E\x02\x03\x0D\x14\x47\x4D\x46\x40\x00\x0C\x29\x36\x75\x2B\x3B\x2C\x28\x32\x20\x36\x77\x7D\x5C\x77\x74\x72\x24\x2E\x3C\x3E\x21\x3D\x27\x60\x30\x26\x33\x24\x32\x60\x68\x65\x5D\x1F\x11\x0A\x15\x52\x1F\x15\x17\x10\x1B\x56\x56\x16\x1A"
"\x11\x4A\x4C\x0C\x06\x0D\x1C\x41\x1E\x07\x02\x12\x02\x0E\x47\x58\x06\x35\x3F\x3D\x67\x79\x71"
);

MAN(faq, "Frequently Asked Questions",
"\x0B\x61\x78\x0E\x36\x26\x7C\x34\x21\x73\x3D\x28\x76\x07\x17\x75\x39\x27\x27\x3E\x71\x45\x0D\x77\x62\x16\x33\x34\x27\x2B\x28\x3C\x40\x5B\x1E\x0C\x12\x13\x5C\x19\x1B\x00\x1B\x5D\x56\x03\x1B\x1A\x4A\x06\x09\x07\x17\x4F\x1F\x19\x03\x11\x14\x14\x16\x47\x05\x15\x2A\x28\x74\x79\x3C\x3E\x3F\x36\x35\x21\x3F"
"\x24\x38\x33\x74\x31\x25\x3C\x26\x25\x21\x2E\x28\x3E\x6E\x49\x60\x61\x66\x28\x31\x31\x1E\x1A\x0C\x1C\x1A\x5F\x18\x0F\x1B\x05\x15\x03\x05\x57\x1B\x07\x4A\x06\x09\x05\x19\x0E\x1E\x08\x4C\x43\x33\x04\x03\x47\x09\x04\x34\x2E\x39\x35\x73\x2F\x39\x2F\x34\x3C\x22\x3C\x37\x39\x37\x30\x6A\x2D\x27\x3B\x6E\x3B"
"\x24\x28\x62\x33\x2C\x20\x28\x69\x4E\x4F\x2B\x41\x58\x3D\x11\x5F\x35\x5D\x1C\x16\x15\x15\x56\x16\x1A\x55\x0B\x05\x1C\x00\x18\x06\x1E\x18\x11\x5C\x6A\x20\x5C\x47\x29\x0C\x39\x29\x37\x2A\x31\x39\x28\x7D\x16\x36\x36\x34\x38\x33\x31\x27\x6A\x22\x3B\x69\x2F\x6F\x2A\x38\x2E\x2F\x60\x20\x28\x33\x2D\x33\x13"
"\x09\x0D\x0A\x52\x5F\x1E\x08\x1B\x1F\x04\x51\x1F\x19\x5A\x55\x21\x0E\x0D\x19\x4E\x1D\x09\x0C\x0E\x4E\x14\x08\x0B\x02\x6E\x45\x7A\x7B\x28\x2B\x31\x2B\x39\x3E\x26\x3A\x3F\x3F\x76\x38\x3A\x75\x2B\x25\x2C\x69\x37\x20\x39\x6D\x23\x31\x25\x61\x25\x28\x32\x20\x08\x1E\x1C\x57\x5E\x3E\x18\x19\x52\x10\x1F\x1C"
"\x1B\x18\x1A\x55\x19\x0E\x06\x1A\x0B\x41\x66\x67\x33\x59\x40\x32\x0E\x08\x11\x09\x3E\x7B\x11\x79\x2B\x2F\x38\x3C\x26\x36\x70\x06\x3F\x39\x30\x3A\x3D\x38\x77\x43\x0F\x75\x6C\x14\x27\x30\x60\x6C\x66\x34\x21\x26\x0F\x09\x11\x0D\x07\x5F\x09\x0D\x16\x12\x04\x14\x05\x57\x04\x14\x1E\x08\x00\x49\x0F\x0C\x18"
"\x04\x14\x06\x0C\x18\x46\x02\x1C\x15\x36\x34\x31\x2D\x3B\x3B\x7C\x35\x3D\x3F\x35\x22\x78\x77\x04\x34\x3F\x38\x2D\x69\x21\x21\x20\x34\x48\x63\x60\x61\x20\x28\x36\x65\x1B\x5B\x1F\x16\x11\x1B\x5C\x0F\x17\x12\x03\x1E\x18\x57\x15\x1B\x0E\x4B\x1A\x0C\x1D\x1A\x01\x08\x42\x12\x15\x08\x05\x0C\x08\x1C\x74\x51"
"\x52\x08\x64\x7F\x14\x32\x25\x73\x34\x3E\x76\x1E\x74\x38\x2B\x20\x2D\x69\x23\x36\x6C\x1D\x01\x63\x22\x2E\x29\x33\x64\x23\x1B\x08\x0C\x1C\x0C\x40\x76\x3C\x48\x53\x34\x18\x05\x16\x16\x19\x0F\x4B\x1D\x07\x1B\x1C\x09\x09\x42\x10\x14\x00\x14\x13\x11\x15\x7A\x3A\x28\x29\x2D\x73\x7C\x3B\x20\x36\x35\x71\x32"
"\x3E\x27\x3E\x6A\x38\x38\x28\x2D\x2A\x60\x6D\x37\x33\x24\x20\x32\x22\x64\x21\x08\x12\x0E\x1C\x0C\x0C\x50\x5D\x19\x16\x15\x01\x7C\x57\x54\x55\x1E\x03\x0D\x49\x3D\x3C\x28\x4D\x0A\x06\x01\x0D\x12\x0F\x1D\x4B\x7A\x08\x3D\x3C\x7E\x32\x3D\x33\x27\x32\x3C\x7C\x26\x32\x26\x33\x25\x39\x25\x28\x20\x2C\x29\x63"
"\x48\x49\x11\x7B\x66\x0A\x3D\x65\x2A\x38\x58\x1F\x0C\x10\x06\x18\x5C\x53\x27\x19\x17\x03\x54\x1B\x05\x1C\x57\x63\x2F\x55\x4C\x3A\x03\x0A\x14\x41\x07\x47\x09\x0C\x34\x2E\x2C\x3C\x7E\x77\x38\x34\x21\x38\x70\x25\x3E\x25\x35\x26\x22\x74\x61\x67\x6E\x1B\x24\x28\x2C\x63\x03\x35\x34\x2B\x6F\x04\x16\x0F\x53"
"\x3D\x1B\x13\x5C\x43\x52\x27\x11\x02\x1D\x57\x39\x14\x04\x0A\x0F\x0C\x1C\x4F\x52\x4D\x07\x0D\x04\x6B\x46\x47\x44\x11\x32\x3E\x78\x2C\x30\x2D\x39\x2E\x22\x3C\x3E\x22\x3F\x21\x31\x75\x2B\x3B\x38\x67\x6E\x09\x23\x3F\x21\x26\x6D\x33\x23\x34\x30\x24\x08\x0F\x58\x16\x10\x13\x05\x5D\x13\x00\x50\x10\x56\x1B"
"\x15\x06\x1E\x4B\x1A\x0C\x1D\x00\x1E\x19\x4C\x69\x6A\x30\x5C\x47\x2C\x0A\x2D\x7B\x3C\x36\x7E\x16\x7C\x29\x33\x38\x35\x71\x37\x77\x27\x36\x38\x2E\x2D\x27\x3D\x27\x23\x39\x7D\x49\x01\x7B\x66\x10\x2D\x2B\x51\x28\x10\x10\x18\x0B\x57\x2E\x52\x5B\x03\x14\x1A\x12\x17\x01\x4A\x0A\x1A\x0C\x0F\x46\x40\x4D\x35"
"\x0A\x0E\x4A\x36\x15\x0D\x0B\x2E\x7B\x70\x3F\x2B\x33\x30\x74\x7E\x73\x3F\x23\x76\x23\x3C\x3C\x39\x6B\x38\x3B\x21\x28\x3E\x2C\x2F\x64\x33\x4B\x66\x67\x64\x62\x09\x18\x0A\x1C\x1B\x11\x0F\x15\x1D\x07\x57\x51\x5E\x11\x01\x19\x06\x4B\x1B\x0A\x1C\x0A\x09\x03\x42\x21\x2D\x31\x4F\x49\x6E\x6F\x0B\x61\x78\x1A"
"\x3F\x31\x7C\x14\x72\x34\x35\x25\x76\x00\x3D\x3B\x2E\x24\x3F\x3A\x6E\x7E\x7D\x6D\x2D\x2D\x60\x2C\x3F\x67\x2B\x29\x1E\x5B\x28\x3A\x41\x75\x3D\x47\x52\x30\x18\x14\x15\x1C\x54\x01\x02\x0E\x48\x39\x2D\x4F\x24\x08\x03\x0F\x14\x09\x46\x24\x0C\x00\x39\x30\x78\x38\x2E\x2F\x7C\x72\x72\x21\x35\x20\x23\x3E\x26"
"\x30\x27\x2E\x26\x3D\x3D\x6F\x64\x19\x12\x0E\x60\x73\x68\x77\x68\x65\x4E\x5B\x3F\x3B\x5E\x2D\x3D\x30\x5E\x79\x50\x51\x56\x41\x40\x58\x08\x02\x1C\x49\x2D\x3F\x39\x44\x4C\x43\x35\x0F\x15\x12\x14\x15\x35\x29\x2C\x3C\x3A\x7F\x34\x3C\x20\x37\x27\x30\x24\x32\x74\x3C\x24\x38\x3C\x28\x22\x23\x3F\x6D\x27\x3B"
"\x29\x32\x32\x67\x26\x30\x0E\x5B\x19\x0B\x1B\x5F\x12\x12\x06\x79\x50\x51\x56\x05\x11\x16\x05\x06\x05\x0C\x00\x0B\x09\x09\x42\x02\x0E\x05\x46\x00\x01\x11\x7A\x35\x37\x79\x39\x2A\x3D\x2F\x33\x3D\x24\x34\x33\x33\x74\x20\x3A\x2F\x29\x3D\x2B\x3C\x62\x47\x48\x12\x7A\x61\x0E\x28\x33\x65\x1E\x14\x58\x30\x5E"
"\x12\x13\x0B\x17\x53\x16\x18\x1A\x12\x07\x55\x1E\x04\x48\x08\x4E\x01\x09\x1A\x42\x33\x23\x5E\x6C\x26\x5E\x45\x15\x35\x3D\x1D\x2C\x36\x2A\x38\x72\x20\x29\x3F\x35\x77\x7F\x75\x1D\x22\x26\x2D\x21\x38\x3F\x6D\x20\x22\x23\x2A\x33\x37\x68\x65\x1B\x15\x58\x1C\x06\x0B\x19\x0F\x1C\x12\x1C\x51\x12\x05\x1D\x03"
"\x0F\x47\x48\x06\x1C\x4F\x0D\x4D\x37\x30\x22\x41\x15\x13\x0D\x06\x31\x75\x52\x79\x7E\x7F\x08\x35\x3B\x20\x70\x21\x24\x38\x33\x27\x2B\x26\x68\x2A\x2F\x21\x6C\x37\x2B\x33\x60\x27\x29\x2B\x20\x20\x08\x08\x58\x1F\x11\x0D\x5C\x09\x00\x12\x1E\x02\x06\x18\x06\x01\x44\x61\x62\x38\x54\x4F\x3B\x05\x1B\x43\x04"
"\x0E\x03\x14\x44\x32\x33\x35\x3C\x36\x29\x2C\x7C\x2F\x37\x20\x24\x30\x24\x23\x74\x37\x33\x6B\x21\x3D\x3D\x2A\x20\x2B\x7D\x49\x01\x7B\x66\x12\x34\x21\x1B\x0F\x1D\x0A\x5E\x57\x1F\x15\x17\x10\x1B\x51\x17\x14\x00\x1C\x1C\x0E\x48\x01\x01\x1A\x1E\x1E\x4B\x4F\x40\x02\x14\x06\x17\x0D\x3F\x28\x78\x71\x1B\x29"
"\x39\x33\x26\x73\x06\x38\x33\x20\x31\x27\x63\x67\x68\x21\x2B\x2E\x38\x61\x48\x63\x60\x61\x35\x24\x2C\x20\x1E\x0E\x14\x1C\x1A\x5F\x08\x1C\x01\x18\x03\x5D\x56\x18\x06\x55\x0B\x4B\x0E\x08\x07\x03\x05\x03\x05\x43\x04\x08\x15\x0C\x4A\x45\x19\x33\x3D\x3A\x35\x7F\x7B\x2F\x37\x3F\x39\x30\x34\x3E\x38\x3C\x3E"
"\x32\x65\x24\x21\x21\x25\x39\x2D\x31\x67\x6F\x4C\x4D\x15\x7F\x5A\x2C\x10\x18\x0A\x5F\x0F\x15\x1D\x06\x1C\x15\x56\x3E\x54\x17\x0B\x08\x03\x49\x1B\x1F\x53\x67\x23\x59\x40\x25\x09\x04\x11\x08\x3F\x35\x2C\x2A\x72\x7F\x2C\x34\x31\x27\x25\x23\x33\x24\x78\x75\x27\x3E\x3B\x20\x2D\x63\x6C\x3B\x2B\x27\x25\x2E"
"\x35\x6B\x64\x21\x15\x0C\x16\x15\x11\x1E\x18\x0E\x5E\x53\x00\x10\x05\x04\x03\x1A\x18\x0F\x1B\x45\x4E\x08\x0D\x00\x07\x69\x40\x41\x46\x14\x05\x13\x3F\x28\x74\x79\x33\x3E\x35\x31\x7C\x73\x03\x34\x33\x77\x39\x34\x24\x3E\x29\x25\x63\x2D\x2D\x2E\x29\x36\x30\x61\x20\x28\x36\x65\x0E\x13\x1D\x59\x4D\x52\x4E"
"\x50\x43\x53\x02\x04\x1A\x12\x5A\x7F\x60\x3A\x52\x49\x27\x1C\x4C\x04\x16\x43\x13\x00\x00\x02\x44\x11\x35\x7B\x2D\x2A\x3B\x7F\x7B\x3E\x3E\x36\x31\x3F\x33\x25\x73\x75\x2B\x3B\x38\x3A\x71\x45\x0D\x77\x62\x0E\x2F\x32\x32\x2B\x3D\x65\x0F\x15\x16\x1C\x1D\x1A\x0F\x0E\x13\x01\x09\x4A\x56\x15\x01\x1C\x06\x1F"
"\x45\x00\x00\x4F\x0F\x01\x07\x02\x0E\x14\x16\x47\x4C\x08\x3B\x35\x2D\x38\x32\x72\x2F\x29\x3D\x21\x31\x36\x33\x7E\x74\x3C\x39\x6B\x3B\x28\x28\x2A\x3E\x63\x48\x63\x60\x61\x14\x22\x23\x2C\x09\x0F\x0A\x00\x5E\x1C\x10\x18\x13\x1D\x15\x03\x05\x57\x11\x06\x1A\x0E\x0B\x00\x0F\x03\x00\x14\x42\x00\x01\x0F\x46"
"\x05\x16\x00\x3B\x30\x78\x2D\x36\x36\x32\x3A\x21\x7D\x5A\x5B\x07\x6D\x74\x1D\x25\x3C\x68\x2D\x21\x6F\x05\x6D\x31\x37\x2F\x31\x66\x37\x2B\x35\x0F\x0B\x0B\x56\x1F\x1B\x0F\x5D\x1B\x1D\x50\x26\x1F\x19\x10\x1A\x1D\x18\x57\x63\x2F\x55\x4C\x29\x0B\x10\x01\x03\x0A\x02\x44\x0B\x35\x2F\x31\x3F\x37\x3C\x3D\x29"
"\x3B\x3C\x3E\x71\x25\x22\x33\x32\x2F\x38\x3C\x20\x21\x21\x3F\x6D\x6A\x10\x25\x35\x32\x2E\x2A\x22\x09\x5B\x46\x59\x30\x10\x08\x14\x14\x1A\x13\x10\x02\x1E\x1B\x1B\x19\x4B\x56\x63\x4E\x4F\x4C\x3E\x17\x04\x07\x04\x15\x13\x0D\x0A\x34\x28\x71\x75\x7E\x2B\x29\x2F\x3C\x73\x3F\x37\x30\x77\x73\x01\x23\x3B\x3B"
"\x69\x2F\x21\x28\x6D\x31\x36\x27\x26\x23\x34\x30\x2C\x15\x15\x0B\x5E\x5E\x16\x12\x5D\x01\x16\x04\x05\x1F\x19\x13\x06\x46\x4B\x09\x07\x0A\x4F\x0F\x05\x07\x00\x0B\x6B\x46\x47\x44\x0C\x34\x28\x2C\x38\x32\x33\x39\x39\x72\x32\x20\x21\x25\x77\x7C\x21\x22\x22\x3B\x69\x3E\x3D\x23\x2A\x30\x22\x2D\x7B\x66\x60"
"\x25\x35\x0A\x56\x0D\x17\x17\x11\x0F\x09\x13\x1F\x1C\x56\x5F\x59\x7E\x7F\x3B\x51\x48\x3E\x06\x16\x4C\x04\x11\x43\x0D\x18\x46\x30\x0D\x48\x1C\x32\x78\x2A\x32\x30\x2B\x62\x58\x12\x6A\x71\x05\x3E\x33\x3B\x2B\x27\x64\x69\x3C\x20\x39\x39\x27\x31\x60\x31\x2A\x26\x27\x20\x17\x1E\x16\x0D\x52\x5F\x13\x09\x1A"
"\x16\x02\x51\x12\x12\x02\x1C\x09\x0E\x1B\x45\x4E\x0B\x1E\x04\x14\x06\x12\x4D\x46\x08\x16\x45\x13\x08\x08\x77\x7E\x0C\x39\x38\x58\x73\x70\x71\x3B\x36\x3A\x20\x2B\x27\x65\x3E\x27\x29\x25\x63\x48\x49\x11\x7B\x66\x04\x25\x2B\x5A\x32\x58\x0B\x1B\x12\x13\x0B\x17\x53\x3D\x18\x15\x05\x1B\x06\x05\x0D\x1C\x49"
"\x2B\x0B\x0B\x08\x42\x4C\x40\x2E\x08\x02\x20\x17\x33\x2D\x3D\x66\x54\x1E\x66\x7D\x17\x37\x37\x34\x76\x3E\x27\x75\x2E\x2E\x2D\x39\x22\x36\x6C\x24\x2C\x37\x25\x26\x34\x26\x30\x20\x1E\x5B\x1A\x0C\x0A\x5F\x1F\x1C\x1C\x53\x12\x14\x56\x02\x1A\x1C\x04\x18\x1C\x08\x02\x03\x09\x09\x42\x0C\x0E\x41\x15\x08\x09"
"\x00\x7A\x39\x2D\x30\x32\x3B\x2F\x66\x58\x73\x70\x71\x19\x39\x31\x11\x38\x22\x3E\x2C\x6E\x2C\x2D\x23\x62\x21\x25\x61\x33\x29\x28\x2C\x14\x10\x1D\x1D\x50\x5F\x32\x12\x06\x53\x02\x14\x15\x18\x19\x18\x0F\x05\x0C\x0C\x0A\x4F\x41\x4D\x00\x16\x14\x41\x0E\x06\x16\x08\x36\x3E\x2B\x2A\x7E\x2B\x33\x7D\x36\x3A"
"\x23\x30\x34\x3B\x31\x7B\x40\x41\x19\x73\x6E\x07\x23\x3A\x62\x27\x2F\x61\x0F\x67\x36\x20\x09\x1E\x0C\x59\x13\x06\x5C\x0D\x13\x00\x03\x06\x19\x05\x10\x4A\x60\x2A\x52\x49\x3D\x0A\x09\x4D\x0F\x02\x0E\x14\x07\x0B\x49\x04\x39\x38\x37\x2C\x30\x2B\x2F\x7D\x7F\x73\x3F\x3F\x3A\x3E\x3A\x30\x6A\x2D\x27\x3B\x6E"
"\x02\x25\x2E\x30\x2C\x33\x2E\x20\x33\x64\x24\x19\x18\x17\x0C\x10\x0B\x0F\x51\x52\x00\x15\x12\x03\x05\x1D\x01\x13\x61\x48\x49\x4E\x1E\x19\x08\x11\x17\x09\x0E\x08\x14\x44\x0A\x28\x7B\x2A\x3C\x3D\x30\x2A\x38\x20\x2A\x70\x37\x39\x25\x74\x39\x25\x28\x29\x25\x6E\x2E\x2F\x2E\x2D\x36\x2E\x35\x35\x69\x4E\x4F"
"\x2B\x41\x58\x2E\x16\x1E\x08\x5D\x1B\x00\x50\x05\x1E\x1E\x07\x55\x1A\x19\x07\x0E\x1C\x0E\x01\x4D\x4A\x34\x09\x0F\x02\x08\x13\x16\x19\x34\x36\x2D\x2C\x30\x30\x74\x6D\x59\x11\x6B\x76\x16\x3A\x75\x2B\x27\x24\x64\x27\x21\x61\x22\x2C\x26\x60\x2D\x27\x32\x2A\x26\x12\x1E\x0A\x43\x5E\x4E\x4C\x4D\x42\x58\x50"
"\x12\x19\x1A\x19\x14\x04\x0F\x1B\x49\x0F\x0C\x1E\x02\x11\x10\x40\x52\x57\x4C\x44\x06\x3B\x2F\x3D\x3E\x31\x2D\x35\x38\x21\x7F\x70\x26\x3F\x23\x3C\x5F\x6A\x6B\x68\x0E\x1B\x06\x60\x6D\x16\x16\x09\x61\x27\x29\x20\x65\x39\x37\x31\x59\x13\x10\x18\x18\x01\x5D\x50\x25\x1E\x1E\x07\x55\x07\x0A\x06\x1C\x0F\x03"
"\x4C\x04\x11\x43\x0F\x0F\x03\x47\x0B\x03\x7A\x32\x2C\x2A\x7E\x3C\x3D\x29\x37\x34\x3F\x23\x3F\x32\x27\x7B\x40\x41\x19\x73\x6E\x02\x23\x3F\x27\x63\x31\x34\x23\x34\x30\x2C\x15\x15\x0B\x46\x74\x3E\x46\x5D\x55\x17\x1F\x12\x05\x5A\x07\x10\x0B\x19\x0B\x01\x4E\x53\x1D\x18\x07\x10\x14\x08\x09\x09\x5A\x42\x7A"
"\x28\x3D\x38\x2C\x3C\x34\x38\x21\x73\x24\x39\x33\x77\x3B\x33\x2C\x22\x2B\x20\x2F\x23\x6C\x00\x2B\x20\x32\x2E\x35\x28\x22\x31\x5A\x1F\x17\x1A\x0D\x53\x5C\x1C\x1C\x17\x7A\x51\x56\x57\x53\x58\x47\x0A\x1B\x02\x4E\x4D\x50\x1C\x17\x06\x13\x15\x0F\x08\x0A\x5B\x78\x7C\x78\x3F\x37\x31\x38\x2E\x72\x27\x38\x34"
"\x76\x3A\x35\x21\x29\x23\x21\x27\x29\x6F\x2F\x22\x2F\x2E\x21\x2F\x22\x67\x2D\x2B\x5A\x0F\x10\x10\x0D\x5F\x0C\x0F\x1D\x14\x02\x10\x1B\x59"
);

MAN(tips, "Tips & Tricks",
"\x0A\x09\x17\x1D\x0B\x1C\x08\x14\x04\x1A\x04\x08\x5C\x7A\x74\x02\x23\x25\x63\x1F\x6E\x2C\x20\x24\x32\x21\x2F\x20\x34\x23\x64\x2D\x13\x08\x0C\x16\x0C\x06\x46\x5D\x02\x1A\x1E\x51\x10\x05\x11\x04\x1F\x0E\x06\x1D\x02\x16\x4C\x1D\x03\x10\x14\x04\x02\x47\x0D\x11\x3F\x36\x2B\x77\x54\x72\x7C\x0A\x3B\x3D\x7B"
"\x7F\x76\x32\x39\x3A\x20\x22\x68\x39\x2F\x21\x29\x21\x62\x6B\x37\x28\x32\x2F\x64\x2E\x1B\x14\x15\x16\x14\x16\x5C\x1C\x1C\x17\x50\x02\x0F\x1A\x16\x1A\x06\x18\x41\x47\x64\x42\x4C\x3A\x0B\x0D\x4B\x32\x0E\x0E\x02\x11\x71\x08\x78\x2A\x30\x36\x2C\x2D\x3B\x3D\x37\x6A\x76\x23\x3C\x30\x6A\x38\x26\x20\x3E\x6F"
"\x2D\x38\x36\x2C\x6D\x32\x27\x31\x21\x36\x5A\x1A\x16\x1D\x5E\x1C\x1D\x13\x52\x3C\x33\x23\x56\x03\x11\x0D\x1E\x45\x62\x44\x4E\x3C\x02\x0C\x12\x43\x0C\x00\x1F\x08\x11\x11\x29\x7B\x70\x0E\x37\x31\x77\x07\x72\x3A\x3E\x71\x01\x3E\x3A\x64\x7B\x62\x72\x69\x2F\x3D\x3E\x2C\x2C\x24\x25\x61\x31\x2E\x2A\x21\x15"
"\x0C\x0B\x59\x17\x11\x5C\x12\x1C\x16\x50\x1C\x19\x01\x11\x5B\x60\x46\x48\x2F\x01\x0C\x19\x1E\x42\x02\x13\x12\x0F\x14\x10\x45\x72\x0C\x31\x37\x6F\x6E\x7C\x1B\x3D\x30\x25\x22\x7F\x6D\x74\x3D\x23\x2F\x2D\x69\x20\x20\x38\x24\x24\x2A\x23\x20\x32\x2E\x2B\x2B\x09\x5B\x0F\x11\x17\x13\x19\x5D\x05\x1C\x02\x1A"
"\x1F\x19\x13\x5B\x60\x46\x48\x2D\x0B\x1C\x07\x19\x0D\x13\x40\x50\x46\x5A\x44\x12\x35\x29\x33\x75\x7E\x1B\x39\x2E\x39\x27\x3F\x21\x76\x65\x74\x68\x6A\x26\x2D\x2D\x27\x2E\x6C\x65\x2F\x22\x2E\x34\x27\x2B\x69\x21\x1F\x08\x13\x0D\x11\x0F\x0F\x54\x5C\x79\x5D\x51\x3A\x1E\x02\x10\x4A\x08\x09\x19\x1A\x06\x03"
"\x03\x11\x43\x06\x0E\x14\x47\x05\x0B\x23\x7B\x39\x2C\x3A\x36\x33\x66\x72\x25\x3F\x38\x35\x32\x74\x21\x33\x3B\x21\x27\x29\x6F\x64\x1A\x2B\x2D\x6B\x09\x6F\x67\x22\x2A\x08\x5B\x0C\x1C\x06\x0B\x52\x77\x78\x36\x28\x21\x3A\x38\x26\x30\x38\x61\x45\x49\x2D\x1B\x1E\x01\x49\x30\x08\x08\x00\x13\x4F\x2B\x7A\x35"
"\x3D\x2E\x7E\x39\x33\x31\x36\x36\x22\x6A\x76\x16\x38\x21\x61\x1E\x38\x69\x29\x20\x29\x3E\x62\x36\x30\x61\x29\x29\x21\x65\x16\x1E\x0E\x1C\x12\x51\x76\x50\x52\x20\x18\x18\x10\x03\x5F\x07\x03\x0C\x00\x1D\x43\x0C\x00\x04\x01\x08\x40\x00\x46\x01\x0B\x09\x3E\x3E\x2A\x79\x60\x7F\x7B\x12\x22\x36\x3E\x71\x3F"
"\x39\x74\x21\x2F\x39\x25\x20\x20\x2E\x20\x6D\x2A\x26\x32\x24\x61\x69\x4E\x68\x5A\x2C\x11\x17\x4F\x4E\x46\x5D\x1F\x1A\x14\x15\x1A\x12\x59\x16\x06\x02\x0B\x02\x4E\x0E\x4C\x0B\x0D\x0F\x04\x04\x14\x47\x0B\x15\x3F\x35\x2B\x79\x37\x2B\x7C\x34\x3C\x73\x31\x71\x38\x32\x23\x75\x3E\x2A\x2A\x67\x44\x62\x6C\x03"
"\x23\x2E\x25\x61\x20\x2E\x28\x20\x09\x5B\x0F\x10\x0A\x17\x5C\x11\x17\x12\x14\x18\x18\x10\x54\x1B\x1F\x06\x0A\x0C\x1C\x1C\x4C\x45\x52\x52\x3F\x4D\x46\x57\x56\x3A\x73\x7B\x2C\x36\x7E\x2C\x33\x2F\x26\x73\x3E\x30\x22\x22\x26\x34\x26\x27\x31\x67\x44\x45\x1F\x14\x11\x17\x05\x0C\x4C\x6A\x64\x12\x13\x15\x53"
"\x21\x5E\x41\x5C\x29\x17\x01\x1D\x18\x18\x16\x18\x55\x42\x2A\x0C\x04\x07\x01\x45\x4D\x0B\x10\x40\x15\x0E\x02\x44\x03\x3B\x28\x2C\x3C\x2D\x2B\x7C\x2A\x33\x2A\x70\x25\x39\x77\x26\x20\x24\x6B\x29\x2D\x23\x26\x22\x6D\x21\x2C\x2D\x2C\x27\x29\x20\x36\x54\x71\x55\x59\x29\x16\x12\x56\x22\x12\x05\x02\x13\x57"
"\x07\x1D\x05\x1C\x1B\x49\x1D\x16\x1F\x19\x07\x0E\x40\x08\x08\x01\x0B\x45\x33\x35\x2B\x2D\x3F\x31\x28\x31\x2B\x7D\x5A\x7C\x76\x16\x38\x21\x61\x1F\x29\x2B\x6E\x64\x6C\x1E\x2A\x2A\x26\x35\x66\x7A\x64\x27\x1B\x18\x13\x0E\x1F\x0D\x18\x0E\x5C\x79\x5D\x51\x32\x18\x01\x17\x06\x0E\x45\x0A\x02\x06\x0F\x06\x42"
"\x02\x40\x16\x0F\x09\x00\x0A\x2D\x7B\x2C\x30\x2A\x33\x39\x7D\x30\x32\x22\x71\x6B\x77\x39\x34\x32\x22\x25\x20\x34\x2A\x63\x3F\x27\x30\x34\x2E\x34\x22\x6A\x4F\x57\x5B\x2A\x10\x19\x17\x08\x50\x11\x1F\x19\x12\x1D\x57\x00\x1D\x0F\x4B\x1C\x08\x1D\x04\x0E\x0C\x10\x43\x03\x0D\x09\x04\x0F\x45\x64\x7B\x19\x3D"
"\x34\x2A\x2F\x29\x72\x37\x31\x25\x33\x78\x20\x3C\x27\x2E\x68\x64\x6E\x20\x3E\x6D\x36\x2B\x29\x32\x66\x37\x36\x2A\x1D\x09\x19\x14\x59\x0C\x76\x5D\x52\x54\x14\x10\x02\x12\x59\x06\x13\x05\x0B\x4E\x4E\x09\x03\x1F\x42\x2D\x34\x31\x46\x14\x1D\x0B\x39\x75\x52\x53\x10\x1A\x08\x0A\x1D\x01\x1B\x71\x70\x77\x04"
"\x1A\x1D\x0E\x1A\x43\x63\x6F\x25\x3D\x21\x2C\x2E\x27\x2F\x20\x64\x6A\x1C\x17\x0D\x0A\x16\x1B\x12\x0E\x52\x12\x16\x05\x13\x05\x54\x31\x24\x38\x48\x19\x1C\x00\x0E\x01\x07\x0E\x13\x41\x4E\x13\x0C\x0C\x29\x7B\x28\x2B\x31\x38\x2E\x3C\x3F\x69\x70\x76\x30\x3B\x21\x26\x22\x66\x2C\x27\x3D\x68\x65\x63\x48\x6E"
"\x60\x13\x23\x34\x30\x24\x08\x0F\x58\x0D\x16\x1A\x5C\x0F\x1D\x06\x04\x14\x04\x57\x16\x10\x0C\x04\x1A\x0C\x4E\x0C\x0D\x01\x0E\x0A\x0E\x06\x46\x14\x11\x15\x2A\x34\x2A\x2D\x7E\x72\x7C\x3B\x3B\x2B\x35\x22\x76\x3F\x35\x39\x2C\x6B\x27\x2F\x6E\x2E\x20\x21\x48\x63\x60\x28\x28\x33\x21\x37\x14\x1E\x0C\x59\x17"
"\x0C\x0F\x08\x17\x00\x5E\x7B\x5B\x57\x36\x14\x1E\x1F\x0D\x1B\x17\x4F\x1E\x08\x12\x0C\x12\x15\x46\x4F\x14\x0A\x2D\x3E\x2A\x3A\x38\x38\x7C\x72\x30\x32\x24\x25\x33\x25\x2D\x27\x2F\x3B\x27\x3B\x3A\x66\x6C\x3E\x2A\x2C\x37\x32\x66\x25\x25\x31\x0E\x1E\x0A\x00\x5E\x17\x19\x1C\x1E\x07\x18\x5F\x7C\x5A\x54\x47"
"\x5A\x4E\x45\x51\x5E\x4A\x4C\x0E\x0A\x02\x12\x06\x03\x47\x16\x04\x34\x3C\x3D\x79\x37\x2C\x7C\x29\x3A\x36\x70\x39\x33\x36\x38\x21\x22\x22\x2D\x3A\x3A\x6F\x2A\x22\x30\x63\x2D\x2E\x35\x33\x64\x29\x1B\x0B\x0C\x16\x0E\x5F\x1E\x1C\x06\x07\x15\x03\x1F\x12\x07\x5B\x60\x61\x3B\x2C\x2D\x3A\x3E\x24\x36\x3A\x40"
"\x30\x33\x2E\x27\x2E\x13\x1E\x0B\x53\x73\x7F\x0B\x34\x3C\x78\x1C\x71\x34\x32\x32\x3A\x38\x2E\x68\x25\x2B\x2E\x3A\x24\x2C\x24\x60\x35\x2E\x22\x64\x21\x1F\x08\x13\x59\x53\x5F\x1D\x11\x05\x12\x09\x02\x58\x7D\x59\x55\x2F\x05\x09\x0B\x02\x0A\x4C\x4A\x24\x0A\x0E\x05\x46\x0A\x1D\x45\x3E\x3E\x2E\x30\x3D\x3A"
"\x7B\x7D\x7F\x73\x39\x25\x76\x34\x35\x3B\x6A\x27\x27\x2A\x2F\x3B\x29\x6D\x23\x63\x33\x35\x29\x2B\x21\x2B\x5A\x17\x19\x09\x0A\x10\x0C\x53\x78\x5E\x50\x3E\x18\x12\x30\x07\x03\x1D\x0D\x49\x49\x29\x05\x01\x07\x10\x40\x0E\x08\x4A\x00\x00\x37\x3A\x36\x3D\x79\x65\x7C\x2F\x3B\x34\x38\x25\x7B\x34\x38\x3C\x29"
"\x20\x68\x77\x6E\x09\x3E\x28\x27\x63\x35\x31\x66\x34\x34\x24\x19\x1E\x56\x73\x53\x5F\x2C\x15\x1B\x00\x18\x18\x18\x10\x54\x16\x02\x0E\x0B\x02\x54\x4F\x04\x02\x14\x06\x12\x41\x0A\x0E\x0A\x0E\x29\x7B\x3A\x3C\x38\x30\x2E\x38\x72\x30\x3C\x38\x35\x3C\x3D\x3B\x2D\x67\x68\x2A\x26\x2A\x2F\x26\x62\x37\x28\x24"
"\x66\x23\x2B\x28\x1B\x12\x16\x57\x74\x75\x34\x34\x36\x37\x35\x3F\x56\x30\x31\x38\x39\x61\x45\x49\x03\x1C\x0F\x02\x0C\x05\x09\x06\x46\x59\x44\x27\x35\x34\x2C\x79\x60\x7F\x1D\x39\x24\x32\x3E\x32\x33\x33\x74\x3A\x3A\x3F\x21\x26\x20\x3C\x6C\x73\x62\x2D\x35\x2C\x24\x22\x36\x65\x15\x1D\x58\x09\x0C\x10\x1F"
"\x18\x01\x00\x1F\x03\x05\x57\x5C\x07\x0B\x19\x0D\x05\x17\x65\x4C\x4D\x0C\x06\x05\x05\x03\x03\x44\x48\x7A\x0C\x31\x37\x3A\x30\x2B\x2E\x72\x3E\x31\x3F\x37\x30\x31\x26\x6A\x3F\x20\x20\x3D\x66\x62\x47\x6F\x63\x07\x2E\x22\x67\x09\x2A\x1E\x1E\x58\x1F\x11\x13\x18\x18\x00\x53\x58\x05\x1E\x1E\x07\x55\x1A\x19"
"\x07\x0E\x1C\x0E\x01\x57\x42\x44\x07\x0E\x02\x4A\x09\x0A\x3E\x3E\x7F\x70\x64\x7F\x3D\x31\x3E\x73\x23\x34\x22\x23\x3D\x3B\x2D\x38\x68\x20\x20\x6F\x23\x23\x27\x49\x60\x61\x20\x28\x28\x21\x1F\x09\x56\x73\x53\x5F\x2F\x13\x1B\x03\x00\x18\x18\x10\x54\x21\x05\x04\x04\x49\x1A\x0A\x14\x19\x42\x02\x03\x15\x0F"
"\x08\x0A\x16\x60\x7B\x3B\x36\x2E\x26\x7C\x29\x37\x2B\x24\x71\x30\x25\x3B\x38\x6A\x38\x2B\x3B\x2B\x2A\x22\x3E\x2A\x2C\x34\x32\x68\x4D\x69\x65\x2E\x13\x1D\x59\x29\x16\x12\x19\x1D\x04\x03\x51\x22\x12\x06\x18\x03\x05\x09\x05\x4E\x07\x0D\x1E\x42\x17\x01\x03\x15\x4B\x44\x15\x3B\x35\x3D\x2A\x7E\x77\x1D\x31"
"\x26\x78\x03\x39\x3F\x31\x20\x7E\x0E\x62\x68\x28\x20\x2B\x6C\x3D\x30\x2C\x26\x28\x2A\x22\x37\x6B\x70\x56\x58\x2E\x17\x11\x57\x3E\x06\x01\x1C\x5A\x25\x1F\x1D\x13\x1E\x40\x2A\x49\x1C\x0A\x1F\x19\x03\x11\x14\x12\x46\x13\x0C\x00\x7A\x3C\x2A\x38\x2E\x37\x35\x3E\x21\x73\x34\x23\x3F\x21\x31\x27\x6A\x63\x2A"
"\x25\x2F\x2C\x27\x6D\x31\x20\x32\x24\x23\x29\x64\x37\x1F\x08\x1B\x0C\x1B\x56\x52\x77\x5F\x53\x24\x10\x05\x1C\x54\x38\x0B\x05\x09\x0E\x0B\x1D\x4C\x53\x42\x33\x05\x13\x00\x08\x16\x08\x3B\x35\x3B\x3C\x64\x7F\x2E\x34\x35\x3B\x24\x7C\x35\x3B\x3D\x36\x21\x6B\x3C\x21\x2B\x6F\x2B\x3F\x23\x33\x28\x61\x78\x67"
"\x63\x13\x13\x1E\x0F\x5E\x5E\x19\x13\x0F\x52\x1E\x1F\x03\x13\x7D\x54\x55\x0E\x0E\x1C\x08\x07\x03\x57\x4D\x45\x2C\x10\x04\x08\x47\x36\x00\x29\x34\x2D\x2B\x3D\x3A\x7C\x10\x3D\x3D\x39\x25\x39\x25\x73\x75\x2C\x24\x3A\x69\x2A\x2A\x29\x3D\x62\x22\x2E\x20\x2A\x3E\x37\x2C\x09\x55\x72\x73\x33\x3E\x35\x33\x26"
"\x36\x3E\x30\x38\x34\x31\x55\x38\x24\x3D\x3D\x27\x21\x29\x4D\x4A\x32\x35\x20\x34\x33\x21\x37\x16\x02\x71\x53\x6F\x76\x7C\x0F\x27\x3D\x70\x06\x3F\x39\x30\x3A\x3D\x38\x68\x1C\x3E\x2B\x2D\x39\x27\x63\x26\x34\x2A\x2B\x3D\x6B\x70\x49\x51\x59\x2A\x17\x15\x0E\x52\x03\x02\x1E\x11\x05\x15\x18\x50\x4B\x4F\x1A"
"\x08\x0C\x41\x1E\x01\x02\x0E\x0F\x09\x10\x43\x49\x7A\x7C\x3C\x30\x2D\x32\x71\x3E\x3A\x36\x33\x3A\x71\x7B\x74\x72\x2E\x22\x3B\x22\x63\x2C\x24\x28\x21\x28\x67\x6F\x4C\x74\x6D\x65\x5D\x1F\x11\x0A\x15\x52\x1F\x11\x17\x12\x1E\x04\x06\x50\x54\x5E\x4A\x4C\x0B\x05\x0B\x0E\x02\x40\x16\x06\x0D\x11\x41\x49\x6E"
"\x51\x73\x7B\x0A\x3C\x28\x36\x39\x2A\x72\x74\x23\x25\x37\x25\x20\x20\x3A\x66\x29\x39\x3E\x3C\x6B\x63\x48\x76\x69\x61\x05\x2F\x21\x26\x11\x5B\x5F\x1B\x1F\x0B\x08\x18\x00\x0A\x5D\x03\x13\x07\x1B\x07\x1E\x4C\x48\x41\x02\x0E\x1C\x19\x0D\x13\x13\x48\x46\x06\x0A\x01\x7A\x7C\x3C\x2B\x37\x29\x39\x70\x3B\x3D"
"\x36\x3E\x71\x77\x7C\x06\x07\x0A\x1A\x1D\x67\x61\x46\x7B\x6B\x63\x03\x33\x23\x26\x30\x20\x5A\x1A\x58\x0B\x1B\x0C\x08\x12\x00\x16\x50\x01\x19\x1E\x1A\x01\x4A\x43\x4F\x1A\x17\x1C\x18\x08\x0F\x4E\x12\x04\x15\x13\x0B\x17\x3F\x76\x3B\x2B\x3B\x3E\x28\x38\x75\x7A\x7E\x5B\x61\x7E\x74\x03\x2F\x39\x21\x2F\x37"
"\x6F\x35\x22\x37\x31\x60\x23\x27\x24\x2F\x30\x0A\x08\x58\x18\x1D\x0B\x09\x1C\x1E\x1F\x09\x51\x04\x12\x07\x01\x05\x19\x0D\x49\x46\x02\x0D\x03\x17\x02\x0C\x4C\x04\x06\x07\x0E\x2F\x2B\x71\x77\x54\x55\x08\x15\x1B\x00\x70\x01\x04\x18\x13\x07\x0B\x06\x6F\x1A\x6E\x1B\x1E\x04\x01\x08\x13\x4B\x6B\x67\x63\x68"
"\x57\x1A\x0B\x12\x5E\x5D\x0D\x08\x17\x00\x04\x18\x19\x19\x56\x52\x4A\x0D\x01\x07\x0A\x1C\x4C\x19\x0A\x06\x40\x13\x0F\x00\x0C\x11\x7A\x38\x37\x34\x33\x3E\x32\x39\x72\x35\x3F\x23\x76\x20\x3C\x34\x3E\x6B\x31\x26\x3B\x6F\x3B\x2C\x2C\x37\x60\x35\x29\x67\x20\x2A\x54\x71\x55\x59\x59\x52\x51\x1B\x1E\x1C\x11"
"\x05\x51\x57\x04\x00\x1E\x18\x48\x08\x4E\x1C\x09\x0C\x10\x00\x08\x00\x04\x0B\x01\x45\x39\x34\x35\x34\x3F\x31\x38\x7D\x30\x32\x3C\x3D\x76\x38\x3A\x75\x33\x24\x3D\x3B\x6E\x2B\x29\x3E\x29\x37\x2F\x31\x68\x4D\x69\x65\x5D\x1F\x17\x1A\x0D\x52\x0F\x18\x13\x01\x13\x19\x51\x57\x18\x1A\x05\x00\x1B\x49\x1B\x1F"
"\x4C\x0C\x0C\x1A\x14\x09\x0F\x09\x03\x45\x33\x35\x78\x2D\x36\x3A\x7C\x32\x34\x35\x39\x32\x3F\x36\x38\x75\x07\x22\x2B\x3B\x21\x3C\x23\x2B\x36\x63\x24\x2E\x25\x34\x6A"
);

static void OnDocs_win11(const char *a) { (void)a; DocsTopic("windows/", "windows 11 overview"); }
static void OnDocs_settings(const char *a) { (void)a; DocsTopic("windows/settings/", "windows settings app"); }
static void OnDocs_update(const char *a) { (void)a; DocsTopic("windows/windows-update/", "windows update"); }
static void OnDocs_security(const char *a) { (void)a; DocsTopic("windows/security/", "windows security"); }
static void OnDocs_privacy(const char *a) { (void)a; DocsTopic("windows/privacy/", "windows privacy"); }
static void OnDocs_defender(const char *a) { (void)a; DocsTopic("windows/security/operating-system-security/virus-and-threat-protection/", "microsoft defender"); }
static void OnDocs_wsl(const char *a) { (void)a; DocsTopic("windows/wsl/", "windows subsystem for linux wsl"); }
static void OnDocs_hyperv(const char *a) { (void)a; DocsTopic("virtualization/hyper-v-on-windows/", "hyper-v on windows"); }
static void OnDocs_terminal(const char *a) { (void)a; DocsTopic("windows/terminal/", "windows terminal"); }
static void OnDocs_powershell(const char *a) { (void)a; DocsTopic("powershell/", "powershell documentation"); }
static void OnDocs_release_health(const char *a) { (void)a; DocsTopic("windows/release-health/", "windows release health"); }
static void OnDocs_dev_drive(const char *a) { (void)a; DocsTopic("windows/dev-drive/", "dev drive"); }
static void OnDocs_recovery(const char *a) { (void)a; DocsTopic("windows/recovery/", "recovery options in windows"); }
static void OnDocs_storage(const char *a) { (void)a; DocsTopic("windows/storage/", "storage in windows"); }
static void OnDocs_backup(const char *a) { (void)a; DocsTopic("windows/backup/", "windows backup"); }
static void OnDocs_performance(const char *a) { (void)a; DocsTopic("windows/performance/", "tips to improve pc performance windows"); }
static void OnDocs_troubleshoot(const char *a) { (void)a; DocsTopic("windows/troubleshoot/", "troubleshooting windows"); }

static const Command g_Manual[] = {
    {"manual-overview", "Windows manual: complete overview", OnManual_overview, 0},
    {"manual-desktop", "Windows manual: desktop, Start menu & taskbar", OnManual_desktop, 0},
    {"manual-settings", "Windows manual: Settings app tour", OnManual_settings, 0},
    {"manual-shortcuts", "Windows manual: keyboard shortcuts master list", OnManual_shortcuts, 0},
    {"manual-cmd", "Windows manual: Command Prompt reference", OnManual_cmd, 0},
    {"manual-powershell", "Windows manual: PowerShell reference", OnManual_powershell, 0},
    {"manual-files", "Windows manual: File Explorer guide", OnManual_files, 0},
    {"manual-network", "Windows manual: networking guide", OnManual_network, 0},
    {"manual-wifi", "Windows manual: Wi-Fi guide", OnManual_wifi, 0},
    {"manual-security", "Windows manual: security guide", OnManual_security, 0},
    {"manual-update", "Windows manual: Windows Update guide", OnManual_update, 0},
    {"manual-accounts", "Windows manual: user accounts guide", OnManual_accounts, 0},
    {"manual-backup", "Windows manual: backup guide", OnManual_backup, 0},
    {"manual-recovery", "Windows manual: recovery & repair", OnManual_recovery, 0},
    {"manual-storage", "Windows manual: storage & disks", OnManual_storage, 0},
    {"manual-performance", "Windows manual: performance guide", OnManual_performance, 0},
    {"manual-gaming", "Windows manual: gaming guide", OnManual_gaming, 0},
    {"manual-troubleshoot", "Windows manual: troubleshooting guide", OnManual_troubleshoot, 0},
    {"manual-devices", "Windows manual: devices & drivers", OnManual_devices, 0},
    {"manual-boot", "Windows manual: boot, UEFI & BIOS", OnManual_boot, 0},
    {"manual-power", "Windows manual: power & battery", OnManual_power, 0},
    {"manual-registry", "Windows manual: registry guide", OnManual_registry, 0},
    {"manual-services", "Windows manual: services guide", OnManual_services, 0},
    {"manual-scheduler", "Windows manual: Task Scheduler guide", OnManual_scheduler, 0},
    {"manual-desktops", "Windows manual: virtual desktops", OnManual_desktops, 0},
    {"manual-accessibility", "Windows manual: accessibility guide", OnManual_accessibility, 0},
    {"manual-clean-install", "Windows manual: clean install guide", OnManual_clean_install, 0},
    {"manual-defender", "Windows manual: Defender deep dive", OnManual_defender, 0},
    {"manual-wsl", "Windows manual: WSL (Linux on Windows)", OnManual_wsl, 0},
    {"manual-glossary", "Windows manual: glossary of terms", OnManual_glossary, 0},
    {"manual-errors", "Windows manual: common error codes", OnManual_errors, 0},
    {"manual-faq", "Windows manual: frequently asked questions", OnManual_faq, 0},
    {"manual-tips", "Windows manual: tips & tricks", OnManual_tips, 0},
    {"docs-search", "Official Microsoft docs: search (needs query)", DocsSearchCmd, 1},
    {"docs-open", "Open an official Microsoft docs page in browser", DocsOpenCmd, 1},
    {"docs-article", "Read any official Microsoft docs article here", DocsArticleCmd, 1},
    {"docs-win32-api", "Official Win32 API reference (needs function name)", DocsWin32ApiCmd, 1},
    {"docs-powershell-cmd", "Official PowerShell cmdlet docs (needs cmdlet name)", DocsPsCmd, 1},
    {"docs-win11", "Official docs: Windows overview", OnDocs_win11, 0},
    {"docs-settings", "Official docs: Settings app", OnDocs_settings, 0},
    {"docs-update", "Official docs: Windows Update", OnDocs_update, 0},
    {"docs-security", "Official docs: Windows security", OnDocs_security, 0},
    {"docs-privacy", "Official docs: privacy", OnDocs_privacy, 0},
    {"docs-defender", "Official docs: Microsoft Defender", OnDocs_defender, 0},
    {"docs-wsl", "Official docs: Windows Subsystem for Linux", OnDocs_wsl, 0},
    {"docs-hyperv", "Official docs: Hyper-V", OnDocs_hyperv, 0},
    {"docs-terminal", "Official docs: Windows Terminal", OnDocs_terminal, 0},
    {"docs-powershell", "Official docs: PowerShell", OnDocs_powershell, 0},
    {"docs-release-health", "Official docs: Windows release health", OnDocs_release_health, 0},
    {"docs-dev-drive", "Official docs: Dev Drive", OnDocs_dev_drive, 0},
    {"docs-recovery", "Official docs: recovery options", OnDocs_recovery, 0},
    {"docs-storage", "Official docs: storage", OnDocs_storage, 0},
    {"docs-backup", "Official docs: backup", OnDocs_backup, 0},
    {"docs-performance", "Official docs: PC performance tips", OnDocs_performance, 0},
    {"docs-troubleshoot", "Official docs: troubleshooting", OnDocs_troubleshoot, 0},
};

static int CheckPortOpen(const char *host, int port, char *errOut, int errSize) {
    errOut[0] = 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        strncpy(errOut, "Winsock init failed", errSize - 1);
        return -1;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        strncpy(errOut, "socket() failed", errSize - 1);
        WSACleanup();
        return -1;
    }
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons((u_short)port);
    sa.sin_addr.s_addr = inet_addr(host);
    if (sa.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *he = gethostbyname(host);
        if (!he) {
            strncpy(errOut, "Cannot resolve host", errSize - 1);
            closesocket(s);
            WSACleanup();
            return -1;
        }
        memcpy(&sa.sin_addr, he->h_addr, (size_t)he->h_length);
    }
    int r = connect(s, (struct sockaddr *)&sa, sizeof sa);
    if (r == 0) {
        closesocket(s);
        WSACleanup();
        return 1;
    }
    if (WSAGetLastError() != WSAEWOULDBLOCK) {
        strncpy(errOut, "Connection refused", errSize - 1);
        closesocket(s);
        WSACleanup();
        return 0;
    }
    fd_set wf;
    FD_ZERO(&wf);
    FD_SET(s, &wf);
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    r = select(0, NULL, &wf, NULL, &tv);
    int open = 0;
    if (r > 0) {
        int so = 0, sl = sizeof so;
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so, &sl);
        open = (so == 0);
    }
    closesocket(s);
    WSACleanup();
    return open;
}

static const char *CTimeStr(FILETIME ft) {
    static char buf[32];
    FILETIME lft;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&ft, &lft);
    FileTimeToSystemTime(&lft, &st);
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

/* ===================== Extended toolset: Network Tools ===================== */

static void OnPingHost(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: ping-host <host-or-ip>");
        ReportAdd("Example: ping-host google.com");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "ping -n 4 -w 2000 %s", arg);
    RunCmd(cmd, 1);
}

static void OnTracertHost(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: tracert-host <host-or-ip>");
        ReportAdd("Example: tracert-host google.com");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "tracert -d -h 20 %s", arg);
    RunCmd(cmd, 1);
}

static void OnNslookupHost(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: nslookup-host <hostname>");
        ReportAdd("Example: nslookup-host google.com");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "nslookup %s", arg);
    RunCmd(cmd, 1);
}

static void OnNetstatPort(const char *arg) {
    if (!arg || !*arg) {
        RunCmd("netstat -ano", 1);
        return;
    }
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: netstat-port <port-number>");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "netstat -ano | findstr \":%s\"", arg);
    RunCmd(cmd, 1);
}

static void OnCheckPort(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: check-port <host>|<port>");
        ReportAdd("Example: check-port google.com|443");
        ShowReport();
        return;
    }
    int port = atoi(parts[1]);
    char err[128];
    int open = CheckPortOpen(parts[0], port, err, sizeof err);
    ReportClear();
    if (open < 0) ReportAdd("Error: %s", err);
    else ReportAdd("Host %s port %d : %s", parts[0], port, open ? "OPEN" : "closed");
    ReportAdd("(TCP connect test, 3 second timeout)");
    ShowReport();
}

static void OnPublicIp(const char *arg) {
    (void)arg;
    char ip[512];
    ReportClear();
    if (HttpGetText("api.ipify.org", "/", ip, sizeof ip)) {
        TrimSpaces(ip);
        ReportAdd("Public IPv4 address: %s", ip);
        ReportAdd("(source: api.ipify.org)");
    } else if (HttpGetText("api64.ipify.org", "/", ip, sizeof ip)) {
        TrimSpaces(ip);
        ReportAdd("Public IP address: %s", ip);
        ReportAdd("(source: api64.ipify.org)");
    } else if (HttpGetText("icanhazip.com", "/", ip, sizeof ip)) {
        TrimSpaces(ip);
        ReportAdd("Public IP address: %s", ip);
        ReportAdd("(source: icanhazip.com)");
    } else if (HttpGetText("v4.ident.me", "/", ip, sizeof ip)) {
        TrimSpaces(ip);
        ReportAdd("Public IPv4 address: %s", ip);
        ReportAdd("(source: v4.ident.me)");
    } else {
        ReportAdd("Failed to get public IP. Check your internet connection.");
    }
    ShowReport();
}

static void OnWifiScan(const char *arg) {
    (void)arg;
    RunCmd("netsh wlan show networks mode=bssid", 1);
}

static void OnWifiProfiles(const char *arg) {
    (void)arg;
    RunCmd("netsh wlan show profiles", 1);
}

static void OnWifiConnect(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: wifi-connect <profile-name>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "netsh wlan connect name=\"%s\"", arg);
    RunCmd(cmd, 1);
}

static void OnWifiPassword(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: wifi-password <profile-name>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "netsh wlan show profile name=\"%s\" key=clear", arg);
    RunCmd(cmd, 1);
}

static void OnWifiDisconnect(const char *arg) {
    (void)arg;
    RunCmd("netsh wlan disconnect", 1);
}

static void OnAdapterList(const char *arg) {
    (void)arg;
    RunCmd("ipconfig /all", 1);
}

static void OnAdapterEnable(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: adapter-enable <adapter-name>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "netsh interface set interface name=\"%s\" admin=enable", arg);
    RunElevatedCmd(cmd);
}

static void OnAdapterDisable(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: adapter-disable <adapter-name>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "netsh interface set interface name=\"%s\" admin=disable", arg);
    RunElevatedCmd(cmd);
}

static void SetDnsAll(const char *dns1, const char *dns2) {
    char ps[1200];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Get-NetAdapter -Physical | ForEach-Object { Set-DnsClientServerAddress -InterfaceIndex $_.ifIndex -ServerAddresses @('%s','%s') }\"",
             dns1, dns2);
    RunElevatedCmd(ps);
}

static void OnDnsGoogle(const char *arg) {
    (void)arg;
    SetDnsAll("8.8.8.8", "8.8.4.4");
}

static void OnDnsCloudflare(const char *arg) {
    (void)arg;
    SetDnsAll("1.1.1.1", "1.0.0.1");
}

static void OnDnsAutomatic(const char *arg) {
    (void)arg;
    char ps[1200];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Get-NetAdapter -Physical | ForEach-Object { Set-DnsClientServerAddress -InterfaceIndex $_.ifIndex -ResetServerAddresses }\"");
    RunElevatedCmd(ps);
}

static void OnFirewallOn(const char *arg) {
    (void)arg;
    RunElevatedCmd("netsh advfirewall set allprofiles state on");
}

static void OnFirewallOff(const char *arg) {
    (void)arg;
    RunElevatedCmd("netsh advfirewall set allprofiles state off");
}

static void OnFirewallAllowPort(const char *arg) {
    char parts[3][256];
    int n = SplitArgs(arg, parts, 3);
    if (n < 1 || !parts[0][0]) {
        ReportClear();
        ReportAdd("Usage: firewall-allow-port <port>|<tcp|udp>|<name>");
        ReportAdd("Example: firewall-allow-port 8080|tcp|Web Server");
        ShowReport();
        return;
    }
    const char *proto = (n >= 2 && parts[1][0]) ? parts[1] : "tcp";
    const char *name = (n >= 3 && parts[2][0]) ? parts[2] : "WindowsControl Port";
    char cmd[800];
    snprintf(cmd, sizeof cmd,
             "netsh advfirewall firewall add rule name=\"%s\" dir=in action=allow protocol=%s localport=%s",
             name, proto, parts[0]);
    RunElevatedCmd(cmd);
}

static void OnFirewallBlockPort(const char *arg) {
    char parts[3][256];
    int n = SplitArgs(arg, parts, 3);
    if (n < 1 || !parts[0][0]) {
        ReportClear();
        ReportAdd("Usage: firewall-block-port <port>|<tcp|udp>|<name>");
        ReportAdd("Example: firewall-block-port 23|tcp|Telnet");
        ShowReport();
        return;
    }
    const char *proto = (n >= 2 && parts[1][0]) ? parts[1] : "tcp";
    const char *name = (n >= 3 && parts[2][0]) ? parts[2] : "WindowsControl Blocked Port";
    char cmd[800];
    snprintf(cmd, sizeof cmd,
             "netsh advfirewall firewall add rule name=\"%s\" dir=in action=block protocol=%s localport=%s",
             name, proto, parts[0]);
    RunElevatedCmd(cmd);
}

static void OnFirewallRemoveRule(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: firewall-remove-rule <rule-name>");
        ShowReport();
        return;
    }
    char cmd[800];
    snprintf(cmd, sizeof cmd, "netsh advfirewall firewall delete rule name=\"%s\"", arg);
    RunElevatedCmd(cmd);
}

static void OnHostsView(const char *arg) {
    (void)arg;
    RunCmd("type \"%WINDIR%\\System32\\drivers\\etc\\hosts\"", 1);
}

static void OnHostsBlock(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: hosts-block <hostname>");
        ReportAdd("Example: hosts-block ads.example.com");
        ShowReport();
        return;
    }
    char ps[1200];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Add-Content -Path $env:windir\\System32\\drivers\\etc\\hosts -Value ('0.0.0.0 ' + '%s') -Encoding ASCII\"",
             arg);
    RunElevatedCmd(ps);
}

/* ===================== Extended toolset: System Tweaks ===================== */

#define TWEAK(nm, desc, root, sub, val, on, off) \
    static void On##nm##On(const char *arg) { (void)arg; TweakApply(root, sub, val, on, desc); } \
    static void On##nm##Off(const char *arg) { (void)arg; TweakApply(root, sub, val, off, desc " (off)"); }

#define TW_TP "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"
#define TW_EA "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"
#define TW_EG "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
#define TW_PW "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power"

TWEAK(LightMode, "light theme (apps)", HKEY_CURRENT_USER, TW_TP, "AppsUseLightTheme", 1, 0)
TWEAK(SystemLightMode, "light theme (system)", HKEY_CURRENT_USER, TW_TP, "SystemUsesLightTheme", 1, 0)
TWEAK(AccentOnTitlebar, "accent color on title bars", HKEY_CURRENT_USER, TW_TP, "ColorPrevalence", 1, 0)
TWEAK(Transparency, "transparency effects", HKEY_CURRENT_USER, TW_TP, "EnableTransparency", 1, 0)
TWEAK(ShowFileExtensions, "show file extensions", HKEY_CURRENT_USER, TW_EA, "HideFileExt", 0, 1)
TWEAK(ShowHiddenFiles, "show hidden files", HKEY_CURRENT_USER, TW_EA, "Hidden", 2, 1)
TWEAK(ShowSuperHidden, "show super hidden files", HKEY_CURRENT_USER, TW_EA, "ShowSuperHidden", 1, 0)
TWEAK(ShowCheckBoxes, "show selection check boxes", HKEY_CURRENT_USER, TW_EA, "CheckBoxSelect", 1, 0)
TWEAK(ConfirmFileDelete, "confirm file delete", HKEY_CURRENT_USER, TW_EA, "ConfirmFileDelete", 1, 0)
TWEAK(HideEmptyDrives, "hide empty drives", HKEY_CURRENT_USER, TW_EA, "HideDrivesWithNoMedia", 1, 0)
TWEAK(ExpandToCurrentFolder, "expand to current folder in navigation pane", HKEY_CURRENT_USER, TW_EA, "NavPaneExpandToCurrentFolder", 1, 0)
TWEAK(ShowAllFolders, "show all folders in navigation pane", HKEY_CURRENT_USER, TW_EA, "NavPaneShowAllFolders", 1, 0)
TWEAK(SyncNotifications, "sync provider notifications", HKEY_CURRENT_USER, TW_EA, "ShowSyncProviderNotifications", 1, 0)
TWEAK(TaskbarSmallIcons, "small taskbar icons", HKEY_CURRENT_USER, TW_EA, "TaskbarSmallIcons", 1, 0)
TWEAK(ShowSecondsInClock, "seconds in taskbar clock", HKEY_CURRENT_USER, TW_EA, "ShowSecondsInSystemClock", 1, 0)
TWEAK(TaskbarAllDisplays, "taskbar on all displays", HKEY_CURRENT_USER, TW_EG, "MMTaskbarEnabled", 1, 0)
TWEAK(GameDvr, "Game DVR (game bar)", HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", 1, 0)
TWEAK(BackgroundRecording, "background recording", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", "AppCaptureEnabled", 1, 0)
TWEAK(ToastNotifications, "app notifications (toasts)", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications", "ToastEnabled", 1, 0)
TWEAK(AdvertisingId, "advertising ID", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", "Enabled", 0, 1)
TWEAK(WidgetsNews, "widgets news & interests feed", HKEY_CURRENT_USER, TW_EG, "EnableActivityFeed", 0, 1)
TWEAK(FastStartup, "fast startup", HKEY_LOCAL_MACHINE, TW_PW, "HiberbootEnabled", 1, 0)
TWEAK(Telemetry, "diagnostic data (telemetry)", HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry", 3, 0)
TWEAK(AutoRestart, "automatic restart on system crash", HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\CrashControl", "AutoReboot", 1, 0)

static void OnClock24(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sTimeFormat", "HH:mm:ss");
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sShortTime", "HH:mm");
    RefreshIntl();
    Notify("Applied: 24-hour clock");
}

static void OnClock12(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sTimeFormat", "h:mm:ss tt");
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sShortTime", "h:mm tt");
    RefreshIntl();
    Notify("Applied: 12-hour clock");
}

static void OnDateUs(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sShortDate", "M/d/yyyy");
    RefreshIntl();
    Notify("Applied: US date format (M/d/yyyy)");
}

static void OnDateEu(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\International", "sShortDate", "dd/MM/yyyy");
    RefreshIntl();
    Notify("Applied: European date format (dd/MM/yyyy)");
}

static void OnMouseSpeedSet(const char *arg) {
    int v = arg && *arg ? atoi(arg) : 10;
    if (v < 1) v = 1;
    if (v > 20) v = 20;
    SystemParametersInfoA(SPI_SETMOUSESPEED, 0, (PVOID)(INT_PTR)v, 0);
    ReportClear();
    ReportAdd("Mouse pointer speed set to %d (range 1-20)", v);
    ShowReport();
}

static void OnMouseDblClick(const char *arg) {
    int ms = arg && *arg ? atoi(arg) : 500;
    if (ms < 100) ms = 100;
    if (ms > 5000) ms = 5000;
    SystemParametersInfoA(SPI_SETDOUBLECLICKTIME, (UINT)ms, NULL, 0);
    ReportClear();
    ReportAdd("Double-click speed set to %d ms", ms);
    ShowReport();
}

static void OnKeyDelay(const char *arg) {
    int v = arg && *arg ? atoi(arg) : 1;
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    char s[8];
    sprintf(s, "%d", v);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Keyboard", "KeyboardDelay", s);
    ReportClear();
    ReportAdd("Keyboard repeat delay set to %d (0-3)", v);
    ShowReport();
}

static void OnKeyRate(const char *arg) {
    int v = arg && *arg ? atoi(arg) : 31;
    if (v < 0) v = 0;
    if (v > 31) v = 31;
    char s[8];
    sprintf(s, "%d", v);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Keyboard", "KeyboardSpeed", s);
    ReportClear();
    ReportAdd("Keyboard repeat rate set to %d (0-31)", v);
    ShowReport();
}

static void OnScreenSaverTimeout(const char *arg) {
    int sec = arg && *arg ? atoi(arg) : 600;
    if (sec < 10) sec = 10;
    SystemParametersInfoA(SPI_SETSCREENSAVETIMEOUT, (UINT)sec, NULL, 0);
    ReportClear();
    ReportAdd("Screen saver timeout set to %d seconds", sec);
    ShowReport();
}

static void OnScreenSaverSecureOn(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Desktop", "ScreenSaverIsSecure", "1");
    Notify("Applied: screen saver requires sign-in");
}

static void OnScreenSaverSecureOff(const char *arg) {
    (void)arg;
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Desktop", "ScreenSaverIsSecure", "0");
    Notify("Applied: screen saver does not require sign-in");
}

static void OnDisplayTimeout(const char *arg) {
    int min = arg && *arg ? atoi(arg) : 10;
    if (min < 0) min = 0;
    char cmd[400];
    snprintf(cmd, sizeof cmd, "powercfg /change monitor-timeout-ac %d & powercfg /change monitor-timeout-dc %d", min, min);
    RunCmd(cmd, 0);
    ReportClear();
    ReportAdd("Display timeout set to %d minutes (on battery too)", min);
    ShowReport();
}

static void OnSleepTimeout(const char *arg) {
    int min = arg && *arg ? atoi(arg) : 30;
    if (min < 0) min = 0;
    char cmd[400];
    snprintf(cmd, sizeof cmd, "powercfg /change standby-timeout-ac %d & powercfg /change standby-timeout-dc %d", min, min);
    RunCmd(cmd, 0);
    ReportClear();
    ReportAdd("Sleep timeout set to %d minutes (on battery too)", min);
    ShowReport();
}

static void OnHibernationOn(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /hibernate on");
}

static void OnHibernationOff(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /hibernate off");
}

static void OnUacLevelSet(const char *arg) {
    int v = arg && *arg ? atoi(arg) : 2;
    DWORD dword;
    switch (v) {
        case 0: dword = 0; break;
        case 1: dword = 1; break;
        case 3: dword = 2; break;
        default: dword = 5; v = 2; break;
    }
    TweakApply(HKEY_LOCAL_MACHINE,
               "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
               "ConsentPromptBehaviorAdmin", dword, "UAC level");
    ReportClear();
    ReportAdd("UAC level set to %d (0=never, 1=always, 2=default, 3=no secure desktop)", v);
    ReportAdd("Note: this requires an administrator; sign out/in to apply.");
    ShowReport();
}

static void OnTweaksStatus(const char *arg) {
    (void)arg;
    DWORD v;
    int f;
    ReportClear();
    f = ReadRegDword(HKEY_CURRENT_USER, TW_TP, "AppsUseLightTheme", &v);
    ReportAdd("Light theme (apps)          : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_TP, "SystemUsesLightTheme", &v);
    ReportAdd("Light theme (system)        : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_TP, "ColorPrevalence", &v);
    ReportAdd("Accent on title bars        : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_TP, "EnableTransparency", &v);
    ReportAdd("Transparency effects        : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "HideFileExt", &v);
    ReportAdd("Show file extensions        : %s", f ? (v ? "off" : "ON") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "Hidden", &v);
    ReportAdd("Show hidden files           : %s", f ? (v == 2 ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "ShowSuperHidden", &v);
    ReportAdd("Show super hidden files     : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "CheckBoxSelect", &v);
    ReportAdd("Selection check boxes       : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "ConfirmFileDelete", &v);
    ReportAdd("Confirm file delete         : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarSmallIcons", &v);
    ReportAdd("Small taskbar icons         : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "ShowSecondsInSystemClock", &v);
    ReportAdd("Seconds in taskbar clock    : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "System\\GameConfigStore", "GameDVR_Enabled", &v);
    ReportAdd("Game DVR                     : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications", "ToastEnabled", &v);
    ReportAdd("App notifications (toasts)  : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_LOCAL_MACHINE, TW_PW, "HiberbootEnabled", &v);
    ReportAdd("Fast startup                : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", "AllowTelemetry", &v);
    ReportAdd("Diagnostic data (telemetry) : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\CrashControl", "AutoReboot", &v);
    ReportAdd("Auto restart on crash       : %s", f ? (v ? "ON" : "off") : "n/a");
    char s[64];
    f = ReadRegStr(HKEY_CURRENT_USER, "Control Panel\\International", "sTimeFormat", s, sizeof s);
    ReportAdd("24-hour clock               : %s", f && s[0] == 'H' ? "ON" : "off");
    f = ReadRegStr(HKEY_CURRENT_USER, "Control Panel\\International", "sShortDate", s, sizeof s);
    ReportAdd("Date format                 : %s", f ? s : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarAutoHide", &v);
    ReportAdd("Auto-hide taskbar           : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarLockAll", &v);
    ReportAdd("Taskbar locked             : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "SearchboxTaskbarMode", &v);
    ReportAdd("Taskbar search box         : %s", f ? (v == 2 ? "hidden" : "shown") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "TaskViewButtonEnabled", &v);
    ReportAdd("Task View button           : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarDa", &v);
    ReportAdd("Widgets button             : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Software\\Microsoft\\GameBar", "AutoGameModeEnabled", &v);
    ReportAdd("Game Mode                  : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", "MinAnimate", &v);
    ReportAdd("Window animations          : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Control Panel\\Desktop", "DragFullWindows", &v);
    ReportAdd("Drag full windows          : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{20D04FE0-3AEA-1069-A2D8-08002B30309D}", &v);
    ReportAdd("This PC desktop icon       : %s", f ? (v ? "hidden" : "shown") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{645FF040-5081-101B-9F08-00AA002F954E}", &v);
    ReportAdd("Recycle Bin desktop icon   : %s", f ? (v ? "hidden" : "shown") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\SnapAssist", "Enabled", &v);
    ReportAdd("Window snapping            : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "Start_TrackDocs", &v);
    ReportAdd("Start recent items         : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EA, "Start_TrackProgs", &v);
    ReportAdd("Start recent apps          : %s", f ? (v ? "ON" : "off") : "n/a");
    f = ReadRegDword(HKEY_CURRENT_USER, TW_EG, "EnableAutoTray", &v);
    ReportAdd("All tray icons shown       : %s", f ? (v ? "ON" : "off") : "n/a");
    ShowReport();
}

/* ===================== Extended toolset 2: Power Plans & Battery ===================== */

static void OnBatteryStatus(const char *arg) {
    (void)arg;
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) {
        ReportClear();
        ReportAdd("Cannot query power status.");
        ShowReport();
        return;
    }
    ReportClear();
    ReportAdd("AC power  : %s", sps.ACLineStatus == 1 ? "Online" :
               (sps.ACLineStatus == 0 ? "Offline (on battery)" : "Unknown"));
    ReportAdd("Battery   : %s", sps.BatteryFlag == 128 ? "No battery present" :
               (sps.BatteryFlag == 255 ? "Unknown" : "Present"));
    if (sps.BatteryLifePercent != 255)
        ReportAdd("Charge    : %u%%", sps.BatteryLifePercent);
    if (sps.BatteryLifeTime != (DWORD)-1)
        ReportAdd("Time left : %u minutes", sps.BatteryLifeTime / 60);
    ReportAdd("Battery saver active: %s", ((const BYTE *)&sps)[3] ? "yes" : "no");
    ShowReport();
}

static void OnPowerPlanList(const char *arg) {
    (void)arg;
    RunCmd("powercfg /list", 1);
}

static void OnPowerPlanBalanced(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /setactive SCHEME_BALANCED");
}

static void OnPowerPlanHigh(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");
}

static void OnPowerPlanSaver(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /setactive a1841308-3541-4fab-bc81-f71556f20b4a");
}

static void OnPowerPlanUltimate(const char *arg) {
    (void)arg;
    RunElevatedCmd("powercfg /setactive e9a42b02-d5df-448d-aa00-03f14749eb61");
}

static void OnSleepNow(const char *arg) {
    (void)arg;
    Run("rundll32.exe", "powrprof.dll,SetSuspendState 0,1,0", 0);
}

static void OnHibernateNow(const char *arg) {
    (void)arg;
    RunElevatedCmd("shutdown /h");
}

/* ===================== Extended toolset 2: Window & Desktop Tools ===================== */

static BOOL CALLBACK ListWinProc(HWND hwnd, LPARAM lParam) {
    int *count = (int *)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    char t[256];
    if (!GetWindowTextA(hwnd, t, sizeof t) || !t[0]) return TRUE;
    if (*count >= 40) return FALSE;
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    RECT r;
    GetWindowRect(hwnd, &r);
    ReportAdd("  %-3d PID %-6lu %5dx%-5d %s", *count + 1, pid,
              r.right - r.left, r.bottom - r.top, t);
    (*count)++;
    return TRUE;
}

static void OnWindowList(const char *arg) {
    (void)arg;
    ReportClear();
    ReportAdd("Visible top-level windows (first 40):");
    int count = 0;
    EnumWindows(ListWinProc, (LPARAM)&count);
    if (!count) ReportAdd("  (none)");
    ReportAdd("Use window-activate <title-part> to focus a window.");
    ShowReport();
}

static HWND g_winFound;

static BOOL CALLBACK FindTitleProc(HWND hwnd, LPARAM lParam) {
    const char *part = (const char *)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    char t[512];
    if (!GetWindowTextA(hwnd, t, sizeof t) || !t[0]) return TRUE;
    if (stristr(t, part)) {
        g_winFound = hwnd;
        return FALSE;
    }
    return TRUE;
}

static HWND FindWindowByTitle(const char *part) {
    g_winFound = NULL;
    EnumWindows(FindTitleProc, (LPARAM)part);
    return g_winFound;
}

static void OnWindowActivate(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: window-activate <title-part>");
        ReportAdd("Example: window-activate Notepad");
        ShowReport();
        return;
    }
    HWND h = FindWindowByTitle(arg);
    ReportClear();
    if (h) {
        SetForegroundWindow(h);
        ReportAdd("Activated window matching: %s", arg);
    } else {
        ReportAdd("No visible window matches: %s", arg);
        ReportAdd("Use window-list to see open windows.");
    }
    ShowReport();
}

static void OnWindowClose(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: window-close <title-part>");
        ShowReport();
        return;
    }
    HWND h = FindWindowByTitle(arg);
    ReportClear();
    if (h) {
        PostMessageA(h, WM_CLOSE, 0, 0);
        ReportAdd("Close requested for: %s", arg);
    } else {
        ReportAdd("No visible window matches: %s", arg);
    }
    ShowReport();
}

static void OnWindowMinimize(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: window-minimize <title-part>");
        ShowReport();
        return;
    }
    HWND h = FindWindowByTitle(arg);
    ReportClear();
    if (h) {
        ShowWindow(h, SW_MINIMIZE);
        ReportAdd("Minimized: %s", arg);
    } else {
        ReportAdd("No visible window matches: %s", arg);
    }
    ShowReport();
}

static void OnWindowRestore(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: window-restore <title-part>");
        ShowReport();
        return;
    }
    HWND h = FindWindowByTitle(arg);
    ReportClear();
    if (h) {
        ShowWindow(h, SW_RESTORE);
        SetForegroundWindow(h);
        ReportAdd("Restored: %s", arg);
    } else {
        ReportAdd("No visible window matches: %s", arg);
    }
    ShowReport();
}

static void OnShowDesktop(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, 'D');
    Notify("Show desktop");
}

static void OnMinimizeAll(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, 'M');
    Notify("All windows minimized");
}

static void OnRestoreMinimized(const char *arg) {
    (void)arg;
    SimulateTriple(VK_LWIN, VK_SHIFT, 'M');
    Notify("Minimized windows restored");
}

static void OnTaskSwitcher(const char *arg) {
    (void)arg;
    SimulateCombo(VK_MENU, VK_TAB);
}

static void OnCloseWindow(const char *arg) {
    (void)arg;
    SimulateCombo(VK_MENU, VK_F4);
}

static void OnLockScreen(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, 'L');
    Notify("Screen locked");
}

static void OnVdNew(const char *arg) {
    (void)arg;
    SimulateTriple(VK_LWIN, VK_CONTROL, 'D');
    Notify("New virtual desktop created");
}

static void OnVdClose(const char *arg) {
    (void)arg;
    SimulateTriple(VK_LWIN, VK_CONTROL, VK_F4);
}

static void OnVdNext(const char *arg) {
    (void)arg;
    SimulateTriple(VK_LWIN, VK_CONTROL, VK_RIGHT);
}

static void OnVdPrev(const char *arg) {
    (void)arg;
    SimulateTriple(VK_LWIN, VK_CONTROL, VK_LEFT);
}

static void OnSnapLeft(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, VK_LEFT);
}

static void OnSnapRight(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, VK_RIGHT);
}

static void OnSnapUp(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, VK_UP);
}

static void OnSnapDown(const char *arg) {
    (void)arg;
    SimulateCombo(VK_LWIN, VK_DOWN);
}

static void OnCycleWindows(const char *arg) {
    (void)arg;
    SimulateCombo(VK_MENU, VK_ESCAPE);
}

/* ===================== Extended toolset 2: Storage & Disks ===================== */

static const char *DriveTypeName(UINT t) {
    switch (t) {
        case DRIVE_FIXED: return "Fixed";
        case DRIVE_REMOVABLE: return "Removable";
        case DRIVE_REMOTE: return "Network";
        case DRIVE_CDROM: return "CD/DVD";
        case DRIVE_RAMDISK: return "RAM";
        default: return "Unknown";
    }
}

static void OnDrivesList(const char *arg) {
    (void)arg;
    ReportClear();
    ReportAdd("Drive       Type      Free of total");
    ReportAdd("------------------------------------");
    DWORD mask = GetLogicalDrives();
    char root[4] = "A:\\";
    int n = 0;
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        root[0] = (char)('A' + i);
        UINT type = GetDriveTypeA(root);
        ULARGE_INTEGER tot, freeb;
        if (GetDiskFreeSpaceExA(root, &freeb, &tot, NULL)) {
            char fs[64], fss[64];
            FormatSize(tot.QuadPart, fs, sizeof fs);
            FormatSize(freeb.QuadPart, fss, sizeof fss);
            ReportAdd("  %s:      %-9s %10s free of %s", root, DriveTypeName(type), fss, fs);
        } else {
            ReportAdd("  %s:      %-9s", root, DriveTypeName(type));
        }
        n++;
    }
    if (!n) ReportAdd("  (no drives)");
    ShowReport();
}

static void OnDriveInfo(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: drive-info <drive-letter>");
        ReportAdd("Example: drive-info C");
        ShowReport();
        return;
    }
    char root[4];
    root[0] = (char)toupper(arg[0]);
    root[1] = ':';
    root[2] = '\\';
    root[3] = 0;
    ReportClear();
    UINT type = GetDriveTypeA(root);
    ReportAdd("Drive     : %s", root);
    ReportAdd("Type      : %s", DriveTypeName(type));
    char vol[256], fs[64];
    DWORD sn, maxlen, flags;
    if (GetVolumeInformationA(root, vol, sizeof vol, &sn, &maxlen, &flags, fs, sizeof fs)) {
        ReportAdd("Label     : %s", vol[0] ? vol : "(none)");
        ReportAdd("Filesystem: %s", fs);
        ReportAdd("Serial    : %04X-%04X", (sn >> 16) & 0xFFFF, sn & 0xFFFF);
    }
    ULARGE_INTEGER tot, freeb, freeu;
    if (GetDiskFreeSpaceExA(root, &freeu, &tot, &freeb)) {
        char fs2[64], fss[64], fsu[64];
        FormatSize(tot.QuadPart, fs2, sizeof fs2);
        FormatSize(freeb.QuadPart, fss, sizeof fss);
        FormatSize(freeu.QuadPart, fsu, sizeof fsu);
        ReportAdd("Total     : %s", fs2);
        ReportAdd("Free      : %s", fss);
        ReportAdd("Available : %s (to this user)", fsu);
        if (tot.QuadPart > 0)
            ReportAdd("Used %llu%%", (unsigned long long)((tot.QuadPart - freeu.QuadPart) * 100 / tot.QuadPart));
    }
    ShowReport();
}

static void OnDrivesRemovable(const char *arg) {
    (void)arg;
    ReportClear();
    DWORD mask = GetLogicalDrives();
    char root[4] = "A:\\";
    int n = 0;
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        root[0] = (char)('A' + i);
        UINT type = GetDriveTypeA(root);
        if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM) {
            char vol[256], fs[64];
            if (GetVolumeInformationA(root, vol, sizeof vol, NULL, NULL, NULL, fs, sizeof fs))
                ReportAdd("  %s:  %-9s %s", root, DriveTypeName(type), vol[0] ? vol : fs);
            else
                ReportAdd("  %s:  %-9s", root, DriveTypeName(type));
            n++;
        }
    }
    if (!n) ReportAdd("  (no removable drives)");
    ReportAdd("Note: for USB drives use safe-remove-hardware first.");
    ShowReport();
}

static void OnDiskDefrag(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: disk-defrag <drive-letter>");
        ReportAdd("Example: disk-defrag C");
        ShowReport();
        return;
    }
    char cmd[300];
    snprintf(cmd, sizeof cmd, "defrag %c: /O", toupper(arg[0]));
    RunElevatedCmd(cmd);
}

static void OnDiskDefragAll(const char *arg) {
    (void)arg;
    RunElevatedCmd("defrag /C /O");
}

static void OnDiskCheck(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: disk-check <drive-letter>");
        ReportAdd("Scans the drive for errors (admin, no repair).");
        ShowReport();
        return;
    }
    char cmd[300];
    snprintf(cmd, sizeof cmd, "chkdsk %c: /scan", toupper(arg[0]));
    RunElevatedCmd(cmd);
}

static void OnDiskCleanup(const char *arg) {
    (void)arg;
    Run("cleanmgr.exe", NULL, 0);
}

static void OnSafeRemoveHardware(const char *arg) {
    (void)arg;
    Run("rundll32.exe", "shell32.dll,Control_RunDLL hotplug.dll", 0);
}

static void OnSystemRestoreCreate(const char *arg) {
    (void)arg;
    RunElevatedCmd("powershell -NoProfile -Command \"Checkpoint-Computer -Description 'WindowsControl checkpoint' -RestorePointType MODIFY_SETTINGS\"");
}

static void OnRestoreManager(const char *arg) {
    (void)arg;
    Run("rstrui.exe", NULL, 1);
}

static void OnMemoryDiagnostics(const char *arg) {
    (void)arg;
    Run("mdsched.exe", NULL, 0);
}

/* ===================== Extended toolset 2: Accessibility ===================== */

static void OnStickyKeysOn(const char *arg) {
    (void)arg;
    STICKYKEYS sk;
    memset(&sk, 0, sizeof sk);
    sk.cbSize = sizeof sk;
    sk.dwFlags = SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY | SKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof sk, &sk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\StickyKeys", "Flags", "510");
    Notify("Applied: Sticky Keys");
}

static void OnStickyKeysOff(const char *arg) {
    (void)arg;
    STICKYKEYS sk;
    memset(&sk, 0, sizeof sk);
    sk.cbSize = sizeof sk;
    sk.dwFlags = SKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof sk, &sk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\StickyKeys", "Flags", "506");
    Notify("Applied: Sticky Keys (off)");
}

static void OnToggleKeysOn(const char *arg) {
    (void)arg;
    TOGGLEKEYS tk;
    memset(&tk, 0, sizeof tk);
    tk.cbSize = sizeof tk;
    tk.dwFlags = TKF_HOTKEYACTIVE | TKF_CONFIRMHOTKEY | TKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETTOGGLEKEYS, sizeof tk, &tk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\ToggleKeys", "Flags", "510");
    Notify("Applied: Toggle Keys");
}

static void OnToggleKeysOff(const char *arg) {
    (void)arg;
    TOGGLEKEYS tk;
    memset(&tk, 0, sizeof tk);
    tk.cbSize = sizeof tk;
    tk.dwFlags = TKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETTOGGLEKEYS, sizeof tk, &tk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\ToggleKeys", "Flags", "506");
    Notify("Applied: Toggle Keys (off)");
}

static void OnFilterKeysOn(const char *arg) {
    (void)arg;
    FILTERKEYS fk;
    memset(&fk, 0, sizeof fk);
    fk.cbSize = sizeof fk;
    fk.dwFlags = FKF_HOTKEYACTIVE | FKF_CONFIRMHOTKEY | FKF_AVAILABLE;
    fk.iWaitMSec = 500;
    fk.iDelayMSec = 500;
    fk.iRepeatMSec = 500;
    fk.iBounceMSec = 0;
    SystemParametersInfoA(SPI_SETFILTERKEYS, sizeof fk, &fk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\Keyboard Response", "Flags", "510");
    Notify("Applied: Filter Keys");
}

static void OnFilterKeysOff(const char *arg) {
    (void)arg;
    FILTERKEYS fk;
    memset(&fk, 0, sizeof fk);
    fk.cbSize = sizeof fk;
    fk.dwFlags = FKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETFILTERKEYS, sizeof fk, &fk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\Keyboard Response", "Flags", "122");
    Notify("Applied: Filter Keys (off)");
}

static void OnMouseKeysOn(const char *arg) {
    (void)arg;
    MOUSEKEYS mk;
    memset(&mk, 0, sizeof mk);
    mk.cbSize = sizeof mk;
    mk.dwFlags = MKF_HOTKEYACTIVE | MKF_CONFIRMHOTKEY | MKF_AVAILABLE;
    mk.iMaxSpeed = 40;
    mk.iTimeToMaxSpeed = 3000;
    mk.iCtrlSpeed = 20;
    SystemParametersInfoA(SPI_SETMOUSEKEYS, sizeof mk, &mk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\MouseKeys", "Flags", "510");
    Notify("Applied: Mouse Keys (numeric keypad)");
}

static void OnMouseKeysOff(const char *arg) {
    (void)arg;
    MOUSEKEYS mk;
    memset(&mk, 0, sizeof mk);
    mk.cbSize = sizeof mk;
    mk.dwFlags = MKF_AVAILABLE;
    SystemParametersInfoA(SPI_SETMOUSEKEYS, sizeof mk, &mk, 0);
    WriteRegString(HKEY_CURRENT_USER, "Control Panel\\Accessibility\\MouseKeys", "Flags", "122");
    Notify("Applied: Mouse Keys (off)");
}

static void OnMouseTrailsOn(const char *arg) {
    (void)arg;
    SystemParametersInfoA(SPI_SETMOUSETRAILS, 6, NULL, 0);
    Notify("Applied: mouse pointer trails");
}

static void OnMouseTrailsOff(const char *arg) {
    (void)arg;
    SystemParametersInfoA(SPI_SETMOUSETRAILS, 0, NULL, 0);
    Notify("Applied: mouse pointer trails (off)");
}

static void OnCursorShadowOn(const char *arg) {
    (void)arg;
    SystemParametersInfoA(SPI_SETCURSORSHADOW, 1, NULL, 0);
    Notify("Applied: cursor shadow");
}

static void OnCursorShadowOff(const char *arg) {
    (void)arg;
    SystemParametersInfoA(SPI_SETCURSORSHADOW, 0, NULL, 0);
    Notify("Applied: cursor shadow (off)");
}

static void OnNumlockAtBootOn(const char *arg) {
    (void)arg;
    TweakApply(HKEY_LOCAL_MACHINE, "DEFAULT\\Control Panel\\Keyboard",
               "InitialKeyboardIndicators", 2, "NumLock on at boot");
}

static void OnNumlockAtBootOff(const char *arg) {
    (void)arg;
    TweakApply(HKEY_LOCAL_MACHINE, "DEFAULT\\Control Panel\\Keyboard",
               "InitialKeyboardIndicators", 0, "NumLock off at boot");
}

/* ===================== Extended toolset 2: Optimization & Maintenance ===================== */

static void OnClearPrintSpooler(const char *arg) {
    (void)arg;
    RunElevatedCmd("net stop spooler & del /q /f \"%WINDIR%\\System32\\spool\\PRINTERS\\*\" & net start spooler");
}

static void OnCleanWindowsTemp(const char *arg) {
    (void)arg;
    RunElevatedCmd("del /q /f \"%WINDIR%\\Temp\\*\"");
}

static void OnCleanSoftwareDistribution(const char *arg) {
    (void)arg;
    RunElevatedCmd("net stop wuauserv & net stop bits & del /q /f \"%WINDIR%\\SoftwareDistribution\\Download\\*\" & net start bits & net start wuauserv");
}

static void OnFontCacheRebuild(const char *arg) {
    (void)arg;
    RunElevatedCmd("net stop FontCache & net start FontCache");
}

static void OnNetworkResetAll(const char *arg) {
    (void)arg;
    RunElevatedCmd("netsh winsock reset & netsh int ip reset & ipconfig /flushdns");
}

static void OnSuperfetchOn(const char *arg) {
    (void)arg;
    RunElevatedCmd("net start SysMain");
}

static void OnSuperfetchOff(const char *arg) {
    (void)arg;
    RunElevatedCmd("net stop SysMain");
}

static void OnSearchIndexOn(const char *arg) {
    (void)arg;
    RunElevatedCmd("net start WSearch");
}

static void OnSearchIndexOff(const char *arg) {
    (void)arg;
    RunElevatedCmd("net stop WSearch");
}

static void OnIndexingOptions(const char *arg) {
    (void)arg;
    Run("rundll32.exe", "shell32.dll,Control_RunDLL srchadmin.dll", 0);
}

static void OnEventLogClear(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: event-log-clear <log-name>");
        ReportAdd("Example: event-log-clear Application");
        ReportAdd("Other logs: System, Security, Setup, PowerShell");
        ShowReport();
        return;
    }
    char cmd[400];
    snprintf(cmd, sizeof cmd, "wevtutil cl %s", arg);
    RunElevatedCmd(cmd);
}

/* ===================== Extended toolset 2: Security, Network, System, Media ===================== */

static void OnDefenderQuickScan(const char *arg) {
    (void)arg;
    RunElevatedCmd("powershell -NoProfile -Command \"Start-MpScan -ScanType QuickScan\"");
}

static void OnDefenderFullScan(const char *arg) {
    (void)arg;
    RunElevatedCmd("powershell -NoProfile -Command \"Start-MpScan -ScanType FullScan\"");
}

static void OnDefenderUpdate(const char *arg) {
    (void)arg;
    RunElevatedCmd("powershell -NoProfile -Command \"Update-MpSignature\"");
}

static void OnDefenderThreatHistory(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-MpThreat | Select-Object ThreatName,SeverityID,IsActive | Format-Table -AutoSize\"", 1);
}

static int SafeUrl(const char *s) {
    if (!s || !*s) return 0;
    if (strchr(s, '"') || strchr(s, '\'')) return 0;
    return 1;
}

static void OnHttpCheck(const char *arg) {
    if (!SafeUrl(arg)) {
        ReportClear();
        ReportAdd("Usage: http-check <url>");
        ReportAdd("Example: http-check https://example.com");
        ShowReport();
        return;
    }
    char ps[1600];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"try { (Invoke-WebRequest -Uri '%s' -Method Head -UseBasicParsing -TimeoutSec 10).StatusCode } catch { Write-Output 'ERROR: unreachable' }\"",
             arg);
    RunCmd(ps, 1);
}

static void OnUrlDownload(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: url-download <url>|<destination-file>");
        ShowReport();
        return;
    }
    if (!SafeUrl(parts[0]) || !SafeUrl(parts[1])) {
        ReportClear();
        ReportAdd("Invalid URL or file path.");
        ShowReport();
        return;
    }
    char ps[1600];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '%s' -OutFile '%s' -UseBasicParsing\"",
             parts[0], parts[1]);
    RunCmd(ps, 1);
}

static void OnIpv6Disable(const char *arg) {
    (void)arg;
    TweakApply(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\TCPIP6\\Parameters",
               "DisabledComponents", 0xFF, "IPv6 disabled (reboot required)");
}

static void OnIpv6Enable(const char *arg) {
    (void)arg;
    TweakApply(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\TCPIP6\\Parameters",
               "DisabledComponents", 0, "IPv6 enabled (reboot required)");
}

static void OnActivationStatus(const char *arg) {
    (void)arg;
    RunCmd("slmgr /xpr", 1);
}

static void OnDriverList(const char *arg) {
    (void)arg;
    RunCmd("pnputil /enum-drivers", 1);
}

static void OnDriverDelete(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: driver-delete <oemNN.inf>");
        ReportAdd("Use driver-list to find the .inf name.");
        ShowReport();
        return;
    }
    char cmd[400];
    snprintf(cmd, sizeof cmd, "pnputil /delete-driver %s /force", arg);
    RunElevatedCmd(cmd);
}

static void OnMediaPlayPause(const char *arg) {
    (void)arg;
    SimulateKey(VK_MEDIA_PLAY_PAUSE);
}

static void OnMediaNext(const char *arg) {
    (void)arg;
    SimulateKey(VK_MEDIA_NEXT_TRACK);
}

static void OnMediaPrev(const char *arg) {
    (void)arg;
    SimulateKey(VK_MEDIA_PREV_TRACK);
}

static void OnMediaStop(const char *arg) {
    (void)arg;
    SimulateKey(VK_MEDIA_STOP);
}

/* ===================== Extended toolset 2: more System Tweaks ===================== */

TWEAK(AutoHideTaskbar, "auto-hide taskbar", HKEY_CURRENT_USER, TW_EA, "TaskbarAutoHide", 1, 0)
TWEAK(LockTaskbar, "lock the taskbar", HKEY_CURRENT_USER, TW_EA, "TaskbarLockAll", 1, 0)
TWEAK(SearchBar, "taskbar search box", HKEY_CURRENT_USER, TW_EA, "SearchboxTaskbarMode", 2, 0)
TWEAK(TaskViewButton, "Task View button", HKEY_CURRENT_USER, TW_EA, "TaskViewButtonEnabled", 1, 0)
TWEAK(WidgetsButton, "widgets button", HKEY_CURRENT_USER, TW_EA, "TaskbarDa", 1, 0)
TWEAK(GameMode, "Game Mode", HKEY_CURRENT_USER, "Software\\Microsoft\\GameBar", "AutoGameModeEnabled", 1, 0)
TWEAK(AutoPlay, "AutoPlay", HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", "NoDriveTypeAutoRun", 0x91, 0xFF)
TWEAK(Animations, "window animations", HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", "MinAnimate", 1, 0)
TWEAK(DragFullWindows, "show window contents while dragging", HKEY_CURRENT_USER, "Control Panel\\Desktop", "DragFullWindows", 1, 0)
TWEAK(VisualFxBest, "best visual appearance", HKEY_CURRENT_USER, TW_EG, "VisualFXSetting", 2, 3)
TWEAK(DesktopIconThisPc, "This PC desktop icon", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{20D04FE0-3AEA-1069-A2D8-08002B30309D}", 0, 1)
TWEAK(DesktopIconRecycle, "Recycle Bin desktop icon", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{645FF040-5081-101B-9F08-00AA002F954E}", 0, 1)
TWEAK(DesktopIconControlPanel, "Control Panel desktop icon", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{5399E694-6CE5-4D6C-8FCE-1D8870FDCBA0}", 0, 1)
TWEAK(DesktopIconUserFolder, "user folder desktop icon", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel", "{59031a47-3f72-44a7-89c5-5595fe6b30ee}", 0, 1)
TWEAK(SnapWindows, "window snapping", HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\SnapAssist", "Enabled", 1, 0)
TWEAK(StartRecentItems, "recent items in Start menu", HKEY_CURRENT_USER, TW_EA, "Start_TrackDocs", 1, 0)
TWEAK(StartRecentApps, "recently added apps in Start menu", HKEY_CURRENT_USER, TW_EA, "Start_TrackProgs", 1, 0)
TWEAK(ShowTrayIcons, "show all notification area icons", HKEY_CURRENT_USER, TW_EG, "EnableAutoTray", 0, 1)

/* ===================== Extended toolset: File & Folder Tools ===================== */

typedef struct {
    ULONGLONG size;
    int files;
} SizeCtx;

typedef struct {
    ULONGLONG minSize;
    char bigPath[20][MAX_PATH];
    ULONGLONG bigSize[20];
    int count;
} LargeCtx;

typedef struct {
    const char *pattern;
    int count;
    char matches[30][MAX_PATH];
} SearchCtx;

typedef struct {
    int count;
    char paths[60][MAX_PATH];
    char hash[60][33];
} FileListCtx;

static void WalkDir(const char *dir,
                    void (*visitor)(const char *path, ULONGLONG size, void *ctx),
                    void *ctx) {
    char pat[MAX_PATH * 2];
    sprintf(pat, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH * 2];
        sprintf(full, "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            WalkDir(full, visitor, ctx);
        } else {
            ULARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            visitor(full, sz.QuadPart, ctx);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void SizeVisitor(const char *path, ULONGLONG size, void *ctx) {
    (void)path;
    SizeCtx *c = (SizeCtx *)ctx;
    c->size += size;
    c->files++;
}

static void LargeVisitor(const char *path, ULONGLONG size, void *ctx) {
    LargeCtx *c = (LargeCtx *)ctx;
    if (size >= c->minSize && c->count < 20) {
        strncpy(c->bigPath[c->count], path, MAX_PATH - 1);
        c->bigPath[c->count][MAX_PATH - 1] = 0;
        c->bigSize[c->count] = size;
        c->count++;
    }
}

static int WildMatch(const char *text, const char *pat) {
    while (*text) {
        if (*pat == '*') {
            pat++;
            if (!*pat) return 1;
            while (*text) {
                if (WildMatch(text, pat)) return 1;
                text++;
            }
            return 0;
        }
        if (*pat == '?' || (*pat | 0x20) == (*text | 0x20)) {
            text++;
            pat++;
        } else return 0;
    }
    while (*pat == '*') pat++;
    return *pat == 0;
}

static void SearchVisitor(const char *path, ULONGLONG size, void *ctx) {
    (void)size;
    SearchCtx *c = (SearchCtx *)ctx;
    if (c->count >= 30) return;
    const char *name = strrchr(path, '\\');
    name = name ? name + 1 : path;
    if (WildMatch(name, c->pattern)) {
        strncpy(c->matches[c->count], path, MAX_PATH - 1);
        c->matches[c->count][MAX_PATH - 1] = 0;
        c->count++;
    }
}

static void DupVisitor(const char *path, ULONGLONG size, void *ctx) {
    (void)size;
    FileListCtx *c = (FileListCtx *)ctx;
    if (c->count >= 60) return;
    strncpy(c->paths[c->count], path, MAX_PATH - 1);
    c->paths[c->count][MAX_PATH - 1] = 0;
    CalcHashFile(path, CALG_MD5, c->hash[c->count], 33);
    c->count++;
}

static void HexStr(const unsigned char *b, size_t n, char *out) {
    out[0] = 0;
    for (size_t i = 0; i < 16; i++) {
        char t[8];
        if (i < n) sprintf(t, "%02X ", b[i]);
        else strcpy(t, "   ");
        strcat(out, t);
    }
}

static void OnFileInfo(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: file-info <path>");
        ShowReport();
        return;
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(arg, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        ReportClear();
        ReportAdd("Not found: %s", arg);
        ShowReport();
        return;
    }
    FindClose(h);
    ULARGE_INTEGER sz;
    sz.LowPart = fd.nFileSizeLow;
    sz.HighPart = fd.nFileSizeHigh;
    char fs[64];
    FormatSize(sz.QuadPart, fs, sizeof fs);
    ReportClear();
    ReportAdd("Path      : %s", arg);
    ReportAdd("Size      : %llu bytes (%s)", (unsigned long long)sz.QuadPart, fs);
    ReportAdd("Type      : %s", (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "Folder" : "File");
    ReportAdd("Hidden    : %s", (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? "yes" : "no");
    ReportAdd("Read-only : %s", (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? "yes" : "no");
    ReportAdd("Created   : %s", CTimeStr(fd.ftCreationTime));
    ReportAdd("Modified  : %s", CTimeStr(fd.ftLastWriteTime));
    ReportAdd("Accessed  : %s", CTimeStr(fd.ftLastAccessTime));
    ShowReport();
}

static void OnFileCopy(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: file-copy <source>|<destination>");
        ShowReport();
        return;
    }
    if (CopyFileA(parts[0], parts[1], FALSE)) {
        ReportClear();
        ReportAdd("Copied: %s", parts[0]);
        ReportAdd("To    : %s", parts[1]);
        ShowReport();
    } else {
        ReportClear();
        ReportAdd("Copy failed (error %lu): %s -> %s", GetLastError(), parts[0], parts[1]);
        ShowReport();
    }
}

static void OnFileMove(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: file-move <source>|<destination>");
        ReportAdd("Also used for renaming.");
        ShowReport();
        return;
    }
    if (MoveFileExA(parts[0], parts[1], MOVEFILE_REPLACE_EXISTING)) {
        ReportClear();
        ReportAdd("Moved: %s", parts[0]);
        ReportAdd("To   : %s", parts[1]);
        ShowReport();
    } else {
        ReportClear();
        ReportAdd("Move failed (error %lu): %s -> %s", GetLastError(), parts[0], parts[1]);
        ShowReport();
    }
}

static void OnFileRename(const char *arg) {
    OnFileMove(arg);
}

static void OnFileDelete(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: file-delete <path>");
        ShowReport();
        return;
    }
    if (DeleteFileA(arg)) {
        ReportClear();
        ReportAdd("Deleted: %s", arg);
        ShowReport();
        return;
    }
    DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
        char from[MAX_PATH * 2];
        strcpy(from, arg);
        from[strlen(arg) + 1] = 0;
        SHFILEOPSTRUCTA fo;
        memset(&fo, 0, sizeof fo);
        fo.wFunc = FO_DELETE;
        fo.pFrom = from;
        fo.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
        int r = SHFileOperationA(&fo);
        ReportClear();
        ReportAdd("Moved to Recycle Bin: %s (%d)", arg, r);
        ShowReport();
    } else {
        ReportClear();
        ReportAdd("Delete failed (error %lu): %s", err, arg);
        ShowReport();
    }
}

static void OnFolderCreate(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: folder-create <path>");
        ShowReport();
        return;
    }
    int r = SHCreateDirectoryExA(NULL, arg, NULL);
    ReportClear();
    if (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS)
        ReportAdd("Folder ready: %s", arg);
    else
        ReportAdd("Cannot create folder (error %d): %s", r, arg);
    ShowReport();
}

static void OnFolderDelete(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: folder-delete <path>  (moves to Recycle Bin)");
        ShowReport();
        return;
    }
    char from[MAX_PATH * 2];
    strcpy(from, arg);
    from[strlen(arg) + 1] = 0;
    SHFILEOPSTRUCTA fo;
    memset(&fo, 0, sizeof fo);
    fo.wFunc = FO_DELETE;
    fo.pFrom = from;
    fo.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    int r = SHFileOperationA(&fo);
    ReportClear();
    if (r == 0) ReportAdd("Folder moved to Recycle Bin: %s", arg);
    else ReportAdd("Cannot delete folder (%d): %s", r, arg);
    ShowReport();
}

static void OnFolderSize(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: folder-size <path>");
        ShowReport();
        return;
    }
    SizeCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    WalkDir(arg, SizeVisitor, &ctx);
    char fs[64];
    FormatSize(ctx.size, fs, sizeof fs);
    ReportClear();
    ReportAdd("Folder : %s", arg);
    ReportAdd("Files  : %d", ctx.files);
    ReportAdd("Size   : %llu bytes (%s)", (unsigned long long)ctx.size, fs);
    ShowReport();
}

static void OnFileSearch(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 1 || !parts[0][0]) {
        ReportClear();
        ReportAdd("Usage: file-search <folder>|<pattern>");
        ReportAdd("Example: file-search C:\\Users|*.log");
        ReportAdd("Pattern default is * (all files).");
        ShowReport();
        return;
    }
    const char *pat = (n >= 2 && parts[1][0]) ? parts[1] : "*";
    SearchCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.pattern = pat;
    WalkDir(parts[0], SearchVisitor, &ctx);
    ReportClear();
    if (ctx.count == 0) {
        ReportAdd("No matches for '%s' in %s", pat, parts[0]);
    } else {
        ReportAdd("%d match(es) for '%s' in %s:", ctx.count, pat, parts[0]);
        for (int i = 0; i < ctx.count; i++)
            ReportAdd("  %s", ctx.matches[i]);
    }
    ReportAdd("(Recursive search, up to 30 results)");
    ShowReport();
}

static void OnLargeFiles(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 1 || !parts[0][0]) {
        ReportClear();
        ReportAdd("Usage: large-files <folder>|<minimum-MB>");
        ReportAdd("Example: large-files C:\\Users|100");
        ShowReport();
        return;
    }
    int mb = (n >= 2 && parts[1][0]) ? atoi(parts[1]) : 100;
    if (mb < 1) mb = 1;
    LargeCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.minSize = (ULONGLONG)mb * 1024 * 1024;
    WalkDir(parts[0], LargeVisitor, &ctx);
    ReportClear();
    ReportAdd("Largest files >= %d MB in %s (%d found):", mb, parts[0], ctx.count);
    for (int i = 0; i < ctx.count; i++) {
        char fs[64];
        FormatSize(ctx.bigSize[i], fs, sizeof fs);
        ReportAdd("  %-10s %s", fs, ctx.bigPath[i]);
    }
    if (!ctx.count) ReportAdd("  (none)");
    ShowReport();
}

static void OnDupFiles(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: dup-files <folder>");
        ReportAdd("Hashes every file (MD5) and shows groups with identical content.");
        ShowReport();
        return;
    }
    FileListCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    WalkDir(arg, DupVisitor, &ctx);
    ReportClear();
    ReportAdd("Hashed %d files (max 60) in %s", ctx.count, arg);
    int groups = 0;
    for (int i = 0; i < ctx.count; i++) {
        if (!ctx.hash[i][0]) continue;
        int first = 1;
        for (int j = i + 1; j < ctx.count; j++) {
            if (ctx.hash[j][0] && strcmp(ctx.hash[i], ctx.hash[j]) == 0) {
                if (first) {
                    groups++;
                    ReportAdd("");
                    ReportAdd("Group %d (MD5 %s):", groups, ctx.hash[i]);
                    ReportAdd("  %s", ctx.paths[i]);
                    first = 0;
                }
                ReportAdd("  %s", ctx.paths[j]);
                ctx.hash[j][0] = 0;
            }
        }
    }
    if (!groups) ReportAdd("No duplicate files found.");
    ReportAdd("(Hashing can be slow on large folders)");
    ShowReport();
}

static void OnHexDump(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: hex-dump <file>");
        ShowReport();
        return;
    }
    FILE *f = fopen(arg, "rb");
    if (!f) {
        ReportClear();
        ReportAdd("Cannot open file: %s", arg);
        ShowReport();
        return;
    }
    ReportClear();
    unsigned char buf[16];
    size_t n;
    long long off = 0;
    while ((n = fread(buf, 1, 16, f)) > 0) {
        if (off >= 4096) {
            ReportAdd("... (truncated after first 4 KB) ...");
            break;
        }
        char hexs[64], asc[17];
        HexStr(buf, n, hexs);
        for (int i = 0; i < 16; i++) {
            if (i < (int)n) asc[i] = (buf[i] >= 32 && buf[i] < 127) ? (char)buf[i] : '.';
            else asc[i] = ' ';
        }
        asc[16] = 0;
        ReportAdd("%08llX  %s |%s|", (unsigned long long)off, hexs, asc);
        off += (long long)n;
    }
    fclose(f);
    ShowReport();
}

static void OnLineCount(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: line-count <file>");
        ShowReport();
        return;
    }
    FILE *f = fopen(arg, "rb");
    if (!f) {
        ReportClear();
        ReportAdd("Cannot open file: %s", arg);
        ShowReport();
        return;
    }
    char buf[65536];
    size_t n;
    long long lines = 1, bytes = 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        bytes += (long long)n;
        for (size_t i = 0; i < n; i++)
            if (buf[i] == '\n') lines++;
    }
    fclose(f);
    ReportClear();
    ReportAdd("File  : %s", arg);
    ReportAdd("Bytes : %lld", bytes);
    ReportAdd("Lines : %lld", lines);
    ShowReport();
}

static void OnBase64EncodeFile(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: base64-encode <file>   (saves <file>.base64)");
        ShowReport();
        return;
    }
    FILE *f = fopen(arg, "rb");
    if (!f) {
        ReportClear();
        ReportAdd("Cannot open file: %s", arg);
        ShowReport();
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        ReportClear();
        ReportAdd("File size unsupported (empty or over 64 MB).");
        ShowReport();
        return;
    }
    unsigned char *data = (unsigned char *)malloc((size_t)sz);
    if (!data) { fclose(f); return; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        ReportClear();
        ReportAdd("Read error: %s", arg);
        ShowReport();
        return;
    }
    fclose(f);
    DWORD need = 0;
    CryptBinaryToStringA(data, (DWORD)sz, CRYPT_STRING_BASE64, NULL, &need);
    char *out = (char *)malloc(need ? need : 1);
    if (out && CryptBinaryToStringA(data, (DWORD)sz, CRYPT_STRING_BASE64, out, &need)) {
        char dst[MAX_PATH + 32];
        sprintf(dst, "%s.base64", arg);
        FILE *o = fopen(dst, "wb");
        if (o) {
            fwrite(out, 1, need, o);
            fclose(o);
            ReportClear();
            ReportAdd("Base64 written to: %s (%lu bytes)", dst, need);
        } else {
            ReportClear();
            ReportAdd("Cannot write output file: %s", dst);
        }
    } else {
        ReportClear();
        ReportAdd("Base64 encoding failed.");
    }
    free(out);
    free(data);
    ShowReport();
}

static void OnBase64DecodeFile(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: base64-decode <file>   (saves <file>.out)");
        ShowReport();
        return;
    }
    FILE *f = fopen(arg, "rb");
    if (!f) {
        ReportClear();
        ReportAdd("Cannot open file: %s", arg);
        ShowReport();
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        ReportClear();
        ReportAdd("File size unsupported.");
        ShowReport();
        return;
    }
    char *data = (char *)malloc((size_t)sz + 1);
    if (!data) { fclose(f); return; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        ReportClear();
        ReportAdd("Read error: %s", arg);
        ShowReport();
        return;
    }
    fclose(f);
    data[sz] = 0;
    DWORD need = 0;
    if (!CryptStringToBinaryA(data, 0, CRYPT_STRING_BASE64, NULL, &need, NULL, NULL)) {
        free(data);
        ReportClear();
        ReportAdd("Invalid base64 data in: %s", arg);
        ShowReport();
        return;
    }
    BYTE *out = (BYTE *)malloc(need ? need : 1);
    if (out && CryptStringToBinaryA(data, 0, CRYPT_STRING_BASE64, out, &need, NULL, NULL)) {
        char dst[MAX_PATH + 32];
        sprintf(dst, "%s.out", arg);
        FILE *o = fopen(dst, "wb");
        if (o) {
            fwrite(out, 1, need, o);
            fclose(o);
            ReportClear();
            ReportAdd("Decoded %lu bytes to: %s", need, dst);
        } else {
            ReportClear();
            ReportAdd("Cannot write output file: %s", dst);
        }
    } else {
        ReportClear();
        ReportAdd("Base64 decoding failed.");
    }
    free(out);
    free(data);
    ShowReport();
}

static void OnClipboardCopyFile(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: clipboard-copy <file>");
        ShowReport();
        return;
    }
    FILE *f = fopen(arg, "rb");
    if (!f) {
        ReportClear();
        ReportAdd("Cannot open file: %s", arg);
        ShowReport();
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 64 * 1024 * 1024) {
        fclose(f);
        ReportClear();
        ReportAdd("File too large for clipboard (max 64 MB).");
        ShowReport();
        return;
    }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;
    if (!OpenClipboard(NULL)) {
        free(buf);
        ReportClear();
        ReportAdd("Cannot open the clipboard.");
        ShowReport();
        return;
    }
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, rd + 1);
    if (h) {
        void *p = GlobalLock(h);
        if (p) {
            memcpy(p, buf, rd);
            ((char *)p)[rd] = 0;
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        }
    }
    CloseClipboard();
    ReportClear();
    ReportAdd("Copied %d bytes from '%s' to the clipboard.", (int)rd, arg);
    ShowReport();
    free(buf);
}

static void OnClipboardView(const char *arg) {
    (void)arg;
    ReportClear();
    if (!OpenClipboard(NULL)) {
        ReportAdd("Cannot open the clipboard.");
        ShowReport();
        return;
    }
    HANDLE h = GetClipboardData(CF_TEXT);
    if (h) {
        const char *p = (const char *)GlobalLock(h);
        if (p) {
            ReportAdd("Clipboard text (%d chars):", (int)strlen(p));
            ReportAdd("%s", p);
            GlobalUnlock(h);
        } else {
            ReportAdd("Clipboard locked.");
        }
    } else {
        HANDLE hu = GetClipboardData(CF_UNICODETEXT);
        if (hu) {
            const wchar_t *w = (const wchar_t *)GlobalLock(hu);
            if (w) {
                char out[8192];
                WideCharToMultiByte(CP_ACP, 0, w, -1, out, sizeof out, NULL, NULL);
                ReportAdd("Clipboard text: %s", out);
                GlobalUnlock(hu);
            }
        } else {
            ReportAdd("Clipboard contains no text.");
        }
    }
    CloseClipboard();
    ShowReport();
}

static void OnClipboardSave(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: clipboard-save <file>");
        ShowReport();
        return;
    }
    ReportClear();
    if (!OpenClipboard(NULL)) {
        ReportAdd("Cannot open the clipboard.");
        ShowReport();
        return;
    }
    HANDLE h = GetClipboardData(CF_TEXT);
    const char *p = h ? (const char *)GlobalLock(h) : NULL;
    if (!p) {
        CloseClipboard();
        ReportAdd("Clipboard contains no text.");
        ShowReport();
        return;
    }
    int len = (int)strlen(p);
    FILE *f = fopen(arg, "wb");
    int ok = 0;
    if (f) {
        ok = fwrite(p, 1, (size_t)len, f) == (size_t)len ? 1 : 0;
        fclose(f);
    }
    GlobalUnlock(h);
    CloseClipboard();
    if (ok) ReportAdd("Clipboard text (%d chars) saved to: %s", len, arg);
    else ReportAdd("Cannot write file: %s", arg);
    ShowReport();
}

static void OnZipCreate(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: zip-create <folder-or-file>|<archive.zip>");
        ShowReport();
        return;
    }
    char ps[1400];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Compress-Archive -Path '%s' -DestinationPath '%s' -Force\"",
             parts[0], parts[1]);
    RunCmd(ps, 1);
}

static void OnZipExtract(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: zip-extract <archive.zip>|<destination-folder>");
        ShowReport();
        return;
    }
    char ps[1400];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
             parts[0], parts[1]);
    RunCmd(ps, 1);
}

static void OnRecentFiles(const char *arg) {
    (void)arg;
    RunCmd("dir \"%APPDATA%\\Microsoft\\Windows\\Recent\"", 1);
}

static void OnRecycleSize(const char *arg) {
    (void)arg;
    SHQUERYRBINFO si;
    memset(&si, 0, sizeof si);
    si.cbSize = sizeof si;
    if (SHQueryRecycleBinA(NULL, &si) == S_OK) {
        char fs[64];
        FormatSize(si.i64Size, fs, sizeof fs);
        ReportClear();
        ReportAdd("Recycle Bin items: %llu", (unsigned long long)si.i64NumItems);
        ReportAdd("Recycle Bin size  : %llu bytes (%s)", (unsigned long long)si.i64Size, fs);
    } else {
        ReportClear();
        ReportAdd("Cannot query the Recycle Bin.");
    }
    ShowReport();
}

/* ===================== Extended toolset: Hardware & Devices ===================== */

static void OnMonitorsList(const char *arg) {
    (void)arg;
    ReportClear();
    DISPLAY_DEVICEA dd;
    dd.cb = sizeof dd;
    int shown = 0;
    for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
        if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) continue;
        DEVMODEA dm;
        memset(&dm, 0, sizeof dm);
        dm.dmSize = sizeof dm;
        EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm);
        shown++;
        ReportAdd("Monitor %d:", shown);
        ReportAdd("  Name      : %s", dd.DeviceString);
        ReportAdd("  Adapter   : %s", dd.DeviceName);
        ReportAdd("  Resolution: %lux%lu @ %lu Hz", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency);
        ReportAdd("  Color     : %lu bits per pixel", dm.dmBitsPerPel);
        if (shown >= 8) break;
    }
    if (!shown) ReportAdd("No attached displays found.");
    ShowReport();
}

static void OnUsbList(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-PnpDevice -Class USB -PresentOnly | Select-Object Status,FriendlyName | Format-Table -AutoSize\"", 1);
}

static void OnGpuList(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,VideoModeDescription,AdapterRAM | Format-List\"", 1);
}

static void OnSoundDevices(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-CimInstance Win32_SoundDevice | Select-Object Name,Status,Manufacturer | Format-Table -AutoSize\"", 1);
}

static void OnPrintersList(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-Printer | Select-Object Name,PortName,DriverName,Shared | Format-Table -AutoSize\"", 1);
}

static void OnNetworkAdapters(const char *arg) {
    (void)arg;
    ULONG size = 0;
    GetAdaptersInfo(NULL, &size);
    if (!size) {
        ReportClear();
        ReportAdd("No network adapters found.");
        ShowReport();
        return;
    }
    IP_ADAPTER_INFO *ai = (IP_ADAPTER_INFO *)malloc(size);
    if (!ai) return;
    if (GetAdaptersInfo(ai, &size) != NO_ERROR) {
        free(ai);
        return;
    }
    ReportClear();
    int n = 0;
    for (IP_ADAPTER_INFO *p = ai; p; p = p->Next) {
        n++;
        char mac[32];
        sprintf(mac, "%02X-%02X-%02X-%02X-%02X-%02X",
                p->Address[0], p->Address[1], p->Address[2],
                p->Address[3], p->Address[4], p->Address[5]);
        ReportAdd("Adapter %d:", n);
        ReportAdd("  Name   : %s", p->Description);
        ReportAdd("  MAC    : %s", mac);
        ReportAdd("  Type   : %lu", p->Type);
        ReportAdd("  DHCP   : %s", p->DhcpEnabled ? "enabled" : "disabled");
    }
    free(ai);
    ShowReport();
}

static void OnMemorySlots(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-CimInstance Win32_PhysicalMemory | Select-Object DeviceLocator,@{n='Size(GB)';e={[math]::Round($_.Capacity/1GB,1)}},Speed,Manufacturer | Format-Table -AutoSize\"", 1);
}

static void OnBatteryDetails(const char *arg) {
    (void)arg;
    RunCmd("powercfg /batteryreport", 1);
}

/* ===================== Extended toolset: Accounts & Scheduling ===================== */

static void OnAccountsList(const char *arg) {
    (void)arg;
    RunCmd("net user", 1);
}

static void OnAccountCreate(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: account-create <username>|<password>");
        ShowReport();
        return;
    }
    if (!SafeArg(parts[0]) || !SafeArg(parts[1])) {
        ReportClear();
        ReportAdd("Invalid characters in username or password.");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "net user \"%s\" \"%s\" /add", parts[0], parts[1]);
    RunElevatedCmd(cmd);
}

static void OnAccountEnable(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: account-enable <username>");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "net user \"%s\" /active:yes", arg);
    RunElevatedCmd(cmd);
}

static void OnAccountDisable(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: account-disable <username>");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "net user \"%s\" /active:no", arg);
    RunElevatedCmd(cmd);
}

static void OnAccountDelete(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: account-delete <username>");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "net user \"%s\" /delete", arg);
    RunElevatedCmd(cmd);
}

static void OnAccountAdmin(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: account-admin <username>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "net localgroup administrators \"%s\" /add", arg);
    RunElevatedCmd(cmd);
}

static void OnAccountRemoveAdmin(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: account-remove-admin <username>");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "net localgroup administrators \"%s\" /delete", arg);
    RunElevatedCmd(cmd);
}

static void OnAccountPasswordSet(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: account-password-set <username>|<new-password>");
        ShowReport();
        return;
    }
    if (!SafeArg(parts[0]) || !SafeArg(parts[1])) {
        ReportClear();
        ReportAdd("Invalid characters in username or password.");
        ShowReport();
        return;
    }
    char cmd[700];
    snprintf(cmd, sizeof cmd, "net user \"%s\" \"%s\"", parts[0], parts[1]);
    RunElevatedCmd(cmd);
}

static void OnSharesList(const char *arg) {
    (void)arg;
    RunCmd("net share", 1);
}

static void OnShareCreate(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: share-create <folder-path>|<share-name>");
        ShowReport();
        return;
    }
    if (!SafeArg(parts[0]) || !SafeArg(parts[1])) {
        ReportClear();
        ReportAdd("Invalid characters in path or share name.");
        ShowReport();
        return;
    }
    char cmd[800];
    snprintf(cmd, sizeof cmd, "net share \"%s\"=\"%s\" /grant:Everyone,READ", parts[1], parts[0]);
    RunElevatedCmd(cmd);
}

static void OnShareDelete(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: share-delete <share-name>");
        ShowReport();
        return;
    }
    char cmd[600];
    snprintf(cmd, sizeof cmd, "net share \"%s\" /delete", arg);
    RunElevatedCmd(cmd);
}

static void OnTaskList(const char *arg) {
    (void)arg;
    RunCmd("schtasks /query /fo TABLE", 1);
}

static void OnTaskCreate(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0] || !parts[1][0]) {
        ReportClear();
        ReportAdd("Usage: task-create <name>|<command>");
        ReportAdd("Example: task-create MyBackup|C:\\backup.bat");
        ReportAdd("Runs at logon of any user.");
        ShowReport();
        return;
    }
    if (!SafeArg(parts[0]) || !SafeArg(parts[1])) {
        ReportClear();
        ReportAdd("Invalid characters in task name or command.");
        ShowReport();
        return;
    }
    char args[1400];
    snprintf(args, sizeof args, "/create /tn \"%s\" /tr \"%s\" /sc ONLOGON /f", parts[0], parts[1]);
    Run("schtasks.exe", args, 1);
}

static void OnTaskDelete(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: task-delete <task-name>");
        ShowReport();
        return;
    }
    char args[700];
    snprintf(args, sizeof args, "/delete /tn \"%s\" /f", arg);
    Run("schtasks.exe", args, 1);
}

static void OnTaskRun(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: task-run <task-name>");
        ShowReport();
        return;
    }
    char args[700];
    snprintf(args, sizeof args, "/run /tn \"%s\"", arg);
    Run("schtasks.exe", args, 1);
}

/* ===================== Extended toolset: App Management ===================== */

static void OnAppUninstall(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: app-uninstall <name-part>");
        ReportAdd("Example: app-uninstall Notepad");
        ShowReport();
        return;
    }
    char found[1024];
    int foundOne = 0;
    const char *roots[2] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
    };
    ReportClear();
    for (int r = 0; r < 2; r++) {
        HKEY hk;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, roots[r], 0, KEY_READ, &hk) != ERROR_SUCCESS)
            continue;
        for (int i = 0;; i++) {
            char name[256];
            DWORD ns = sizeof name;
            if (RegEnumKeyExA(hk, i, name, &ns, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
                break;
            HKEY sk;
            if (RegOpenKeyExA(hk, name, 0, KEY_READ, &sk) != ERROR_SUCCESS)
                continue;
            char disp[512], us[1024];
            DWORD ds = sizeof disp, uss = sizeof us;
            LONG r1 = RegQueryValueExA(sk, "DisplayName", NULL, NULL, (BYTE *)disp, &ds);
            LONG r2 = RegQueryValueExA(sk, "UninstallString", NULL, NULL, (BYTE *)us, &uss);
            RegCloseKey(sk);
            if (r1 != ERROR_SUCCESS || r2 != ERROR_SUCCESS) continue;
            if (ds >= sizeof disp) disp[sizeof disp - 1] = 0;
            if (uss >= sizeof us) us[sizeof us - 1] = 0;
            if (stristr(disp, arg)) {
                ReportAdd("Uninstaller found: %s", disp);
                if (!foundOne) {
                    snprintf(found, sizeof found, "%s", us);
                    foundOne = 1;
                }
            }
        }
        RegCloseKey(hk);
    }
    if (!foundOne) ReportAdd("No matching installed application found.");
    ShowReport();
    if (foundOne) {
        char args[1100];
        snprintf(args, sizeof args, "/c start \"\" \"%s\"", found);
        Run("cmd.exe", args, 1);
    }
}

static void OnAppxList(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-AppxPackage | Select-Object Name,Version,Status | Format-Table -AutoSize\"", 1);
}

static void OnAppxRemove(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: appx-remove <package-name>");
        ReportAdd("Example: appx-remove Microsoft.ZuneVideo");
        ShowReport();
        return;
    }
    char ps[1200];
    snprintf(ps, sizeof ps,
             "powershell -NoProfile -Command \"Get-AppxPackage -Name '%s' | Remove-AppxPackage\"",
             arg);
    RunElevatedCmd(ps);
}

static void OnMsiInstall(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: msi-install <file.msi>");
        ShowReport();
        return;
    }
    char args[700];
    snprintf(args, sizeof args, "/i \"%s\"", arg);
    Run("msiexec.exe", args, 1);
}

static void OnMsiRepair(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: msi-repair <file.msi-or-product-code>");
        ShowReport();
        return;
    }
    char args[700];
    snprintf(args, sizeof args, "/fa \"%s\"", arg);
    Run("msiexec.exe", args, 1);
}

static void OnMsiUninstall(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: msi-uninstall <file.msi-or-product-code>");
        ShowReport();
        return;
    }
    char args[700];
    snprintf(args, sizeof args, "/x \"%s\"", arg);
    Run("msiexec.exe", args, 1);
}

static void OnUpdateCheckNow(const char *arg) {
    (void)arg;
    RunElevatedCmd("powershell -NoProfile -Command \"$u = New-Object -ComObject Microsoft.Update.AutoUpdate; $u.DetectNow()\"");
}

static void OnUpdateHistory(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-HotFix | Sort-Object InstalledOn -Descending | Select-Object -First 30 HotFixID,InstalledOn,Description | Format-Table -AutoSize\"", 1);
}

/* ===================== Extended toolset: Reports ===================== */

static void OnRepFirewall(const char *arg) {
    (void)arg;
    RunCmd("netsh advfirewall show allprofiles state", 1);
}

static void OnRepDefender(const char *arg) {
    (void)arg;
    RunCmd("powershell -NoProfile -Command \"Get-MpComputerStatus | Select-Object AntivirusEnabled,RealTimeProtectionEnabled,AntivirusSignatureLastUpdated,QuickScanEndTime | Format-List\"", 1);
}

static void OnRepSleepStates(const char *arg) {
    (void)arg;
    RunCmd("powercfg /a", 1);
}

/* ===================== Extended toolset: Utilities ===================== */

static void OnEnvList(const char *arg) {
    (void)arg;
    HKEY hk;
    ReportClear();
    ReportAdd("=== User environment variables (HKCU\\Environment) ===");
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        for (DWORD i = 0;; i++) {
            char vn[256];
            DWORD vns = sizeof vn;
            DWORD type;
            char vd[4096];
            DWORD vds = sizeof vd;
            LONG r = RegEnumValueA(hk, i, vn, &vns, NULL, &type, (BYTE *)vd, &vds);
            if (r != ERROR_SUCCESS) break;
            if (vns >= sizeof vn) vn[sizeof vn - 1] = 0;
            if (vds >= sizeof vd) vd[sizeof vd - 1] = 0;
            ReportAdd("  %s = %s", vn, vd);
        }
        RegCloseKey(hk);
    }
    ReportAdd("");
    ReportAdd("=== System environment variables (HKLM) ===");
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
                      0, KEY_READ, &hk) == ERROR_SUCCESS) {
        for (DWORD i = 0;; i++) {
            char vn[256];
            DWORD vns = sizeof vn;
            DWORD type;
            char vd[4096];
            DWORD vds = sizeof vd;
            LONG r = RegEnumValueA(hk, i, vn, &vns, NULL, &type, (BYTE *)vd, &vds);
            if (r != ERROR_SUCCESS) break;
            if (vns >= sizeof vn) vn[sizeof vn - 1] = 0;
            if (vds >= sizeof vd) vd[sizeof vd - 1] = 0;
            ReportAdd("  %s = %s", vn, vd);
        }
        RegCloseKey(hk);
    }
    ShowReport();
}

static void OnEnvSet(const char *arg) {
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n < 2 || !parts[0][0]) {
        ReportClear();
        ReportAdd("Usage: set-env <NAME>|<value>");
        ReportAdd("Example: set-env MYTOOL|C:\\tools");
        ShowReport();
        return;
    }
    if (!SafeArg(parts[0])) {
        ReportClear();
        ReportAdd("Invalid variable name.");
        ShowReport();
        return;
    }
    WriteRegString(HKEY_CURRENT_USER, "Environment", parts[0], parts[1]);
    SetEnvironmentVariableA(parts[0], parts[1]);
    RefreshEnv();
    NotifyF("Environment variable set: %s", parts[0]);
}

static void OnEnvDelete(const char *arg) {
    if (!SafeArg(arg)) {
        ReportClear();
        ReportAdd("Usage: delete-env <NAME>");
        ShowReport();
        return;
    }
    DeleteRegValue(HKEY_CURRENT_USER, "Environment", arg);
    SetEnvironmentVariableA(arg, NULL);
    RefreshEnv();
    NotifyF("Environment variable deleted: %s", arg);
}

static void OnRunCommand(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: run-command <any-command>");
        ReportAdd("Example: run-command ipconfig /all");
        ShowReport();
        return;
    }
    RunCmd(arg, 1);
}

static void OnRunCommandAdmin(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: run-command-admin <any-command>");
        ShowReport();
        return;
    }
    RunElevatedCmd(arg);
}

static void OnRandomPassword(const char *arg) {
    int len = arg && *arg ? atoi(arg) : 16;
    if (len < 8) len = 8;
    if (len > 64) len = 64;
    const char *cs = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%^&*";
    unsigned char buf[64];
    HCRYPTPROV prov;
    if (!CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        ReportClear();
        ReportAdd("CryptAcquireContext failed.");
        ShowReport();
        return;
    }
    CryptGenRandom(prov, (DWORD)len, buf);
    CryptReleaseContext(prov, 0);
    char out[65];
    size_t csl = strlen(cs);
    for (int i = 0; i < len; i++) out[i] = cs[buf[i] % csl];
    out[len] = 0;
    ReportClear();
    ReportAdd("Random password (%d chars):", len);
    ReportAdd("%s", out);
    ShowReport();
}

static void OnUuidGenerate(const char *arg) {
    (void)arg;
    GUID g;
    CoCreateGuid(&g);
    char out[64];
    sprintf(out, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            g.Data1, g.Data2, g.Data3,
            g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
            g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    ReportClear();
    ReportAdd("New UUID: %s", out);
    ShowReport();
}

static void OnBeep(const char *arg) {
    int freq = 800, ms = 200;
    char parts[2][256];
    int n = SplitArgs(arg, parts, 2);
    if (n >= 1 && parts[0][0]) freq = atoi(parts[0]);
    if (n >= 2 && parts[1][0]) ms = atoi(parts[1]);
    if (freq < 37) freq = 800;
    if (ms < 1) ms = 200;
    Beep((DWORD)freq, (DWORD)ms);
    ReportClear();
    ReportAdd("Beep: %d Hz for %d ms", freq, ms);
    ShowReport();
}

static void OnBase64TextEncode(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: base64-text-encode <text>");
        ShowReport();
        return;
    }
    DWORD need = 0;
    CryptBinaryToStringA((const BYTE *)arg, (DWORD)strlen(arg),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &need);
    char *out = (char *)malloc(need ? need : 1);
    if (!out) return;
    CryptBinaryToStringA((const BYTE *)arg, (DWORD)strlen(arg),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out, &need);
    ReportClear();
    ReportAdd("Base64: %s", out);
    free(out);
    ShowReport();
}

static void OnBase64TextDecode(const char *arg) {
    if (!arg || !*arg) {
        ReportClear();
        ReportAdd("Usage: base64-text-decode <base64-text>");
        ShowReport();
        return;
    }
    DWORD need = 0;
    if (!CryptStringToBinaryA(arg, 0, CRYPT_STRING_BASE64, NULL, &need, NULL, NULL)) {
        ReportClear();
        ReportAdd("Invalid base64 text.");
        ShowReport();
        return;
    }
    BYTE *out = (BYTE *)malloc(need ? need + 1 : 2);
    if (!out) return;
    CryptStringToBinaryA(arg, 0, CRYPT_STRING_BASE64, out, &need, NULL, NULL);
    out[need] = 0;
    ReportClear();
    ReportAdd("Decoded: %s", (char *)out);
    free(out);
    ShowReport();
}

static void OnCleanPrefetch(const char *arg) {
    (void)arg;
    RunElevatedCmd("del /q /f \"%WINDIR%\\Prefetch\\*.pf\"");
}

static void OnCleanThumbcache(const char *arg) {
    (void)arg;
    RunCmd("del /q /f \"%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\"", 1);
}

static void OnRestartTimer(const char *arg) {
    int sec = arg && *arg ? atoi(arg) : 300;
    if (sec < 10) sec = 10;
    char cmd[400];
    snprintf(cmd, sizeof cmd, "shutdown /r /t %d /c \"WindowsControl: restarting in %d seconds\"", sec, sec);
    RunElevatedCmd(cmd);
}

void OnUtilInstall(const char *arg);
void OnUtilUninstall(const char *arg);
void OnUtilGui(const char *arg);
void OnUtilHelp(const char *arg);
void OnUtilAbout(const char *arg);

static const Command g_System[] = {
    {"system-properties", "Open the classic System Properties window", OnSysProps, 0},
    {"advanced-settings", "Advanced System Settings dialog", OnSysAdvanced, 0},
    {"environment-variables", "Edit user and system environment variables", OnSysEnvVar, 0},
    {"performance-options", "Visual effects and performance options", OnSysPerfOpt, 0},
    {"user-profiles", "Manage user profiles", OnSysUserProf, 0},
    {"startup-recovery", "Startup and Recovery settings", OnSysStartRec, 0},
    {"system-protection", "System Restore protection settings", OnSysProtection, 0},
    {"device-manager", "Manage hardware devices and drivers", OnSysDevMgr, 0},
    {"disk-management", "Manage disks, partitions and volumes", OnSysDiskMgmt, 0},
    {"computer-management", "All-in-one admin console", OnSysCompMgmt, 0},
    {"services-console", "Services console (services.msc)", OnSysServices, 0},
    {"task-scheduler", "Schedule automated tasks", OnSysTaskSchd, 0},
    {"event-viewer", "View system and application logs", OnSysEventVwr, 0},
    {"registry-editor", "Edit the Windows registry", OnSysRegedit, 0},
    {"group-policy", "Local Group Policy Editor", OnSysGpedit, 0},
    {"system-information", "Full system report (msinfo32)", OnSysMsinfo, 0},
    {"systeminfo", "System information via command line", OnSysInfoCmd, 0},
    {"directx-diagnostics", "DirectX diagnostic tool", OnSysDxdiag, 0},
    {"task-manager", "Open Task Manager", OnSysTaskmgr, 0},
    {"msconfig", "System Configuration utility", OnSysMsconfig, 0},
    {"winver", "Show Windows version dialog", OnSysWinver, 0},
    {"performance-monitor", "Live system performance graphs", OnSysPerfMon, 0},
    {"resource-monitor", "Detailed resource usage monitor", OnSysResMon, 0},
    {"restart-explorer", "Restart Windows Explorer (shell)", OnSysRestartExplorer, 0},
    {"empty-recycle-bin", "Empty the Recycle Bin", OnSysEmptyRecycle, 0},
    {"edit-hosts", "Edit the hosts file (admin)", OnSysEditHosts, 0},
    {"printers-settings", "Printers & scanners settings", OnSysPrinters, 0},
    {"taskbar-settings", "Taskbar settings", OnSysTaskbar, 0},
    {"bluetooth-settings", "Bluetooth & devices settings", OnSysBluetooth, 0},
    {"storage-settings", "Storage sense settings", OnSysStorage, 0},
    {"clipboard-settings", "Clipboard settings", OnSysClipboard, 0},
    {"about-windows", "About your PC", OnSysAboutWin, 0},
    {"shared-folders", "Shared Folders console", OnSysSharedFolders, 0},
    {"print-management", "Print Management console", OnSysPrintMgmt, 0},
    {"driver-list", "List installed drivers (pnputil)", OnDriverList, 0},
    {"driver-delete", "Delete a driver package (oemNN.inf)", OnDriverDelete, 1},
    {"activation-status", "Show Windows activation status", OnActivationStatus, 0},
};

static const Command g_Settings[] = {
    {"settings-home", "Open Windows Settings home", OnStHome, 0},
    {"display", "Display settings", OnStDisplay, 0},
    {"sound", "Sound settings", OnStSound, 0},
    {"notifications", "Notifications & actions", OnStNotif, 0},
    {"power-sleep", "Power & sleep settings", OnStPower, 0},
    {"battery-saver", "Battery saver settings", OnStBatterySaver, 0},
    {"storage", "Storage sense and drives", OnStStorage, 0},
    {"bluetooth", "Bluetooth & devices", OnStBluetooth, 0},
    {"printers-scanners", "Printers & scanners", OnStPrinters, 0},
    {"mouse", "Mouse & touchpad settings", OnStMouse, 0},
    {"typing", "Typing settings", OnStTyping, 0},
    {"touch", "Touch settings", OnStTouch, 0},
    {"usb", "USB settings", OnStUsb, 0},
    {"pen", "Pen & Windows Ink", OnStPen, 0},
    {"autoplay", "AutoPlay settings", OnStAutoPlay, 0},
    {"network", "Network & internet", OnStNetwork, 0},
    {"wifi", "Wi-Fi settings", OnStWifi, 0},
    {"ethernet", "Ethernet settings", OnStEthernet, 0},
    {"cellular", "Cellular settings", OnStCellular, 0},
    {"vpn", "VPN settings", OnStVpn, 0},
    {"proxy", "Proxy settings", OnStProxy, 0},
    {"airplane-mode", "Airplane mode settings", OnStAirplane, 0},
    {"mobile-hotspot", "Mobile hotspot settings", OnStMobileHotspot, 0},
    {"data-usage", "Data usage settings", OnStDataUsage, 0},
    {"background", "Desktop background settings", OnStBackground, 0},
    {"colors", "Colors & appearance", OnStColors, 0},
    {"lockscreen", "Lock screen settings", OnStLockScreen, 0},
    {"themes", "Themes", OnStThemes, 0},
    {"start", "Start menu settings", OnStStart, 0},
    {"taskbar", "Taskbar settings", OnStTaskbar, 0},
    {"apps", "Installed apps & features", OnStApps, 0},
    {"default-apps", "Default apps", OnStDefaultApps, 0},
    {"apps-websites", "Apps for websites", OnStAppsWebsites, 0},
    {"optional-features", "Optional features manager", OnStOptFeat, 0},
    {"startup-apps", "Startup apps settings", OnStStartupApps, 0},
    {"accounts", "Your Microsoft account info", OnStAccounts, 0},
    {"signin-options", "Sign-in options", OnStSignin, 0},
    {"windows-hello", "Windows Hello settings", OnStHello, 0},
    {"email-accounts", "Email & accounts", OnStEmail, 0},
    {"work-school", "Access work or school", OnStWorkSchool, 0},
    {"other-users", "Family & other users", OnStOtherUsers, 0},
    {"sync-settings", "Sync your settings", OnStSync, 0},
    {"family-options", "Family options", OnStFamilies, 0},
    {"time-language", "Date & time settings", OnStTime, 0},
    {"language", "Language & region settings", OnStLanguage, 0},
    {"gaming", "Gaming & game bar settings", OnStGaming, 0},
    {"game-mode", "Game Mode settings", OnStGameMode, 0},
    {"broadcasting", "Broadcasting settings", OnStBroadcasting, 0},
    {"xbox-networking", "Xbox networking", OnStXboxNet, 0},
    {"accessibility", "Accessibility (ease of access)", OnStAccess, 0},
    {"magnifier", "Magnifier settings", OnStMagnifier, 0},
    {"narrator", "Narrator settings", OnStNarrator, 0},
    {"high-contrast", "High contrast settings", OnStHighContrast, 0},
    {"closed-captions", "Closed captions settings", OnStClosedCaptions, 0},
    {"keyboard-access", "Accessibility keyboard settings", OnStKeyboardAccess, 0},
    {"mouse-access", "Accessibility mouse settings", OnStMouseAccess, 0},
    {"audio-access", "Accessibility audio settings", OnStAudioAccess, 0},
    {"display-access", "Accessibility display settings", OnStDisplayAccess, 0},
    {"privacy", "Privacy settings", OnStPrivacy, 0},
    {"location", "Location privacy settings", OnStLocation, 0},
    {"camera", "Camera privacy settings", OnStCamera, 0},
    {"microphone", "Microphone privacy settings", OnStMicrophone, 0},
    {"speech", "Speech, inking & typing privacy", OnStSpeech, 0},
    {"activity-history", "Activity history settings", OnStActivityHistory, 0},
    {"background-apps", "Background apps settings", OnStBackgroundApps, 0},
    {"documents", "Documents privacy settings", OnStDocuments, 0},
    {"downloads", "Downloads privacy settings", OnStDownloads, 0},
    {"pictures", "Pictures privacy settings", OnStPictures, 0},
    {"videos", "Videos privacy settings", OnStVideos, 0},
    {"clipboard", "Clipboard history settings", OnStClipboard, 0},
    {"focus-assist", "Focus assist settings", OnStFocus, 0},
    {"projecting", "Projecting to this PC", OnStProject, 0},
    {"tablet-mode", "Tablet mode settings", OnStTablet, 0},
    {"multitasking", "Multitasking settings", OnStMultitasking, 0},
    {"remote-desktop", "Remote Desktop settings", OnStRemoteDesktop, 0},
    {"troubleshoot", "Troubleshooters", OnStTrouble, 0},
    {"recovery", "Recovery options", OnStRecovery, 0},
    {"windows-update", "Windows Update", OnStUpdate, 0},
    {"backup", "Backup settings", OnStBackup, 0},
    {"about", "About your PC", OnStAboutWin, 0},
    {"phone-link", "Phone Link settings", OnStPhoneLink, 0},
    {"nearby-sharing", "Nearby sharing settings", OnStNearby, 0},
    {"night-light", "Night light settings", OnStNightLight, 0},
    {"gpu", "Graphics (GPU) settings", OnStGpu, 0},
    {"display-advanced", "Advanced display settings", OnStDisplayAdvanced, 0},
    {"captures-settings", "Gaming captures settings", OnStCaptures, 0},
    {"app-volume-settings", "App volume and device settings", OnStAppVolume, 0},
    {"calendar-privacy", "Calendar privacy settings", OnStCalendar, 0},
    {"contacts-privacy", "Contacts privacy settings", OnStContacts, 0},
    {"email-privacy", "Email privacy settings", OnStEmailP, 0},
    {"call-history-privacy", "Call history privacy settings", OnStCallHistory, 0},
    {"radios-privacy", "Radios privacy settings", OnStRadios, 0},
    {"notifications-privacy", "Notifications privacy settings", OnStNotifP, 0},
    {"feedback-privacy", "Feedback and diagnostics privacy", OnStFeedback, 0},
    {"diagnostics-privacy", "Diagnostics data settings", OnStDiagnostics, 0},
    {"network-advanced", "Advanced network settings", OnStNetAdvanced, 0},
    {"auto-file-downloads-privacy", "Auto file downloads privacy", OnStAutoDl, 0},
    {"windows-update-history", "Windows Update history", OnSP_windows_update_history, 0},
    {"windows-update-restart-options", "Windows Update restart options", OnSP_windows_update_restart_options, 0},
    {"windows-update-optional", "Optional Windows updates", OnSP_windows_update_optional, 0},
    {"windows-update-advanced", "Advanced Windows Update options", OnSP_windows_update_advanced, 0},
    {"windows-update-active-hours", "Windows Update active hours", OnSP_windows_update_active_hours, 0},
    {"windows-update-pause", "Pause Windows updates", OnSP_windows_update_pause, 0},
    {"windows-update-delivery-optimization", "Delivery optimization", OnSP_windows_update_delivery_optimization, 0},
    {"windows-update-targeted-version", "Targeted Windows version", OnSP_windows_update_targeted_version, 0},
    {"windows-update-troubleshoot", "Windows Update troubleshooter", OnSP_windows_update_troubleshoot, 0},
    {"start-layout", "Start menu layout settings", OnSP_start_layout, 0},
    {"personalization-taskbar", "Taskbar personalization", OnSP_personalization_taskbar, 0},
    {"accent-colors", "Accent color settings", OnSP_accent_colors, 0},
    {"desktop-background", "Desktop background settings", OnSP_desktop_background, 0},
    {"lockscreen-detailed", "Lock screen details", OnSP_lockscreen_detailed, 0},
    {"personalization-fonts", "Fonts settings", OnSP_personalization_fonts, 0},
    {"personalization-tasks", "Taskbar corner settings", OnSP_personalization_tasks, 0},
    {"dynamic-lock", "Dynamic lock settings", OnSP_dynamic_lock, 0},
    {"privacy-activity-history", "Activity history privacy", OnSP_privacy_activity_history, 0},
    {"location-privacy", "Location privacy", OnSP_location_privacy, 0},
    {"camera-privacy", "Camera privacy", OnSP_camera_privacy, 0},
    {"microphone-privacy", "Microphone privacy", OnSP_microphone_privacy, 0},
    {"privacy-voice-activation", "Voice activation privacy", OnSP_privacy_voice_activation, 0},
    {"speech-privacy", "Speech privacy", OnSP_speech_privacy, 0},
    {"privacy-account-info", "Account info privacy", OnSP_privacy_account_info, 0},
    {"privacy-tasks", "Tasks privacy", OnSP_privacy_tasks, 0},
    {"privacy-messaging", "Messaging privacy", OnSP_privacy_messaging, 0},
    {"privacy-other-devices", "Other devices privacy", OnSP_privacy_other_devices, 0},
    {"privacy-app-diagnostics", "App diagnostics privacy", OnSP_privacy_app_diagnostics, 0},
    {"documents-privacy", "Documents privacy", OnSP_documents_privacy, 0},
    {"pictures-privacy", "Pictures privacy", OnSP_pictures_privacy, 0},
    {"videos-privacy", "Videos privacy", OnSP_videos_privacy, 0},
    {"privacy-notifications", "Notification privacy per app", OnSP_privacy_notifications, 0},
    {"privacy-screenshots", "Screenshots privacy", OnSP_privacy_screenshots, 0},
    {"background-apps-privacy", "Background apps privacy", OnSP_background_apps_privacy, 0},
    {"privacy-devices", "Devices privacy", OnSP_privacy_devices, 0},
    {"privacy-graphics-capture", "Graphics capture privacy", OnSP_privacy_graphics_capture, 0},
    {"privacy-search", "Search permissions", OnSP_privacy_search, 0},
    {"privacy-broadcasting", "Broadcasting privacy", OnSP_privacy_broadcasting, 0},
    {"account-your-info", "Your account info", OnSP_account_your_info, 0},
    {"account-passwordless", "Passwordless account", OnSP_account_passwordless, 0},
    {"account-payment", "Payment methods", OnSP_account_payment, 0},
    {"family-group", "Family group settings", OnSP_family_group, 0},
    {"language-date-time", "Date and time settings", OnSP_language_date_time, 0},
    {"language-region", "Region and language", OnSP_language_region, 0},
    {"language-region-format", "Regional format", OnSP_language_region_format, 0},
    {"language-options", "Language options", OnSP_language_options, 0},
    {"language-keyboard", "Keyboard language settings", OnSP_language_keyboard, 0},
    {"speech-language", "Speech language settings", OnSP_speech_language, 0},
    {"language-handwriting", "Handwriting settings", OnSP_language_handwriting, 0},
    {"typing-language", "Typing settings", OnSP_typing_language, 0},
    {"accessibility-display", "Display accessibility", OnSP_accessibility_display, 0},
    {"accessibility-text-size", "Text size settings", OnSP_accessibility_text_size, 0},
    {"accessibility-visual-effects", "Visual effects accessibility", OnSP_accessibility_visual_effects, 0},
    {"accessibility-mouse-pointer", "Mouse pointer accessibility", OnSP_accessibility_mouse_pointer, 0},
    {"accessibility-text-cursor", "Text cursor settings", OnSP_accessibility_text_cursor, 0},
    {"accessibility-color-filters", "Color filters", OnSP_accessibility_color_filters, 0},
    {"accessibility-keyboard", "Keyboard accessibility", OnSP_accessibility_keyboard, 0},
    {"accessibility-mouse", "Mouse accessibility", OnSP_accessibility_mouse, 0},
    {"accessibility-eye-control", "Eye control", OnSP_accessibility_eye_control, 0},
    {"accessibility-audio", "Audio accessibility", OnSP_accessibility_audio, 0},
    {"accessibility-captioning", "Captioning settings", OnSP_accessibility_captioning, 0},
    {"accessibility-voice", "Voice accessibility", OnSP_accessibility_voice, 0},
    {"accessibility-privacy", "Accessibility privacy", OnSP_accessibility_privacy, 0},
    {"accessibility-cursor", "Cursor settings", OnSP_accessibility_cursor, 0},
    {"gaming-game-bar", "Game Bar settings", OnSP_gaming_game_bar, 0},
    {"gaming-game-dvr", "Game DVR settings", OnSP_gaming_game_dvr, 0},
    {"gaming-game-mode", "Game Mode settings", OnSP_gaming_game_mode, 0},
    {"gaming-controllers", "Game controllers", OnSP_gaming_controllers, 0},
    {"apps-installed", "Installed apps", OnSP_apps_installed, 0},
    {"apps-default-browser", "Default browser app", OnSP_apps_default_browser, 0},
    {"apps-default-email", "Default email app", OnSP_apps_default_email, 0},
    {"apps-default-media", "Default media player", OnSP_apps_default_media, 0},
    {"apps-default-maps", "Default maps app", OnSP_apps_default_maps, 0},
    {"apps-default-music", "Default music app", OnSP_apps_default_music, 0},
    {"apps-default-photos", "Default photos app", OnSP_apps_default_photos, 0},
    {"apps-default-video", "Default video player", OnSP_apps_default_video, 0},
    {"apps-alerts", "App alerts settings", OnSP_apps_alerts, 0},
    {"apps-startup", "App startup settings", OnSP_apps_startup, 0},
    {"apps-clipboard", "Clipboard app settings", OnSP_apps_clipboard, 0},
    {"devices-bluetooth", "Bluetooth devices", OnSP_devices_bluetooth, 0},
    {"devices-printers", "Printers and scanners", OnSP_devices_printers, 0},
    {"devices-mouse", "Mouse settings", OnSP_devices_mouse, 0},
    {"devices-touchpad", "Touchpad settings", OnSP_devices_touchpad, 0},
    {"devices-typing", "Typing devices settings", OnSP_devices_typing, 0},
    {"devices-pen", "Pen and Windows Ink", OnSP_devices_pen, 0},
    {"devices-autoplay", "AutoPlay device settings", OnSP_devices_autoplay, 0},
    {"devices-multitasking", "Multitasking device settings", OnSP_devices_multitasking, 0},
    {"devices-project", "Project to this PC", OnSP_devices_project, 0},
    {"devices-cameras", "Camera settings", OnSP_devices_cameras, 0},
    {"devices-touch", "Touch settings", OnSP_devices_touch, 0},
    {"devices-usb", "USB device settings", OnSP_devices_usb, 0},
    {"network-wifi", "Wi-Fi settings", OnSP_network_wifi, 0},
    {"network-ethernet", "Ethernet settings", OnSP_network_ethernet, 0},
    {"network-vpn", "VPN settings", OnSP_network_vpn, 0},
    {"network-airplane", "Airplane mode", OnSP_network_airplane, 0},
    {"network-mobile-hotspot", "Mobile hotspot", OnSP_network_mobile_hotspot, 0},
    {"network-proxy", "Proxy settings", OnSP_network_proxy, 0},
    {"network-dialup", "Dial-up connections", OnSP_network_dialup, 0},
    {"network-cellular", "Cellular settings", OnSP_network_cellular, 0},
    {"network-data-usage", "Data usage", OnSP_network_data_usage, 0},
    {"network-nearby", "Nearby sharing", OnSP_network_nearby, 0},
    {"system-storage-sense", "Storage Sense", OnSP_system_storage_sense, 0},
    {"system-storage-cleanup", "Storage cleanup", OnSP_system_storage_cleanup, 0},
    {"system-storage-optimization", "Storage optimization", OnSP_system_storage_optimization, 0},
    {"system-battery-saver", "Battery saver", OnSP_system_battery_saver, 0},
    {"system-battery-usage", "Battery usage", OnSP_system_battery_usage, 0},
    {"system-power", "Power settings", OnSP_system_power, 0},
    {"system-multitasking", "Multitasking settings", OnSP_system_multitasking, 0},
    {"system-multitasking-snap", "Snap windows settings", OnSP_system_multitasking_snap, 0},
    {"system-multitasking-display", "Multitasking across displays", OnSP_system_multitasking_display, 0},
    {"system-clipboard", "Clipboard settings", OnSP_system_clipboard, 0},
    {"system-remote-desktop", "Remote Desktop settings", OnSP_system_remote_desktop, 0},
    {"system-recovery", "Recovery options", OnSP_system_recovery, 0},
    {"system-activation", "Activation settings", OnSP_system_activation, 0},
    {"system-troubleshooters", "Troubleshooters", OnSP_system_troubleshooters, 0},
    {"system-about", "About your PC", OnSP_system_about, 0},
    {"system-backup", "Backup settings", OnSP_system_backup, 0},
    {"system-developers", "Developer Mode settings", OnSP_system_developers, 0},
    {"sync", "Sync your settings", OnSP_sync, 0},
    {"taskbar-settings", "Taskbar behavior settings", OnSP_taskbar_settings, 0},
    {"insider-program", "Windows Insider Program", OnSP_insider_program, 0},
    {"sound-devices", "Sound device settings", OnSP_sound_devices, 0},
    {"windows-update-pause-reboot", "Pause updates before reboot", OnSP_windows_update_pause_reboot, 0},
    {"windows-update-reboot-schedule", "Schedule update reboot", OnSP_windows_update_reboot_schedule, 0},
    {"windows-update-reboot", "Reboot for updates", OnSP_windows_update_reboot, 0},
    {"windows-update-reboot-week", "Choose reboot week", OnSP_windows_update_reboot_week, 0},
    {"windows-update-reboot-day", "Choose reboot day", OnSP_windows_update_reboot_day, 0},
    {"windows-update-reboot-time", "Choose reboot time", OnSP_windows_update_reboot_time, 0},
    {"windows-update-restart", "Restart for updates", OnSP_windows_update_restart, 0},
    {"windows-update-restart-required", "Restart required page", OnSP_windows_update_restart_required, 0},
    {"windows-update-snooze", "Snooze updates", OnSP_windows_update_snooze, 0},
    {"windows-update-snooze-reboot", "Snooze reboot", OnSP_windows_update_snooze_reboot, 0},
    {"windows-update-install", "Install updates now", OnSP_windows_update_install, 0},
    {"windows-update-download", "Download updates page", OnSP_windows_update_download, 0},
    {"windows-update-uninstall", "Uninstall updates", OnSP_windows_update_uninstall, 0},
    {"windows-update-drivers", "Driver updates", OnSP_windows_update_drivers, 0},
    {"windows-update-hide", "Hidden updates", OnSP_windows_update_hide, 0},
    {"windows-update-locked", "Locked update settings", OnSP_windows_update_locked, 0},
    {"windows-update-manual", "Manual update check", OnSP_windows_update_manual, 0},
    {"windows-update-compliance", "Update compliance", OnSP_windows_update_compliance, 0},
    {"windows-update-preview", "Preview updates", OnSP_windows_update_preview, 0},
    {"windows-update-experimental", "Experimental updates", OnSP_windows_update_experimental, 0},
    {"windows-update-cooldown", "Update cooldown", OnSP_windows_update_cooldown, 0},
    {"windows-update-registry-recovery", "Registry recovery", OnSP_windows_update_registry_recovery, 0},
    {"windows-update-delivery-optimization-advanced", "Delivery optimization advanced", OnSP_windows_update_delivery_optimization_advanced, 0},
    {"windows-update-delivery-optimization-activity", "Delivery optimization activity", OnSP_windows_update_delivery_optimization_activity, 0},
    {"bluetooth-add-device", "Add a Bluetooth device", OnSP_bluetooth_add_device, 0},
    {"bluetooth-discovered", "Discovered Bluetooth devices", OnSP_bluetooth_discovered, 0},
    {"bluetooth-paired", "Paired Bluetooth devices", OnSP_bluetooth_paired, 0},
    {"bluetooth-other-devices", "Other Bluetooth devices", OnSP_bluetooth_other_devices, 0},
    {"bluetooth-firmware", "Bluetooth firmware", OnSP_bluetooth_firmware, 0},
    {"network-adapters-advanced", "Advanced network adapters", OnSP_network_adapters_advanced, 0},
    {"network-dns-advanced", "Advanced DNS settings", OnSP_network_dns_advanced, 0},
    {"network-proxy-advanced", "Advanced proxy settings", OnSP_network_proxy_advanced, 0},
    {"network-manual-proxy", "Manual proxy setup", OnSP_network_manual_proxy, 0},
    {"network-auto-proxy", "Automatic proxy setup", OnSP_network_auto_proxy, 0},
    {"network-pac-url", "PAC script URL", OnSP_network_pac_url, 0},
    {"network-connection-details", "Network connection details", OnSP_network_connection_details, 0},
    {"network-sim", "SIM card settings", OnSP_network_sim, 0},
    {"network-roaming", "Roaming settings", OnSP_network_roaming, 0},
    {"network-wifi-settings", "Wi-Fi network settings", OnSP_network_wifi_settings, 0},
    {"devices-audio", "Audio device settings", OnSP_devices_audio, 0},
    {"devices-display", "Display device settings", OnSP_devices_display, 0},
    {"devices-display-performance", "Display performance", OnSP_devices_display_performance, 0},
    {"devices-nfc", "NFC settings", OnSP_devices_nfc, 0},
    {"devices-stylus", "Stylus settings", OnSP_devices_stylus, 0},
    {"devices-android", "Android device settings", OnSP_devices_android, 0},
    {"devices-car", "Car device settings", OnSP_devices_car, 0},
    {"devices-device-properties", "Device properties", OnSP_devices_device_properties, 0},
    {"devices-other-devices", "Other devices", OnSP_devices_other_devices, 0},
    {"devices-servicelist", "Device service list", OnSP_devices_servicelist, 0},
    {"personalization-getting-started", "Getting started personalization", OnSP_personalization_getting_started, 0},
    {"personalization-tile-launcher", "Tile launcher", OnSP_personalization_tile_launcher, 0},
    {"personalization-variants", "Personalization variants", OnSP_personalization_variants, 0},
    {"personalization-pane", "Personalization pane", OnSP_personalization_pane, 0},
    {"personalization-fon", "Personalization fonts", OnSP_personalization_fon, 0},
    {"sound-settings", "Sound settings page", OnSP_sound_settings, 0},
    {"sound-volume", "Sound volume page", OnSP_sound_volume, 0},
    {"sound-audio-bluetooth", "Bluetooth audio settings", OnSP_sound_audio_bluetooth, 0},
    {"sound-audio-usb", "USB audio settings", OnSP_sound_audio_usb, 0},
    {"sound-audio-output", "Audio output devices", OnSP_sound_audio_output, 0},
    {"sound-audio-input", "Audio input devices", OnSP_sound_audio_input, 0},
    {"taskbar-personalize", "Personalize the taskbar", OnSP_taskbar_personalize, 0},
    {"taskbar-tray", "Taskbar tray settings", OnSP_taskbar_tray, 0},
    {"taskbar-behavior", "Taskbar behavior", OnSP_taskbar_behavior, 0},
    {"taskbar-system-tray", "System tray icons", OnSP_taskbar_system_tray, 0},
    {"taskbar-notification-center", "Notification center settings", OnSP_taskbar_notification_center, 0},
    {"taskbar-caption", "Taskbar caption", OnSP_taskbar_caption, 0},
    {"taskbar-search-settings", "Taskbar search settings", OnSP_taskbar_search_settings, 0},
    {"taskbar-corner", "Taskbar corner icons", OnSP_taskbar_corner, 0},
    {"default-apps-browser-desktop", "Default browser (desktop)", OnSP_default_apps_browser_desktop, 0},
    {"default-apps-email-desktop", "Default email (desktop)", OnSP_default_apps_email_desktop, 0},
    {"default-apps-media-desktop", "Default media player (desktop)", OnSP_default_apps_media_desktop, 0},
    {"default-apps-maps-desktop", "Default maps (desktop)", OnSP_default_apps_maps_desktop, 0},
    {"default-apps-music-desktop", "Default music (desktop)", OnSP_default_apps_music_desktop, 0},
    {"default-apps-photos-desktop", "Default photos (desktop)", OnSP_default_apps_photos_desktop, 0},
    {"default-apps-video-desktop", "Default video (desktop)", OnSP_default_apps_video_desktop, 0},
    {"default-apps-search", "Default search app", OnSP_default_apps_search, 0},
    {"printers-add", "Add a printer", OnSP_printers_add, 0},
    {"printers-advanced", "Printer advanced settings", OnSP_printers_advanced, 0},
    {"printers-default", "Default printer", OnSP_printers_default, 0},
    {"printers-scanner", "Scanner settings", OnSP_printers_scanner, 0},
    {"region-display-language", "Set display language", OnSP_region_display_language, 0},
    {"region-display-language-options", "Display language options", OnSP_region_display_language_options, 0},
    {"region-format-region", "Set region", OnSP_region_format_region, 0},
    {"region-language-list", "Languages list", OnSP_region_language_list, 0},
    {"cortana", "Cortana settings", OnSP_cortana, 0},
    {"cortana-suggestions", "Cortana suggestions", OnSP_cortana_suggestions, 0},
    {"cortana-account", "Cortana account", OnSP_cortana_account, 0},
    {"cortana-skills", "Cortana skills", OnSP_cortana_skills, 0},
    {"cortana-notifications", "Cortana notifications", OnSP_cortana_notifications, 0},
    {"cortana-permissions", "Cortana permissions", OnSP_cortana_permissions, 0},
    {"holographic", "Holographic settings", OnSP_holographic, 0},
    {"holographic-headset", "Holographic headset", OnSP_holographic_headset, 0},
    {"holographic-display", "Holographic display", OnSP_holographic_display, 0},
    {"holographic-audio", "Holographic audio", OnSP_holographic_audio, 0},
    {"holographic-room", "Holographic room", OnSP_holographic_room, 0},
    {"surfacehub", "Surface Hub settings", OnSP_surfacehub, 0},
    {"surfacehub-calling", "Surface Hub calling", OnSP_surfacehub_calling, 0},
    {"surfacehub-sessions", "Surface Hub sessions", OnSP_surfacehub_sessions, 0},
    {"surfacehub-device", "Surface Hub device", OnSP_surfacehub_device, 0},
    {"gaming-trueplay", "TruePlay protection", OnSP_gaming_trueplay, 0},
    {"gaming-audio-capture", "Game audio capture", OnSP_gaming_audio_capture, 0},
    {"gaming-podcasts", "Game podcasts", OnSP_gaming_podcasts, 0},
    {"storage-policies", "Storage policies", OnSP_storage_policies, 0},
    {"storage-devices", "Storage devices", OnSP_storage_devices, 0},
    {"storage-usage", "Storage usage", OnSP_storage_usage, 0},
    {"session", "Session settings", OnSP_session, 0},
    {"session-devices", "Session devices", OnSP_session_devices, 0},
    {"session-connect", "Session connect", OnSP_session_connect, 0},
    {"tips", "Windows tips", OnSP_tips, 0},
    {"tips-getting-started", "Tips getting started", OnSP_tips_getting_started, 0},
    {"shared-folders", "Shared folders", OnSP_shared_folders, 0},
    {"shared-folders-pc", "Shared folders on PC", OnSP_shared_folders_pc, 0},
    {"crossrealm-devices", "Cross realm devices", OnSP_crossrealm_devices, 0},
    {"emergency-alerts", "Emergency alerts", OnSP_emergency_alerts, 0},
    {"phone", "Phone settings", OnSP_phone, 0},
    {"windows-anywhere", "Windows Anywhere", OnSP_windows_anywhere, 0},
    {"workloads", "Workloads settings", OnSP_workloads, 0},
    {"windows-update-optional-drivers", "Optional driver updates", OnSP_windows_update_optional_drivers, 0},
    {"windows-update-optional-apps", "Optional app updates", OnSP_windows_update_optional_apps, 0},
    {"windows-update-optional-firmware", "Optional firmware updates", OnSP_windows_update_optional_firmware, 0},
    {"windows-update-optional-security", "Optional security updates", OnSP_windows_update_optional_security, 0},
    {"windows-update-optional-critical", "Optional critical updates", OnSP_windows_update_optional_critical, 0},
    {"windows-update-optional-bugfixes", "Optional bugfix updates", OnSP_windows_update_optional_bugfixes, 0},
    {"windows-update-optional-tools", "Optional tool updates", OnSP_windows_update_optional_tools, 0},
    {"windows-update-optional-compatibility", "Optional compatibility updates", OnSP_windows_update_optional_compatibility, 0},
    {"windows-update-optional-enhancements", "Optional enhancement updates", OnSP_windows_update_optional_enhancements, 0},
    {"windows-update-optional-featurepacks", "Optional feature packs", OnSP_windows_update_optional_featurepacks, 0},
    {"windows-update-optional-servicing", "Optional servicing updates", OnSP_windows_update_optional_servicing, 0},
    {"windows-update-optional-previews", "Optional preview updates", OnSP_windows_update_optional_previews, 0},
    {"windows-update-optional-required", "Required optional updates", OnSP_windows_update_optional_required, 0},
    {"windows-update-active-hours-options", "Active hours options", OnSP_windows_update_active_hours_options, 0},
    {"windows-update-advanced-options-options", "Advanced options subpage", OnSP_windows_update_advanced_options_options, 0},
    {"windows-update-targeted-version-options", "Targeted version options", OnSP_windows_update_targeted_version_options, 0},
    {"windows-update-troubleshoot-reset", "Reset Windows Update", OnSP_windows_update_troubleshoot_reset, 0},
    {"windows-update-limit-connectivity", "Limit update connectivity", OnSP_windows_update_limit_connectivity, 0},
    {"windows-update-suppress-driver-update", "Suppress driver updates", OnSP_windows_update_suppress_driver_update, 0},
    {"windows-update-usb-device", "Update USB devices", OnSP_windows_update_usb_device, 0},
    {"windows-update-logout", "Updates on logout", OnSP_windows_update_logout, 0},
    {"windows-update-logon", "Updates on logon", OnSP_windows_update_logon, 0},
    {"windows-update-mobile-hotspot", "Updates over mobile hotspot", OnSP_windows_update_mobile_hotspot, 0},
    {"windows-update-saved", "Saved update settings", OnSP_windows_update_saved, 0},
    {"windows-update-software-policy-notification", "Update policy notification", OnSP_windows_update_software_policy_notification, 0},
    {"windows-update-stuck", "Stuck update recovery", OnSP_windows_update_stuck, 0},
    {"windows-update-update-comp", "Update compatibility", OnSP_windows_update_update_comp, 0},
    {"bluetooth-device", "Bluetooth device details", OnSP_bluetooth_device, 0},
    {"bluetooth-settings", "Bluetooth settings page", OnSP_bluetooth_settings, 0},
    {"bluetooth-events", "Bluetooth events", OnSP_bluetooth_events, 0},
    {"network-connections", "All network connections", OnSP_network_connections, 0},
    {"network-connectivity", "Network connectivity status", OnSP_network_connectivity, 0},
    {"network-devices", "Network devices", OnSP_network_devices, 0},
    {"network-device-usage", "Network device usage", OnSP_network_device_usage, 0},
    {"network-operator-messages", "Operator messages", OnSP_network_operator_messages, 0},
    {"network-wifi-hotspot", "Wi-Fi hotspot settings", OnSP_network_wifi_hotspot, 0},
    {"devices-tel", "Telephony devices", OnSP_devices_tel, 0},
    {"devices-rsd", "Removable storage devices", OnSP_devices_rsd, 0},
    {"devices-telemetry", "Device telemetry", OnSP_devices_telemetry, 0},
    {"devices-mobilephone", "Mobile phone devices", OnSP_devices_mobilephone, 0},
    {"devices-sim", "SIM settings", OnSP_devices_sim, 0},
    {"devices-sim-settings", "SIM card details", OnSP_devices_sim_settings, 0},
    {"apps-features-app", "App details page", OnSP_apps_features_app, 0},
    {"apps-features-advanced", "App advanced options", OnSP_apps_features_advanced, 0},
    {"apps-notification-settings", "App notification settings", OnSP_apps_notification_settings, 0},
    {"apps-advanced-notification-settings", "Advanced notification settings", OnSP_apps_advanced_notification_settings, 0},
    {"face-enrollment", "Windows Hello face enrollment", OnSP_face_enrollment, 0},
    {"fingerprint-enrollment", "Windows Hello fingerprint enrollment", OnSP_fingerprint_enrollment, 0},
    {"security-key-enrollment", "Security key enrollment", OnSP_security_key_enrollment, 0},
    {"create-security-key", "Create a security key", OnSP_create_security_key, 0},
    {"family-sign-in", "Family sign-in", OnSP_family_sign_in, 0},
    {"family-manage", "Manage family", OnSP_family_manage, 0},
    {"family-invite", "Invite family members", OnSP_family_invite, 0},
    {"family-group-options", "Family group options", OnSP_family_group_options, 0},
    {"family-suggestions", "Family suggestions", OnSP_family_suggestions, 0},
    {"family-account", "Family account settings", OnSP_family_account, 0},
    {"region-relaunch", "Restart for language changes", OnSP_region_relaunch, 0},
    {"region-language-options", "Language options page", OnSP_region_language_options, 0},
    {"region-set-region-options", "Region options", OnSP_region_set_region_options, 0},
    {"easeofaccess-display", "Legacy accessibility display", OnSP_easeofaccess_display, 0},
    {"easeofaccess-narrator", "Legacy Narrator settings", OnSP_easeofaccess_narrator, 0},
    {"easeofaccess-magnifier", "Legacy Magnifier settings", OnSP_easeofaccess_magnifier, 0},
    {"easeofaccess-closedcaptioning", "Legacy closed captions", OnSP_easeofaccess_closedcaptioning, 0},
    {"easeofaccess-keyboard", "Legacy keyboard accessibility", OnSP_easeofaccess_keyboard, 0},
    {"easeofaccess-mouse", "Legacy mouse accessibility", OnSP_easeofaccess_mouse, 0},
    {"easeofaccess-otheroptions", "Legacy other options", OnSP_easeofaccess_otheroptions, 0},
    {"easeofaccess-highcontrast", "Legacy high contrast", OnSP_easeofaccess_highcontrast, 0},
    {"easeofaccess-colorfilter", "Legacy color filters", OnSP_easeofaccess_colorfilter, 0},
    {"easeofaccess-cursor", "Legacy cursor settings", OnSP_easeofaccess_cursor, 0},
    {"easeofaccess-textcursor", "Legacy text cursor", OnSP_easeofaccess_textcursor, 0},
    {"easeofaccess-eyecontrol", "Legacy eye control", OnSP_easeofaccess_eyecontrol, 0},
    {"easeofaccess-audio", "Legacy audio accessibility", OnSP_easeofaccess_audio, 0},
    {"easeofaccess-speechrecognition", "Legacy speech recognition", OnSP_easeofaccess_speechrecognition, 0},
    {"easeofaccess-dictation", "Legacy dictation", OnSP_easeofaccess_dictation, 0},
    {"easeofaccess-visual", "Legacy visual settings", OnSP_easeofaccess_visual, 0},
    {"easeofaccess-hearing", "Legacy hearing settings", OnSP_easeofaccess_hearing, 0},
    {"taskbar-corner-overflow", "Taskbar corner overflow icons", OnSP_taskbar_corner_overflow, 0},
    {"taskbar-view", "Taskbar view button", OnSP_taskbar_view, 0},
    {"taskbar-widgets", "Taskbar widgets button", OnSP_taskbar_widgets, 0},
    {"taskbar-copilot", "Taskbar Copilot button", OnSP_taskbar_copilot, 0},
    {"taskbar-pinned", "Pinned taskbar apps", OnSP_taskbar_pinned, 0},
    {"printers-fax", "Fax settings", OnSP_printers_fax, 0},
    {"printers-shared", "Shared printers", OnSP_printers_shared, 0},
    {"printers-print-server", "Print server settings", OnSP_printers_print_server, 0},
    {"printers-fax-server", "Fax server settings", OnSP_printers_fax_server, 0},
    {"printers-printer-server", "Printer server details", OnSP_printers_printer_server, 0},
    {"printers-print-queue", "Print queue", OnSP_printers_print_queue, 0},
    {"cortana-language", "Cortana language", OnSP_cortana_language, 0},
    {"cortana-recent", "Cortana recent activity", OnSP_cortana_recent, 0},
    {"holographic-apps", "Holographic apps", OnSP_holographic_apps, 0},
    {"holographic-environment", "Holographic environment", OnSP_holographic_environment, 0},
    {"surfacehub-wifi", "Surface Hub Wi-Fi", OnSP_surfacehub_wifi, 0},
    {"surfacehub-casual", "Surface Hub casual mode", OnSP_surfacehub_casual, 0},
    {"workloads-ai", "AI workloads", OnSP_workloads_ai, 0},
    {"workloads-auto-settings", "Auto workloads settings", OnSP_workloads_auto_settings, 0},
    {"workloads-search", "Search workloads", OnSP_workloads_search, 0},
    {"workloads-apps", "Apps workloads", OnSP_workloads_apps, 0},
    {"workloads-data", "Data workloads", OnSP_workloads_data, 0},
    {"workloads-devices", "Devices workloads", OnSP_workloads_devices, 0},
    {"workloads-developers", "Developer workloads", OnSP_workloads_developers, 0},
    {"workloads-security", "Security workloads", OnSP_workloads_security, 0},
    {"wormhole", "Wormhole settings", OnSP_wormhole, 0},
    {"wormhole-account", "Wormhole account", OnSP_wormhole_account, 0},
    {"wormhole-device", "Wormhole device", OnSP_wormhole_device, 0},
    {"wormhole-bridge", "Wormhole bridge", OnSP_wormhole_bridge, 0},
    {"wormhole-display", "Wormhole display", OnSP_wormhole_display, 0},
    {"wormhole-experience", "Wormhole experience", OnSP_wormhole_experience, 0},
    {"wormhole-security", "Wormhole security", OnSP_wormhole_security, 0},
    {"wormhole-settings", "Wormhole settings page", OnSP_wormhole_settings, 0},
    {"clusters", "Storage clusters", OnSP_clusters, 0},
    {"clusters-keys", "Cluster keys", OnSP_clusters_keys, 0},
    {"clusters-resources", "Cluster resources", OnSP_clusters_resources, 0},
    {"clusters-health", "Cluster health", OnSP_clusters_health, 0},
    {"windows-anywhere-mobile", "Windows Anywhere mobile", OnSP_windows_anywhere_mobile, 0},
    {"windows-anywhere-account", "Windows Anywhere account", OnSP_windows_anywhere_account, 0},
    {"windows-anywhere-options", "Windows Anywhere options", OnSP_windows_anywhere_options, 0},
    {"windows-anywhere-devices", "Windows Anywhere devices", OnSP_windows_anywhere_devices, 0},
    {"session-disk", "Session disk settings", OnSP_session_disk, 0},
    {"tips-feedback", "Tips feedback", OnSP_tips_feedback, 0},
    {"tips-settings", "Tips settings page", OnSP_tips_settings, 0},
    {"maps", "Maps settings", OnSP_maps, 0},
    {"maps-download-maps", "Download offline maps", OnSP_maps_download_maps, 0},
    {"eas", "Enterprise access settings", OnSP_eas, 0},
    {"shared-folders-mobile", "Shared folders on mobile", OnSP_shared_folders_mobile, 0},
    {"workplace", "Access work or school settings", OnSP_workplace, 0},
    {"emailandaccounts", "Email & accounts settings", OnSP_emailandaccounts, 0},
    {"otherusers", "Family & other users settings", OnSP_otherusers, 0},
    {"assignedaccess", "Set up a kiosk (assigned access)", OnSP_assignedaccess, 0},
    {"signinoptions", "Sign-in options settings", OnSP_signinoptions, 0},
    {"signinoptions-dynamiclock", "Dynamic lock settings", OnSP_signinoptions_dynamiclock, 0},
    {"provisioning", "Provisioning package settings", OnSP_provisioning, 0},
    {"workplace-provisioning", "Work or school provisioning", OnSP_workplace_provisioning, 0},
    {"workplace-repairtoken", "Repair work or school account", OnSP_workplace_repairtoken, 0},
    {"appsforwebsites", "Apps for websites settings", OnSP_appsforwebsites, 0},
    {"defaultapps", "Default apps settings", OnSP_defaultapps, 0},
    {"defaultbrowsersettings", "Default browser settings (deprecated)", OnSP_defaultbrowsersettings, 0},
    {"videoplayback", "Video playback settings", OnSP_videoplayback, 0},
    {"controlcenter", "Control center settings", OnSP_controlcenter, 0},
    {"connecteddevices", "Connected devices settings", OnSP_connecteddevices, 0},
    {"camera-settings", "Camera device settings", OnSP_camera, 0},
    {"pen-button", "Pen shortcut button settings", OnSP_pen_button, 0},
    {"devicestyping-hwkbtextsuggestions", "Hardware keyboard text suggestions", OnSP_devicestyping_hwkbtextsuggestions, 0},
    {"wheel", "Wheel settings (Surface Dial)", OnSP_wheel, 0},
    {"mobile-devices-addphone", "Phone Link add phone", OnSP_mobile_devices_addphone, 0},
    {"mobile-devices-addphone-direct", "Phone Link add phone (direct)", OnSP_mobile_devices_addphone_direct, 0},
    {"deviceusage", "Device usage settings", OnSP_deviceusage, 0},
    {"easeofaccess-colorfilter-adaptivecolorlink", "Color filter adaptive link", OnSP_easeofaccess_colorfilter_adaptivecolorlink, 0},
    {"easeofaccess-colorfilter-bluelightlink", "Color filter blue light link", OnSP_easeofaccess_colorfilter_bluelightlink, 0},
    {"easeofaccess-hearingaids", "Hearing aids settings", OnSP_easeofaccess_hearingaids, 0},
    {"easeofaccess-mousepointer", "Mouse pointer and touch", OnSP_easeofaccess_mousepointer, 0},
    {"easeofaccess-narrator-isautostartenabled", "Narrator auto-start setting", OnSP_easeofaccess_narrator_isautostartenabled, 0},
    {"easeofaccess-visualeffects", "Visual effects settings", OnSP_easeofaccess_visualeffects, 0},
    {"easeofaccess-fonts", "Fonts settings (accessibility)", OnSP_easeofaccess_fonts, 0},
    {"extras", "Extras settings page", OnSP_extras, 0},
    {"family-group-ms", "Family Group settings", OnSP_family_group_ms, 0},
    {"quietmomentsgame", "Focus assist while gaming", OnSP_quietmomentsgame, 0},
    {"privacy-holographic-environment", "Holographic environment privacy", OnSP_privacy_holographic_environment, 0},
    {"holographic-management", "Holographic uninstall", OnSP_holographic_management, 0},
    {"holographic-startupandesktop", "Holographic startup and desktop", OnSP_holographic_startupandesktop, 0},
    {"network-status", "Network status page", OnSP_network_status, 0},
    {"network-advancedsettings", "Advanced network settings", OnSP_network_advancedsettings, 0},
    {"proximity", "Proximity sensing settings", OnSP_proximity, 0},
    {"network-directaccess", "DirectAccess settings", OnSP_network_directaccess, 0},
    {"wifi-provisioning", "Wi-Fi provisioning", OnSP_wifi_provisioning, 0},
    {"personalization-start-places", "Start folder shortcuts", OnSP_personalization_start_places, 0},
    {"personalization-touchkeyboard", "Touch keyboard settings", OnSP_personalization_touchkeyboard, 0},
    {"fonts", "Fonts settings", OnSP_fonts, 0},
    {"personalization-textinput", "Text input settings", OnSP_personalization_textinput, 0},
    {"personalization-textinput-copilot-hardwarekey", "Customize Copilot key", OnSP_personalization_textinput_copilot_hardwarekey, 0},
    {"personalization-lighting", "Dynamic lighting settings", OnSP_personalization_lighting, 0},
    {"privacy-accessoryapps", "Accessory apps privacy", OnSP_privacy_accessoryapps, 0},
    {"privacy-advertisingid", "Advertising ID privacy", OnSP_privacy_advertisingid, 0},
    {"privacy-automaticfiledownloads", "Automatic file downloads privacy", OnSP_privacy_automaticfiledownloads, 0},
    {"privacy-backgroundspatialperception", "Background spatial perception privacy", OnSP_privacy_backgroundspatialperception, 0},
    {"privacy-callhistory", "Call history privacy", OnSP_privacy_callhistory, 0},
    {"privacy-eyetracker", "Eye tracker privacy", OnSP_privacy_eyetracker, 0},
    {"privacy-broadfilesystemaccess", "File system privacy", OnSP_privacy_broadfilesystemaccess, 0},
    {"privacy-general", "General privacy settings", OnSP_privacy_general, 0},
    {"privacy-graphicscaptureprogrammatic", "Graphics capture privacy (programmatic)", OnSP_privacy_graphicscaptureprogrammatic, 0},
    {"privacy-graphicscapturewithoutborder", "Graphics capture without border", OnSP_privacy_graphicscapturewithoutborder, 0},
    {"privacy-motion", "Motion sensor privacy", OnSP_privacy_motion, 0},
    {"privacy-musiclibrary", "Music library privacy", OnSP_privacy_musiclibrary, 0},
    {"privacy-customdevices", "Other devices privacy", OnSP_privacy_customdevices, 0},
    {"privacy-phonecalls", "Phone calls privacy", OnSP_privacy_phonecalls, 0},
    {"search", "Search settings", OnSP_search, 0},
    {"search-moredetails", "Search more details", OnSP_search_moredetails, 0},
    {"search-permissions", "Search permissions", OnSP_search_permissions, 0},
    {"sound-defaultinputproperties", "Default microphone properties", OnSP_sound_defaultinputproperties, 0},
    {"sound-defaultoutputproperties", "Default audio output properties", OnSP_sound_defaultoutputproperties, 0},
    {"screenrotation", "Screen rotation settings", OnSP_screenrotation, 0},
    {"display-advancedgraphics-default", "Graphics default settings", OnSP_display_advancedgraphics_default, 0},
    {"batterysaver-settings", "Battery saver settings", OnSP_batterysaver_settings, 0},
    {"batterysaver-usagedetails", "Battery usage details", OnSP_batterysaver_usagedetails, 0},
    {"savelocations", "Default save locations", OnSP_savelocations, 0},
    {"deviceencryption", "Device encryption settings", OnSP_deviceencryption, 0},
    {"energyrecommendations", "Energy recommendations", OnSP_energyrecommendations, 0},
    {"quietmomentsscheduled", "Focus assist during these hours", OnSP_quietmomentsscheduled, 0},
    {"quietmomentspresentation", "Focus assist while duplicating display", OnSP_quietmomentspresentation, 0},
    {"multitasking-sgupdate", "Snap Groups update setting", OnSP_multitasking_sgupdate, 0},
    {"remotedesktop", "Remote Desktop settings", OnSP_remotedesktop, 0},
    {"presence", "Presence sensing settings", OnSP_presence, 0},
    {"storagerecommendations", "Storage recommendations", OnSP_storagerecommendations, 0},
    {"disksandvolumes", "Disks and volumes", OnSP_disksandvolumes, 0},
    {"regionlanguage-jpnime", "Japan IME settings", OnSP_regionlanguage_jpnime, 0},
    {"regionformatting", "Region formatting", OnSP_regionformatting, 0},
    {"keyboard-advanced", "Advanced keyboard settings", OnSP_keyboard_advanced, 0},
    {"regionlanguage-bpmfime", "Bopomofo IME settings", OnSP_regionlanguage_bpmfime, 0},
    {"regionlanguage-cangjieime", "Cangjie IME settings", OnSP_regionlanguage_cangjieime, 0},
    {"regionlanguage-chsime-wubi-udp", "Wubi IME user-defined phrases", OnSP_regionlanguage_chsime_wubi_udp, 0},
    {"regionlanguage-quickime", "Quick IME settings", OnSP_regionlanguage_quickime, 0},
    {"regionlanguage-korime", "Korean IME settings", OnSP_regionlanguage_korime, 0},
    {"regionlanguage-chsime-pinyin", "Pinyin IME settings", OnSP_regionlanguage_chsime_pinyin, 0},
    {"regionlanguage-chsime-pinyin-domainlexicon", "Pinyin IME domain lexicon", OnSP_regionlanguage_chsime_pinyin_domainlexicon, 0},
    {"regionlanguage-chsime-pinyin-keyconfig", "Pinyin IME key configuration", OnSP_regionlanguage_chsime_pinyin_keyconfig, 0},
    {"regionlanguage-chsime-pinyin-udp", "Pinyin IME user-defined phrases", OnSP_regionlanguage_chsime_pinyin_udp, 0},
    {"regionlanguage-chsime-wubi", "Wubi IME settings", OnSP_regionlanguage_chsime_wubi, 0},
    {"delivery-optimization", "Delivery Optimization settings", OnSP_delivery_optimization, 0},
    {"delivery-optimization-activity", "Delivery Optimization activity", OnSP_delivery_optimization_activity, 0},
    {"delivery-optimization-advanced", "Delivery Optimization advanced settings", OnSP_delivery_optimization_advanced, 0},
    {"findmydevice", "Find my device", OnSP_findmydevice, 0},
    {"windowsinsider", "Windows Insider Program", OnSP_windowsinsider, 0},
    {"windowsinsider-optin", "Windows Insider opt-in", OnSP_windowsinsider_optin, 0},
    {"windowsupdate-action", "Windows Update action page", OnSP_windowsupdate_action, 0},
    {"windowsupdate-options", "Windows Update advanced options", OnSP_windowsupdate_options, 0},
    {"windowsupdate-seekerondemand", "Seek optional updates on demand", OnSP_windowsupdate_seekerondemand, 0},
    {"cortana-moredetails", "Cortana more details", OnSP_cortana_moredetails, 0},
    {"cortana-windowssearch", "Cortana searching Windows", OnSP_cortana_windowssearch, 0},
    {"personalization-glance", "Glance personalization (deprecated)", OnSP_personalization_glance, 0},
    {"personalization-navbar", "Navigation bar settings (deprecated)", OnSP_personalization_navbar, 0},
    {"privacy-feedback-telemetryviewergroup", "View diagnostic data", OnSP_privacy_feedback_telemetryviewergroup, 0},
};

static const Command g_ControlPanel[] = {
    {"cp-all", "All Control Panel items", OnCplAll, 0},
    {"programs-features", "Programs and Features (uninstall apps)", OnCplPrograms, 0},
    {"date-time", "Date and Time control panel", OnCplDateTime, 0},
    {"region", "Region and language formats", OnCplRegion, 0},
    {"keyboard", "Keyboard properties", OnCplKeyboard, 0},
    {"mouse", "Mouse properties", OnCplMouse, 0},
    {"sound", "Sound control panel", OnCplSound, 0},
    {"sound-effects", "Sound effects (sounds tab)", OnCplSoundEffects, 0},
    {"sound-communications", "Sound communications tab", OnCplSoundComm, 0},
    {"fonts", "Fonts folder", OnCplFonts, 0},
    {"folder-options", "Folder Options", OnCplFolderOpts, 0},
    {"internet-options", "Internet Options", OnCplInetOpts, 0},
    {"user-accounts", "User Accounts", OnCplUsers, 0},
    {"power-options", "Power Options control panel", OnCplPower, 0},
    {"firewall", "Windows Defender Firewall", OnCplFirewall, 0},
    {"admin-tools", "Administrative Tools folder", OnCplAdminTools, 0},
    {"bitlocker", "BitLocker Drive Encryption", OnCplBitlocker, 0},
    {"credential-manager", "Credential Manager", OnCplCredMgr, 0},
    {"autoplay", "AutoPlay control panel", OnCplAutoPlay, 0},
    {"color-management", "Color Management", OnCplColor, 0},
    {"game-controllers", "Game Controllers", OnCplGameCtrl, 0},
    {"odbc", "ODBC Data Source Administrator", OnCplOdbc, 0},
    {"devices-printers", "Devices and Printers", OnCplDevicesPrinters, 0},
    {"display", "Display (screen resolution)", OnCplDisplay, 0},
    {"screen-saver", "Screen saver settings", OnCplScreenSaver, 0},
    {"troubleshooting", "Troubleshooting control panel", OnCplTroubleshooting, 0},
    {"recovery", "Recovery control panel", OnCplRecovery, 0},
    {"file-history", "File History", OnCplFileHistory, 0},
    {"backup-restore", "Backup and Restore", OnCplBackupRestore, 0},
    {"ease-of-access", "Ease of Access center", OnCplEaseOfAccess, 0},
};

static const Command g_Network[] = {
    {"network-settings", "Network & internet settings", OnNetSettings, 0},
    {"adapter-settings", "Network adapters (ncpa.cpl)", OnNetAdapters, 0},
    {"wifi", "Wi-Fi settings", OnNetWifi, 0},
    {"vpn", "VPN settings", OnNetVpn, 0},
    {"proxy", "Proxy settings", OnNetProxy, 0},
    {"sharing-center", "Network and Sharing Center", OnNetShareCenter, 0},
    {"advanced-sharing", "Advanced sharing settings", OnNetAdvShare, 0},
    {"network-reset", "Reset network stack", OnNetReset, 0},
    {"wifi-sense", "Wi-Fi Sense settings", OnNetWifiSense, 0},
    {"firewall-rules", "Firewall advanced rules (wf.msc)", OnNetFwRules, 0},
    {"internet-properties", "Internet properties", OnNetInetProp, 0},
    {"remote-desktop", "Remote Desktop Connection", OnNetRdp, 0},
    {"ipconfig", "Show IP configuration", OnNetIpconfig, 0},
    {"ipconfig-all", "Show full IP configuration", OnNetIpconfigAll, 0},
    {"ipconfig-release", "Release the IP address", OnNetIpconfigRelease, 0},
    {"ipconfig-renew", "Renew the IP address", OnNetIpconfigRenew, 0},
    {"ping", "Ping google.com continuously", OnNetPing, 0},
    {"ping-localhost", "Ping 127.0.0.1 continuously", OnNetPingLocal, 0},
    {"tracert", "Trace route to google.com", OnNetTracert, 0},
    {"nslookup", "DNS lookup utility", OnNetNslookup, 0},
    {"dns-check", "Resolve google.com (nslookup)", OnNetDns, 0},
    {"netstat", "Show network connections", OnNetNetstat, 0},
    {"netstat-b", "Show connections with executables", OnNetNetstatB, 0},
    {"route-print", "Show routing table", OnNetRoute, 0},
    {"arp", "Show ARP cache", OnNetArp, 0},
    {"getmac", "Show MAC addresses", OnNetGetmac, 0},
    {"nbtstat", "Show NetBIOS over TCP/IP info", OnNetNbtstat, 0},
    {"flush-dns", "Flush DNS resolver cache", OnNetFlushDns, 0},
    {"winsock-reset", "Reset Winsock catalog", OnNetWinsock, 0},
    {"open-browser", "Open default browser", OnNetBrowser, 0},
    {"http-check", "Check a URL status code", OnHttpCheck, 1},
    {"url-download", "Download a URL to a file (url|file)", OnUrlDownload, 1},
    {"ipv6-disable", "Disable IPv6 (admin, reboot)", OnIpv6Disable, 0},
    {"ipv6-enable", "Enable IPv6 (admin, reboot)", OnIpv6Enable, 0},
};

static const Command g_Power[] = {
    {"sleep", "Put the computer to sleep", OnPwSleep, 0},
    {"hibernate", "Hibernate the computer", OnPwHibernate, 0},
    {"restart", "Restart the computer", OnPwRestart, 0},
    {"shutdown", "Shut down the computer", OnPwShutdown, 0},
    {"signout", "Sign out of the current user", OnPwSignout, 0},
    {"lock", "Lock the workstation", OnPwLock, 0},
    {"power-options", "Power Options control panel", OnPwOptions, 0},
    {"list-plans", "List all power plans", OnPwListPlans, 0},
    {"plan-balanced", "Activate Balanced power plan", OnPwPlanBalanced, 0},
    {"plan-high-performance", "Activate High performance plan", OnPwPlanHighPerf, 0},
    {"plan-power-saver", "Activate Power saver plan", OnPwPlanSaver, 0},
    {"battery-report", "Generate battery report (HTML)", OnPwBatteryReport, 0},
    {"energy-report", "Generate energy report (admin)", OnPwEnergyReport, 0},
};

static const Command g_Display[] = {
    {"display-settings", "Display settings", OnDispSettings, 0},
    {"personalization", "Personalization settings", OnDispPersonal, 0},
    {"background", "Desktop background", OnDispBackground, 0},
    {"colors", "Colors settings", OnDispColors, 0},
    {"lockscreen", "Lock screen settings", OnDispLockScreen, 0},
    {"themes", "Themes settings", OnDispThemes, 0},
    {"start", "Start menu settings", OnDispStart, 0},
    {"taskbar", "Taskbar settings", OnDispTaskbar, 0},
    {"resolution", "Screen resolution (desk.cpl)", OnDispResolution, 0},
    {"screen-saver", "Screen saver settings", OnDispScreenSaver, 0},
    {"night-light", "Night light settings", OnDispNightLight, 0},
    {"multiple-displays", "Multiple displays settings", OnDispMultiple, 0},
    {"show-desktop", "Show desktop (Win+D)", OnDispShowDesktop, 0},
    {"zoom-in", "Zoom in (Ctrl++)", OnDispZoomIn, 0},
    {"zoom-out", "Zoom out (Ctrl+-)", OnDispZoomOut, 0},
    {"zoom-100", "Reset zoom to 100% (Ctrl+0)", OnDispZoom100, 0},
};

static const Command g_Audio[] = {
    {"volume-up", "Increase system volume", OnAudVolUp, 0},
    {"volume-down", "Decrease system volume", OnAudVolDown, 0},
    {"mute", "Toggle system mute", OnAudMute, 0},
    {"mixer", "Open volume mixer", OnAudMixer, 0},
    {"sound-settings", "Sound settings", OnAudSettings, 0},
    {"playback", "Playback devices", OnAudPlayback, 0},
    {"recording", "Recording devices", OnAudRecording, 0},
    {"sound-effects", "Sound effects (sounds tab)", OnAudSounds, 0},
    {"communications", "Communications sound tab", OnAudComm, 0},
    {"sound-control", "Full sound control panel", OnAudControl, 0},
    {"media-play-pause", "Play / pause media (media key)", OnMediaPlayPause, 0},
    {"media-next", "Next track (media key)", OnMediaNext, 0},
    {"media-prev", "Previous track (media key)", OnMediaPrev, 0},
    {"media-stop", "Stop media (media key)", OnMediaStop, 0},
};

static const Command g_Security[] = {
    {"windows-security", "Windows Security dashboard", OnSecDashboard, 0},
    {"quick-scan", "Run a quick virus scan (admin)", OnSecQuickScan, 0},
    {"virus-threat", "Virus & threat protection", OnSecVirusThreat, 0},
    {"threat-settings", "Virus & threat protection settings", OnSecThreatSettings, 0},
    {"account-protection", "Account protection", OnSecAccountProtection, 0},
    {"firewall", "Windows Defender Firewall", OnSecFirewall, 0},
    {"firewall-protection", "Firewall & network protection", OnSecFwProtect, 0},
    {"app-browser", "App & browser control", OnSecAppBrowser, 0},
    {"device-security", "Device security", OnSecDeviceSec, 0},
    {"device-health", "Device performance & health", OnSecDeviceHealth, 0},
    {"exploit-protection", "Exploit protection settings", OnSecExploitProtection, 0},
    {"family-options", "Family options", OnSecFamily, 0},
    {"update-settings", "Windows Update settings", OnSecUpdate, 0},
    {"backup", "Backup settings", OnSecBackup, 0},
    {"credential-manager", "Credential Manager", OnSecCredMgr, 0},
    {"bitlocker", "BitLocker Drive Encryption", OnSecBitlocker, 0},
    {"defender-quick-scan", "Run a Defender quick scan (admin)", OnDefenderQuickScan, 0},
    {"defender-full-scan", "Run a Defender full scan (admin)", OnDefenderFullScan, 0},
    {"defender-update", "Update Defender signatures (admin)", OnDefenderUpdate, 0},
    {"defender-threat-history", "Show Defender threat history", OnDefenderThreatHistory, 0},
};

static const Command g_Apps[] = {
    {"notepad", "Launch Notepad", OnAppNotepad, 0},
    {"wordpad", "Launch WordPad", OnAppWordpad, 0},
    {"paint", "Launch Paint", OnAppPaint, 0},
    {"calculator", "Launch Calculator", OnAppCalc, 0},
    {"snipping-tool", "Launch Snipping Tool", OnAppSnipping, 0},
    {"sticky-notes", "Launch Sticky Notes", OnAppSticky, 0},
    {"edge", "Launch Microsoft Edge", OnAppEdge, 0},
    {"store", "Open Microsoft Store", OnAppStore, 0},
    {"internet-explorer", "Launch Internet Explorer", OnAppIE, 0},
    {"media-player", "Launch Windows Media Player", OnAppWmp, 0},
    {"xbox-game-bar", "Xbox Game Bar settings", OnAppGameBar, 0},
    {"file-explorer", "Open File Explorer", OnAppExplorer, 0},
    {"this-pc", "Open This PC", OnAppThisPC, 0},
    {"documents", "Open Documents folder", OnAppDocuments, 0},
    {"downloads", "Open Downloads folder", OnAppDownloads, 0},
    {"desktop", "Open Desktop folder", OnAppDesktop, 0},
    {"pictures", "Open Pictures folder", OnAppPictures, 0},
    {"music", "Open Music folder", OnAppMusic, 0},
    {"videos", "Open Videos folder", OnAppVideos, 0},
    {"onedrive", "Open OneDrive folder", OnAppOneDrive, 0},
    {"network-folder", "Open Network folder", OnAppNetFolder, 0},
    {"recycle-bin", "Open Recycle Bin", OnAppRecycle, 0},
    {"control-panel", "Open Control Panel", OnAppControlPanel, 0},
    {"run-dialog", "Open Run dialog (Win+R)", OnAppRunDialog, 0},
    {"search", "Open Windows Search (Win+S)", OnAppSearch, 0},
    {"settings", "Open Settings (Win+I)", OnAppSettings, 0},
    {"notification-center", "Open Notification Center (Win+A)", OnAppNotification, 0},
    {"screenshot", "Capture screen region (Win+Shift+S)", OnAppScreenshot, 0},
    {"emoji-panel", "Open emoji panel (Win+.)", OnAppEmoji, 0},
    {"clipboard-history", "Open clipboard history (Win+V)", OnAppClipboardHist, 0},
    {"magnifier", "Open Magnifier (Win++)", OnAppMagnifier, 0},
};

static const Command g_Maintenance[] = {
    {"disk-cleanup", "Free up disk space (cleanmgr)", OnMaintClean, 0},
    {"defrag", "Defragment and optimize drives", OnMaintDefrag, 0},
    {"chkdsk", "Check disk for errors", OnMaintChkdsk, 0},
    {"sfc-scannow", "System File Checker (admin)", OnMaintSfc, 0},
    {"dism-check", "DISM health check (admin)", OnMaintDismCheck, 0},
    {"dism-restore", "DISM restore health (admin)", OnMaintDismRestore, 0},
    {"system-restore", "System Restore", OnMaintSysRestore, 0},
    {"windows-backup", "Windows backup settings", OnMaintWinBackup, 0},
    {"reliability-monitor", "Reliability Monitor", OnMaintReliability, 0},
    {"print-management", "Print Management console", OnMaintPrintMgmt, 0},
    {"shared-folders", "Shared Folders console", OnMaintShared, 0},
    {"storage-sense", "Storage Sense settings", OnMaintStorageSense, 0},
    {"startup-apps", "Startup apps settings", OnMaintStartupApps, 0},
};

static const Command g_Recovery[] = {
    {"troubleshoot", "Open troubleshooters", OnRecTrouble, 0},
    {"startup-repair", "Run startup repair (admin)", OnRecStartupRepair, 0},
    {"memory-diagnostic", "Windows Memory Diagnostic", OnRecMemDiag, 0},
    {"steps-recorder", "Problem Steps Recorder", OnRecSteps, 0},
    {"reset-pc", "Reset this PC options", OnRecResetPc, 0},
    {"advanced-startup", "Restart into advanced startup", OnRecAdvStartup, 0},
    {"recovery-drive", "Create a recovery drive", OnRecDrive, 0},
    {"system-image", "System image backup", OnRecSysImage, 0},
    {"file-history", "File History", OnRecFileHistory, 0},
    {"bcd-store", "Show boot configuration (admin)", OnRecBcd, 0},
    {"safe-mode", "Safe Mode setup (msconfig)", OnRecSafeMode, 0},
};

static const Command g_Dev[] = {
    {"cmd", "Open Command Prompt", OnDevCmd, 0},
    {"cmd-admin", "Open Command Prompt as Administrator", OnDevCmdAdmin, 0},
    {"powershell", "Open PowerShell", OnDevPowershell, 0},
    {"powershell-admin", "Open PowerShell as Administrator", OnDevPowershellAdmin, 0},
    {"windows-terminal", "Open Windows Terminal", OnDevTerminal, 0},
    {"terminal-admin", "Open Windows Terminal as Administrator", OnDevTerminalAdmin, 0},
    {"developer-prompt-vs", "Visual Studio Developer Prompt", OnDevVsPrompt, 0},
    {"wsl", "Launch Windows Subsystem for Linux", OnDevWsl, 0},
    {"wsl-list", "List WSL distributions", OnDevWslList, 0},
    {"wsl-shutdown", "Shut down all WSL distributions", OnDevWslShutdown, 0},
    {"wsl-update", "Update the WSL kernel", OnDevWslUpdate, 0},
    {"performance-recorder", "Windows Performance Recorder", OnDevPerfRec, 0},
    {"windows-tools", "Open Windows Tools folder", OnDevWinTools, 0},
    {"dotnet-check", "Check installed .NET runtimes", OnDevDotnetCheck, 0},
    {"node-check", "Check Node.js version", OnDevNodeCheck, 0},
    {"git-check", "Check Git version", OnDevGitCheck, 0},
    {"python-check", "Check Python version", OnDevPythonCheck, 0},
    {"go-check", "Check Go version", OnDevGoCheck, 0},
    {"java-check", "Check Java version", OnDevJavaCheck, 0},
    {"curl-check", "Check cURL version", OnDevCurlCheck, 0},
};

static const Command g_Folders[] = {
    {"appdata", "Open AppData (Roaming)", OnFldAppData, 0},
    {"localappdata", "Open Local AppData", OnFldLocalAppData, 0},
    {"programdata", "Open ProgramData", OnFldProgramData, 0},
    {"startup-folder", "Open Startup folder", OnFldStartup, 0},
    {"temp", "Open Temp folder", OnFldTemp, 0},
    {"windows-dir", "Open Windows directory", OnFldWindows, 0},
    {"system32", "Open System32 directory", OnFldSystem32, 0},
    {"sendto", "Open SendTo folder", OnFldSendTo, 0},
    {"quick-launch", "Open Quick Launch folder", OnFldQuickLaunch, 0},
    {"recent", "Open Recent Items", OnFldRecent, 0},
    {"templates", "Open Templates folder", OnFldTemplates, 0},
    {"favorites", "Open Favorites folder", OnFldFavorites, 0},
    {"cookies", "Open Cookies folder", OnFldCookies, 0},
    {"history", "Open History folder", OnFldHistory, 0},
    {"nethood", "Open NetHood folder", OnFldNetHood, 0},
    {"printhood", "Open PrintHood folder", OnFldPrintHood, 0},
    {"public", "Open Public folder", OnFldPublic, 0},
    {"users", "Open Users folder", OnFldUsers, 0},
    {"contacts", "Open Contacts folder", OnFldContacts, 0},
    {"libraries", "Open Libraries", OnFldLibraries, 0},
    {"saved-games", "Open Saved Games folder", OnFldSavedGames, 0},
    {"searches", "Open Searches folder", OnFldSearches, 0},
    {"desktop", "Open Desktop folder", OnFldDesktop, 0},
    {"downloads", "Open Downloads folder", OnFldDownloads, 0},
};

static const Command g_Admin[] = {
    {"lusrmgr", "Local Users and Groups", OnAdmLusrmgr, 0},
    {"secpol", "Local Security Policy", OnAdmSecpol, 0},
    {"certmgr", "Certificate Manager", OnAdmCertmgr, 0},
    {"component-services", "Component Services (COM+)", OnAdmComServices, 0},
    {"tpm", "TPM Management", OnAdmTpm, 0},
    {"hyper-v", "Hyper-V Manager", OnAdmHyperV, 0},
    {"iscsi", "iSCSI Initiator", OnAdmIscsi, 0},
    {"storage-spaces", "Storage Spaces settings", OnAdmStorageSpaces, 0},
    {"wmi", "WMI Management console", OnAdmWmi, 0},
    {"rsop", "Resultant Set of Policy", OnAdmRsop, 0},
};

static void OnExplorerOpenThisPc(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "LaunchTo", 1);
    RefreshShell();
    Notify("Applied: Explorer opens This PC");
}

static void OnExplorerOpenQuickAccess(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "LaunchTo", 0);
    RefreshShell();
    Notify("Applied: Explorer opens Quick Access");
}

static void OnTaskbarCombineAlways(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarGlomLevel", 0);
    RefreshShell();
    Notify("Applied: taskbar buttons always combined");
}

static void OnTaskbarCombineNever(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarGlomLevel", 2);
    RefreshShell();
    Notify("Applied: taskbar buttons never combined");
}

static void OnTaskbarAlignLeft(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarAl", 0);
    RefreshShell();
    Notify("Applied: taskbar icons aligned left");
}

static void OnTaskbarAlignCenter(const char *arg) {
    (void)arg;
    WriteRegDword(HKEY_CURRENT_USER, TW_EA, "TaskbarAl", 1);
    RefreshShell();
    Notify("Applied: taskbar icons centered");
}

static const Command g_Storage[] = {
    {"drives-list", "List all drives with free space", OnDrivesList, 0},
    {"drive-info", "Detailed drive information (letter)", OnDriveInfo, 1},
    {"disk-space-report", "Free space report for all drives", OnDrivesList, 0},
    {"drives-removable", "List removable and optical drives", OnDrivesRemovable, 0},
    {"disk-defrag", "Optimize a drive (letter)", OnDiskDefrag, 1},
    {"disk-defrag-all", "Optimize all drives", OnDiskDefragAll, 0},
    {"disk-check", "Scan a drive for errors (letter)", OnDiskCheck, 1},
    {"disk-cleanup", "Open Disk Cleanup", OnDiskCleanup, 0},
    {"safe-remove-hardware", "Safely Remove Hardware dialog", OnSafeRemoveHardware, 0},
    {"system-restore-create", "Create a system restore point", OnSystemRestoreCreate, 0},
    {"restore-manager", "System Restore manager (rstrui)", OnRestoreManager, 0},
    {"memory-diagnostics", "Windows Memory Diagnostics", OnMemoryDiagnostics, 0},
};

static const Command g_Windows[] = {
    {"window-list", "List open windows with PID and size", OnWindowList, 0},
    {"window-activate", "Bring a window to front (title-part)", OnWindowActivate, 1},
    {"window-close", "Close a window (title-part)", OnWindowClose, 1},
    {"window-minimize", "Minimize a window (title-part)", OnWindowMinimize, 1},
    {"window-restore", "Restore a window (title-part)", OnWindowRestore, 1},
    {"show-desktop", "Show desktop (Win+D)", OnShowDesktop, 0},
    {"minimize-all", "Minimize all windows (Win+M)", OnMinimizeAll, 0},
    {"restore-minimized", "Restore minimized windows (Win+Shift+M)", OnRestoreMinimized, 0},
    {"task-switcher", "Open the task switcher (Alt+Tab)", OnTaskSwitcher, 0},
    {"close-window", "Close the active window (Alt+F4)", OnCloseWindow, 0},
    {"lock-screen", "Lock the screen (Win+L)", OnLockScreen, 0},
    {"virtual-desktop-new", "Create a virtual desktop (Win+Ctrl+D)", OnVdNew, 0},
    {"virtual-desktop-close", "Close current virtual desktop", OnVdClose, 0},
    {"virtual-desktop-next", "Switch to next virtual desktop", OnVdNext, 0},
    {"virtual-desktop-prev", "Switch to previous virtual desktop", OnVdPrev, 0},
    {"snap-left", "Snap window to the left half", OnSnapLeft, 0},
    {"snap-right", "Snap window to the right half", OnSnapRight, 0},
    {"snap-up", "Maximize window vertically (snap up)", OnSnapUp, 0},
    {"snap-down", "Restore window from snap up", OnSnapDown, 0},
    {"cycle-windows", "Cycle through windows (Alt+Esc)", OnCycleWindows, 0},
};

static const Command g_Access[] = {
    {"sticky-keys-on", "Turn on Sticky Keys", OnStickyKeysOn, 0},
    {"sticky-keys-off", "Turn off Sticky Keys", OnStickyKeysOff, 0},
    {"toggle-keys-on", "Turn on Toggle Keys", OnToggleKeysOn, 0},
    {"toggle-keys-off", "Turn off Toggle Keys", OnToggleKeysOff, 0},
    {"filter-keys-on", "Turn on Filter Keys (ignore fast repeats)", OnFilterKeysOn, 0},
    {"filter-keys-off", "Turn off Filter Keys", OnFilterKeysOff, 0},
    {"mouse-keys-on", "Control mouse with the numeric keypad", OnMouseKeysOn, 0},
    {"mouse-keys-off", "Turn off Mouse Keys", OnMouseKeysOff, 0},
    {"mouse-trails-on", "Show mouse pointer trails", OnMouseTrailsOn, 0},
    {"mouse-trails-off", "Hide mouse pointer trails", OnMouseTrailsOff, 0},
    {"cursor-shadow-on", "Show cursor shadow", OnCursorShadowOn, 0},
    {"cursor-shadow-off", "Hide cursor shadow", OnCursorShadowOff, 0},
    {"numlock-at-boot-on", "Num Lock on at startup", OnNumlockAtBootOn, 0},
    {"numlock-at-boot-off", "Num Lock off at startup", OnNumlockAtBootOff, 0},
};

static const Command g_Optimize[] = {
    {"clear-print-spooler", "Clear the print spooler queue", OnClearPrintSpooler, 0},
    {"clean-windows-temp", "Delete Windows Temp files (admin)", OnCleanWindowsTemp, 0},
    {"clean-software-distribution", "Clear Windows Update download cache", OnCleanSoftwareDistribution, 0},
    {"font-cache-rebuild", "Rebuild the font cache", OnFontCacheRebuild, 0},
    {"network-reset-all", "Reset Winsock + IP stack + DNS", OnNetworkResetAll, 0},
    {"superfetch-on", "Enable SysMain (Superfetch)", OnSuperfetchOn, 0},
    {"superfetch-off", "Disable SysMain (Superfetch)", OnSuperfetchOff, 0},
    {"search-indexing-on", "Start Windows Search indexing", OnSearchIndexOn, 0},
    {"search-indexing-off", "Stop Windows Search indexing", OnSearchIndexOff, 0},
    {"indexing-options", "Open indexing options", OnIndexingOptions, 0},
    {"event-log-clear", "Clear an event log (log-name)", OnEventLogClear, 1},
};

static const Command g_PowerPlans[] = {
    {"battery-status", "Show battery and AC power status", OnBatteryStatus, 0},
    {"power-plan-list", "List all power plans", OnPowerPlanList, 0},
    {"power-plan-balanced", "Activate Balanced power plan", OnPowerPlanBalanced, 0},
    {"power-plan-high-performance", "Activate High Performance plan", OnPowerPlanHigh, 0},
    {"power-plan-power-saver", "Activate Power Saver plan", OnPowerPlanSaver, 0},
    {"power-plan-ultimate", "Activate Ultimate Performance plan", OnPowerPlanUltimate, 0},
    {"sleep-now", "Put the computer to sleep now", OnSleepNow, 0},
    {"hibernate-now", "Hibernate now", OnHibernateNow, 0},
};

static const Command g_NetTools[] = {
    {"ping-host", "Ping a host 4 times", OnPingHost, 1},
    {"tracert-host", "Trace the route to a host", OnTracertHost, 1},
    {"nslookup-host", "Resolve a hostname to IP", OnNslookupHost, 1},
    {"netstat-port", "Show connections for a port (arg optional)", OnNetstatPort, 1},
    {"check-port", "Test if a TCP port is open (host|port)", OnCheckPort, 1},
    {"public-ip", "Show your public IPv4 address", OnPublicIp, 0},
    {"wifi-scan", "Scan for nearby Wi-Fi networks", OnWifiScan, 0},
    {"wifi-profiles", "List saved Wi-Fi profiles", OnWifiProfiles, 0},
    {"wifi-connect", "Connect to a saved Wi-Fi profile", OnWifiConnect, 1},
    {"wifi-password", "Show a saved Wi-Fi password (profile)", OnWifiPassword, 1},
    {"wifi-disconnect", "Disconnect from Wi-Fi", OnWifiDisconnect, 0},
    {"adapter-list", "List all network adapters (ipconfig /all)", OnAdapterList, 0},
    {"adapter-enable", "Enable a network adapter (name)", OnAdapterEnable, 1},
    {"adapter-disable", "Disable a network adapter (name)", OnAdapterDisable, 1},
    {"dns-google", "Set all adapters to Google DNS 8.8.8.8/8.8.4.4", OnDnsGoogle, 0},
    {"dns-cloudflare", "Set all adapters to Cloudflare DNS 1.1.1.1/1.0.0.1", OnDnsCloudflare, 0},
    {"dns-automatic", "Reset DNS to automatic (DHCP)", OnDnsAutomatic, 0},
    {"firewall-on", "Turn Windows Firewall on for all profiles", OnFirewallOn, 0},
    {"firewall-off", "Turn Windows Firewall off for all profiles", OnFirewallOff, 0},
    {"firewall-allow-port", "Allow a port inbound (port|tcp|udp|name)", OnFirewallAllowPort, 1},
    {"firewall-block-port", "Block a port inbound (port|tcp|udp|name)", OnFirewallBlockPort, 1},
    {"firewall-remove-rule", "Delete a firewall rule by name", OnFirewallRemoveRule, 1},
    {"hosts-view", "Show the hosts file", OnHostsView, 0},
    {"hosts-block", "Block a hostname in the hosts file", OnHostsBlock, 1},
};

static const Command g_Tweaks[] = {
    {"light-mode-on", "Use light theme (apps)", OnLightModeOn, 0},
    {"light-mode-off", "Use dark theme (apps)", OnLightModeOff, 0},
    {"system-light-mode-on", "Use light theme (system)", OnSystemLightModeOn, 0},
    {"system-light-mode-off", "Use dark theme (system)", OnSystemLightModeOff, 0},
    {"accent-titlebar-on", "Show accent color on title bars", OnAccentOnTitlebarOn, 0},
    {"accent-titlebar-off", "Hide accent color on title bars", OnAccentOnTitlebarOff, 0},
    {"transparency-on", "Enable transparency effects", OnTransparencyOn, 0},
    {"transparency-off", "Disable transparency effects", OnTransparencyOff, 0},
    {"show-file-extensions-on", "Show file name extensions", OnShowFileExtensionsOn, 0},
    {"show-file-extensions-off", "Hide file name extensions", OnShowFileExtensionsOff, 0},
    {"show-hidden-files-on", "Show hidden files", OnShowHiddenFilesOn, 0},
    {"show-hidden-files-off", "Hide hidden files", OnShowHiddenFilesOff, 0},
    {"show-super-hidden-on", "Show protected operating system files", OnShowSuperHiddenOn, 0},
    {"show-super-hidden-off", "Hide protected operating system files", OnShowSuperHiddenOff, 0},
    {"show-checkboxes-on", "Show selection check boxes", OnShowCheckBoxesOn, 0},
    {"show-checkboxes-off", "Hide selection check boxes", OnShowCheckBoxesOff, 0},
    {"confirm-file-delete-on", "Ask before deleting files", OnConfirmFileDeleteOn, 0},
    {"confirm-file-delete-off", "Do not ask before deleting files", OnConfirmFileDeleteOff, 0},
    {"hide-empty-drives-on", "Hide empty drives", OnHideEmptyDrivesOn, 0},
    {"hide-empty-drives-off", "Show empty drives", OnHideEmptyDrivesOff, 0},
    {"expand-to-current-folder-on", "Expand to the current folder", OnExpandToCurrentFolderOn, 0},
    {"expand-to-current-folder-off", "Do not expand to the current folder", OnExpandToCurrentFolderOff, 0},
    {"show-all-folders-on", "Show all folders in navigation pane", OnShowAllFoldersOn, 0},
    {"show-all-folders-off", "Hide folders in navigation pane", OnShowAllFoldersOff, 0},
    {"sync-notifications-on", "Show sync provider notifications", OnSyncNotificationsOn, 0},
    {"sync-notifications-off", "Hide sync provider notifications", OnSyncNotificationsOff, 0},
    {"small-taskbar-icons-on", "Use small taskbar icons", OnTaskbarSmallIconsOn, 0},
    {"small-taskbar-icons-off", "Use large taskbar icons", OnTaskbarSmallIconsOff, 0},
    {"show-seconds-in-clock-on", "Show seconds in the taskbar clock", OnShowSecondsInClockOn, 0},
    {"show-seconds-in-clock-off", "Hide seconds in the taskbar clock", OnShowSecondsInClockOff, 0},
    {"taskbar-all-displays-on", "Show taskbar on all displays", OnTaskbarAllDisplaysOn, 0},
    {"taskbar-all-displays-off", "Show taskbar only on main display", OnTaskbarAllDisplaysOff, 0},
    {"taskbar-combine-always", "Always combine taskbar buttons", OnTaskbarCombineAlways, 0},
    {"taskbar-combine-never", "Never combine taskbar buttons", OnTaskbarCombineNever, 0},
    {"taskbar-align-left", "Align taskbar icons to the left", OnTaskbarAlignLeft, 0},
    {"taskbar-align-center", "Center taskbar icons", OnTaskbarAlignCenter, 0},
    {"explorer-open-this-pc", "Explorer opens This PC", OnExplorerOpenThisPc, 0},
    {"explorer-open-quick-access", "Explorer opens Quick Access", OnExplorerOpenQuickAccess, 0},
    {"game-dvr-on", "Enable Game DVR (game bar)", OnGameDvrOn, 0},
    {"game-dvr-off", "Disable Game DVR (game bar)", OnGameDvrOff, 0},
    {"background-recording-on", "Enable background recording", OnBackgroundRecordingOn, 0},
    {"background-recording-off", "Disable background recording", OnBackgroundRecordingOff, 0},
    {"toast-notifications-on", "Enable app notifications (toasts)", OnToastNotificationsOn, 0},
    {"toast-notifications-off", "Disable app notifications (toasts)", OnToastNotificationsOff, 0},
    {"advertising-id-off", "Turn off the advertising ID", OnAdvertisingIdOff, 0},
    {"advertising-id-on", "Turn on the advertising ID", OnAdvertisingIdOn, 0},
    {"widgets-news-off", "Disable widgets news feed", OnWidgetsNewsOff, 0},
    {"widgets-news-on", "Enable widgets news feed", OnWidgetsNewsOn, 0},
    {"fast-startup-on", "Enable fast startup", OnFastStartupOn, 0},
    {"fast-startup-off", "Disable fast startup", OnFastStartupOff, 0},
    {"telemetry-off", "Disable diagnostic data (telemetry)", OnTelemetryOff, 0},
    {"telemetry-on", "Enable full diagnostic data", OnTelemetryOn, 0},
    {"auto-restart-on", "Auto restart on system crash", OnAutoRestartOn, 0},
    {"auto-restart-off", "No auto restart on system crash", OnAutoRestartOff, 0},
    {"clock-24h-on", "Use 24-hour clock", OnClock24, 0},
    {"clock-24h-off", "Use 12-hour clock (AM/PM)", OnClock12, 0},
    {"date-format-us", "US date format (M/d/yyyy)", OnDateUs, 0},
    {"date-format-eu", "European date format (dd/MM/yyyy)", OnDateEu, 0},
    {"mouse-speed-set", "Set mouse pointer speed (1-20)", OnMouseSpeedSet, 1},
    {"mouse-double-click-speed", "Set double-click speed (ms)", OnMouseDblClick, 1},
    {"keyboard-repeat-delay", "Set keyboard repeat delay (0-3)", OnKeyDelay, 1},
    {"keyboard-repeat-rate", "Set keyboard repeat rate (0-31)", OnKeyRate, 1},
    {"screen-saver-timeout", "Set screen saver timeout (seconds)", OnScreenSaverTimeout, 1},
    {"screen-saver-secure-on", "Screen saver requires sign-in", OnScreenSaverSecureOn, 0},
    {"screen-saver-secure-off", "Screen saver does not require sign-in", OnScreenSaverSecureOff, 0},
    {"display-timeout", "Set display off timeout (minutes)", OnDisplayTimeout, 1},
    {"sleep-timeout", "Set sleep timeout (minutes)", OnSleepTimeout, 1},
    {"hibernation-on", "Enable hibernation", OnHibernationOn, 0},
    {"hibernation-off", "Disable hibernation", OnHibernationOff, 0},
    {"uac-level-set", "Set UAC level (0-3)", OnUacLevelSet, 1},
    {"auto-hide-taskbar-on", "Auto-hide the taskbar", OnAutoHideTaskbarOn, 0},
    {"auto-hide-taskbar-off", "Always show the taskbar", OnAutoHideTaskbarOff, 0},
    {"lock-taskbar-on", "Lock the taskbar", OnLockTaskbarOn, 0},
    {"lock-taskbar-off", "Unlock the taskbar", OnLockTaskbarOff, 0},
    {"search-bar-hide", "Hide the taskbar search box", OnSearchBarOn, 0},
    {"search-bar-show", "Show the taskbar search box", OnSearchBarOff, 0},
    {"taskview-button-on", "Show the Task View button", OnTaskViewButtonOn, 0},
    {"taskview-button-off", "Hide the Task View button", OnTaskViewButtonOff, 0},
    {"widgets-button-on", "Show the widgets button", OnWidgetsButtonOn, 0},
    {"widgets-button-off", "Hide the widgets button", OnWidgetsButtonOff, 0},
    {"game-mode-on", "Enable Game Mode", OnGameModeOn, 0},
    {"game-mode-off", "Disable Game Mode", OnGameModeOff, 0},
    {"autoplay-disable", "Disable AutoPlay (all drives)", OnAutoPlayOn, 0},
    {"autoplay-enable", "Enable AutoPlay", OnAutoPlayOff, 0},
    {"window-animations-on", "Enable window animations", OnAnimationsOn, 0},
    {"window-animations-off", "Disable window animations", OnAnimationsOff, 0},
    {"drag-full-windows-on", "Show window contents while dragging", OnDragFullWindowsOn, 0},
    {"drag-full-windows-off", "Show only a frame while dragging", OnDragFullWindowsOff, 0},
    {"visualfx-best", "Best visual appearance", OnVisualFxBestOn, 0},
    {"visualfx-performance", "Best performance appearance", OnVisualFxBestOff, 0},
    {"desktop-icon-thispc-on", "Show This PC desktop icon", OnDesktopIconThisPcOn, 0},
    {"desktop-icon-thispc-off", "Hide This PC desktop icon", OnDesktopIconThisPcOff, 0},
    {"desktop-icon-recycle-on", "Show Recycle Bin desktop icon", OnDesktopIconRecycleOn, 0},
    {"desktop-icon-recycle-off", "Hide Recycle Bin desktop icon", OnDesktopIconRecycleOff, 0},
    {"desktop-icon-controlpanel-on", "Show Control Panel desktop icon", OnDesktopIconControlPanelOn, 0},
    {"desktop-icon-controlpanel-off", "Hide Control Panel desktop icon", OnDesktopIconControlPanelOff, 0},
    {"desktop-icon-userfolder-on", "Show user folder desktop icon", OnDesktopIconUserFolderOn, 0},
    {"desktop-icon-userfolder-off", "Hide user folder desktop icon", OnDesktopIconUserFolderOff, 0},
    {"snap-windows-on", "Enable window snapping", OnSnapWindowsOn, 0},
    {"snap-windows-off", "Disable window snapping", OnSnapWindowsOff, 0},
    {"start-recent-items-on", "Show recent items in Start menu", OnStartRecentItemsOn, 0},
    {"start-recent-items-off", "Hide recent items in Start menu", OnStartRecentItemsOff, 0},
    {"start-recent-apps-on", "Show recently added apps in Start menu", OnStartRecentAppsOn, 0},
    {"start-recent-apps-off", "Hide recently added apps in Start menu", OnStartRecentAppsOff, 0},
    {"show-tray-icons-on", "Show all notification area icons", OnShowTrayIconsOn, 0},
    {"show-tray-icons-off", "Auto-hide inactive tray icons", OnShowTrayIconsOff, 0},
    {"tweaks-status", "Show current tweak status report", OnTweaksStatus, 0},
};

static const Command g_FileTools[] = {
    {"file-info", "Show file or folder details", OnFileInfo, 1},
    {"file-copy", "Copy a file (source|destination)", OnFileCopy, 1},
    {"file-move", "Move a file (source|destination)", OnFileMove, 1},
    {"file-rename", "Rename a file (old|new)", OnFileRename, 1},
    {"file-delete", "Delete a file", OnFileDelete, 1},
    {"folder-create", "Create a folder (with parents)", OnFolderCreate, 1},
    {"folder-delete", "Delete a folder to Recycle Bin", OnFolderDelete, 1},
    {"folder-size", "Measure a folder size recursively", OnFolderSize, 1},
    {"file-search", "Search files by name (folder|pattern)", OnFileSearch, 1},
    {"large-files", "Find large files (folder|min-MB)", OnLargeFiles, 1},
    {"dup-files", "Find duplicate files by MD5 (folder)", OnDupFiles, 1},
    {"hex-dump", "Show a file in hexadecimal", OnHexDump, 1},
    {"line-count", "Count lines and bytes of a file", OnLineCount, 1},
    {"base64-encode", "Base64-encode a file to <file>.base64", OnBase64EncodeFile, 1},
    {"base64-decode", "Base64-decode a file to <file>.out", OnBase64DecodeFile, 1},
    {"clipboard-copy", "Copy a text file into the clipboard", OnClipboardCopyFile, 1},
    {"clipboard-view", "Show the clipboard text", OnClipboardView, 0},
    {"clipboard-save", "Save clipboard text to a file", OnClipboardSave, 1},
    {"zip-create", "Create a zip archive (folder|archive.zip)", OnZipCreate, 1},
    {"zip-extract", "Extract a zip archive (archive.zip|folder)", OnZipExtract, 1},
    {"recent-files", "List recent documents", OnRecentFiles, 0},
    {"recycle-size", "Show Recycle Bin size and item count", OnRecycleSize, 0},
};

static const Command g_Hardware[] = {
    {"monitors-list", "List displays and their resolutions", OnMonitorsList, 0},
    {"usb-list", "List USB devices", OnUsbList, 0},
    {"gpu-list", "List graphics adapters", OnGpuList, 0},
    {"sound-devices", "List sound devices", OnSoundDevices, 0},
    {"printers-list", "List printers", OnPrintersList, 0},
    {"network-adapters", "List network adapters (MAC, IP, gateway)", OnNetworkAdapters, 0},
    {"memory-slots", "List physical memory modules", OnMemorySlots, 0},
    {"battery-details", "Generate the battery report", OnBatteryDetails, 0},
};

static const Command g_Accounts[] = {
    {"accounts-list", "List local user accounts", OnAccountsList, 0},
    {"account-create", "Create a user (name|password)", OnAccountCreate, 1},
    {"account-enable", "Enable a user account", OnAccountEnable, 1},
    {"account-disable", "Disable a user account", OnAccountDisable, 1},
    {"account-delete", "Delete a user account", OnAccountDelete, 1},
    {"account-admin", "Add a user to Administrators", OnAccountAdmin, 1},
    {"account-remove-admin", "Remove a user from Administrators", OnAccountRemoveAdmin, 1},
    {"account-password-set", "Set a user password (name|password)", OnAccountPasswordSet, 1},
    {"shares-list", "List shared folders", OnSharesList, 0},
    {"share-create", "Share a folder (path|share-name)", OnShareCreate, 1},
    {"share-delete", "Remove a share", OnShareDelete, 1},
    {"task-list", "List scheduled tasks", OnTaskList, 0},
    {"task-create", "Create a logon task (name|command)", OnTaskCreate, 1},
    {"task-delete", "Delete a scheduled task", OnTaskDelete, 1},
    {"task-run", "Run a scheduled task now", OnTaskRun, 1},
};

static const Command g_AppMgmt[] = {
    {"apps-list", "List installed applications", OnInstalledApps, 0},
    {"app-uninstall", "Launch an app uninstaller (name-part)", OnAppUninstall, 1},
    {"appx-list", "List Store (AppX) packages", OnAppxList, 0},
    {"appx-remove", "Remove a Store (AppX) package", OnAppxRemove, 1},
    {"msi-install", "Install an MSI package", OnMsiInstall, 1},
    {"msi-repair", "Repair an MSI package", OnMsiRepair, 1},
    {"msi-uninstall", "Uninstall an MSI package", OnMsiUninstall, 1},
    {"update-check-now", "Trigger Windows Update check", OnUpdateCheckNow, 0},
    {"update-history", "Show installed updates history", OnUpdateHistory, 0},
};

static const Command g_Services[] = {
    {"services-list", "List all Windows services with state", OnServicesList, 0},
    {"services-start", "Start a service (needs the service name)", OnServiceStart, 1},
    {"services-stop", "Stop a service (needs the service name)", OnServiceStop, 1},
    {"services-restart", "Restart a service (needs the service name)", OnServiceRestart, 1},
    {"services-enable", "Enable a service (manual start, needs the name)", OnServiceEnable, 1},
    {"services-disable", "Disable a service (needs the service name)", OnServiceDisable, 1},
    {"services-query", "Show service configuration (needs the name)", OnServiceQuery, 1},
};

static const Command g_Processes[] = {
    {"processes-list", "List all running processes", OnProcessesList, 0},
    {"processes-count", "Count running processes", OnProcessCount, 0},
    {"processes-kill", "Terminate a process by name or PID", OnProcessKill, 1},
    {"processes-kill-all", "Terminate all processes with this name", OnProcessKillAll, 1},
};

static const Command g_Startup[] = {
    {"startup-list", "List all startup entries", OnStartupList, 0},
    {"startup-remove", "Remove a startup entry (needs its name)", OnStartupRemove, 1},
    {"startup-folder", "Open the Startup folder", OnFldStartup, 0},
};

static const Command g_Registry[] = {
    {"registry-open-editor", "Open Registry Editor", OnSysRegedit, 0},
    {"registry-open", "Open Registry Editor at a key (needs ROOT\\path)", OnRegOpen, 1},
    {"registry-read", "Read values (needs ROOT\\path|value)", OnRegRead, 1},
    {"registry-write", "Write a value (needs ROOT\\path|value|data)", OnRegWrite, 1},
    {"registry-delete", "Delete a value or key (needs ROOT\\path|value)", OnRegDelete, 1},
    {"registry-search", "Search subkeys (needs text)", OnRegSearch, 1},
    {"registry-backup", "Export a key to a .reg file (needs ROOT\\path)", OnRegBackup, 1},
    {"registry-import", "Import a .reg file", OnRegImport, 0},
};

static const Command g_Reports[] = {
    {"reports-overview", "Full system overview report", OnSysInfoOverview, 0},
    {"reports-os", "Windows OS version report", OnSysInfoOs, 0},
    {"reports-cpu", "CPU report", OnSysInfoCpu, 0},
    {"reports-ram", "RAM / memory report", OnSysInfoRam, 0},
    {"reports-disk", "Disk drives report", OnSysInfoDisk, 0},
    {"reports-uptime", "System uptime report", OnSysInfoUptime, 0},
    {"reports-user", "User and computer report", OnSysInfoUser, 0},
    {"reports-battery", "Battery / power status report", OnSysInfoBattery, 0},
    {"reports-gpu", "GPU report", OnSysInfoGpu, 0},
    {"reports-motherboard", "Motherboard report", OnSysInfoMotherboard, 0},
    {"reports-bios", "BIOS report", OnSysInfoBios, 0},
    {"reports-apps", "Installed applications report", OnInstalledApps, 0},
    {"reports-hotfixes", "Installed Windows updates report", OnHotfixList, 0},
    {"reports-network", "Network configuration report", OnSysInfoNetwork, 0},
    {"reports-adapters", "Network adapters report (MAC, IP)", OnNetworkAdapters, 0},
    {"reports-monitors", "Displays and resolutions report", OnMonitorsList, 0},
    {"reports-usb", "USB devices report", OnUsbList, 0},
    {"reports-printers", "Printers report", OnPrintersList, 0},
    {"reports-sound", "Sound devices report", OnSoundDevices, 0},
    {"reports-memory-slots", "Memory modules report", OnMemorySlots, 0},
    {"reports-firewall", "Firewall state report", OnRepFirewall, 0},
    {"reports-defender", "Windows Defender status report", OnRepDefender, 0},
    {"reports-sleep-states", "Supported sleep states report", OnRepSleepStates, 0},
    {"reports-update-history", "Installed updates history report", OnUpdateHistory, 0},
    {"reports-recycle", "Recycle Bin report", OnRecycleSize, 0},
    {"reports-env", "Environment variables report", OnEnvList, 0},
};

void PrintHelp(void);

void OnUtilInstall(const char *arg) { (void)arg; DoInstall(); }
void OnUtilUninstall(const char *arg) { (void)arg; DoUninstall(); }
void OnUtilGui(const char *arg) { (void)arg; g_launchGui = 1; }
void OnUtilHelp(const char *arg) { (void)arg; PrintHelp(); }
void OnUtilAbout(const char *arg) { (void)arg;
    char msg[800];
    sprintf(msg, APP_NAME "\n"
            "An all-in-one launcher for Windows features and settings.\n"
            "Written in pure C using the Win32 API.\n\n"
            "Includes:\n"
"  - 32 categories, 1000+ commands\n"
            "  - Services, processes and startup managers\n"
            "  - Registry explorer (read / write / delete / search)\n"
            "  - 566 Windows Settings pages (ms-settings)\n"
            "  - Network tools: ping, traceroute, Wi-Fi, DNS, firewall\n"
            "  - System tweaks: theme, taskbar, clock, power, UAC\n"
            "  - Power plans, battery status, sleep / hibernate\n"
            "  - Window & desktop tools, virtual desktops, snapping\n"
            "  - Accessibility: Sticky / Toggle / Filter / Mouse keys\n"
            "  - Storage tools: drives, defrag, disk check, cleanup\n"
            "  - Optimization: temp cleanup, Superfetch, indexing\n"
            "  - File tools: search, zip, base64, clipboard, hashing\n"
            "  - System info and hardware reports\n"
            "  - Utilities: hashing, screenshots, wallpaper, volume\n"
            "  - Windows Manual: 33 offline manual articles + live official\n"
            "    Microsoft Learn docs (search API, article reader, browser)\n\n"
            "Modes:\n"
            "  Default / --gui : GUI panel with buttons\n"
            "  --tui           : Text user interface (menus)\n"
            "  --cli           : Command line interface\n"
            "  --install       : One-click install\n"
            "  --uninstall     : Remove the install");
    Notify(msg);
}

static const Command g_Utilities[] = {
    {"install", "Install this tool into the computer", OnUtilInstall, 0},
    {"uninstall", "Remove this tool from the computer", OnUtilUninstall, 0},
    {"open-gui", "Switch to GUI mode", OnUtilGui, 0},
    {"help", "Show help and usage", OnUtilHelp, 0},
    {"about", "About this program", OnUtilAbout, 0},
    {"explain", "Explain any command in detail (category|command)", OnExplain, 1},
    {"terminal", "Open the built-in terminal", OnUtilTerminal, 0},
    {"hash-file", "Compute MD5 / SHA-1 / SHA-256 of a file", OnHashFile, 0},
    {"screenshot", "Save a full-screen screenshot as BMP", OnScreenshot, 0},
    {"set-wallpaper", "Set an image as desktop wallpaper", OnSetWallpaper, 0},
    {"volume-set", "Set system volume (needs 0-100)", OnVolumeSet, 1},
    {"volume-25", "Set system volume to 25%", OnVolumePreset, 0},
    {"volume-50", "Set system volume to 50%", OnVolumePreset, 0},
    {"volume-75", "Set system volume to 75%", OnVolumePreset, 0},
    {"volume-100", "Set system volume to 100%", OnVolumePreset, 0},
    {"monitor-off", "Turn off the display", OnMonitorOff, 0},
    {"mouse-swap-on", "Swap left/right mouse buttons", OnMouseSwapOn, 0},
    {"mouse-swap-off", "Restore left/right mouse buttons", OnMouseSwapOff, 0},
    {"caps-lock", "Toggle Caps Lock", OnCapsToggle, 0},
    {"numlock", "Toggle Num Lock", OnNumlockToggle, 0},
    {"scroll-lock", "Toggle Scroll Lock", OnScrollToggle, 0},
    {"clipboard-clear", "Clear the clipboard", OnClipboardClear, 0},
    {"god-mode", "Create God Mode folder on the desktop", OnGodMode, 0},
    {"shutdown-timer", "Schedule shutdown (needs seconds)", OnShutdownTimer, 1},
    {"abort-shutdown", "Cancel a scheduled shutdown", OnAbortShutdown, 0},
    {"open-file", "Open any file, folder or URL", OnOpenFile, 1},
    {"date-sync", "Synchronize the clock (admin)", OnDateSync, 0},
    {"time-settings", "Date and time settings", OnStTime, 0},
    {"env-open", "Open environment variables dialog", OnSysEnvVar, 0},
    {"clean-temp", "Delete all files in the Temp folder", OnCleanTemp, 0},
    {"recycle-bin", "Open the Recycle Bin", OnAppRecycle, 0},
    {"env-list", "List user and system environment variables", OnEnvList, 0},
    {"set-env", "Set a user environment variable (NAME|value)", OnEnvSet, 1},
    {"delete-env", "Delete a user environment variable (NAME)", OnEnvDelete, 1},
    {"run-command", "Run any command in a console", OnRunCommand, 1},
    {"run-command-admin", "Run any command as administrator", OnRunCommandAdmin, 1},
    {"random-password", "Generate a random password (length)", OnRandomPassword, 1},
    {"uuid-generate", "Generate a new UUID", OnUuidGenerate, 0},
    {"beep", "Play a beep (freq|ms)", OnBeep, 1},
    {"base64-text-encode", "Base64-encode a text string", OnBase64TextEncode, 1},
    {"base64-text-decode", "Base64-decode a text string", OnBase64TextDecode, 1},
    {"clean-prefetch", "Delete Prefetch files", OnCleanPrefetch, 0},
    {"clean-thumbcache", "Delete thumbnail cache files", OnCleanThumbcache, 0},
    {"restart-timer", "Schedule a restart (seconds)", OnRestartTimer, 1},
    {"monitor", "Open the system monitor (live CPU/RAM/net/disk)", OnMonitorTool, 0},
    {"system-stats", "Show a live system statistics snapshot", OnSystemStats, 0},
    {"uninstall-apps", "Open the app uninstaller", OnUninstallTool, 0},
    {"list-apps", "List installed applications", OnListApps, 0},
    {"find-files", "Search files by mask (mask|root)", OnFindFiles, 1},
    {"find-tool", "Open the file search window", OnFindTool, 0},
    {"wifi-tool", "Open the Wi-Fi manager (networks + profiles)", OnWifiTool, 0},
    {"process-list", "Show running processes by memory", OnProcessList, 0},
    {"processes-tool", "Open the process manager", OnProcessTool, 0},
    {"net-speed", "Show current network upload/download speeds", OnNetSpeed, 0},
    {"tools", "Open the interactive system tools menu (monitor, apps, wifi, ...)", OnToolsMenu, 0},
};

static const Category g_Categories[] = {
    {"System & Management", g_System, ARRAY_LEN(g_System)},
    {"Windows Settings", g_Settings, ARRAY_LEN(g_Settings)},
    {"Control Panel", g_ControlPanel, ARRAY_LEN(g_ControlPanel)},
    {"Network & Internet", g_Network, ARRAY_LEN(g_Network)},
    {"Network Tools", g_NetTools, ARRAY_LEN(g_NetTools)},
    {"Power & Session", g_Power, ARRAY_LEN(g_Power)},
    {"Power Plans & Battery", g_PowerPlans, ARRAY_LEN(g_PowerPlans)},
    {"Display & Personalization", g_Display, ARRAY_LEN(g_Display)},
    {"Audio & Volume", g_Audio, ARRAY_LEN(g_Audio)},
    {"Window & Desktop Tools", g_Windows, ARRAY_LEN(g_Windows)},
    {"Accessibility", g_Access, ARRAY_LEN(g_Access)},
    {"Security & Defender", g_Security, ARRAY_LEN(g_Security)},
    {"System Tweaks", g_Tweaks, ARRAY_LEN(g_Tweaks)},
    {"Apps & Tools", g_Apps, ARRAY_LEN(g_Apps)},
    {"App Management", g_AppMgmt, ARRAY_LEN(g_AppMgmt)},
    {"File & Folder Tools", g_FileTools, ARRAY_LEN(g_FileTools)},
    {"Storage & Disks", g_Storage, ARRAY_LEN(g_Storage)},
    {"Hardware & Devices", g_Hardware, ARRAY_LEN(g_Hardware)},
    {"Accounts & Scheduling", g_Accounts, ARRAY_LEN(g_Accounts)},
    {"Optimization & Maintenance", g_Optimize, ARRAY_LEN(g_Optimize)},
    {"Maintenance & Repair", g_Maintenance, ARRAY_LEN(g_Maintenance)},
    {"Troubleshooting & Recovery", g_Recovery, ARRAY_LEN(g_Recovery)},
    {"Developer Tools", g_Dev, ARRAY_LEN(g_Dev)},
    {"Special Folders", g_Folders, ARRAY_LEN(g_Folders)},
    {"Advanced Admin Tools", g_Admin, ARRAY_LEN(g_Admin)},
    {"Services Manager", g_Services, ARRAY_LEN(g_Services)},
    {"Processes Manager", g_Processes, ARRAY_LEN(g_Processes)},
    {"Startup Manager", g_Startup, ARRAY_LEN(g_Startup)},
    {"Registry Explorer", g_Registry, ARRAY_LEN(g_Registry)},
    {"System Info & Reports", g_Reports, ARRAY_LEN(g_Reports)},
    {"Utilities", g_Utilities, ARRAY_LEN(g_Utilities)},
    {"Windows Manual & Docs", g_Manual, ARRAY_LEN(g_Manual)},
};

static const int g_categoryCount = ARRAY_LEN(g_Categories);

void PrintHelp(void);

static int stristr(const char *hay, const char *needle) {
    size_t nh = strlen(hay), nn = strlen(needle);
    if (nn == 0) return 1;
    if (nn > nh) return 0;
    for (size_t i = 0; i + nn <= nh; i++)
        if (_strnicmp(hay + i, needle, nn) == 0) return 1;
    return 0;
}

const Category *FindCategory(const char *catName) {
    for (int i = 0; i < g_categoryCount; i++) {
        if (stricmp(catName, g_Categories[i].name) == 0)
            return &g_Categories[i];
    }
    for (int i = 0; i < g_categoryCount; i++) {
        if (stristr(g_Categories[i].name, catName))
            return &g_Categories[i];
    }
    return NULL;
}

int tokenizeLine(char *line, char *tokens[], int maxTokens) {
    int n = 0;
    char *p = strtok(line, " \t\r\n");
    while (p && n < maxTokens) {
        while (*p == '"') p++;
        size_t l = strlen(p);
        while (l > 0 && p[l - 1] == '"') p[--l] = 0;
        if (*p) tokens[n++] = p;
        p = strtok(NULL, " \t\r\n");
    }
    return n;
}

void EnsureConsole(void) {
    if (g_consoleReady) return;
    g_consoleMode = 1;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == NULL || hOut == INVALID_HANDLE_VALUE) {
        if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            if (hIn == NULL || hIn == INVALID_HANDLE_VALUE)
                freopen("CONIN$", "r", stdin);
        }
    }
    g_consoleReady = 1;
}

static void ConColor(WORD c) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != NULL && h != INVALID_HANDLE_VALUE)
        SetConsoleTextAttribute(h, c);
}

#define CC_GRAY   7
#define CC_DGRAY  8
#define CC_BLUE   9
#define CC_GREEN  10
#define CC_CYAN   11
#define CC_RED    12
#define CC_MAG    13
#define CC_YELLOW 14
#define CC_WHITE  15

static void WinBannerDraw(int dRow, int dCol, int rot, int wave,
                          HANDLE hOut, CONSOLE_SCREEN_BUFFER_INFO *cbi) {
    static const WORD pal[4] = { CC_BLUE, CC_GREEN, CC_RED, CC_YELLOW };
    static const char title[] = "WINDOWS";
    int oy = cbi->dwCursorPosition.Y, ox = cbi->dwCursorPosition.X;
    COORD c;
    for (int r = 0; r <= 10; r++) {
        c.X = (SHORT)ox; c.Y = (SHORT)(oy + r);
        SetConsoleCursorPosition(hOut, c);
        printf("                                            ");
    }
    for (int r = 1; r <= 3; r++) {
        if (r >= 1 - dRow && r <= 3 - dRow) {
            int cl = 3 - dCol;
            if (cl >= 0) { c.X = (SHORT)(ox + cl); c.Y = (SHORT)(oy + r);
                SetConsoleCursorPosition(hOut, c); ConColor(pal[(0 + rot) % 4]);
                printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"); }
            cl = 10 + dCol;
            if (cl + 4 <= 44) { c.X = (SHORT)(ox + cl); c.Y = (SHORT)(oy + r);
                SetConsoleCursorPosition(hOut, c); ConColor(pal[(1 + rot) % 4]);
                printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"); }
        }
    }
    for (int r = 5; r <= 7; r++) {
        if (r >= 5 + dRow && r <= 7 + dRow) {
            int cl = 3 - dCol;
            if (cl >= 0) { c.X = (SHORT)(ox + cl); c.Y = (SHORT)(oy + r);
                SetConsoleCursorPosition(hOut, c); ConColor(pal[(2 + rot) % 4]);
                printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"); }
            cl = 10 + dCol;
            if (cl + 4 <= 44) { c.X = (SHORT)(ox + cl); c.Y = (SHORT)(oy + r);
                SetConsoleCursorPosition(hOut, c); ConColor(pal[(3 + rot) % 4]);
                printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"); }
        }
    }
    for (int i = 0; i < 7; i++) {
        c.X = (SHORT)(ox + 3 + i * 2); c.Y = (SHORT)(oy + 9);
        SetConsoleCursorPosition(hOut, c);
        ConColor(i < wave ? CC_BLUE : CC_DGRAY);
        printf("%c", title[i]);
    }
    c.X = (SHORT)(ox + 3); c.Y = (SHORT)(oy + 10);
    SetConsoleCursorPosition(hOut, c);
    ConColor(CC_DGRAY);
    printf("WINDOWS CONTROL CENTER - PRO MAX");
}

static void PrintWinBanner(void) {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO cbi;
    if (hOut == NULL || hOut == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(hOut, &cbi)) {
        ConColor(CC_GRAY);
        printf("\n");
        for (int r = 0; r < 3; r++) {
            printf("   ");
            ConColor(CC_BLUE);  printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88");
            ConColor(CC_GRAY);  printf("   ");
            ConColor(CC_GREEN); printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88");
            ConColor(CC_GRAY);  printf("   ");
            ConColor(CC_RED);   printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88");
            ConColor(CC_GRAY);  printf("   ");
            ConColor(CC_YELLOW);printf("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88");
            ConColor(CC_GRAY);
            printf("\n");
        }
        ConColor(CC_BLUE);
        printf("  W   I   N   D   O   W   S\n");
        ConColor(CC_GRAY);
        printf("  WINDOWS CONTROL CENTER - PRO MAX\n");
        ConColor(CC_BLUE);
        printf("\n");
        return;
    }
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);

    static const int slide[5][2] = { {1, 4}, {1, 3}, {1, 2}, {0, 1}, {0, 0} };
    for (int f = 0; f < 5; f++) {
        WinBannerDraw(slide[f][0], slide[f][1], 0, 0, hOut, &cbi);
        Sleep(75);
    }
    for (int f = 1; f <= 4; f++) {
        WinBannerDraw(0, 0, f, 0, hOut, &cbi);
        Sleep(90);
    }
    for (int w = 1; w <= 7; w++) {
        WinBannerDraw(0, 0, 0, w, hOut, &cbi);
        Sleep(45);
    }
    WinBannerDraw(0, 0, 0, 7, hOut, &cbi);

    ci.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &ci);
    COORD end = { (SHORT)cbi.dwCursorPosition.X, (SHORT)(cbi.dwCursorPosition.Y + 11) };
    SetConsoleCursorPosition(hOut, end);
    ConColor(CC_BLUE);
}

void PrintHelp(void) {
    ConColor(CC_CYAN);
    printf("\n  WINDOWS CONTROL CENTER - All-in-one Windows control center\n");
    ConColor(CC_DGRAY);
    printf("  Pure C / Win32. 32 categories, 1000+ commands, English only.\n\n");
    ConColor(CC_CYAN);
    printf("  USAGE:\n");
    ConColor(CC_GRAY);
    printf("  " PROG_NAME "              Run GUI mode (buttons)\n");
    printf("  " PROG_NAME " --gui       Run GUI mode\n");
    printf("  " PROG_NAME " --float     Run the always-on-top floating widget\n");
    printf("  " PROG_NAME " --tui       Run text-menu mode (type numbers)\n");
    printf("  " PROG_NAME " --cli       Start interactive CLI prompt\n");
    ConColor(CC_YELLOW);
    printf("  " PROG_NAME " --ask <question>   Ask the assistant, get matching commands\n");
    ConColor(CC_GRAY);
    printf("  " PROG_NAME " --cli explain <category> <command>   Explain a command in detail\n");
    printf("  " PROG_NAME " --cli terminal       Built-in terminal (internal + system commands)\n");
    printf("  " PROG_NAME " --cli <category> <command> [argument]  Run one command\n");
    printf("  " PROG_NAME " --cli utilities monitor       Open the live system monitor\n");
    printf("  " PROG_NAME " --cli utilities uninstall-apps  Open the app uninstaller\n");
    printf("  " PROG_NAME " --cli utilities wifi-scan     Scan nearby Wi-Fi networks\n");
    printf("  " PROG_NAME " --cli utilities process-list  List top processes by memory\n");
    printf("  " PROG_NAME " --cli list [category]        List commands\n");
    printf("  " PROG_NAME " --install   One-click install into this computer\n");
    printf("  " PROG_NAME " --uninstall Remove the install\n");
    printf("  " PROG_NAME " --list      Print all categories\n");
    printf("  " PROG_NAME " --version   Print version\n");
    ConColor(CC_CYAN);
    printf("\n  EXAMPLE:\n");
    ConColor(CC_GRAY);
    printf("  " PROG_NAME " --cli system device-manager\n");
    printf("  " PROG_NAME " --cli network flush-dns\n");
    printf("  " PROG_NAME " --cli services services-stop spooler\n");
    printf("  " PROG_NAME " --cli registry registry-read \"HKCU\\Software\\WindowsControlCenter|Installed\"\n");
    printf("  " PROG_NAME " --cli list network\n");
    ConColor(CC_CYAN);
    printf("\n  ARGUMENT COMMANDS (need a value after the command):\n");
    ConColor(CC_GRAY);
    printf("  services-*, processes-kill*, startup-remove, registry-*,\n");
    printf("  volume-set, shutdown-timer, open-file, find-files, wifi-connect\n");
    ConColor(CC_CYAN);
    printf("\n  INTERACTIVE CLI COMMANDS:\n");
    ConColor(CC_GRAY);
    printf("  help, list [category], <category> <command> [argument],\n");
    printf("  explain <category> <command> (or <unique-command-name>), terminal, tools,\n");
    ConColor(CC_YELLOW);
    printf("  ask <question> (English, e.g. 'ask clear dns'), install, uninstall, open-gui, exit\n");
    ConColor(CC_BLUE);
}

void PrintAllCategories(void) {
    ConColor(CC_CYAN);
    printf("\n  Categories (%d):\n", g_categoryCount);
    ConColor(CC_GRAY);
    for (int i = 0; i < g_categoryCount; i++) {
        ConColor(CC_WHITE);
        printf("  %-28s", g_Categories[i].name);
        ConColor(CC_GRAY);
        printf("%d commands\n", g_Categories[i].count);
    }
    int total = 0;
    for (int i = 0; i < g_categoryCount; i++) total += g_Categories[i].count;
    ConColor(CC_GREEN);
    printf("\n  Total: %d commands\n", total);
    ConColor(CC_GRAY);
    printf("  Type: " PROG_NAME " --cli list <category> to see commands.\n");
    ConColor(CC_BLUE);
}

void PrintCategoryList(const char *catName) {
    const Category *c = FindCategory(catName);
    if (!c) { ConColor(CC_RED); printf("Unknown category '%s'. Use 'list' to see all.\n", catName); ConColor(CC_BLUE); return; }
    ConColor(CC_CYAN);
    printf("\n  [%s] (%d commands)\n", c->name, c->count);
    ConColor(CC_GRAY);
    for (int i = 0; i < c->count; i++) {
        ConColor(CC_WHITE);
        printf("  %-24s", c->items[i].name);
        ConColor(CC_GRAY);
        printf(" %s", c->items[i].desc);
        if (c->items[i].needsArg) {
            ConColor(CC_YELLOW);
            printf("  [ARG]");
            ConColor(CC_GRAY);
        }
        printf("\n");
    }
    ConColor(CC_BLUE);
}

static int FindCommandAll(const char *name, int *outCat) {
    for (int i = 0; i < g_categoryCount; i++)
        for (int j = 0; j < g_Categories[i].count; j++)
            if (stricmp(name, g_Categories[i].items[j].name) == 0) { *outCat = i; return j; }
    return -1;
}

static void ExplainOne(const Command *cmd, const char *catName) {
    const char *d = CmdDetail(catName, cmd->name);
    ReportAdd("Command : %s", cmd->name);
    ReportAdd("Category: %s", catName);
    ReportAdd("Purpose : %s", cmd->desc);
    ReportAdd("Detail  : %s", d ? d : "(no extra detail in the knowledge base)");
}

void OnExplain(const char *arg) {
    if (!arg || !arg[0]) {
        ReportAdd("Usage: explain <category>|<command>");
        ReportAdd("Example: explain settings|display");
        ReportAdd("Also works: explain <command-name> when the name is unique");
        return;
    }
    char cat[96], name[96];
    cat[0] = name[0] = 0;
    const char *sep = strchr(arg, '|');
    if (sep) {
        size_t cl = (size_t)(sep - arg);
        if (cl >= sizeof cat) cl = sizeof cat - 1;
        memcpy(cat, arg, cl);
        cat[cl] = 0;
        snprintf(name, sizeof name, "%s", sep + 1);
    } else {
        size_t bestLen = 0;
        int best = -1;
        for (int i = 0; i < g_categoryCount; i++) {
            size_t cl = strlen(g_Categories[i].name);
            if (cl > bestLen && cl < strlen(arg) &&
                strnicmp(arg, g_Categories[i].name, cl) == 0 && arg[cl] == ' ') {
                bestLen = cl;
                best = i;
            }
        }
        if (best >= 0) {
            snprintf(cat, sizeof cat, "%s", g_Categories[best].name);
            snprintf(name, sizeof name, "%s", arg + bestLen);
        } else {
            const char *sp = strchr(arg, ' ');
            if (sp) {
                char first[96];
                size_t fl = (size_t)(sp - arg);
                if (fl >= sizeof first) fl = sizeof first - 1;
                memcpy(first, arg, fl);
                first[fl] = 0;
                const Category *fc = FindCategory(first);
                if (fc) {
                    snprintf(cat, sizeof cat, "%s", fc->name);
                    snprintf(name, sizeof name, "%s", sp + 1);
                }
            }
        }
    }
    TrimSpaces(cat);
    TrimSpaces(name);
    if (cat[0] && name[0]) {
        int ci = -1;
        for (int i = 0; i < g_categoryCount; i++)
            if (stricmp(cat, g_Categories[i].name) == 0) { ci = i; break; }
        if (ci < 0) {
            ReportAdd("Unknown category '%s'. Use 'list' to see all categories.", cat);
            return;
        }
        int jj = -1;
        for (int j = 0; j < g_Categories[ci].count; j++)
            if (stricmp(name, g_Categories[ci].items[j].name) == 0) { jj = j; break; }
        if (jj < 0) {
            ReportAdd("Unknown command '%s' in category '%s'.", name, cat);
            return;
        }
        ExplainOne(&g_Categories[ci].items[jj], g_Categories[ci].name);
        return;
    }
    int foundCat = -1, foundCmd = -1, matches = 0;
    for (int i = 0; i < g_categoryCount; i++)
        for (int j = 0; j < g_Categories[i].count; j++)
            if (stricmp(arg, g_Categories[i].items[j].name) == 0) {
                matches++;
                if (matches == 1) { foundCat = i; foundCmd = j; }
            }
    if (matches == 0) {
        ReportAdd("No command named '%s' was found.", arg);
        return;
    }
    if (matches > 1) {
        ReportAdd("'%s' exists in %d categories - be more specific:", arg, matches);
        for (int i = 0; i < g_categoryCount; i++)
            for (int j = 0; j < g_Categories[i].count; j++)
                if (stricmp(arg, g_Categories[i].items[j].name) == 0)
                    ReportAdd("  %s %s", g_Categories[i].name, g_Categories[i].items[j].name);
        ReportAdd("Use: explain <category> <command>");
        return;
    }
    ExplainOne(&g_Categories[foundCat].items[foundCmd], g_Categories[foundCat].name);
}

static void RunCommandCapture(const char *cmdLine, void (*out)(const char *)) {
    char full[1200];
    snprintf(full, sizeof full, "cmd.exe /c %s", cmdLine);
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    HANDLE hOutR = NULL, hOutW = NULL;
    if (!CreatePipe(&hOutR, &hOutW, &sa, 0)) {
        out("(failed to create output pipe)");
        return;
    }
    SetHandleInformation(hOutW, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    STARTUPINFOA si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutW;
    si.hStdError = hOutW;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);
    BOOL ok = CreateProcessA(NULL, full, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                             NULL, NULL, &si, &pi);
    CloseHandle(hOutW);
    if (!ok) {
        CloseHandle(hOutR);
        out("(failed to start command)");
        return;
    }
    char buf[4096];
    DWORD rd;
    while (ReadFile(hOutR, buf, sizeof buf - 1, &rd, NULL) && rd > 0) {
        buf[rd] = 0;
        char *line = buf;
        while (line && line[0]) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = 0;
            char *e = line + strlen(line);
            while (e > line && e[-1] == '\r') *--e = 0;
            if (line[0]) out(line);
            line = nl ? nl + 1 : NULL;
        }
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutR);
}

static void TermRunCategory(const Command *cmd, const char *catName, int n,
                            char **tokens, void (*out)(const char *)) {
    const char *arg = NULL;
    if (cmd->needsArg && n >= 3) {
        static char targ[1024];
        targ[0] = 0;
        for (int k = 2; k < n; k++) {
            if (k > 2) strncat(targ, " ", sizeof targ - strlen(targ) - 1);
            strncat(targ, tokens[k], sizeof targ - strlen(targ) - 1);
        }
        arg = targ;
    } else if (cmd->needsArg) {
        char tmp[256];
        snprintf(tmp, sizeof tmp, "Command '%s' needs an argument: %s %s <value>",
                 cmd->name, catName, cmd->name);
        out(tmp);
        return;
    }
    g_reportSink = out;
    ReportClear();
    cmd->fn(arg);
    if (g_report[0]) out(g_report);
    g_reportSink = NULL;
}

void TermExecInternal(const char *line, void (*out)(const char *), int *closeFlag) {
    char buf[1200];
    char *tokens[32];
    snprintf(buf, sizeof buf, "%s", line);
    TrimSpaces(buf);
    int n = tokenizeLine(buf, tokens, 32);
    if (n == 0) return;
    if (stricmp(tokens[0], "exit") == 0 || stricmp(tokens[0], "quit") == 0 ||
        stricmp(tokens[0], "close") == 0) {
        *closeFlag = 1;
        return;
    }
    if (stricmp(tokens[0], "cls") == 0 || stricmp(tokens[0], "clear") == 0) {
        out("\x01");
        return;
    }
    if (stricmp(tokens[0], "help") == 0) {
        out("  WC Terminal - type a command and press Enter.");
        out("  Internal: <category> <command> [argument]    e.g. settings display");
        out("  Internal: explain <category> <command>       detailed description");
        out("  Internal: list [category], cls/clear, exit/quit/close, help");
        out("  Everything else runs in cmd.exe, e.g. ipconfig, dir C:\\, ping 8.8.8.8");
        return;
    }
    if (stricmp(tokens[0], "list") == 0) {
        char tmp[512];
        if (n >= 2) {
            const Category *c = FindCategory(tokens[1]);
            if (!c) {
                out("Unknown category. Type 'list' to see the category names.");
                return;
            }
            snprintf(tmp, sizeof tmp, "[%s] (%d commands)", c->name, c->count);
            out(tmp);
            for (int j = 0; j < c->count; j++) {
                snprintf(tmp, sizeof tmp, "  %s - %s", c->items[j].name, c->items[j].desc);
                out(tmp);
            }
        } else {
            snprintf(tmp, sizeof tmp, "Categories (%d):", g_categoryCount);
            out(tmp);
            for (int i = 0; i < g_categoryCount; i++) {
                snprintf(tmp, sizeof tmp, "  %-26s %3d commands", g_Categories[i].name, g_Categories[i].count);
                out(tmp);
            }
        }
        return;
    }
    if (stricmp(tokens[0], "explain") == 0) {
        if (n >= 2) {
            static char earg[1024];
            earg[0] = 0;
            for (int k = 1; k < n; k++) {
                if (k > 1) strncat(earg, " ", sizeof earg - strlen(earg) - 1);
                strncat(earg, tokens[k], sizeof earg - strlen(earg) - 1);
            }
            g_reportSink = out;
            ReportClear();
            OnExplain(earg);
            if (g_report[0]) out(g_report);
            g_reportSink = NULL;
        } else {
            out("Usage: explain <category> <command>  (or: explain <unique-command-name>)");
        }
        return;
    }
    int ci = -1;
    const Category *fc = FindCategory(tokens[0]);
    if (fc) ci = (int)(fc - g_Categories);
    if (ci >= 0 && n >= 2) {
        int jj = -1;
        for (int j = 0; j < g_Categories[ci].count; j++)
            if (stricmp(tokens[1], g_Categories[ci].items[j].name) == 0) { jj = j; break; }
        if (jj < 0) {
            char tmp[300];
            snprintf(tmp, sizeof tmp, "Unknown command '%s' in '%s'. Type: list %s",
                     tokens[1], g_Categories[ci].name, g_Categories[ci].name);
            out(tmp);
            return;
        }
        TermRunCategory(&g_Categories[ci].items[jj], g_Categories[ci].name, n, tokens, out);
        return;
    }
    if (ci >= 0) {
        char tmp[300];
        snprintf(tmp, sizeof tmp, "Usage: %s <command> [argument]. Type: list %s",
                 g_Categories[ci].name, g_Categories[ci].name);
        out(tmp);
        return;
    }
    char head[256];
    snprintf(head, sizeof head, "C:\\> %s", line);
    out(head);
    RunCommandCapture(line, out);
}

static void TermConOut(const char *text) {
    if (text[0] == '\x01') {
        system("cls");
        return;
    }
    printf("%s\n", text);
}

void TermConsoleLoop(void) {
    char line[1024];
    int closeFlag = 0;
    TermConOut("WC Terminal - type 'help' for usage, 'exit' to leave.");
    while (!closeFlag) {
        printf("wc> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
        if (l == 0) continue;
        TermExecInternal(line, TermConOut, &closeFlag);
    }
    TermConOut("Terminal closed.");
}

void OnUtilTerminal(const char *arg) {
    (void)arg;
    if (g_consoleMode) {
        TermConsoleLoop();
    } else {
        OpenTerminalWindow();
    }
}

#define IDC_TERM_OUT 401
#define IDC_TERM_IN 402
#define IDC_TERM_RUN 403

static HWND g_hTermWnd, g_hTermOut, g_hTermIn;
static WNDPROC g_oldTermInProc;
static char g_termHist[16][512];
static int g_termHistN = 0, g_termHistPos = -1;

static void TermGuiOut(const char *text) {
    if (!g_hTermOut) return;
    if (text[0] == '\x01') {
        SendMessageA(g_hTermOut, WM_SETREDRAW, FALSE, 0);
        SetWindowTextA(g_hTermOut, "");
        SendMessageA(g_hTermOut, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_hTermOut, NULL, TRUE);
        return;
    }
    int len = GetWindowTextLengthA(g_hTermOut);
    SendMessageA(g_hTermOut, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(g_hTermOut, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessageA(g_hTermOut, EM_REPLACESEL, 0, (LPARAM)"\r\n");
    SendMessageA(g_hTermOut, EM_SCROLLCARET, 0, 0);
}

static void TermGuiRun(void) {
    char line[1024];
    GetWindowTextA(g_hTermIn, line, sizeof line);
    if (line[0] == 0) return;
    if (g_termHistN == 0 || strcmp(line, g_termHist[g_termHistN - 1]) != 0) {
        if (g_termHistN < 16)
            snprintf(g_termHist[g_termHistN], sizeof g_termHist[g_termHistN], "%s", line);
        g_termHistN++;
    }
    g_termHistPos = g_termHistN;
    SetWindowTextA(g_hTermIn, "");
    char echo[1100];
    snprintf(echo, sizeof echo, "wc> %s", line);
    TermGuiOut(echo);
    int closeFlag = 0;
    TermExecInternal(line, TermGuiOut, &closeFlag);
    if (closeFlag && g_hTermWnd) DestroyWindow(g_hTermWnd);
}

static LRESULT CALLBACK TermInSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            TermGuiRun();
            return 0;
        }
        if (wParam == VK_UP && g_termHistPos > 0) {
            g_termHistPos--;
            SetWindowTextA(hWnd, g_termHist[g_termHistPos]);
            SendMessageA(hWnd, EM_SETSEL, 0, -1);
            return 0;
        }
        if (wParam == VK_DOWN && g_termHistPos >= 0 && g_termHistPos < g_termHistN - 1) {
            g_termHistPos++;
            SetWindowTextA(hWnd, g_termHist[g_termHistPos]);
            SendMessageA(hWnd, EM_SETSEL, 0, -1);
            return 0;
        }
    }
    return CallWindowProcA(g_oldTermInProc, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK TermWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hTermOut = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER |
                                       ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
                                       WS_VSCROLL | WS_HSCROLL,
                                       10, 10, 760, 400, hWnd, (HMENU)IDC_TERM_OUT, g_hInst, NULL);
            SendMessageA(g_hTermOut, EM_SETREADONLY, TRUE, 0);
            g_hTermIn = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      10, 420, 610, 28, hWnd, (HMENU)IDC_TERM_IN, g_hInst, NULL);
            g_oldTermInProc = (WNDPROC)SetWindowLongPtrA(g_hTermIn, GWLP_WNDPROC,
                                                         (LONG_PTR)TermInSubProc);
            CreateWindowA("BUTTON", "Run", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                          630, 420, 140, 28, hWnd, (HMENU)IDC_TERM_RUN, g_hInst, NULL);
            SetFocus(g_hTermIn);
            char welcome[256];
            snprintf(welcome, sizeof welcome,
                     "WC Terminal - type 'help' for usage, 'exit' to close. Running on %s.",
                     g_consoleMode ? "console" : "Windows");
            TermGuiOut(welcome);
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_TERM_RUN) {
                TermGuiRun();
                SetFocus(g_hTermIn);
            }
            break;
        case WM_SETFOCUS:
            if (g_hTermIn) SetFocus(g_hTermIn);
            break;
        case WM_DESTROY:
            g_hTermWnd = NULL;
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OpenTerminalWindow(void) {
    WNDCLASSA tc;
    memset(&tc, 0, sizeof tc);
    tc.lpfnWndProc = TermWndProc;
    tc.hInstance = g_hInst;
    tc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    tc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    tc.lpszClassName = "WcTermClass";
    RegisterClassA(&tc);
    g_hTermWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcTermClass",
                                 APP_NAME " - Terminal",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
                                 NULL, NULL, g_hInst, NULL);
    if (!g_hTermWnd) return;
    ShowWindow(g_hTermWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static const char *g_AskHints[][2] = {
    {"wallpaper", "set-wallpaper"}, {"background", "set-wallpaper"},
    {"slideshow", "set-wallpaper"}, {"dns", "flush-dns"},
    {"internet", "open-browser"}, {"wifi", "wifi"},
    {"wireless", "wifi"}, {"bluetooth", "bluetooth"},
    {"virus", "quick-scan"}, {"malware", "defender-quick-scan"},
    {"scan", "quick-scan"}, {"slow", "clean-windows-temp"},
    {"slow", "superfetch-off"}, {"speed", "superfetch-off"},
    {"lag", "superfetch-off"}, {"free", "disk-cleanup"},
    {"update", "windows-update"}, {"password", "account-password-set"},
    {"battery", "battery-status"}, {"shutdown", "shutdown"},
    {"restart", "restart"}, {"reboot", "restart"},
    {"sleep", "sleep"}, {"hibernate", "hibernate"},
    {"lock", "lock"}, {"screenshot", "screenshot"},
    {"capture", "screenshot"}, {"clean", "disk-cleanup"},
    {"temp", "clean-temp"}, {"defrag", "defrag"},
    {"disk", "drives-list"}, {"task", "task-manager"},
    {"process", "processes-list"}, {"service", "services-list"},
    {"driver", "driver-list"}, {"firewall", "firewall"},
    {"defender", "windows-security"}, {"ip", "ipconfig"},
    {"ping", "ping"}, {"network", "network-settings"},
    {"sound", "sound-settings"}, {"volume", "volume-set"},
    {"mute", "mute"}, {"brightness", "display-settings"},
    {"resolution", "resolution"}, {"theme", "themes"},
    {"cursor", "mouse-access"}, {"mouse", "mouse"},
    {"keyboard", "keyboard"}, {"language", "language"},
    {"region", "region"}, {"date", "date-time"},
    {"time", "time-settings"}, {"printer", "printers-settings"},
    {"camera", "camera"}, {"microphone", "microphone"},
    {"notifications", "notifications"}, {"storage", "storage-settings"},
    {"recycle", "empty-recycle-bin"}, {"startup", "startup-list"},
    {"registry", "registry-read"}, {"regedit", "registry-open-editor"},
    {"device", "device-manager"}, {"hardware", "device-manager"},
    {"gpu", "gpu-list"}, {"cpu", "reports-cpu"},
    {"ram", "reports-ram"}, {"memory", "reports-ram"},
    {"user", "accounts-list"}, {"account", "accounts-list"},
    {"power", "power-options"}, {"plan", "power-plan-list"},
    {"winver", "winver"}, {"info", "system-information"},
    {"folder", "file-explorer"}, {"explorer", "file-explorer"},
    {"notepad", "notepad"}, {"cmd", "cmd"},
    {"terminal", "windows-terminal"}, {"calculator", "calculator"},
    {"paint", "paint"}, {"wordpad", "wordpad"},
    {"hidden", "show-hidden-files-on"}, {"extensions", "show-file-extensions-on"},
    {"airplane", "airplane-mode"}, {"vpn", "vpn"},
    {"proxy", "proxy"}, {"error", "event-viewer"},
    {"log", "event-viewer"}, {"crash", "event-viewer"},
    {"troubleshoot", "troubleshoot"}, {"diagnose", "troubleshoot"},
    {"repair", "system-restore"}, {"restore", "system-restore"},
    {"backup", "windows-backup"}, {"uninstall", "app-uninstall"},
    {"remove", "startup-remove"}, {"install", "msi-install"},
    {"game", "gaming"}, {"xbox", "xbox-game-bar"},
    {"night", "night-light"}, {"font", "fonts"},
    {"display", "display-settings"}, {"monitor", "monitors-list"},
    {"energy", "energy-report"}, {"taskbar", "taskbar-settings"},
    {"clock", "time-settings"}, {"location", "location"},
    {"privacy", "privacy"}, {"audio", "sound-devices"},
    {"speaker", "sound-settings"}, {"wake", "power-options"},
    {"dpi", "resolution"}, {"read", "narrator"},
    {"contrast", "high-contrast"}, {"color", "colors"},
    {"browser", "open-browser"}, {"default", "default-apps"},
    {"photo", "pictures"}, {"image", "system-image"},
    {"clip", "snipping-tool"}, {"snipping", "snipping-tool"},
    {"record", "steps-recorder"}, {"gaming", "gaming"},
    {"controller", "game-controllers"}, {"touch", "touch"},
    {"pen", "pen"}, {"project", "projecting"},
    {"remote", "remote-desktop"}, {"rdp", "remote-desktop"},
    {"share", "shares-list"}, {"nearby", "nearby-sharing"},
    {"phone", "phone-link"}, {"mobile", "phone-link"},
    {"sync", "sync-settings"}, {"onedrive", "onedrive"},
    {"recovery", "recovery"}, {"reset", "reset-pc"},
    {"fresh", "reset-pc"}, {"boot", "advanced-startup"},
    {"scheduled", "task-scheduler"}, {"hotspot", "mobile-hotspot"},
    {"release", "ipconfig-release"}, {"renew", "ipconfig-renew"},
    {"gateway", "ipconfig"}, {"mac", "getmac"},
    {"address", "ipconfig"}, {"test", "check-port"},
    {"latency", "ping"}, {"route", "route-print"},
    {"tracert", "tracert"}, {"trace", "tracert"},
    {"arp", "arp"}, {"netstat", "netstat"},
    {"connection", "network-connections"}, {"port", "netstat-port"},
    {"hostname", "nslookup-host"}, {"computer", "computer-management"},
    {"rename", "file-rename"}, {"workgroup", "shared-folders"},
    {"domain", "computer-management"}, {"admin", "cmd-admin"},
    {"guest", "account-create"}, {"family", "family-options"},
    {"parental", "family-options"}, {"pin", "windows-hello"},
    {"face", "face-enrollment"}, {"fingerprint", "fingerprint-enrollment"},
    {"biometric", "fingerprint-enrollment"}, {"sign-in", "signin-options"},
    {"login", "signin-options"}, {"dynamic", "dynamic-lock"},
    {"timer", "shutdown-timer"}, {"scheduler", "task-scheduler"},
    {"visual", "visualfx-best"}, {"animation", "window-animations-on"},
    {"transparency", "transparency-on"}, {"focus", "focus-assist"},
    {"disturb", "focus-assist"}, {"quiet", "focus-assist"},
    {"do not disturb", "focus-assist"}, {"warm", "night-light"},
    {"dark", "light-mode-off"}, {"light", "light-mode-on"},
    {"pointer", "mouse-access"}, {"scroll", "mouse"},
    {"click", "mouse"}, {"touchpad", "devices-touchpad"},
    {"typing", "typing"}, {"spell", "typing"},
    {"voice", "speech"}, {"speech", "speech"},
    {"narrator", "narrator"}, {"magnifier", "magnifier"},
    {"zoom", "zoom-in"}, {"caption", "closed-captions"},
    {"subtitle", "closed-captions"}, {"hearing", "easeofaccess-hearing"},
    {"aids", "easeofaccess-hearing"}, {"sticky", "sticky-keys-on"},
    {"toggle", "toggle-keys-on"}, {"filter", "filter-keys-on"},
    {"key", "keyboard"}, {"shortcut", "keyboard"},
    {"disks", "disk-management"}, {"volumes", "disk-management"},
    {"partition", "disk-management"}, {"format", "disk-management"},
    {"chkdsk", "chkdsk"}, {"check", "disk-check"},
    {"bitlocker", "bitlocker"}, {"encrypt", "bitlocker"},
    {"compress", "zip-create"}, {"zip", "zip-create"},
    {"extract", "zip-extract"}, {"unzip", "zip-extract"},
    {"copy", "file-copy"}, {"move", "file-move"},
    {"delete", "file-delete"}, {"create", "folder-create"},
    {"mkdir", "folder-create"}, {"new", "folder-create"},
    {"hide", "show-hidden-files-off"}, {"show", "show-hidden-files-on"},
    {"attributes", "file-info"}, {"readonly", "file-info"},
    {"permissions", "file-info"}, {"owner", "file-info"},
    {"takeown", "file-info"}, {"find", "file-search"},
    {"locate", "file-search"}, {"lnk", "file-explorer"},
    {"desktop", "desktop"}, {"windows", "windows-dir"},
    {"system32", "system32"}, {"downloads", "downloads"},
    {"documents", "documents"}, {"pictures", "pictures"},
    {"music", "music"}, {"videos", "videos"},
    {"bin", "recycle-bin"}, {"trash", "recycle-bin"},
    {"checkpoint", "system-restore-create"}, {"safe", "safe-mode"},
    {"msconfig", "msconfig"}, {"config", "msconfig"},
    {"daemon", "services-list"}, {"auto", "startup-apps"},
    {"start", "services-start"}, {"stop", "services-stop"},
    {"disable", "services-disable"}, {"enable", "services-enable"},
    {"delay", "keyboard-repeat-delay"}, {"trigger", "services-list"},
    {"fail", "services-list"}, {"hang", "services-list"},
    {"taskmanager", "task-manager"}, {"kill", "processes-kill"},
    {"end", "processes-kill"}, {"terminate", "processes-kill"},
    {"priority", "processes-list"}, {"autostart", "startup-apps"},
    {"write", "registry-write"}, {"delete-key", "registry-delete"},
    {"export", "registry-backup"}, {"import", "registry-import"},
    {"sysinfo", "systeminfo"}, {"about", "about-windows"},
    {"version", "winver"}, {"os", "reports-os"},
    {"activation", "activation-status"}, {"license", "activation-status"},
    {"genuine", "activation-status"}, {"processor", "reports-cpu"},
    {"graphics", "gpu-list"}, {"video", "videos"},
    {"space", "disk-space-report"}, {"charge", "battery-status"},
    {"uptime", "reports-uptime"}, {"runtime", "reports-uptime"},
    {"temperature", "reports-cpu"}, {"heat", "reports-cpu"},
    {"directx", "directx-diagnostics"}, {"dxdiag", "directx-diagnostics"},
    {"bios", "reports-bios"}, {"firmware", "reports-bios"},
    {"motherboard", "reports-motherboard"}, {"adapters", "adapter-settings"},
    {"interfaces", "adapter-list"}, {"hosts", "edit-hosts"},
    {"mtu", "adapter-settings"}, {"wlan", "wifi-profiles"},
    {"passwords", "wifi-password"}, {"metered", "data-usage"},
    {"public", "public-ip"}, {"private", "ipconfig"},
    {"sharing", "advanced-sharing"}, {"discovery", "advanced-sharing"},
    {"homegroup", "advanced-sharing"}, {"pair", "bluetooth-add-device"},
    {"printers", "printers-list"}, {"add-printer", "printers-add"},
    {"default-printer", "printers-default"}, {"scanner", "printers-scanner"},
    {"fax", "printers-fax"}, {"gamepad", "game-controllers"},
    {"joystick", "game-controllers"}, {"gamebar", "xbox-game-bar"},
    {"capture", "screenshot"}, {"nvidia", "gpu-list"},
    {"amd", "gpu-list"}, {"intel", "reports-cpu"},
    {"speakers", "sound-devices"}, {"recording", "recording"},
    {"app-volume", "app-volume-settings"}, {"scheme", "sound-effects"},
    {"beep", "beep"}, {"alerts", "notifications"},
    {"actions", "notification-center"}, {"quick", "search"},
    {"calendar", "time-settings"}, {"format", "region"},
    {"unicode", "region"}, {"keyboard-lang", "language-keyboard"},
    {"timezone", "time-settings"}, {"utc", "time-settings"},
    {"tzutil", "time-settings"}, {"auto-time", "date-sync"},
    {"internet-time", "date-sync"}, {"startmenu", "start"},
    {"tiles", "start"}, {"tray", "taskbar-settings"},
    {"notify", "notifications"}, {"icons", "desktop-icon-thispc-on"},
    {"widgets", "widgets-button-on"}, {"sidebar", "widgets-button-on"},
    {"multitasking", "multitasking"}, {"snap", "snap-windows-on"},
    {"alt-tab", "task-switcher"}, {"virtual", "virtual-desktop-new"},
    {"desktops", "virtual-desktop-new"}, {"clipboard", "clipboard-settings"},
    {"history", "update-history"}, {"paste", "clipboard-history"},
    {"cloud-clipboard", "clipboard-settings"}, {"dock", "tablet-mode"},
    {"tablet", "tablet-mode"}, {"rotation", "multiple-displays"},
    {"flight", "airplane-mode"}, {"gps", "location"},
    {"webcam", "camera-privacy"}, {"mic", "microphone-privacy"},
    {"contacts", "contacts-privacy"}, {"email", "email-accounts"},
    {"call", "call-history-privacy"}, {"sms", "privacy-messaging"},
    {"mms", "privacy-messaging"}, {"diag", "diagnostics-privacy"},
    {"telemetry", "telemetry-off"}, {"feedback", "feedback-privacy"},
    {"activity", "activity-history"}, {"indexing", "indexing-options"},
    {"cortana", "cortana"}, {"assistant", "cortana"},
    {"profile", "user-profiles"}, {"security-key", "security-key-enrollment"},
    {"lock-screen", "lock-screen"}, {"administrator", "cmd-admin"},
    {"workplace", "work-school"}, {"school", "work-school"},
    {"windows-backup", "windows-backup"}, {"one-drive", "onedrive"},
    {"optional", "optional-features"}, {"features", "optional-features"},
    {"programs", "programs-features"}, {"apps", "apps"},
    {"apps-install", "msi-install"}, {"associations", "default-apps"},
    {"startup-apps", "startup-apps"}, {"play", "media-play-pause"},
    {"apps-repair", "msi-repair"}, {"apps-reset", "reset-pc"},
    {"fresh-start", "reset-pc"}, {"reinstall", "reset-pc"},
    {"advanced-startup", "advanced-startup"}, {"uefi", "advanced-startup"},
    {"shadow", "system-restore"}, {"volume-shadow", "system-restore"},
    {"recovery-drive", "recovery-drive"}, {"media", "media-player"},
    {"fix", "startup-repair"}, {"gone", "startup-repair"},
    {"sfc", "sfc-scannow"}, {"system-file", "sfc-scannow"},
    {"integrity", "sfc-scannow"}, {"corrupt", "sfc-scannow"},
    {"dism", "dism-check"}, {"image", "system-image"},
    {"health", "device-health"}, {"servicing", "dism-restore"},
    {"troubleshooter", "troubleshoot"}, {"wizard", "troubleshoot"},
    {"blue-screen", "event-viewer"}, {"bsod", "event-viewer"},
    {"dump", "hex-dump"}, {"memory-diagnostic", "memory-diagnostic"},
    {"ram-test", "memory-diagnostic"}, {"perf", "performance-monitor"},
    {"performance", "performance-options"}, {"reliability", "reliability-monitor"},
    {"events", "event-viewer"}, {"logs", "event-viewer"},
    {"warnings", "event-viewer"}, {"cleanup", "disk-cleanup"},
    {"junk", "clean-temp"}, {"cache", "clean-thumbcache"},
    {"prefetch", "clean-prefetch"}, {"browser-cache", "clean-temp"},
    {"optimize", "disk-defrag"}, {"fragment", "disk-defrag"},
    {"trim", "disk-defrag"}, {"ssd", "disk-defrag"},
    {"smart", "drive-info"}, {"verify", "disk-check"},
    {"mount", "disk-management"}, {"dismount", "disk-management"},
    {"map", "network-folder"}, {"network-drive", "network-folder"},
    {"unmap", "network-folder"}, {"ram-cleaner", "clean-temp"},
    {"boost", "performance-options"}, {"fast", "fast-startup-on"},
    {"tune", "performance-options"}, {"game-boost", "game-mode-on"},
    {"balanced", "plan-balanced"}, {"high", "plan-high-performance"},
    {"ultimate", "power-plan-ultimate"}, {"battery-saver", "battery-saver"},
    {"efficiency", "energy-report"}, {"suspend", "sleep"},
    {"standby", "sleep"}, {"hybrid", "hibernation-on"},
    {"wake-timer", "power-options"}, {"logoff", "signout"},
    {"signout", "signout"}, {"switch", "task-switcher"},
    {"fast-startup", "fast-startup-on"}, {"hybrid-boot", "fast-startup-on"},
    {"clear", "clipboard-clear"}, {"hiberfile", "hibernation-off"},
    {"menu", "start"}, {"parallel", "msconfig"},
    {"cores", "msconfig"}, {"boot-default", "msconfig"},
    {"option", "msconfig"}, {"boot-options", "msconfig"},
    {"pagefile", "performance-options"}, {"swap", "performance-options"},
    {"ntp", "date-sync"}, {"server", "services-list"},
    {"saver", "screen-saver"}, {"sense", "storage-sense"},
    {"auto-clean", "storage-sense"}, {"save", "clipboard-save"},
    {"storage-optimize", "system-storage-optimization"},
    {"your-phone", "phone-link"}, {"link", "phone-link"},
    {"cross-device", "phone-link"}, {"messages", "email-accounts"},
    {"text", "typing"}, {"on-this-device", "phone-link"},
    {"data", "data-usage"}, {"usage", "data-usage"},
    {"data-limit", "data-usage"}, {"limit", "data-usage"},
    {"background-data", "background-apps"}, {"quiet-hours", "focus-assist"},
    {"action", "notification-center"}, {"center", "notification-center"},
    {"flyout", "search"}, {"edit", "edit-hosts"},
    {"status", "tweaks-status"}, {"idle", "lock-screen"},
    {"timeout", "screen-saver-timeout"}, {"blank", "screen-saver-timeout"},
    {"off", "monitor-off"}, {"refresh", "restart-explorer"},
    {"rate", "keyboard-repeat-rate"}, {"hz", "resolution"},
    {"scale", "display-settings"}, {"size", "folder-size"},
    {"screen", "screenshot"}, {"orientation", "multiple-displays"},
    {"flip", "multiple-displays"}, {"landscape", "multiple-displays"},
    {"portrait", "multiple-displays"}, {"multiple", "multiple-displays"},
    {"monitors", "monitors-list"}, {"extend", "multiple-displays"},
    {"duplicate", "dup-files"}, {"second", "multiple-displays"},
    {"projector", "projecting"}, {"presentation", "projecting"},
    {"accent", "accent-colors"}, {"themes", "themes"},
    {"dark-theme", "light-mode-off"}, {"light-theme", "light-mode-on"},
    {"high-contrast", "high-contrast"}, {"animations", "window-animations-on"},
    {"shadows", "visualfx-best"}, {"typeface", "fonts"},
    {"font-size", "accessibility-text-size"}, {"text-size", "accessibility-text-size"},
    {"large", "accessibility-text-size"}, {"clear-type", "fonts"},
    {"cleartype", "fonts"}, {"smoothing", "fonts"},
    {"double", "mouse-double-click-speed"}, {"gesture", "devices-touchpad"},
    {"swipe", "devices-touchpad"}, {"ink", "pen"},
    {"stylus", "devices-stylus"}, {"autocorrect", "typing"},
    {"suggestions", "typing"}, {"dictation", "easeofaccess-dictation"},
    {"screen-reader", "narrator"}, {"lens", "magnifier"},
    {"closed", "closed-captions"}, {"captions", "closed-captions"},
    {"subtitles", "closed-captions"}, {"shift", "sticky-keys-on"},
    {"repeat", "filter-keys-on"}, {"ease", "accessibility"},
    {"accessibility", "accessibility"}, {"settings-app", "settings"},
    {"control", "control-panel"}, {"panel", "control-panel"},
    {"classic", "control-panel"}, {"chrome", "open-browser"},
    {"edge", "edge"}, {"firefox", "open-browser"},
    {"files", "file-explorer"}, {"folders", "file-explorer"},
    {"command", "cmd"}, {"prompt", "cmd"},
    {"dos", "cmd"}, {"elevated", "cmd-admin"},
    {"powershell", "powershell"}, {"ps", "powershell"},
    {"wt", "windows-terminal"}, {"run", "run-dialog"},
    {"dialog", "run-dialog"}, {"registry-editor", "registry-open-editor"},
    {"taskmgr", "task-manager"}, {"perfmon", "performance-monitor"},
    {"resmon", "resource-monitor"}, {"services.msc", "services-console"},
    {"diskmgmt", "disk-management"}, {"devmgmt", "device-manager"},
    {"compmgmt", "computer-management"}, {"computer-management", "computer-management"},
    {"printmanagement", "print-management"}, {"eventvwr", "event-viewer"},
    {"msinfo32", "system-information"}, {"taskschd", "task-scheduler"},
    {"gpedit", "group-policy"}, {"group-policy", "group-policy"},
    {"policy", "group-policy"}, {"secpol", "secpol"},
    {"security-policy", "secpol"}, {"lusrmgr", "lusrmgr"},
    {"local-users", "lusrmgr"}, {"netplwiz", "user-accounts"},
    {"odbc", "odbc"}, {"data-sources", "odbc"},
    {"sysdm", "system-properties"}, {"system-properties", "system-properties"},
    {"advanced", "advanced-settings"}, {"charmap", "fonts"},
    {"symbol", "fonts"}, {"editor", "notepad"},
    {"mspaint", "paint"}, {"calc", "calculator"},
    {"write", "wordpad"}, {"snip", "snipping-tool"},
    {"windows-media", "media-player"}, {"wmp", "media-player"},
    {"recorder", "steps-recorder"}, {"osk", "keyboard"},
    {"on-screen", "keyboard"}, {"read-aloud", "narrator"},
    {"paint3d", "paint"}, {"3d", "paint"},
    {"image-viewer", "pictures"}, {"microsoft-store", "store"},
    {"alarms", "time-settings"}, {"weather", "time-settings"},
    {"news", "start"}, {"mail", "email-accounts"},
    {"outlook", "email-accounts"}, {"people", "contacts"},
    {"tips", "tips"}, {"windows-defender", "windows-security"},
    {"security", "windows-security"}, {"windows-firewall", "firewall"},
    {"network-share", "network-folder"}, {"shares", "shares-list"},
    {"pairing", "bluetooth-add-device"}, {"connections", "network-connections"},
    {"game-dvr", "game-dvr-on"}, {"powercfg", "power-options"},
    {"ms-settings", "settings"}, {"windows-settings", "settings"},
    {"home", "this-pc"}, {"download", "downloads"},
    {"docs", "documents"}, {"images", "pictures"},
    {"songs", "music"}, {"movies", "videos"},
    {"recent", "recent-files"}, {"recents", "recent-files"},
    {"recycle-bin", "recycle-bin"}, {"system-drive", "this-pc"},
    {"c-drive", "this-pc"}, {"root", "this-pc"},
    {"network-folder", "network-folder"}, {"minimize", "minimize-all"},
    {"cycle", "cycle-windows"}, {"resize", "window-restore"},
    {"position", "window-activate"}, {"bring", "window-activate"},
    {"to-front", "window-activate"}, {"fullscreen", "window-restore"},
    {"enumerate", "window-list"}, {"topmost", "window-list"},
    {"always", "window-list"}, {"on-top", "window-list"},
    {"opacity", "window-list"}, {"glass", "window-list"},
    {"title", "window-list"}, {"exists", "window-list"},
    {"teleport", "window-activate"}, {"send", "window-activate"},
    {"other", "other-users"}, {"setup", "install"},
    {"one-click", "install"}, {"categories", "help"},
    {"graphical", "open-gui"}, {"browse", "open-gui"},
{"window", "window-list"}, {"files", "file-explorer"},
    {"manual", "manual-overview"}, {"guide", "manual-overview"},
    {"tutorial", "manual-overview"}, {"learn", "docs-search"},
    {"documentation", "docs-search"}, {"docs", "docs-search"},
    {"microsoft-learn", "docs-search"}, {"api", "docs-win32-api"},
    {"how-to", "manual-overview"}, {"help-topic", "manual-faq"},
    {"book", "manual-overview"}, {"readme", "manual-overview"},
    {"wsl", "manual-wsl"}, {"powershell-docs", "docs-powershell"},
    {"terminal-docs", "docs-terminal"}, {"shortcut", "manual-shortcuts"},
    {"hotkey", "manual-shortcuts"}, {"keyboard-keys", "manual-shortcuts"},
    {"glossary", "manual-glossary"}, {"terms", "manual-glossary"},
    {"errors", "manual-errors"}, {"error-code", "manual-errors"},
    {"faq", "manual-faq"}, {"questions", "manual-faq"},
    {"clean-install", "manual-clean-install"}, {"fresh-install", "manual-clean-install"},
    {"defender-docs", "manual-defender"}, {"hyperv", "docs-hyperv"},
    {"dev-drive", "docs-dev-drive"}, {"release-health", "docs-release-health"},
};

static void AskAssistant(const char *question, int allowRun) {
    char q[1024];
    strncpy(q, question, sizeof q - 1);
    q[sizeof q - 1] = 0;
    for (char *p = q; *p; p++)
        if (!isalpha((unsigned char)*p)) *p = ' ';
    char *words[48];
    int lt = 0;
    char *tok = strtok(q, " \t\r\n");
    static const char *stopwords[] = {"the", "and", "for", "how", "you", "can", "that",
                                      "with", "what", "when", "where", "why", "this",
                                      "from", "are", "was", "have", "has", "its", "get",
                                      "use", "using", "does", "will", "would", "could",
                                      "very", "much", "really", "just", "some", "more",
                                      "most", "there", "here", "into", "about", "than"};
    while (tok && lt < 48) {
        int len = (int)strlen(tok);
        if (len >= 3) {
            for (int i = 0; i < len; i++) tok[i] = tolower((unsigned char)tok[i]);
            int sw = 0;
            for (int s = 0; s < (int)(sizeof stopwords / sizeof stopwords[0]); s++)
                if (strcmp(tok, stopwords[s]) == 0) { sw = 1; break; }
            if (!sw) words[lt++] = tok;
        }
        tok = strtok(NULL, " \t\r\n");
    }
    if (lt == 0) {
        printf("Please describe what you want to do in English.\n");
        printf("Example: ask change wallpaper / ask clear dns cache\n");
        return;
    }

    int nRes = 0;
    int bestCat[64], bestItem[64], bestScore[64];
    for (int i = 0; i < g_categoryCount; i++) {
        for (int j = 0; j < g_Categories[i].count; j++) {
            const Command *c = &g_Categories[i].items[j];
            int score = 0;
            char nameL[128], descL[512], catL[64];
            strncpy(nameL, c->name, sizeof nameL - 1); nameL[sizeof nameL - 1] = 0;
            strncpy(descL, c->desc, sizeof descL - 1); descL[sizeof descL - 1] = 0;
            strncpy(catL, g_Categories[i].name, sizeof catL - 1); catL[sizeof catL - 1] = 0;
            for (char *p = nameL; *p; p++) *p = tolower((unsigned char)*p);
            for (char *p = descL; *p; p++) *p = tolower((unsigned char)*p);
            for (char *p = catL; *p; p++) *p = tolower((unsigned char)*p);
            for (int k = 0; k < lt; k++) {
                if (strstr(nameL, words[k])) score += 6;
                if (strstr(descL, words[k])) score += 3;
                if (strstr(catL, words[k])) score += 1;
            }
            if (score > 0) {
                if (nRes < 64) {
                    bestCat[nRes] = i; bestItem[nRes] = j; bestScore[nRes] = score;
                    nRes++;
                }
            }
        }
    }

    for (int i = 0; i < (int)(sizeof g_AskHints / sizeof g_AskHints[0]); i++) {
        int matched = 0;
        for (int k = 0; k < lt; k++)
            if (strcmp(words[k], g_AskHints[i][0]) == 0) { matched = 1; break; }
        if (!matched) {
            for (char *p = q; *p; p++)
                if (!isalpha((unsigned char)*p)) *p = ' ';
            char *h = strstr(q, g_AskHints[i][0]);
            if (h) {
                char before = (h == q) ? ' ' : *(h - 1);
                char after = *(h + strlen(g_AskHints[i][0]));
                if ((before == ' ' || before == 0) && (after == ' ' || after == 0)) matched = 1;
            }
        }
        if (matched) {
            int hc = -1;
            int hi = FindCommandAll(g_AskHints[i][1], &hc);
            if (hi >= 0) {
                int found = -1;
                for (int r = 0; r < nRes; r++)
                    if (bestCat[r] == hc && bestItem[r] == hi) { found = r; break; }
                if (found >= 0) bestScore[found] += 40;
                else if (nRes < 64) { bestCat[nRes] = hc; bestItem[nRes] = hi; bestScore[nRes] = 30; nRes++; }
            }
        }
    }

    for (int i = 1; i < nRes; i++) {
        int sc = bestScore[i], bc = bestCat[i], bi = bestItem[i];
        int k = i - 1;
        while (k >= 0 && bestScore[k] < sc) {
            bestScore[k + 1] = bestScore[k];
            bestCat[k + 1] = bestCat[k];
            bestItem[k + 1] = bestItem[k];
            k--;
        }
        bestScore[k + 1] = sc; bestCat[k + 1] = bc; bestItem[k + 1] = bi;
    }

    int shown = nRes < 10 ? nRes : 10;
    if (nRes == 0) {
        ConColor(CC_RED);
        printf("No matching commands found for '%s'.\n", question);
        printf("Try simpler English words, e.g. 'ask change wallpaper'.\n");
        ConColor(CC_BLUE);
        return;
    }
    ConColor(CC_CYAN);
    printf("\n  Best matches (%d found) for: %s\n", nRes, question);
    ConColor(CC_DGRAY);
    printf("  ------------------------------------------------------------------\n");
    ConColor(CC_GRAY);
    for (int i = 0; i < shown; i++) {
        const Command *c = &g_Categories[bestCat[i]].items[bestItem[i]];
        ConColor(CC_GREEN);
        printf("%3d.", i + 1);
        ConColor(CC_DGRAY);
        printf(" [%s]", g_Categories[bestCat[i]].name);
        ConColor(CC_WHITE);
        printf(" %s", c->name);
        ConColor(CC_GRAY);
        printf(" -> %s", c->desc);
        if (c->needsArg) {
            ConColor(CC_YELLOW);
            printf("  [ARG]");
        }
        printf("\n");
    }
    if (nRes > shown) {
        ConColor(CC_DGRAY);
        printf("  ... and %d more (ask with different words to refine).\n", nRes - shown);
    }
    ConColor(CC_BLUE);

    if (!allowRun) {
        ConColor(CC_DGRAY);
        printf("  To run one: " PROG_NAME " --cli <category> <command> [argument]\n");
        ConColor(CC_BLUE);
        return;
    }
    for (;;) {
        ConColor(CC_CYAN);
        printf("\n  Enter number to run it, 0 to ask another question, q to quit: ");
        fflush(stdout);
        if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return;
        size_t l = strlen(g_argBuf);
        while (l > 0 && (g_argBuf[l - 1] == '\n' || g_argBuf[l - 1] == '\r')) g_argBuf[--l] = 0;
        if (g_argBuf[0] == 'q' || g_argBuf[0] == 'Q') { ConColor(CC_BLUE); return; }
        int num = atoi(g_argBuf);
        if (num == 0) { ConColor(CC_BLUE); return; }
        if (num < 1 || num > shown) { ConColor(CC_RED); printf("Invalid choice.\n"); ConColor(CC_BLUE); continue; }
        int ci = bestCat[num - 1], ii = bestItem[num - 1];
        const Command *c = &g_Categories[ci].items[ii];
        const char *arg = NULL;
        if (c->needsArg) {
            ConColor(CC_YELLOW);
            printf("  Argument for '%s': ", c->name);
            fflush(stdout);
            if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return;
            size_t l2 = strlen(g_argBuf);
            while (l2 > 0 && (g_argBuf[l2 - 1] == '\n' || g_argBuf[l2 - 1] == '\r')) g_argBuf[--l2] = 0;
            arg = g_argBuf;
        }
        ConColor(CC_GREEN);
        printf("  Executing: %s -> %s\n", g_Categories[ci].name, c->name);
        ConColor(CC_BLUE);
        c->fn(arg);
        ConColor(CC_GREEN);
        printf("  Done.\n");
        ConColor(CC_BLUE);
        return;
    }
}

const char *AskArgCli(const char *prompt) {
    printf("%s: ", prompt);
    fflush(stdout);
    if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return NULL;
    size_t l = strlen(g_argBuf);
    while (l > 0 && (g_argBuf[l - 1] == '\n' || g_argBuf[l - 1] == '\r'))
        g_argBuf[--l] = 0;
    return g_argBuf;
}

void RunCliTokens(int n, char *tokens[]) {
    if (n <= 0) return;
    if (stricmp(tokens[0], "help") == 0) { PrintHelp(); return; }
    if (stricmp(tokens[0], "list") == 0) {
        if (n >= 2) PrintCategoryList(tokens[1]);
        else PrintAllCategories();
        return;
    }
    if (stricmp(tokens[0], "install") == 0) { DoInstall(); return; }
    if (stricmp(tokens[0], "uninstall") == 0) { DoUninstall(); return; }
    if (stricmp(tokens[0], "open-gui") == 0) { OnUtilGui(NULL); return; }
    if (stricmp(tokens[0], "about") == 0) { OnUtilAbout(NULL); return; }
    if (stricmp(tokens[0], "explain") == 0) {
        if (n >= 2) {
            g_argBuf[0] = 0;
            for (int k = 1; k < n; k++) {
                if (k > 1) strncat(g_argBuf, " ", sizeof g_argBuf - strlen(g_argBuf) - 1);
                strncat(g_argBuf, tokens[k], sizeof g_argBuf - strlen(g_argBuf) - 1);
            }
            ReportClear();
            OnExplain(g_argBuf);
        } else {
            printf("Usage: explain <category> <command>\n");
            printf("Example: explain settings display\n");
            return;
        }
        if (g_consoleMode) printf("\n%s\n", g_report);
        else ShowReport();
        return;
    }
    if (stricmp(tokens[0], "tools") == 0) { OnToolsMenu(NULL); return; }
    if (stricmp(tokens[0], "terminal") == 0) { OnUtilTerminal(NULL); return; }
    if (stricmp(tokens[0], "ask") == 0) {
        if (n >= 2) {
            g_argBuf[0] = 0;
            for (int k = 1; k < n; k++) {
                if (k > 1) strncat(g_argBuf, " ", sizeof g_argBuf - strlen(g_argBuf) - 1);
                strncat(g_argBuf, tokens[k], sizeof g_argBuf - strlen(g_argBuf) - 1);
            }
            AskAssistant(g_argBuf, g_cliInteractive);
        } else if (g_cliInteractive) {
            printf("Describe what you want to do in English: ");
            fflush(stdout);
            if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return;
            size_t l = strlen(g_argBuf);
            while (l > 0 && (g_argBuf[l - 1] == '\n' || g_argBuf[l - 1] == '\r')) g_argBuf[--l] = 0;
            AskAssistant(g_argBuf, 1);
        } else {
            printf("Usage: ask <question>\n");
            printf("Example: ask change wallpaper\n");
        }
        return;
    }
    if (stricmp(tokens[0], "exit") == 0 || stricmp(tokens[0], "quit") == 0) return;

    if (n >= 2) {
        /* Multi-word category names ("Windows Settings <command>"): match the
           longest exact category-name prefix of the typed tokens first. */
        if (n >= 3) {
            char catname[160] = "";
            for (int k = 0; k + 1 < n; k++) {
                if (k) strncat(catname, " ", sizeof catname - strlen(catname) - 1);
                strncat(catname, tokens[k], sizeof catname - strlen(catname) - 1);
                const Category *mc = NULL;
                for (int i = 0; i < g_categoryCount; i++)
                    if (stricmp(catname, g_Categories[i].name) == 0) { mc = &g_Categories[i]; break; }
                if (!mc) continue;
                const Command *cmd = NULL;
                for (int j = 0; j < mc->count; j++)
                    if (stricmp(tokens[k + 1], mc->items[j].name) == 0) { cmd = &mc->items[j]; break; }
                if (!cmd) {
                    printf("Unknown command '%s' in category '%s'. Use 'list %s'.\n",
                           tokens[k + 1], mc->name, mc->name);
                    return;
                }
                const char *arg = NULL;
                if (cmd->needsArg) {
                    if (n > k + 2) {
                        g_argBuf[0] = 0;
                        for (int t = k + 2; t < n; t++) {
                            if (t > k + 2) strncat(g_argBuf, " ", sizeof g_argBuf - strlen(g_argBuf) - 1);
                            strncat(g_argBuf, tokens[t], sizeof g_argBuf - strlen(g_argBuf) - 1);
                        }
                        arg = g_argBuf;
                    } else if (g_cliInteractive) {
                        arg = AskArgCli("Enter argument");
                    } else {
                        printf("Command '%s' needs an argument.\n", cmd->name);
                        printf("Usage: %s %s <argument>\n", mc->name, cmd->name);
                        return;
                    }
                }
                if (g_consoleMode)
                    printf("Executing: %s -> %s%s\n", mc->name, cmd->name, arg ? " (with argument)" : "");
                cmd->fn(arg);
                if (g_consoleMode) printf("Done.\n");
                return;
            }
        }
        const Category *c = FindCategory(tokens[0]);
        if (c) {
            const Command *cmd = NULL;
            for (int j = 0; j < c->count; j++)
                if (stricmp(tokens[1], c->items[j].name) == 0) { cmd = &c->items[j]; break; }
            if (cmd) {
                const char *arg = NULL;
                if (cmd->needsArg) {
                    if (n >= 3) {
                        g_argBuf[0] = 0;
                        for (int k = 2; k < n; k++) {
                            if (k > 2) strncat(g_argBuf, " ", sizeof g_argBuf - strlen(g_argBuf) - 1);
                            strncat(g_argBuf, tokens[k], sizeof g_argBuf - strlen(g_argBuf) - 1);
                        }
                        arg = g_argBuf;
                    } else if (g_cliInteractive) {
                        arg = AskArgCli("Enter argument");
                    } else {
                        printf("Command '%s' needs an argument.\n", cmd->name);
                        printf("Usage: %s %s <argument>\n", tokens[0], cmd->name);
                        return;
                    }
                }
                if (g_consoleMode)
                    printf("Executing: %s -> %s%s\n", c->name, cmd->name, arg ? " (with argument)" : "");
                cmd->fn(arg);
                if (g_consoleMode) printf("Done.\n");
                return;
            }
            printf("Unknown command '%s' in category '%s'. Use 'list %s'.\n",
                   tokens[1], tokens[0], tokens[0]);
            return;
        }
    }
    if (n == 1 && FindCategory(tokens[0])) { PrintCategoryList(tokens[0]); return; }
    printf("Unknown input. Type 'help' for usage.\n");
}

void RunCli(int argc, char *argv[]) {
    EnsureConsole();
    PrintWinBanner();
    ConColor(CC_CYAN);
    printf("\n  WINDOWS CONTROL CENTER - CLI MODE\n");
    ConColor(CC_DGRAY);
    printf("  Type: help | list [category] | <category> <command> [argument] | explain <category> <command> | terminal | tools | ask <question> | install | uninstall | exit\n");
    ConColor(CC_BLUE);

    if (argc >= 1) {
        RunCliTokens(argc, argv);
        return;
    }

    g_cliInteractive = 1;
    char line[512];
    for (;;) {
        ConColor(CC_CYAN);
        printf("\n  wc> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        char *tokens[16];
        int n = tokenizeLine(line, tokens, 16);
        if (n == 0) continue;
        if (stricmp(tokens[0], "exit") == 0 || stricmp(tokens[0], "quit") == 0) break;
        RunCliTokens(n, tokens);
    }
    ConColor(CC_CYAN);
    printf("  Goodbye.\n");
    ConColor(CC_BLUE);
}

void RunTui(void) {
    EnsureConsole();
    PrintWinBanner();
    char line[128];
    ConColor(CC_CYAN);
    printf("\n  WINDOWS CONTROL CENTER\n");
    ConColor(CC_DGRAY);
    printf("  An all-in-one launcher for Windows features and settings.\n");
    printf("  Type a number to choose, 0 to go back or exit.\n");
    ConColor(CC_BLUE);

    for (;;) {
        ConColor(CC_CYAN);
        printf("\n  ==================== MAIN MENU ====================\n");
        ConColor(CC_GREEN);
        printf("    0) Exit\n");
        for (int i = 0; i < g_categoryCount; i++) {
            ConColor(CC_CYAN);
            printf("%4d) ", i + 1);
            ConColor(CC_WHITE);
            printf("%s\n", g_Categories[i].name);
        }
        ConColor(CC_YELLOW);
        printf("%4d) ", g_categoryCount + 1);
        ConColor(CC_WHITE);
        printf("Ask the Assistant (type a question in English)\n");
        ConColor(CC_MAG);
        printf("%4d) ", g_categoryCount + 2);
        ConColor(CC_WHITE);
        printf("Terminal (type commands, e.g. ipconfig, settings display)\n");
        ConColor(CC_CYAN);
        printf("%4d) ", g_categoryCount + 3);
        ConColor(CC_WHITE);
        printf("System Tools (monitor, uninstall, search, wifi, processes)\n");
        ConColor(CC_CYAN);
        printf("  ===================================================\n");
        ConColor(CC_GREEN);
        printf("  Enter category number: ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) return;
        int cat = atoi(line);
        if (cat == 0) break;
        if (cat == g_categoryCount + 3) {
            OnToolsMenu(NULL);
            continue;
        }
        if (cat == g_categoryCount + 2) {
            ConColor(CC_WHITE);
            OnUtilTerminal(NULL);
            continue;
        }
        if (cat == g_categoryCount + 1) {
            for (;;) {
                ConColor(CC_YELLOW);
                printf("\n  Describe what you want to do in English (q to return to menu): ");
                fflush(stdout);
                if (!fgets(line, sizeof line, stdin)) return;
                size_t l = strlen(line);
                while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
                if (l == 0 || line[0] == 'q' || line[0] == 'Q') break;
                AskAssistant(line, 1);
            }
            continue;
        }
        if (cat < 1 || cat > g_categoryCount) {
            ConColor(CC_RED);
            printf("  Invalid choice.\n");
            ConColor(CC_BLUE);
            continue;
        }
        int idx = cat - 1;
        for (;;) {
            ConColor(CC_CYAN);
            printf("\n  ------------------- %s -------------------\n", g_Categories[idx].name);
            ConColor(CC_GREEN);
            printf("    0) Back to main menu\n");
            for (int j = 0; j < g_Categories[idx].count; j++) {
                ConColor(CC_CYAN);
                printf("%4d) ", j + 1);
                ConColor(CC_WHITE);
                printf("%s", g_Categories[idx].items[j].name);
                ConColor(CC_GRAY);
                printf(" -> %s", g_Categories[idx].items[j].desc);
                if (g_Categories[idx].items[j].needsArg) {
                    ConColor(CC_YELLOW);
                    printf("  [ARG]");
                }
                printf("\n");
            }
            ConColor(CC_CYAN);
            printf("  -----------------------------------------------\n");
            ConColor(CC_GREEN);
            printf("  Enter command number: ");
            fflush(stdout);
            if (!fgets(line, sizeof line, stdin)) return;
            int cmd = atoi(line);
            if (cmd == 0) break;
            if (cmd < 1 || cmd > g_Categories[idx].count) {
                ConColor(CC_RED);
                printf("  Invalid choice.\n");
                ConColor(CC_BLUE);
                continue;
            }
            const char *arg = NULL;
            if (g_Categories[idx].items[cmd - 1].needsArg) {
                ConColor(CC_YELLOW);
                printf("  Argument: ");
                fflush(stdout);
                if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return;
                size_t l = strlen(g_argBuf);
                while (l > 0 && (g_argBuf[l - 1] == '\n' || g_argBuf[l - 1] == '\r'))
                    g_argBuf[--l] = 0;
                arg = g_argBuf;
            }
            ConColor(CC_GREEN);
            printf("  Executing: %s -> %s\n", g_Categories[idx].name, g_Categories[idx].items[cmd - 1].name);
            ConColor(CC_BLUE);
            g_Categories[idx].items[cmd - 1].fn(arg);
            ConColor(CC_GREEN);
            printf("\n  Done. Press Enter to continue...");
            fflush(stdout);
            while (fgets(line, sizeof line, stdin)) {
                if (strlen(line) <= 1) break;
            }
        }
    }
    ConColor(CC_CYAN);
    printf("  Goodbye.\n");
    ConColor(CC_BLUE);
}

static HWND g_hMain;
static HWND g_hLbCat, g_hLbCmd, g_hStatus;
static HWND g_hDesc, g_hArgEdit;
static HWND g_hFloat;
static int g_bOpenMain = 0;
static int g_bFloatStandalone = 0;
static HFONT g_hFont;
static char g_argInput[ARG_MAX];
static char g_argPrompt[512];
static int g_argOK;
static char g_rptText[REPORT_MAX];

LRESULT CALLBACK ArgDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HWND lbl = CreateWindowA("STATIC", g_argPrompt, WS_CHILD | WS_VISIBLE,
                                     14, 10, 420, 42, hWnd, NULL, g_hInst, NULL);
            SendMessageA(lbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            HWND ed = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                    14, 56, 420, 26, hWnd, (HMENU)IDC_ARG_EDIT, g_hInst, NULL);
            SendMessageA(ed, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SetFocus(ed);
            HWND ok = CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    110, 96, 110, 30, hWnd, (HMENU)IDC_ARG_OK, g_hInst, NULL);
            SendMessageA(ok, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            HWND cc = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                                    230, 96, 110, 30, hWnd, (HMENU)IDC_ARG_CANCEL, g_hInst, NULL);
            SendMessageA(cc, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == IDC_ARG_OK) {
                    GetWindowTextA(GetDlgItem(hWnd, IDC_ARG_EDIT), g_argInput, ARG_MAX);
                    g_argOK = 1;
                    DestroyWindow(hWnd);
                } else if (LOWORD(wParam) == IDC_ARG_CANCEL) {
                    g_argOK = 0;
                    DestroyWindow(hWnd);
                }
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int PromptArg(const char *prompt) {
    strncpy(g_argPrompt, prompt, sizeof g_argPrompt - 1);
    g_argPrompt[sizeof g_argPrompt - 1] = 0;
    g_argOK = 0;
    g_argInput[0] = 0;
    WNDCLASSA ac;
    memset(&ac, 0, sizeof ac);
    ac.lpfnWndProc = ArgDlgProc;
    ac.hInstance = g_hInst;
    ac.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    ac.lpszClassName = "WcArgClass";
    RegisterClassA(&ac);
    HWND h = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcArgClass", APP_NAME " - Argument",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, 460, 170, NULL, NULL, g_hInst, NULL);
    if (!h) return 0;
    ShowWindow(h, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return g_argOK;
}

LRESULT CALLBACK ReportDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HWND ed = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", NULL,
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                      ES_READONLY | ES_AUTOVSCROLL,
                                      10, 10, 640, 400, hWnd, (HMENU)IDC_RPT_EDIT, g_hInst, NULL);
            SendMessageA(ed, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SetWindowTextA(ed, g_rptText);
            HWND ok = CreateWindowA("BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    280, 422, 100, 32, hWnd, (HMENU)IDC_RPT_OK, g_hInst, NULL);
            SendMessageA(ok, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_RPT_OK)
                DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowReportDialog(void) {
    strncpy(g_rptText, g_report, sizeof g_rptText - 1);
    g_rptText[sizeof g_rptText - 1] = 0;
    WNDCLASSA rc;
    memset(&rc, 0, sizeof rc);
    rc.lpfnWndProc = ReportDlgProc;
    rc.hInstance = g_hInst;
    rc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    rc.lpszClassName = "WcReportClass";
    RegisterClassA(&rc);
    HWND h = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcReportClass", APP_NAME " - Report",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT, 680, 500, NULL, NULL, g_hInst, NULL);
    if (!h) return;
    ShowWindow(h, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

typedef struct { const char *label; void (*fn)(const char *); } FloatItem;
static const FloatItem g_floatItems[] = {
    {"Show Desktop", OnShowDesktop},
    {"Task Switcher", OnTaskSwitcher},
    {"Lock Screen", OnLockScreen},
    {"Sleep", OnSleepNow},
    {"Screenshot", OnScreenshot},
    {"Mute", OnAudMute},
    {"Vol -", OnAudVolDown},
    {"Vol +", OnAudVolUp},
    {"Clipboard", OnClipboardClear},
    {"Task Manager", OnSysTaskmgr},
    {"Settings", OnStHome},
    {"This PC", OnAppThisPC},
    {"Calculator", OnAppCalc},
    {"Notepad", OnAppNotepad},
    {"Command", OnDevCmd},
    {"Control Panel", OnCplAll},
};
#define FLOAT_BTN_BASE 300
#define FLOAT_BTN_MAIN 316
#define FLOAT_BTN_CLOSE 317
#define BM_SEARCH 501
#define BM_LIST 502
#define BM_RUN 503

static HWND g_hBallMenu;
static int g_ballDrag = 0;
static int g_ballMoved = 0;
static POINT g_ballDown;

static void BallToggleMenu(HWND ball);
static void BallMenuClose(void);

static void BallRunCommand(int catIdx, int itemIdx) {
    if (catIdx < 0 || catIdx >= g_categoryCount) return;
    const Command *cmd = &g_Categories[catIdx].items[itemIdx];
    if (cmd->needsArg) {
        char p[512];
        sprintf(p, "%s\n\nEnter a value, then click OK.", cmd->desc);
        if (!PromptArg(p)) return;
        cmd->fn(g_argInput);
    } else {
        cmd->fn(NULL);
    }
}

static void BallMenuRefresh(HWND menu) {
    char needle[128];
    GetWindowTextA(GetDlgItem(menu, BM_SEARCH), needle, sizeof needle);
    HWND lb = GetDlgItem(menu, BM_LIST);
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    TrimSpaces(needle);
    if (needle[0] == 0) {
        SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"Type above to search all commands...");
        return;
    }
    int shown = 0;
    char text[256];
    for (int i = 0; i < g_categoryCount && shown < 200; i++) {
        for (int j = 0; j < g_Categories[i].count && shown < 200; j++) {
            const Command *c = &g_Categories[i].items[j];
            if (stristr(c->name, needle) || stristr(c->desc, needle)) {
                sprintf(text, "%s / %s", g_Categories[i].name, c->name);
                int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)text);
                SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)(i * 4096 + j));
                shown++;
            }
        }
    }
    if (shown == 0) SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"No matches.");
}

static void BallMenuRunSelected(HWND menu) {
    HWND lb = GetDlgItem(menu, BM_LIST);
    int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    int data = (int)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
    int cat = data / 4096, item = data % 4096;
    if (cat < 0 || cat >= g_categoryCount) return;
    BallRunCommand(cat, item);
}

LRESULT CALLBACK BallMenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT f = CreateFontA(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");
            HWND h;
            h = CreateWindowA("STATIC", APP_NAME " - Ball Menu (search runs anything)",
                              WS_CHILD | WS_VISIBLE, 8, 6, 344, 18, hWnd, NULL, g_hInst, NULL);
            SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);
            h = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                              8, 26, 344, 24, hWnd, (HMENU)BM_SEARCH, g_hInst, NULL);
            SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);
            h = CreateWindowA("LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                              8, 54, 344, 152, hWnd, (HMENU)BM_LIST, g_hInst, NULL);
            SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);
            h = CreateWindowA("BUTTON", "Run selected (or double-click)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              8, 210, 344, 26, hWnd, (HMENU)BM_RUN, g_hInst, NULL);
            SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE);
            for (int i = 0; i < 16; i++) {
                int col = i % 3, row = i / 3;
                HWND b = CreateWindowA("BUTTON", g_floatItems[i].label,
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       8 + col * 116, 240 + row * 34, 112, 30, hWnd,
                                       (HMENU)(INT_PTR)(FLOAT_BTN_BASE + i), g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)f, TRUE);
            }
            HWND bm = CreateWindowA("BUTTON", "Open Main Panel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    8, 444, 240, 28, hWnd, (HMENU)FLOAT_BTN_MAIN, g_hInst, NULL);
            SendMessageA(bm, WM_SETFONT, (WPARAM)f, TRUE);
            HWND bc = CreateWindowA("BUTTON", "Close Widget", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    252, 444, 100, 28, hWnd, (HMENU)FLOAT_BTN_CLOSE, g_hInst, NULL);
            SendMessageA(bc, WM_SETFONT, (WPARAM)f, TRUE);
            BallMenuRefresh(hWnd);
            SetFocus(GetDlgItem(hWnd, BM_SEARCH));
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                int id = LOWORD(wParam);
                if (id >= FLOAT_BTN_BASE && id < FLOAT_BTN_BASE + 16) {
                    g_floatItems[id - FLOAT_BTN_BASE].fn(NULL);
                } else if (id == FLOAT_BTN_MAIN) {
                    g_bOpenMain = 1;
                    BallMenuClose();
                    if (g_hFloat) DestroyWindow(g_hFloat);
                } else if (id == FLOAT_BTN_CLOSE) {
                    BallMenuClose();
                    if (g_hFloat) DestroyWindow(g_hFloat);
                } else if (id == BM_RUN) {
                    BallMenuRunSelected(hWnd);
                }
            } else if (LOWORD(wParam) == BM_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                BallMenuRefresh(hWnd);
            } else if (LOWORD(wParam) == BM_LIST && HIWORD(wParam) == LBN_DBLCLK) {
                BallMenuRunSelected(hWnd);
            }
            break;
        case WM_DESTROY:
            g_hBallMenu = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

static void BallMenuClose(void) {
    if (g_hBallMenu) DestroyWindow(g_hBallMenu);
}

static void BallToggleMenu(HWND ball) {
    if (g_hBallMenu) { BallMenuClose(); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = BallMenuWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WindowsControlBallMenuClass";
    RegisterClassA(&wc);
    RECT br;
    GetWindowRect(ball, &br);
    int x = br.left - 170;
    int y = br.top - 500;
    if (x < 0) x = br.right + 8;
    if (y < 0) y = br.bottom + 8;
    RECT wa;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    if (x + 360 > wa.right) x = wa.right - 360;
    if (y + 480 > wa.bottom) y = wa.bottom - 480;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    g_hBallMenu = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                  "WindowsControlBallMenuClass",
                                  "Ball Menu",
                                  WS_POPUP | WS_BORDER,
                                  x, y, 360, 480, NULL, NULL, g_hInst, NULL);
    if (!g_hBallMenu) return;
    ShowWindow(g_hBallMenu, SW_SHOW);
    SetForegroundWindow(g_hBallMenu);
    SetFocus(GetDlgItem(g_hBallMenu, BM_SEARCH));
}

LRESULT CALLBACK BallWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            HBRUSH fill = CreateSolidBrush(RGB(16, 40, 90));
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(150, 195, 255));
            HBRUSH oldb = (HBRUSH)SelectObject(hdc, fill);
            HPEN oldp = (HPEN)SelectObject(hdc, pen);
            Ellipse(hdc, 3, 3, 53, 53);
            SelectObject(hdc, oldb);
            SelectObject(hdc, oldp);
            DeleteObject(fill);
            DeleteObject(pen);

            static const COLORREF logoCol[4] = {
                RGB(0, 161, 241), RGB(124, 187, 0), RGB(247, 99, 12), RGB(255, 187, 0)
            };
            const int rects[4][4] = {
                { 11, 11, 26, 26 }, { 29, 11, 44, 26 },
                { 11, 29, 26, 44 }, { 29, 29, 44, 44 }
            };
            for (int i = 0; i < 4; i++) {
                HBRUSH b = CreateSolidBrush(logoCol[i]);
                HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
                HPEN np = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HPEN op2 = (HPEN)SelectObject(hdc, np);
                RoundRect(hdc, rects[i][0], rects[i][1], rects[i][2], rects[i][3], 4, 4);
                SelectObject(hdc, ob);
                SelectObject(hdc, op2);
                DeleteObject(b);
                DeleteObject(np);
            }
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_NCHITTEST: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT r;
            GetWindowRect(hWnd, &r);
            int cx = pt.x - (r.left + 28), cy = pt.y - (r.top + 28);
            if (cx * cx + cy * cy > 27 * 27) return HTTRANSPARENT;
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN:
            g_ballDrag = 1;
            g_ballMoved = 0;
            GetCursorPos(&g_ballDown);
            SetCapture(hWnd);
            break;
        case WM_MOUSEMOVE:
            if (g_ballDrag) {
                POINT cur;
                GetCursorPos(&cur);
                if (abs(cur.x - g_ballDown.x) + abs(cur.y - g_ballDown.y) > 4) g_ballMoved = 1;
                RECT r;
                GetWindowRect(hWnd, &r);
                SetWindowPos(hWnd, NULL, r.left + cur.x - g_ballDown.x, r.top + cur.y - g_ballDown.y,
                             0, 0, SWP_NOSIZE | SWP_NOZORDER);
                g_ballDown = cur;
            }
            break;
        case WM_LBUTTONUP:
            if (g_ballDrag) {
                ReleaseCapture();
                g_ballDrag = 0;
                if (!g_ballMoved) BallToggleMenu(hWnd);
            }
            break;
        case WM_RBUTTONUP:
            BallMenuClose();
            if (g_hFloat) DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            BallMenuClose();
            g_hFloat = NULL;
            if (g_bFloatStandalone) PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void RunFloat(void) {
    if (g_hFloat) {
        ShowWindow(g_hFloat, SW_SHOW);
        SetForegroundWindow(g_hFloat);
        return;
    }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = BallWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "WindowsControlBallClass";
    RegisterClassA(&wc);
    RECT wa;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    g_hFloat = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                               "WindowsControlBallClass",
                               APP_NAME " - Ball Widget",
                               WS_POPUP,
                               wa.right - 80, wa.bottom - 90, 56, 56,
                               NULL, NULL, g_hInst, NULL);
    if (!g_hFloat) return;
    ShowWindow(g_hFloat, SW_SHOW);
}

void RunGui(void) {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = GuiWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(14, 32, 66));
    wc.lpszClassName = "WindowsControlCenterClass";
    RegisterClassA(&wc);

    g_hMain = CreateWindowExA(WS_EX_DLGMODALFRAME, "WindowsControlCenterClass",
                              APP_NAME " - All-in-one Windows Control",
WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               1180, 740, NULL, NULL, g_hInst, NULL);
    if (!g_hMain) { Notify("Failed to create the main window."); return; }
    ShowWindow(g_hMain, SW_SHOW);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/* ==========================================================================
   MODERN DARK UI + POWER TOOLS
   Dark theme, pure GDI. Adds: modern main panel (header/search/sidebar/
   cards/status bar), live system monitor, app uninstaller, file search,
   Wi-Fi manager, process manager, and CLI text tools.
   ========================================================================== */

#include <psapi.h>
#include <ctype.h>
#include <conio.h>
#pragma comment(lib, "psapi.lib")

static void MonitorConsole(void);
static void UninsConsole(void);
static void FindConsole(void);
static void WifiConsole(void);
static void ProcConsole(void);
static void LaunchUninstaller(int idx);

/* ---- palette ---- */
#define UI_BG        RGB(15, 17, 24)
#define UI_PANEL     RGB(22, 25, 33)
#define UI_CARD      RGB(30, 34, 45)
#define UI_CARD_HOV  RGB(40, 45, 60)
#define UI_ACCENT    RGB(70, 145, 255)
#define UI_ACCENT2   RGB(0, 190, 220)
#define UI_ACCENT_D  RGB(22, 56, 110)
#define UI_TEXT      RGB(232, 236, 244)
#define UI_DIM       RGB(138, 146, 163)
#define UI_BORDER    RGB(46, 51, 66)
#define UI_GREEN     RGB(92, 200, 122)
#define UI_RED       RGB(235, 90, 90)
#define UI_YELLOW    RGB(240, 190, 70)
#define UI_ORANGE    RGB(255, 150, 60)
#define UI_PURPLE    RGB(170, 120, 255)

static const COLORREF g_avatarCol[12] = {
    RGB(70, 145, 255), RGB(92, 200, 122), RGB(255, 150, 60), RGB(170, 120, 255),
    RGB(0, 190, 220), RGB(240, 190, 70), RGB(235, 90, 90), RGB(120, 200, 160),
    RGB(255, 130, 150), RGB(140, 165, 255), RGB(190, 150, 90), RGB(110, 200, 240),
};

/* ---- fonts (created once) ---- */
static HFONT g_fTiny, g_fSmall, g_fNormal, g_fTitle, g_fBig, g_fMono;
static int g_fontsMade = 0;

static HFONT UiMakeFont(int px, int weight, const char *face) {
    return CreateFontA(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH, face);
}

static void UiEnsureFonts(void) {
    if (g_fontsMade) return;
    g_fontsMade = 1;
    g_fTiny = UiMakeFont(10, FW_NORMAL, "Segoe UI");
    g_fSmall = UiMakeFont(11, FW_NORMAL, "Segoe UI");
    g_fNormal = UiMakeFont(12, FW_NORMAL, "Segoe UI");
    g_fTitle = UiMakeFont(16, FW_BOLD, "Segoe UI");
    g_fBig = UiMakeFont(22, FW_BOLD, "Segoe UI");
    g_fMono = UiMakeFont(11, FW_NORMAL, "Consolas");
}

/* ---- draw helpers ---- */
static void UiFill(HDC hdc, int x, int y, int w, int h, COLORREF c) {
    RECT r = { x, y, x + w, y + h };
    HBRUSH b = CreateSolidBrush(c);
    FillRect(hdc, &r, b);
    DeleteObject(b);
}

static void UiRound(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border) {
    HBRUSH fb = CreateSolidBrush(fill);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, fb);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HPEN op = (HPEN)SelectObject(hdc, p);
    RoundRect(hdc, x, y, x + w, y + h, r, r);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(fb);
    DeleteObject(p);
}

static void UiGradV(HDC hdc, int x, int y, int w, int h, COLORREF c1, COLORREF c2) {
    if (h <= 0) return;
    int r1 = GetRValue(c1), g1 = GetGValue(c1), b1 = GetBValue(c1);
    int r2 = GetRValue(c2), g2 = GetGValue(c2), b2 = GetBValue(c2);
    for (int i = 0; i < h; i++) {
        int t = i * 100 / h;
        UiFill(hdc, x, y + i, w, 1,
               RGB(r1 + (r2 - r1) * t / 100, g1 + (g2 - g1) * t / 100, b1 + (b2 - b1) * t / 100));
    }
}

static void UiText(HDC hdc, const char *s, int x, int y, int w, int h,
                   COLORREF c, HFONT f, UINT flags) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, c);
    HFONT of = (HFONT)SelectObject(hdc, f);
    RECT r = { x, y, x + w, y + h };
    DrawTextA(hdc, s, -1, &r, flags);
    SelectObject(hdc, of);
}

static void UiClipText(char *dst, size_t dstSize, const char *src, int maxLen) {
    if (dstSize == 0) return;
    size_t n = strlen(src);
    if (n > (size_t)maxLen) n = (size_t)maxLen;
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* ---- live stats ---- */
static double g_cpuPct = 0, g_ramPct = 0, g_netDown = 0, g_netUp = 0;
static double g_diskUsedPct = 0;
static ULONGLONG g_lastIdle = 0, g_lastKernel = 0, g_lastUser = 0;
static DWORD g_lastTick = 0;
static ULONGLONG g_lastNetIn = 0, g_lastNetOut = 0;

static void UiSampleStats(void) {
    FILETIME idle, kern, user;
    if (GetSystemTimes(&idle, &kern, &user)) {
        ULONGLONG i = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
        ULONGLONG k = ((ULONGLONG)kern.dwHighDateTime << 32) | kern.dwLowDateTime;
        ULONGLONG u = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;
        ULONGLONG kd = k - g_lastKernel, ud = u - g_lastUser, id = i - g_lastIdle;
        if (g_lastKernel && (kd + ud) > 0)
            g_cpuPct = 100.0 - (double)id * 100.0 / (double)(kd + ud);
        g_lastIdle = i; g_lastKernel = k; g_lastUser = u;
    }
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) {
        g_ramPct = (double)ms.dwMemoryLoad;
    }
    static BYTE nb[128 * 1024];
    MIB_IFTABLE *t = (MIB_IFTABLE *)nb;
    DWORD sz = sizeof nb;
    ULONGLONG in = 0, out = 0;
    if (GetIfTable(t, &sz, FALSE) == NO_ERROR) {
        for (DWORD i = 0; i < t->dwNumEntries; i++) {
            if (t->table[i].dwOperStatus == 1) {
                in += t->table[i].dwInOctets;
                out += t->table[i].dwOutOctets;
            }
        }
    }
    ULONGLONG now = GetTickCount64();
    double secs = (double)(now - g_lastTick) / 1000.0;
    if (g_lastTick && secs > 0.1) {
        g_netDown = (double)(in - g_lastNetIn) / secs;
        g_netUp = (double)(out - g_lastNetOut) / secs;
    }
    g_lastTick = (DWORD)now;
    g_lastNetIn = in;
    g_lastNetOut = out;
    ULARGE_INTEGER fa, ta, fp;
    if (GetDiskFreeSpaceExA("C:\\", &fa, &ta, &fp) && ta.QuadPart > 0) {
        g_diskUsedPct = 100.0 - (double)fa.QuadPart * 100.0 / (double)ta.QuadPart;
    }
}

/* ==========================================================================
   MODERN MAIN PANEL
   ========================================================================== */
static int g_uiCat = 0;
static int g_uiScroll = 0;
static int g_uiSideScroll = 0;
static int g_uiHover = -1;
static int g_uiHoverSide = -1;
static int g_uiHoverHead = -1;
static int g_uiCards = 0;
static int g_uiCardCat[600], g_uiCardItem[600];
static char g_uiSearch[128];
static int g_uiSearching = 0;
static HWND g_hSearchEdit;
static UINT_PTR g_uiTimerId = 0;

static const char *g_uiSearchHint = "Search all 1200+ commands...";
static int g_totalCommands = 0;

static void UiBuildView(void) {
    g_uiCards = 0;
    TrimSpaces(g_uiSearch);
    g_uiSearching = g_uiSearch[0] != 0;
    g_totalCommands = 0;
    for (int i = 0; i < g_categoryCount; i++) g_totalCommands += g_Categories[i].count;
    if (g_uiSearching) {
        for (int i = 0; i < g_categoryCount && g_uiCards < 600; i++) {
            for (int j = 0; j < g_Categories[i].count && g_uiCards < 600; j++) {
                const Command *c = &g_Categories[i].items[j];
                if (stristr(c->name, g_uiSearch) || stristr(c->desc, g_uiSearch)) {
                    g_uiCardCat[g_uiCards] = i;
                    g_uiCardItem[g_uiCards] = j;
                    g_uiCards++;
                }
            }
        }
    } else {
        if (g_uiCat >= g_categoryCount) g_uiCat = 0;
        int n = g_Categories[g_uiCat].count;
        if (n > 600) n = 600;
        for (int j = 0; j < n; j++) {
            g_uiCardCat[g_uiCards] = g_uiCat;
            g_uiCardItem[g_uiCards] = j;
            g_uiCards++;
        }
    }
    if (g_uiScroll > g_uiCards) g_uiScroll = 0;
    if (g_uiScroll < 0) g_uiScroll = 0;
}

static int UiCardPos(int idx, int *x, int *y, int *w, int *h, RECT *client) {
    int cols = 3;
    int cw = (client->right - 232 - 30) / cols;
    if (cw < 220) { cols = 2; cw = (client->right - 232 - 24) / cols; }
    int ch = 84;
    int col = idx % cols, row = idx / cols - g_uiScroll;
    int px = 244 + col * (cw + 10);
    int py = 118 + row * (ch + 10);
    if (py + ch < 108 || py > client->bottom - 28) return 0;
    *x = px; *y = py; *w = cw; *h = ch;
    return 1;
}

static void GuiRefreshCommands(void) {
    UiBuildView();
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

static void GuiRefreshStatus(void) {
    if (g_hMain) InvalidateRect(g_hMain, NULL, FALSE);
}

static void GuiRunSelected(void) {
    if (g_uiCards <= 0) return;
    int ci = g_uiCardCat[0], ii = g_uiCardItem[0];
    const Command *cmd = &g_Categories[ci].items[ii];
    if (cmd->needsArg) {
        char p[512];
        sprintf(p, "%s\n\nEnter a value, then click OK.", cmd->desc);
        if (!PromptArg(p)) return;
        cmd->fn(g_argInput);
    } else {
        cmd->fn(NULL);
    }
}

static void GuiExplainSelected(void) {
    if (g_uiCards <= 0) return;
    int ci = g_uiCardCat[0], ii = g_uiCardItem[0];
    const Command *cmd = &g_Categories[ci].items[ii];
    const char *d = CmdDetail(g_Categories[ci].name, cmd->name);
    ReportClear();
    ReportAdd("Command : %s", cmd->name);
    ReportAdd("Category: %s", g_Categories[ci].name);
    ReportAdd("Purpose : %s", cmd->desc);
    ReportAdd("Detail  : %s", d ? d : "(no extra detail in the knowledge base)");
    ShowReport();
}

static void UiRunCard(int cardIdx) {
    if (cardIdx < 0 || cardIdx >= g_uiCards) return;
    int ci = g_uiCardCat[cardIdx], ii = g_uiCardItem[cardIdx];
    const Command *cmd = &g_Categories[ci].items[ii];
    if (cmd->needsArg) {
        char p[512];
        sprintf(p, "%s\n\nEnter a value, then click OK.", cmd->desc);
        if (!PromptArg(p)) return;
        cmd->fn(g_argInput);
    } else {
        cmd->fn(NULL);
    }
}

static void UiExplainCard(int cardIdx) {
    if (cardIdx < 0 || cardIdx >= g_uiCards) return;
    int ci = g_uiCardCat[cardIdx], ii = g_uiCardItem[cardIdx];
    const Command *cmd = &g_Categories[ci].items[ii];
    const char *d = CmdDetail(g_Categories[ci].name, cmd->name);
    ReportClear();
    ReportAdd("Command : %s", cmd->name);
    ReportAdd("Category: %s", g_Categories[ci].name);
    ReportAdd("Purpose : %s", cmd->desc);
    ReportAdd("Detail  : %s", d ? d : "(no extra detail in the knowledge base)");
    ShowReport();
}

static int UiHitCard(POINT pt, RECT *client) {
    for (int i = 0; i < g_uiCards; i++) {
        int x, y, w, h;
        if (!UiCardPos(i, &x, &y, &w, &h, client)) continue;
        if (pt.x >= x && pt.x < x + w && pt.y >= y && pt.y < y + h) return i;
    }
    return -1;
}

static void UiDrawSidebar(HDC hdc, RECT *rc) {
    int sy = 104;
    int itemH = 38;
    int vis = (rc->bottom - 28 - sy) / itemH;
    if (vis < 1) vis = 1;
    for (int i = g_uiSideScroll; i < g_categoryCount; i++) {
        int row = i - g_uiSideScroll;
        if (row >= vis) break;
        int y = sy + row * itemH;
        int selected = (i == g_uiCat && !g_uiSearching);
        int hover = (i == g_uiHoverSide);
        COLORREF bg = UI_PANEL;
        if (selected) bg = RGB(28, 40, 62);
        else if (hover) bg = RGB(27, 30, 40);
        UiFill(hdc, 0, y, 232, itemH, bg);
        if (selected) UiFill(hdc, 0, y, 4, itemH, UI_ACCENT);
        int cx = 30, cy = y + itemH / 2;
        HBRUSH b = CreateSolidBrush(g_avatarCol[i % 12]);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
        HPEN p = CreatePen(PS_SOLID, 1, selected ? UI_ACCENT : UI_BORDER);
        HPEN op = (HPEN)SelectObject(hdc, p);
        Ellipse(hdc, cx - 11, cy - 11, cx + 11, cy + 11);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(b);
        DeleteObject(p);
        char letter[2] = { (char)toupper(g_Categories[i].name[0]), 0 };
        UiText(hdc, letter, cx - 11, cy - 9, 22, 18, UI_TEXT, g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        UiText(hdc, g_Categories[i].name, 48, y, 150, itemH,
               selected ? UI_TEXT : UI_DIM, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        char cnt[32];
        sprintf(cnt, "%d", g_Categories[i].count);
        UiText(hdc, cnt, 186, y, 40, itemH, UI_DIM, g_fTiny, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void UiDrawCards(HDC hdc, RECT *rc) {
    for (int i = 0; i < g_uiCards; i++) {
        int x, y, w, h;
        if (!UiCardPos(i, &x, &y, &w, &h, rc)) continue;
        int ci = g_uiCardCat[i], ii = g_uiCardItem[i];
        const Command *cmd = &g_Categories[ci].items[ii];
        int hover = (i == g_uiHover);
        COLORREF bg = hover ? UI_CARD_HOV : UI_CARD;
        COLORREF bd = hover ? UI_ACCENT : UI_BORDER;
        UiRound(hdc, x, y, w, h, 10, bg, bd);
        int ac = ci % 12;
        UiRound(hdc, x + 12, y + 16, 44, 44, 8, g_avatarCol[ac], g_avatarCol[ac]);
        char letter[2] = { (char)toupper(cmd->name[0]), 0 };
        UiText(hdc, letter, x + 12, y + 16, 44, 44, RGB(255, 255, 255), g_fTitle,
               DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        char title[64], sub[96];
        UiClipText(title, sizeof title, cmd->name, 46);
        UiClipText(sub, sizeof sub, cmd->desc, 54);
        UiText(hdc, title, x + 66, y + 12, w - 78, 24, UI_TEXT, g_fNormal,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        UiText(hdc, sub, x + 66, y + 40, w - 78, 18, UI_DIM, g_fSmall,
               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (g_uiSearching) {
            UiText(hdc, g_Categories[ci].name, x + 66, y + 60, w - 78, 16, UI_ACCENT2, g_fTiny,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else if (cmd->needsArg) {
            UiText(hdc, "[needs argument]", x + 66, y + 60, w - 78, 16, UI_ORANGE, g_fTiny,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void UiDrawHeader(HDC hdc, RECT *rc) {
    UiGradV(hdc, 0, 0, rc->right, 64, RGB(16, 30, 60), RGB(10, 16, 32));
    const int rects[4][4] = {
        { 16, 16, 34, 34 }, { 36, 16, 54, 34 }, { 16, 36, 34, 54 }, { 36, 36, 54, 54 }
    };
    for (int i = 0; i < 4; i++) {
        UiRound(hdc, rects[i][0], rects[i][1], 18, 18, 4, g_avatarCol[i * 3 % 12], g_avatarCol[i * 3 % 12]);
    }
    UiText(hdc, "WINDOWS CONTROL CENTER", 62, 8, 360, 30, UI_TEXT, g_fTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    UiText(hdc, "PRO MAX - all-in-one panel", 62, 38, 320, 20, RGB(120, 170, 255), g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* live chips */
    char chip[64];
    int cx = 330;
    sprintf(chip, "CPU %3.0f%%", g_cpuPct);
    UiRound(hdc, cx, 16, 118, 34, 8, RGB(20, 24, 36), UI_BORDER);
    UiText(hdc, chip, cx + 8, 16, 104, 34, g_cpuPct > 85 ? UI_RED : UI_TEXT, g_fSmall,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    cx += 126;
    sprintf(chip, "RAM %3.0f%%", g_ramPct);
    UiRound(hdc, cx, 16, 118, 34, 8, RGB(20, 24, 36), UI_BORDER);
    UiText(hdc, chip, cx + 8, 16, 104, 34, g_ramPct > 90 ? UI_RED : UI_TEXT, g_fSmall,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    cx += 126;
    sprintf(chip, "D: %3.0f%%", g_diskUsedPct);
    UiRound(hdc, cx, 16, 118, 34, 8, RGB(20, 24, 36), UI_BORDER);
    UiText(hdc, chip, cx + 8, 16, 104, 34, g_diskUsedPct > 95 ? UI_RED : UI_TEXT, g_fSmall,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* header tool buttons */
    static const struct { int id; const char *label; } btns[] = {
        { IDC_BTN_TERM, "Terminal" }, { IDC_BTN_MON, "Monitor" },
        { IDC_BTN_UNINS, "Apps" }, { IDC_BTN_FIND, "Find" },
        { IDC_BTN_WIFI, "Wi-Fi" }, { IDC_BTN_PROC, "Procs" },
        { IDC_BTN_FLOAT, "Widget" }, { IDC_BTN_ABOUT, "About" },
        { IDC_BTN_EXIT, "Exit" },
    };
    int nb = (int)(sizeof btns / sizeof btns[0]);
    int bw = 70, gap = 6;
    int total = nb * bw + (nb - 1) * gap;
    int x0 = rc->right - total - 12;
    for (int i = 0; i < nb; i++) {
        int x = x0 + i * (bw + gap);
        int hover = (i == g_uiHoverHead);
        int exit = (btns[i].id == IDC_BTN_EXIT);
        COLORREF bg = hover ? (exit ? RGB(120, 40, 45) : RGB(30, 60, 110)) : RGB(22, 28, 44);
        COLORREF tc = exit ? (hover ? UI_TEXT : RGB(255, 140, 140)) : (hover ? UI_TEXT : RGB(150, 165, 190));
        UiRound(hdc, x, 15, bw, 34, 8, bg, hover ? UI_ACCENT : UI_BORDER);
        UiText(hdc, btns[i].label, x, 15, bw, 34, tc, g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void UiDrawSearchBar(HDC hdc, RECT *rc) {
    UiFill(hdc, 0, 64, rc->right, 40, UI_PANEL);
    UiFill(hdc, 0, 103, rc->right, 1, UI_BORDER);
    UiText(hdc, "SEARCH", 14, 68, 60, 30, UI_DIM, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    UiText(hdc, "RIGHT-CLICK A CARD FOR DETAILS - CLICK TO RUN", 620, 68, 560, 30, UI_DIM, g_fTiny,
           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void UiDrawStatus(HDC hdc, RECT *rc) {
    int sy = rc->bottom - 28;
    UiFill(hdc, 0, sy, rc->right, 28, UI_PANEL);
    UiFill(hdc, 0, sy, rc->right, 1, UI_BORDER);
    char txt[128];
    sprintf(txt, "%d categories  |  %d commands  |  %s", g_categoryCount, g_totalCommands,
            g_uiSearching ? "search results" : g_Categories[g_uiCat].name);
    UiText(hdc, txt, 12, sy, 700, 28, UI_DIM, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    sprintf(txt, "viewing %d cards", g_uiCards);
    UiText(hdc, txt, rc->right - 180, sy, 168, 28, UI_ACCENT2, g_fSmall,
           DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void UiPaintAll(HWND hWnd, HDC hdc) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    UiFill(hdc, 0, 0, rc.right, rc.bottom, UI_BG);
    UiDrawHeader(hdc, &rc);
    UiDrawSearchBar(hdc, &rc);
    UiFill(hdc, 230, 104, 2, rc.bottom - 132, UI_BORDER);
    UiDrawSidebar(hdc, &rc);
    UiDrawCards(hdc, &rc);
    UiDrawStatus(hdc, &rc);
}

static int UiHitHeaderBtn(POINT pt, RECT *rc) {
    static const struct { int id; } btns[] = {
        { IDC_BTN_TERM }, { IDC_BTN_MON }, { IDC_BTN_UNINS }, { IDC_BTN_FIND },
        { IDC_BTN_WIFI }, { IDC_BTN_PROC }, { IDC_BTN_FLOAT }, { IDC_BTN_ABOUT },
        { IDC_BTN_EXIT },
    };
    int nb = (int)(sizeof btns / sizeof btns[0]);
    int bw = 70, gap = 6;
    int total = nb * bw + (nb - 1) * gap;
    int x0 = rc->right - total - 12;
    for (int i = 0; i < nb; i++) {
        int x = x0 + i * (bw + gap);
        if (pt.x >= x && pt.x < x + bw && pt.y >= 15 && pt.y < 49) return btns[i].id;
    }
    return -1;
}

static void UiRunHeaderAction(int id) {
    switch (id) {
        case IDC_BTN_TERM: OnUtilTerminal(NULL); break;
        case IDC_BTN_MON: OnMonitorTool(NULL); break;
        case IDC_BTN_UNINS: OnUninstallTool(NULL); break;
        case IDC_BTN_FIND: OnFindTool(NULL); break;
        case IDC_BTN_WIFI: OnWifiTool(NULL); break;
        case IDC_BTN_PROC: OnProcessTool(NULL); break;
        case IDC_BTN_FLOAT: RunFloat(); break;
        case IDC_BTN_ABOUT: OnUtilAbout(NULL); break;
        case IDC_BTN_EXIT: if (g_hMain) DestroyWindow(g_hMain); break;
    }
}

LRESULT CALLBACK GuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            UiEnsureFonts();
            g_uiCat = 0;
            g_uiScroll = 0;
            g_uiSideScroll = 0;
            g_uiSearch[0] = 0;
            g_uiHover = g_uiHoverSide = g_uiHoverHead = -1;
            g_hSearchEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                            74, 70, 520, 28, hWnd, (HMENU)IDC_SEARCH, g_hInst, NULL);
            SendMessageA(g_hSearchEdit, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            UiBuildView();
            g_uiTimerId = SetTimer(hWnd, 1, 1000, NULL);
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                GetWindowTextA(g_hSearchEdit, g_uiSearch, sizeof g_uiSearch);
                UiBuildView();
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (HIWORD(wParam) == BN_CLICKED) {
                UiRunHeaderAction((int)LOWORD(wParam));
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            UiPaintAll(hWnd, hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_TIMER:
            if (wParam == 1) {
                UiSampleStats();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (g_hSearchEdit)
                SetWindowPos(g_hSearchEdit, NULL, 74, 70,
                             (rc.right > 300 ? rc.right - 700 : 500), 28, SWP_NOZORDER);
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_MOUSEWHEEL: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            RECT rcb;
            GetClientRect(hWnd, &rcb);
            int delta = (short)HIWORD(wParam);
            int rows = (delta > 0) ? -3 : 3;
            if (pt.x < 232) {
                int itemH = 38;
                g_uiSideScroll += rows;
                int maxSide = g_categoryCount - (rcb.bottom / itemH);
                if (maxSide < 0) maxSide = 0;
                if (g_uiSideScroll < 0) g_uiSideScroll = 0;
                if (g_uiSideScroll > maxSide) g_uiSideScroll = maxSide;
            } else {
                g_uiScroll += rows;
                if (g_uiScroll < 0) g_uiScroll = 0;
                int rowsTotal = (g_uiCards + 2) / 3;
                int maxScroll = rowsTotal - ((rcb.bottom - 108 - 28) / 94);
                if (maxScroll < 0) maxScroll = 0;
                if (g_uiScroll > maxScroll) g_uiScroll = maxScroll;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_MOUSEMOVE: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc;
            GetClientRect(hWnd, &rc);
            int card = UiHitCard(pt, &rc);
            int side = -1;
            if (pt.x < 232 && pt.y >= 104) {
                side = g_uiSideScroll + (pt.y - 104) / 38;
                if (side >= g_categoryCount) side = -1;
            }
            int head = UiHitHeaderBtn(pt, &rc);
            if (card != g_uiHover || side != g_uiHoverSide || head != g_uiHoverHead) {
                g_uiHover = card;
                g_uiHoverSide = side;
                g_uiHoverHead = head;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_SETCURSOR:
            if (g_uiHover >= 0 || g_uiHoverSide >= 0 || g_uiHoverHead >= 0) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return 1;
            }
            break;
        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc;
            GetClientRect(hWnd, &rc);
            int head = UiHitHeaderBtn(pt, &rc);
            if (head >= 0) { UiRunHeaderAction(head); break; }
            if (pt.x < 232 && pt.y >= 104) {
                int side = g_uiSideScroll + (pt.y - 104) / 38;
                if (side >= 0 && side < g_categoryCount) {
                    g_uiCat = side;
                    g_uiSearch[0] = 0;
                    if (g_hSearchEdit) SetWindowTextA(g_hSearchEdit, "");
                    UiBuildView();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                break;
            }
            int card = UiHitCard(pt, &rc);
            if (card >= 0) UiRunCard(card);
            break;
        }
        case WM_RBUTTONUP: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc;
            GetClientRect(hWnd, &rc);
            int card = UiHitCard(pt, &rc);
            if (card >= 0) UiExplainCard(card);
            break;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            if (g_uiTimerId) KillTimer(hWnd, g_uiTimerId);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

/* ==========================================================================
   SYSTEM MONITOR TOOL (live charts + top processes)
   ========================================================================== */
#define MON_HIST 240
static double g_monCpu[MON_HIST], g_monRam[MON_HIST], g_monNet[MON_HIST];
static int g_monN = 0;
static char g_monProc[6][110];
static HWND g_hMonWnd;

static void MonSample(void) {
    UiSampleStats();
    if (g_monN < MON_HIST) {
        g_monCpu[g_monN] = g_cpuPct;
        g_monRam[g_monN] = g_ramPct;
        g_monNet[g_monN] = (g_netDown + g_netUp) / 2.0 / 1048576.0;
        g_monN++;
    } else {
        memmove(&g_monCpu[0], &g_monCpu[1], (MON_HIST - 1) * sizeof(double));
        memmove(&g_monRam[0], &g_monRam[1], (MON_HIST - 1) * sizeof(double));
        memmove(&g_monNet[0], &g_monNet[1], (MON_HIST - 1) * sizeof(double));
        g_monCpu[MON_HIST - 1] = g_cpuPct;
        g_monRam[MON_HIST - 1] = g_ramPct;
        g_monNet[MON_HIST - 1] = (g_netDown + g_netUp) / 2.0 / 1048576.0;
    }
    /* top processes by memory */
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        static struct { DWORD pid; SIZE_T mem; char name[64]; } top[6];
        int topN = 0;
        memset(top, 0, sizeof top);
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof pe;
        if (Process32FirstW(snap, &pe)) {
            do {
                SIZE_T mem = 0;
                HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (hp) {
                    PROCESS_MEMORY_COUNTERS pmc;
                    pmc.cb = sizeof pmc;
                    if (GetProcessMemoryInfo(hp, &pmc, sizeof pmc)) mem = pmc.WorkingSetSize;
                    CloseHandle(hp);
                }
                int slot = (topN < 6) ? topN : -1;
                for (int i = 0; i < topN; i++) if (top[i].mem < mem) { slot = i; break; }
                if (slot >= 0) {
                    if (topN < 6) topN++;
                    for (int i = topN - 1; i > slot; i--) top[i] = top[i - 1];
                    top[slot].pid = pe.th32ProcessID;
                    top[slot].mem = mem;
                    WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, top[slot].name, 64, NULL, NULL);
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        for (int i = 0; i < 6; i++) {
            if (i < topN)
                sprintf(g_monProc[i], "%s  -  %.1f MB  (PID %u)",
                        top[i].name, (double)top[i].mem / 1048576.0, top[i].pid);
            else
                g_monProc[i][0] = 0;
        }
    }
}

static void MonChart(HDC hdc, int x, int y, int w, int h, const double *data,
                     int n, const char *title, double maxv, COLORREF col, const char *unit) {
    UiRound(hdc, x, y, w, h, 10, UI_CARD, UI_BORDER);
    UiText(hdc, title, x + 12, y + 6, w - 24, 20, UI_DIM, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    char mx[32];
    sprintf(mx, "%.0f%s", maxv, unit);
    UiText(hdc, mx, x + w - 60, y + 6, 48, 20, UI_DIM, g_fTiny, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    int gx = x + 12, gy = y + 32, gw = w - 24, gh = h - 44;
    UiFill(hdc, gx, gy, gw, gh, RGB(16, 18, 26));
    for (int i = 1; i < 4; i++) {
        UiFill(hdc, gx, gy + gh * i / 4, gw, 1, UI_BORDER);
    }
    if (n > 1) {
        POINT *pts = (POINT *)malloc(sizeof(POINT) * n);
        if (pts) {
            for (int i = 0; i < n; i++) {
                double v = data[i];
                if (v < 0) v = 0;
                if (v > maxv) v = maxv;
                pts[i].x = gx + (int)((double)i * gw / (double)(MON_HIST - 1));
                pts[i].y = gy + gh - (int)(v * gh / maxv);
            }
            HBRUSH fb = CreateSolidBrush(RGB(GetRValue(col) / 3, GetGValue(col) / 3, GetBValue(col) / 3));
            HBRUSH ob = (HBRUSH)SelectObject(hdc, fb);
            HPEN p = CreatePen(PS_SOLID, 2, col);
            HPEN op = (HPEN)SelectObject(hdc, p);
            POINT *poly = (POINT *)malloc(sizeof(POINT) * (n + 2));
            if (poly) {
                for (int i = 0; i < n; i++) poly[i] = pts[i];
                poly[n].x = pts[n - 1].x; poly[n].y = gy + gh;
                poly[n + 1].x = pts[0].x; poly[n + 1].y = gy + gh;
                Polygon(hdc, poly, n + 2);
                free(poly);
            }
            Polyline(hdc, pts, n);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(fb);
            DeleteObject(p);
            free(pts);
        }
    }
    char last[32];
    if (n > 0) sprintf(last, "now: %.1f%s", data[n - 1], unit);
    else sprintf(last, "now: -");
    UiText(hdc, last, x + 12, y + h - 18, w - 24, 16, col, g_fTiny, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CALLBACK MonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            UiEnsureFonts();
            g_monN = 0;
            MonSample();
            SetTimer(hWnd, 2, 1000, NULL);
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            if (wParam == 2) {
                MonSample();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            UiFill(hdc, 0, 0, rc.right, rc.bottom, UI_BG);
            UiGradV(hdc, 0, 0, rc.right, 46, RGB(16, 30, 60), RGB(10, 16, 32));
            UiText(hdc, "SYSTEM MONITOR - live 1s refresh", 14, 6, 500, 34, UI_TEXT, g_fTitle,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            MonChart(hdc, 12, 56, 430, 150, g_monCpu, g_monN, "CPU HISTORY", 100.0, UI_ACCENT, "%");
            MonChart(hdc, 12, 216, 430, 150, g_monRam, g_monN, "RAM HISTORY", 100.0, UI_GREEN, "%");
            MonChart(hdc, 12, 376, 430, 120, g_monNet, g_monN, "NETWORK (up+down)", 20.0, UI_ACCENT2, " MB/s");
            /* right: processes */
            UiRound(hdc, 456, 56, rc.right - 468, 300, 10, UI_CARD, UI_BORDER);
            UiText(hdc, "TOP PROCESSES BY MEMORY", 470, 62, rc.right - 480, 20, UI_DIM, g_fSmall,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            for (int i = 0; i < 6; i++) {
                int y = 92 + i * 40;
                if (g_monProc[i][0]) {
                    UiRound(hdc, 470, y, rc.right - 484, 34, 8, RGB(22, 26, 36), UI_BORDER);
                    UiText(hdc, g_monProc[i], 482, y, rc.right - 500, 34, UI_TEXT, g_fSmall,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }
            /* bottom stats */
            int by = rc.bottom - 116;
            char st[128];
            UiRound(hdc, 456, by, rc.right - 468, 100, 10, UI_CARD, UI_BORDER);
            sprintf(st, "CPU   %5.1f%%", g_cpuPct);
            UiText(hdc, st, 470, by + 10, 200, 22, UI_TEXT, g_fNormal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            sprintf(st, "RAM   %5.1f%%", g_ramPct);
            UiText(hdc, st, 470, by + 36, 200, 22, UI_TEXT, g_fNormal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            sprintf(st, "DISK  %5.1f%%", g_diskUsedPct);
            UiText(hdc, st, 470, by + 62, 200, 22, UI_TEXT, g_fNormal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            sprintf(st, "DOWN %.2f MB/s   UP %.2f MB/s", g_netDown / 1048576.0, g_netUp / 1048576.0);
            UiText(hdc, st, 600, by + 10, 220, 22, UI_ACCENT2, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            UiText(hdc, "CLICK [X] OR PRESS ESC TO CLOSE", 470, by + 74, 300, 18, UI_DIM, g_fTiny,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            KillTimer(hWnd, 2);
            g_hMonWnd = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnMonitorTool(const char *arg) {
    (void)arg;
    if (g_consoleMode) { MonitorConsole(); return; }
    if (g_hMonWnd) { ShowWindow(g_hMonWnd, SW_SHOW); SetForegroundWindow(g_hMonWnd); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = MonWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "WcMonClass";
    RegisterClassA(&wc);
    g_hMonWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcMonClass",
                                APP_NAME " - System Monitor",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                940, 560, NULL, NULL, g_hInst, NULL);
    if (!g_hMonWnd) { Notify("Failed to create the System Monitor window."); return; }
    ShowWindow(g_hMonWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static const char *GetUptimeStr(void) {
    static char buf[64];
    ULONGLONG ms = GetTickCount64();
    ULONGLONG d = ms / 86400000ULL, h = (ms / 3600000ULL) % 24,
              m = (ms / 60000ULL) % 60, s = (ms / 1000ULL) % 60;
    sprintf(buf, "%llu days %llu hours %llu min %llu sec", d, h, m, s);
    return buf;
}

void OnSystemStats(const char *arg) {
    (void)arg;
    EnsureConsole();
    UiSampleStats();
    UiSampleStats();
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    GlobalMemoryStatusEx(&ms);
    ConColor(CC_CYAN);
    printf("\n  ==== SYSTEM STATS ====\n");
    ConColor(CC_BLUE);
    printf("  CPU   : %.1f%%\n", g_cpuPct);
    printf("  RAM   : %.1f%%  (%.1f / %.1f GB)\n", g_ramPct,
           (double)(ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0,
           (double)ms.ullTotalPhys / 1073741824.0);
    printf("  DISK  : %.1f%% used on C:\\\n", g_diskUsedPct);
    printf("  NET   : down %.2f MB/s, up %.2f MB/s\n", g_netDown / 1048576.0, g_netUp / 1048576.0);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    printf("  CORES : %u logical processors\n", si.dwNumberOfProcessors);
    printf("  UPTIME: %s\n", GetUptimeStr());
    ConColor(CC_BLUE);
}

/* ==========================================================================
   APP UNINSTALLER
   ========================================================================== */
typedef struct { char name[256]; char cmd[640]; char ver[64]; char pub[128]; } AppRec;
static AppRec g_apps[1024];
static int g_appCount = 0;

static int EnumApps(void) {
    g_appCount = 0;
    const char *sub = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for (int pass = 0; pass < 4 && g_appCount < 1024; pass++) {
        HKEY root = (pass < 2) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        const char *path = (pass == 0 || pass == 2) ? sub : "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
        HKEY k;
        if (RegOpenKeyExA(root, path, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        DWORD i = 0;
        char kid[512];
        DWORD klen = sizeof kid;
        while (RegEnumKeyExA(k, i++, kid, &klen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            if (g_appCount >= 1024) break;
            HKEY kk;
            char full[640];
            sprintf(full, "%s\\%s", path, kid);
            if (RegOpenKeyExA(root, full, 0, KEY_READ, &kk) != ERROR_SUCCESS) { klen = sizeof kid; continue; }
            char name[256] = "", cmd[640] = "", ver[64] = "", pub[128] = "";
            DWORD sz = sizeof name;
            if (RegQueryValueExA(kk, "DisplayName", NULL, NULL, (BYTE *)name, &sz) != ERROR_SUCCESS)
                name[0] = 0;
            sz = sizeof cmd;
            if (RegQueryValueExA(kk, "UninstallString", NULL, NULL, (BYTE *)cmd, &sz) != ERROR_SUCCESS)
                cmd[0] = 0;
            sz = sizeof ver;
            if (RegQueryValueExA(kk, "DisplayVersion", NULL, NULL, (BYTE *)ver, &sz) != ERROR_SUCCESS)
                ver[0] = 0;
            sz = sizeof pub;
            if (RegQueryValueExA(kk, "Publisher", NULL, NULL, (BYTE *)pub, &sz) != ERROR_SUCCESS)
                pub[0] = 0;
            RegCloseKey(kk);
            if (name[0] && cmd[0]) {
                int dup = 0;
                for (int j = 0; j < g_appCount; j++)
                    if (stricmp(g_apps[j].name, name) == 0 && stricmp(g_apps[j].cmd, cmd) == 0) { dup = 1; break; }
                if (!dup) {
                    AppRec *a = &g_apps[g_appCount++];
                    strncpy(a->name, name, sizeof a->name - 1);
                    strncpy(a->cmd, cmd, sizeof a->cmd - 1);
                    strncpy(a->ver, ver, sizeof a->ver - 1);
                    strncpy(a->pub, pub, sizeof a->pub - 1);
                }
            }
            klen = sizeof kid;
        }
        RegCloseKey(k);
    }
    return g_appCount;
}

void OnListApps(const char *arg) {
    (void)arg;
    EnsureConsole();
    EnumApps();
    ConColor(CC_CYAN);
    printf("\n  ==== INSTALLED APPLICATIONS (%d) ====\n", g_appCount);
    ConColor(CC_BLUE);
    for (int i = 0; i < g_appCount && i < 400; i++) {
        printf("  %s", g_apps[i].name);
        if (g_apps[i].ver[0]) printf("  [%s]", g_apps[i].ver);
        if (g_apps[i].pub[0]) printf("  (%s)", g_apps[i].pub);
        printf("\n");
    }
    if (g_appCount > 400) printf("  ... and %d more\n", g_appCount - 400);
    ConColor(CC_BLUE);
}

static HWND g_hUnWnd;
static char g_unFilter[128];

static void UnFillList(HWND lb) {
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_appCount; i++) {
        if (g_unFilter[0] && !stristr(g_apps[i].name, g_unFilter)) continue;
        char text[512];
        sprintf(text, "%s  [%s]", g_apps[i].name, g_apps[i].cmd);
        int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)i);
    }
}

LRESULT CALLBACK UnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            UiEnsureFonts();
            EnumApps();
            CreateWindowA("STATIC", "Search installed apps:", WS_CHILD | WS_VISIBLE,
                          14, 10, 180, 20, hWnd, NULL, g_hInst, NULL);
            HWND ed = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                    200, 8, 400, 24, hWnd, (HMENU)IDC_ARG_TOOL, g_hInst, NULL);
            SendMessageA(ed, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            HWND lb = CreateWindowA("LISTBOX", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                    14, 40, 586, 380, hWnd, (HMENU)IDC_LB_TOOL, g_hInst, NULL);
            SendMessageA(lb, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            UnFillList(lb);
            struct { int id; const char *t; int x; } bs[] = {
                { IDC_BTN_TOOL1, "Uninstall selected", 14 },
                { IDC_BTN_TOOL2, "Refresh", 180 },
                { IDC_BTN_TOOL3, "Close", 300 },
            };
            for (int i = 0; i < 3; i++) {
                HWND b = CreateWindowA("BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       bs[i].x, 432, 150, 30, hWnd, (HMENU)(INT_PTR)bs[i].id, g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_ARG_TOOL && HIWORD(wParam) == EN_CHANGE) {
                GetWindowTextA(GetDlgItem(hWnd, IDC_ARG_TOOL), g_unFilter, sizeof g_unFilter);
                TrimSpaces(g_unFilter);
                UnFillList(GetDlgItem(hWnd, IDC_LB_TOOL));
            } else if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == IDC_BTN_TOOL1) {
                    HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                    int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                    if (sel >= 0) {
                        int i = (int)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
                        LaunchUninstaller(i);
                    }
                } else if (LOWORD(wParam) == IDC_BTN_TOOL2) {
                    EnumApps();
                    UnFillList(GetDlgItem(hWnd, IDC_LB_TOOL));
                } else if (LOWORD(wParam) == IDC_BTN_TOOL3) {
                    DestroyWindow(hWnd);
                }
            } else if (LOWORD(wParam) == IDC_LB_TOOL && HIWORD(wParam) == LBN_DBLCLK) {
                SendMessageA(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_TOOL1, BN_CLICKED), 0);
            }
            break;
        case WM_DESTROY:
            g_hUnWnd = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnUninstallTool(const char *arg) {
    (void)arg;
    if (g_consoleMode) { UninsConsole(); return; }
    if (g_hUnWnd) { ShowWindow(g_hUnWnd, SW_SHOW); SetForegroundWindow(g_hUnWnd); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = UnWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WcUnClass";
    RegisterClassA(&wc);
    g_hUnWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcUnClass",
                               APP_NAME " - App Uninstaller",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               620, 500, NULL, NULL, g_hInst, NULL);
    if (!g_hUnWnd) { Notify("Failed to create the App Uninstaller window."); return; }
    ShowWindow(g_hUnWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/* ==========================================================================
   FILE SEARCH TOOL
   ========================================================================== */
static char g_fndPaths[2000][520];
static int g_fndCount = 0;
static volatile int g_fndStop = 0;
static HWND g_hFindWnd;
static char g_findRoot[300], g_findMask[100];

static int MatchWildcard(const char *s, const char *pat) {
    while (*pat) {
        if (*pat == '*') {
            while (*pat == '*') pat++;
            if (!*pat) return 1;
            for (; *s; s++)
                if (MatchWildcard(s, pat)) return 1;
            return 0;
        }
        if (*pat == '?') {
            if (!*s) return 0;
            s++;
            pat++;
            continue;
        }
        if (_strnicmp(s, pat, 1) != 0) return 0;
        s++;
        pat++;
    }
    return *s == 0;
}

static void FindWorkerRec(const char *dir, int depth) {
    if (g_fndStop || g_fndCount >= 2000 || depth > 9) return;
    char pat[640];
    sprintf(pat, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (g_fndStop || g_fndCount >= 2000) break;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[600];
        sprintf(full, "%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            FindWorkerRec(full, depth + 1);
        } else if (g_findMask[0] == 0 || stristr(fd.cFileName, g_findMask) ||
                   MatchWildcard(fd.cFileName, g_findMask)) {
            if (g_fndCount < 2000) {
                strncpy(g_fndPaths[g_fndCount], full, sizeof g_fndPaths[0] - 1);
                g_fndPaths[g_fndCount][sizeof g_fndPaths[0] - 1] = 0;
                g_fndCount++;
                if (g_fndCount % 50 == 0) PostMessageA(g_hFindWnd, WM_APP + 1, 0, 0);
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static DWORD WINAPI FindWorker(LPVOID p) {
    (void)p;
    g_fndCount = 0;
    FindWorkerRec(g_findRoot, 0);
    PostMessageA(g_hFindWnd, WM_APP + 2, 0, 0);
    return 0;
}

static void FndFillList(HWND lb) {
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_fndCount; i++) {
        const char *slash = strrchr(g_fndPaths[i], '\\');
        const char *name = slash ? slash + 1 : g_fndPaths[i];
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(g_fndPaths[i], &fd);
        char sz[32];
        if (h != INVALID_HANDLE_VALUE) {
            ULONGLONG s = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (s >= 1048576) sprintf(sz, "%.1f MB", (double)s / 1048576.0);
            else if (s >= 1024) sprintf(sz, "%.1f KB", (double)s / 1024.0);
            else sprintf(sz, "%llu B", s);
            FindClose(h);
        } else {
            sprintf(sz, "?");
        }
        char text[640];
        sprintf(text, "%s  (%s)  %s", name, sz, g_fndPaths[i]);
        int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)i);
    }
}

LRESULT CALLBACK FindWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            UiEnsureFonts();
            g_fndCount = 0;
            CreateWindowA("STATIC", "Root:", WS_CHILD | WS_VISIBLE, 14, 10, 50, 20, hWnd, NULL, g_hInst, NULL);
            HWND root = CreateWindowA("COMBOBOX", "C:\\", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                                      70, 8, 170, 200, hWnd, (HMENU)IDC_FIND_ROOT, g_hInst, NULL);
            SendMessageA(root, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            SendMessageA(root, CB_ADDSTRING, 0, (LPARAM)"C:\\");
            SendMessageA(root, CB_ADDSTRING, 0, (LPARAM)"D:\\");
            SendMessageA(root, CB_ADDSTRING, 0, (LPARAM)"E:\\");
            SendMessageA(root, CB_SETCURSEL, 0, 0);
            CreateWindowA("STATIC", "Mask (*.mp4, *., etc):", WS_CHILD | WS_VISIBLE,
                          250, 10, 160, 20, hWnd, NULL, g_hInst, NULL);
            HWND mask = CreateWindowA("EDIT", "*", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      420, 8, 150, 24, hWnd, (HMENU)IDC_ARG_TOOL, g_hInst, NULL);
            SendMessageA(mask, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            HWND lb = CreateWindowA("LISTBOX", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                    14, 40, 610, 360, hWnd, (HMENU)IDC_LB_TOOL, g_hInst, NULL);
            SendMessageA(lb, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            struct { int id; const char *t; int x; } bs[] = {
                { IDC_BTN_TOOL1, "Search", 14 },
                { IDC_BTN_TOOL2, "Open", 150 },
                { IDC_BTN_TOOL3, "Open Folder", 280 },
                { IDC_BTN_TOOL4, "Close", 410 },
            };
            for (int i = 0; i < 4; i++) {
                HWND b = CreateWindowA("BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       bs[i].x, 410, 120, 30, hWnd, (HMENU)(INT_PTR)bs[i].id, g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_TOOL1 && HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemTextA(hWnd, IDC_FIND_ROOT, g_findRoot, sizeof g_findRoot);
                GetDlgItemTextA(hWnd, IDC_ARG_TOOL, g_findMask, sizeof g_findMask);
                TrimSpaces(g_findRoot);
                TrimSpaces(g_findMask);
                size_t rl = strlen(g_findRoot);
                while (rl > 0 && (g_findRoot[rl - 1] == '\\')) g_findRoot[--rl] = 0;
                if (g_findRoot[0] == 0) strcpy(g_findRoot, "C:");
                g_fndStop = 0;
                g_fndCount = 0;
                SendMessageA(GetDlgItem(hWnd, IDC_LB_TOOL), LB_RESETCONTENT, 0, 0);
                CreateThread(NULL, 0, FindWorker, NULL, 0, NULL);
            } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_BTN_TOOL2) {
                HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    int i = (int)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
                    if (i >= 0 && i < g_fndCount)
                        ShellExecuteA(NULL, "open", g_fndPaths[i], NULL, NULL, SW_SHOWNORMAL);
                }
            } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_BTN_TOOL3) {
                HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    int i = (int)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
                    if (i >= 0 && i < g_fndCount) {
                        char arg[700];
                        sprintf(arg, "/select,\"%s\"", g_fndPaths[i]);
                        ShellExecuteA(NULL, "open", "explorer.exe", arg, NULL, SW_SHOWNORMAL);
                    }
                }
            } else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_BTN_TOOL4) {
                DestroyWindow(hWnd);
            } else if (LOWORD(wParam) == IDC_LB_TOOL && HIWORD(wParam) == LBN_DBLCLK) {
                SendMessageA(hWnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_TOOL2, BN_CLICKED), 0);
            }
            break;
        case WM_APP + 1:
            FndFillList(GetDlgItem(hWnd, IDC_LB_TOOL));
            break;
        case WM_APP + 2:
            FndFillList(GetDlgItem(hWnd, IDC_LB_TOOL));
            SetWindowTextA(hWnd, "File Search - done");
            break;
        case WM_DESTROY:
            g_fndStop = 1;
            g_hFindWnd = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnFindTool(const char *arg) {
    (void)arg;
    if (g_consoleMode) { FindConsole(); return; }
    if (g_hFindWnd) { ShowWindow(g_hFindWnd, SW_SHOW); SetForegroundWindow(g_hFindWnd); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = FindWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WcFindClass";
    RegisterClassA(&wc);
    g_hFindWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcFindClass",
                                 APP_NAME " - File Search",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 660, 480, NULL, NULL, g_hInst, NULL);
    if (!g_hFindWnd) { Notify("Failed to create the File Search window."); return; }
    ShowWindow(g_hFindWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void OnFindFiles(const char *arg) {
    EnsureConsole();
    if (!arg || !arg[0]) {
        ConColor(CC_YELLOW);
        printf("Usage: find-files <mask|root>\nExample: find-files *.mp4|C:\\Users\n");
        ConColor(CC_BLUE);
        return;
    }
    char buf[320];
    strncpy(buf, arg, sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    char *bar = strchr(buf, '|');
    if (bar) {
        *bar = 0;
        strncpy(g_findMask, buf, sizeof g_findMask - 1);
        strncpy(g_findRoot, bar + 1, sizeof g_findRoot - 1);
    } else {
        strncpy(g_findMask, buf, sizeof g_findMask - 1);
        strcpy(g_findRoot, "C:\\");
    }
    TrimSpaces(g_findMask);
    TrimSpaces(g_findRoot);
    size_t rl = strlen(g_findRoot);
    while (rl > 0 && g_findRoot[rl - 1] == '\\') g_findRoot[--rl] = 0;
    if (g_findRoot[0] == 0) strcpy(g_findRoot, "C:");
    ConColor(CC_CYAN);
    printf("\n  Searching '%s' under %s ...\n", g_findMask, g_findRoot);
    ConColor(CC_BLUE);
    g_fndCount = 0;
    g_fndStop = 0;
    FindWorkerRec(g_findRoot, 0);
    ConColor(CC_GREEN);
    printf("  %d matches.\n", g_fndCount);
    ConColor(CC_BLUE);
    for (int i = 0; i < g_fndCount && i < 100; i++) printf("  %s\n", g_fndPaths[i]);
    if (g_fndCount > 100) printf("  ... and %d more (use the GUI tool for full results)\n", g_fndCount - 100);
    ConColor(CC_BLUE);
}

/* ==========================================================================
   WI-FI MANAGER
   ========================================================================== */
static char g_wifiNet[64][128];
static int g_wifiNetCount = 0;
static char g_wifiOut[65536];
static HWND g_hWifiWnd;

static void WifiCollect(const char *line) {
    size_t l = strlen(g_wifiOut);
    if (l + strlen(line) + 2 < sizeof g_wifiOut) {
        strcat(g_wifiOut, line);
        strcat(g_wifiOut, "\n");
    }
}

static void WifiScanNets(void) {
    g_wifiNetCount = 0;
    g_wifiOut[0] = 0;
    RunCommandCapture("netsh wlan show networks", WifiCollect);
    char *p = g_wifiOut;
    char *line = p;
    while ((line = strtok(line, "\n")) != NULL) {
        char *ssid = strstr(line, "SSID");
        if (ssid) {
            char *colon = strchr(ssid, ':');
            if (colon && colon[1]) {
                char *name = colon + 1;
                TrimSpaces(name);
                if (name[0] && g_wifiNetCount < 64) {
                    strncpy(g_wifiNet[g_wifiNetCount], name, 127);
                    g_wifiNet[g_wifiNetCount][127] = 0;
                    g_wifiNetCount++;
                }
            }
        }
        line = NULL;
    }
    /* remove duplicates */
    int n = 0;
    for (int i = 0; i < g_wifiNetCount; i++) {
        int dup = 0;
        for (int j = 0; j < n; j++)
            if (stricmp(g_wifiNet[j], g_wifiNet[i]) == 0) { dup = 1; break; }
        if (!dup) strncpy(g_wifiNet[n++], g_wifiNet[i], 127);
    }
    g_wifiNetCount = n;
}

LRESULT CALLBACK WifiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            UiEnsureFonts();
            WifiScanNets();
            HWND lb = CreateWindowA("LISTBOX", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                    14, 12, 360, 320, hWnd, (HMENU)IDC_LB_TOOL, g_hInst, NULL);
            SendMessageA(lb, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            for (int i = 0; i < g_wifiNetCount; i++) {
                int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)g_wifiNet[i]);
                SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)i);
            }
            HWND ed = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER |
                                    ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                    386, 12, 220, 320, hWnd, (HMENU)IDC_STATUS, g_hInst, NULL);
            SendMessageA(ed, WM_SETFONT, (WPARAM)g_fMono, TRUE);
            struct { int id; const char *t; int x; } bs[] = {
                { IDC_BTN_TOOL1, "Refresh networks", 14 },
                { IDC_BTN_TOOL2, "Connect to selected", 160 },
                { IDC_BTN_TOOL3, "Show profiles", 340 },
                { IDC_BTN_TOOL4, "Close", 500 },
            };
            for (int i = 0; i < 4; i++) {
                HWND b = CreateWindowA("BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       bs[i].x, 344, 140, 30, hWnd, (HMENU)(INT_PTR)bs[i].id, g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            }
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == IDC_BTN_TOOL1) {
                    WifiScanNets();
                    HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
                    for (int i = 0; i < g_wifiNetCount; i++) {
                        int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)g_wifiNet[i]);
                        SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)i);
                    }
                } else if (LOWORD(wParam) == IDC_BTN_TOOL2) {
                    HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                    int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < g_wifiNetCount) {
                        char cmd[400];
                        sprintf(cmd, "netsh wlan connect name=\"%s\"", g_wifiNet[sel]);
                        g_wifiOut[0] = 0;
                        RunCommandCapture(cmd, WifiCollect);
                        SetWindowTextA(GetDlgItem(hWnd, IDC_STATUS), g_wifiOut);
                    }
                } else if (LOWORD(wParam) == IDC_BTN_TOOL3) {
                    g_wifiOut[0] = 0;
                    RunCommandCapture("netsh wlan show profiles", WifiCollect);
                    SetWindowTextA(GetDlgItem(hWnd, IDC_STATUS), g_wifiOut);
                } else if (LOWORD(wParam) == IDC_BTN_TOOL4) {
                    DestroyWindow(hWnd);
                }
            }
            break;
        case WM_DESTROY:
            g_hWifiWnd = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnWifiTool(const char *arg) {
    (void)arg;
    if (g_consoleMode) { WifiConsole(); return; }
    if (g_hWifiWnd) { ShowWindow(g_hWifiWnd, SW_SHOW); SetForegroundWindow(g_hWifiWnd); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = WifiWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WcWifiClass";
    RegisterClassA(&wc);
    g_hWifiWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcWifiClass",
                                 APP_NAME " - Wi-Fi Manager",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 640, 420, NULL, NULL, g_hInst, NULL);
    if (!g_hWifiWnd) { Notify("Failed to create the Wi-Fi Manager window."); return; }
    ShowWindow(g_hWifiWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/* ==========================================================================
   PROCESS MANAGER
   ========================================================================== */
static HWND g_hProcWnd;
static char g_procDetail[1024];

static void ProcFillList(HWND lb) {
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    if (Process32FirstW(snap, &pe)) {
        do {
            SIZE_T mem = 0;
            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hp) {
                PROCESS_MEMORY_COUNTERS pmc;
                pmc.cb = sizeof pmc;
                if (GetProcessMemoryInfo(hp, &pmc, sizeof pmc)) mem = pmc.WorkingSetSize;
                CloseHandle(hp);
            }
            char text[320], name[64];
            WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, name, sizeof name, NULL, NULL);
            sprintf(text, "%6u  %8.1f MB  %s", pe.th32ProcessID, (double)mem / 1048576.0, name);
            int pos = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)text);
            SendMessageA(lb, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

LRESULT CALLBACK ProcWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            UiEnsureFonts();
            CreateWindowA("STATIC", "PID    MEMORY    NAME", WS_CHILD | WS_VISIBLE,
                          14, 10, 400, 18, hWnd, NULL, g_hInst, NULL);
            HWND lb = CreateWindowA("LISTBOX", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                    14, 30, 560, 320, hWnd, (HMENU)IDC_LB_TOOL, g_hInst, NULL);
            SendMessageA(lb, WM_SETFONT, (WPARAM)g_fMono, TRUE);
            ProcFillList(lb);
            HWND det = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     14, 356, 560, 60, hWnd, (HMENU)IDC_STATUS, g_hInst, NULL);
            SendMessageA(det, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            struct { int id; const char *t; int x; } bs[] = {
                { IDC_BTN_TOOL1, "Kill process", 14 },
                { IDC_BTN_TOOL2, "Refresh", 160 },
                { IDC_BTN_TOOL3, "Close", 300 },
            };
            for (int i = 0; i < 3; i++) {
                HWND b = CreateWindowA("BUTTON", bs[i].t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       bs[i].x, 426, 140, 30, hWnd, (HMENU)(INT_PTR)bs[i].id, g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)g_fNormal, TRUE);
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_LB_TOOL && HIWORD(wParam) == LBN_SELCHANGE) {
                HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    DWORD pid = (DWORD)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
                    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hp) {
                        char path[512] = "?";
                        DWORD sz = sizeof path;
                        QueryFullProcessImageNameA(hp, 0, path, &sz);
                        CloseHandle(hp);
                        sprintf(g_procDetail, "PID %u\n%s", pid, path);
                        SetWindowTextA(GetDlgItem(hWnd, IDC_STATUS), g_procDetail);
                    }
                }
            } else if (HIWORD(wParam) == BN_CLICKED) {
                if (LOWORD(wParam) == IDC_BTN_TOOL1) {
                    HWND lb = GetDlgItem(hWnd, IDC_LB_TOOL);
                    int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
                    if (sel >= 0) {
                        DWORD pid = (DWORD)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
                        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                        if (hp) {
                            if (!TerminateProcess(hp, 1)) Notify("Failed to terminate the process.");
                            CloseHandle(hp);
                            ProcFillList(lb);
                        }
                    }
                } else if (LOWORD(wParam) == IDC_BTN_TOOL2) {
                    ProcFillList(GetDlgItem(hWnd, IDC_LB_TOOL));
                } else if (LOWORD(wParam) == IDC_BTN_TOOL3) {
                    DestroyWindow(hWnd);
                }
            }
            break;
        case WM_DESTROY:
            g_hProcWnd = NULL;
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnProcessTool(const char *arg) {
    (void)arg;
    if (g_consoleMode) { ProcConsole(); return; }
    if (g_hProcWnd) { ShowWindow(g_hProcWnd, SW_SHOW); SetForegroundWindow(g_hProcWnd); return; }
    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = ProcWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WcProcClass";
    RegisterClassA(&wc);
    g_hProcWnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "WcProcClass",
                                 APP_NAME " - Process Manager",
                                 WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                 610, 500, NULL, NULL, g_hInst, NULL);
    if (!g_hProcWnd) { Notify("Failed to create the Process Manager window."); return; }
    ShowWindow(g_hProcWnd, SW_SHOW);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

typedef struct { DWORD pid; SIZE_T mem; char name[64]; } ProcRec;

static int ProcTop(ProcRec *top, int maxN) {
    int topN = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof pe;
    if (Process32FirstW(snap, &pe)) {
        do {
            SIZE_T mem = 0;
            HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (hp) {
                PROCESS_MEMORY_COUNTERS pmc;
                pmc.cb = sizeof pmc;
                if (GetProcessMemoryInfo(hp, &pmc, sizeof pmc)) mem = pmc.WorkingSetSize;
                CloseHandle(hp);
            }
            int slot = (topN < maxN) ? topN : -1;
            for (int i = 0; i < topN; i++) if (top[i].mem < mem) { slot = i; break; }
            if (slot >= 0) {
                if (topN < maxN) topN++;
                for (int i = topN - 1; i > slot; i--) top[i] = top[i - 1];
                top[slot].pid = pe.th32ProcessID;
                top[slot].mem = mem;
                WideCharToMultiByte(CP_ACP, 0, pe.szExeFile, -1, top[slot].name, 64, NULL, NULL);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return topN;
}

void OnProcessList(const char *arg) {
    (void)arg;
    EnsureConsole();
    ConColor(CC_CYAN);
    printf("\n  ==== TOP PROCESSES BY MEMORY ====\n");
    ConColor(CC_BLUE);
    ProcRec top[20];
    int topN = ProcTop(top, 20);
    if (topN == 0) { ConColor(CC_RED); printf("  (snapshot failed)\n"); ConColor(CC_BLUE); return; }
    for (int i = 0; i < topN; i++) {
        printf("  %-40s %8.1f MB   (PID %u)\n", top[i].name, (double)top[i].mem / 1048576.0, top[i].pid);
    }
    ConColor(CC_BLUE);
}

void OnNetSpeed(const char *arg) {
    (void)arg;
    EnsureConsole();
    ConColor(CC_CYAN);
    printf("\n  Measuring network speed (1 second)...\n");
    ConColor(CC_BLUE);
    UiSampleStats();
    Sleep(1000);
    UiSampleStats();
    ConColor(CC_GREEN);
    printf("  Download: %.2f MB/s\n  Upload  : %.2f MB/s\n", g_netDown / 1048576.0, g_netUp / 1048576.0);
    ConColor(CC_BLUE);
}

/* ==========================================================================
   CONSOLE TOOLS - text versions of the GUI tools for CLI and TUI modes.
   ========================================================================== */

static void CBar(double pct, int width, char *out) {
    int filled = (int)(pct * width / 100.0);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    int i = 0;
    for (; i < filled; i++) { out[i * 3] = '\xe2'; out[i * 3 + 1] = '\x96'; out[i * 3 + 2] = '\x88'; }
    for (; i < width; i++) { out[i * 3] = ' '; out[i * 3 + 1] = ' '; out[i * 3 + 2] = ' '; }
    out[width * 3] = 0;
}

static void MonitorConsole(void) {
    ConColor(CC_CYAN);
    printf("\n  ==== LIVE SYSTEM MONITOR ====   (press q to quit)\n");
    ConColor(CC_BLUE);
    DWORD inMode = 0;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    int interactive = (hIn != NULL && hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &inMode));
    for (;;) {
        UiSampleStats();
        char b1[64], b2[64], b3[64];
        CBar(g_cpuPct, 15, b1);
        CBar(g_ramPct, 15, b2);
        CBar(g_diskUsedPct, 15, b3);
        printf("\r  CPU %5.1f%% %s  RAM %5.1f%% %s  DISK %5.1f%% %s",
               g_cpuPct, b1, g_ramPct, b2, g_diskUsedPct, b3);
        printf("  NET v%.2f/^%.2f MB/s   ", g_netDown / 1048576.0, g_netUp / 1048576.0);
        fflush(stdout);
        if (interactive) {
            if (_kbhit()) {
                int ch = _getch();
                if (ch == 'q' || ch == 'Q') break;
            }
        } else {
            break;
        }
        Sleep(1000);
    }
    printf("\n");
    ConColor(CC_BLUE);
}

static void LaunchUninstaller(int idx) {
    if (idx < 0 || idx >= g_appCount) return;
    char cmd[768];
    strncpy(cmd, g_apps[idx].cmd, sizeof cmd - 1);
    cmd[sizeof cmd - 1] = 0;
    if (strstr(cmd, "msiexec") || strstr(cmd, "MsiExec")) {
        if (!strstr(cmd, "/I") && !strstr(cmd, "/X") &&
            !strstr(cmd, "/i") && !strstr(cmd, "/x"))
            strncat(cmd, " /X", sizeof cmd - strlen(cmd) - 1);
    }
    HINSTANCE r = ShellExecuteA(NULL, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) Notify("Failed to start the uninstaller.");
}

static void UninsConsole(void) {
    EnumApps();
    char filter[128] = "";
    for (;;) {
        ConColor(CC_GREEN);
        printf("\n  Filter (enter = all, q = quit): ");
        fflush(stdout);
        const char *in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        strncpy(filter, in, sizeof filter - 1);
        filter[sizeof filter - 1] = 0;
        TrimSpaces(filter);
        ConColor(CC_CYAN);
        printf("  ==== INSTALLED APPLICATIONS ====\n");
        ConColor(CC_BLUE);
        int shown = 0;
        for (int i = 0; i < g_appCount && shown < 100; i++) {
            if (filter[0] && !stristr(g_apps[i].name, filter)) continue;
            printf("  %3d) %s", shown + 1, g_apps[i].name);
            if (g_apps[i].ver[0]) printf("  [%s]", g_apps[i].ver);
            printf("\n");
            shown++;
        }
        ConColor(CC_GREEN);
        printf("  [n] uninstall   [f] filter   [q] quit: ");
        fflush(stdout);
        in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        if (in[0] == 'f' || in[0] == 'F') continue;
        int num = atoi(in);
        int matched = 0;
        for (int i = 0; i < g_appCount && matched < num; i++) {
            if (filter[0] && !stristr(g_apps[i].name, filter)) continue;
            matched++;
            if (matched == num) {
                ConColor(CC_GREEN);
                printf("  Launching uninstaller for '%s'...\n", g_apps[i].name);
                ConColor(CC_BLUE);
                LaunchUninstaller(i);
                break;
            }
        }
        if (num <= 0 || matched != num) { ConColor(CC_RED); printf("  Invalid choice.\n"); ConColor(CC_BLUE); }
    }
    ConColor(CC_BLUE);
}

static void FindConsole(void) {
    char root[300], mask[100];
    for (;;) {
        ConColor(CC_GREEN);
        printf("\n  Root folder [C:\\]: ");
        fflush(stdout);
        const char *in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        if (in[0] == 0) strcpy(root, "C:\\");
        else {
            strncpy(root, in, sizeof root - 1);
            root[sizeof root - 1] = 0;
        }
        ConColor(CC_GREEN);
        printf("  Mask [*]: ");
        fflush(stdout);
        in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        strncpy(mask, in, sizeof mask - 1);
        mask[sizeof mask - 1] = 0;
        TrimSpaces(root);
        TrimSpaces(mask);
        size_t rl = strlen(root);
        while (rl > 0 && root[rl - 1] == '\\') root[--rl] = 0;
        if (root[0] == 0) strcpy(root, "C:");
        ConColor(CC_CYAN);
        printf("  Searching '%s' under %s ...\n", mask[0] ? mask : "*", root);
        ConColor(CC_BLUE);
        g_fndStop = 0;
        g_fndCount = 0;
        FindWorkerRec(root, 0);
        ConColor(CC_GREEN);
        printf("  %d matches.\n", g_fndCount);
        ConColor(CC_BLUE);
        if (g_fndCount > 0) {
            for (int i = 0; i < g_fndCount && i < 50; i++)
                printf("  %3d) %s\n", i + 1, g_fndPaths[i]);
            if (g_fndCount > 50) printf("  ... %d more\n", g_fndCount - 50);
            ConColor(CC_GREEN);
            printf("  [n] open   [d] open folder   [s] new search   [q] quit: ");
            fflush(stdout);
            in = AskArgCli("");
            if (!in) return;
            if (in[0] == 'q' || in[0] == 'Q') break;
            if (in[0] == 's' || in[0] == 'S') continue;
            int num = atoi(in);
            if (num < 1 || num > g_fndCount) { ConColor(CC_RED); printf("  Invalid choice.\n"); ConColor(CC_BLUE); continue; }
            if (in[0] == 'd' || in[0] == 'D') {
                char arg[700];
                sprintf(arg, "/select,\"%s\"", g_fndPaths[num - 1]);
                ShellExecuteA(NULL, "open", "explorer.exe", arg, NULL, SW_SHOWNORMAL);
            } else {
                ShellExecuteA(NULL, "open", g_fndPaths[num - 1], NULL, NULL, SW_SHOWNORMAL);
            }
        }
    }
    ConColor(CC_BLUE);
}

static void WifiConsole(void) {
    for (;;) {
        ConColor(CC_CYAN);
        printf("\n  ==== WI-FI MANAGER ====\n");
        ConColor(CC_GREEN);
        printf("    1) Scan networks\n");
        printf("    2) Show saved profiles\n");
        printf("    3) Connect to a network\n");
        printf("    4) Show networks detail (mode=bssid)\n");
        printf("    q) Quit\n");
        ConColor(CC_CYAN);
        printf("  Choice: ");
        fflush(stdout);
        const char *in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        if (in[0] == '1' || in[0] == '2' || in[0] == '3' || in[0] == '4') {
            char cmd[420];
            if (in[0] == '1') strcpy(cmd, "netsh wlan show networks");
            else if (in[0] == '2') strcpy(cmd, "netsh wlan show profiles");
            else if (in[0] == '4') strcpy(cmd, "netsh wlan show networks mode=bssid");
            else {
                ConColor(CC_GREEN);
                printf("  Network name: ");
                fflush(stdout);
                in = AskArgCli("");
                if (!in || !in[0]) continue;
                sprintf(cmd, "netsh wlan connect name=\"%s\"", in);
            }
            g_wifiOut[0] = 0;
            RunCommandCapture(cmd, WifiCollect);
            ConColor(CC_GREEN);
            printf("%s", g_wifiOut);
            ConColor(CC_BLUE);
        } else {
            ConColor(CC_RED);
            printf("  Invalid choice.\n");
            ConColor(CC_BLUE);
        }
    }
    ConColor(CC_BLUE);
}

static void ProcConsole(void) {
    ProcRec top[20];
    for (;;) {
        int n = ProcTop(top, 20);
        ConColor(CC_CYAN);
        printf("\n  ==== TOP PROCESSES BY MEMORY ====\n");
        ConColor(CC_BLUE);
        if (n == 0) { ConColor(CC_RED); printf("  (snapshot failed)\n"); ConColor(CC_BLUE); }
        for (int i = 0; i < n; i++)
            printf("  %2d) %-40s %8.1f MB   (PID %u)\n", i + 1, top[i].name,
                   (double)top[i].mem / 1048576.0, top[i].pid);
        ConColor(CC_GREEN);
        printf("  [n] kill process   [r] refresh   [q] quit: ");
        fflush(stdout);
        const char *in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        if (in[0] == 'r' || in[0] == 'R') continue;
        int num = atoi(in);
        if (num < 1 || num > n) { ConColor(CC_RED); printf("  Invalid choice.\n"); ConColor(CC_BLUE); continue; }
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, top[num - 1].pid);
        if (!hp) {
            ConColor(CC_RED);
            printf("  Cannot open process (access denied).\n");
            ConColor(CC_BLUE);
        } else {
            if (!TerminateProcess(hp, 1)) {
                ConColor(CC_RED);
                printf("  Failed to terminate.\n");
                ConColor(CC_BLUE);
            } else {
                ConColor(CC_GREEN);
                printf("  Terminated PID %u (%s).\n", top[num - 1].pid, top[num - 1].name);
                ConColor(CC_BLUE);
            }
            CloseHandle(hp);
        }
    }
    ConColor(CC_BLUE);
}

void OnToolsMenu(const char *arg) {
    (void)arg;
    if (!g_consoleMode) {
        Notify("The interactive Tools menu runs from CLI or TUI mode.\n"
               "In the GUI use the Monitor / Apps / Find / Wi-Fi / Procs buttons.");
        return;
    }
    for (;;) {
        ConColor(CC_CYAN);
        printf("\n  ==== SYSTEM TOOLS ====\n");
        ConColor(CC_GREEN);
        printf("    1) Live system monitor\n");
        printf("    2) System stats\n");
        printf("    3) App uninstaller\n");
        printf("    4) List installed apps\n");
        printf("    5) File search\n");
        printf("    6) Wi-Fi manager\n");
        printf("    7) Process manager\n");
        printf("    8) Process list\n");
        printf("    9) Net speed\n");
        ConColor(CC_CYAN);
        printf("    q) Quit\n");
        printf("  Choice: ");
        fflush(stdout);
        const char *in = AskArgCli("");
        if (!in) return;
        if (in[0] == 'q' || in[0] == 'Q') break;
        int num = atoi(in);
        switch (num) {
            case 1: MonitorConsole(); break;
            case 2: OnSystemStats(NULL); break;
            case 3: UninsConsole(); break;
            case 4: OnListApps(NULL); break;
            case 5: FindConsole(); break;
            case 6: WifiConsole(); break;
            case 7: ProcConsole(); break;
            case 8: OnProcessList(NULL); break;
            case 9: OnNetSpeed(NULL); break;
            default:
                ConColor(CC_RED);
                printf("  Invalid choice.\n");
                ConColor(CC_BLUE);
                break;
        }
    }
    ConColor(CC_BLUE);
}


/* ==========================================================================
   QT BRIDGE - C-linkage shims used by the Qt front-end (main.cpp).
   The Qt app is GUI mode (g_consoleMode == 0): tool commands open their
   Win32 windows, system-stats/process-list/net-speed print to the attached
   console when present. UiSampleStats feeds the live status chips.
   ========================================================================== */

int QcCategoryCount(void) { return g_categoryCount; }

const char *QcCategoryName(int cat) {
    if (cat < 0 || cat >= g_categoryCount) return "";
    return g_Categories[cat].name;
}

int QcCategoryCommandCount(int cat) {
    if (cat < 0 || cat >= g_categoryCount) return 0;
    return g_Categories[cat].count;
}

const char *QcCommandName(int cat, int idx) {
    if (cat < 0 || cat >= g_categoryCount) return "";
    if (idx < 0 || idx >= g_Categories[cat].count) return "";
    return g_Categories[cat].items[idx].name;
}

const char *QcCommandDesc(int cat, int idx) {
    if (cat < 0 || cat >= g_categoryCount) return "";
    if (idx < 0 || idx >= g_Categories[cat].count) return "";
    return g_Categories[cat].items[idx].desc;
}

int QcCommandNeedsArg(int cat, int idx) {
    if (cat < 0 || cat >= g_categoryCount) return 0;
    if (idx < 0 || idx >= g_Categories[cat].count) return 0;
    return g_Categories[cat].items[idx].needsArg;
}

const char *QcCommandDetail(int cat, int idx) {
    static char qd[1200];
    if (cat < 0 || cat >= g_categoryCount) return "";
    if (idx < 0 || idx >= g_Categories[cat].count) return "";
    const char *d = CmdDetail(g_Categories[cat].name, g_Categories[cat].items[idx].name);
    if (!d) return "";
    snprintf(qd, sizeof qd, "%s", d);
    return qd;
}

int QcTotalCommands(void) {
    int total = 0;
    for (int i = 0; i < g_categoryCount; i++) total += g_Categories[i].count;
    return total;
}

void QcRunCommand(int cat, int idx, const char *arg) {
    if (cat < 0 || cat >= g_categoryCount) return;
    if (idx < 0 || idx >= g_Categories[cat].count) return;
    g_Categories[cat].items[idx].fn(arg);
}

void QcSample(void) { UiSampleStats(); }

double QcCpuPct(void) { return g_cpuPct; }

double QcRamPct(void) { return g_ramPct; }

double QcDiskPct(void) { return g_diskUsedPct; }

double QcNetDown(void) { return g_netDown; }

double QcNetUp(void) { return g_netUp; }

const char *QcUptime(void) { return GetUptimeStr(); }

/* 0 monitor, 1 uninstall, 2 find, 3 wifi, 4 processes, 5 stats,
   6 list-apps, 7 process-list, 8 net-speed */
void QcOpenTool(int tool) {
    switch (tool) {
        case 0:  OnMonitorTool(NULL);   break;
        case 1:  OnUninstallTool(NULL); break;
        case 2:  OnFindTool(NULL);      break;
        case 3:  OnWifiTool(NULL);      break;
        case 4:  OnProcessTool(NULL);   break;
        case 5:  OnSystemStats(NULL);   break;
        case 6:  OnListApps(NULL);      break;
        case 7:  OnProcessList(NULL);   break;
        case 8:  OnNetSpeed(NULL);      break;
        default: break;
    }
}

static void (*g_qcTermFn)(const char *) = NULL;
int g_qcTermClose = 0;

static void QcTermOut(const char *line) {
    if (g_qcTermFn) g_qcTermFn(line);
}

void QcTermExec(const char *line, void (*fn)(const char *)) {
    int closeFlag = 0;
    g_qcTermClose = 0;
    g_qcTermFn = fn;
    TermExecInternal(line, QcTermOut, &closeFlag);
    g_qcTermFn = NULL;
    if (closeFlag) g_qcTermClose = 1;
}
