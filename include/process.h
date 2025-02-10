#include "param.h"
#include "record.h"

namespace filterx {

class Processor {
public:
  Processor(ProcessorParams& params);
  ~Processor() {
    for (auto record : this->records) {
      delete record;
    }
    if (this->output_file == stdout) {
      return;
    }
    if (this->output_file != nullptr) {
      fflush(this->output_file);
      fclose(this->output_file);
      this->output_file = nullptr;
    }
  }
  void add_record(Record* record);
  void prepare();
  void flush_all_records_to_file();
  void flush_all_records_to_file_row_mode();
  void drop_all_records_and_update_next();
  void process();

private:
  std::string output_buffer;
  FILE* output_file;
  std::vector<Record*> records;
  ProcessorParams params;
};

} // namespace filterx
