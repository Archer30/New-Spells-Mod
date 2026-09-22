#pragma once

#include <map>
#include <string>
#include <vector>

struct NsVirtualFile
{
   std::string name;               // lower case, as the game asks for it
   std::string diskPath;
   std::vector<unsigned char> data;
   bool loaded;
};

std::map<std::string, NsVirtualFile> nsVirtualFiles;

// Icon sheets that get blank frames appended (SpellInt.def has a leading null frame).
struct NsPaddedSheet { const char* name; int frames; };
const NsPaddedSheet nsPaddedSheets[] =
{
   {"spellint.def", SPELLS_MAX + 1},
   {"spells.def", SPELLS_MAX},
   {"spellscr.def", SPELLS_MAX},
   {"spellbon.def", SPELLS_MAX},
};

// The file the game asked for last, when it is one of ours. ReadFromLod gets no name, only
// the lod object, so the answer to SearchFileInLoad is remembered per lod object.
struct NsPendingRead
{
   void* lod;
   NsVirtualFile* file;
   unsigned int pos;
   unsigned char header[32];       // fake lod entry: name[16], offset, size, type, csize
};
NsPendingRead nsPending = {0};

HiHook* nsHookSearchFile;
HiHook* nsHookLoadFile;
HiHook* nsHookReadFile;

void nsLodLog(const char* format, ...)
{
   char buffer[512];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, sizeof(buffer), format, args);
   va_end(args);
   Era::WriteLog("NewSpells", "Virtual LOD", buffer);
}

std::string nsLowerName(const char* name)
{
   std::string s = name ? name : "";
   for (std::size_t i = 0; i < s.size(); ++i)
      s[i] = (char)tolower((unsigned char)s[i]);
   return s;
}

NsVirtualFile& nsAddVirtualFile(const char* name)
{
   NsVirtualFile& f = nsVirtualFiles[nsLowerName(name)];
   f.name = nsLowerName(name);
   f.loaded = false;
   return f;
}

void nsAddMemoryFile(const char* name, const std::vector<unsigned char>& data)
{
   NsVirtualFile& f = nsAddVirtualFile(name);
   f.data = data;
   f.loaded = true;
}

void nsScanLooseFiles()
{
   const char* dir = "Data\\NewSpells\\Files";
   char mask[MAX_PATH];
   WIN32_FIND_DATAA fd;
   int count = 0;

   sprintf(mask, "%s\\*.*", dir);
   HANDLE h = FindFirstFileA(mask, &fd);
   if (h == INVALID_HANDLE_VALUE)
      return;

   do
   {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         continue;
      if (strlen(fd.cFileName) > 15)
      {
         nsLodLog("%s\\%s skipped, lod names have at most 15 characters", dir, fd.cFileName);
         continue;
      }
      NsVirtualFile& f = nsAddVirtualFile(fd.cFileName);
      f.diskPath = std::string(dir) + "\\" + fd.cFileName;
      ++count;
   }
   while (FindNextFileA(h, &fd));
   FindClose(h);

   nsLodLog("%d loose file(s) in %s", count, dir);
}

bool nsLoadVirtualFile(NsVirtualFile& f)
{
   if (f.loaded)
      return true;

   FILE* fp = fopen(f.diskPath.c_str(), "rb");
   if (!fp)
   {
      nsLodLog("cannot read %s", f.diskPath.c_str());
      return false;
   }
   fseek(fp, 0, SEEK_END);
   long size = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   f.data.resize(size > 0 ? size : 0);
   if (size > 0)
      fread(&f.data[0], 1, size, fp);
   fclose(fp);
   f.loaded = true;
   return true;
}

void nsPutDword(std::vector<unsigned char>& out, unsigned int value)
{
   out.push_back((unsigned char)value);
   out.push_back((unsigned char)(value >> 8));
   out.push_back((unsigned char)(value >> 16));
   out.push_back((unsigned char)(value >> 24));
}

void nsSetDword(std::vector<unsigned char>& out, std::size_t at, unsigned int value)
{
   out[at] = (unsigned char)value;
   out[at + 1] = (unsigned char)(value >> 8);
   out[at + 2] = (unsigned char)(value >> 16);
   out[at + 3] = (unsigned char)(value >> 24);
}

