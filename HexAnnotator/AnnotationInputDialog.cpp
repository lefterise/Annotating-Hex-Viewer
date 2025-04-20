#include <Windows.h>
extern HINSTANCE g_hInstance;

// Control IDs
#define IDC_EDIT1 1001
#define IDC_COMBO1 1002
#define IDC_STATIC 1003

INT_PTR CALLBACK InputDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

//-------------------------------------------------------------------
// Simple Dialog for Annotations - Show the dialog to create/edit annotation
//-------------------------------------------------------------------
void ShowAnnotationInputDialog(HWND hwnd, char* buffer, int bufferSize, char* format, int formatSize)
{
    // If we don't have direct access to resources, we need to create the dialog programmatically
    HINSTANCE hInstance = g_hInstance;

    // Create the dialog template dynamically
    LPDLGTEMPLATE lpdt;
    LPDLGITEMTEMPLATE lpdit;
    LPWORD lpw;
    LPWSTR lpwsz;
    int nchar;

    // Allocate memory for the dialog template
    lpdt = (LPDLGTEMPLATE)GlobalAlloc(GPTR, 2048 + 1000); // Increased size for more controls

    // Define the dialog template
    lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | DS_CENTER | DS_SETFONT;
    lpdt->cdit = 6;  // Number of controls (including 2 static texts)
    lpdt->x = 10;
    lpdt->y = 10;
    lpdt->cx = 180;
    lpdt->cy = 80;

    // Skip past the header to start defining items
    lpw = (LPWORD)(lpdt + 1);

    // No menu
    *lpw++ = 0;

    // Use default dialog box class
    *lpw++ = 0;

    // Set dialog title
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "Create Annotation", -1, lpwsz, 50);
    lpw += nchar;

    *lpw++ = 8; // Font size (in points)

    // Font name (modern default)
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "MS Shell Dlg 2", -1, lpwsz, 50);
    lpw += nchar;

    // First item: static text (prompt)
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 10;
    lpdit->y = 10;
    lpdit->cx = 40;
    lpdit->cy = 15;
    lpdit->id = IDC_STATIC;
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;    // Static control

    // Static text content
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "Label:", -1, lpwsz, 100);
    lpw += nchar;
    *lpw++ = 0;

    // Second item: edit control Annotation Label
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 50;
    lpdit->y = 7;
    lpdit->cx = 120;
    lpdit->cy = 14;
    lpdit->id = IDC_EDIT1;
    lpdit->style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0081;    // Edit control
    *lpw++ = 0;
    *lpw++ = 0;

    // Third item: static text (format label)
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 10;
    lpdit->y = 26;
    lpdit->cx = 40;
    lpdit->cy = 15;
    lpdit->id = IDC_STATIC;
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;    // Static control

    // Static text content
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "Format:", -1, lpwsz, 100);
    lpw += nchar;
    *lpw++ = 0;

    // Fourth item: Format Combo box
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 50;
    lpdit->y = 26;
    lpdit->cx = 120;
    lpdit->cy = 100; // Height includes dropdown portion
    lpdit->id = IDC_COMBO1;
    lpdit->style = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0085;    // Combo box control
    *lpw++ = 0;
    *lpw++ = 0;

    // Fifth item: OK button
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 35;
    lpdit->y = 50;
    lpdit->cx = 50;
    lpdit->cy = 20;
    lpdit->id = IDOK;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // Button control

    // Button text
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "OK", -1, lpwsz, 50);
    lpw += nchar;
    *lpw++ = 0;

    // Sixth item: Cancel button
    lpw = (LPWORD)((((ULONG_PTR)lpw + 3) & ~3)); // Align DWORD
    lpdit = (LPDLGITEMTEMPLATE)lpw;
    lpdit->x = 95;
    lpdit->y = 50;
    lpdit->cx = 50;
    lpdit->cy = 20;
    lpdit->id = IDCANCEL;
    lpdit->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;

    lpw = (LPWORD)(lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0080;    // Button control

    // Button text
    lpwsz = (LPWSTR)lpw;
    nchar = MultiByteToWideChar(CP_ACP, 0, "Cancel", -1, lpwsz, 50);
    lpw += nchar;
    *lpw++ = 0;

    // Prepare dialog parameters
    LPARAM dialogParams[4] = { (LPARAM)buffer, (LPARAM)bufferSize, (LPARAM)format, (LPARAM)formatSize };

    // Display dialog and get result
    INT_PTR result = DialogBoxIndirectParam(hInstance, lpdt, hwnd, (DLGPROC)InputDialogProc, (LPARAM)dialogParams);

    // Free the template memory
    GlobalFree(lpdt);

    // If dialog was canceled, clear the buffer
    if (result != IDOK) {
        buffer[0] = '\0';
        if (format)
            format[0] = '\0';
    }
}

