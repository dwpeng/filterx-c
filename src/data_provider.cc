#include "data_provider.h"

namespace filterx {

PlainDataProvider::~PlainDataProvider() { this->close(); }

bool
PlainDataProvider::open(const std::string& path) {
  this->file.open(path, std::ios::in);
  return this->file.is_open();
}

void
PlainDataProvider::close() {
  if (this->file.is_open()) {
    this->file.close();
  }
}

std::optional<std::string_view>
PlainDataProvider::readline() {
  if (this->eof) {
    return std::nullopt;
  }
  while (1) {
    std::getline(this->file, this->line);
    if (this->file.eof()) {
      this->eof = true;
      return std::nullopt;
    }
    this->line_number++;
    if (this->line.empty()) {
      continue;
    }
    break;
  }
  return this->line;
}

GzipDataProvider::~GzipDataProvider() { this->close(); }

bool
GzipDataProvider::open(const std::string& path) {
  file = gzopen(path.c_str(), "rb");
  return file != nullptr;
}

void
GzipDataProvider::close() {
  if (file) {
    gzclose(file);
    file = nullptr;
  }
}

std::optional<std::string_view>
GzipDataProvider::readline() {
  if (this->eof) {
    return std::nullopt;
  }
  this->line.clear();
  while (true) {
    int read = gzread(file, buff, sizeof(buff));
    if (read == 0) {
      if (!this->decompressed_buff.empty()) {
        this->line = this->decompressed_buff;
        this->decompressed_buff.clear();
        return this->line;
      } else {
        this->eof = true;
        return std::nullopt;
      }
    }
    this->decompressed_buff.append(buff, read);
    auto pos = this->decompressed_buff.find('\n');
    if (pos != std::string::npos) {
      this->line = this->decompressed_buff.substr(0, pos);
      this->decompressed_buff.erase(0, pos + 1);
      break;
    }
  }
  this->line_number++;
  if (this->line.empty()) {
    return std::nullopt;
  }
  return this->line;
}

DataProvider*
createDataProvider(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    auto error = errno;
    auto error_msg = strerror(error);
    std::cerr << "Failed to open file " << path << ", message: " << error_msg
              << std::endl;
    exit(EXIT_FAILURE);
  }
  char buff[2];
  file.read(buff, 2);
  file.close();

  DataProvider* data_provider = nullptr;
  if (static_cast<unsigned char>(buff[0]) == 0x1f
      && static_cast<unsigned char>(buff[1]) == 0x8b) {
    data_provider = new GzipDataProvider();
  } else {
    data_provider = new PlainDataProvider();
  }

  if (!data_provider->open(path)) {
    delete data_provider;
    return nullptr;
  }
  return data_provider;
}

} // namespace filterx
