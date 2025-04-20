// HexViewerMDI.cpp - Main application file for the hex viewer with MDI support

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <Ole2.h>
#include "includes.h"

// Ensure common controls are initialized
#pragma comment(lib, "comctl32.lib")

// Define window class names
#define MAIN_WINDOW_CLASS    "HexViewerMDIMainClass"
#define HEX_VIEWER_CLASS     "HexViewerWindowClass"
#define DOCK_WINDOW_CLASS    "HexViewerDockWindowClass"

#define IDC_GRID 1004

// Menu IDs
#define IDM_FILE_NEW         2000
#define IDM_FILE_OPEN        2001
#define IDM_FILE_EXIT        2002
#define IDM_WINDOW_CASCADE   2010
#define IDM_WINDOW_TILE      2011
#define IDM_WINDOW_ARRANGE   2012
#define IDM_FILE_SAVE_ANNOTATIONS   2020
#define IDM_FILE_LOAD_ANNOTATIONS   2021

// Version number for annotation file format
const int ANNOTATION_FILE_VERSION = 1;

// Annotation file header
struct AnnotationFileHeader {
    char signature[4];        // "HVA\0" (Hex Viewer Annotations)
    int version;              // File format version
    int annotationCount;      // Number of annotations
    int reserved[4];          // Reserved for future use
};

// Global variables
HINSTANCE g_hInstance;
HWND g_hMainWindow;
HWND g_hMDIClient;
HWND g_hDockWindow;
HWND g_hGridView;
HWND g_hActiveHexViewer = NULL;
int g_dockWidth = 250;

// Forward declarations
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HexViewerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DockWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND CreateHexViewerWindow(HWND hMDIClient, const char* fileName);
HWND CreateDockWindow(HWND hParent);
HWND CreateGridView(HWND hParent);
void OpenFileInNewWindow(HWND hWnd);
void UpdateGridView(HWND hGridView, int offset, const std::vector<BYTE>& data);
const char* GetDataTypeName(DataType type);
std::string FormatDataAtOffset(const std::vector<BYTE>& data, int offset, DataType type);


bool SaveAnnotationsToFile(HWND hwnd, DocumentWindowState& state);
bool LoadAnnotationsFromFile(HWND hwnd, DocumentWindowState& state);


// Each window has its own state
std::unordered_map<HWND, DocumentWindowState*> windowStates;

