#include "storage/FSFile.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>

#include "utils/zerobuf.h"

namespace taco {

FSFile*
FSFile::Open(const std::string& path, bool o_trunc,
             bool o_direct, bool o_creat, mode_t mode) {
    // Hint: wse open(2) to obtain a file descriptor of the file for read/write.
    // The file should be opened with O_RDWR flag per specification.
    // Run "man 2 open" in the shell for details.

    int flags = O_RDWR;

    if (o_trunc) 
        flags = flags | O_TRUNC;
    
    if (o_direct)
        flags = flags | O_DIRECT;

    if (o_creat)
        flags = flags | O_CREAT;

    int fd = open(path.c_str(), flags, mode);
    if (fd < 0)
        return nullptr;

    FSFile* f = new FSFile(path, fd, o_direct);
    f->set_size(f->Size());
    return f;
}

FSFile::~FSFile() {
   if (is_file_open())
      FSFile::Close();
}

bool
FSFile::Reopen() {
    
    int flags = O_RDWR;

    if (get_o_direct())
        flags = flags | O_DIRECT;	 

    int fd = open(get_file_path().c_str(), O_RDWR, 0600);
    
    if (fd < 0)
        return false;
    set_fd(fd);
    set_file_open(true);
    set_size(-1);
    return true;

}

void
FSFile::Close() {
    
    if (!is_file_open())
        return; 

    int fd = FSFile::get_fd();
    int ret_val = close(fd);
    if (ret_val != 0) {
        LOG(kWarning, "Close failed with error %s", strerror(errno));
        return;
    }
    set_file_open(false);
    set_size(-1);
}

bool
FSFile::IsOpen() const {
    return is_file_open(); 
}

void
FSFile::Delete() const {
    
	if (is_file_open())
		Close();

    int ret_val = unlink(get_file_path().c_str());
    
    if (ret_val != 0) {
        LOG(kWarning, "Delete failed with error %s", strerror(errno));
        return;
    }
}

void
FSFile::Read(void *buf, size_t count, off_t offset) {

    if (count + offset > Size()) {
        LOG(kFatal, "Invalid read");
        return;
    }
    
    int ret_val = pread(get_fd(), buf, count, offset);
    if (ret_val < 0) {
        LOG(kFatal, "Read failed with error %s", strerror(errno));
        return;
    }

}

void
FSFile::Write(const void *buf, size_t count, off_t offset) {

    if (Size() == 0) {
        LOG(kFatal, "File is empty");
        return;
    }

    if (offset >= Size()) {
        LOG(kFatal, "Offset is outside the file size");
        return;
    }
   
    if (offset % PAGE_SIZE != 0) {
        LOG(kFatal, "Offset is not page size aligned");
        return;
    }
   
    LOG(kInfo, "Count : %d", count);
    LOG(kInfo, "FD : %d", get_fd());
    LOG(kInfo, "offset : %d", offset);
    int ret_val = pwrite(get_fd(), buf, count, offset);
    LOG(kInfo, "ret : %d", ret_val);
    if (ret_val < 0) {
        LOG(kFatal, "Write failed with error %s", strerror(errno));
        return;
    }
}

void
FSFile::Allocate(size_t count) {
    // Hint: call fallocate_zerofill_fast() first to see if we can use
    // the faster fallocate(2) to extend the file.
    //
    // If it returns false and errno == EOPNOTSUPP (not supported by the file
    // system), fall back to writing `count' of zeros at the end of the file.
    // You may use g_zerobuf defined in utils/zerobuf.h as a large buffer that
    // is always all 0.
    //
    // If fallocate_zerofill_fast() returns false and errno is not either 0 or
    // EOPNOTSUPP, log a fatal error with strerror(errno) as a substring.

    //TODO implement it
    bool ret = fallocate_zerofill_fast(get_fd(), Size(), count);
    if (!ret && errno == EOPNOTSUPP) {
    
    int size = Size(); 

    int ret_val = pwrite(get_fd(), g_zerobuf, count, size);

    LOG(kInfo, "ret : %d", ret_val);
    if (ret_val < 0) {
        LOG(kFatal, "Write failed with error %s", strerror(errno));
        return;
    }

    set_size(-1);
    size = Size();
    set_size(size);
    } 
}

size_t
FSFile::Size() const noexcept {
    // Hint: you may obtain the file size using stat(2)
    // FSFile::Size() is frequently called to determine the file size, so
    // you might want to cache the result of stat(2) in the file (but then
    // an FSFile::Allocate() call may extend it). You may assume no one may
    // extend or shrink the file externally when the database is running.

    if (get_size() != -1) {
        LOG(kInfo, "Return size from cache");
        return get_size();    
    }

    struct stat *buf = (struct stat*) malloc(sizeof(struct stat));
    if (buf == nullptr) {
        LOG(kError, "Malloc failed");
        return ~(size_t) 0;
    }

    // int ret_val = stat(get_file_path().c_str(), buf);
    int ret_val = fstat(get_fd(), buf);
    if (ret_val != 0) {
        LOG(kFatal, "stat failed with error %s", strerror(errno));
        free(buf);
        return ~(size_t) 0;
    }

    size_t size = buf->st_size;
    LOG(kInfo, "size : %d", size);
    free(buf);
    return size;
}

void
FSFile::Flush() {
    // Hint: use fsync(2) or fdatasync(2).
    //TODO implement it
}

}   // namespace taco
