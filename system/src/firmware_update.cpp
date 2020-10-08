/*
 * Copyright (c) 2020 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "logging.h"

LOG_SOURCE_CATEGORY("system.ota");

#include "firmware_update.h"

#include "system_task.h"
#include "error_message.h"

#include "ota_flash_hal.h"
#include "timer_hal.h"

#include "simple_file_storage.h"
#include "scope_guard.h"
#include "check.h"
#include "debug.h"

#if HAL_PLATFORM_RESUMABLE_OTA
#include "sha256.h"
#endif // HAL_PLATFORM_RESUMABLE_OTA

#include "spark_wiring_system.h"

#include <cstdio>
#include <cstdarg>

namespace particle::system {

namespace {

#if HAL_PLATFORM_RESUMABLE_OTA

// Name of the file storing the transfer state
const auto TRANSFER_STATE_FILE = "/sys/fw_transfer";

// Interval at which the transfer state file is synced
const system_tick_t TRANSFER_STATE_SYNC_INTERVAL = 1000;

// The data stored in the OTA section is read in blocks of this size
const size_t OTA_FLASH_READ_BLOCK_SIZE = 128;

// The same buffer is used as a temporary storage for a SHA-256 hash
static_assert(OTA_FLASH_READ_BLOCK_SIZE >= Sha256::HASH_SIZE, "OTA_FLASH_READ_BLOCK_SIZE is too small");

struct PersistentTransferState {
    char fileHash[Sha256::HASH_SIZE]; // SHA-256 of the update binary
    char partialHash[Sha256::HASH_SIZE]; // SHA-256 of the partially transferred data
    uint32_t fileSize; // Size of the update binary
    uint32_t partialSize; // Size of the partially transferred data
} /* __attribute__((packed)) */;

#endif // HAL_PLATFORM_RESUMABLE_OTA

} // namespace

namespace detail {

#if HAL_PLATFORM_RESUMABLE_OTA

struct TransferState {
    SimpleFileStorage file; // File storing the transfer state
    Sha256 partialHash; // SHA-256 of the partially transferred data
    Sha256 tempHash; // Intermediate SHA-256 checksum
    PersistentTransferState persist; // Persistently stored transfer state
    system_tick_t lastSynced; // Time when the file was last synced
    bool needSync; // Whether the file needs to be synced

    TransferState() :
            file(TRANSFER_STATE_FILE),
            persist(),
            lastSynced(0),
            needSync(false) {
    }
};

#endif // HAL_PLATFORM_RESUMABLE_OTA

} // namespace detail

FirmwareUpdate::FirmwareUpdate() :
        validResult_(0),
        validChecked_(false),
        updating_(false) {
}

