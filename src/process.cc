#include "process.h"
#include <set>

namespace filterx {

Processor::Processor(ProcessorParams& params) : params(params) {
  if (strcmp(params.output_path.c_str(), "-") == 0) {
    this->output_file = stdout;
    return;
  }
  this->output_file = fopen(params.output_path.c_str(), "w");
  if (this->output_file == nullptr) {
    fprintf(stderr, "Failed to open file: %s\n", params.output_path.c_str());
    fflush(stderr);
    exit(EXIT_FAILURE);
  }
}

void
Processor::add_record(Record* record) {
  if (this->records.empty()) {
    record->set_must_exist(ExistConditionMust);
  }
  this->records.push_back(record);
}

void
Processor::prepare() {
  for (int i = 0; i < this->records.size(); i++) {
    RecordStatus s;
    while (1) {
      s = this->records[i]->next();
      if (s == RecordStatusEof) {
        if (i == 0) {
          fprintf(stderr, "The first record is empty\n");
          fflush(stderr);
          exit(EXIT_FAILURE);
        }
        break;
      }
      if (s == RecordStatusWaitConsumption) {
        break;
      }
    }
  }
}

void
Processor::flush_all_records_to_file() {
  int max_rows = 0;
  for (int i = 0; i < this->records.size(); i++) {
    if (this->records[i]->status() != RecordStatusWaitOutput) {
      continue;
    }
    auto record = this->records[i];
    if (record->get_cut_columns().empty()) {
      continue;
    }
    auto nrows = record->get_record_limit();
    if (nrows > max_rows) {
      max_rows = nrows;
    }
  }
  for (int n = 0; n < max_rows; n++) {
    for (int i = 0; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        auto cut = this->records[i]->get_cut_columns();
        char placehoder = this->records[i]->get_placehoder();
        for (int j = 0; j < cut.size(); j++) {
          fprintf(this->output_file, "%c%c", placehoder,
                  this->params.output_separator);
        }
        continue;
      }
      auto record = this->records[i];
      auto buffer = record->buffer();
      auto cut = record->get_cut_columns();
      if (n >= record->get_record_limit()) {
        char placehoder = record->get_placehoder();
        for (int j = 0; j < cut.size(); j++) {
          fprintf(this->output_file, "%c%c", placehoder,
                  this->params.output_separator);
        }
        continue;
      }
      auto row = buffer->get_row(n).value_or(nullptr);
      char placehoder = record->get_placehoder();
      for (int j = 0; j < cut.size(); j++) {
        auto item = row->get_item(cut[j]);
        if (item.has_value()) {
          auto item_value = item.value();
          fprintf(this->output_file, "%s%c", item_value.data(),
                  this->params.output_separator);
        } else {
          fprintf(this->output_file, "%c%c", placehoder,
                  this->params.output_separator);
        }
      }
    }
    // remove the last separator
    fseek(this->output_file, -1, SEEK_CUR);
    fprintf(this->output_file, "\n");
  }
}

void
Processor::flush_root_record_to_file() {
  // output the first record
  auto record = this->records[0];
  auto buffer = record->buffer();
  auto cut = record->get_cut_columns();
  for (int n = 0; n < record->get_record_limit(); n++) {
    auto row = buffer->get_row(n).value_or(nullptr);
    if (row == nullptr) {
      continue;
    }
    char placehoder = record->get_placehoder();
    for (int j = 0; j < cut.size(); j++) {
      auto item = row->get_item(cut[j]);
      if (item.has_value()) {
        auto item_value = item.value();
        fprintf(this->output_file, "%s%c", item_value.data(),
                this->params.output_separator);
      } else {
        fprintf(this->output_file, "%c%c", placehoder,
                this->params.output_separator);
      }
    }
    // output the rest of the records
    for (int i = 1; i < this->records.size(); i++) {
      auto cut = this->records[i]->get_cut_columns();
      for (int j = 0; j < cut.size(); j++) {
        fprintf(this->output_file, "%c%c", placehoder,
                this->params.output_separator);
      }
    }
    // remove the last separator
    fseek(this->output_file, -1, SEEK_CUR);
    fprintf(this->output_file, "\n");
  }
}

void
Processor::drop_all_records_and_update_next() {
  for (int i = 0; i < this->records.size(); i++) {
    auto s = this->records[i]->status();
    if (s == RecordStatusWaitOutput || s == RecordStatusNotPassCondition
        || s == RecordStatusUnavailable || s == RecordStatusNotPassExists) {
      this->records[i]->next();
    }
  }
}

RowKey*
get_key(Record* r) {
  RowKey* row_key = nullptr;
  while (1) {
    row_key = r->key().value_or(nullptr);
    if (row_key == nullptr) {
      auto s = r->next();
      if (s == RecordStatusEof) {
        return nullptr;
      }
      assert(s == RecordStatusWaitConsumption);
    } else {
      assert(r->status() == RecordStatusWaitConsumption);
      int all_keys_have_value = 1;
      for (int i = 0; i < row_key->size(); i++) {
        auto key = row_key->get_key(i).value_or(nullptr);
        if (key == nullptr) {
          all_keys_have_value = 0;
          break;
        }
      }
      if (all_keys_have_value) {
        return row_key;
      } else {
        auto s = r->next();
        if (s == RecordStatusEof) {
          return nullptr;
        }
      }
    }
  }
  return row_key;
}

