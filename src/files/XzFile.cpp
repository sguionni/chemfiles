// Chemfiles, a modern library for chemistry file reading and writing
// Copyright (C) Guillaume Fraux and contributors -- BSD license

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <limits>

#include <lzma.h>

#include "chemfiles/File.hpp"
#include "chemfiles/files/XzFile.hpp"

#include "chemfiles/error_fmt.hpp"
#include "chemfiles/string_view.hpp"

using namespace chemfiles;

static void check(lzma_ret code) {
    switch (code) {
    case LZMA_OK:
    case LZMA_STREAM_END:
        return;
    case LZMA_MEM_ERROR:
    case LZMA_MEMLIMIT_ERROR:
        throw file_error("lzma: memory allocation failed (code: {})", code);
    case LZMA_FORMAT_ERROR:
        throw file_error("lzma: input not in .xz format (code: {})", code);
    case LZMA_OPTIONS_ERROR:
        throw file_error(
            "lzma: unsupported compression options (code: {})", code);
    case LZMA_DATA_ERROR:
        throw file_error("lzma: compressed file is corrupted (code: {})", code);
    case LZMA_BUF_ERROR:
        throw file_error(
            "lzma: compressed file is truncated or corrupted (code: {})", code);
    case LZMA_UNSUPPORTED_CHECK:
        throw file_error("lzma: specified integrity check is not supported (code: {})", code);
    case LZMA_PROG_ERROR:
        throw file_error("lzma: programming error (this is a bug) (code: {})", code);
    default:
        throw file_error("lzma: unknown error (code: {})", code);
    }
}

static void open_stream_read(lzma_stream* stream) {
    auto flags = LZMA_TELL_UNSUPPORTED_CHECK | LZMA_CONCATENATED;
    auto memory_limit = std::numeric_limits<uint64_t>::max();
    check(lzma_stream_decoder(stream, memory_limit, flags));
}

XzFile::XzFile(const std::string& path, File::Mode mode): TextFileImpl(path), mode_(mode), buffer_(8192) {
    const char* openmode = nullptr;
    if (mode == File::READ) {
        openmode = "rb";
        open_stream_read(&stream_);
    } else if (mode == File::WRITE) {
        openmode = "wb";
        check(lzma_easy_encoder(&stream_, 6, LZMA_CHECK_CRC64));

        stream_.next_out = buffer_.data();
        stream_.avail_out = buffer_.size();
    } else if (mode == File::APPEND) {
        throw file_error("appending (open mode 'a') is not supported with xz files");
    }

    file_ = std::fopen(path.c_str(), openmode);
    if (file_ == nullptr) {
        lzma_end(&stream_);
        throw file_error("could not open the file at '{}'", path);
    }
}

XzFile::~XzFile() {
    if (mode_ == File::WRITE) {
        compress_and_write(LZMA_FINISH);
    }

    lzma_end(&stream_);
    if (file_) {
        fclose(file_);
    }
}

size_t XzFile::read(char* data, size_t count) {
    auto action = LZMA_RUN;

    stream_.next_out = reinterpret_cast<uint8_t*>(data);
    stream_.avail_out = count;

    while (stream_.avail_out != 0) {
        // read more compressed data from the file
        if (stream_.avail_in == 0 && !std::feof(file_)) {
            stream_.next_in = buffer_.data();
            stream_.avail_in = std::fread(buffer_.data(), 1, buffer_.size(), file_);

            if (std::ferror(file_)) {
                throw file_error("IO error while reading xz file");
            }
        }

        if (std::feof(file_)) {
            action = LZMA_FINISH;
        }

        auto status = lzma_code(&stream_, action);

        if (status == LZMA_STREAM_END) {
            return count - stream_.avail_out;
        } else {
            // Check for error
            check(status);
        }
    }
    return count;
}

void XzFile::clear() noexcept {
    std::clearerr(file_);
}

void XzFile::seek(int64_t position) {
    assert(mode_ == File::READ);
    // Reset stream state
    lzma_end(&stream_);
    stream_ = LZMA_STREAM_INIT;
    open_stream_read(&stream_);

    // Dumb implementation, re-decompressing the file from the begining
    std::fseek(file_, 0, SEEK_SET);
    constexpr size_t BUFFSIZE = 4096;
    char buffer[BUFFSIZE];

    while (static_cast<size_t>(position) > BUFFSIZE) {
        auto count = read(buffer, BUFFSIZE);
        assert(count == BUFFSIZE);
        position -= count;
    }

    auto count = read(buffer, static_cast<size_t>(position));
    assert(count == static_cast<size_t>(position));
}

void XzFile::write(const char* data, size_t count) {
    stream_.next_in = reinterpret_cast<const uint8_t*>(data);
    stream_.avail_in = count;
    compress_and_write(LZMA_RUN);

    auto actual = count - stream_.avail_in;
    if (actual != count) {
        throw file_error("could not write data to the file at '{}'", this->path());
    }
}

void XzFile::compress_and_write(lzma_action action) {
    lzma_ret status = LZMA_OK;
    do {
        status = lzma_code(&stream_, action);

        if (stream_.avail_out == 0 || status == LZMA_STREAM_END) {
            auto size = buffer_.size() - stream_.avail_out;
            auto written = std::fwrite(buffer_.data(), sizeof(uint8_t), size, file_);
            if (written != size) {
                throw file_error("error while writting data to xz file");
            }

            stream_.next_out = buffer_.data();
            stream_.avail_out = buffer_.size();
        }

        check(status);

    } while (stream_.avail_in != 0 || (action == LZMA_FINISH && status != LZMA_STREAM_END));
}
