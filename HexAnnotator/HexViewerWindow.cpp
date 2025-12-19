#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "includes.h"

// Constants for visualization - adjusted for better spacing
const int BYTES_PER_ROW = 16;
const int CHAR_WIDTH = 12;
const int ROW_HEIGHT = 32;
const int OFFSET_MARGIN = 80;
const int HEX_MARGIN = 100;
const int HEX_BYTE_SPACING = 20;
const int ASCII_MARGIN = 440;
const int ANNOTATION_MARGIN = 3;

extern std::unordered_map<HWND, DocumentWindowState*> windowStates;
extern HWND g_hActiveHexViewer;
extern HWND g_hGridView;

void UpdateGridView(HWND hGridView, int offset, const std::vector<BYTE>& data);
void DrawHexView(HWND hwnd, HDC hdc, DocumentWindowState& state);
void DrawAnnotations(HWND hwnd, HDC hdc, DocumentWindowState& state);
std::string FormatData(const std::vector<BYTE>& data, int offset, int length, const std::string& format);
void ShowContextMenu(HWND hwnd, int x, int y, DocumentWindowState& state);
void CreateAnnotation(HWND hwnd, DocumentWindowState& state);
void EditAnnotation(HWND hwnd, int index, DocumentWindowState& state);
void ShowAnnotationInputDialog(HWND hwnd, char* buffer, int bufferSize, char* format, int formatSize);
void tagBytesThatAreAnnotated(DocumentWindowState& state);
void UpdateStatusbar(int offset, int length);
//-------------------------------------------------------------------
// Hex Viewer Window Procedure
//-------------------------------------------------------------------
LRESULT CALLBACK HexViewerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Get the state for this window
    DocumentWindowState* pState = NULL;
    auto it = windowStates.find(hwnd);
    if (it != windowStates.end()) {
        pState = it->second;
    }

    switch (msg) {
    case WM_CREATE:
    {
        // Create a new state for this window
        DocumentWindowState* newState = new DocumentWindowState();
        windowStates[hwnd] = newState;
        pState = newState;

        // Get the filename from the creation parameters if any
        MDICREATESTRUCT* pMDICreate = (MDICREATESTRUCT*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (pMDICreate && pMDICreate->lParam) {
            // Open the file
            const char* fileName = (const char*)pMDICreate->lParam;
            std::ifstream file(fileName, std::ios::binary);

            if (file) {
                // Read file into memory
                file.seekg(0, std::ios::end);
                size_t fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                pState->fileData.resize(fileSize);
                file.read(reinterpret_cast<char*>(pState->fileData.data()), fileSize);
                pState->fileName = fileName;

                // Setup scrollbars
                pState->totalRows = (fileSize + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
                SetScrollRange(hwnd, SB_VERT, 0, pState->totalRows, TRUE);

                // Update window title
                std::string title = "Hex View - " + pState->fileName;
                SetWindowText(hwnd, title.c_str());
                tagBytesThatAreAnnotated(*pState);
            }
        }

        pState->gdi.hFontHex = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Consolas");

        pState->gdi.hFontAnnotations = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, "Segoe UI");

        pState->gdi.selectionBrush = CreateSolidBrush(RGB(0, 120, 215));

        pState->gdi.grayPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));

        for (int i = 0; i < std::size(annotationColors); ++i) {
            pState->gdi.annotationPen[i] = CreatePen(PS_SOLID, 1, annotationColors[i]);
        }

        return 0;
    }

    case WM_MDIACTIVATE:
    {
        // Store the active hex viewer window when activated
        if ((HWND)lParam == hwnd) {
            g_hActiveHexViewer = hwnd;

            int gridViewOffset = std::min(pState->selectionStart, pState->selectionEnd); // pState->cursorPosition;

            // Update grid view if there's a selection
            if (pState && gridViewOffset >= 0 && !pState->fileData.empty()) {
                UpdateGridView(g_hGridView, gridViewOffset, pState->fileData);
            }
        }
        return 0;
    }

    case WM_SIZE:
    {
        if (!pState) return 0;

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        int height = clientRect.bottom - clientRect.top;
        pState->bytesPerPage = (height / ROW_HEIGHT) * BYTES_PER_ROW;

        if (!pState->fileData.empty()) {
            pState->totalRows = (pState->fileData.size() + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
            int maxScroll = std::max(0, pState->totalRows - (height / ROW_HEIGHT));
            SetScrollRange(hwnd, SB_VERT, 0, maxScroll, TRUE);
        }

        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Don't erase to prevent flicker

    case WM_VSCROLL:
    {
        if (!pState) return 0;

        int action = LOWORD(wParam);
        int position = HIWORD(wParam);
        int currentPos = pState->scrollPosition;

        switch (action) {
        case SB_LINEDOWN:
            currentPos += 1;
            break;
        case SB_LINEUP:
            currentPos -= 1;
            break;
        case SB_PAGEDOWN:
            currentPos += (pState->bytesPerPage / BYTES_PER_ROW);
            break;
        case SB_PAGEUP:
            currentPos -= (pState->bytesPerPage / BYTES_PER_ROW);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            currentPos = position;
            break;
        }

        // Get scroll limits and clamp the position
        int minScroll, maxScroll;
        GetScrollRange(hwnd, SB_VERT, &minScroll, &maxScroll);
        currentPos = std::max(minScroll, std::min(currentPos, maxScroll));

        if (currentPos != pState->scrollPosition) {
            pState->scrollPosition = currentPos;
            SetScrollPos(hwnd, SB_VERT, currentPos, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }

        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        if (!pState) return 0;

        int delta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        int newPos = pState->scrollPosition - delta;

        // Get scroll limits and clamp the position
        int minScroll, maxScroll;
        GetScrollRange(hwnd, SB_VERT, &minScroll, &maxScroll);
        newPos = std::max(minScroll, std::min(newPos, maxScroll));

        if (newPos != pState->scrollPosition) {
            pState->scrollPosition = newPos;
            SetScrollPos(hwnd, SB_VERT, newPos, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }

        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (!pState || pState->fileData.empty()) return 0;

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Skip if clicking on header
        if (y <= ROW_HEIGHT) return 0;

        // Calculate the byte offset from coordinates
        int row = (y - ROW_HEIGHT) / ROW_HEIGHT + pState->scrollPosition;
        int offset = -1;

        if (x >= HEX_MARGIN && x < ASCII_MARGIN) {
            // Clicked in hex section
            int col = -1;
            // Find the column by checking each byte position
            for (int i = 0; i < BYTES_PER_ROW; ++i) {
                int byteStartX = HEX_MARGIN + i * HEX_BYTE_SPACING -5; //lef
                int byteEndX = byteStartX + CHAR_WIDTH * 2;
                if (x >= byteStartX && x < byteEndX) {
                    col = i;
                    break;
                }
            }

            if (col >= 0) {
                offset = row * BYTES_PER_ROW + col;
            }
        }
        else if (x >= ASCII_MARGIN) {
            // Clicked in ASCII section
            int charPos = (x - ASCII_MARGIN) / CHAR_WIDTH;
            if (charPos >= 0 && charPos < BYTES_PER_ROW) {
                offset = row * BYTES_PER_ROW + charPos;
            }
        }

        if (offset >= 0 && offset < pState->fileData.size()) {
            pState->cursorPosition = offset;
            pState->selectionStart = offset;
            pState->selectionEnd = offset;
            pState->isSelecting = true;

            // Update the grid view with the new selection
            UpdateGridView(g_hGridView, offset, pState->fileData);
            UpdateStatusbar(offset, 1);
            InvalidateRect(hwnd, NULL, TRUE);
        }

        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!pState || !pState->isSelecting || pState->fileData.empty()) return 0;

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Skip if over header
        if (y <= ROW_HEIGHT) return 0;

        // Calculate the byte offset from coordinates
        int row = (y - ROW_HEIGHT) / ROW_HEIGHT + pState->scrollPosition;
        int offset = -1;

        if (x >= HEX_MARGIN && x < ASCII_MARGIN) {
            // In hex section
            int col = -1;
            // Find the column by checking each byte position
            for (int i = 0; i < BYTES_PER_ROW; ++i) {
                int byteStartX = HEX_MARGIN + i * HEX_BYTE_SPACING - 5; //lef
                int byteEndX = byteStartX + CHAR_WIDTH * 2;
                if (x >= byteStartX && x < byteEndX) {
                    col = i;
                    break;
                }
            }

            if (col >= 0) {
                offset = row * BYTES_PER_ROW + col;
            }
        }
        else if (x >= ASCII_MARGIN) {
            // In ASCII section
            int charPos = (x - ASCII_MARGIN) / CHAR_WIDTH;
            if (charPos >= 0 && charPos < BYTES_PER_ROW) {
                offset = row * BYTES_PER_ROW + charPos;
            }
        }

        if (offset >= 0 && offset < pState->fileData.size()) {
            pState->cursorPosition = offset;
            pState->selectionEnd = offset;

            int gridViewOffset = std::min(pState->selectionStart, pState->selectionEnd); // pState->cursorPosition;
            // Update grid view with new selection
            UpdateGridView(g_hGridView, gridViewOffset, pState->fileData);
            UpdateStatusbar(gridViewOffset, abs(pState->selectionEnd - pState->selectionStart)+1);
            InvalidateRect(hwnd, NULL, TRUE);
        }

        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!pState) return 0;
        pState->isSelecting = false;
        return 0;
    }

    case WM_PAINT:
    {
        if (!pState) return 0;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Double buffering to reduce flicker
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Fill background
        HBRUSH whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
        FillRect(memDC, &clientRect, whiteBrush);

        // Draw the hex view and annotations
        DrawHexView(hwnd, memDC, *pState);
        DrawAnnotations(hwnd, memDC, *pState);

        // Copy to screen
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

        // Clean up
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        if (!pState || pState->fileData.empty()) return 0;

        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Skip if clicking on header
        if (y <= ROW_HEIGHT) return 0;

        // Set cursor position for context menu
        int row = (y - ROW_HEIGHT) / ROW_HEIGHT + pState->scrollPosition;
        int offset = -1;

        if (x >= HEX_MARGIN && x < ASCII_MARGIN) {
            // Clicked in hex section
            for (int i = 0; i < BYTES_PER_ROW; ++i) {
                int byteStartX = HEX_MARGIN + i * HEX_BYTE_SPACING;
                int byteEndX = byteStartX + CHAR_WIDTH * 2;
                if (x >= byteStartX && x < byteEndX) {
                    offset = row * BYTES_PER_ROW + i;
                    break;
                }
            }
        }
        else if (x >= ASCII_MARGIN) {
            // Clicked in ASCII section
            int charPos = (x - ASCII_MARGIN) / CHAR_WIDTH;
            if (charPos >= 0 && charPos < BYTES_PER_ROW) {
                offset = row * BYTES_PER_ROW + charPos;
            }
        }

        if (offset >= 0 && offset < pState->fileData.size()) {
            pState->cursorPosition = offset;

            // Update grid view
            int gridViewOffset = std::min(pState->selectionStart, pState->selectionEnd); // pState->cursorPosition;
            UpdateGridView(g_hGridView, gridViewOffset, pState->fileData);
        }

        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (!pState || pState->fileData.empty()) return 0;

        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ClientToScreen(hwnd, &pt);

        ShowContextMenu(hwnd, pt.x, pt.y, *pState);
        return 0;
    }

    case WM_CONTEXTMENU:
    {
        if (!pState || pState->fileData.empty()) return 0;

        POINT pt;
        GetCursorPos(&pt);

        ShowContextMenu(hwnd, pt.x, pt.y, *pState);
        return 0;
    }

    case WM_COMMAND:
    {
        if (!pState) return 0;

        int wmId = LOWORD(wParam);

        switch (wmId) {
        case 2001: // Edit Annotation
        {
            int annotationIndex = -1;
            for (size_t i = 0; i < pState->annotations.size(); i++) {
                const auto& anno = pState->annotations[i];
                if (pState->cursorPosition >= anno.startOffset && pState->cursorPosition <= anno.endOffset) {
                    annotationIndex = static_cast<int>(i);
                    break;
                }
            }

            if (annotationIndex >= 0) {
                EditAnnotation(hwnd, annotationIndex, *pState);
                tagBytesThatAreAnnotated(*pState);
            }
            break;
        }

        case 2002: // Remove Annotation
        {
            int annotationIndex = -1;
            for (size_t i = 0; i < pState->annotations.size(); i++) {
                const auto& anno = pState->annotations[i];
                if (pState->cursorPosition >= anno.startOffset && pState->cursorPosition <= anno.endOffset) {
                    annotationIndex = static_cast<int>(i);
                    break;
                }
            }

            if (annotationIndex >= 0) {
                pState->annotations.erase(pState->annotations.begin() + annotationIndex);
                tagBytesThatAreAnnotated(*pState);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case 2003: // Set format to Hex
        case 2004: // Set format to Int
        case 2005: // Set format to Float
        case 2006: // Set format to Double
        case 2007: // Set format to Ascii
        case 2008: // Set format to Unicode
        {
            int annotationIndex = -1;
            for (size_t i = 0; i < pState->annotations.size(); i++) {
                const auto& anno = pState->annotations[i];
                if (pState->cursorPosition >= anno.startOffset && pState->cursorPosition <= anno.endOffset) {
                    annotationIndex = static_cast<int>(i);
                    break;
                }
            }

            if (annotationIndex >= 0) {
                // Set the format based on the menu item
                switch (wmId) {
                case 2003: pState->annotations[annotationIndex].displayFormat = "hex"; break;
                case 2004: pState->annotations[annotationIndex].displayFormat = "int"; break;
                case 2005: pState->annotations[annotationIndex].displayFormat = "float"; break;
                case 2006: pState->annotations[annotationIndex].displayFormat = "double"; break;
                case 2007: pState->annotations[annotationIndex].displayFormat = "ascii"; break;
                case 2008: pState->annotations[annotationIndex].displayFormat = "unicode"; break;
                }
                tagBytesThatAreAnnotated(*pState);

                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case 2009: // Create Annotation
            if (pState->selectionStart >= 0 && pState->selectionEnd >= 0) {
                CreateAnnotation(hwnd, *pState);
                tagBytesThatAreAnnotated(*pState);
            }
            break;
        }

        return 0;
    }

    case WM_DESTROY:
    {
        // Clean up this window's state
        if (pState) {
            DeleteObject(pState->gdi.hFontHex);
            DeleteObject(pState->gdi.hFontAnnotations);
            DeleteObject(pState->gdi.selectionBrush);
            DeleteObject(pState->gdi.grayPen);
            for (int i = 0; i < std::size(annotationColors); ++i) {
                DeleteObject(pState->gdi.annotationPen[i]);
            }

            delete pState;
            windowStates.erase(hwnd);
        }
        return 0;
    }
    }

    return DefMDIChildProc(hwnd, msg, wParam, lParam);
}

void tagBytesThatAreAnnotated(DocumentWindowState& state) {
    std::vector<ByteRange> byteTags;
    byteTags.reserve(state.annotations.size());

    // Pre-populate annotation information for ASCII section formatting
    for (size_t annoIdx = 0; annoIdx < state.annotations.size(); annoIdx++) {
        const auto& anno = state.annotations[annoIdx];
        std::string formattedValue = FormatData(state.fileData, anno.startOffset, anno.endOffset - anno.startOffset + 1, anno.displayFormat);
        
        byteTags.push_back(ByteRange{
            .start = anno.startOffset,
            .end = anno.endOffset,
            .info = {
                formattedValue,
                anno.colorIndex,
                anno.endOffset - anno.startOffset + 1,
                anno.startOffset,
                static_cast<int>(annoIdx)
            }
        });
    }

    std::sort(byteTags.begin(), byteTags.end());

    state.annotationMap.ranges = std::move(byteTags);
}



void DrawHexView(HWND hwnd, HDC hdc, DocumentWindowState& state) {
    if (state.fileData.empty()) {
        TextOut(hdc, 10, 10, "Open a file to start viewing.", 28);
        return;
    }

    // Set up fonts

    HFONT hOldFont = (HFONT)SelectObject(hdc, state.gdi.hFontHex);

    // Setup colors
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    // Draw the header
    char headerText[50];
    sprintf_s(headerText, sizeof(headerText), "Offset");
    TextOut(hdc, 10, 0, headerText, strlen(headerText));

    for (int i = 0; i < BYTES_PER_ROW; i++) {
        sprintf_s(headerText, sizeof(headerText), "%02X", i);
        TextOut(hdc, HEX_MARGIN + i * HEX_BYTE_SPACING, 0, headerText, strlen(headerText));
    }

    TextOut(hdc, ASCII_MARGIN, 0, "ASCII", 5);

    // Draw a separator line between header and data
    HPEN hOldPen = (HPEN)SelectObject(hdc, state.gdi.grayPen);
    MoveToEx(hdc, 0, ROW_HEIGHT - 2, NULL);
    LineTo(hdc, ASCII_MARGIN + BYTES_PER_ROW * CHAR_WIDTH + 50, ROW_HEIGHT - 2);
    SelectObject(hdc, hOldPen);

    // Calculate the visible range
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int visibleRows = (clientRect.bottom - ROW_HEIGHT) / ROW_HEIGHT;
    int startRow = state.scrollPosition;
    int endRow = std::min(startRow + visibleRows, state.totalRows);

    // Draw each row
    for (int row = startRow; row < endRow; row++) {
        int yPos = (row - startRow) * ROW_HEIGHT + ROW_HEIGHT;
        int offsetBase = row * BYTES_PER_ROW;

        // Draw offset
        sprintf_s(headerText, sizeof(headerText), "%08X", offsetBase);
        TextOut(hdc, 10, yPos, headerText, strlen(headerText));

        // Draw hex values and prepare ASCII 
        std::string asciiLine;
        std::vector<bool> drawnInAscii(BYTES_PER_ROW, false);

        for (int col = 0; col < BYTES_PER_ROW; col++) {
            int offset = offsetBase + col;

            if (offset < state.fileData.size()) {
                BYTE byte = state.fileData[offset];

                // Determine if this byte is selected
                bool isSelected = false;
                if (state.selectionStart >= 0 && state.selectionEnd >= 0) {
                    int selStart = std::min(state.selectionStart, state.selectionEnd);
                    int selEnd = std::max(state.selectionStart, state.selectionEnd);
                    isSelected = (offset >= selStart && offset <= selEnd);
                }

                // Check if this byte is in any annotation
                bool isAnnotated = state.annotationMap.at(offset) != nullptr;

                // Prepare rectangle for selection background
                RECT hexRect = {
                    HEX_MARGIN + col * HEX_BYTE_SPACING,
                    yPos - 0,
                    HEX_MARGIN + col * HEX_BYTE_SPACING + CHAR_WIDTH * 2 - 5,
                    yPos + 16
                };

                RECT asciiRect = {
                    ASCII_MARGIN + col * CHAR_WIDTH,
                    yPos - 0,
                    ASCII_MARGIN + (col + 1) * CHAR_WIDTH,
                    yPos + 16
                };

                // Set colors based on selection/annotation
                if (isSelected) {
                    // Draw selection background for hex
                    SetBkColor(hdc, RGB(0, 120, 215));
                    SetBkMode(hdc, OPAQUE);
                    FillRect(hdc, &hexRect, state.gdi.selectionBrush);
                    SetTextColor(hdc, RGB(255, 255, 255));
                }
                else if (isAnnotated) {
                    SetTextColor(hdc, RGB(0, 0, 150));
                    SetBkMode(hdc, TRANSPARENT);
                }
                else {
                    SetTextColor(hdc, RGB(0, 0, 0));
                    SetBkMode(hdc, TRANSPARENT);
                }

                // Draw hex value
                char hexByte[3];
                sprintf_s(hexByte, sizeof(hexByte), "%02X", byte);
                int xPos = HEX_MARGIN + col * HEX_BYTE_SPACING;
                TextOut(hdc, xPos, yPos, hexByte, 2);

                // Append to ASCII representation (will be used for non-annotated bytes later)
                if (byte >= 32 && byte <= 126) {
                    asciiLine += static_cast<char>(byte);
                }
                else {
                    asciiLine += '.';
                }

                // Draw selection for ASCII too
                if (isSelected) {
                    // Store this character to draw later
                    char asciiChar = (byte >= 32 && byte <= 126) ? static_cast<char>(byte) : '.';
                    FillRect(hdc, &asciiRect, state.gdi.selectionBrush);
                    SetTextColor(hdc, RGB(255, 255, 255));
                    char aChar[2] = { asciiChar, 0 };
                    TextOut(hdc, ASCII_MARGIN + col * CHAR_WIDTH, yPos, aChar, 1);
                    drawnInAscii[col] = true;
                }
            }
            else {
                asciiLine += ' ';
            }
        }

        // Draw ASCII representation or annotation values
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        // First pass - draw annotation values in ASCII section
        for (int col = 0; col < BYTES_PER_ROW && offsetBase + col < state.fileData.size(); col++) {
            int offset = offsetBase + col;

            auto annotation = state.annotationMap.at(offset);

            if (annotation && !drawnInAscii[col]) {
                int annoIdx = annotation->annotationIndex;
                int startRow = annotation->startOffset / BYTES_PER_ROW;
                int currentRow = offset / BYTES_PER_ROW;
                int posInRow = annotation->positionInRow(offset);

                // Get the annotation this byte belongs to
                if (annoIdx >= 0 && annoIdx < state.annotations.size()) {
                    const auto& anno = state.annotations[annoIdx];
                    std::string value = annotation->formattedValue;

                    // Handle multi-row annotations
                    if (startRow != currentRow) {
                        // For continuation rows, calculate how much of the value was already shown
                        int charsInPreviousRows = 0;

                        // If this is a continuation row, show the remaining part of the value
                        if (posInRow == 0) {  // Only at the start of each row
                            // Calculate how many characters were shown in previous rows
                            int bytesInPrevRows = offset - anno.startOffset - posInRow;
                            int charsPerByte = value.length() / (anno.endOffset - anno.startOffset + 1);
                            charsInPreviousRows = std::min(static_cast<int>(value.length()), bytesInPrevRows * charsPerByte);

                            if (charsInPreviousRows < value.length()) {
                                // Get remaining part of the value
                                std::string remainingValue = value.substr(charsInPreviousRows);

                                // Ensure it fits in this row
                                int maxLength = std::min(static_cast<int>(remainingValue.length()), BYTES_PER_ROW - col);
                                if (maxLength > 0) {
                                    // Draw continuation of the value
                                    SetTextColor(hdc, annotationColors[annotation->colorIndex]);
                                    TextOut(hdc, ASCII_MARGIN + col * CHAR_WIDTH, yPos, remainingValue.c_str(), maxLength);

                                    // Mark these positions as drawn
                                    for (int i = 0; i < maxLength && col + i < BYTES_PER_ROW; i++) {
                                        drawnInAscii[col + i] = true;
                                    }
                                }
                            }
                        }
                    }
                    // For the first row of the annotation or single-row annotations
                    else if (offset == anno.startOffset) {
                        // Display the formatted value in the ASCII section
                        SetTextColor(hdc, annotationColors[annotation->colorIndex]);

                        // Ensure the value fits in the row
                        int maxLength = std::min(static_cast<int>(value.length()), BYTES_PER_ROW - col);

                        // Draw the formatted value
                        TextOut(hdc, ASCII_MARGIN + col * CHAR_WIDTH, yPos, value.c_str(), maxLength);

                        // Mark these positions as drawn
                        for (int i = 0; i < maxLength && col + i < BYTES_PER_ROW; i++) {
                            drawnInAscii[col + i] = true;
                        }

                        // For multi-row annotations that start on this row
                        int bytesInAnnotation = anno.endOffset - anno.startOffset + 1;
                        int bytesInThisRow = std::min(bytesInAnnotation, BYTES_PER_ROW - col);

                        // Mark all bytes covered by this annotation in this row as drawn
                        for (int i = 0; i < bytesInThisRow; i++) {
                            drawnInAscii[col + i] = true;
                        }
                    }
                }
            }
        }

        // Second pass - draw regular ASCII characters for non-annotated, non-selected bytes
        for (int col = 0; col < BYTES_PER_ROW && col < asciiLine.length(); col++) {
            if (!drawnInAscii[col]) {
                SetTextColor(hdc, RGB(0, 0, 0));  // Reset to default text color
                char aChar[2] = { asciiLine[col], 0 };
                TextOut(hdc, ASCII_MARGIN + col * CHAR_WIDTH, yPos, aChar, 1);
            }
        }
    }

    // Clean up
    SelectObject(hdc, hOldFont);
}

void drawLeftRoundedRect(HDC hdc, int startX, int startY, int endX, int endY) {
    // Top horizontal line
    MoveToEx(hdc, startX + 5, startY, NULL);
    LineTo(hdc, endX, startY);

    // Top-left corner arc
    Arc(hdc, startX, startY, startX + 10, startY + 10,
        startX + 5, startY, startX, startY + 5);

    // Left vertical line
    MoveToEx(hdc, startX, startY + 5, NULL);
    LineTo(hdc, startX, endY - 5);

    // Bottom-left corner arc
    Arc(hdc, startX, endY - 10, startX + 10, endY,
        startX, endY - 5, startX + 5, endY);

    // Bottom horizontal line
    MoveToEx(hdc, startX + 5, endY, NULL);
    LineTo(hdc, endX, endY);
}

void drawRightRoundedRect(HDC hdc, int startX, int startY, int endX, int endY) {
    // Top horizontal line
    MoveToEx(hdc, startX, startY, NULL);
    LineTo(hdc, endX - 5, startY);

    // Top-right corner arc
    Arc(hdc, endX - 10, startY, endX, startY + 10,
        endX, startY + 5, endX - 5, startY);

    // Right vertical line
    MoveToEx(hdc, endX, startY + 5, NULL);
    LineTo(hdc, endX, endY - 5);

    // Bottom-right corner arc
    Arc(hdc, endX - 10, endY - 10, endX, endY,
        endX - 5, endY, endX, endY - 5);

    // Bottom horizontal line
    MoveToEx(hdc, startX, endY, NULL);
    LineTo(hdc, endX - 5, endY);
}

void DrawAnnotations(HWND hwnd, HDC hdc, DocumentWindowState& state) {
    if (state.fileData.empty() || state.annotations.empty()) {
        return;
    }


    HFONT hOldFont = (HFONT)SelectObject(hdc, state.gdi.hFontAnnotations);

    for (const auto& anno : state.annotations) {
        int startRow = anno.startOffset / BYTES_PER_ROW;
        int startCol = anno.startOffset % BYTES_PER_ROW;
        int endRow = anno.endOffset / BYTES_PER_ROW;
        int endCol = anno.endOffset % BYTES_PER_ROW;

        // Skip annotations outside the visible area
        if (endRow < state.scrollPosition ||
            startRow > state.scrollPosition + (state.bytesPerPage / BYTES_PER_ROW)) {
            continue;
        }

        // Create pen for drawing
        HPEN hOldPen = (HPEN)SelectObject(hdc, state.gdi.annotationPen[anno.colorIndex]);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

        // Handle multi-row annotations by drawing separate rectangles for each row
        for (int row = startRow; row <= endRow; row++) {
            // Skip rows outside visible area
            if (row < state.scrollPosition ||
                row > state.scrollPosition + (state.bytesPerPage / BYTES_PER_ROW)) {
                continue;
            }

            int rowY = (row - state.scrollPosition) * ROW_HEIGHT + ROW_HEIGHT;
            int rowStartCol = (row == startRow) ? startCol : 0;
            int rowEndCol = (row == endRow) ? endCol : BYTES_PER_ROW - 1;

            // Calculate coordinates for this row
            int hexStartX = HEX_MARGIN + rowStartCol * HEX_BYTE_SPACING - ANNOTATION_MARGIN;
            int hexEndX = HEX_MARGIN + rowEndCol * HEX_BYTE_SPACING + CHAR_WIDTH * 2 - 9 + ANNOTATION_MARGIN;
            int startY = rowY - 0;  // Aligned with text baseline
            int endY = rowY + 16;   // Extend below text

            // ASCII section coordinates
            int asciiStartX = ASCII_MARGIN + rowStartCol * CHAR_WIDTH - ANNOTATION_MARGIN;
            int asciiEndX = ASCII_MARGIN + (rowEndCol + 1) * CHAR_WIDTH + ANNOTATION_MARGIN-5;

            // Determine which part of the annotation this row represents
            bool isFirstRow = (row == startRow);
            bool isMiddleRow = (row > startRow && row < endRow);
            bool isLastRow = (row == endRow);

            // For multi-row annotations, draw the outline according to the specified pattern
            if (startRow != endRow) {
                // First row - draw top, left with arcs, and bottom
                if (isFirstRow) {
                    drawLeftRoundedRect(hdc, hexStartX,   startY, hexEndX,   endY);
                    drawLeftRoundedRect(hdc, asciiStartX, startY, asciiEndX, endY);
                }
                // Middle rows - just horizontal lines on top and bottom
                else if (isMiddleRow) {
                    // HEX SECTION - Middle row outline
                    // Top horizontal line
                    MoveToEx(hdc, hexStartX, startY, NULL);
                    LineTo(hdc, hexEndX, startY);

                    // Bottom horizontal line
                    MoveToEx(hdc, hexStartX, endY, NULL);
                    LineTo(hdc, hexEndX, endY);

                    // ASCII SECTION - Middle row outline
                    // Top horizontal line
                    MoveToEx(hdc, asciiStartX, startY, NULL);
                    LineTo(hdc, asciiEndX, startY);

                    // Bottom horizontal line
                    MoveToEx(hdc, asciiStartX, endY, NULL);
                    LineTo(hdc, asciiEndX, endY);
                }
                // Last row - draw top, right with arcs, and bottom
                else if (isLastRow) {
                    // HEX SECTION - Last row outline
                    drawRightRoundedRect(hdc, hexStartX,   startY,    hexEndX, endY);
                    drawRightRoundedRect(hdc, asciiStartX, startY, asciiEndX, endY);
                }
            }
            else {
                // Single row annotation - use rounded rectangle for both hex and ASCII
                RoundRect(hdc, hexStartX, startY, hexEndX, endY, 10, 10);
                RoundRect(hdc, asciiStartX, startY, asciiEndX, endY, 10, 10);
            }

            // Draw the label - only on the first visible row
            if (row == startRow || (startRow < state.scrollPosition && row == state.scrollPosition)) {
                SetTextColor(hdc, annotationColors[anno.colorIndex]);
                SetBkMode(hdc, TRANSPARENT);
                TextOut(hdc, hexStartX, startY - 11, anno.label.c_str(), anno.label.length());
            }

            // Draw the formatted value - only on the last visible row and only for the HEX section
            // (we already display the value in the ASCII section)
            if ((row == endRow || (endRow > state.scrollPosition + (state.bytesPerPage / BYTES_PER_ROW) &&
                row == state.scrollPosition + (state.bytesPerPage / BYTES_PER_ROW))) &&
                (row == endRow)) {  // Only show at the end row

                std::string formattedValue = FormatData(state.fileData, anno.startOffset,
                    anno.endOffset - anno.startOffset + 1,
                    anno.displayFormat);

                std::string displayText = anno.displayFormat + ": " + formattedValue;
               // TextOut(hdc, hexStartX, endY - 1, displayText.c_str(), displayText.length());
            }
        }

        // Cleanup
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
    }

    SelectObject(hdc, hOldFont);
}
//-------------------------------------------------------------------
// ShowContextMenu - Display context menu for annotations
//-------------------------------------------------------------------
void ShowContextMenu(HWND hwnd, int x, int y, DocumentWindowState& state) {
    // Check if the cursor is over an annotation
    int annotationIndex = -1;

    if (state.cursorPosition >= 0) {
        for (size_t i = 0; i < state.annotations.size(); i++) {
            const auto& anno = state.annotations[i];
            if (state.cursorPosition >= anno.startOffset && state.cursorPosition <= anno.endOffset) {
                annotationIndex = static_cast<int>(i);
                break;
            }
        }
    }

    HMENU hPopupMenu = CreatePopupMenu();

    if (annotationIndex >= 0) {
        // Mouse is over an annotation - show annotation-specific menu
        AppendMenu(hPopupMenu, MF_STRING, 2001, "Edit Annotation");
        AppendMenu(hPopupMenu, MF_STRING, 2002, "Remove Annotation");
        AppendMenu(hPopupMenu, MF_SEPARATOR, 0, NULL);

        // Add format options with checkmark on the current format
        const std::string& currentFormat = state.annotations[annotationIndex].displayFormat;

        UINT hexFlags = MF_STRING | (currentFormat == "hex" ? MF_CHECKED : MF_UNCHECKED);
        UINT intFlags = MF_STRING | (currentFormat == "int" ? MF_CHECKED : MF_UNCHECKED);
        UINT floatFlags = MF_STRING | (currentFormat == "float" ? MF_CHECKED : MF_UNCHECKED);
        UINT doubleFlags = MF_STRING | (currentFormat == "double" ? MF_CHECKED : MF_UNCHECKED);
        UINT asciiFlags = MF_STRING | (currentFormat == "ascii" ? MF_CHECKED : MF_UNCHECKED);
        UINT unicodeFlags = MF_STRING | (currentFormat == "unicode" ? MF_CHECKED : MF_UNCHECKED);

        AppendMenu(hPopupMenu, hexFlags, 2003, "Hex");
        AppendMenu(hPopupMenu, intFlags, 2004, "Int");
        AppendMenu(hPopupMenu, floatFlags, 2005, "Float");
        AppendMenu(hPopupMenu, doubleFlags, 2006, "Double");
        AppendMenu(hPopupMenu, asciiFlags, 2007, "Ascii");
        AppendMenu(hPopupMenu, unicodeFlags, 2008, "Unicode");
    }
    else if (state.selectionStart >= 0 && state.selectionEnd >= 0) {
        // There's an active selection - show create option
        AppendMenu(hPopupMenu, MF_STRING, 2009, "Create Annotation");
    }
    else {
        // No annotation or selection - no context menu needed
        DestroyMenu(hPopupMenu);
        return;
    }

    // Show the popup menu
    TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);

    // Clean up
    DestroyMenu(hPopupMenu);
}

//-------------------------------------------------------------------
// FormatData - Format data for annotations
//-------------------------------------------------------------------
std::string FormatData(const std::vector<BYTE>& data, int offset, int length, const std::string& format) {
    std::stringstream ss;

    // Make sure we don't go out of bounds
    if (offset + length > data.size()) {
        length = data.size() - offset;
    }

    if (format == "hex") {
        for (int i = 0; i < length; ++i) {
            ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[offset + i]) << " ";
        }
    }
    else if (format == "int") {
        // For integers, interpret based on length
        if (length == 1) {
            // 8-bit integer
            int8_t value = static_cast<int8_t>(data[offset]);
            ss << static_cast<int>(value); // Cast to int to display as number
        }
        else if (length == 2) {
            // 16-bit integer
            int16_t value = 0;
            memcpy(&value, &data[offset], sizeof(int16_t));
            ss << value;
        }
        else if (length == 4) {
            // 32-bit integer
            int32_t value = 0;
            memcpy(&value, &data[offset], sizeof(int32_t));
            ss << value;
        }
        else if (length == 8) {
            // 64-bit integer
            int64_t value = 0;
            memcpy(&value, &data[offset], sizeof(int64_t));
            ss << value;
        }
        else {
            // Handle unusual length
            int value = 0;
            for (int i = 0; i < length; ++i) {
                value |= static_cast<int>(data[offset + i]) << (i * 8);
            }
            ss << value;
        }
    }
    else if (format == "float") {
        if (length >= 4) {
            float value;
            memcpy(&value, &data[offset], sizeof(float));
            ss << std::fixed << std::setprecision(6) << value;
        }
        else {
            ss << "Insufficient bytes";
        }
    }
    else if (format == "double") {
        if (length >= 8) {
            double value;
            memcpy(&value, &data[offset], sizeof(double));
            ss << std::fixed << std::setprecision(6) << value;
        }
        else {
            ss << "Insufficient bytes";
        }
    }
    else if (format == "ascii") {
        for (int i = 0; i < length; ++i) {
            BYTE byte = data[offset + i];
            if (byte >= 32 && byte <= 126) {
                ss << static_cast<char>(byte);
            }
            else {
                ss << '.';
            }
        }
    }
    else if (format == "unicode") {
        for (int i = 0; i < length; i += 2) {
            BYTE byte = data[offset + i];
            if (byte >= 32 && byte <= 126) {
                ss << static_cast<char>(byte);
            }
            else {
                ss << '.';
            }
        }
    }
    return ss.str();
}

//-------------------------------------------------------------------
// CreateAnnotation - Create a new annotation
//-------------------------------------------------------------------
void CreateAnnotation(HWND hwnd, DocumentWindowState& state) {
    if (state.selectionStart < 0 || state.selectionEnd < 0 || state.fileData.empty()) {
        return;
    }

    int selStart = std::min(state.selectionStart, state.selectionEnd);
    int selEnd = std::max(state.selectionStart, state.selectionEnd);

    // Simple input dialog implementation
    char labelBuffer[256] = {};
    char formatBuffer[32] = "hex"; // Default format

    ShowAnnotationInputDialog(hwnd, labelBuffer, sizeof(labelBuffer), formatBuffer, sizeof(formatBuffer));

    if (strlen(labelBuffer) > 0) {
        Annotation newAnnotation;
        newAnnotation.startOffset = selStart;
        newAnnotation.endOffset = selEnd;
        newAnnotation.label = labelBuffer;
        newAnnotation.displayFormat = formatBuffer; // Use the selected format

        newAnnotation.colorIndex = state.annotations.size() % std::size(annotationColors);

        state.annotations.push_back(newAnnotation);
        state.isAnnotating = false;

        InvalidateRect(hwnd, NULL, TRUE);
    }
}

//-------------------------------------------------------------------
// EditAnnotation - Edit an existing annotation
//-------------------------------------------------------------------
void EditAnnotation(HWND hwnd, int index, DocumentWindowState& state) {
    if (index < 0 || index >= state.annotations.size()) {
        return;
    }

    Annotation& anno = state.annotations[index];

    char labelBuffer[256] = {};
    char formatBuffer[32] = {};

    // Copy existing values to buffers
    strcpy_s(labelBuffer, sizeof(labelBuffer), anno.label.c_str());
    strcpy_s(formatBuffer, sizeof(formatBuffer), anno.displayFormat.c_str());

    ShowAnnotationInputDialog(hwnd, labelBuffer, sizeof(labelBuffer), formatBuffer, sizeof(formatBuffer));

    if (strlen(labelBuffer) > 0) {
        // Update annotation with new values
        anno.label = labelBuffer;
        anno.displayFormat = formatBuffer;

        InvalidateRect(hwnd, NULL, TRUE);
    }
}




