#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "zlib.h"

namespace filterx {

class DataProvider {
public:
  std::string line;
  uint32_t line_number = 0;

  bool eof = false;
  DataProvider() { line.reserve(1024); }
  virtual ~DataProvider() = default;

  virtual bool open(const std::string& path) = 0;
  virtual std::optional<std::string_view> readline() = 0;
  virtual void close() = 0;
};

class PlainDataProvider : public DataProvider {

public:
  ~PlainDataProvider() override;

  bool open(const std::string& path) override;

  void close() override;

  std::optional<std::string_view> readline() override;

private:
  std::ifstream file;

  std::string line;

  bool eof = false;
};

class GzipDataProvider : public DataProvider {
public:
  ~GzipDataProvider() override;
  bool open(const std::string& path) override;
  void close() override;
  std::optional<std::string_view> readline() override;

private:
  gzFile file;
  std::string decompressed_buff;
  char buff[1024];
};

DataProvider* createDataProvider(const std::string& path);

} // namespace filterx
