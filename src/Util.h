#pragma once

#include "SDLComponents/Logger.h"

#include <stddef.h>
#include <string>

size_t nextPow2(size_t v);
void*  loadFile(Logger* logger, const std::string& path, size_t* size);
void   saveFile(Logger* logger, const std::string& path, const void* data, size_t size);
