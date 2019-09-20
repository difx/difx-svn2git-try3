#ifndef VDIFFILEREADER_ISTREAM_H
#define VDIFFILEREADER_ISTREAM_H

#include <iostream>
#include <vdifio.h>

class VDIFFileReaderIStream : public std::istream
{
public:
  VDIFFileReaderIStream();
  void open(const char_type* filename, std::ios_base::openmode mode = std::ios_base::in);
  void close();

  bool is_open();
  bool fail() const;
  bool bad() const;
  bool eof() const;
  void clear();

  std::streamsize gcount() const;

  VDIFFileReaderIStream& read(char_type* s, std::streamsize count);
  VDIFFileReaderIStream& seekg(off_type off, std::ios_base::seekdir way);
  int peek();

public:
  void setHintFramespersec(int fps) { fps_hint = fps; }

protected:
  bool opened;
  bool failed;
  bool endoffile;
  vdif_file_summary summary;
  vdif_file_reader reader;
  std::streamsize nread;
  int fps_hint;

};

#endif

// vim: shiftwidth=2:softtabstop=2:expandtab
