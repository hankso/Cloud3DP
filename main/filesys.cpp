/* 
 * File: SDSPIFS.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-15 19:42:04
 */

#include "filesys.h"
#include "drivers.h"
#include "globals.h"

#include "cJSON.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"

using namespace fs;

bool _valid_path(const char *path) { return path && path[0] == '/'; }
bool _write_mode(const char *mode) { return mode && strchr(mode, 'w'); }

// File System APIs

FileImplPtr CFSImpl::open(const char *path, const char *mode) {
    if (!_mountpoint || !_valid_path(path) ||
        (!_write_mode(mode) && !exists(path))) return FileImplPtr();
    return std::make_shared<CFSFileImpl>(this, path, mode);
}

bool CFSImpl::exists(const char *path) {
    if (!_mountpoint || !_valid_path(path)) return false;
    CFSFileImpl file(this, path, "r");
    if (file) {
        file.close();
        return true;
    } else {
        return false;
    }
}

bool CFSImpl::rename(const char *from, const char *to) {
    if (!exists(from) || !_valid_path(to)) return false;
    // char *tmp1 = (char *)malloc(strlen(_mountpoint) + strlen(from) + 1);
    // if (!tmp1) return false;
    // char *tmp2 = (char *)malloc(strlen(_mountpoint) + strlen(to) + 1);
    // if (!tmp2) { free(tmp1); return false; }
    // strcpy(tmp1, _mountpoint); strcat(tmp1, from);
    // strcpy(tmp2, _mountpoint); strcat(tmp2, to);
    String mp = _mountpoint, f = mp + from, t = mp + to;
    return ::rename(f.c_str(), t.c_str()) == 0;
}

bool CFSImpl::remove(const char *path) {
    if (!exists(path)) return false;
    String mp = _mountpoint, fullpath = mp + path;
    return unlink(fullpath.c_str()) == 0;
}

bool CFSImpl::mkdir(const char *path) {
    if (!_mountpoint || !_valid_path(path)) return false;
    CFSFileImpl file(this, path, "r");
    if (file) return file.isDirectory();
    String mp = _mountpoint, fullpath = mp + path;
    return ::mkdir(fullpath.c_str(), ACCESSPERMS) == 0;
}

bool CFSImpl::rmdir(const char *path) { return remove(path); }

void _jsonify_file(File file, void *arg) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", file.name());
    cJSON_AddNumberToObject(obj, "size", file.size());
    cJSON_AddNumberToObject(obj, "date", file.getLastWrite());
    cJSON_AddStringToObject(obj, "type", file.isDirectory() ? "folder":"file");
    cJSON_AddItemToArray((cJSON *)arg, obj);
}

void _loginfo_file(File file, void *arg) {
    fprintf((FILE *)arg, "%c %6s %12lu %s\n",
            file.isDirectory() ? 'd' : 'f',
            format_size(file.size()),
            file.getLastWrite(),
            file.name());
}

void CFS::list(const char *path, FILE *stream) {
    walk(path, &_loginfo_file, stream);
}

char * CFS::list(const char *path) {
    cJSON *lst = cJSON_CreateArray();
    walk(path, &_jsonify_file, lst);
    char *json = cJSON_Print(lst);
    cJSON_Delete(lst);
    return json;
}

// File APIs

