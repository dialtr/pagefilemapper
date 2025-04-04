#include <Windows.h>

#include <iostream>
#include <memory>

class MemoryMappedBuffer {
 public:
  // TODO(tdial): Make static constructor that returns NULL or valid object.
  MemoryMappedBuffer(size_t size)
      : size_(size), mapping_(INVALID_HANDLE_VALUE), region_(nullptr) {
    Create(size);
  }

  ~MemoryMappedBuffer() { Delete(); }

  // Copy / assignment not allowed.
  MemoryMappedBuffer(const MemoryMappedBuffer&) = delete;
  MemoryMappedBuffer(MemoryMappedBuffer&&) = delete;
  MemoryMappedBuffer& operator=(const MemoryMappedBuffer&) = delete;
  MemoryMappedBuffer& operator=(MemoryMappedBuffer&&) = delete;

  size_t Size() const { return size_; }

  void* Region() const { return region_; }

  bool IsValid() const { return region_ != nullptr; }

 private:
  // TODO(tdial): propagate the error somehow. The static constructor could
  // return something like a StatusOr<>, for example.
  DWORD Create(size_t size) {
    // size_t is 64 bits. DWORDs required by CreateFileMapping() are 32 bits.
    // So we need to split out the low and high DWORDS.
    const DWORD low = size & 0xffffffff;
    size >>= 32;
    const DWORD high = size & 0xffffffff;

    mapping_ = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
                                 PAGE_READWRITE | SEC_COMMIT, high, low, NULL);
    if (mapping_ == NULL) {
      return GetLastError();
    }

    region_ = MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, 0);
    if (!region_) {
      DWORD lasterror = GetLastError();
      CloseHandle(mapping_);
      mapping_ = INVALID_HANDLE_VALUE;
      return lasterror;
    }
    return 0;
  }

  void Delete() {
    if (!region_) {
      return;
    }
    UnmapViewOfFile(region_);
    CloseHandle(mapping_);
  }

 private:
  size_t size_;
  HANDLE mapping_;
  LPVOID region_;
};

int main(int argc, char* argv[]) {
  // Allocate a memory mapped buffer using the page file as backing.
  const size_t kSixteenGiB = (16ull * 1024ull * 1024ull * 1024ull);
  std::unique_ptr<MemoryMappedBuffer> buf(new MemoryMappedBuffer(kSixteenGiB));
  if (!buf->IsValid()) {
    std::cout << "error creating mapping." << std::endl;
    return 1;
  }

  // Write zeroes to every single byte in the region.
  char* ptr = reinterpret_cast<char*>(buf->Region());
  for (size_t i = 0; i < kSixteenGiB; ++i) {
    ptr[i] = 0;
  }
  std::cout << "Done!" << std::endl;

  return 0;
}
