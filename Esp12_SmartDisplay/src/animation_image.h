#pragma once

#include <Arduino.h>

const uint16_t ANIMATION_IMAGE_WIDTH = 240;
const uint16_t ANIMATION_IMAGE_HEIGHT = 240;
const uint16_t ANIMATION_IMAGE_MAX_FRAMES = 8;
const uint32_t ANIMATION_IMAGE_DATA_BYTES = uint32_t(ANIMATION_IMAGE_WIDTH) * ANIMATION_IMAGE_HEIGHT * 2;
const uint32_t ANIMATION_IMAGE_MAX_DATA_BYTES = ANIMATION_IMAGE_DATA_BYTES * ANIMATION_IMAGE_MAX_FRAMES;
const uint32_t ANIMATION_IMAGE_UPLOAD_BYTES = ANIMATION_IMAGE_MAX_DATA_BYTES + 32;

bool animationImageIsAvailable();
bool animationImageRender();
void animationImageUpdateIfNeeded();

bool animationImageUploadBegin();
bool animationImageUploadWrite(const uint8_t* data, size_t size);
bool animationImageUploadEnd();
void animationImageUploadAbort();
const char* animationImageUploadError();
uint32_t animationImageStorageBytes();
uint32_t animationImagePayloadBytes();