CFSFileImpl::CFSFileImpl(CFSImpl *fs, const char *path, const char *mode)
    : _fs(fs)
    , _file(NULL), _dir(NULL)
    , _path(NULL), _fullpath(NULL)
    , _isdir(true)
    , _badfile(true), _baddir(true)
    , _written(true)
{
    char *tmp = (char *)malloc(strlen(_fs->mountpoint()) + strlen(path) + 1);
    if (!tmp) return; else _fullpath = tmp;
    strcpy(_fullpath, _fs->mountpoint()); strcat(_fullpath, path);

    _path = strdup(path);
    if (!_path || !strlen(_path)) {
        free(_fullpath);
        _fullpath = NULL;
        return;
    }
    if (_getstat()) {
        if (S_ISREG(_stat.st_mode)) {
            _file = fopen(_fullpath, mode);
            _isdir = false;
        } else if (S_ISDIR(_stat.st_mode)) {
            _dir = opendir(_fullpath);
        } else {
            printf("Unknown %s type 0x%08X", _fullpath, _stat.st_mode & _IFMT);
        }
    } else {
        if (_write_mode(mode)) {
            _file = fopen(_fullpath, mode);
            _isdir = false;
        } else if (_path[strlen(_path) - 1] == '/') {
            _dir = opendir(_fullpath);
            _isdir = _dir != NULL;
        } else {
            // We're using different `FS.exists` logic so _dir can be NULL
            // do nothing
        }
    }
    _badfile = _isdir || !_file;
    _baddir = !_isdir || !_dir;
}

bool CFSFileImpl::_getstat() const {
    if (!_fullpath) return false;
    if (!_written) return true;
    return (!stat(_fullpath, &_stat)) ? !(_written = false) : false;
}

size_t CFSFileImpl::write(const uint8_t *buf, size_t size) {
    if (_badfile || !buf || !size) return 0;
    _written = true;
    return fwrite(buf, 1, size, _file);
}

size_t CFSFileImpl::read(uint8_t *buf, size_t size) {
    if (_badfile || !buf || !size) return 0;
    return fread(buf, 1, size, _file);
}

bool CFSFileImpl::seek(uint32_t pos, SeekMode mode) {
    return _badfile ? false : fseek(_file, pos, mode) == 0;
}

void CFSFileImpl::flush() {
    if (_badfile) return;
    fflush(_file);
    fsync(fileno(_file));
}

void CFSFileImpl::close() {
    if (_path)      { free(_path);      _path = NULL; }
    if (_fullpath)  { free(_fullpath);  _fullpath = NULL; }
    if (_dir)       { closedir(_dir);   _dir = NULL; }
    if (_file)      { fclose(_file);    _file = NULL; }
    _badfile = _baddir = true;
}

size_t CFSFileImpl::tell() const { return _badfile ? 0 : ftell(_file); }

FileImplPtr CFSFileImpl::openNextFile(const char *mode) {
    if (_baddir) return FileImplPtr();
    struct dirent *file = readdir(_dir);
    if (file == NULL) return FileImplPtr();
    if (file->d_type != DT_REG && file->d_type != DT_DIR)
        return openNextFile(mode);
    String fname = String(file->d_name), name = String(_path);
    if (!fname.startsWith("/") && !name.endsWith("/")) name += "/";
    return std::make_shared<CFSFileImpl>(_fs, (name + fname).c_str(), mode);
}

void CFSFileImpl::rewindDirectory(void) { if (!_baddir) rewinddir(_dir); }

// SDMMC - SPI interface (using native spi driver instead of Arduino SPI.h)

