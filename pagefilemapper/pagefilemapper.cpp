#include <Windows.h>

#include <iostream>
#include <memory>

// Simple class that creates a (potentially large, greater than system RAM)
// buffer that is backed by the system's page file. To use:
//
// 1. Create new instance with MemoryMappedBuffer::New().
// 1a. On success, returns valid pointer.
// 1b. On failure, returns nullptr; if opt_error is valid, provides error.
// 2. Call Address() to get the base address of the region
// 3. Destroy object by calling delete.

// Iff dest is a valid pointer, assign src to *dest.
template <typename T>
void MaybeAssign(T* dest, const T& src) {
  if (dest) {
    *dest = src;
  }
}

class MemoryMappedBuffer {
 public:
  // Static function that creates a new instance.
  // Returns valid pointer on success.
  // Returns nullptr on failure. If there is a failure, and if the caller
  // passes a valid DWORD ptr in opt_error, the reason for the failure
  // is returned in opt_error.
  static MemoryMappedBuffer* New(size_t size, DWORD* opt_error) {
    const DWORD low = (size & 0xffffffff);
    const DWORD high = ((size >> 32) & 0xffffffff);

    // Create the file mapping.
    HANDLE mapping = CreateFileMapping(
        INVALID_HANDLE_VALUE,  // No file, this means: back with the page file
        NULL,                  // No special security attributes
        PAGE_READWRITE |
            SEC_COMMIT,  // We want read/write, and already committed
        high,    // The size of the mapping is specified in two 32-bit DWORDS
        low,     // for historical reasons. We extracted these above.
        nullptr  // No name for the file mapping
    );

    if (mapping == NULL) {
      MaybeAssign(opt_error, GetLastError());
      return nullptr;
    }

    LPVOID region = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);
    if (!region) {
      CloseHandle(mapping);
      MaybeAssign(opt_error, GetLastError());
      return nullptr;
    }
    return new MemoryMappedBuffer(size, mapping, region);
  }

  // Destructor: Unmap the view and close the underlying file mapping.
  ~MemoryMappedBuffer() {
    UnmapViewOfFile(address_);
    CloseHandle(mapping_);
  }

  // Copy / assignment not allowed.
  MemoryMappedBuffer(const MemoryMappedBuffer&) = delete;
  MemoryMappedBuffer(MemoryMappedBuffer&&) = delete;
  MemoryMappedBuffer& operator=(const MemoryMappedBuffer&) = delete;
  MemoryMappedBuffer& operator=(MemoryMappedBuffer&&) = delete;

  // Return size of mapping.
  size_t Size() const { return size_; }

  // Return the base address of the mapped file.
  void* Address() const { return address_; }

 private:
  MemoryMappedBuffer(size_t size, HANDLE mapping, LPVOID region)
      : size_(size), mapping_(mapping), address_(region) {}

 private:
  size_t size_;
  HANDLE mapping_;
  LPVOID address_;
};

int main(int argc, char* argv[]) {
  // Allocate a memory mapped buffer using the page file as backing.
  const size_t kSixteenGiB = (16ull * 1024ull * 1024ull * 1024ull);
  DWORD error = 0;
  std::unique_ptr<MemoryMappedBuffer> buf(
      MemoryMappedBuffer::New(kSixteenGiB, &error));
  if (!buf) {
    std::cout << "Error " << error << " creating mapping." << std::endl;
    return 1;
  }

  // Write zeroes to every single byte in the region.
  char* ptr = reinterpret_cast<char*>(buf->Address());
  for (size_t i = 0; i < kSixteenGiB; ++i) {
    ptr[i] = 0;
  }

  std::cout << "Done!" << std::endl;

  return 0;
}
