/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "memory/ColumnarBatch.h"

#include <arrow/ipc/message.h>
#include <arrow/ipc/options.h>

#include "Options.h"
#include "compute/ResultIterator.h"
#include "utils/Compression.h"

namespace gluten {

class ShuffleReader {
 public:
  explicit ShuffleReader(std::shared_ptr<arrow::Schema> schema, ShuffleReaderOptions options, arrow::MemoryPool* pool);

  virtual ~ShuffleReader() = default;

  // FIXME iterator should be unique_ptr or un-copyable singleton
  virtual std::shared_ptr<ResultIterator> readStream(std::shared_ptr<arrow::io::InputStream> in);

  arrow::Status close();

  int64_t getDecompressTime() const {
    return decompressTime_;
  }

  int64_t getIpcTime() const {
    return ipcTime_;
  }

  int64_t getDeserializeTime() const {
    return deserializeTime_;
  }

  arrow::MemoryPool* getPool() const;

 protected:
  arrow::MemoryPool* pool_;
  int64_t decompressTime_ = 0;
  int64_t ipcTime_ = 0;
  int64_t deserializeTime_ = 0;

  ShuffleReaderOptions options_;

 private:
  std::shared_ptr<arrow::Schema> schema_;
};

} // namespace gluten
