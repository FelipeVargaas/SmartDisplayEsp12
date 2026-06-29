#include "animation_image.h"

#include <Esp.h>
#include <TFT_eSPI.h>
#include <flash_hal.h>
#include <string.h>

#include "app_state.h"

namespace
{
const uint32_t IMAGE_MAGIC = 0x31494D54UL; // TMI1, little-endian on wire.
const uint16_t IMAGE_VERSION = 1;
const uint16_t IMAGE_FORMAT_RGB565_LE = 1;
const uint32_t IMAGE_STORAGE_BYTES = 128UL * 1024UL;
const uint16_t RENDER_ROWS = 4;
const uint32_t ERASED_WORD = 0xFFFFFFFFUL;

struct ImageHeader
{
  uint32_t magic;
  uint16_t version;
  uint16_t format;
  uint16_t width;
  uint16_t height;
  uint32_t dataLength;
  uint32_t crc32;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
};

static_assert(sizeof(ImageHeader) == 32, "Animation image header must be 32 bytes.");

struct UploadState
{
  bool active;
  bool failed;
  bool headerReady;
  size_t headerBytes;
  ImageHeader header;
  uint32_t dataBytes;
  uint32_t crc;
  char error[40];
};

UploadState uploadState = {};
uint16_t renderBuffer[ANIMATION_IMAGE_WIDTH * RENDER_ROWS];

uint32_t storageStart()
{
  return FS_PHYS_ADDR;
}

bool storageIsAvailable()
{
  return FS_PHYS_SIZE >= IMAGE_STORAGE_BYTES && ESP.getFlashChipRealSize() >= storageStart() + IMAGE_STORAGE_BYTES;
}

void setUploadError(const char* error)
{
  uploadState.failed = true;
  strncpy(uploadState.error, error, sizeof(uploadState.error) - 1);
  uploadState.error[sizeof(uploadState.error) - 1] = '\0';
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t size)
{
  crc = ~crc;
  for (size_t i = 0; i < size; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

bool readHeader(ImageHeader& header)
{
  return storageIsAvailable() && ESP.flashRead(storageStart(), reinterpret_cast<uint8_t*>(&header), sizeof(header));
}

bool headerIsValid(const ImageHeader& header)
{
  return header.magic == IMAGE_MAGIC &&
         header.version == IMAGE_VERSION &&
         header.format == IMAGE_FORMAT_RGB565_LE &&
         header.width == ANIMATION_IMAGE_WIDTH &&
         header.height == ANIMATION_IMAGE_HEIGHT &&
         header.dataLength == ANIMATION_IMAGE_DATA_BYTES;
}

bool eraseStorage()
{
  const uint32_t firstSector = storageStart() / SPI_FLASH_SEC_SIZE;
  const uint32_t sectorCount = IMAGE_STORAGE_BYTES / SPI_FLASH_SEC_SIZE;
  for (uint32_t i = 0; i < sectorCount; i++)
  {
    if (!ESP.flashEraseSector(firstSector + i)) return false;
    yield();
  }
  return true;
}

bool appendImageData(const uint8_t* data, size_t size)
{
  if (size == 0) return true;
  if (uploadState.dataBytes + size > ANIMATION_IMAGE_DATA_BYTES)
  {
    setUploadError("payload_too_large");
    return false;
  }

  uint32_t address = storageStart() + sizeof(ImageHeader) + uploadState.dataBytes;
  if (!ESP.flashWrite(address, data, size))
  {
    setUploadError("flash_write_failed");
    return false;
  }

  uploadState.crc = crc32Update(uploadState.crc, data, size);
  uploadState.dataBytes += size;
  yield();
  return true;
}

bool consumeHeaderBytes(const uint8_t*& data, size_t& size)
{
  if (uploadState.headerReady) return true;

  uint8_t* headerBytes = reinterpret_cast<uint8_t*>(&uploadState.header);
  while (size > 0 && uploadState.headerBytes < sizeof(ImageHeader))
  {
    headerBytes[uploadState.headerBytes++] = *data++;
    size--;
  }

  if (uploadState.headerBytes < sizeof(ImageHeader)) return true;

  uploadState.headerReady = true;
  if (!headerIsValid(uploadState.header))
  {
    setUploadError("invalid_header");
    return false;
  }
  return true;
}
}

bool animationImageIsAvailable()
{
  ImageHeader header;
  return readHeader(header) && headerIsValid(header);
}

bool animationImageRender()
{
  ImageHeader header;
  if (!readHeader(header) || !headerIsValid(header)) return false;

  uint32_t address = storageStart() + sizeof(ImageHeader);
  for (uint16_t y = 0; y < ANIMATION_IMAGE_HEIGHT; y += RENDER_ROWS)
  {
    uint16_t rows = ANIMATION_IMAGE_HEIGHT - y;
    if (rows > RENDER_ROWS) rows = RENDER_ROWS;
    size_t bytes = size_t(ANIMATION_IMAGE_WIDTH) * rows * 2;
    if (!ESP.flashRead(address, reinterpret_cast<uint8_t*>(renderBuffer), bytes)) return false;
    appState.tft.pushImage(0, y, ANIMATION_IMAGE_WIDTH, rows, renderBuffer);
    address += bytes;
    yield();
  }
  return true;
}

bool animationImageUploadBegin()
{
  memset(&uploadState, 0, sizeof(uploadState));
  uploadState.active = true;
  uploadState.crc = 0;

  if (!storageIsAvailable())
  {
    setUploadError("storage_unavailable");
    return false;
  }

  if (!eraseStorage())
  {
    setUploadError("flash_erase_failed");
    return false;
  }

  uint32_t erased = ERASED_WORD;
  ESP.flashWrite(storageStart(), reinterpret_cast<const uint8_t*>(&erased), sizeof(erased));
  return true;
}

bool animationImageUploadWrite(const uint8_t* data, size_t size)
{
  if (!uploadState.active)
  {
    setUploadError("upload_not_started");
    return false;
  }
  if (uploadState.failed) return false;
  if (data == nullptr && size > 0)
  {
    setUploadError("invalid_chunk");
    return false;
  }

  if (!consumeHeaderBytes(data, size)) return false;
  return appendImageData(data, size);
}

bool animationImageUploadEnd()
{
  if (!uploadState.active)
  {
    setUploadError("upload_not_started");
    return false;
  }
  uploadState.active = false;

  if (uploadState.failed) return false;
  if (!uploadState.headerReady)
  {
    setUploadError("missing_header");
    return false;
  }
  if (uploadState.dataBytes != ANIMATION_IMAGE_DATA_BYTES)
  {
    setUploadError("invalid_length");
    return false;
  }
  if (uploadState.crc != uploadState.header.crc32)
  {
    setUploadError("crc_mismatch");
    return false;
  }

  if (!ESP.flashWrite(storageStart(), reinterpret_cast<const uint8_t*>(&uploadState.header), sizeof(uploadState.header)))
  {
    setUploadError("header_write_failed");
    return false;
  }

  return true;
}

void animationImageUploadAbort()
{
  uploadState.active = false;
  setUploadError("upload_aborted");
}

const char* animationImageUploadError()
{
  return uploadState.error[0] ? uploadState.error : "unknown";
}

uint32_t animationImageStorageBytes()
{
  return storageIsAvailable() ? IMAGE_STORAGE_BYTES : 0;
}

uint32_t animationImagePayloadBytes()
{
  return ANIMATION_IMAGE_DATA_BYTES;
}