RowKey*
topest_of_2(RowKey* left_key, RowKey* right_key) {
  // if left key is more at the top return left key otherwise return right key
  for (int i = 0; i < left_key->size(); i++) {
    auto left_key_item = left_key->get_key(i).value_or(nullptr);
    auto right_key_item = right_key->get_key(i).value_or(nullptr);
    assert(left_key_item != nullptr);
    assert(right_key_item != nullptr);
    if (left_key_item->equals(right_key_item)) {
      continue;
    } else {
      // 升序
      if (left_key_item->sort_order == RowKeySortOrderAsc) {
        if (left_key_item->small_than(right_key_item)) {
          return left_key;
        } else {
          return right_key;
        }
      } else {
        // 降序
        if (left_key_item->big_than(right_key_item)) {
          return left_key;
        } else {
          return right_key;
        }
      }
    }
  }
  return left_key;
}

RowKey*
the_topest_key(std::vector<Record*>& records, std::vector<int>* topest_idx) {
  topest_idx->clear();
  if (records.size() == 0) {
    return nullptr;
  }

  RowKey* top = nullptr;
  int idx = 0;
  while (idx < records.size()) {
    auto r = records[idx];
    if (r->status() != RecordStatusWaitConsumption) {
      idx++;
      continue;
    }
    top = get_key(r);
    if (top != nullptr) {
      break;
    }
    idx++;
  }
  if (top == nullptr) {
    topest_idx->clear();
    return nullptr;
  }

  if (records.size() == 1) {
    topest_idx->push_back(0);
    return top;
  }

  for (int i = idx; i < records.size(); i++) {
    auto r = records[i];
    if (r->status() != RecordStatusWaitConsumption) {
      continue;
    }
    RowKey* other = get_key(r);

    if (other == nullptr) {
      continue;
    }

    top = topest_of_2(top, other);
  }

  for (int i = 0; i < records.size(); i++) {
    auto r = records[i];
    if (r->status() != RecordStatusWaitConsumption) {
      continue;
    }
    RowKey* other = get_key(r);
    if (other == nullptr) {
      continue;
    }

    if (top->equals(other)) {
      topest_idx->push_back(i);
    }
  }
  return top;
}

void
Processor::process() {
  if (this->records.size() < this->params.min_count) {
    fprintf(stderr, "The number of records is less than the minimum count\n");
    fflush(stderr);
    return;
  }
  auto other_records = this->records;
  // remove the root record
  other_records.erase(other_records.begin());
  uint32_t ouput_number = 0;
  auto topest_idx = std::vector<int>{};
  assert(this->records.size() > 0);
  while (1) {
    int c = 0;
    auto root_record_keys = get_key(this->records[0]);
    if (root_record_keys == nullptr) {
      break;
    }
    records[0]->set_status(RecordStatusWaitOutput);
    c++;
    int stop = 0;
    while (!stop) {

      auto other_top_key = the_topest_key(other_records, &topest_idx);

      if (other_top_key == nullptr) {
        break;
      }
      if (root_record_keys->equals(other_top_key)) {
        this->records[0]->set_status(RecordStatusWaitOutput);
        for (int j = 0; j < topest_idx.size(); j++) {
          auto idx = topest_idx[j];
          other_records[idx]->set_status(RecordStatusWaitOutput);
          c++;
        }
        break;
      } else {
        auto which_is_top = topest_of_2(root_record_keys, other_top_key);
        if (which_is_top != root_record_keys) {
          for (int j = 0; j < topest_idx.size(); j++) {
            auto idx = topest_idx[j];
            other_records[idx]->set_status(RecordStatusUnavailable);
            auto s = other_records[idx]->next();
            if (s == RecordStatusEof) {
              stop = 1;
            }
          }
        } else {
          // try to update next keys in root record
          if (this->records[0]->status() == RecordStatusWaitOutput) {
            double fc = 1.0 / this->records.size();
            if (1 >= this->params.min_count && this->params.max_count >= 1
                && fc >= this->params.fmin_count
                && fc <= this->params.fmax_count) {
              this->flush_root_record_to_file();
              ouput_number++;
              if (this->params.output_limit > 0
                  && ouput_number >= this->params.output_limit) {
                return;
              }
            }
          }
          this->records[0]->set_status(RecordStatusUnavailable);
          auto s = this->records[0]->next();
          if (s == RecordStatusEof) {
            stop = 1;
          }
          root_record_keys = get_key(this->records[0]);
          if (root_record_keys == nullptr) {
            break;
          }
        }
      }
    }

    bool drop_all = false;
    for (int i = 1; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        if (this->records[i]->get_exist() == ExistConditionMust) {
          this->records[i]->set_status(RecordStatusNotPassExists);
          drop_all = true;
          break;
        }
      } else {
        if (this->records[i]->get_exist() == ExistConditionNot) {
          this->records[i]->set_status(RecordStatusNotPassExists);
          drop_all = true;
          break;
        }
      }
    }

    if (drop_all) {
      this->drop_all_records_and_update_next();
      continue;
    }

    // check if the number of records is within the range
    if (c < this->params.min_count || c > this->params.max_count) {
      this->drop_all_records_and_update_next();
      continue;
    }
    float fc = 1.0 * c / this->records.size();
    if (fc < this->params.fmin_count || fc > this->params.fmax_count) {
      this->drop_all_records_and_update_next();
      continue;
    }
    this->flush_all_records_to_file();
    ouput_number++;
    if (this->params.output_limit > 0
        && ouput_number >= this->params.output_limit) {
      return;
    }

    for (int i = 0; i < this->records.size(); i++) {
      if (this->records[i]->status() != RecordStatusWaitOutput) {
        continue;
      }
      this->records[i]->next();
    }
  }
}

} // namespace filterx
