#include "vdiffilereader_istream.h"

#include <iostream>
#include <vdifio.h>

VDIFFileReaderIStream::VDIFFileReaderIStream()
{
  opened = false;
  failed = false;
  endoffile = false;
  fps_hint = 0;
}

void VDIFFileReaderIStream::open(const char_type* filename, std::ios_base::openmode mode)
{
  if (!opened) {
    resetvdiffilesummary(&summary);
    bool ok = (summarizevdiffile(&summary, filename, 0) >= 0);
    if (summary.framesPerSecond == 0 && fps_hint > 0) {
       summary.framesPerSecond = fps_hint;
    }
    printvdiffilesummary(&summary);
    ok &= (vdifreaderOpen(&summary, &reader) == 0);
    failed = !ok;
    opened = ok;
  }
}

void VDIFFileReaderIStream::close()
{
  if (opened)
  {
    vdifreaderClose(&reader);
  }
  opened = false;
}

bool VDIFFileReaderIStream::is_open()
{
  return opened;
}

bool VDIFFileReaderIStream::fail() const
{
  return failed;
}

bool VDIFFileReaderIStream::bad() const
{
  return failed || !opened;
}

bool VDIFFileReaderIStream::eof() const
{
  return endoffile;
}

void VDIFFileReaderIStream::clear()
{
  failed = false;
  endoffile = false;
}

std::streamsize VDIFFileReaderIStream::gcount() const
{
  return nread;
}

VDIFFileReaderIStream& VDIFFileReaderIStream::read(char_type* buf, std::streamsize count)
{
  nread = vdifreaderRead(&reader, (void*)buf, count);
  std::cout << "vrs::read(" << count << ") = " << nread << "\n";
  if (nread == 0) {
    endoffile = true;
  } else if (nread < 0) {
    failed = true;
    nread = 0;
  }
  return *this;
}

VDIFFileReaderIStream& VDIFFileReaderIStream::seekg(off_type off, std::ios_base::seekdir way)
{
  if (way != std::ios_base::beg) {
    std::cerr << "Warning: VDIFFileReaderIStream::seekg(..., whence) supports only ios_base::beg. Not doing the seek.\n";
  } else {
    failed = (vdifreaderSeek(&reader, off) != 0);
  }
  return *this;
}

int VDIFFileReaderIStream::peek()
{
  unsigned char buf[10000];
  off_type orig_off = reader.firstframeoffset;
  size_t nrd = vdifreaderRead(&reader, (void*)buf, 1);
  std::cout << "vrs::peek() nrd=" << nrd << "\n"; 
  this->seekg(orig_off, std::ios_base::beg);
  return buf[0];
}
