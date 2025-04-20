#pragma once
#include <string>
#include <vector>

// Structure to store annotation information
struct Annotation {
    int startOffset;
    int endOffset;
    std::string label;
    std::string displayFormat; // "hex", "int", "float", "ascii", etc.
    COLORREF color;
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