// Simple dialog procedure for the annotation input dialog
INT_PTR CALLBACK InputDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static char* pBuffer = NULL;
    static int bufferSize = 0;
    static char* pFormat = NULL;
    static int formatSize = 0;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        // Store the buffer pointers and sizes from lParam
        LPARAM* params = reinterpret_cast<LPARAM*>(lParam);
        pBuffer = reinterpret_cast<char*>(params[0]);
        bufferSize = static_cast<int>(params[1]);
        pFormat = reinterpret_cast<char*>(params[2]);
        formatSize = static_cast<int>(params[3]);

        // Set the initial text if provided
        if (pBuffer && pBuffer[0] != '\0') {
            SetDlgItemText(hwndDlg, IDC_EDIT1, pBuffer);
        }

        // Populate the combo box with format options
        HWND hCombo = GetDlgItem(hwndDlg, IDC_COMBO1);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"hex");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"int");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"float");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"ascii");

        // Select the current format if provided
        if (pFormat && pFormat[0] != '\0') {
            int index = -1;
            if (strcmp(pFormat, "hex") == 0) index = 0;
            else if (strcmp(pFormat, "int") == 0) index = 1;
            else if (strcmp(pFormat, "float") == 0) index = 2;
            else if (strcmp(pFormat, "ascii") == 0) index = 3;

            if (index >= 0) {
                SendMessage(hCombo, CB_SETCURSEL, index, 0);
            }
        }
        else {
            // Default to hex
            SendMessage(hCombo, CB_SETCURSEL, 0, 0);
        }

        // Center the dialog on its parent
        HWND hwndParent = GetParent(hwndDlg);
        RECT rcParent, rcDlg, rc;
        GetWindowRect(hwndParent, &rcParent);
        GetWindowRect(hwndDlg, &rcDlg);
        CopyRect(&rc, &rcParent);

        int x = (rc.right + rc.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = (rc.bottom + rc.top - (rcDlg.bottom - rcDlg.top)) / 2;

        SetWindowPos(hwndDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);

        // Set focus to the edit control
        SetFocus(GetDlgItem(hwndDlg, IDC_EDIT1));
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            // Copy the edit control text to the buffer
            if (pBuffer && bufferSize > 0) {
                GetDlgItemText(hwndDlg, IDC_EDIT1, pBuffer, bufferSize);
            }

            // Get the selected format
            if (pFormat && formatSize > 0) {
                HWND hCombo = GetDlgItem(hwndDlg, IDC_COMBO1);
                int index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);

                switch (index) {
                case 0: strcpy_s(pFormat, formatSize, "hex"); break;
                case 1: strcpy_s(pFormat, formatSize, "int"); break;
                case 2: strcpy_s(pFormat, formatSize, "float"); break;
                case 3: strcpy_s(pFormat, formatSize, "ascii"); break;
                default: strcpy_s(pFormat, formatSize, "hex"); break;
                }
            }

            EndDialog(hwndDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}