unsigned int nsGetDword(const unsigned char* p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// A 1x1 transparent frame in format 1: one line offset and one run of palette index 0.
void nsPutBlankFrame(std::vector<unsigned char>& out, int fullWidth, int fullHeight)
{
   nsPutDword(out, 6);          // data size
   nsPutDword(out, 1);          // format
   nsPutDword(out, fullWidth);
   nsPutDword(out, fullHeight);
   nsPutDword(out, 1);          // width
   nsPutDword(out, 1);          // height
   nsPutDword(out, 0);          // left
   nsPutDword(out, 0);          // top
   nsPutDword(out, 4);          // line 0 offset
   out.push_back(0);            // run of palette index 0
   out.push_back(0);            // length 1
}

// A def whose frames are all blank. ERA draws Data\Defs\<name>\0_<frame>.png over them.
void nsBuildBlankDef(std::vector<unsigned char>& out, int type, int width, int height, int frames)
{
   out.clear();
   nsPutDword(out, type);
   nsPutDword(out, width);
   nsPutDword(out, height);
   nsPutDword(out, 1);
   // Palette: index 0 is the transparent cyan, the rest black.
   out.push_back(0); out.push_back(255); out.push_back(255);
   for (int i = 1; i < 256; ++i) { out.push_back(0); out.push_back(0); out.push_back(0); }

   nsPutDword(out, 0);          // group id
   nsPutDword(out, frames);
   nsPutDword(out, 0);
   nsPutDword(out, 0);
   for (int i = 0; i < frames; ++i)
   {
      char name[13];
      sprintf(name, "NSF%05d.pcx", i);
      for (int k = 0; k < 13; ++k)
         out.push_back(k < 12 ? (unsigned char)name[k] : 0);
   }
   std::size_t offsetsAt = out.size();
   for (int i = 0; i < frames; ++i)
      nsPutDword(out, 0);
   unsigned int frameAt = out.size();
   nsPutBlankFrame(out, width, height);
   for (int i = 0; i < frames; ++i)
      nsSetDword(out, offsetsAt + 4 * i, frameAt);
}

bool nsRegisterSyntheticDef(const char* name, int frames, int width, int height)
{
   if (!name || !*name || strlen(name) > 15 || frames <= 0 || width <= 0 || height <= 0)
   {
      nsLodLog("%s needs animationFrames, animationWidth and animationHeight", name ? name : "?");
      return false;
   }
   std::vector<unsigned char> blob;
   nsBuildBlankDef(blob, 0x40, width, height, frames);
   nsAddMemoryFile(name, blob);
   nsLodLog("generated %s with %d blank frames of %dx%d", name, frames, width, height);
   return true;
}

// Appends blank frames to group 0 of a def until it has `count` frames.
bool nsPadDef(const std::vector<unsigned char>& in, int count, std::vector<unsigned char>& out)
{
   if (in.size() < 16 + 768 + 16)
      return false;

   const unsigned char* p = &in[0];
   int width = nsGetDword(p + 4);
   int height = nsGetDword(p + 8);
   int groups = nsGetDword(p + 12);
   std::size_t off = 16 + 768;
   std::vector<int> groupFrames;
   std::vector<std::size_t> groupAt;

   for (int g = 0; g < groups; ++g)
   {
      if (off + 16 > in.size())
         return false;
      int frames = nsGetDword(p + off + 4);
      groupAt.push_back(off);
      groupFrames.push_back(frames);
      off += 16 + 13 * frames + 4 * frames;
   }
   if (groups < 1 || off > in.size())
      return false;

   int have = groupFrames[0];
   if (have >= count)
   {
      out = in;
      return true;
   }
   int add = count - have;
   std::size_t tableEnd = off;
   std::size_t tableDelta = add * (13 + 4);

   out.clear();
   out.insert(out.end(), p, p + 16 + 768);
   std::vector<std::size_t> offsetTables;

   for (int g = 0; g < groups; ++g)
   {
      const unsigned char* gp = p + groupAt[g];
      int frames = groupFrames[g];
      int newFrames = g == 0 ? count : frames;
      out.insert(out.end(), gp, gp + 4);
      nsPutDword(out, newFrames);
      out.insert(out.end(), gp + 8, gp + 16);
      out.insert(out.end(), gp + 16, gp + 16 + 13 * frames);
      for (int i = frames; i < newFrames; ++i)
      {
         char name[13];
         sprintf(name, "NSPAD%03d.pcx", i - frames);
         for (int k = 0; k < 13; ++k)
            out.push_back(k < 12 ? (unsigned char)name[k] : 0);
      }
      offsetTables.push_back(out.size());
      const unsigned char* offsets = gp + 16 + 13 * frames;
      for (int i = 0; i < frames; ++i)
         nsPutDword(out, nsGetDword(offsets + 4 * i) + tableDelta);
      for (int i = frames; i < newFrames; ++i)
         nsPutDword(out, 0);
   }

   // Original frame data keeps its layout after the enlarged table, the blank frame goes last.
   out.insert(out.end(), p + tableEnd, p + in.size());
   unsigned int blankAt = out.size();
   nsPutBlankFrame(out, width, height);
   for (int i = have; i < count; ++i)
      nsSetDword(out, offsetTables[0] + 4 * i, blankAt);
   return true;
}

const NsPaddedSheet* nsPaddedSheetFor(const std::string& lowerName)
{
   for (std::size_t i = 0; i < sizeof(nsPaddedSheets) / sizeof(NsPaddedSheet); ++i)
      if (lowerName == nsPaddedSheets[i].name)
         return &nsPaddedSheets[i];
   return NULL;
}

void nsSetPending(void* lod, NsVirtualFile* file)
{
   nsPending.lod = lod;
   nsPending.file = file;
   nsPending.pos = 0;
   memset(nsPending.header, 0, sizeof(nsPending.header));
   strncpy((char*)nsPending.header, file->name.c_str(), 15);
   *(unsigned int*)(nsPending.header + 20) = file->data.size();
}

// Reads a file through the original lod functions, the lod object must have found it already.
bool nsReadOriginalFile(void* lod, const char* name, std::vector<unsigned char>& out)
{
   int entry = CALL_2(int, __thiscall, nsHookLoadFile->GetDefaultFunc(), lod, name);
   if (!entry)
      return false;
   unsigned int size = *(unsigned int*)(entry + 20);
   out.resize(size);
   if (size)
      CALL_3(int, __thiscall, nsHookReadFile->GetDefaultFunc(), lod, &out[0], size);
   return true;
}

int __stdcall nsSearchFileInLod(HiHook* h, void* lod, const char* name)
{
   std::string lower = nsLowerName(name);

   std::map<std::string, NsVirtualFile>::iterator it = nsVirtualFiles.find(lower);
   if (it != nsVirtualFiles.end())
   {
      if (nsLoadVirtualFile(it->second))
      {
         nsSetPending(lod, &it->second);
         return 1;
      }
   }

   // The exe returns the result in AL only.
   int found = CALL_2(int, __thiscall, h->GetDefaultFunc(), lod, name) & 0xFF;

   const NsPaddedSheet* sheet = found ? nsPaddedSheetFor(lower) : NULL;
   if (sheet)
   {
      std::string key = "padded:" + lower;
      NsVirtualFile* padded;
      it = nsVirtualFiles.find(key);
      if (it == nsVirtualFiles.end())
      {
         std::vector<unsigned char> original, result;
         if (!nsReadOriginalFile(lod, name, original) || !nsPadDef(original, sheet->frames, result))
         {
            nsLodLog("could not pad %s, spells above its frame count have no icon", name);
            return found;
         }
         // The original read moved the lod's file position, search again so the game's own
         // read would still work if it ever bypassed the pending buffer.
         CALL_2(int, __thiscall, h->GetDefaultFunc(), lod, name);
         padded = &nsVirtualFiles[key];
         padded->name = lower;
         padded->data = result;
         padded->loaded = true;
         nsLodLog("%s padded to %d frames", name, sheet->frames);
      }
      else
         padded = &it->second;

      nsSetPending(lod, padded);
      return 1;
   }

   if (nsPending.lod == lod)
      nsPending.file = NULL;
   return found;
}

int __stdcall nsLoadFileFromLod(HiHook* h, void* lod, const char* name)
{
   if (nsPending.file && nsPending.lod == lod && nsLowerName(name) == nsPending.file->name)
   {
      nsPending.pos = 0;
      return (int)nsPending.header;
   }
   return CALL_2(int, __thiscall, h->GetDefaultFunc(), lod, name);
}

int __stdcall nsReadFromLod(HiHook* h, void* lod, void* buffer, unsigned int size)
{
   if (nsPending.file && nsPending.lod == lod)
   {
      const std::vector<unsigned char>& data = nsPending.file->data;
      unsigned int left = nsPending.pos < data.size() ? data.size() - nsPending.pos : 0;
      unsigned int n = size < left ? size : left;
      if (n)
         memcpy(buffer, &data[nsPending.pos], n);
      if (n < size)
         memset((char*)buffer + n, 0, size - n);
      nsPending.pos += n;
      if (nsPending.pos >= data.size())
         nsPending.file = NULL;
      return 0;
   }
   return CALL_3(int, __thiscall, h->GetDefaultFunc(), lod, buffer, size);
}

void nsWriteVirtualLodHooks()
{
   nsHookSearchFile = _PI->WriteHiHook(0x4FB100, SPLICE_, EXTENDED_, THISCALL_, nsSearchFileInLod);
   nsHookLoadFile = _PI->WriteHiHook(0x4FACA0, SPLICE_, EXTENDED_, THISCALL_, nsLoadFileFromLod);
   nsHookReadFile = _PI->WriteHiHook(0x4FB1B0, SPLICE_, EXTENDED_, THISCALL_, nsReadFromLod);
}