//-------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    // Register the main window class
    WNDCLASS wcMain = {};
    wcMain.style = CS_HREDRAW | CS_VREDRAW;
    wcMain.lpfnWndProc = MainWndProc;
    wcMain.hInstance = hInstance;
    wcMain.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcMain.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wcMain.lpszClassName = MAIN_WINDOW_CLASS;
    RegisterClass(&wcMain);

    // Register the hex viewer window class
    WNDCLASS wcHex = {};
    wcHex.style = CS_HREDRAW | CS_VREDRAW;
    wcHex.lpfnWndProc = HexViewerProc;
    wcHex.hInstance = hInstance;
    wcHex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcHex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcHex.lpszClassName = HEX_VIEWER_CLASS;
    RegisterClass(&wcHex);

    // Register the dock window class
    WNDCLASS wcDock = {};
    wcDock.style = CS_HREDRAW | CS_VREDRAW;
    wcDock.lpfnWndProc = DockWndProc;
    wcDock.hInstance = hInstance;
    wcDock.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcDock.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcDock.lpszClassName = DOCK_WINDOW_CLASS;
    RegisterClass(&wcDock);

    // Create the main window
    g_hMainWindow = CreateWindowEx(
        0,
        MAIN_WINDOW_CLASS,
        "Hex Viewer MDI Application",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hMainWindow)
        return 0;

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Main message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Handle MDI accelerators if needed
        if (!TranslateMDISysAccel(g_hMDIClient, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

//-------------------------------------------------------------------
// Main window procedure
//-------------------------------------------------------------------
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hMDIClient;
    static HWND hDockWindow;

    switch (msg) {
    case WM_CREATE:
    {
        // Create menus
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreatePopupMenu();
        HMENU hWindowMenu = CreatePopupMenu();

        //AppendMenu(hFileMenu, MF_STRING, IDM_FILE_NEW, "New");
        AppendMenu(hFileMenu, MF_STRING, IDM_FILE_OPEN, "Open...");
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFileMenu, MF_STRING, IDM_FILE_SAVE_ANNOTATIONS, "Save Annotations...");
        AppendMenu(hFileMenu, MF_STRING, IDM_FILE_LOAD_ANNOTATIONS, "Load Annotations...");
        
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, "Exit");

        AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_CASCADE, "Cascade");
        AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_TILE, "Tile");
        AppendMenu(hWindowMenu, MF_STRING, IDM_WINDOW_ARRANGE, "Arrange Icons");

        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hWindowMenu, "Window");

        SetMenu(hwnd, hMenu);

        // Create the dock window on the right side
        hDockWindow = CreateDockWindow(hwnd);
        g_hDockWindow = hDockWindow;

        // Create MDI Client
        CLIENTCREATESTRUCT ccs;
        ccs.hWindowMenu = GetSubMenu(hMenu, 1); // Window menu is at index 1
        ccs.idFirstChild = 1000; // First child window ID

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        hMDIClient = CreateWindow(
            "MDICLIENT",
            NULL,
            WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL | WS_VISIBLE,
            0, 0, clientRect.right - g_dockWidth, clientRect.bottom,
            hwnd,
            NULL,
            g_hInstance,
            (LPVOID)&ccs
        );

        g_hMDIClient = hMDIClient;
        return 0;
    }

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Resize the MDI client window and the dock window
        MoveWindow(g_hMDIClient, 0, 0, width - g_dockWidth, height, TRUE);
        MoveWindow(g_hDockWindow, width - g_dockWidth, 0, g_dockWidth, height, TRUE);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW:
            // Create a new hex viewer window (empty)
            CreateHexViewerWindow(g_hMDIClient, NULL);
            break;

        case IDM_FILE_OPEN:
            OpenFileInNewWindow(hwnd);
            break;

        case IDM_FILE_SAVE_ANNOTATIONS:
        {
            HWND hActiveChild = (HWND)SendMessage(g_hMDIClient, WM_MDIGETACTIVE, 0, 0);
            if (hActiveChild && g_hActiveHexViewer == hActiveChild) {
                // Find the state for this window
                auto it = windowStates.find(hActiveChild);
                if (it != windowStates.end()) {
                    DocumentWindowState* pState = it->second;
                    SaveAnnotationsToFile(hwnd, *pState);
                }
            }
            else {
                MessageBox(hwnd, "Please activate a hex viewer window first.", "Save Annotations", MB_OK | MB_ICONINFORMATION);
            }
        }
        break;

        case IDM_FILE_LOAD_ANNOTATIONS:
        {
            HWND hActiveChild = (HWND)SendMessage(g_hMDIClient, WM_MDIGETACTIVE, 0, 0);
            if (hActiveChild && g_hActiveHexViewer == hActiveChild) {
                // Find the state for this window
                auto it = windowStates.find(hActiveChild);
                if (it != windowStates.end()) {
                    DocumentWindowState* pState = it->second;
                    LoadAnnotationsFromFile(hwnd, *pState);
                }
            }
            else {
                MessageBox(hwnd, "Please activate a hex viewer window first.", "Load Annotations", MB_OK | MB_ICONINFORMATION);
            }
        }
        break;

        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_WINDOW_CASCADE:
            SendMessage(g_hMDIClient, WM_MDICASCADE, 0, 0);
            break;

        case IDM_WINDOW_TILE:
            SendMessage(g_hMDIClient, WM_MDITILE, MDITILE_HORIZONTAL, 0);
            break;

        case IDM_WINDOW_ARRANGE:
            SendMessage(g_hMDIClient, WM_MDIICONARRANGE, 0, 0);
            break;
        }
        
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefFrameProc(hwnd, g_hMDIClient, msg, wParam, lParam);
}



