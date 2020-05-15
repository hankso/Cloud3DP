/* 
 * File: SDSPIFS.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-15 19:43:15
 */

#ifndef _SDSPIFS_H_
#define _SDSPIFS_H_

#include <FS.h>
#include <vfs_api.h>

#include "sdmmc_types.h"

namespace fs {
    class SDSPIFSFS : public FS {
    public:
        SDSPIFSFS() : FS(FSImplPtr(new VFSImpl())) {}
        bool begin(bool format, const char *basePath, uint8_t maxFiles=10);
        void info();
        void end();
    private:
        sdmmc_card_t *card;
    };
}

extern fs::SDSPIFSFS SDSPIFS;

#endif // _SDSPIFS_H_
