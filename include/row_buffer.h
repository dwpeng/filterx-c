#pragma once

#include <cassert>

#include "row.h"

namespace filterx {
class RowBuffer {

public:
  RowBuffer(std::vector<uint32_t>& row_keys, std::vector<RowKeyType>& key_types,
            std::vector<RowKeySortOrder>& sort_order, char separator)
      : separator(separator), key1(row_keys, key_types, sort_order),
        key2(row_keys, key_types, sort_order) {
    this->rows.reserve(16);
    this->last_read_row = std::nullopt;
    this->newline = Row();
    this->newline.set_separator(separator);
  }

  bool
  add_row(char* line, uint32_t row_idx) {
    assert(!this->last_read_row.has_value());

    this->newline.set_row(line);
    this->newline.row_idx = row_idx;
    this->newline.split();

    if (this->rows.empty()) {
      this->rows.push_back(this->newline);
      this->newline.clear();
      return true;
    }

    this->key1.update_row(&this->rows.back());
    this->key2.update_row(&this->newline);

    if (!this->key1.equals(&this->key2)) {
      this->last_read_row = this->newline;
      this->newline.clear();
      return false;
    }
    this->rows.push_back(this->newline);
    this->newline.clear();
    return true;
  }

  void
  consume() {
    this->rows.clear();
    if (this->last_read_row.has_value()) {
      this->rows.push_back(this->last_read_row.value());
      this->last_read_row.reset();
    }
  }

  size_t
  size() {
    return this->rows.size();
  }

  std::optional<Row*>
  get_row(int index) {
    if (index < 0 || index >= this->rows.size()) {
      return std::nullopt;
    }
    return &this->rows[index];
  }

  std::optional<RowKey*>
  key() {
    if (this->rows.empty()) {
      return std::nullopt;
    }
    return &this->key1;
  }

private:
  char separator;
  std::vector<Row> rows;
  std::optional<Row> last_read_row;
  Row newline;
  RowKey key1;
  RowKey key2;
};
} // namespace filterx
