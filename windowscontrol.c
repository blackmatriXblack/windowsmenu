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
    ShellExecuteA(NULL, admin ? "runas" : "open", file, args, NULL, SW_SHOWNORMAL);
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
    GetSelfPath(self, MAX_PATH);
    ShellExecuteA(NULL, "runas", self, cliArgs, NULL, SW_SHOWNORMAL);
}

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
        sprintf(cmd, "\"%s\"", destExe);
        WriteRegString(HKEY_CURRENT_USER,
                       "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       APP_NAME, cmd);
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

    DeleteRegValue(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Run", APP_NAME);
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
    const char *runKeys[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
    };
    for (int r = 0; r < 2; r++) {
        for (int k = 0; k < 2; k++) {
            HKEY hk;
            if (RegOpenKeyExA(roots[r].root, runKeys[k], 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
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
                ReportAdd("  [%s\\%s] %s = %s", roots[r].name, runKeys[k], vn, (char *)vd);
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
    const char *runKeys[] = {
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        "Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
    };
    for (int k = 0; k < 2; k++) {
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

static int HttpGetText(const char *host, const char *path, char *out, int outSize) {
    out[0] = 0;
    BOOL ok = FALSE;
    HINTERNET hS = WinHttpOpen(L"WindowsControl/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
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
    ReportAdd("Battery saver active: %s", sps.SystemStatusFlag ? "yes" : "no");
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
    sprintf(msg, APP_NAME " v" APP_VERSION "\n"
            "An all-in-one launcher for Windows features and settings.\n"
            "Written in pure C using the Win32 API.\n\n"
            "Includes:\n"
"  - 31 categories, 1000+ commands\n"
            "  - Services, processes and startup managers\n"
            "  - Registry explorer (read / write / delete / search)\n"
            "  - 460 Windows Settings pages (ms-settings)\n"
            "  - Network tools: ping, traceroute, Wi-Fi, DNS, firewall\n"
            "  - System tweaks: theme, taskbar, clock, power, UAC\n"
            "  - Power plans, battery status, sleep / hibernate\n"
            "  - Window & desktop tools, virtual desktops, snapping\n"
            "  - Accessibility: Sticky / Toggle / Filter / Mouse keys\n"
            "  - Storage tools: drives, defrag, disk check, cleanup\n"
            "  - Optimization: temp cleanup, Superfetch, indexing\n"
            "  - File tools: search, zip, base64, clipboard, hashing\n"
            "  - System info and hardware reports\n"
            "  - Utilities: hashing, screenshots, wallpaper, volume\n\n"
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

void PrintHelp(void) {
    printf("\n" APP_NAME " v" APP_VERSION " - All-in-one Windows control center\n");
printf("Pure C / Win32. 31 categories, 1000+ commands, English only.\n\n");
    printf("USAGE:\n");
    printf("  " PROG_NAME "              Run GUI mode (buttons)\n");
    printf("  " PROG_NAME " --gui       Run GUI mode\n");
    printf("  " PROG_NAME " --float     Run the always-on-top floating widget\n");
    printf("  " PROG_NAME " --tui       Run text-menu mode (type numbers)\n");
    printf("  " PROG_NAME " --cli       Start interactive CLI prompt\n");
    printf("  " PROG_NAME " --cli <category> <command> [argument]  Run one command\n");
    printf("  " PROG_NAME " --cli list [category]        List commands\n");
    printf("  " PROG_NAME " --install   One-click install into this computer\n");
    printf("  " PROG_NAME " --uninstall Remove the install\n");
    printf("  " PROG_NAME " --list      Print all categories\n");
    printf("  " PROG_NAME " --version   Print version\n");
    printf("\nEXAMPLE:\n");
    printf("  " PROG_NAME " --cli system device-manager\n");
    printf("  " PROG_NAME " --cli network flush-dns\n");
    printf("  " PROG_NAME " --cli services services-stop spooler\n");
    printf("  " PROG_NAME " --cli registry registry-read \"HKCU\\Software\\WindowsControlCenter|Installed\"\n");
    printf("  " PROG_NAME " --cli list network\n");
    printf("\nARGUMENT COMMANDS (need a value after the command):\n");
    printf("  services-*, processes-kill*, startup-remove, registry-*,\n");
    printf("  volume-set, shutdown-timer, open-file\n");
    printf("\nINTERACTIVE CLI COMMANDS:\n");
    printf("  help, list [category], <category> <command> [argument],\n");
    printf("  install, uninstall, open-gui, exit\n");
}

void PrintAllCategories(void) {
    printf("\nCategories (%d):\n", g_categoryCount);
    for (int i = 0; i < g_categoryCount; i++)
        printf("  %-28s %d commands\n", g_Categories[i].name, g_Categories[i].count);
    int total = 0;
    for (int i = 0; i < g_categoryCount; i++) total += g_Categories[i].count;
    printf("\nTotal: %d commands\n", total);
    printf("Type: " PROG_NAME " --cli list <category> to see commands.\n");
}

void PrintCategoryList(const char *catName) {
    const Category *c = FindCategory(catName);
    if (!c) { printf("Unknown category '%s'. Use 'list' to see all.\n", catName); return; }
    printf("\n[%s] (%d commands)\n", c->name, c->count);
    for (int i = 0; i < c->count; i++)
        printf("  %-24s %s%s\n", c->items[i].name, c->items[i].desc,
               c->items[i].needsArg ? "  [ARG]" : "");
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
    if (stricmp(tokens[0], "exit") == 0 || stricmp(tokens[0], "quit") == 0) return;

    if (n >= 2) {
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
    printf("\n" APP_NAME " v" APP_VERSION " - CLI Mode\n");
    printf("Type: help | list [category] | <category> <command> [argument] | install | uninstall | exit\n");

    if (argc >= 1) {
        RunCliTokens(argc, argv);
        return;
    }

    g_cliInteractive = 1;
    char line[512];
    for (;;) {
        printf("\nwc> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        char *tokens[16];
        int n = tokenizeLine(line, tokens, 16);
        if (n == 0) continue;
        if (stricmp(tokens[0], "exit") == 0 || stricmp(tokens[0], "quit") == 0) break;
        RunCliTokens(n, tokens);
    }
    printf("Goodbye.\n");
}

void RunTui(void) {
    EnsureConsole();
    char line[128];
    printf("\n" APP_NAME " v" APP_VERSION "\n");
    printf("An all-in-one launcher for Windows features and settings.\n");
    printf("Type a number to choose, 0 to go back or exit.\n");

    for (;;) {
        printf("\n==================== MAIN MENU ====================\n");
        printf("  0) Exit\n");
        for (int i = 0; i < g_categoryCount; i++)
            printf("%3d) %s\n", i + 1, g_Categories[i].name);
        printf("===================================================\n");
        printf("Enter category number: ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) return;
        int cat = atoi(line);
        if (cat == 0) break;
        if (cat < 1 || cat > g_categoryCount) { printf("Invalid choice.\n"); continue; }
        int idx = cat - 1;
        for (;;) {
            printf("\n------------------- %s -------------------\n", g_Categories[idx].name);
            printf("  0) Back to main menu\n");
            for (int j = 0; j < g_Categories[idx].count; j++)
                printf("%3d) %s  ->  %s%s\n", j + 1, g_Categories[idx].items[j].name,
                       g_Categories[idx].items[j].desc,
                       g_Categories[idx].items[j].needsArg ? "  [ARG]" : "");
            printf("-----------------------------------------------\n");
            printf("Enter command number: ");
            fflush(stdout);
            if (!fgets(line, sizeof line, stdin)) return;
            int cmd = atoi(line);
            if (cmd == 0) break;
            if (cmd < 1 || cmd > g_Categories[idx].count) { printf("Invalid choice.\n"); continue; }
            const char *arg = NULL;
            if (g_Categories[idx].items[cmd - 1].needsArg) {
                printf("Argument: ");
                fflush(stdout);
                if (!fgets(g_argBuf, sizeof g_argBuf, stdin)) return;
                size_t l = strlen(g_argBuf);
                while (l > 0 && (g_argBuf[l - 1] == '\n' || g_argBuf[l - 1] == '\r'))
                    g_argBuf[--l] = 0;
                arg = g_argBuf;
            }
            g_Categories[idx].items[cmd - 1].fn(arg);
            printf("\nDone. Press Enter to continue...");
            fflush(stdout);
            while (fgets(line, sizeof line, stdin)) {
                if (strlen(line) <= 1) break;
            }
        }
    }
    printf("Goodbye.\n");
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

void GuiRefreshStatus(void);
void RunFloat(void);

void GuiRefreshCommands(void) {
    SendMessageA(g_hLbCmd, LB_RESETCONTENT, 0, 0);
    int sel = (int)SendMessageA(g_hLbCat, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_categoryCount) {
        SetWindowTextA(g_hStatus, "Select a category and a command, then click Run (or double-click).");
        return;
    }
    char text[128];
    for (int j = 0; j < g_Categories[sel].count; j++) {
        sprintf(text, "%d. %s%s", j + 1, g_Categories[sel].items[j].name,
                g_Categories[sel].items[j].needsArg ? "  [ARG]" : "");
        int pos = (int)SendMessageA(g_hLbCmd, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessageA(g_hLbCmd, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)j);
    }
    SendMessageA(g_hLbCmd, LB_SETCURSEL, 0, 0);
    GuiRefreshStatus();
}

void GuiRefreshStatus(void) {
    int cs = (int)SendMessageA(g_hLbCat, LB_GETCURSEL, 0, 0);
    int ci = (int)SendMessageA(g_hLbCmd, LB_GETCURSEL, 0, 0);
    if (cs < 0 || cs >= g_categoryCount) return;
    char text[800];
    if (ci >= 0) {
        int item = (int)SendMessageA(g_hLbCmd, LB_GETITEMDATA, (WPARAM)ci, 0);
        if (item >= 0 && item < g_Categories[cs].count) {
            SetWindowTextA(g_hDesc, g_Categories[cs].items[item].desc);
            sprintf(text, "Category: %s    Command: %s",
                    g_Categories[cs].name, g_Categories[cs].items[item].name);
            if (g_Categories[cs].items[item].needsArg) {
                char hint[128];
                sprintf(hint, "Argument: %s", g_Categories[cs].items[item].name);
                SetWindowTextA(g_hArgEdit, "");
                sprintf(text, "Category: %s    Command: %s    (needs an argument - type it below)",
                        g_Categories[cs].name, g_Categories[cs].items[item].name);
                (void)hint;
            }
        } else {
            sprintf(text, "Category: %s", g_Categories[cs].name);
        }
    } else {
        sprintf(text, "Category: %s", g_Categories[cs].name);
    }
    SetWindowTextA(g_hStatus, text);
}

int PromptArg(const char *prompt);

void GuiRunSelected(void) {
    int cs = (int)SendMessageA(g_hLbCat, LB_GETCURSEL, 0, 0);
    int ci = (int)SendMessageA(g_hLbCmd, LB_GETCURSEL, 0, 0);
    if (cs < 0 || cs >= g_categoryCount) return;
    if (ci < 0) return;
    int item = (int)SendMessageA(g_hLbCmd, LB_GETITEMDATA, (WPARAM)ci, 0);
    if (item < 0 || item >= g_Categories[cs].count) return;
    const Command *cmd = &g_Categories[cs].items[item];
    if (cmd->needsArg) {
        GetWindowTextA(g_hArgEdit, g_argInput, ARG_MAX);
        TrimSpaces(g_argInput);
        if (g_argInput[0] == 0) {
            char p[512];
            sprintf(p, "%s\n\nEnter a value, then click OK.", cmd->desc);
            if (!PromptArg(p)) return;
        }
        cmd->fn(g_argInput);
    } else {
        cmd->fn(NULL);
    }
}

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

LRESULT CALLBACK GuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
case WM_CREATE: {
            g_hFont = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");
            HWND hLbl;
            hLbl = CreateWindowA("STATIC", "Categories:", WS_CHILD | WS_VISIBLE,
                                 12, 10, 240, 18, hWnd, NULL, g_hInst, NULL);
            SendMessageA(hLbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            g_hLbCat = CreateWindowA("LISTBOX", NULL,
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                     12, 30, 250, 600, hWnd, (HMENU)IDC_CAT, g_hInst, NULL);
            SendMessageA(g_hLbCat, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            hLbl = CreateWindowA("STATIC", "Commands:", WS_CHILD | WS_VISIBLE,
                                 280, 10, 520, 18, hWnd, NULL, g_hInst, NULL);
            SendMessageA(hLbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            g_hLbCmd = CreateWindowA("LISTBOX", NULL,
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                                     280, 30, 520, 430, hWnd, (HMENU)IDC_CMD, g_hInst, NULL);
            SendMessageA(g_hLbCmd, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            hLbl = CreateWindowA("STATIC", "Description:", WS_CHILD | WS_VISIBLE,
                                 280, 470, 200, 18, hWnd, NULL, g_hInst, NULL);
            SendMessageA(hLbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            g_hDesc = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    280, 490, 520, 44, hWnd, (HMENU)IDC_DESC, g_hInst, NULL);
            SendMessageA(g_hDesc, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            hLbl = CreateWindowA("STATIC", "Argument:", WS_CHILD | WS_VISIBLE,
                                 280, 542, 90, 20, hWnd, NULL, g_hInst, NULL);
            SendMessageA(hLbl, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            g_hArgEdit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                       360, 540, 280, 24, hWnd, (HMENU)IDC_ARG_MAIN, g_hInst, NULL);
            SendMessageA(g_hArgEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            HWND bRun = CreateWindowA("BUTTON", "Run", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      650, 538, 150, 30, hWnd, (HMENU)IDC_BTN_RUN, g_hInst, NULL);
            SendMessageA(bRun, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            g_hStatus = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE,
                                      12, 640, 1048, 52, hWnd, (HMENU)IDC_STATUS, g_hInst, NULL);
            SendMessageA(g_hStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            struct { int id; const char *text; int y; } btns[] = {
                {IDC_BTN_FLOAT, "Floating Widget", 30},
                {IDC_BTN_INSTALL, "Install", 74},
                {IDC_BTN_UNINSTALL, "Uninstall", 118},
                {IDC_BTN_ABOUT, "About", 162},
                {IDC_BTN_EXIT, "Exit", 206},
            };
            for (int i = 0; i < 5; i++) {
                HWND b = CreateWindowA("BUTTON", btns[i].text,
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       820, btns[i].y, 240, 34, hWnd,
                                       (HMENU)(INT_PTR)btns[i].id, g_hInst, NULL);
                SendMessageA(b, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            }
            for (int i = 0; i < g_categoryCount; i++) {
                int pos = (int)SendMessageA(g_hLbCat, LB_ADDSTRING, 0, (LPARAM)g_Categories[i].name);
                SendMessageA(g_hLbCat, LB_SETITEMDATA, (WPARAM)pos, (LPARAM)i);
            }
            SendMessageA(g_hLbCat, LB_SETCURSEL, 0, 0);
            GuiRefreshCommands();
            break;
        }
        case WM_COMMAND:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                if (LOWORD(wParam) == IDC_CAT) GuiRefreshCommands();
                else if (LOWORD(wParam) == IDC_CMD) GuiRefreshStatus();
            } else if (HIWORD(wParam) == LBN_DBLCLK) {
                if (LOWORD(wParam) == IDC_CMD) GuiRunSelected();
            } else if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case IDC_BTN_RUN: GuiRunSelected(); break;
                    case IDC_BTN_FLOAT: RunFloat(); break;
                    case IDC_BTN_INSTALL: DoInstall(); break;
                    case IDC_BTN_UNINSTALL: DoUninstall(); break;
                    case IDC_BTN_ABOUT: OnUtilAbout(NULL); break;
                    case IDC_BTN_EXIT: DestroyWindow(hWnd); break;
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
            HBRUSH fill = CreateSolidBrush(RGB(28, 76, 160));
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(140, 190, 255));
            HBRUSH oldb = (HBRUSH)SelectObject(hdc, fill);
            HPEN oldp = (HPEN)SelectObject(hdc, pen);
            Ellipse(hdc, 3, 3, 53, 53);
            SelectObject(hdc, oldb);
            SelectObject(hdc, oldp);
            DeleteObject(fill);
            DeleteObject(pen);
            SetBkMode(hdc, TRANSPARENT);
            HFONT f = CreateFontA(-20, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");
            HFONT of = (HFONT)SelectObject(hdc, f);
            SetTextColor(hdc, RGB(255, 255, 255));
            RECT r = { 0, 0, 56, 56 };
            DrawTextA(hdc, "W", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of);
            DeleteObject(f);
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
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WindowsControlCenterClass";
    RegisterClassA(&wc);

    g_hMain = CreateWindowExA(WS_EX_DLGMODALFRAME, "WindowsControlCenterClass",
                              APP_NAME " v" APP_VERSION " - All-in-one Windows Control",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              1080, 740, NULL, NULL, g_hInst, NULL);
    if (!g_hMain) { Notify("Failed to create the main window."); return; }
    ShowWindow(g_hMain, SW_SHOW);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int main(void) {
    g_hInst = GetModuleHandleA(NULL);

    char *raw = GetCommandLineA();
    char line[1024];
    strncpy(line, raw, sizeof line - 1);
    line[sizeof line - 1] = 0;
    char *tokens[16];
    int n = tokenizeLine(line, tokens, 16);
    int argStart = (n > 0) ? 1 : 0;
    int m = n - argStart;
    char **args = (m > 0) ? tokens + argStart : NULL;

    if (m == 0) {
        RunGui();
        return 0;
    }

    if (stricmp(args[0], "-g") == 0 || stricmp(args[0], "--gui") == 0) {
        RunGui();
    } else if (stricmp(args[0], "-t") == 0 || stricmp(args[0], "--tui") == 0) {
        RunTui();
    } else if (stricmp(args[0], "-c") == 0 || stricmp(args[0], "--cli") == 0) {
        RunCli(m - 1, m > 1 ? args + 1 : NULL);
    } else if (stricmp(args[0], "-i") == 0 || stricmp(args[0], "--install") == 0) {
        EnsureConsole();
        DoInstall();
    } else if (stricmp(args[0], "-u") == 0 || stricmp(args[0], "--uninstall") == 0) {
        EnsureConsole();
        DoUninstall();
    } else if (stricmp(args[0], "-h") == 0 || stricmp(args[0], "--help") == 0) {
        EnsureConsole();
        PrintHelp();
    } else if (stricmp(args[0], "-l") == 0 || stricmp(args[0], "--list") == 0) {
        EnsureConsole();
        PrintAllCategories();
    } else if (stricmp(args[0], "-f") == 0 || stricmp(args[0], "--float") == 0) {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
        g_bFloatStandalone = 1;
        RunFloat();
        if (!g_hFloat) return 0;
        MSG fmsg;
        while (GetMessageA(&fmsg, NULL, 0, 0)) {
            TranslateMessage(&fmsg);
            DispatchMessageA(&fmsg);
        }
        g_bFloatStandalone = 0;
        if (g_bOpenMain) RunGui();
    } else if (stricmp(args[0], "-v") == 0 || stricmp(args[0], "--version") == 0) {
        EnsureConsole();
        printf(APP_NAME " v" APP_VERSION "\n");
    } else {
        EnsureConsole();
        printf("Unknown option '%s'.\n", args[0]);
        PrintHelp();
    }
    if (g_launchGui) RunGui();
    return 0;
}