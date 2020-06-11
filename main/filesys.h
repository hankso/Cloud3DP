/* 
 * File: filesys.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-15 19:43:15
 */

/* Framework EspAsyncWebServer depends on Arduino FS libraries, so we have to
 * implement our file system based on Arduino FS instead of native ESP32 VFS.
 *
 * - SDSPIFSFS is implemented for SD Card FAT File System in SPI mode.
 *
 * - FLASHFSFS supports SPIFFS & FFATFS and is configurable by defining macro
 * `CONFIG_USE_FFATFS`. It default initializes SPIFFS on flash partition.
 *
 * This module relies on Arduino FS Abstract Layer but nothing.
 *
 * fs::File implements an `operator bool()` operator, thus File instances
 * can be validated in a boolean context like:
 *
 *      File file = MyFS.open("/filename");
 *      if (!!file || file==true || (bool)file || file ? true : false) {
 *          printf("FileImplPtr is valid\n");
 *          file.close();
 *      }
 *
 * fs::File's boolean operator is actually the validation of `FileImplPtr _p`,
 * which is returned and registered by fs::FSImpl::open. If file doesn't exist
 * or `mode` doesn't contain write flag, `open` should return a null pointer.
 * So we can use a different `exists` logic and employ it in `open`.
 *
 * Note that fs::FileImpl's `operator bool()` has no relation with FileImplPtr,
 * it is used to determine whether the file exists:
 *
 *      MyFSFileImpl file_impl(MyFS, "/filename", "r");
 *      if (file_impl) {
 *          printf("file/dir does exist\n");
 *          file_impl.close();
 *      }
 */

#ifndef _FILESYS_H_
#define _FILESYS_H_

#include <FS.h>
#include <FSImpl.h>

#include "sdmmc_cmd.h"
#include "wear_levelling.h"
#include "driver/sdmmc_types.h"
#include "sys/unistd.h"
#include "sys/stat.h"
#include "dirent.h"

#define SDFS_MP "/sdcard"
#define FFS_MP "/flashfs"

namespace fs {

class CFSFileImpl;

class CFSImpl : public FSImpl {
protected:
    friend class CFSFileImpl;
public:
    FileImplPtr open(const char *path, const char *mode) override;
    bool        exists(const char *path) override;
    bool        rename(const char *from, const char *to) override;
    bool        remove(const char *path) override;
    bool        mkdir(const char *path) override;
    bool        rmdir(const char *path) override;
};

class CFSFileImpl : public FileImpl {
protected:
    CFSImpl *           _fs;
    FILE *              _file;
    DIR *               _dir;
    char *              _path;
    char *              _fullpath;
    bool                _isdir;
    bool                _badfile;
    bool                _baddir;
    mutable struct stat _stat;
    mutable bool        _written;
    bool                _getstat() const;
public:
    CFSFileImpl(CFSImpl *fs, const char *path, const char *mode);
    ~CFSFileImpl() override { close(); }
    size_t      write(const uint8_t *buf, size_t size) override;
    size_t      read(uint8_t *buf, size_t size) override;
    bool        seek(uint32_t pos, SeekMode mode) override;
    size_t      tell() const;
    void        flush() override;
    void        close() override;

    operator    bool()                  { return !_badfile || !_baddir; }
    const char* name() const override   { return (const char *)_path; }
    size_t      size() const override   { _getstat(); return _stat.st_size; }
    time_t      getLastWrite() override { _getstat(); return _stat.st_mtime; }
    size_t      position() const override { return tell(); }
    boolean     isDirectory(void) override { return _isdir; }

    FileImplPtr openNextFile(const char *mode) override;
    void        rewindDirectory(void) override;
};

class CFS : public FS {
protected:
    size_t _total, _used;
public:
    CFS() : FS(FSImplPtr(new CFSImpl())), _total(0), _used(0) {}

    // work through directory
    virtual void walk(const char *, void (*cb)(File, void *), void *) = 0;
    // print information of file entries
    void list(const char *path, FILE *stream);
    // conver list result to JSON
    char * list(const char *path);

    size_t totalBytes() { return _total; }
    size_t usedBytes() { return _used; }

    void printInfo() {
        fprintf(stdout, "File System use: %d/%d KB\n", _used/1024, _total/1024);
    }
};

class FLASHFSFS : public CFS {
private:
    void _getsize();
    const char *_label;
    wl_handle_t _wlhdl = WL_INVALID_HANDLE;
public:
    // can specify partition label name
    FLASHFSFS(const char *label=NULL) : _label(label) {}

    bool begin(bool format=false, const char *base=FFS_MP, uint8_t max=10);
    void end();

    void walk(const char *path, void (*cb)(File, void *), void *arg) override;
};

class SDSPIFSFS : public CFS {
private:
    void _getsize();
    sdmmc_card_t *_card = NULL;
public:
    SDSPIFSFS() {}

    bool begin(bool format=false, const char *base=SDFS_MP, uint8_t max=10);
    void end();

    void walk(const char *path, void (*cb)(File, void *), void *arg) override;

    void printInfo() {
        if (_card) sdmmc_card_print_info(stdout, _card);
        else fprintf(stdout, "SD Card not detected\n");
    }
};

}

extern fs::FLASHFSFS FFS;
extern fs::SDSPIFSFS SDFS;

#endif // _FILESYS_H_
