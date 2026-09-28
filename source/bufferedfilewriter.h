#ifndef _BUFFERED_FILE_WRITER_H_
#define _BUFFERED_FILE_WRITER_H_

#include <stdio.h>

class BufferedFileWriter {
    FILE* RawFilePointer;
    char* Buffer;
    int Position;
    static const int BufferSize = 1024 * 512; // 512 KB is big enough to hold all savestates I've seen.

public:
    BufferedFileWriter() : RawFilePointer(NULL), Position(0) {
        Buffer = new char[BufferSize];
    }

    ~BufferedFileWriter() {
        close();
        delete[] Buffer;
    }

    bool open(const char* filename, const char* mode) {
        RawFilePointer = fopen(filename, mode);
        return RawFilePointer != NULL;
    }

    bool open(int fd, const char* mode) {
        RawFilePointer = fdopen(fd, mode);
        return RawFilePointer != NULL;
    }

    size_t write(const void* ptr, int count) {
        if (count <= 0)
            return 0;

        const char* source = static_cast<const char*>(ptr);
        int remaining = count;

        while (remaining > 0) {
            if (Position == BufferSize)
                flush();

            int chunk = BufferSize - Position;
            if (chunk > remaining)
                chunk = remaining;

            memcpy(&Buffer[Position], source, chunk);
            Position += chunk;
            source += chunk;
            remaining -= chunk;
        }

        return static_cast<size_t>(count);
    }

    void flush() {
        if (RawFilePointer && Position > 0) {
            fwrite(Buffer, 1, Position, RawFilePointer);
            Position = 0;
        }
    }

    int close() {
        if (RawFilePointer) {
            flush();
            int rv = fclose(RawFilePointer);
            RawFilePointer = NULL;
            return rv;
        }
        return -1;
    }
};

#endif // _BUFFERED_FILE_WRITER_H_