int FirmwareUpdate::startUpdate(size_t fileSize, const char* fileHash, size_t* fileOffset, FirmwareUpdateFlags flags) {
    const bool discardData = flags & FirmwareUpdateFlag::DISCARD_DATA;
    const bool nonResumable = flags & FirmwareUpdateFlag::NON_RESUMABLE;
    const bool validateOnly = flags & FirmwareUpdateFlag::VALIDATE_ONLY;
    if (!nonResumable && (!fileHash || !fileOffset)) {
        return SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    if (updating_) {
        setErrorMessage("Firmware update is already in progress");
        return SYSTEM_ERROR_INVALID_STATE;
    }
    if (!System.updatesEnabled() && !System.updatesForced()) {
        return SYSTEM_ERROR_OTA_UPDATES_DISABLED;
    }
    if (fileSize == 0 && fileSize > HAL_OTA_FlashLength()) {
        return SYSTEM_ERROR_OTA_INVALID_SIZE;
    }
    size_t partialSize = 0;
#if HAL_PLATFORM_RESUMABLE_OTA
    if ((discardData || nonResumable) && !validateOnly) {
        clearTransferState();
    }
    // Do not load the transfer state if both VALIDATE_ONLY and DISCARD_DATA are set. DISCARD_DATA
    // would have cleared it anyway if it wasn't a dry-run
    if (!nonResumable && !(validateOnly && discardData)) {
        const int r = initTransferState(fileSize, fileHash);
        if (r == 0) {
            partialSize = transferState_->persist.partialSize;
            if (validateOnly) {
                transferState_.reset();
            }
        } else {
            // Not a critical error
            LOG(ERROR, "Failed to initialize persistent transfer state: %d", r);
            if (!validateOnly) {
                clearTransferState();
            }
        }
    }
#endif // HAL_PLATFORM_RESUMABLE_OTA
    if (!validateOnly) {
        // Erase the OTA section if we're not resuming the previous transfer
        if (!partialSize && !HAL_FLASH_Begin(HAL_OTA_FlashAddress(), fileSize, nullptr)) {
            endUpdate();
            return SYSTEM_ERROR_FLASH;
        }
        SPARK_FLASH_UPDATE = 1; // TODO: Get rid of legacy state variables
        updating_ = true;
        // TODO: System events
    }
    if (fileOffset) {
        *fileOffset = partialSize;
    }
    return 0;
}

int FirmwareUpdate::finishUpdate(FirmwareUpdateFlags flags) {
    const bool discardData = flags & FirmwareUpdateFlag::DISCARD_DATA;
    const bool validateOnly = flags & FirmwareUpdateFlag::VALIDATE_ONLY;
    const bool cancel = flags & FirmwareUpdateFlag::CANCEL;
    if (!cancel) {
        if (!updating_) {
            return SYSTEM_ERROR_INVALID_STATE;
        }
#if HAL_PLATFORM_RESUMABLE_OTA
        clearTransferState();
#endif
    } else if (!updating_) {
#if HAL_PLATFORM_RESUMABLE_OTA
        if (discardData && !validateOnly) {
            clearTransferState();
        }
#endif
    }
    return 0;
}

int FirmwareUpdate::saveChunk(const char* chunkData, size_t chunkSize, size_t chunkOffset, size_t partialSize) {
    if (!updating_) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    int r = HAL_FLASH_Update((const uint8_t*)chunkData, chunkOffset, chunkSize, nullptr);
    if (r != 0) {
        formatErrorMessage("Failed to save chunk to OTA section: %d", r);
        endUpdate();
        return SYSTEM_ERROR_FLASH;
    }
#if HAL_PLATFORM_RESUMABLE_OTA
    if (transferState_) {
        r = updateTransferState(chunkData, chunkSize, chunkOffset, partialSize);
        if (r != 0) {
            // Not a critical error
            LOG(ERROR, "Failed to update persistent transfer state: %d", r);
            clearTransferState();
        }
    }
#endif
    return 0;
}

bool FirmwareUpdate::isInProgress() const {
    return updating_;
}

FirmwareUpdate* FirmwareUpdate::instance() {
    static FirmwareUpdate instance;
    return &instance;
}

#if HAL_PLATFORM_RESUMABLE_OTA

int FirmwareUpdate::initTransferState(size_t fileSize, const char* fileHash) {
    std::unique_ptr<detail::TransferState> state(new(std::nothrow) detail::TransferState());
    if (!state) {
        return SYSTEM_ERROR_NO_MEMORY;
    }
    CHECK(state->partialHash.init());
    CHECK(state->tempHash.init());
    bool resumeTransfer = false;
    const auto persist = &state->persist;
    const int r = state->file.load(persist, sizeof(PersistentTransferState));
    if (r == sizeof(PersistentTransferState)) {
        if (persist->fileSize == fileSize && persist->partialSize <= fileSize &&
                memcmp(persist->fileHash, fileHash, Sha256::HASH_SIZE) == 0) {
            // Compute the hash of the partially transferred data in the OTA section
            CHECK(state->partialHash.start());
            char buf[OTA_FLASH_READ_BLOCK_SIZE] = {};
            uintptr_t addr = HAL_OTA_FlashAddress();
            const uintptr_t endAddr = addr + persist->partialSize;
            while (addr < endAddr) {
                const size_t n = std::min(endAddr - addr, sizeof(buf));
                CHECK(HAL_OTA_Flash_Read(addr, (uint8_t*)buf, n));
                CHECK(state->partialHash.update(buf, n));
                addr += n;
            }
            CHECK(state->tempHash.copyFrom(state->partialHash));
            CHECK(state->tempHash.finish(buf));
            if (memcmp(persist->partialHash, buf, Sha256::HASH_SIZE) == 0) {
                resumeTransfer = true;
            }
        }
    } else if (r < 0 && r != SYSTEM_ERROR_NOT_FOUND) {
        return r;
    }
    if (resumeTransfer) {
        state->file.close(); // Will be reopened for writing
    } else {
        state->file.clear();
        memcpy(persist->fileHash, fileHash, Sha256::HASH_SIZE);
        memset(persist->partialHash, 0, Sha256::HASH_SIZE);
        persist->fileSize = fileSize;
        persist->partialSize = 0;
        CHECK(state->partialHash.start()); // Reset SHA-256 context
    }
    transferState_ = std::move(state);
    return 0;
}

int FirmwareUpdate::updateTransferState(const char* chunkData, size_t chunkSize, size_t chunkOffset, size_t partialSize) {
    const auto state = transferState_.get();
    if (!state) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    bool updateState = false;
    const auto persist = &state->persist;
    // Check if the chunk is adjacent to or overlaps with the contiguous fragment of the data for
    // which we have already calculated the checksum
    if (persist->partialSize >= chunkOffset && persist->partialSize < chunkOffset + chunkSize) {
        const auto n = chunkOffset + chunkSize - persist->partialSize;
        CHECK(state->partialHash.update(chunkData + persist->partialSize - chunkOffset, n));
        persist->partialSize += n;
        updateState = true;
    }
    // Chunks are not necessarily transferred sequentially. We may need to read them back from the
    // OTA section to calculate the checksum of the data transferred so far
    if (partialSize > persist->partialSize) {
        char buf[OTA_FLASH_READ_BLOCK_SIZE] = {};
        uintptr_t addr = HAL_OTA_FlashAddress() + persist->partialSize;
        const uintptr_t endAddr = addr + partialSize - persist->partialSize;
        while (addr < endAddr) {
            const size_t n = std::min(endAddr - addr, sizeof(buf));
            CHECK(HAL_OTA_Flash_Read(addr, (uint8_t*)buf, n));
            CHECK(state->partialHash.update(buf, n));
            addr += n;
        }
        persist->partialSize = partialSize;
        updateState = true;
    }
    if (updateState) {
        CHECK(state->tempHash.copyFrom(state->partialHash));
        CHECK(state->tempHash.finish(persist->partialHash));
        CHECK(state->file.save(persist, sizeof(PersistentTransferState)));
        state->needSync = true;
    }
    if (state->needSync && HAL_Timer_Get_Milli_Seconds() - state->lastSynced >= TRANSFER_STATE_SYNC_INTERVAL) {
        CHECK(state->file.sync());
        state->lastSynced = HAL_Timer_Get_Milli_Seconds();
    }
    return 0;
}

int FirmwareUpdate::finalizeTransferState() {
    const auto state = transferState_.get();
    if (!state) {
        return SYSTEM_ERROR_INVALID_STATE;
    }
    const auto persist = &state->persist;
    if (persist->partialSize != persist->fileSize) {
        return SYSTEM_ERROR_OTA_INVALID_SIZE;
    }
    if (memcmp(persist->partialHash, persist->fileHash, Sha256::HASH_SIZE) != 0) {
        return SYSTEM_ERROR_OTA_INTEGRITY_CHECK_FAILED;
    }
    CHECK(state->file.sync());
    state->file.close();
    transferState_.reset();
    return 0;
}

void FirmwareUpdate::clearTransferState() {
    if (transferState_) {
        transferState_->file.clear();
        transferState_.reset();
    } else {
        SimpleFileStorage::clear(TRANSFER_STATE_FILE);
    }
}

#endif // HAL_PLATFORM_RESUMABLE_OTA

void FirmwareUpdate::endUpdate() {
#if HAL_PLATFORM_RESUMABLE_OTA
    transferState_.reset();
#endif
    SPARK_FLASH_UPDATE = 0;
    updating_ = false;
}

} // namespace particle::system
