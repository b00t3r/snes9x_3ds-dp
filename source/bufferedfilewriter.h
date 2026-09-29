#ifndef _BUFFERED_FILE_WRITER_H_
#define _BUFFERED_FILE_WRITER_H_

#include <stdio.h>

class BufferedFileWriter {
    FILE* RawFilePointer;
    bool WriteFailed;

public:
    BufferedFileWriter() : RawFilePointer(NULL), WriteFailed(false) {}

    ~BufferedFileWriter() {
        close();
    }

    bool open(const char* filename, const char* mode) {
        RawFilePointer = fopen(filename, mode);
        WriteFailed = false;
        return RawFilePointer != NULL;
    }

    bool open(int fd, const char* mode) {
        RawFilePointer = fdopen(fd, mode);
        WriteFailed = false;
        return RawFilePointer != NULL;
    }

    size_t write(const void* ptr, int count) {
        if (count <= 0 || !RawFilePointer)
            return 0;

        size_t written = fwrite(ptr, 1, static_cast<size_t>(count), RawFilePointer);
        if (written != static_cast<size_t>(count))
            WriteFailed = true;
        return written;
    }

    void flush() {
        if (RawFilePointer && fflush(RawFilePointer) != 0)
            WriteFailed = true;
    }

    bool failed() const {
        return WriteFailed;
    }

    int close() {
        if (RawFilePointer) {
            flush();
            int rv = fclose(RawFilePointer);
            RawFilePointer = NULL;
            if (rv != 0)
                WriteFailed = true;
            return rv;
        }
        return -1;
    }
};

#endif // _BUFFERED_FILE_WRITER_H_
