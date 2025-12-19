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
    std::string formattedValue;
    int colorIndex;
    int length;
    int startOffset;
    int annotationIndex;    // To identify which annotation this byte belongs to

    int positionInRow(int i) const{
        const int BYTES_PER_ROW = 16;
        int row = i / BYTES_PER_ROW;
        int startRow = startOffset / BYTES_PER_ROW;
        if (row == startRow) {
            // First row - position starts from the column of first byte
            return i - startOffset;
        }
        else {
            // Subsequent rows - position starts from 0 relative to this row
            return i % BYTES_PER_ROW;
        }
    }
};

struct ByteRange {
    int start;
    int end;
    AnnotationInfo info;

    bool operator<(const ByteRange& other) const {
        return start < other.start ||
            (start == other.start && end < other.end);
    }
};

struct ByteMap {
    const AnnotationInfo* at(size_t byteOffset) const
    {
        auto it = std::upper_bound(
            ranges.begin(),
            ranges.end(),
            byteOffset,
            [](size_t value, const ByteRange& r) {
                return value < r.start;
            });

        if (it == ranges.begin())
            return nullptr;

        --it;

        if (byteOffset <= it->end)
            return &it->info;

        return nullptr;
    }

    std::vector<ByteRange> ranges;
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

    ByteMap annotationMap;
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