bool SDSPIFSFS::begin(bool format, const char *base, uint8_t max) {
    esp_vfs_fat_sdmmc_mount_config_t mount_conf = {
        .format_if_mount_failed = format,
        .max_files = max,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host_conf = SDSPI_HOST_DEFAULT();

    /* SDSPI_SLOT_CONFIG_DEFAULT() */
    sdspi_slot_config_t slot_conf = {
        .gpio_miso = PIN_HMISO,
        .gpio_mosi = PIN_HMOSI,
        .gpio_sck  = PIN_HSCLK,
        .gpio_cs   = PIN_HCS0,
        .gpio_cd   = PIN_HSDCD,
        .gpio_wp   = SDSPI_SLOT_NO_WP,
        .dma_channel = 1,
    };

    // This convenience function calls init function `sdspi_host_init`, which
    // will initialize SPI bus and won't handle ESP_ERR_INVALID_STATE error.
    // So keep in mind to manually handle this error if you re-init a SPI bus.
    esp_err_t err = esp_vfs_fat_sdmmc_mount(
        base, &host_conf, &slot_conf, &mount_conf, &_card);
    if (err) {
        printf("Failed to mount SD Card: %s\n", esp_err_to_name(err));
        return false;
    } else {
        _impl->mountpoint(base);
        printf("Mounted SD Card to %s\n", base);
        _getsize();
        return true;
    }
}

void SDSPIFSFS::end() {
    esp_err_t err = esp_vfs_fat_sdmmc_unmount();
    if (!err) {
        _card = NULL;
        _impl->mountpoint(NULL);
    }
}

void SDSPIFSFS::walk(const char *path, void (*cb)(File, void *), void *arg) {
    String base = path;
    if (!base.startsWith("/")) base = "/" + base;
    if (!base.endsWith("/")) base += "/";
    File root = open(base), file;
    while (file = root.openNextFile()) {
        (*cb)(file, arg);
        file.close();
    }
    root.close();
}

void SDSPIFSFS::_getsize() {
    _total = _card ? (size_t)_card->csd.capacity * _card->csd.sector_size : 0;
    _used = 0; // not implemented yet
}

// Flash FAT / SPI Flash File System

bool FLASHFSFS::begin(bool format, const char *base, uint8_t max) {
#ifdef CONFIG_USE_FFATFS
    if (_wlhdl != WL_INVALID_HANDLE) return true;
    esp_vfs_fat_mount_config_t conf = {
        .format_if_mount_failed = format,
        .max_files = max,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(base, _label, &conf, &_wlhdl);
#else
    if (esp_spiffs_mounted(_label)) return true;
    esp_vfs_spiffs_conf_t conf = {
        .base_path = base,
        .partition_label = _label,
        .max_files = max,
        .format_if_mount_failed = format
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
#endif
    if (err) {
        printf("Failed to mount FlashFS: %s\n", esp_err_to_name(err));
        return false;
    } else {
        _impl->mountpoint(base);
        printf("Mounted FlashFS to %s\n", base);
        _getsize();
        return true;
    }
}

void FLASHFSFS::_getsize() {
#ifdef CONFIG_USE_FFATFS
    FATFS *fsinfo;
    DWORD free_clust;
    BYTE pdrv = ff_diskio_get_pdrv_wl(_wlhdl);
    char drv[3] = { (char)(48 + pdrv), ':', '\0' };
    if (f_getfree(drv, &free_clust, &fsinfo) != FR_OK) return;
    size_t ssize = fsinfo->csize * CONFIG_WL_SECTOR_SIZE;
    _total = (fs->n_fatent - 2) * ssize;
    _used = (fs->n_fatent - 2 - free_clust) * ssize;
#else
    esp_spiffs_info(_label, &_total, &_used);
#endif
}

void FLASHFSFS::end() {
    esp_err_t err =
#ifdef CONFIG_USE_FFATFS
        esp_vfs_fat_spiflash_unmount(_impl->mountpoint(), _wlhdl);
#else
        esp_spiffs_mounted(_label) ? \
            esp_vfs_spiffs_unregister(_label) : ESP_ERR_INVALID_STATE;
#endif
    if (!err) {
        _impl->mountpoint(NULL);
        _wlhdl = WL_INVALID_HANDLE;
    }
}

void FLASHFSFS::walk(const char *path, void (*cb)(File, void *), void *arg) {
    String base = path;
    if (!base.startsWith("/")) base = "/" + base;
    if (!base.endsWith("/")) base = base + "/";
#ifdef CONFIG_USE_FFATFS
    File root = open(base), file;
    while (file = root.openNextFile()) {
#else
    File root = open("/"), file;
    static String name, lastDir = "";
    while (file = root.openNextFile()) {
        // SPIFFS uses flatten file structure, so skip files under other dirs
        name = file.name();
        if (!name.startsWith(base)) continue;
        // resolve directory path from filename
        int idx = name.indexOf('/', base.length());
        if (idx != -1) {
            name = name.substring(0, idx + 1);
            if (lastDir == name) continue;
            else file = open(lastDir = name);
        }
#endif
        (*cb)(file, arg);
        file.close();
    }
    root.close();
}

SDSPIFSFS SDFS;
FLASHFSFS FFS;
