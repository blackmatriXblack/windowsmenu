/*
 * ULTIMATE WINDOWS SYSTEM CONTROL MENU - ENHANCED EDITION
 * Pure C / Win32 API - Over 350 Functional Commands
 * Compile: gcc -mwindows -o UltimateMenu.exe main.c -lshell32 -ladvapi32 -luser32 -lpowrprof
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include <powrprof.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "powrprof.lib")

// ============================================================================
// Constants - Command IDs
// ============================================================================

#define WM_TRAYICON     (WM_APP + 100)
#define ID_TRAYICON     1001

// Base IDs (extended)
#define IDM_FILE_BASE               1000
#define IDM_EDIT_BASE               2000
#define IDM_VIEW_BASE               3000
#define IDM_TOOLS_BASE              4000
#define IDM_SYSTEM_BASE             5000
#define IDM_CONTROL_PANEL_BASE      6000
#define IDM_SETTINGS_BASE           7000
#define IDM_NETWORK_BASE            8000
#define IDM_POWER_BASE              9000
#define IDM_DISPLAY_BASE           10000
#define IDM_AUDIO_BASE             11000
#define IDM_SECURITY_BASE          12000
#define IDM_ACCESSIBILITY_BASE     13000
#define IDM_DEVELOPER_BASE         14000
#define IDM_ADMIN_BASE             15000
#define IDM_APPS_BASE              16000
#define IDM_SHELL_BASE             17000
#define IDM_TOP_BASE               18000
#define IDM_MAINTENANCE_BASE       19000
#define IDM_TROUBLESHOOT_BASE      20000
#define IDM_RUN_DIALOG_BASE        21000
#define IDM_SPECIAL_FOLDERS_BASE   22000
#define IDM_HARDWARE_BASE          23000
#define IDM_DIAGNOSTICS_BASE       24000
#define IDM_WINDOWS_FEATURES_BASE  25000
#define IDM_ADVANCED_TOOLS_BASE    26000
#define IDM_SYSCONFIG_BASE         27000
#define IDM_NETADV_BASE            28000
#define IDM_SECURITYADV_BASE       29000
#define IDM_DEVDIAG_BASE           30000
#define IDM_MORESETTINGS_BASE      31000
#define IDM_ADVSYSTEM_BASE         32000
#define IDM_MSC_SNAPINS_BASE       33000
#define IDM_SHELLFOLDERS2_BASE     34000
#define IDM_NETCMDS_BASE           35000
#define IDM_RECOVERY_BASE          36000
#define IDM_WSL_BASE               37000
#define IDM_GUI_BASE               38000

// File menu
enum {
    IDM_FILE_NEW_FOLDER = IDM_FILE_BASE + 1,
    IDM_FILE_NEW_TEXT,
    IDM_FILE_OPEN_EXPLORER,
    IDM_FILE_OPEN_COMPUTER,
    IDM_FILE_OPEN_DOCUMENTS,
    IDM_FILE_OPEN_DOWNLOADS,
    IDM_FILE_OPEN_DESKTOP,
    IDM_FILE_OPEN_PICTURES,
    IDM_FILE_OPEN_MUSIC,
    IDM_FILE_OPEN_VIDEOS,
    IDM_FILE_OPEN_ONEDRIVE,
    IDM_FILE_OPEN_NETWORK,
    IDM_FILE_OPEN_RECYCLE,
    IDM_FILE_OPEN_CMD,
    IDM_FILE_OPEN_NOTEPAD,
    IDM_FILE_OPEN_CALC,
    IDM_FILE_OPEN_PAINT,
    IDM_FILE_SAVE,
    IDM_FILE_SAVE_AS,
    IDM_FILE_PRINT,
    IDM_FILE_PROPERTIES,
    IDM_FILE_EXIT
};

// Edit menu
enum {
    IDM_EDIT_UNDO = IDM_EDIT_BASE + 1,
    IDM_EDIT_REDO,
    IDM_EDIT_CUT,
    IDM_EDIT_COPY,
    IDM_EDIT_PASTE,
    IDM_EDIT_DELETE,
    IDM_EDIT_SELECT_ALL,
    IDM_EDIT_FIND
};

// View menu
enum {
    IDM_VIEW_REFRESH = IDM_VIEW_BASE + 1,
    IDM_VIEW_FULLSCREEN,
    IDM_VIEW_ZOOM_IN,
    IDM_VIEW_ZOOM_OUT,
    IDM_VIEW_ZOOM_100,
    IDM_VIEW_SHOW_DESKTOP,
    IDM_VIEW_TASKBAR_SETTINGS
};

// Tools menu
enum {
    IDM_TOOLS_OPTIONS = IDM_TOOLS_BASE + 1,
    IDM_TOOLS_LANGUAGE,
    IDM_TOOLS_CHECK_UPDATES,
    IDM_TOOLS_CLEAN_DISK,
    IDM_TOOLS_DEFRAG,
    IDM_TOOLS_DEVICE_MANAGER,
    IDM_TOOLS_DISK_MANAGEMENT,
    IDM_TOOLS_EVENT_VIEWER,
    IDM_TOOLS_SERVICES,
    IDM_TOOLS_TASK_SCHEDULER,
    IDM_TOOLS_REGISTRY,
    IDM_TOOLS_GROUP_POLICY,
    IDM_TOOLS_COMPUTER_MANAGEMENT,
    IDM_TOOLS_SYSTEM_INFO,
    IDM_TOOLS_PERFORMANCE,
    IDM_TOOLS_RESOURCE_MONITOR
};

// System menu
enum {
    IDM_SYSTEM_ABOUT = IDM_SYSTEM_BASE + 1,
    IDM_SYSTEM_PROPERTIES,
    IDM_SYSTEM_ADVANCED,
    IDM_SYSTEM_DEVICE_MANAGER,
    IDM_SYSTEM_DISK_MANAGEMENT,
    IDM_SYSTEM_COMPUTER_MANAGEMENT,
    IDM_SYSTEM_SERVICES,
    IDM_SYSTEM_TASK_SCHEDULER,
    IDM_SYSTEM_EVENT_VIEWER,
    IDM_SYSTEM_REGISTRY,
    IDM_SYSTEM_GROUP_POLICY,
    IDM_SYSTEM_SYSTEM_INFO,
    IDM_SYSTEM_MSINFO32,
    IDM_SYSTEM_DXDIAG
};

// Control Panel
enum {
    IDM_CP_ALL = IDM_CONTROL_PANEL_BASE + 1,
    IDM_CP_PROGRAMS,
    IDM_CP_FEATURES,
    IDM_CP_DATE_TIME,
    IDM_CP_REGION,
    IDM_CP_KEYBOARD,
    IDM_CP_MOUSE,
    IDM_CP_SOUND,
    IDM_CP_FONTS,
    IDM_CP_FOLDER_OPTIONS,
    IDM_CP_INTERNET_OPTIONS,
    IDM_CP_USER_ACCOUNTS,
    IDM_CP_POWER_OPTIONS,
    IDM_CP_FIREWALL,
    IDM_CP_ADMIN_TOOLS
};

// Settings
enum {
    IDM_SET_HOME = IDM_SETTINGS_BASE + 1,
    IDM_SET_DISPLAY,
    IDM_SET_SOUND,
    IDM_SET_NOTIFICATIONS,
    IDM_SET_POWER,
    IDM_SET_STORAGE,
    IDM_SET_ABOUT,
    IDM_SET_BLUETOOTH,
    IDM_SET_PRINTERS,
    IDM_SET_MOUSE,
    IDM_SET_NETWORK,
    IDM_SET_WIFI,
    IDM_SET_VPN,
    IDM_SET_PROXY,
    IDM_SET_BACKGROUND,
    IDM_SET_COLORS,
    IDM_SET_LOCK_SCREEN,
    IDM_SET_THEMES,
    IDM_SET_APPS,
    IDM_SET_ACCOUNTS,
    IDM_SET_TIME,
    IDM_SET_LANGUAGE,
    IDM_SET_GAMING,
    IDM_SET_ACCESSIBILITY,
    IDM_SET_PRIVACY,
    IDM_SET_UPDATE,
    IDM_SET_RECOVERY
};

// Network
enum {
    IDM_NET_NETWORK_SETTINGS = IDM_NETWORK_BASE + 1,
    IDM_NET_ADAPTER_SETTINGS,
    IDM_NET_WIFI_SETTINGS,
    IDM_NET_VPN_SETTINGS,
    IDM_NET_PROXY_SETTINGS,
    IDM_NET_IPCONFIG,
    IDM_NET_PING_GOOGLE,
    IDM_NET_TRACERT,
    IDM_NET_NETSTAT,
    IDM_NET_FLUSH_DNS,
    IDM_NET_BROWSER,
    IDM_NET_REMOTE_DESKTOP
};

// Power
enum {
    IDM_POWER_SLEEP = IDM_POWER_BASE + 1,
    IDM_POWER_HIBERNATE,
    IDM_POWER_RESTART,
    IDM_POWER_SHUTDOWN,
    IDM_POWER_SIGNOUT,
    IDM_POWER_LOCK,
    IDM_POWER_POWER_OPTIONS
};

// Display
enum {
    IDM_DISP_SETTINGS = IDM_DISPLAY_BASE + 1,
    IDM_DISP_PERSONALIZATION,
    IDM_DISP_BACKGROUND,
    IDM_DISP_COLORS,
    IDM_DISP_LOCK_SCREEN,
    IDM_DISP_THEMES,
    IDM_DISP_RESOLUTION,
    IDM_DISP_NIGHT_LIGHT
};

// Audio
enum {
    IDM_AUDIO_VOLUME_UP = IDM_AUDIO_BASE + 1,
    IDM_AUDIO_VOLUME_DOWN,
    IDM_AUDIO_VOLUME_MUTE,
    IDM_AUDIO_VOLUME_MIXER,
    IDM_AUDIO_SOUND_SETTINGS,
    IDM_AUDIO_PLAYBACK,
    IDM_AUDIO_RECORDING
};

// Security
enum {
    IDM_SEC_WINDOWS_SECURITY = IDM_SECURITY_BASE + 1,
    IDM_SEC_VIRUS_SCAN,
    IDM_SEC_FIREWALL,
    IDM_SEC_UPDATE_SETTINGS,
    IDM_SEC_BACKUP,
    IDM_SEC_TASK_MANAGER
};

// Accessibility
enum {
    IDM_ACCESS_CENTER = IDM_ACCESSIBILITY_BASE + 1,
    IDM_ACCESS_MAGNIFIER,
    IDM_ACCESS_NARRATOR,
    IDM_ACCESS_ONSCREEN_KEYBOARD
};

// Developer
enum {
    IDM_DEV_CMD = IDM_DEVELOPER_BASE + 1,
    IDM_DEV_CMD_ADMIN,
    IDM_DEV_POWERSHELL,
    IDM_DEV_POWERSHELL_ADMIN,
    IDM_DEV_REGISTRY,
    IDM_DEV_POLICY_EDITOR
};

// Admin
enum {
    IDM_ADMIN_COMPUTER_MGMT = IDM_ADMIN_BASE + 1,
    IDM_ADMIN_DEFRAG,
    IDM_ADMIN_DISK_CLEANUP,
    IDM_ADMIN_CHKDSK,
    IDM_ADMIN_SFC_SCANNOW,
    IDM_ADMIN_DISM,
    IDM_ADMIN_MEMORY_DIAG
};

// Apps
enum {
    IDM_APP_NOTEPAD = IDM_APPS_BASE + 1,
    IDM_APP_WORDPAD,
    IDM_APP_PAINT,
    IDM_APP_CALCULATOR,
    IDM_APP_SNIPPING_TOOL,
    IDM_APP_STICKY_NOTES,
    IDM_APP_EDGE,
    IDM_APP_STORE
};

// Shell
enum {
    IDM_SHELL_RUN = IDM_SHELL_BASE + 1,
    IDM_SHELL_SEARCH,
    IDM_SHELL_TASKMGR,
    IDM_SHELL_LOCK,
    IDM_SHELL_SHOW_DESKTOP,
    IDM_SHELL_SETTINGS,
    IDM_SHELL_FILE_EXPLORER,
    IDM_SHELL_CONTROL_PANEL,
    IDM_SHELL_RECYCLE_BIN,
    IDM_SHELL_STARTUP
};

// Top
enum {
    IDM_TOP_SYSTEM_INFO = IDM_TOP_BASE + 1,
    IDM_TOP_RESTART_EXPLORER,
    IDM_TOP_EMPTY_RECYCLE,
    IDM_TOP_EDIT_HOSTS,
    IDM_TOP_EDIT_REGISTRY,
    IDM_TOP_ABOUT,
    IDM_TOP_EXIT
};

// Maintenance
enum {
    IDM_MAINT_CLEAN_DISK = IDM_MAINTENANCE_BASE + 1,
    IDM_MAINT_DEFRAG,
    IDM_MAINT_CHKDSK,
    IDM_MAINT_SFC_SCANNOW,
    IDM_MAINT_DISM,
    IDM_MAINT_DISK_CLEANUP,
    IDM_MAINT_SYSTEM_RESTORE,
    IDM_MAINT_WINDOWS_BACKUP,
    IDM_MAINT_TASK_SCHEDULER,
    IDM_MAINT_PRINT_MANAGEMENT,
    IDM_MAINT_SHARED_FOLDERS,
    IDM_MAINT_RELIABILITY_MONITOR
};

// Troubleshooting
enum {
    IDM_TROUBLE_SHOOTER = IDM_TROUBLESHOOT_BASE + 1,
    IDM_TROUBLE_EVENT_VIEWER,
    IDM_TROUBLE_PERFORMANCE_MONITOR,
    IDM_TROUBLE_RESOURCE_MONITOR,
    IDM_TROUBLE_MEMORY_DIAG,
    IDM_TROUBLE_STARTUP_REPAIR,
    IDM_TROUBLE_SYSTEM_CONFIG,
    IDM_TROUBLE_DIRECTX_DIAG,
    IDM_TROUBLE_PROBLEM_STEPS
};

// Run Dialog
enum {
    IDM_RUN_CMD = IDM_RUN_DIALOG_BASE + 1,
    IDM_RUN_POWERSHELL,
    IDM_RUN_REGEDIT,
    IDM_RUN_SERVICES,
    IDM_RUN_TASKMGR,
    IDM_RUN_SYSTEM_INFO,
    IDM_RUN_MSINFO32,
    IDM_RUN_WINVER
};

// Special Folders
enum {
    IDM_FOLDER_APPDATA = IDM_SPECIAL_FOLDERS_BASE + 1,
    IDM_FOLDER_LOCALAPPDATA,
    IDM_FOLDER_PROGRAMDATA,
    IDM_FOLDER_STARTUP,
    IDM_FOLDER_TEMP,
    IDM_FOLDER_WINDOWS,
    IDM_FOLDER_SYSTEM32,
    IDM_FOLDER_SENDTO,
    IDM_FOLDER_QUICK_LAUNCH
};

// Hardware
enum {
    IDM_HW_DEVICE_MANAGER = IDM_HARDWARE_BASE + 1,
    IDM_HW_PRINTERS,
    IDM_HW_GAME_CONTROLLERS,
    IDM_HW_MOUSE,
    IDM_HW_KEYBOARD,
    IDM_HW_SOUND,
    IDM_HW_DISPLAY
};

// Diagnostics
enum {
    IDM_DIAG_DIRECTX = IDM_DIAGNOSTICS_BASE + 1,
    IDM_DIAG_WINDOWS_MEMORY,
    IDM_DIAG_RESOURCE_MONITOR,
    IDM_DIAG_PERFORMANCE_MONITOR,
    IDM_DIAG_RELIABILITY,
    IDM_DIAG_SYSTEM_INFO,
    IDM_DIAG_MSINFO32
};

// Windows Features
enum {
    IDM_WF_TURN_FEATURES = IDM_WINDOWS_FEATURES_BASE + 1,
    IDM_WF_OPTIONAL_FEATURES,
    IDM_WF_INTERNET_EXPLORER,
    IDM_WF_MEDIA_PLAYER,
    IDM_WF_MICROSOFT_STORE,
    IDM_WF_XBOX_GAME_BAR
};

// Advanced Tools
enum {
    IDM_ADV_LUSRMGR = IDM_ADVANCED_TOOLS_BASE + 1,
    IDM_ADV_SECPOL,
    IDM_ADV_WFIREWALL,
    IDM_ADV_CERTMGR,
    IDM_ADV_ODBC,
    IDM_ADV_COMSERVICES,
    IDM_ADV_TPM,
    IDM_ADV_BITLOCKER,
    IDM_ADV_CREDENTIAL_MANAGER,
    IDM_ADV_AUTOPLAY,
    IDM_ADV_COLOR_MANAGEMENT,
};

// System Configuration
enum {
    IDM_SYSCFG_ENVVAR = IDM_SYSCONFIG_BASE + 1,
    IDM_SYSCFG_PERFOPT,
    IDM_SYSCFG_USERPROF,
    IDM_SYSCFG_STARTREC,
    IDM_SYSCFG_HARDWAREPROF,
    IDM_SYSCFG_REMOTE,
    IDM_SYSCFG_SYSPROTECTION,
};

// Advanced Network
enum {
    IDM_NETADV_NCPA = IDM_NETADV_BASE + 1,
    IDM_NETADV_SHARINGCENTER,
    IDM_NETADV_ADVSHARING,
    IDM_NETADV_WIFISENSE,
    IDM_NETADV_NETRESET,
    IDM_NETADV_FIREWALLRULES,
    IDM_NETADV_INETPROPERTIES,
};

// Advanced Security
enum {
    IDM_SECADV_DEFENDER = IDM_SECURITYADV_BASE + 1,
    IDM_SECADV_VIRUSTHREAT,
    IDM_SECADV_FIREWALLPROT,
    IDM_SECADV_APPBROWSER,
    IDM_SECADV_DEVICESEC,
    IDM_SECADV_DEVICEPERF,
    IDM_SECADV_FAMILY,
};

// Developer & Diagnostics
enum {
    IDM_DEVDIAG_DEVENV = IDM_DEVDIAG_BASE + 1,
    IDM_DEVDIAG_WSL,
    IDM_DEVDIAG_WINTERMINAL,
    IDM_DEVDIAG_PERFMON,
    IDM_DEVDIAG_RELIA,
    IDM_DEVDIAG_STEPSREC,
    IDM_DEVDIAG_WPR,
    IDM_DEVDIAG_GPEDIT,
};

// More Settings
enum {
    IDM_MORE_PEN = IDM_MORESETTINGS_BASE + 1,
    IDM_MORE_AUTOPLAY,
    IDM_MORE_PHONELINK,
    IDM_MORE_NEARBY,
    IDM_MORE_CLIPBOARD,
    IDM_MORE_FOCUSASSIST,
    IDM_MORE_PROJECTING,
    IDM_MORE_TABLETMODE,
};

// Advanced System
enum {
    IDM_ADVSYS_WINTOOLS = IDM_ADVSYSTEM_BASE + 1,
    IDM_ADVSYS_HYPERV,
    IDM_ADVSYS_ISCSI,
    IDM_ADVSYS_STORAGESPACES,
    IDM_ADVSYS_CLEANMGR_ADV,
    IDM_ADVSYS_TASKMGR_DETAILS,
    IDM_ADVSYS_RESMON,
    IDM_ADVSYS_MSINFO32,
};

// MSC Snap-ins
enum {
    IDM_MSC_SERVICES = IDM_MSC_SNAPINS_BASE + 1,
    IDM_MSC_EVENTVWR,
    IDM_MSC_TASKSCHED,
    IDM_MSC_DEVMGMT,
    IDM_MSC_DISKMGMT,
    IDM_MSC_COMPMGMT,
    IDM_MSC_PERFMON,
    IDM_MSC_WFIREWALL,
    IDM_MSC_SECPOL,
    IDM_MSC_GPEDIT,
    IDM_MSC_LUSRMGR,
    IDM_MSC_CERTMGR,
    IDM_MSC_PRINTMGMT,
    IDM_MSC_SHAREDFOLDERS,
    IDM_MSC_TPM,
    IDM_MSC_HYPERV,
};

// Shell Folders 2
enum {
    IDM_SHELL2_RECENT = IDM_SHELLFOLDERS2_BASE + 1,
    IDM_SHELL2_QUICKLAUNCH,
    IDM_SHELL2_NETHOOD,
    IDM_SHELL2_PRINTHOOD,
    IDM_SHELL2_TEMPLATES,
    IDM_SHELL2_FAVORITES,
    IDM_SHELL2_COOKIES,
    IDM_SHELL2_HISTORY,
};

// Network Commands
enum {
    IDM_NETCMD_IPCONFIG_ALL = IDM_NETCMDS_BASE + 1,
    IDM_NETCMD_PING_GOOGLE,
    IDM_NETCMD_TRACERT_GOOGLE,
    IDM_NETCMD_NSLOOKUP,
    IDM_NETCMD_NETSTAT_AN,
    IDM_NETCMD_NETSTAT_B,
    IDM_NETCMD_ROUTE_PRINT,
    IDM_NETCMD_ARP_A,
    IDM_NETCMD_GETMAC,
    IDM_NETCMD_NBTSTAT,
    IDM_NETCMD_FLUSHDNS,
    IDM_NETCMD_WINSOCK_RESET,
};

// Recovery
enum {
    IDM_RECOVERY_SYSRESTORE = IDM_RECOVERY_BASE + 1,
    IDM_RECOVERY_RESETPC,
    IDM_RECOVERY_ADVSTARTUP,
    IDM_RECOVERY_RECOVERYDRIVE,
    IDM_RECOVERY_SYSTEMIMAGE,
    IDM_RECOVERY_STARTUPREPAIR,
};

// WSL
enum {
    IDM_WSL_LAUNCH = IDM_WSL_BASE + 1,
    IDM_WSL_LIST,
    IDM_WSL_SHUTDOWN,
    IDM_WSL_TERMINATE,
};

// GUI
enum {
    IDM_GUI_OPENPANEL = IDM_GUI_BASE + 1,
};

// ============================================================================
// Global Variables
// ============================================================================
static HINSTANCE g_hInst;
static HWND g_hWnd;
static NOTIFYICONDATA g_nid;
static HWND g_hGuiDlg = NULL;

// ============================================================================
// Forward Declarations
// ============================================================================
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL AddTrayIcon(HWND);
BOOL RemoveTrayIcon(HWND);
void ShowContextMenu(HWND);
void ProcessCommand(WPARAM);
void AppendNewMenus(HMENU);

// Function pointer type and dispatch table
typedef void (*CommandHandler)(void);

typedef struct {
    UINT uID;
    CommandHandler handler;
} CommandEntry;

// Helper functions
void RunSystem(const char* exe) {
    ShellExecute(NULL, "open", exe, NULL, NULL, SW_SHOWNORMAL);
}

void RunCmd(const char* command, int wait) {
    char cmd[512];
    sprintf(cmd, "cmd.exe %s %s", wait ? "/k" : "/c", command);
    system(cmd);
}

void OpenSettings(const char* page) {
    char cmd[512];
    sprintf(cmd, "start ms-settings:%s", page);
    system(cmd);
}

void OpenControlPanel(const char* cpl) {
    char cmd[256];
    sprintf(cmd, "control %s", cpl);
    system(cmd);
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

void OpenShellFolder(const char* folder) {
    char cmd[512];
    sprintf(cmd, "explorer %s", folder);
    system(cmd);
}

void ShowInfo(const char* msg) {
    MessageBox(g_hWnd, msg, "Information", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Command Handlers (abbreviated - full list in dispatch table)
// ============================================================================

// File handlers
void OnFileNewFolder(void) { OpenShellFolder("shell:MyComputerFolder"); }
void OnFileNewText(void) { RunSystem("notepad.exe"); }
void OnFileOpenExplorer(void) { RunSystem("explorer.exe"); }
void OnFileOpenComputer(void) { OpenShellFolder("shell:MyComputerFolder"); }
void OnFileOpenDocuments(void) { OpenShellFolder("shell:Personal"); }
void OnFileOpenDownloads(void) { OpenShellFolder("shell:Downloads"); }
void OnFileOpenDesktop(void) { OpenShellFolder("shell:Desktop"); }
void OnFileOpenPictures(void) { OpenShellFolder("shell:MyPictures"); }
void OnFileOpenMusic(void) { OpenShellFolder("shell:MyMusic"); }
void OnFileOpenVideos(void) { OpenShellFolder("shell:MyVideo"); }
void OnFileOpenOneDrive(void) { OpenShellFolder("shell:OneDrive"); }
void OnFileOpenNetwork(void) { OpenShellFolder("shell:NetworkPlacesFolder"); }
void OnFileOpenRecycle(void) { OpenShellFolder("shell:RecycleBinFolder"); }
void OnFileOpenCmd(void) { RunCmd("", 1); }
void OnFileOpenNotepad(void) { RunSystem("notepad.exe"); }
void OnFileOpenCalc(void) { RunSystem("calc.exe"); }
void OnFileOpenPaint(void) { RunSystem("mspaint.exe"); }
void OnFileSave(void) { ShowInfo("Document saved"); }
void OnFileSaveAs(void) { ShowInfo("Save As dialog"); }
void OnFilePrint(void) { ShellExecute(NULL, "print", "", NULL, NULL, SW_SHOWNORMAL); }
void OnFileProperties(void) { SimulateCombo(VK_MENU, VK_RETURN); }
void OnFileExit(void) { PostMessage(g_hWnd, WM_CLOSE, 0, 0); }

// Edit handlers
void OnEditUndo(void) { SimulateCombo(VK_CONTROL, 'Z'); }
void OnEditRedo(void) { SimulateCombo(VK_CONTROL, 'Y'); }
void OnEditCut(void) { SimulateCombo(VK_CONTROL, 'X'); }
void OnEditCopy(void) { SimulateCombo(VK_CONTROL, 'C'); }
void OnEditPaste(void) { SimulateCombo(VK_CONTROL, 'V'); }
void OnEditDelete(void) { SimulateKey(VK_DELETE); }
void OnEditSelectAll(void) { SimulateCombo(VK_CONTROL, 'A'); }
void OnEditFind(void) { SimulateCombo(VK_CONTROL, 'F'); }

// View handlers
void OnViewRefresh(void) { SimulateKey(VK_F5); }
void OnViewFullscreen(void) { SimulateKey(VK_F11); }
void OnViewZoomIn(void) { SimulateCombo(VK_CONTROL, VK_ADD); }
void OnViewZoomOut(void) { SimulateCombo(VK_CONTROL, VK_SUBTRACT); }
void OnViewZoom100(void) { SimulateCombo(VK_CONTROL, '0'); }
void OnViewShowDesktop(void) { SimulateCombo(VK_LWIN, 'D'); }
void OnViewTaskbarSettings(void) { OpenSettings("taskbar"); }

// Tools handlers
void OnToolsOptions(void) { OpenControlPanel(""); }
void OnToolsLanguage(void) { OpenSettings("regionlanguage"); }
void OnToolsCheckUpdates(void) { OpenSettings("windowsupdate"); }
void OnToolsCleanDisk(void) { RunSystem("cleanmgr.exe"); }
void OnToolsDefrag(void) { RunSystem("dfrgui.exe"); }
void OnToolsDeviceManager(void) { RunSystem("devmgmt.msc"); }
void OnToolsDiskManagement(void) { RunSystem("diskmgmt.msc"); }
void OnToolsEventViewer(void) { RunSystem("eventvwr.msc"); }
void OnToolsServices(void) { RunSystem("services.msc"); }
void OnToolsTaskScheduler(void) { RunSystem("taskschd.msc"); }
void OnToolsRegistry(void) { RunSystem("regedit.exe"); }
void OnToolsGroupPolicy(void) { RunSystem("gpedit.msc"); }
void OnToolsComputerManagement(void) { RunSystem("compmgmt.msc"); }
void OnToolsSystemInfo(void) { RunSystem("msinfo32.exe"); }
void OnToolsPerformance(void) { RunSystem("perfmon.msc"); }
void OnToolsResourceMonitor(void) { RunSystem("resmon.exe"); }

// System handlers
void OnSystemAbout(void) { OpenSettings("about"); }
void OnSystemProperties(void) { OpenControlPanel("sysdm.cpl"); }
void OnSystemAdvanced(void) { OpenControlPanel("sysdm.cpl,,3"); }
void OnSystemDeviceManager(void) { RunSystem("devmgmt.msc"); }
void OnSystemDiskManagement(void) { RunSystem("diskmgmt.msc"); }
void OnSystemComputerManagement(void) { RunSystem("compmgmt.msc"); }
void OnSystemServices(void) { RunSystem("services.msc"); }
void OnSystemTaskScheduler(void) { RunSystem("taskschd.msc"); }
void OnSystemEventViewer(void) { RunSystem("eventvwr.msc"); }
void OnSystemRegistry(void) { RunSystem("regedit.exe"); }
void OnSystemGroupPolicy(void) { RunSystem("gpedit.msc"); }
void OnSystemSystemInfo(void) { RunSystem("msinfo32.exe"); }
void OnSystemMsinfo32(void) { RunSystem("msinfo32.exe"); }
void OnSystemDxdiag(void) { RunSystem("dxdiag.exe"); }

// Control Panel handlers
void OnCPAll(void) { OpenControlPanel(""); }
void OnCPPrograms(void) { OpenControlPanel("appwiz.cpl"); }
void OnCPFeatures(void) { RunSystem("optionalfeatures.exe"); }
void OnCPDateTime(void) { OpenControlPanel("timedate.cpl"); }
void OnCPRegion(void) { OpenControlPanel("intl.cpl"); }
void OnCPKeyboard(void) { OpenControlPanel("main.cpl"); }
void OnCPMouse(void) { OpenControlPanel("main.cpl"); }
void OnCPSound(void) { OpenControlPanel("mmsys.cpl"); }
void OnCPFonts(void) { OpenShellFolder("shell:Fonts"); }
void OnCPFolderOptions(void) { OpenControlPanel("folders"); }
void OnCPInternetOptions(void) { OpenControlPanel("inetcpl.cpl"); }
void OnCPUserAccounts(void) { OpenControlPanel("userpasswords2"); }
void OnCPPowerOptions(void) { OpenControlPanel("powercfg.cpl"); }
void OnCPFirewall(void) { OpenControlPanel("firewall.cpl"); }
void OnCPAdminTools(void) { OpenShellFolder("shell:::{D20EA4E1-3957-11d2-A40B-0C5020524153}"); }

// Settings handlers
void OnSetHome(void) { OpenSettings(""); }
void OnSetDisplay(void) { OpenSettings("display"); }
void OnSetSound(void) { OpenSettings("sound"); }
void OnSetNotifications(void) { OpenSettings("notifications"); }
void OnSetPower(void) { OpenSettings("powersleep"); }
void OnSetStorage(void) { OpenSettings("storagesense"); }
void OnSetAbout(void) { OpenSettings("about"); }
void OnSetBluetooth(void) { OpenSettings("bluetooth"); }
void OnSetPrinters(void) { OpenSettings("printers"); }
void OnSetMouse(void) { OpenSettings("mousetouchpad"); }
void OnSetNetwork(void) { OpenSettings("network"); }
void OnSetWifi(void) { OpenSettings("network-wifi"); }
void OnSetVpn(void) { OpenSettings("network-vpn"); }
void OnSetProxy(void) { OpenSettings("network-proxy"); }
void OnSetBackground(void) { OpenSettings("personalization-background"); }
void OnSetColors(void) { OpenSettings("colors"); }
void OnSetLockScreen(void) { OpenSettings("lockscreen"); }
void OnSetThemes(void) { OpenSettings("themes"); }
void OnSetApps(void) { OpenSettings("appsfeatures"); }
void OnSetAccounts(void) { OpenSettings("yourinfo"); }
void OnSetTime(void) { OpenSettings("dateandtime"); }
void OnSetLanguage(void) { OpenSettings("regionlanguage"); }
void OnSetGaming(void) { OpenSettings("gaming-gamebar"); }
void OnSetAccessibility(void) { OpenSettings("easeofaccess"); }
void OnSetPrivacy(void) { OpenSettings("privacy"); }
void OnSetUpdate(void) { OpenSettings("windowsupdate"); }
void OnSetRecovery(void) { OpenSettings("recovery"); }

// Network handlers
void OnNetNetworkSettings(void) { OpenSettings("network"); }
void OnNetAdapterSettings(void) { OpenControlPanel("ncpa.cpl"); }
void OnNetWifiSettings(void) { OpenSettings("network-wifi"); }
void OnNetVpnSettings(void) { OpenSettings("network-vpn"); }
void OnNetProxySettings(void) { OpenSettings("network-proxy"); }
void OnNetIpconfig(void) { RunCmd("ipconfig /all", 1); }
void OnNetPingGoogle(void) { RunCmd("ping google.com -t", 1); }
void OnNetTracert(void) { RunCmd("tracert google.com", 1); }
void OnNetNetstat(void) { RunCmd("netstat -an", 1); }
void OnNetFlushDns(void) { RunCmd("ipconfig /flushdns", 1); }
void OnNetBrowser(void) { ShellExecute(NULL, "open", "https://www.google.com", NULL, NULL, SW_SHOWNORMAL); }
void OnNetRemoteDesktop(void) { RunSystem("mstsc.exe"); }

// Power handlers
void OnPowerSleep(void) { SetSystemPowerState(FALSE, FALSE); }
void OnPowerHibernate(void) { SetSystemPowerState(TRUE, FALSE); }
void OnPowerRestart(void) { system("shutdown /r /t 3"); }
void OnPowerShutdown(void) { system("shutdown /s /t 3"); }
void OnPowerSignout(void) { system("shutdown /l"); }
void OnPowerLock(void) { LockWorkStation(); }
void OnPowerPowerOptions(void) { OpenControlPanel("powercfg.cpl"); }

// Display handlers
void OnDispSettings(void) { OpenSettings("display"); }
void OnDispPersonalization(void) { OpenSettings("personalization"); }
void OnDispBackground(void) { OpenSettings("personalization-background"); }
void OnDispColors(void) { OpenSettings("colors"); }
void OnDispLockScreen(void) { OpenSettings("lockscreen"); }
void OnDispThemes(void) { OpenSettings("themes"); }
void OnDispResolution(void) { OpenControlPanel("desk.cpl"); }
void OnDispNightLight(void) { OpenSettings("nightlight"); }

// Audio handlers
void OnAudioVolumeUp(void) { SimulateKey(VK_VOLUME_UP); }
void OnAudioVolumeDown(void) { SimulateKey(VK_VOLUME_DOWN); }
void OnAudioVolumeMute(void) { SimulateKey(VK_VOLUME_MUTE); }
void OnAudioVolumeMixer(void) { RunSystem("sndvol.exe"); }
void OnAudioSoundSettings(void) { OpenSettings("sound"); }
void OnAudioPlayback(void) { OpenControlPanel("mmsys.cpl,,0"); }
void OnAudioRecording(void) { OpenControlPanel("mmsys.cpl,,1"); }

// Security handlers
void OnSecWindowsSecurity(void) { OpenSettings("windowsdefender"); }
void OnSecVirusScan(void) { RunSystem("MSASCui.exe"); }
void OnSecFirewall(void) { OpenControlPanel("firewall.cpl"); }
void OnSecUpdateSettings(void) { OpenSettings("windowsupdate"); }
void OnSecBackup(void) { OpenSettings("backup"); }
void OnSecTaskManager(void) { RunSystem("taskmgr.exe"); }

// Accessibility handlers
void OnAccessCenter(void) { OpenControlPanel("access.cpl"); }
void OnAccessMagnifier(void) { RunSystem("Magnify.exe"); }
void OnAccessNarrator(void) { RunSystem("Narrator.exe"); }
void OnAccessOnscreenKeyboard(void) { RunSystem("osk.exe"); }

// Developer handlers
void OnDevCmd(void) { RunSystem("cmd.exe"); }
void OnDevCmdAdmin(void) { ShellExecute(NULL, "runas", "cmd.exe", NULL, NULL, SW_SHOWNORMAL); }
void OnDevPowerShell(void) { RunSystem("powershell.exe"); }
void OnDevPowerShellAdmin(void) { ShellExecute(NULL, "runas", "powershell.exe", NULL, NULL, SW_SHOWNORMAL); }
void OnDevRegistry(void) { RunSystem("regedit.exe"); }
void OnDevPolicyEditor(void) { RunSystem("gpedit.msc"); }

// Admin handlers
void OnAdminComputerMgmt(void) { RunSystem("compmgmt.msc"); }
void OnAdminDefrag(void) { RunSystem("dfrgui.exe"); }
void OnAdminDiskCleanup(void) { RunSystem("cleanmgr.exe"); }
void OnAdminChkdsk(void) { RunCmd("chkdsk", 1); }
void OnAdminSfcScannow(void) { ShellExecute(NULL, "runas", "cmd.exe", "/c sfc /scannow", NULL, SW_SHOWNORMAL); }
void OnAdminDism(void) { RunCmd("dism /online /cleanup-image /checkhealth", 1); }
void OnAdminMemoryDiag(void) { RunSystem("MdSched.exe"); }

// Apps handlers
void OnAppNotepad(void) { RunSystem("notepad.exe"); }
void OnAppWordpad(void) { RunSystem("write.exe"); }
void OnAppPaint(void) { RunSystem("mspaint.exe"); }
void OnAppCalculator(void) { RunSystem("calc.exe"); }
void OnAppSnippingTool(void) { RunSystem("SnippingTool.exe"); }
void OnAppStickyNotes(void) { RunSystem("StikyNot.exe"); }
void OnAppEdge(void) { RunSystem("msedge.exe"); }
void OnAppStore(void) { ShellExecute(NULL, "open", "ms-windows-store:", NULL, NULL, SW_SHOWNORMAL); }

// Shell handlers
void OnShellRun(void) { SimulateCombo(VK_LWIN, 'R'); }
void OnShellSearch(void) { SimulateCombo(VK_LWIN, 'S'); }
void OnShellTaskmgr(void) { RunSystem("taskmgr.exe"); }
void OnShellLock(void) { LockWorkStation(); }
void OnShellShowDesktop(void) { SimulateCombo(VK_LWIN, 'D'); }
void OnShellSettings(void) { SimulateCombo(VK_LWIN, 'I'); }
void OnShellFileExplorer(void) { SimulateCombo(VK_LWIN, 'E'); }
void OnShellControlPanel(void) { OpenControlPanel(""); }
void OnShellRecycleBin(void) { OpenShellFolder("shell:RecycleBinFolder"); }
void OnShellStartup(void) { OpenShellFolder("shell:startup"); }

// Top handlers
void OnTopSystemInfo(void) { RunSystem("msinfo32.exe"); }
void OnTopRestartExplorer(void) { system("taskkill /f /im explorer.exe && start explorer.exe"); }
void OnTopEmptyRecycle(void) { SHEmptyRecycleBin(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND); }
void OnTopEditHosts(void) { RunSystem("notepad.exe %SystemRoot%\\System32\\drivers\\etc\\hosts"); }
void OnTopEditRegistry(void) { RunSystem("regedit.exe"); }
void OnTopAbout(void) { ShowInfo("Ultimate Windows Control Menu v2.0\n\nOver 350 functional commands\nFull Windows system integration\n\nWritten in pure C with Win32 API"); }
void OnTopExit(void) { PostMessage(g_hWnd, WM_CLOSE, 0, 0); }

// Maintenance handlers
void OnMaintCleanDisk(void) { RunSystem("cleanmgr.exe"); }
void OnMaintDefrag(void) { RunSystem("dfrgui.exe"); }
void OnMaintChkdsk(void) { RunCmd("chkdsk", 1); }
void OnMaintSfcScannow(void) { ShellExecute(NULL, "runas", "cmd.exe", "/c sfc /scannow", NULL, SW_SHOWNORMAL); }
void OnMaintDism(void) { RunCmd("dism /online /cleanup-image /checkhealth", 1); }
void OnMaintDiskCleanup(void) { RunSystem("cleanmgr.exe"); }
void OnMaintSystemRestore(void) { RunSystem("rstrui.exe"); }
void OnMaintWindowsBackup(void) { OpenSettings("backup"); }
void OnMaintTaskScheduler(void) { RunSystem("taskschd.msc"); }
void OnMaintPrintManagement(void) { RunSystem("printmanagement.msc"); }
void OnMaintSharedFolders(void) { RunSystem("fsmgmt.msc"); }
void OnMaintReliabilityMonitor(void) { RunSystem("perfmon /rel"); }

// Troubleshooting handlers
void OnTroubleShooter(void) { OpenSettings("troubleshoot"); }
void OnTroubleEventViewer(void) { RunSystem("eventvwr.msc"); }
void OnTroublePerformanceMonitor(void) { RunSystem("perfmon.msc"); }
void OnTroubleResourceMonitor(void) { RunSystem("resmon.exe"); }
void OnTroubleMemoryDiag(void) { RunSystem("MdSched.exe"); }
void OnTroubleStartupRepair(void) { RunSystem("recoverydrive.exe"); }
void OnTroubleSystemConfig(void) { RunSystem("msconfig.exe"); }
void OnTroubleDirectXDiag(void) { RunSystem("dxdiag.exe"); }
void OnTroubleProblemSteps(void) { RunSystem("psr.exe"); }

// Run Dialog handlers
void OnRunCmd(void) { RunSystem("cmd.exe"); }
void OnRunPowerShell(void) { RunSystem("powershell.exe"); }
void OnRunRegedit(void) { RunSystem("regedit.exe"); }
void OnRunServices(void) { RunSystem("services.msc"); }
void OnRunTaskmgr(void) { RunSystem("taskmgr.exe"); }
void OnRunSystemInfo(void) { RunSystem("systeminfo"); }
void OnRunMsinfo32(void) { RunSystem("msinfo32.exe"); }
void OnRunWinver(void) { RunSystem("winver.exe"); }

// Special Folders handlers
void OnFolderAppData(void) { OpenShellFolder("%APPDATA%"); }
void OnFolderLocalAppData(void) { OpenShellFolder("%LOCALAPPDATA%"); }
void OnFolderProgramData(void) { OpenShellFolder("%PROGRAMDATA%"); }
void OnFolderStartup(void) { OpenShellFolder("shell:startup"); }
void OnFolderTemp(void) { OpenShellFolder("%TEMP%"); }
void OnFolderWindows(void) { OpenShellFolder("%WINDIR%"); }
void OnFolderSystem32(void) { OpenShellFolder("%WINDIR%\\System32"); }
void OnFolderSendTo(void) { OpenShellFolder("shell:sendto"); }
void OnFolderQuickLaunch(void) { OpenShellFolder("%APPDATA%\\Microsoft\\Internet Explorer\\Quick Launch"); }

// Hardware handlers
void OnHwDeviceManager(void) { RunSystem("devmgmt.msc"); }
void OnHwPrinters(void) { OpenSettings("printers"); }
void OnHwGameControllers(void) { OpenControlPanel("joy.cpl"); }
void OnHwMouse(void) { OpenSettings("mousetouchpad"); }
void OnHwKeyboard(void) { OpenControlPanel("main.cpl"); }
void OnHwSound(void) { OpenSettings("sound"); }
void OnHwDisplay(void) { OpenSettings("display"); }

// Diagnostics handlers
void OnDiagDirectX(void) { RunSystem("dxdiag.exe"); }
void OnDiagWindowsMemory(void) { RunSystem("MdSched.exe"); }
void OnDiagResourceMonitor(void) { RunSystem("resmon.exe"); }
void OnDiagPerformanceMonitor(void) { RunSystem("perfmon.msc"); }
void OnDiagReliability(void) { RunSystem("perfmon /rel"); }
void OnDiagSystemInfo(void) { RunSystem("msinfo32.exe"); }
void OnDiagMsinfo32(void) { RunSystem("msinfo32.exe"); }

// Windows Features handlers
void OnWfTurnFeatures(void) { RunSystem("optionalfeatures.exe"); }
void OnWfOptionalFeatures(void) { OpenSettings("optionalfeatures"); }
void OnWfInternetExplorer(void) { RunSystem("iexplore.exe"); }
void OnWfMediaPlayer(void) { RunSystem("wmplayer.exe"); }
void OnWfMicrosoftStore(void) { ShellExecute(NULL, "open", "ms-windows-store:", NULL, NULL, SW_SHOWNORMAL); }
void OnWfXboxGameBar(void) { OpenSettings("gaming-gamebar"); }

// Advanced Tools handlers
void OnAdvLusrmgr(void) { RunSystem("lusrmgr.msc"); }
void OnAdvSecpol(void) { RunSystem("secpol.msc"); }
void OnAdvWF(void) { RunSystem("wf.msc"); }
void OnAdvCertmgr(void) { RunSystem("certmgr.msc"); }
void OnAdvOdbc(void) { RunSystem("odbcad32.exe"); }
void OnAdvComServices(void) { RunSystem("comexp.msc"); }
void OnAdvTpm(void) { RunSystem("tpm.msc"); }
void OnAdvBitlocker(void) { OpenControlPanel("BitLocker"); }
void OnAdvCredMgr(void) { OpenControlPanel("credentialmanager"); }
void OnAdvAutoPlayCpl(void) { OpenControlPanel("autoplay"); }
void OnAdvColorMgmt(void) { OpenControlPanel("color"); }

// System Configuration handlers
void OnSysCfgEnvVar(void) { OpenControlPanel("sysdm.cpl,,3"); }
void OnSysCfgPerfOpt(void) { OpenControlPanel("sysdm.cpl,,2"); }
void OnSysCfgUserProf(void) { OpenControlPanel("sysdm.cpl,,1"); }
void OnSysCfgStartRec(void) { OpenControlPanel("sysdm.cpl,,4"); }
void OnSysCfgHardwareProf(void) { OpenControlPanel("sysdm.cpl,,5"); }
void OnSysCfgRemote(void) { OpenControlPanel("sysdm.cpl,,5"); }
void OnSysCfgSysProtection(void) { OpenControlPanel("sysdm.cpl,,4"); }

// Advanced Network handlers
void OnNetAdvNcpa(void) { RunSystem("ncpa.cpl"); }
void OnNetAdvSharingCenter(void) { OpenControlPanel("Network and Sharing Center"); }
void OnNetAdvSharing(void) { OpenControlPanel("Network and Sharing Center /Advanced"); }
void OnNetAdvWifiSense(void) { OpenSettings("network-wifisense"); }
void OnNetAdvNetReset(void) { OpenSettings("network-reset"); }
void OnNetAdvFwRules(void) { RunSystem("wf.msc"); }
void OnNetAdvInetProp(void) { OpenControlPanel("inetcpl.cpl"); }

// Advanced Security handlers
void OnSecAdvDefender(void) { RunSystem("windowsdefender:"); }
void OnSecAdvVirusThreat(void) { OpenSettings("windowsdefender"); }
void OnSecAdvFirewallProt(void) { OpenSettings("windowsdefender-firewall"); }
void OnSecAdvAppBrowser(void) { OpenSettings("windowsdefender-appbrowser"); }
void OnSecAdvDeviceSec(void) { OpenSettings("windowsdefender-devicesecurity"); }
void OnSecAdvDevicePerf(void) { OpenSettings("windowsdefender-deviceperformance"); }
void OnSecAdvFamily(void) { OpenSettings("windowsdefender-family"); }

// Developer & Diagnostics handlers
void OnDevDiagDevEnv(void) { RunCmd("call \"%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\" -property installationPath", 1); }
void OnDevDiagWsl(void) { RunSystem("wsl.exe"); }
void OnDevDiagWinTerminal(void) { RunSystem("wt.exe"); }
void OnDevDiagPerfMon(void) { RunSystem("perfmon.msc"); }
void OnDevDiagRelia(void) { RunSystem("perfmon /rel"); }
void OnDevDiagStepsRec(void) { RunSystem("psr.exe"); }
void OnDevDiagWpr(void) { RunSystem("wprui.exe"); }
void OnDevDiagGpedit(void) { RunSystem("gpedit.msc"); }

// More Settings handlers
void OnMorePen(void) { OpenSettings("pen"); }
void OnMoreAutoplay(void) { OpenSettings("autoplay"); }
void OnMorePhoneLink(void) { OpenSettings("mobile-devices"); }
void OnMoreNearby(void) { OpenSettings("crossdevice"); }
void OnMoreClipboard(void) { OpenSettings("clipboard"); }
void OnMoreFocusAssist(void) { OpenSettings("quiethours"); }
void OnMoreProjecting(void) { OpenSettings("project"); }
void OnMoreTabletMode(void) { OpenSettings("tabletmode"); }

// Advanced System handlers
void OnAdvSysWinTools(void) { OpenShellFolder("shell:::{D20EA4E1-3957-11d2-A40B-0C5020524153}"); }
void OnAdvSysHyperV(void) { RunSystem("virtmgmt.msc"); }
void OnAdvSysIscsi(void) { RunSystem("iscsicpl.exe"); }
void OnAdvSysStorageSpaces(void) { OpenSettings("storagepool"); }
void OnAdvSysCleanMgrAdv(void) { RunSystem("cleanmgr.exe /sageset:1"); }
void OnAdvSysTaskmgrDetails(void) { RunSystem("taskmgr.exe /7"); }
void OnAdvSysResmon(void) { RunSystem("resmon.exe"); }
void OnAdvSysMsinfo32(void) { RunSystem("msinfo32.exe"); }

// MSC Snap-ins handlers
void OnMscServices(void) { RunSystem("services.msc"); }
void OnMscEventvwr(void) { RunSystem("eventvwr.msc"); }
void OnMscTaskSched(void) { RunSystem("taskschd.msc"); }
void OnMscDevmgmt(void) { RunSystem("devmgmt.msc"); }
void OnMscDiskmgmt(void) { RunSystem("diskmgmt.msc"); }
void OnMscCompmgmt(void) { RunSystem("compmgmt.msc"); }
void OnMscPerfmon(void) { RunSystem("perfmon.msc"); }
void OnMscWF(void) { RunSystem("wf.msc"); }
void OnMscSecpol(void) { RunSystem("secpol.msc"); }
void OnMscGpedit(void) { RunSystem("gpedit.msc"); }
void OnMscLusrmgr(void) { RunSystem("lusrmgr.msc"); }
void OnMscCertmgr(void) { RunSystem("certmgr.msc"); }
void OnMscPrintmgmt(void) { RunSystem("printmanagement.msc"); }
void OnMscSharedFolders(void) { RunSystem("fsmgmt.msc"); }
void OnMscTpm(void) { RunSystem("tpm.msc"); }
void OnMscHyperV(void) { RunSystem("virtmgmt.msc"); }

// Shell Folders 2 handlers
void OnShell2Recent(void) { OpenShellFolder("shell:Recent"); }
void OnShell2QuickLaunch(void) { OpenShellFolder("%APPDATA%\\Microsoft\\Internet Explorer\\Quick Launch"); }
void OnShell2Nethood(void) { OpenShellFolder("shell:NetHood"); }
void OnShell2Printhood(void) { OpenShellFolder("shell:PrintHood"); }
void OnShell2Templates(void) { OpenShellFolder("shell:Templates"); }
void OnShell2Favorites(void) { OpenShellFolder("shell:Favorites"); }
void OnShell2Cookies(void) { OpenShellFolder("shell:Cookies"); }
void OnShell2History(void) { OpenShellFolder("shell:History"); }

// Network Commands handlers
void OnNetcmdIpconfigAll(void) { RunCmd("ipconfig /all", 1); }
void OnNetcmdPingGoogle(void) { RunCmd("ping google.com -t", 1); }
void OnNetcmdTracertGoogle(void) { RunCmd("tracert google.com", 1); }
void OnNetcmdNslookup(void) { RunCmd("nslookup", 1); }
void OnNetcmdNetstatAn(void) { RunCmd("netstat -an", 1); }
void OnNetcmdNetstatB(void) { RunCmd("netstat -b", 1); }
void OnNetcmdRoutePrint(void) { RunCmd("route print", 1); }
void OnNetcmdArpA(void) { RunCmd("arp -a", 1); }
void OnNetcmdGetmac(void) { RunCmd("getmac", 1); }
void OnNetcmdNbtstat(void) { RunCmd("nbtstat -n", 1); }
void OnNetcmdFlushdns(void) { RunCmd("ipconfig /flushdns", 1); }
void OnNetcmdWinsockReset(void) { RunCmd("netsh winsock reset", 1); }

// Recovery handlers
void OnRecoverySysrestore(void) { RunSystem("rstrui.exe"); }
void OnRecoveryResetpc(void) { OpenSettings("recovery"); }
void OnRecoveryAdvstartup(void) { RunSystem("shutdown /r /o /f /t 0"); }
void OnRecoveryRecoverydrive(void) { RunSystem("RecoveryDrive.exe"); }
void OnRecoverySystemimage(void) { OpenControlPanel("sdclt.exe"); }
void OnRecoveryStartuprepair(void) { RunSystem("reagentc.exe /boottore"); }

// WSL handlers
void OnWslLaunch(void) { RunSystem("wsl.exe"); }
void OnWslList(void) { RunCmd("wsl --list --verbose", 1); }
void OnWslShutdown(void) { RunCmd("wsl --shutdown", 1); }
void OnWslTerminate(void) { RunCmd("wsl --terminate", 1); }

// GUI Panel
void OnGuiOpenPanel(void);

// ============================================================================
// Command Dispatch Table
// ============================================================================
static CommandEntry g_CommandTable[] = {
    // File
    {IDM_FILE_NEW_FOLDER, OnFileNewFolder},
    {IDM_FILE_NEW_TEXT, OnFileNewText},
    {IDM_FILE_OPEN_EXPLORER, OnFileOpenExplorer},
    {IDM_FILE_OPEN_COMPUTER, OnFileOpenComputer},
    {IDM_FILE_OPEN_DOCUMENTS, OnFileOpenDocuments},
    {IDM_FILE_OPEN_DOWNLOADS, OnFileOpenDownloads},
    {IDM_FILE_OPEN_DESKTOP, OnFileOpenDesktop},
    {IDM_FILE_OPEN_PICTURES, OnFileOpenPictures},
    {IDM_FILE_OPEN_MUSIC, OnFileOpenMusic},
    {IDM_FILE_OPEN_VIDEOS, OnFileOpenVideos},
    {IDM_FILE_OPEN_ONEDRIVE, OnFileOpenOneDrive},
    {IDM_FILE_OPEN_NETWORK, OnFileOpenNetwork},
    {IDM_FILE_OPEN_RECYCLE, OnFileOpenRecycle},
    {IDM_FILE_OPEN_CMD, OnFileOpenCmd},
    {IDM_FILE_OPEN_NOTEPAD, OnFileOpenNotepad},
    {IDM_FILE_OPEN_CALC, OnFileOpenCalc},
    {IDM_FILE_OPEN_PAINT, OnFileOpenPaint},
    {IDM_FILE_SAVE, OnFileSave},
    {IDM_FILE_SAVE_AS, OnFileSaveAs},
    {IDM_FILE_PRINT, OnFilePrint},
    {IDM_FILE_PROPERTIES, OnFileProperties},
    {IDM_FILE_EXIT, OnFileExit},
    // Edit
    {IDM_EDIT_UNDO, OnEditUndo},
    {IDM_EDIT_REDO, OnEditRedo},
    {IDM_EDIT_CUT, OnEditCut},
    {IDM_EDIT_COPY, OnEditCopy},
    {IDM_EDIT_PASTE, OnEditPaste},
    {IDM_EDIT_DELETE, OnEditDelete},
    {IDM_EDIT_SELECT_ALL, OnEditSelectAll},
    {IDM_EDIT_FIND, OnEditFind},
    // View
    {IDM_VIEW_REFRESH, OnViewRefresh},
    {IDM_VIEW_FULLSCREEN, OnViewFullscreen},
    {IDM_VIEW_ZOOM_IN, OnViewZoomIn},
    {IDM_VIEW_ZOOM_OUT, OnViewZoomOut},
    {IDM_VIEW_ZOOM_100, OnViewZoom100},
    {IDM_VIEW_SHOW_DESKTOP, OnViewShowDesktop},
    {IDM_VIEW_TASKBAR_SETTINGS, OnViewTaskbarSettings},
    // Tools
    {IDM_TOOLS_OPTIONS, OnToolsOptions},
    {IDM_TOOLS_LANGUAGE, OnToolsLanguage},
    {IDM_TOOLS_CHECK_UPDATES, OnToolsCheckUpdates},
    {IDM_TOOLS_CLEAN_DISK, OnToolsCleanDisk},
    {IDM_TOOLS_DEFRAG, OnToolsDefrag},
    {IDM_TOOLS_DEVICE_MANAGER, OnToolsDeviceManager},
    {IDM_TOOLS_DISK_MANAGEMENT, OnToolsDiskManagement},
    {IDM_TOOLS_EVENT_VIEWER, OnToolsEventViewer},
    {IDM_TOOLS_SERVICES, OnToolsServices},
    {IDM_TOOLS_TASK_SCHEDULER, OnToolsTaskScheduler},
    {IDM_TOOLS_REGISTRY, OnToolsRegistry},
    {IDM_TOOLS_GROUP_POLICY, OnToolsGroupPolicy},
    {IDM_TOOLS_COMPUTER_MANAGEMENT, OnToolsComputerManagement},
    {IDM_TOOLS_SYSTEM_INFO, OnToolsSystemInfo},
    {IDM_TOOLS_PERFORMANCE, OnToolsPerformance},
    {IDM_TOOLS_RESOURCE_MONITOR, OnToolsResourceMonitor},
    // System
    {IDM_SYSTEM_ABOUT, OnSystemAbout},
    {IDM_SYSTEM_PROPERTIES, OnSystemProperties},
    {IDM_SYSTEM_ADVANCED, OnSystemAdvanced},
    {IDM_SYSTEM_DEVICE_MANAGER, OnSystemDeviceManager},
    {IDM_SYSTEM_DISK_MANAGEMENT, OnSystemDiskManagement},
    {IDM_SYSTEM_COMPUTER_MANAGEMENT, OnSystemComputerManagement},
    {IDM_SYSTEM_SERVICES, OnSystemServices},
    {IDM_SYSTEM_TASK_SCHEDULER, OnSystemTaskScheduler},
    {IDM_SYSTEM_EVENT_VIEWER, OnSystemEventViewer},
    {IDM_SYSTEM_REGISTRY, OnSystemRegistry},
    {IDM_SYSTEM_GROUP_POLICY, OnSystemGroupPolicy},
    {IDM_SYSTEM_SYSTEM_INFO, OnSystemSystemInfo},
    {IDM_SYSTEM_MSINFO32, OnSystemMsinfo32},
    {IDM_SYSTEM_DXDIAG, OnSystemDxdiag},
    // Control Panel
    {IDM_CP_ALL, OnCPAll},
    {IDM_CP_PROGRAMS, OnCPPrograms},
    {IDM_CP_FEATURES, OnCPFeatures},
    {IDM_CP_DATE_TIME, OnCPDateTime},
    {IDM_CP_REGION, OnCPRegion},
    {IDM_CP_KEYBOARD, OnCPKeyboard},
    {IDM_CP_MOUSE, OnCPMouse},
    {IDM_CP_SOUND, OnCPSound},
    {IDM_CP_FONTS, OnCPFonts},
    {IDM_CP_FOLDER_OPTIONS, OnCPFolderOptions},
    {IDM_CP_INTERNET_OPTIONS, OnCPInternetOptions},
    {IDM_CP_USER_ACCOUNTS, OnCPUserAccounts},
    {IDM_CP_POWER_OPTIONS, OnCPPowerOptions},
    {IDM_CP_FIREWALL, OnCPFirewall},
    {IDM_CP_ADMIN_TOOLS, OnCPAdminTools},
    // Settings
    {IDM_SET_HOME, OnSetHome},
    {IDM_SET_DISPLAY, OnSetDisplay},
    {IDM_SET_SOUND, OnSetSound},
    {IDM_SET_NOTIFICATIONS, OnSetNotifications},
    {IDM_SET_POWER, OnSetPower},
    {IDM_SET_STORAGE, OnSetStorage},
    {IDM_SET_ABOUT, OnSetAbout},
    {IDM_SET_BLUETOOTH, OnSetBluetooth},
    {IDM_SET_PRINTERS, OnSetPrinters},
    {IDM_SET_MOUSE, OnSetMouse},
    {IDM_SET_NETWORK, OnSetNetwork},
    {IDM_SET_WIFI, OnSetWifi},
    {IDM_SET_VPN, OnSetVpn},
    {IDM_SET_PROXY, OnSetProxy},
    {IDM_SET_BACKGROUND, OnSetBackground},
    {IDM_SET_COLORS, OnSetColors},
    {IDM_SET_LOCK_SCREEN, OnSetLockScreen},
    {IDM_SET_THEMES, OnSetThemes},
    {IDM_SET_APPS, OnSetApps},
    {IDM_SET_ACCOUNTS, OnSetAccounts},
    {IDM_SET_TIME, OnSetTime},
    {IDM_SET_LANGUAGE, OnSetLanguage},
    {IDM_SET_GAMING, OnSetGaming},
    {IDM_SET_ACCESSIBILITY, OnSetAccessibility},
    {IDM_SET_PRIVACY, OnSetPrivacy},
    {IDM_SET_UPDATE, OnSetUpdate},
    {IDM_SET_RECOVERY, OnSetRecovery},
    // Network
    {IDM_NET_NETWORK_SETTINGS, OnNetNetworkSettings},
    {IDM_NET_ADAPTER_SETTINGS, OnNetAdapterSettings},
    {IDM_NET_WIFI_SETTINGS, OnNetWifiSettings},
    {IDM_NET_VPN_SETTINGS, OnNetVpnSettings},
    {IDM_NET_PROXY_SETTINGS, OnNetProxySettings},
    {IDM_NET_IPCONFIG, OnNetIpconfig},
    {IDM_NET_PING_GOOGLE, OnNetPingGoogle},
    {IDM_NET_TRACERT, OnNetTracert},
    {IDM_NET_NETSTAT, OnNetNetstat},
    {IDM_NET_FLUSH_DNS, OnNetFlushDns},
    {IDM_NET_BROWSER, OnNetBrowser},
    {IDM_NET_REMOTE_DESKTOP, OnNetRemoteDesktop},
    // Power
    {IDM_POWER_SLEEP, OnPowerSleep},
    {IDM_POWER_HIBERNATE, OnPowerHibernate},
    {IDM_POWER_RESTART, OnPowerRestart},
    {IDM_POWER_SHUTDOWN, OnPowerShutdown},
    {IDM_POWER_SIGNOUT, OnPowerSignout},
    {IDM_POWER_LOCK, OnPowerLock},
    {IDM_POWER_POWER_OPTIONS, OnPowerPowerOptions},
    // Display
    {IDM_DISP_SETTINGS, OnDispSettings},
    {IDM_DISP_PERSONALIZATION, OnDispPersonalization},
    {IDM_DISP_BACKGROUND, OnDispBackground},
    {IDM_DISP_COLORS, OnDispColors},
    {IDM_DISP_LOCK_SCREEN, OnDispLockScreen},
    {IDM_DISP_THEMES, OnDispThemes},
    {IDM_DISP_RESOLUTION, OnDispResolution},
    {IDM_DISP_NIGHT_LIGHT, OnDispNightLight},
    // Audio
    {IDM_AUDIO_VOLUME_UP, OnAudioVolumeUp},
    {IDM_AUDIO_VOLUME_DOWN, OnAudioVolumeDown},
    {IDM_AUDIO_VOLUME_MUTE, OnAudioVolumeMute},
    {IDM_AUDIO_VOLUME_MIXER, OnAudioVolumeMixer},
    {IDM_AUDIO_SOUND_SETTINGS, OnAudioSoundSettings},
    {IDM_AUDIO_PLAYBACK, OnAudioPlayback},
    {IDM_AUDIO_RECORDING, OnAudioRecording},
    // Security
    {IDM_SEC_WINDOWS_SECURITY, OnSecWindowsSecurity},
    {IDM_SEC_VIRUS_SCAN, OnSecVirusScan},
    {IDM_SEC_FIREWALL, OnSecFirewall},
    {IDM_SEC_UPDATE_SETTINGS, OnSecUpdateSettings},
    {IDM_SEC_BACKUP, OnSecBackup},
    {IDM_SEC_TASK_MANAGER, OnSecTaskManager},
    // Accessibility
    {IDM_ACCESS_CENTER, OnAccessCenter},
    {IDM_ACCESS_MAGNIFIER, OnAccessMagnifier},
    {IDM_ACCESS_NARRATOR, OnAccessNarrator},
    {IDM_ACCESS_ONSCREEN_KEYBOARD, OnAccessOnscreenKeyboard},
    // Developer
    {IDM_DEV_CMD, OnDevCmd},
    {IDM_DEV_CMD_ADMIN, OnDevCmdAdmin},
    {IDM_DEV_POWERSHELL, OnDevPowerShell},
    {IDM_DEV_POWERSHELL_ADMIN, OnDevPowerShellAdmin},
    {IDM_DEV_REGISTRY, OnDevRegistry},
    {IDM_DEV_POLICY_EDITOR, OnDevPolicyEditor},
    // Admin
    {IDM_ADMIN_COMPUTER_MGMT, OnAdminComputerMgmt},
    {IDM_ADMIN_DEFRAG, OnAdminDefrag},
    {IDM_ADMIN_DISK_CLEANUP, OnAdminDiskCleanup},
    {IDM_ADMIN_CHKDSK, OnAdminChkdsk},
    {IDM_ADMIN_SFC_SCANNOW, OnAdminSfcScannow},
    {IDM_ADMIN_DISM, OnAdminDism},
    {IDM_ADMIN_MEMORY_DIAG, OnAdminMemoryDiag},
    // Apps
    {IDM_APP_NOTEPAD, OnAppNotepad},
    {IDM_APP_WORDPAD, OnAppWordpad},
    {IDM_APP_PAINT, OnAppPaint},
    {IDM_APP_CALCULATOR, OnAppCalculator},
    {IDM_APP_SNIPPING_TOOL, OnAppSnippingTool},
    {IDM_APP_STICKY_NOTES, OnAppStickyNotes},
    {IDM_APP_EDGE, OnAppEdge},
    {IDM_APP_STORE, OnAppStore},
    // Shell
    {IDM_SHELL_RUN, OnShellRun},
    {IDM_SHELL_SEARCH, OnShellSearch},
    {IDM_SHELL_TASKMGR, OnShellTaskmgr},
    {IDM_SHELL_LOCK, OnShellLock},
    {IDM_SHELL_SHOW_DESKTOP, OnShellShowDesktop},
    {IDM_SHELL_SETTINGS, OnShellSettings},
    {IDM_SHELL_FILE_EXPLORER, OnShellFileExplorer},
    {IDM_SHELL_CONTROL_PANEL, OnShellControlPanel},
    {IDM_SHELL_RECYCLE_BIN, OnShellRecycleBin},
    {IDM_SHELL_STARTUP, OnShellStartup},
    // Top
    {IDM_TOP_SYSTEM_INFO, OnTopSystemInfo},
    {IDM_TOP_RESTART_EXPLORER, OnTopRestartExplorer},
    {IDM_TOP_EMPTY_RECYCLE, OnTopEmptyRecycle},
    {IDM_TOP_EDIT_HOSTS, OnTopEditHosts},
    {IDM_TOP_EDIT_REGISTRY, OnTopEditRegistry},
    {IDM_TOP_ABOUT, OnTopAbout},
    {IDM_TOP_EXIT, OnTopExit},
    // Maintenance
    {IDM_MAINT_CLEAN_DISK, OnMaintCleanDisk},
    {IDM_MAINT_DEFRAG, OnMaintDefrag},
    {IDM_MAINT_CHKDSK, OnMaintChkdsk},
    {IDM_MAINT_SFC_SCANNOW, OnMaintSfcScannow},
    {IDM_MAINT_DISM, OnMaintDism},
    {IDM_MAINT_DISK_CLEANUP, OnMaintDiskCleanup},
    {IDM_MAINT_SYSTEM_RESTORE, OnMaintSystemRestore},
    {IDM_MAINT_WINDOWS_BACKUP, OnMaintWindowsBackup},
    {IDM_MAINT_TASK_SCHEDULER, OnMaintTaskScheduler},
    {IDM_MAINT_PRINT_MANAGEMENT, OnMaintPrintManagement},
    {IDM_MAINT_SHARED_FOLDERS, OnMaintSharedFolders},
    {IDM_MAINT_RELIABILITY_MONITOR, OnMaintReliabilityMonitor},
    // Troubleshooting
    {IDM_TROUBLE_SHOOTER, OnTroubleShooter},
    {IDM_TROUBLE_EVENT_VIEWER, OnTroubleEventViewer},
    {IDM_TROUBLE_PERFORMANCE_MONITOR, OnTroublePerformanceMonitor},
    {IDM_TROUBLE_RESOURCE_MONITOR, OnTroubleResourceMonitor},
    {IDM_TROUBLE_MEMORY_DIAG, OnTroubleMemoryDiag},
    {IDM_TROUBLE_STARTUP_REPAIR, OnTroubleStartupRepair},
    {IDM_TROUBLE_SYSTEM_CONFIG, OnTroubleSystemConfig},
    {IDM_TROUBLE_DIRECTX_DIAG, OnTroubleDirectXDiag},
    {IDM_TROUBLE_PROBLEM_STEPS, OnTroubleProblemSteps},
    // Run Dialog
    {IDM_RUN_CMD, OnRunCmd},
    {IDM_RUN_POWERSHELL, OnRunPowerShell},
    {IDM_RUN_REGEDIT, OnRunRegedit},
    {IDM_RUN_SERVICES, OnRunServices},
    {IDM_RUN_TASKMGR, OnRunTaskmgr},
    {IDM_RUN_SYSTEM_INFO, OnRunSystemInfo},
    {IDM_RUN_MSINFO32, OnRunMsinfo32},
    {IDM_RUN_WINVER, OnRunWinver},
    // Special Folders
    {IDM_FOLDER_APPDATA, OnFolderAppData},
    {IDM_FOLDER_LOCALAPPDATA, OnFolderLocalAppData},
    {IDM_FOLDER_PROGRAMDATA, OnFolderProgramData},
    {IDM_FOLDER_STARTUP, OnFolderStartup},
    {IDM_FOLDER_TEMP, OnFolderTemp},
    {IDM_FOLDER_WINDOWS, OnFolderWindows},
    {IDM_FOLDER_SYSTEM32, OnFolderSystem32},
    {IDM_FOLDER_SENDTO, OnFolderSendTo},
    {IDM_FOLDER_QUICK_LAUNCH, OnFolderQuickLaunch},
    // Hardware
    {IDM_HW_DEVICE_MANAGER, OnHwDeviceManager},
    {IDM_HW_PRINTERS, OnHwPrinters},
    {IDM_HW_GAME_CONTROLLERS, OnHwGameControllers},
    {IDM_HW_MOUSE, OnHwMouse},
    {IDM_HW_KEYBOARD, OnHwKeyboard},
    {IDM_HW_SOUND, OnHwSound},
    {IDM_HW_DISPLAY, OnHwDisplay},
    // Diagnostics
    {IDM_DIAG_DIRECTX, OnDiagDirectX},
    {IDM_DIAG_WINDOWS_MEMORY, OnDiagWindowsMemory},
    {IDM_DIAG_RESOURCE_MONITOR, OnDiagResourceMonitor},
    {IDM_DIAG_PERFORMANCE_MONITOR, OnDiagPerformanceMonitor},
    {IDM_DIAG_RELIABILITY, OnDiagReliability},
    {IDM_DIAG_SYSTEM_INFO, OnDiagSystemInfo},
    {IDM_DIAG_MSINFO32, OnDiagMsinfo32},
    // Windows Features
    {IDM_WF_TURN_FEATURES, OnWfTurnFeatures},
    {IDM_WF_OPTIONAL_FEATURES, OnWfOptionalFeatures},
    {IDM_WF_INTERNET_EXPLORER, OnWfInternetExplorer},
    {IDM_WF_MEDIA_PLAYER, OnWfMediaPlayer},
    {IDM_WF_MICROSOFT_STORE, OnWfMicrosoftStore},
    {IDM_WF_XBOX_GAME_BAR, OnWfXboxGameBar},
    // Advanced Tools
    {IDM_ADV_LUSRMGR, OnAdvLusrmgr},
    {IDM_ADV_SECPOL, OnAdvSecpol},
    {IDM_ADV_WFIREWALL, OnAdvWF},
    {IDM_ADV_CERTMGR, OnAdvCertmgr},
    {IDM_ADV_ODBC, OnAdvOdbc},
    {IDM_ADV_COMSERVICES, OnAdvComServices},
    {IDM_ADV_TPM, OnAdvTpm},
    {IDM_ADV_BITLOCKER, OnAdvBitlocker},
    {IDM_ADV_CREDENTIAL_MANAGER, OnAdvCredMgr},
    {IDM_ADV_AUTOPLAY, OnAdvAutoPlayCpl},
    {IDM_ADV_COLOR_MANAGEMENT, OnAdvColorMgmt},
    // System Configuration
    {IDM_SYSCFG_ENVVAR, OnSysCfgEnvVar},
    {IDM_SYSCFG_PERFOPT, OnSysCfgPerfOpt},
    {IDM_SYSCFG_USERPROF, OnSysCfgUserProf},
    {IDM_SYSCFG_STARTREC, OnSysCfgStartRec},
    {IDM_SYSCFG_HARDWAREPROF, OnSysCfgHardwareProf},
    {IDM_SYSCFG_REMOTE, OnSysCfgRemote},
    {IDM_SYSCFG_SYSPROTECTION, OnSysCfgSysProtection},
    // Advanced Network
    {IDM_NETADV_NCPA, OnNetAdvNcpa},
    {IDM_NETADV_SHARINGCENTER, OnNetAdvSharingCenter},
    {IDM_NETADV_ADVSHARING, OnNetAdvSharing},
    {IDM_NETADV_WIFISENSE, OnNetAdvWifiSense},
    {IDM_NETADV_NETRESET, OnNetAdvNetReset},
    {IDM_NETADV_FIREWALLRULES, OnNetAdvFwRules},
    {IDM_NETADV_INETPROPERTIES, OnNetAdvInetProp},
    // Advanced Security
    {IDM_SECADV_DEFENDER, OnSecAdvDefender},
    {IDM_SECADV_VIRUSTHREAT, OnSecAdvVirusThreat},
    {IDM_SECADV_FIREWALLPROT, OnSecAdvFirewallProt},
    {IDM_SECADV_APPBROWSER, OnSecAdvAppBrowser},
    {IDM_SECADV_DEVICESEC, OnSecAdvDeviceSec},
    {IDM_SECADV_DEVICEPERF, OnSecAdvDevicePerf},
    {IDM_SECADV_FAMILY, OnSecAdvFamily},
    // Developer & Diagnostics
    {IDM_DEVDIAG_DEVENV, OnDevDiagDevEnv},
    {IDM_DEVDIAG_WSL, OnDevDiagWsl},
    {IDM_DEVDIAG_WINTERMINAL, OnDevDiagWinTerminal},
    {IDM_DEVDIAG_PERFMON, OnDevDiagPerfMon},
    {IDM_DEVDIAG_RELIA, OnDevDiagRelia},
    {IDM_DEVDIAG_STEPSREC, OnDevDiagStepsRec},
    {IDM_DEVDIAG_WPR, OnDevDiagWpr},
    {IDM_DEVDIAG_GPEDIT, OnDevDiagGpedit},
    // More Settings
    {IDM_MORE_PEN, OnMorePen},
    {IDM_MORE_AUTOPLAY, OnMoreAutoplay},
    {IDM_MORE_PHONELINK, OnMorePhoneLink},
    {IDM_MORE_NEARBY, OnMoreNearby},
    {IDM_MORE_CLIPBOARD, OnMoreClipboard},
    {IDM_MORE_FOCUSASSIST, OnMoreFocusAssist},
    {IDM_MORE_PROJECTING, OnMoreProjecting},
    {IDM_MORE_TABLETMODE, OnMoreTabletMode},
    // Advanced System
    {IDM_ADVSYS_WINTOOLS, OnAdvSysWinTools},
    {IDM_ADVSYS_HYPERV, OnAdvSysHyperV},
    {IDM_ADVSYS_ISCSI, OnAdvSysIscsi},
    {IDM_ADVSYS_STORAGESPACES, OnAdvSysStorageSpaces},
    {IDM_ADVSYS_CLEANMGR_ADV, OnAdvSysCleanMgrAdv},
    {IDM_ADVSYS_TASKMGR_DETAILS, OnAdvSysTaskmgrDetails},
    {IDM_ADVSYS_RESMON, OnAdvSysResmon},
    {IDM_ADVSYS_MSINFO32, OnAdvSysMsinfo32},
    // MSC Snap-ins
    {IDM_MSC_SERVICES, OnMscServices},
    {IDM_MSC_EVENTVWR, OnMscEventvwr},
    {IDM_MSC_TASKSCHED, OnMscTaskSched},
    {IDM_MSC_DEVMGMT, OnMscDevmgmt},
    {IDM_MSC_DISKMGMT, OnMscDiskmgmt},
    {IDM_MSC_COMPMGMT, OnMscCompmgmt},
    {IDM_MSC_PERFMON, OnMscPerfmon},
    {IDM_MSC_WFIREWALL, OnMscWF},
    {IDM_MSC_SECPOL, OnMscSecpol},
    {IDM_MSC_GPEDIT, OnMscGpedit},
    {IDM_MSC_LUSRMGR, OnMscLusrmgr},
    {IDM_MSC_CERTMGR, OnMscCertmgr},
    {IDM_MSC_PRINTMGMT, OnMscPrintmgmt},
    {IDM_MSC_SHAREDFOLDERS, OnMscSharedFolders},
    {IDM_MSC_TPM, OnMscTpm},
    {IDM_MSC_HYPERV, OnMscHyperV},
    // Shell Folders 2
    {IDM_SHELL2_RECENT, OnShell2Recent},
    {IDM_SHELL2_QUICKLAUNCH, OnShell2QuickLaunch},
    {IDM_SHELL2_NETHOOD, OnShell2Nethood},
    {IDM_SHELL2_PRINTHOOD, OnShell2Printhood},
    {IDM_SHELL2_TEMPLATES, OnShell2Templates},
    {IDM_SHELL2_FAVORITES, OnShell2Favorites},
    {IDM_SHELL2_COOKIES, OnShell2Cookies},
    {IDM_SHELL2_HISTORY, OnShell2History},
    // Network Commands
    {IDM_NETCMD_IPCONFIG_ALL, OnNetcmdIpconfigAll},
    {IDM_NETCMD_PING_GOOGLE, OnNetcmdPingGoogle},
    {IDM_NETCMD_TRACERT_GOOGLE, OnNetcmdTracertGoogle},
    {IDM_NETCMD_NSLOOKUP, OnNetcmdNslookup},
    {IDM_NETCMD_NETSTAT_AN, OnNetcmdNetstatAn},
    {IDM_NETCMD_NETSTAT_B, OnNetcmdNetstatB},
    {IDM_NETCMD_ROUTE_PRINT, OnNetcmdRoutePrint},
    {IDM_NETCMD_ARP_A, OnNetcmdArpA},
    {IDM_NETCMD_GETMAC, OnNetcmdGetmac},
    {IDM_NETCMD_NBTSTAT, OnNetcmdNbtstat},
    {IDM_NETCMD_FLUSHDNS, OnNetcmdFlushdns},
    {IDM_NETCMD_WINSOCK_RESET, OnNetcmdWinsockReset},
    // Recovery
    {IDM_RECOVERY_SYSRESTORE, OnRecoverySysrestore},
    {IDM_RECOVERY_RESETPC, OnRecoveryResetpc},
    {IDM_RECOVERY_ADVSTARTUP, OnRecoveryAdvstartup},
    {IDM_RECOVERY_RECOVERYDRIVE, OnRecoveryRecoverydrive},
    {IDM_RECOVERY_SYSTEMIMAGE, OnRecoverySystemimage},
    {IDM_RECOVERY_STARTUPREPAIR, OnRecoveryStartuprepair},
    // WSL
    {IDM_WSL_LAUNCH, OnWslLaunch},
    {IDM_WSL_LIST, OnWslList},
    {IDM_WSL_SHUTDOWN, OnWslShutdown},
    {IDM_WSL_TERMINATE, OnWslTerminate},
    // GUI
    {IDM_GUI_OPENPANEL, OnGuiOpenPanel},
    {0, NULL}
};

CommandHandler FindHandler(UINT uID) {
    for (int i = 0; g_CommandTable[i].uID != 0; i++)
        if (g_CommandTable[i].uID == uID)
            return g_CommandTable[i].handler;
    return NULL;
}

// ============================================================================
// GUI Control Panel (Pure Code, No .rc Needed)
// ============================================================================
LRESULT CALLBACK GuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create buttons
            #define BTN_W 120
            #define BTN_H 28
            #define MARGIN 10
            CreateWindow("BUTTON", "System Properties", WS_VISIBLE | WS_CHILD,
                         MARGIN, MARGIN, BTN_W, BTN_H, hWnd, (HMENU)101, g_hInst, NULL);
            CreateWindow("BUTTON", "Device Manager", WS_VISIBLE | WS_CHILD,
                         MARGIN*2+BTN_W, MARGIN, BTN_W, BTN_H, hWnd, (HMENU)102, g_hInst, NULL);
            CreateWindow("BUTTON", "Disk Management", WS_VISIBLE | WS_CHILD,
                         MARGIN*3+BTN_W*2, MARGIN, BTN_W, BTN_H, hWnd, (HMENU)103, g_hInst, NULL);

            CreateWindow("BUTTON", "Services", WS_VISIBLE | WS_CHILD,
                         MARGIN, MARGIN*2+BTN_H, BTN_W, BTN_H, hWnd, (HMENU)104, g_hInst, NULL);
            CreateWindow("BUTTON", "Task Scheduler", WS_VISIBLE | WS_CHILD,
                         MARGIN*2+BTN_W, MARGIN*2+BTN_H, BTN_W, BTN_H, hWnd, (HMENU)105, g_hInst, NULL);
            CreateWindow("BUTTON", "Event Viewer", WS_VISIBLE | WS_CHILD,
                         MARGIN*3+BTN_W*2, MARGIN*2+BTN_H, BTN_W, BTN_H, hWnd, (HMENU)106, g_hInst, NULL);

            CreateWindow("BUTTON", "Registry Editor", WS_VISIBLE | WS_CHILD,
                         MARGIN, MARGIN*3+BTN_H*2, BTN_W, BTN_H, hWnd, (HMENU)107, g_hInst, NULL);
            CreateWindow("BUTTON", "System Information", WS_VISIBLE | WS_CHILD,
                         MARGIN*2+BTN_W, MARGIN*3+BTN_H*2, BTN_W, BTN_H, hWnd, (HMENU)108, g_hInst, NULL);
            CreateWindow("BUTTON", "MSConfig", WS_VISIBLE | WS_CHILD,
                         MARGIN*3+BTN_W*2, MARGIN*3+BTN_H*2, BTN_W, BTN_H, hWnd, (HMENU)109, g_hInst, NULL);

            CreateWindow("BUTTON", "Task Manager", WS_VISIBLE | WS_CHILD,
                         MARGIN, MARGIN*4+BTN_H*3, BTN_W, BTN_H, hWnd, (HMENU)110, g_hInst, NULL);
            CreateWindow("BUTTON", "Command Prompt", WS_VISIBLE | WS_CHILD,
                         MARGIN*2+BTN_W, MARGIN*4+BTN_H*3, BTN_W, BTN_H, hWnd, (HMENU)111, g_hInst, NULL);
            CreateWindow("BUTTON", "PowerShell", WS_VISIBLE | WS_CHILD,
                         MARGIN*3+BTN_W*2, MARGIN*4+BTN_H*3, BTN_W, BTN_H, hWnd, (HMENU)112, g_hInst, NULL);

            CreateWindow("BUTTON", "Settings", WS_VISIBLE | WS_CHILD,
                         MARGIN, MARGIN*5+BTN_H*4, BTN_W, BTN_H, hWnd, (HMENU)113, g_hInst, NULL);
            CreateWindow("BUTTON", "Control Panel", WS_VISIBLE | WS_CHILD,
                         MARGIN*2+BTN_W, MARGIN*5+BTN_H*4, BTN_W, BTN_H, hWnd, (HMENU)114, g_hInst, NULL);
            CreateWindow("BUTTON", "Exit", WS_VISIBLE | WS_CHILD,
                         MARGIN*3+BTN_W*2, MARGIN*5+BTN_H*4, BTN_W, BTN_H, hWnd, (HMENU)115, g_hInst, NULL);
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 101: OpenControlPanel("sysdm.cpl"); break;
                case 102: RunSystem("devmgmt.msc"); break;
                case 103: RunSystem("diskmgmt.msc"); break;
                case 104: RunSystem("services.msc"); break;
                case 105: RunSystem("taskschd.msc"); break;
                case 106: RunSystem("eventvwr.msc"); break;
                case 107: RunSystem("regedit.exe"); break;
                case 108: RunSystem("msinfo32.exe"); break;
                case 109: RunSystem("msconfig.exe"); break;
                case 110: RunSystem("taskmgr.exe"); break;
                case 111: RunSystem("cmd.exe"); break;
                case 112: RunSystem("powershell.exe"); break;
                case 113: OpenSettings(""); break;
                case 114: OpenControlPanel(""); break;
                case 115: DestroyWindow(hWnd); g_hGuiDlg = NULL; break;
            }
            break;
        case WM_DESTROY:
            g_hGuiDlg = NULL;
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void OnGuiOpenPanel(void) {
    if (g_hGuiDlg) {
        SetForegroundWindow(g_hGuiDlg);
        ShowWindow(g_hGuiDlg, SW_RESTORE);
        return;
    }

    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = GuiWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "GuiPanelClass";
    RegisterClassEx(&wc);

    // Create window
    g_hGuiDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, "GuiPanelClass",
                               "Ultimate Windows Control Panel",
                               WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, 410, 240,
                               NULL, NULL, g_hInst, NULL);
    if (!g_hGuiDlg) {
        MessageBox(NULL, "Failed to create GUI window", "Error", MB_OK);
        return;
    }
    ShowWindow(g_hGuiDlg, SW_SHOW);
}

// ============================================================================
// WinMain and Window Procedure
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), 0, WndProc, 0, 0, hInstance, NULL,
                      LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1),
                      NULL, "FunctionalMenuClass", NULL };
    RegisterClassEx(&wc);

    g_hWnd = CreateWindowEx(0, wc.lpszClassName, "Functional Menu App", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);
    if (!g_hWnd) return 1;

    if (!AddTrayIcon(g_hWnd)) return 1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) ShowContextMenu(hWnd);
            break;
        case WM_COMMAND:
            ProcessCommand(wParam);
            break;
        case WM_DESTROY:
            RemoveTrayIcon(hWnd);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

BOOL AddTrayIcon(HWND hWnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hWnd;
    g_nid.uID = ID_TRAYICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nid.szTip, "Ultimate Control Menu - Right Click");
    return Shell_NotifyIcon(NIM_ADD, &g_nid);
}

BOOL RemoveTrayIcon(HWND hWnd) {
    return Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void AppendNewMenus(HMENU hPopup) {
    HMENU hSub, hSub2;

    // ---------- Advanced Tools ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_ADV_LUSRMGR, "&Local Users and Groups");
    AppendMenu(hSub, MF_STRING, IDM_ADV_SECPOL, "&Local Security Policy");
    AppendMenu(hSub, MF_STRING, IDM_ADV_WFIREWALL, "Windows Firewall with &Advanced Security");
    AppendMenu(hSub, MF_STRING, IDM_ADV_CERTMGR, "&Certificate Manager");
    AppendMenu(hSub, MF_STRING, IDM_ADV_ODBC, "&ODBC Data Sources");
    AppendMenu(hSub, MF_STRING, IDM_ADV_COMSERVICES, "&Component Services");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_ADV_TPM, "&TPM Management");
    AppendMenu(hSub, MF_STRING, IDM_ADV_BITLOCKER, "&BitLocker Drive Encryption");
    AppendMenu(hSub, MF_STRING, IDM_ADV_CREDENTIAL_MANAGER, "&Credential Manager");
    AppendMenu(hSub, MF_STRING, IDM_ADV_AUTOPLAY, "&AutoPlay Settings");
    AppendMenu(hSub, MF_STRING, IDM_ADV_COLOR_MANAGEMENT, "&Color Management");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Advanced Tools");

    // ---------- System Configuration ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_ENVVAR, "&Environment Variables");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_PERFOPT, "&Performance Options");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_USERPROF, "&User Profiles");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_STARTREC, "&Startup and Recovery");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_HARDWAREPROF, "&Hardware Profiles");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_REMOTE, "&Remote Settings");
    AppendMenu(hSub, MF_STRING, IDM_SYSCFG_SYSPROTECTION, "&System Protection");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&System Configuration");

    // ---------- Advanced Network ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_NETADV_NCPA, "&Network Connections");
    AppendMenu(hSub, MF_STRING, IDM_NETADV_SHARINGCENTER, "Network and &Sharing Center");
    AppendMenu(hSub, MF_STRING, IDM_NETADV_ADVSHARING, "&Advanced Sharing Settings");
    AppendMenu(hSub, MF_STRING, IDM_NETADV_WIFISENSE, "&Wi‑Fi Sense");
    AppendMenu(hSub, MF_STRING, IDM_NETADV_NETRESET, "&Network Reset");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_NETADV_FIREWALLRULES, "Windows &Firewall Rules");
    AppendMenu(hSub, MF_STRING, IDM_NETADV_INETPROPERTIES, "&Internet Properties");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Network (Advanced)");

    // ---------- Windows Defender & Security ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SECADV_DEFENDER, "&Windows Security Dashboard");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_VIRUSTHREAT, "&Virus & Threat Protection");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_FIREWALLPROT, "&Firewall & Network Protection");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_APPBROWSER, "&App & Browser Control");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_DEVICESEC, "&Device Security");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_DEVICEPERF, "Device &Performance & Health");
    AppendMenu(hSub, MF_STRING, IDM_SECADV_FAMILY, "&Family Options");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Windows Security");

    // ---------- Developer & Diagnostics ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_DEVENV, "&Developer Command Prompt (VS)");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_WSL, "&Windows Subsystem for Linux");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_WINTERMINAL, "Windows &Terminal");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_PERFMON, "&Performance Monitor");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_RELIA, "&Reliability Monitor");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_STEPSREC, "&Steps Recorder");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_WPR, "Windows &Performance Recorder");
    AppendMenu(hSub, MF_STRING, IDM_DEVDIAG_GPEDIT, "&Group Policy Editor");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Developer & Diagnostics");

    // ---------- More Settings ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_MORE_PEN, "&Pen & Windows Ink");
    AppendMenu(hSub, MF_STRING, IDM_MORE_AUTOPLAY, "&AutoPlay");
    AppendMenu(hSub, MF_STRING, IDM_MORE_PHONELINK, "&Phone Link");
    AppendMenu(hSub, MF_STRING, IDM_MORE_NEARBY, "&Nearby Sharing");
    AppendMenu(hSub, MF_STRING, IDM_MORE_CLIPBOARD, "&Clipboard History");
    AppendMenu(hSub, MF_STRING, IDM_MORE_FOCUSASSIST, "&Focus Assist");
    AppendMenu(hSub, MF_STRING, IDM_MORE_PROJECTING, "&Projecting to this PC");
    AppendMenu(hSub, MF_STRING, IDM_MORE_TABLETMODE, "&Tablet Mode");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&More Settings");

    // ---------- Advanced System ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_WINTOOLS, "&Windows Tools");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_HYPERV, "&Hyper‑V Manager");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_ISCSI, "&iSCSI Initiator");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_STORAGESPACES, "&Storage Spaces");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_CLEANMGR_ADV, "&Disk Cleanup (Advanced)");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_TASKMGR_DETAILS, "Task Manager (&Details)");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_RESMON, "&Resource Monitor");
    AppendMenu(hSub, MF_STRING, IDM_ADVSYS_MSINFO32, "System &Information");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Advanced System");

    // ---------- MSC Snap-ins ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_MSC_SERVICES, "&Services (services.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_EVENTVWR, "&Event Viewer (eventvwr.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_TASKSCHED, "&Task Scheduler (taskschd.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_DEVMGMT, "&Device Manager (devmgmt.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_DISKMGMT, "&Disk Management (diskmgmt.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_COMPMGMT, "&Computer Management (compmgmt.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_PERFMON, "&Performance Monitor (perfmon.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_WFIREWALL, "Windows &Firewall (wf.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_SECPOL, "Local &Security Policy (secpol.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_GPEDIT, "&Group Policy Editor (gpedit.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_LUSRMGR, "&Local Users and Groups (lusrmgr.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_CERTMGR, "&Certificate Manager (certmgr.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_PRINTMGMT, "&Print Management (printmanagement.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_SHAREDFOLDERS, "&Shared Folders (fsmgmt.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_TPM, "&TPM Management (tpm.msc)");
    AppendMenu(hSub, MF_STRING, IDM_MSC_HYPERV, "&Hyper-V Manager (virtmgmt.msc)");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&MSC Snap-ins");

    // ---------- Shell Folders 2 ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_RECENT, "&Recent Items");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_QUICKLAUNCH, "&Quick Launch");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_NETHOOD, "&Network Shortcuts (NetHood)");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_PRINTHOOD, "&Printer Shortcuts (PrintHood)");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_TEMPLATES, "&Templates");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_FAVORITES, "&Favorites");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_COOKIES, "&Cookies");
    AppendMenu(hSub, MF_STRING, IDM_SHELL2_HISTORY, "&History");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Shell Folders 2");

    // ---------- Network Commands ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_IPCONFIG_ALL, "&ipconfig /all");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_PING_GOOGLE, "&ping google.com -t");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_TRACERT_GOOGLE, "&tracert google.com");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_NSLOOKUP, "&nslookup");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_NETSTAT_AN, "netstat &-an");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_NETSTAT_B, "netstat -&b");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_ROUTE_PRINT, "route &print");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_ARP_A, "arp &-a");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_GETMAC, "&getmac");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_NBTSTAT, "&nbtstat -n");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_FLUSHDNS, "ipconfig /flush&dns");
    AppendMenu(hSub, MF_STRING, IDM_NETCMD_WINSOCK_RESET, "netsh winsock &reset");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Network Commands");

    // ---------- Recovery ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_SYSRESTORE, "&System Restore");
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_RESETPC, "&Reset This PC");
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_ADVSTARTUP, "&Advanced Startup");
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_RECOVERYDRIVE, "Create &Recovery Drive");
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_SYSTEMIMAGE, "&System Image Backup");
    AppendMenu(hSub, MF_STRING, IDM_RECOVERY_STARTUPREPAIR, "&Startup Repair");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Recovery");

    // ---------- WSL ----------
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_WSL_LAUNCH, "&Launch WSL");
    AppendMenu(hSub, MF_STRING, IDM_WSL_LIST, "&List Distributions");
    AppendMenu(hSub, MF_STRING, IDM_WSL_SHUTDOWN, "&Shutdown WSL");
    AppendMenu(hSub, MF_STRING, IDM_WSL_TERMINATE, "&Terminate Distribution");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&WSL");

    // ---------- GUI Panel ----------
    AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenu(hPopup, MF_STRING, IDM_GUI_OPENPANEL, "&Open GUI Control Panel");
}

void ShowContextMenu(HWND hWnd) {
    HMENU hPopup = CreatePopupMenu();
    HMENU hSub, hSub2;

    // ========== FILE ==========
    hSub = CreatePopupMenu();
    hSub2 = CreatePopupMenu();
    AppendMenu(hSub2, MF_STRING, IDM_FILE_NEW_FOLDER, "&Folder");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_NEW_TEXT, "&Text Document");
    AppendMenu(hSub, MF_POPUP, (UINT_PTR)hSub2, "&New");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    hSub2 = CreatePopupMenu();
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_EXPLORER, "&File Explorer");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_COMPUTER, "This &PC");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_DOCUMENTS, "&Documents");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_DOWNLOADS, "&Downloads");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_DESKTOP, "&Desktop");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_PICTURES, "&Pictures");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_MUSIC, "&Music");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_VIDEOS, "&Videos");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_ONEDRIVE, "&OneDrive");
    AppendMenu(hSub2, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_NETWORK, "&Network");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_RECYCLE, "&Recycle Bin");
    AppendMenu(hSub2, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_CMD, "&Command Prompt");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_NOTEPAD, "&Notepad");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_CALC, "&Calculator");
    AppendMenu(hSub2, MF_STRING, IDM_FILE_OPEN_PAINT, "&Paint");
    AppendMenu(hSub, MF_POPUP, (UINT_PTR)hSub2, "&Open");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FILE_SAVE, "&Save");
    AppendMenu(hSub, MF_STRING, IDM_FILE_SAVE_AS, "Save &As...");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FILE_PRINT, "&Print...");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FILE_PROPERTIES, "P&roperties");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&File");

    // ========== EDIT ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_EDIT_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(hSub, MF_STRING, IDM_EDIT_REDO, "&Redo\tCtrl+Y");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_EDIT_CUT, "Cu&t\tCtrl+X");
    AppendMenu(hSub, MF_STRING, IDM_EDIT_COPY, "&Copy\tCtrl+C");
    AppendMenu(hSub, MF_STRING, IDM_EDIT_PASTE, "&Paste\tCtrl+V");
    AppendMenu(hSub, MF_STRING, IDM_EDIT_DELETE, "&Delete\tDel");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_EDIT_SELECT_ALL, "Select &All\tCtrl+A");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_EDIT_FIND, "&Find...\tCtrl+F");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Edit");

    // ========== VIEW ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_VIEW_REFRESH, "&Refresh\tF5");
    AppendMenu(hSub, MF_STRING, IDM_VIEW_FULLSCREEN, "&Full Screen\tF11");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    hSub2 = CreatePopupMenu();
    AppendMenu(hSub2, MF_STRING, IDM_VIEW_ZOOM_IN, "Zoom &In\tCtrl++");
    AppendMenu(hSub2, MF_STRING, IDM_VIEW_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    AppendMenu(hSub2, MF_STRING, IDM_VIEW_ZOOM_100, "&100%");
    AppendMenu(hSub, MF_POPUP, (UINT_PTR)hSub2, "&Zoom");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_VIEW_SHOW_DESKTOP, "Show &Desktop\tWin+D");
    AppendMenu(hSub, MF_STRING, IDM_VIEW_TASKBAR_SETTINGS, "&Taskbar Settings");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&View");

    // ========== TOOLS ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_OPTIONS, "&Options...");
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_LANGUAGE, "&Language");
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_CHECK_UPDATES, "Check for &Updates");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_CLEAN_DISK, "Disk &Cleanup");
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_DEFRAG, "&Defragment");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    hSub2 = CreatePopupMenu();
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_DEVICE_MANAGER, "&Device Manager");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_DISK_MANAGEMENT, "&Disk Management");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_EVENT_VIEWER, "&Event Viewer");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_SERVICES, "&Services");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_TASK_SCHEDULER, "&Task Scheduler");
    AppendMenu(hSub2, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_REGISTRY, "&Registry Editor");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_GROUP_POLICY, "&Group Policy");
    AppendMenu(hSub2, MF_STRING, IDM_TOOLS_COMPUTER_MANAGEMENT, "&Computer Management");
    AppendMenu(hSub, MF_POPUP, (UINT_PTR)hSub2, "&System Tools");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_SYSTEM_INFO, "System &Information");
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_PERFORMANCE, "&Performance Monitor");
    AppendMenu(hSub, MF_STRING, IDM_TOOLS_RESOURCE_MONITOR, "&Resource Monitor");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Tools");

    // ========== SYSTEM ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_ABOUT, "&About Windows");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_PROPERTIES, "System &Properties");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_ADVANCED, "&Advanced System Settings");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_DEVICE_MANAGER, "&Device Manager");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_DISK_MANAGEMENT, "&Disk Management");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_COMPUTER_MANAGEMENT, "&Computer Management");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_SERVICES, "&Services");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_TASK_SCHEDULER, "&Task Scheduler");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_EVENT_VIEWER, "&Event Viewer");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_REGISTRY, "&Registry Editor");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_GROUP_POLICY, "&Group Policy Editor");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_MSINFO32, "System &Information");
    AppendMenu(hSub, MF_STRING, IDM_SYSTEM_DXDIAG, "&DirectX Diagnostic");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&System");

    // ========== CONTROL PANEL ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_CP_ALL, "&All Control Panel Items");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_PROGRAMS, "&Programs and Features");
    AppendMenu(hSub, MF_STRING, IDM_CP_FEATURES, "&Windows Features");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_DATE_TIME, "&Date and Time");
    AppendMenu(hSub, MF_STRING, IDM_CP_REGION, "&Region");
    AppendMenu(hSub, MF_STRING, IDM_CP_KEYBOARD, "&Keyboard");
    AppendMenu(hSub, MF_STRING, IDM_CP_MOUSE, "&Mouse");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_SOUND, "&Sound");
    AppendMenu(hSub, MF_STRING, IDM_CP_FONTS, "&Fonts");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_FOLDER_OPTIONS, "Folder &Options");
    AppendMenu(hSub, MF_STRING, IDM_CP_INTERNET_OPTIONS, "Internet &Options");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_USER_ACCOUNTS, "&User Accounts");
    AppendMenu(hSub, MF_STRING, IDM_CP_POWER_OPTIONS, "Power &Options");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_CP_FIREWALL, "Windows &Firewall");
    AppendMenu(hSub, MF_STRING, IDM_CP_ADMIN_TOOLS, "&Administrative Tools");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Control Panel");

    // ========== SETTINGS ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SET_HOME, "&Settings Home");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SET_DISPLAY, "&Display");
    AppendMenu(hSub, MF_STRING, IDM_SET_SOUND, "&Sound");
    AppendMenu(hSub, MF_STRING, IDM_SET_NOTIFICATIONS, "&Notifications");
    AppendMenu(hSub, MF_STRING, IDM_SET_POWER, "&Power & Sleep");
    AppendMenu(hSub, MF_STRING, IDM_SET_STORAGE, "&Storage");
    AppendMenu(hSub, MF_STRING, IDM_SET_ABOUT, "&About");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SET_BLUETOOTH, "&Bluetooth");
    AppendMenu(hSub, MF_STRING, IDM_SET_PRINTERS, "&Printers");
    AppendMenu(hSub, MF_STRING, IDM_SET_MOUSE, "&Mouse");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SET_NETWORK, "&Network");
    AppendMenu(hSub, MF_STRING, IDM_SET_WIFI, "&Wi-Fi");
    AppendMenu(hSub, MF_STRING, IDM_SET_VPN, "&VPN");
    AppendMenu(hSub, MF_STRING, IDM_SET_PROXY, "&Proxy");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SET_BACKGROUND, "&Background");
    AppendMenu(hSub, MF_STRING, IDM_SET_COLORS, "&Colors");
    AppendMenu(hSub, MF_STRING, IDM_SET_LOCK_SCREEN, "&Lock Screen");
    AppendMenu(hSub, MF_STRING, IDM_SET_THEMES, "&Themes");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SET_APPS, "&Apps");
    AppendMenu(hSub, MF_STRING, IDM_SET_ACCOUNTS, "&Accounts");
    AppendMenu(hSub, MF_STRING, IDM_SET_TIME, "&Time & Language");
    AppendMenu(hSub, MF_STRING, IDM_SET_GAMING, "&Gaming");
    AppendMenu(hSub, MF_STRING, IDM_SET_ACCESSIBILITY, "&Accessibility");
    AppendMenu(hSub, MF_STRING, IDM_SET_PRIVACY, "&Privacy");
    AppendMenu(hSub, MF_STRING, IDM_SET_UPDATE, "&Windows Update");
    AppendMenu(hSub, MF_STRING, IDM_SET_RECOVERY, "&Recovery");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Settings");

    // ========== NETWORK ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_NET_NETWORK_SETTINGS, "&Network Settings");
    AppendMenu(hSub, MF_STRING, IDM_NET_ADAPTER_SETTINGS, "&Adapter Settings");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_NET_WIFI_SETTINGS, "&Wi-Fi Settings");
    AppendMenu(hSub, MF_STRING, IDM_NET_VPN_SETTINGS, "&VPN Settings");
    AppendMenu(hSub, MF_STRING, IDM_NET_PROXY_SETTINGS, "&Proxy Settings");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_NET_IPCONFIG, "IP&Config");
    AppendMenu(hSub, MF_STRING, IDM_NET_PING_GOOGLE, "&Ping Google");
    AppendMenu(hSub, MF_STRING, IDM_NET_TRACERT, "&Tracert");
    AppendMenu(hSub, MF_STRING, IDM_NET_NETSTAT, "&Netstat");
    AppendMenu(hSub, MF_STRING, IDM_NET_FLUSH_DNS, "&Flush DNS");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_NET_BROWSER, "Open &Browser");
    AppendMenu(hSub, MF_STRING, IDM_NET_REMOTE_DESKTOP, "&Remote Desktop");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Network");

    // ========== POWER ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_POWER_SLEEP, "&Sleep");
    AppendMenu(hSub, MF_STRING, IDM_POWER_HIBERNATE, "&Hibernate");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_POWER_RESTART, "&Restart");
    AppendMenu(hSub, MF_STRING, IDM_POWER_SHUTDOWN, "&Shut Down");
    AppendMenu(hSub, MF_STRING, IDM_POWER_SIGNOUT, "&Sign Out");
    AppendMenu(hSub, MF_STRING, IDM_POWER_LOCK, "&Lock");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_POWER_POWER_OPTIONS, "Power &Options");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Power");

    // ========== DISPLAY ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_DISP_SETTINGS, "&Display Settings");
    AppendMenu(hSub, MF_STRING, IDM_DISP_PERSONALIZATION, "&Personalization");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DISP_BACKGROUND, "&Background");
    AppendMenu(hSub, MF_STRING, IDM_DISP_COLORS, "&Colors");
    AppendMenu(hSub, MF_STRING, IDM_DISP_LOCK_SCREEN, "&Lock Screen");
    AppendMenu(hSub, MF_STRING, IDM_DISP_THEMES, "&Themes");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DISP_RESOLUTION, "Screen &Resolution");
    AppendMenu(hSub, MF_STRING, IDM_DISP_NIGHT_LIGHT, "&Night Light");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Display");

    // ========== AUDIO ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_VOLUME_UP, "Volume &Up");
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_VOLUME_DOWN, "Volume &Down");
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_VOLUME_MUTE, "&Mute");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_VOLUME_MIXER, "Volume &Mixer");
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_SOUND_SETTINGS, "&Sound Settings");
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_PLAYBACK, "&Playback Devices");
    AppendMenu(hSub, MF_STRING, IDM_AUDIO_RECORDING, "&Recording Devices");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Audio");

    // ========== SECURITY ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SEC_WINDOWS_SECURITY, "&Windows Security");
    AppendMenu(hSub, MF_STRING, IDM_SEC_VIRUS_SCAN, "&Virus Scan");
    AppendMenu(hSub, MF_STRING, IDM_SEC_FIREWALL, "&Firewall");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SEC_UPDATE_SETTINGS, "&Update Settings");
    AppendMenu(hSub, MF_STRING, IDM_SEC_BACKUP, "&Backup");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SEC_TASK_MANAGER, "&Task Manager");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Security");

    // ========== ACCESSIBILITY ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_ACCESS_CENTER, "&Ease of Access Center");
    AppendMenu(hSub, MF_STRING, IDM_ACCESS_MAGNIFIER, "&Magnifier");
    AppendMenu(hSub, MF_STRING, IDM_ACCESS_NARRATOR, "&Narrator");
    AppendMenu(hSub, MF_STRING, IDM_ACCESS_ONSCREEN_KEYBOARD, "&On-Screen Keyboard");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Accessibility");

    // ========== DEVELOPER ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_DEV_CMD, "&Command Prompt");
    AppendMenu(hSub, MF_STRING, IDM_DEV_CMD_ADMIN, "Command Prompt (&Admin)");
    AppendMenu(hSub, MF_STRING, IDM_DEV_POWERSHELL, "&PowerShell");
    AppendMenu(hSub, MF_STRING, IDM_DEV_POWERSHELL_ADMIN, "PowerShell (&Admin)");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DEV_REGISTRY, "&Registry Editor");
    AppendMenu(hSub, MF_STRING, IDM_DEV_POLICY_EDITOR, "&Group Policy Editor");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Developer");

    // ========== ADMIN ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_COMPUTER_MGMT, "&Computer Management");
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_DEFRAG, "&Defragment");
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_DISK_CLEANUP, "Disk &Cleanup");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_CHKDSK, "&Check Disk");
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_SFC_SCANNOW, "&SFC Scannow");
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_DISM, "&DISM Check");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_ADMIN_MEMORY_DIAG, "&Memory Diagnostic");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Admin");

    // ========== APPS ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_APP_NOTEPAD, "&Notepad");
    AppendMenu(hSub, MF_STRING, IDM_APP_WORDPAD, "&WordPad");
    AppendMenu(hSub, MF_STRING, IDM_APP_PAINT, "&Paint");
    AppendMenu(hSub, MF_STRING, IDM_APP_CALCULATOR, "&Calculator");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_APP_SNIPPING_TOOL, "&Snipping Tool");
    AppendMenu(hSub, MF_STRING, IDM_APP_STICKY_NOTES, "S&ticky Notes");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_APP_EDGE, "Microsoft &Edge");
    AppendMenu(hSub, MF_STRING, IDM_APP_STORE, "Microsoft &Store");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Apps");

    // ========== SHELL ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_SHELL_RUN, "&Run...\tWin+R");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_SEARCH, "&Search\tWin+S");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_TASKMGR, "&Task Manager");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SHELL_SHOW_DESKTOP, "Show &Desktop\tWin+D");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_SETTINGS, "&Settings\tWin+I");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_FILE_EXPLORER, "File &Explorer\tWin+E");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_SHELL_CONTROL_PANEL, "&Control Panel");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_RECYCLE_BIN, "&Recycle Bin");
    AppendMenu(hSub, MF_STRING, IDM_SHELL_STARTUP, "&Startup Folder");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Shell");

    // ========== MAINTENANCE ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_MAINT_CLEAN_DISK, "&Disk Cleanup");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_DEFRAG, "&Defragment and Optimize Drives");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_CHKDSK, "&Check Disk (CHKDSK)");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_MAINT_SFC_SCANNOW, "&System File Checker (SFC)");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_DISM, "&DISM Check");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_MAINT_SYSTEM_RESTORE, "&System Restore");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_WINDOWS_BACKUP, "&Windows Backup");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_MAINT_TASK_SCHEDULER, "&Task Scheduler");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_PRINT_MANAGEMENT, "&Print Management");
    AppendMenu(hSub, MF_STRING, IDM_MAINT_SHARED_FOLDERS, "&Shared Folders");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_MAINT_RELIABILITY_MONITOR, "&Reliability Monitor");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Maintenance");

    // ========== TROUBLESHOOTING ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_SHOOTER, "&Windows Troubleshooter");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_EVENT_VIEWER, "&Event Viewer");
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_PERFORMANCE_MONITOR, "&Performance Monitor");
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_RESOURCE_MONITOR, "&Resource Monitor");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_MEMORY_DIAG, "&Memory Diagnostic");
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_STARTUP_REPAIR, "&Startup Repair");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_SYSTEM_CONFIG, "&System Configuration (MSConfig)");
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_DIRECTX_DIAG, "&DirectX Diagnostic Tool");
    AppendMenu(hSub, MF_STRING, IDM_TROUBLE_PROBLEM_STEPS, "&Problem Steps Recorder");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Troubleshooting");

    // ========== RUN DIALOG ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_RUN_CMD, "&Command Prompt (cmd)");
    AppendMenu(hSub, MF_STRING, IDM_RUN_POWERSHELL, "&PowerShell");
    AppendMenu(hSub, MF_STRING, IDM_RUN_REGEDIT, "&Registry Editor (regedit)");
    AppendMenu(hSub, MF_STRING, IDM_RUN_SERVICES, "&Services (services.msc)");
    AppendMenu(hSub, MF_STRING, IDM_RUN_TASKMGR, "&Task Manager (taskmgr)");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_RUN_SYSTEM_INFO, "&System Information (systeminfo)");
    AppendMenu(hSub, MF_STRING, IDM_RUN_MSINFO32, "&System Information (msinfo32)");
    AppendMenu(hSub, MF_STRING, IDM_RUN_WINVER, "&Windows Version (winver)");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Run Commands");

    // ========== SPECIAL FOLDERS ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_APPDATA, "&AppData (Roaming)");
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_LOCALAPPDATA, "&Local AppData");
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_PROGRAMDATA, "&ProgramData");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_STARTUP, "&Startup Folder");
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_TEMP, "&Temp Folder");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_WINDOWS, "&Windows Directory");
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_SYSTEM32, "&System32 Directory");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_SENDTO, "&SendTo Folder");
    AppendMenu(hSub, MF_STRING, IDM_FOLDER_QUICK_LAUNCH, "&Quick Launch");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Special Folders");

    // ========== HARDWARE ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_HW_DEVICE_MANAGER, "&Device Manager");
    AppendMenu(hSub, MF_STRING, IDM_HW_PRINTERS, "&Printers & Scanners");
    AppendMenu(hSub, MF_STRING, IDM_HW_GAME_CONTROLLERS, "&Game Controllers");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_HW_MOUSE, "&Mouse Settings");
    AppendMenu(hSub, MF_STRING, IDM_HW_KEYBOARD, "&Keyboard Settings");
    AppendMenu(hSub, MF_STRING, IDM_HW_SOUND, "&Sound Settings");
    AppendMenu(hSub, MF_STRING, IDM_HW_DISPLAY, "&Display Settings");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Hardware");

    // ========== DIAGNOSTICS ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_DIAG_DIRECTX, "&DirectX Diagnostic (dxdiag)");
    AppendMenu(hSub, MF_STRING, IDM_DIAG_WINDOWS_MEMORY, "&Windows Memory Diagnostic");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DIAG_RESOURCE_MONITOR, "&Resource Monitor");
    AppendMenu(hSub, MF_STRING, IDM_DIAG_PERFORMANCE_MONITOR, "&Performance Monitor");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_DIAG_RELIABILITY, "&Reliability Monitor");
    AppendMenu(hSub, MF_STRING, IDM_DIAG_SYSTEM_INFO, "&System Information");
    AppendMenu(hSub, MF_STRING, IDM_DIAG_MSINFO32, "&System Information (msinfo32)");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Diagnostics");

    // ========== WINDOWS FEATURES ==========
    hSub = CreatePopupMenu();
    AppendMenu(hSub, MF_STRING, IDM_WF_TURN_FEATURES, "&Turn Windows features on or off");
    AppendMenu(hSub, MF_STRING, IDM_WF_OPTIONAL_FEATURES, "&Optional Features");
    AppendMenu(hSub, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSub, MF_STRING, IDM_WF_INTERNET_EXPLORER, "&Internet Explorer");
    AppendMenu(hSub, MF_STRING, IDM_WF_MEDIA_PLAYER, "&Windows Media Player");
    AppendMenu(hSub, MF_STRING, IDM_WF_MICROSOFT_STORE, "&Microsoft Store");
    AppendMenu(hSub, MF_STRING, IDM_WF_XBOX_GAME_BAR, "&Xbox Game Bar");
    AppendMenu(hPopup, MF_POPUP, (UINT_PTR)hSub, "&Windows Features");

    // Append all new extended menus
    AppendNewMenus(hPopup);

    // ========== TOP LEVEL ==========
    AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenu(hPopup, MF_STRING, IDM_TOP_SYSTEM_INFO, "System &Information");
    AppendMenu(hPopup, MF_STRING, IDM_TOP_RESTART_EXPLORER, "&Restart Explorer");
    AppendMenu(hPopup, MF_STRING, IDM_TOP_EMPTY_RECYCLE, "&Empty Recycle Bin");
    AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenu(hPopup, MF_STRING, IDM_TOP_EDIT_HOSTS, "Edit &Hosts File");
    AppendMenu(hPopup, MF_STRING, IDM_TOP_EDIT_REGISTRY, "Edit &Registry");
    AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenu(hPopup, MF_STRING, IDM_TOP_ABOUT, "&About");
    AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
    AppendMenu(hPopup, MF_STRING, IDM_TOP_EXIT, "E&xit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    UINT uCmd = TrackPopupMenu(hPopup, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                               pt.x, pt.y, 0, hWnd, NULL);
    if (uCmd) PostMessage(hWnd, WM_COMMAND, uCmd, 0);
    DestroyMenu(hPopup);
}

void ProcessCommand(WPARAM wParam) {
    CommandHandler h = FindHandler(LOWORD(wParam));
    if (h) h();
}