//-------------------------------------------------------------------
// Dock Window Procedure
//-------------------------------------------------------------------
LRESULT CALLBACK DockWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
    {
        // Create the grid view that shows data interpretations
        g_hGridView = CreateGridView(hwnd);
        return 0;
    }

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Resize the grid view to fill the dock window
        MoveWindow(g_hGridView, 0, 0, width, height, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Paint background
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(hdc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//-------------------------------------------------------------------
// Create Hex Viewer Window
//-------------------------------------------------------------------
//-------------------------------------------------------------------
// Create Hex Viewer Window (continued)
//-------------------------------------------------------------------
HWND CreateHexViewerWindow(HWND hMDIClient, const char* fileName) {
    static int childWindowCount = 0;
    char title[100];

    if (fileName) {
        // Use the filename as the title
        sprintf_s(title, sizeof(title), "Hex View - %s", fileName);
    }
    else {
        // Use a generic title
        sprintf_s(title, sizeof(title), "Hex View - Untitled %d", ++childWindowCount);
    }

    // Create the MDI child window
    MDICREATESTRUCT mcs;
    mcs.szClass = HEX_VIEWER_CLASS;
    mcs.szTitle = title;
    mcs.hOwner = g_hInstance;
    mcs.x = mcs.y = CW_USEDEFAULT;
    mcs.cx = mcs.cy = CW_USEDEFAULT;
    mcs.style = WS_VISIBLE | WS_BORDER;
    mcs.lParam = (LPARAM)fileName;

    HWND hChild = (HWND)SendMessage(hMDIClient, WM_MDICREATE, 0, (LPARAM)&mcs);

    // Set as active MDI child
    SendMessage(hMDIClient, WM_MDIACTIVATE, (WPARAM)hChild, 0);

    return hChild;
}

//-------------------------------------------------------------------
// Create Dock Window
//-------------------------------------------------------------------
HWND CreateDockWindow(HWND hParent) {
    // Create the dock window
    HWND hDock = CreateWindowEx(
        0,
        DOCK_WINDOW_CLASS,
        "Data Interpreter",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, g_dockWidth, 0, // Width will be set in WM_SIZE
        hParent,
        NULL,
        g_hInstance,
        NULL
    );

    return hDock;
}

//-------------------------------------------------------------------
// Create Grid View
//-------------------------------------------------------------------
HWND CreateGridView(HWND hParent) {
    // Create the list view control
    HWND hListView = CreateWindowEx(
        0,
        WC_LISTVIEW,
        "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        0, 0, 0, 0, // Will be sized by parent's WM_SIZE
        hParent,
        (HMENU)IDC_GRID,
        g_hInstance,
        NULL
    );

    // Set extended list view styles
    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    
    // Add columns
    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    // Type column
    lvc.iSubItem = 0;
    lvc.cx = 100;
    lvc.pszText = (LPSTR)"Type";
    ListView_InsertColumn(hListView, 0, &lvc);

    // Value column
    lvc.iSubItem = 1;
    lvc.cx = 150;
    lvc.pszText = (LPSTR)"Value";
    ListView_InsertColumn(hListView, 1, &lvc);

    // Add rows for each data type
    LVITEM lvi;
    lvi.mask = LVIF_TEXT;
    lvi.iSubItem = 0;

    for (int i = 0; i < DT_COUNT; i++) {
        lvi.iItem = i;
        lvi.pszText = (LPSTR)GetDataTypeName((DataType)i);
        ListView_InsertItem(hListView, &lvi);
    }

    return hListView;
}

//-------------------------------------------------------------------
// Open File in New Window
//-------------------------------------------------------------------
void OpenFileInNewWindow(HWND hWnd) {
    char fileName[MAX_PATH] = {};

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        // Create a new hex viewer window with this file
        CreateHexViewerWindow(g_hMDIClient, fileName);
    }
}

//-------------------------------------------------------------------
// Update Grid View with data from current selection
//-------------------------------------------------------------------
void UpdateGridView(HWND hGridView, int offset, const std::vector<BYTE>& data) {
    static int lastOffset = -1;
    if (!hGridView || offset < 0 || offset >= data.size())
        return;
    
    if (offset == lastOffset)
        return;

    lastOffset = offset;

    SendMessage(hGridView, WM_SETREDRAW, FALSE, 0);
    // Update each row with the interpreted value
    for (int i = 0; i < DT_COUNT; i++) {
        std::string value = FormatDataAtOffset(data, offset, (DataType)i);
        ListView_SetItemText(hGridView, i, 1, (LPSTR)value.c_str());
    }
    SendMessage(hGridView, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hGridView, NULL, TRUE);
    
}

//-------------------------------------------------------------------
// Get Data Type Name
//-------------------------------------------------------------------
const char* GetDataTypeName(DataType type) {
    switch (type) {
    case DT_BYTE: return "Byte (8-bit)";
    case DT_UBYTE: return "Unsigned Byte";
    case DT_SHORT: return "Short (16-bit)";
    case DT_USHORT: return "Unsigned Short";
    case DT_INT: return "Int (32-bit)";
    case DT_UINT: return "Unsigned Int";
    case DT_FLOAT: return "Float";
    case DT_DOUBLE: return "Double";
    case DT_STRING: return "ASCII String";
    case DT_UNICODE_STRING: return "Unicode String";
    case DT_TIME_T: return "time_t";
    case DT_TIME64_T: return "time64_t";
    case DT_OLETIME: return "OLE Time";
    default: return "Unknown";
    }
}

//-------------------------------------------------------------------
// Format Data At Offset for a specific type
//-------------------------------------------------------------------
std::string FormatDataAtOffset(const std::vector<BYTE>& data, int offset, DataType type) {
    std::stringstream ss;
    size_t remainingBytes = data.size() - offset;

    try {
        switch (type) {
        case DT_BYTE:
        {
            if (remainingBytes >= 1) {
                int8_t value = static_cast<int8_t>(data[offset]);
                ss << static_cast<int>(value) << " (0x" << std::hex << static_cast<int>(static_cast<uint8_t>(value)) << ")";
            }
            break;
        }
        case DT_UBYTE:
        {
            if (remainingBytes >= 1) {
                uint8_t value = data[offset];
                ss << static_cast<int>(value) << " (0x" << std::hex << static_cast<int>(value) << ")";
            }
            break;
        }
        case DT_SHORT:
        {
            if (remainingBytes >= 2) {
                int16_t value;
                memcpy(&value, &data[offset], sizeof(int16_t));
                ss << value << " (0x" << std::hex << static_cast<int>(value) << ")";
            }
            break;
        }
        case DT_USHORT:
        {
            if (remainingBytes >= 2) {
                uint16_t value;
                memcpy(&value, &data[offset], sizeof(uint16_t));
                ss << value << " (0x" << std::hex << value << ")";
            }
            break;
        }
        case DT_INT:
        {
            if (remainingBytes >= 4) {
                int32_t value;
                memcpy(&value, &data[offset], sizeof(int32_t));
                ss << value << " (0x" << std::hex << value << ")";
            }
            break;
        }
        case DT_UINT:
        {
            if (remainingBytes >= 4) {
                uint32_t value;
                memcpy(&value, &data[offset], sizeof(uint32_t));
                ss << value << " (0x" << std::hex << value << ")";
            }
            break;
        }
        case DT_FLOAT:
        {
            if (remainingBytes >= 4) {
                float value;
                memcpy(&value, &data[offset], sizeof(float));
                ss << std::fixed << std::setprecision(6) << value;
            }
            break;
        }
        case DT_DOUBLE:
        {
            if (remainingBytes >= 8) {
                double value;
                memcpy(&value, &data[offset], sizeof(double));
                ss << std::fixed << std::setprecision(10) << value;
            }
            break;
        }
        case DT_STRING:
        {
            // Display up to 32 chars of ASCII string
            const int MAX_STRING_LENGTH = 32;
            std::string result;
            for (size_t i = 0; i < MAX_STRING_LENGTH && (offset + i) < data.size(); i++) {
                uint8_t ch = data[offset + i];
                if (ch == 0) break; // Stop at null terminator
                if (ch >= 32 && ch <= 126) { // Printable ASCII
                    result += static_cast<char>(ch);
                }
                else {
                    result += '.'; // Non-printable
                }
            }
            ss << result;
            break;
        }
        case DT_UNICODE_STRING:
        {
            // Display up to 16 wide chars of Unicode string
            const int MAX_WSTRING_LENGTH = 16;
            std::string result;
            for (size_t i = 0; i < MAX_WSTRING_LENGTH && (offset + i * 2 + 1) < data.size(); i++) {
                wchar_t ch;
                memcpy(&ch, &data[offset + i * 2], sizeof(wchar_t));
                if (ch == 0) break; // Stop at null terminator
                if (ch >= 32 && ch <= 126) { // Basic Latin Unicode
                    result += static_cast<char>(ch);
                }
                else {
                    result += '.'; // Non-printable
                }
            }
            ss << result;
            break;
        }
        case DT_TIME_T:
        {
            if (remainingBytes >= 4) {
                time_t value;
                memcpy(&value, &data[offset], sizeof(time_t));
                char buffer[100];
                struct tm timeinfo;
                gmtime_s(&timeinfo, &value);

                sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

                ss << buffer;
            }
            break;
        }
        case DT_TIME64_T:
        {
            if (remainingBytes >= 8) {
                __time64_t value;
                memcpy(&value, &data[offset], sizeof(__time64_t));
                char buffer[100];
                struct tm timeinfo;
                _gmtime64_s(&timeinfo, &value);

                sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

                ss << buffer;
            }
            break;
        }
        case DT_OLETIME:
        {
            if (remainingBytes >= 8) {
                double value;
                memcpy(&value, &data[offset], sizeof(double));

                // OLE automation date (days since December 30, 1899)
                SYSTEMTIME sysTime;
                VariantTimeToSystemTime(value, &sysTime);

                char buffer[100];
                sprintf_s(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                    sysTime.wYear, sysTime.wMonth, sysTime.wDay,
                    sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
                ss << buffer;
            }
            break;
        }
        default:
            ss << "Unknown type";
        }
    }
    catch (const std::exception& e) {
        ss << "Error: " << e.what();
    }

    // If no value could be formatted
    if (ss.str().empty()) {
        return "Not enough data";
    }

    return ss.str();
}


bool SaveAnnotationsToFile(HWND hwnd, DocumentWindowState& state) {
    if (state.annotations.empty()) {
        MessageBox(hwnd, "No annotations to save.", "Save Annotations", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    // Get a filename from the user
    char fileName[MAX_PATH] = {};

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Hex Viewer Annotations (*.hva)\0*.hva\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "hva";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileName(&ofn)) {
        return false; // User cancelled
    }

    // Open the file for writing
    std::ofstream file(fileName, std::ios::binary);
    if (!file) {
        MessageBox(hwnd, "Failed to open file for writing.", "Save Annotations", MB_OK | MB_ICONERROR);
        return false;
    }

    try {
        // Write file header
        AnnotationFileHeader header;
        memset(&header, 0, sizeof(header));

        // Set signature "HVA\0"
        header.signature[0] = 'H';
        header.signature[1] = 'V';
        header.signature[2] = 'A';
        header.signature[3] = '\0';

        header.version = ANNOTATION_FILE_VERSION;
        header.annotationCount = static_cast<int>(state.annotations.size());

        file.write(reinterpret_cast<char*>(&header), sizeof(header));

        // Write the filename that these annotations belong to
        int fileNameLength = static_cast<int>(state.fileName.length());
        file.write(reinterpret_cast<char*>(&fileNameLength), sizeof(fileNameLength));
        file.write(state.fileName.c_str(), fileNameLength);

        // Write each annotation
        for (const auto& anno : state.annotations) {
            // Write offsets
            file.write(reinterpret_cast<const char*>(&anno.startOffset), sizeof(anno.startOffset));
            file.write(reinterpret_cast<const char*>(&anno.endOffset), sizeof(anno.endOffset));

            // Write color
            file.write(reinterpret_cast<const char*>(&anno.color), sizeof(anno.color));

            // Write label string
            int labelLength = static_cast<int>(anno.label.length());
            file.write(reinterpret_cast<char*>(&labelLength), sizeof(labelLength));
            file.write(anno.label.c_str(), labelLength);

            // Write format string
            int formatLength = static_cast<int>(anno.displayFormat.length());
            file.write(reinterpret_cast<char*>(&formatLength), sizeof(formatLength));
            file.write(anno.displayFormat.c_str(), formatLength);
        }

        file.close();

        MessageBox(hwnd, "Annotations saved successfully.", "Save Annotations", MB_OK | MB_ICONINFORMATION);
        return true;
    }
    catch (const std::exception& e) {
        MessageBox(hwnd, e.what(), "Error Saving Annotations", MB_OK | MB_ICONERROR);
        return false;
    }
}

//-------------------------------------------------------------------
// Load annotations from a file
//-------------------------------------------------------------------
bool LoadAnnotationsFromFile(HWND hwnd, DocumentWindowState& state) {
    // Get a filename from the user
    char fileName[MAX_PATH] = {};

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Hex Viewer Annotations (*.hva)\0*.hva\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (!GetOpenFileName(&ofn)) {
        return false; // User cancelled
    }

    // Open the file for reading
    std::ifstream file(fileName, std::ios::binary);
    if (!file) {
        MessageBox(hwnd, "Failed to open file for reading.", "Load Annotations", MB_OK | MB_ICONERROR);
        return false;
    }

    try {
        // Read and verify file header
        AnnotationFileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Check signature
        if (header.signature[0] != 'H' || header.signature[1] != 'V' ||
            header.signature[2] != 'A' || header.signature[3] != '\0') {
            MessageBox(hwnd, "Invalid annotation file format.", "Load Annotations", MB_OK | MB_ICONERROR);
            return false;
        }

        // Check version
        if (header.version > ANNOTATION_FILE_VERSION) {
            MessageBox(hwnd, "Annotation file was created with a newer version of this application.",
                "Load Annotations", MB_OK | MB_ICONWARNING);
            // Continue anyway, we'll read what we can understand
        }

        // Read the filename that these annotations belong to
        int fileNameLength;
        file.read(reinterpret_cast<char*>(&fileNameLength), sizeof(fileNameLength));

        std::vector<char> fileNameBuffer(fileNameLength + 1, 0);
        file.read(fileNameBuffer.data(), fileNameLength);
        std::string storedFileName(fileNameBuffer.data(), fileNameLength);

        // Check if the annotations match the current file
        if (storedFileName != state.fileName) {
            // Ask user if they want to continue even though filenames don't match
            if (MessageBox(hwnd,
                "The annotations were saved for a different file. Load anyway?",
                "Load Annotations",
                MB_YESNO | MB_ICONQUESTION) == IDNO) {
                return false;
            }
        }

        // Clear existing annotations if successful
        std::vector<Annotation> newAnnotations;

        // Read each annotation
        for (int i = 0; i < header.annotationCount; i++) {
            Annotation anno;

            // Read offsets
            file.read(reinterpret_cast<char*>(&anno.startOffset), sizeof(anno.startOffset));
            file.read(reinterpret_cast<char*>(&anno.endOffset), sizeof(anno.endOffset));

            // Check if offsets are valid for this file
            if (anno.startOffset < 0 || anno.endOffset < 0 ||
                anno.startOffset >= state.fileData.size() ||
                anno.endOffset >= state.fileData.size() ||
                anno.startOffset > anno.endOffset) {
                // Skip this annotation
                continue;
            }

            // Read color
            file.read(reinterpret_cast<char*>(&anno.color), sizeof(anno.color));

            // Read label string
            int labelLength;
            file.read(reinterpret_cast<char*>(&labelLength), sizeof(labelLength));

            std::vector<char> labelBuffer(labelLength + 1, 0);
            file.read(labelBuffer.data(), labelLength);
            anno.label = std::string(labelBuffer.data(), labelLength);

            // Read format string
            int formatLength;
            file.read(reinterpret_cast<char*>(&formatLength), sizeof(formatLength));

            std::vector<char> formatBuffer(formatLength + 1, 0);
            file.read(formatBuffer.data(), formatLength);
            anno.displayFormat = std::string(formatBuffer.data(), formatLength);

            // Add the annotation to our new list
            newAnnotations.push_back(anno);
        }

        // If we got here without exceptions, update the state
        state.annotations = newAnnotations;

        // Redraw to show the loaded annotations
        if (g_hActiveHexViewer) {
            InvalidateRect(g_hActiveHexViewer, NULL, TRUE);
        }

        MessageBox(hwnd, "Annotations loaded successfully.", "Load Annotations", MB_OK | MB_ICONINFORMATION);
        return true;
    }
    catch (const std::exception& e) {
        MessageBox(hwnd, e.what(), "Error Loading Annotations", MB_OK | MB_ICONERROR);
        return false;
    }
}