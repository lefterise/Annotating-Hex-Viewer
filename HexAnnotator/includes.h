#pragma once
#include <string>
#include <vector>

static const COLORREF annotationColors[] = {
    RGB(255, 0, 0),    // Red
    RGB(0, 128, 0),    // Green
    RGB(0, 0, 255),    // Blue
    RGB(128, 0, 128),  // Purple
    RGB(255, 165, 0),  // Orange
    RGB(0, 128, 128)   // Teal
};

// Structure to store annotation information
struct Annotation {
    int startOffset;
    int endOffset;
    std::string label;
    std::string displayFormat; // "hex", "int", "float", "ascii", etc.
    int colorIndex;
};

struct AnnotationInfo {
    bool isAnnotated;
    std::string formattedValue;
    int colorIndex;
    int annotationLength;
    int annotationStartOffset;
    int annotationIndex;    // To identify which annotation this byte belongs to
    int positionInRow;      // Position within this row (for multi-row annotations)
};

// Structure to represent the application state
struct DocumentWindowState {
    std::vector<BYTE> fileData;
    std::string fileName;
    int scrollPosition = 0;
    int bytesPerPage = 0;
    int totalRows = 0;
    int cursorPosition = -1;
    int selectionStart = -1;
    int selectionEnd = -1;
    bool isSelecting = false;
    std::vector<Annotation> annotations;
    bool isAnnotating = false;
    std::string tempAnnotationLabel;
    std::string currentDisplayFormat = "hex";

    std::vector<AnnotationInfo> annotationMap;
    struct {
        HFONT hFontHex;
        HFONT hFontAnnotations;
        HBRUSH selectionBrush;
        HPEN grayPen;
        HPEN annotationPen[6];
    } gdi;

};

// Grid data types
enum DataType {
    DT_BYTE,
    DT_UBYTE,
    DT_SHORT,
    DT_USHORT,
    DT_INT,
    DT_UINT,
    DT_FLOAT,
    DT_DOUBLE,
    DT_STRING,
    DT_UNICODE_STRING,
    DT_TIME_T,
    DT_TIME64_T,
    DT_OLETIME,
    DT_COUNT  // Total number of data types
};
