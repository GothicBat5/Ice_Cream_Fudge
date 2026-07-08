#include <algorithm>
#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#define TRACE_CALL(expression) \
  do {  \
    SCOPED_TRACE(#expression); \
    expression; \
  } while (0)

using apps::SavedFilesService;
using extensions::SavedFileEntry;

namespace {

std::string GenerateId(int i) 
{
  return base::NumberToString(i) + ":filename.ext";
}

}  // namespace

class SavedFilesServiceUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    static const char kManifest[] = "{" "  \"app\": {"
        "    \"background\": {"
        "      \"scripts\": [\"background.js\"]"
        "    }"
        "  },"
        "  \"permissions\": ["
        "    {\"fileSystem\": [\"retainEntries\"]}"
        "  ]"
        "}";
    testing::Test::SetUp();
    extension_ = env_.MakeExtension(base::test::ParseJsonDict(kManifest));
    service_ = SavedFilesService::Get(env_.profile());
    path_ = base::FilePath(FILE_PATH_LITERAL("filename.ext"));
  }

  void TearDown() override {
    SavedFilesService::ClearMaxSequenceNumberForTest();
    SavedFilesService::ClearLruSizeForTest();
    testing::Test::TearDown();
  }

  void CheckEntrySequenceNumber(int id, int sequence_number) 
  {
    std::string id_string = GenerateId(id);
    SCOPED_TRACE(id_string);
    EXPECT_TRUE(service_->IsRegistered(extension_->id(), id_string));
    const SavedFileEntry* entry = service_->GetFileEntry(extension_->id(), id_string);
    ASSERT_TRUE(entry);
    EXPECT_EQ(id_string, entry->id);
    EXPECT_EQ(path_, entry->path);
    EXPECT_TRUE(entry->is_directory);
    EXPECT_EQ(sequence_number, entry->sequence_number);
  }

  void CheckRangeEnqueuedInOrder(int start, int end) 
  {
    SavedFileEntry entry;
    
    for (int i = start; i < end; i++) 
    {
      CheckEntrySequenceNumber(i, i + 1);
    }
  }

  extensions::TestExtensionEnvironment env_;
  raw_ptr<const extensions::Extension> extension_;
  raw_ptr<SavedFilesService> service_;
  base::FilePath path_;
};

TEST_F(SavedFilesServiceUnitTest, RetainTwoFilesTest) 
{
  service_->RegisterFileEntry(extension_->id(), GenerateId(1), path_, true);
  service_->RegisterFileEntry(extension_->id(), GenerateId(2), path_, true);
  service_->RegisterFileEntry(extension_->id(), GenerateId(3), path_, true);

  TRACE_CALL(CheckEntrySequenceNumber(1, 0));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 0));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));
  EXPECT_FALSE(service_->IsRegistered(extension_->id(), "another id"));
  SavedFileEntry entry;
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), "another id"));
  service_->ClearQueueIfNoRetainPermission(extension_);
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  TRACE_CALL(CheckEntrySequenceNumber(3, 0));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(1, 3));
  TRACE_CALL(CheckEntrySequenceNumber(2, 2));
  EXPECT_FALSE(service_->IsRegistered(extension_->id(), GenerateId(3)));
}

TEST_F(SavedFilesServiceUnitTest, NoRetainEntriesPermissionTest) 
{
  static const char kManifest[] = "{\"app\": {\"background\": 
  {\"scripts\": [\"background.js\"]}}," "\"permissions\": [\"fileSystem\"]}";
  extension_ = nullptr;
  extension_ = env_.MakeExtension(base::test::ParseJsonDict(kManifest));
  service_->RegisterFileEntry(extension_->id(), GenerateId(1), path_, true);
  TRACE_CALL(CheckEntrySequenceNumber(1, 0));
  SavedFileEntry entry;
  service_->EnqueueFileEntry(extension_->id(), GenerateId(1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 1));
  EXPECT_FALSE(service_->IsRegistered(extension_->id(), "another id"));
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), "another id"));

  service_->ClearQueueIfNoRetainPermission(extension_);
  std::vector<SavedFileEntry> entries = service_->GetAllFileEntries(extension_->id());
  EXPECT_TRUE(entries.empty());
}

TEST_F(SavedFilesServiceUnitTest, EvictionTest) 
{
  SavedFilesService::SetLruSizeForTest(10);
  for (int i = 0; i < 10; i++) 
  {
    service_->RegisterFileEntry(extension_->id(), GenerateId(i), path_, true);
    service_->EnqueueFileEntry(extension_->id(), GenerateId(i));
  }
  service_->RegisterFileEntry(extension_->id(), GenerateId(10), path_, true);

  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 10));
  TRACE_CALL(CheckEntrySequenceNumber(10, 0));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(10));
  TRACE_CALL(CheckEntrySequenceNumber(0, 0));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 11));
  service_->Clear(extension_->id());
  SavedFileEntry entry;
  EXPECT_FALSE(service_->GetFileEntry(extension_->id(), GenerateId(0)));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 11));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 12));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 1));
  TRACE_CALL(CheckRangeEnqueuedInOrder(3, 11));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(2, 12));
  TRACE_CALL(CheckRangeEnqueuedInOrder(1, 1));
  TRACE_CALL(CheckRangeEnqueuedInOrder(3, 11));
}

TEST_F(SavedFilesServiceUnitTest, SequenceNumberCompactionTest) 
{
  SavedFilesService::SetMaxSequenceNumberForTest(8);
  SavedFilesService::SetLruSizeForTest(8);
  
  for (int i = 0; i < 4; i++) 
  {
    service_->RegisterFileEntry(extension_->id(), GenerateId(i), path_, true);
    service_->EnqueueFileEntry(extension_->id(), GenerateId(i));
  }
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(3));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(2));

  TRACE_CALL(CheckEntrySequenceNumber(0, 1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 7));
  TRACE_CALL(CheckEntrySequenceNumber(3, 6));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckEntrySequenceNumber(0, 1));
  TRACE_CALL(CheckEntrySequenceNumber(1, 2));
  TRACE_CALL(CheckEntrySequenceNumber(2, 7));
  TRACE_CALL(CheckEntrySequenceNumber(3, 6));
  service_->EnqueueFileEntry(extension_->id(), GenerateId(3));
  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 4));
  service_->Clear(extension_->id());
  TRACE_CALL(CheckRangeEnqueuedInOrder(0, 4));
}
