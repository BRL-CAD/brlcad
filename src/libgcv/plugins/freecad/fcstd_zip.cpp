#include "fcstd_zip.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

struct zip_entry {
    std::string name;
    std::uint16_t method = 0;
    std::uint32_t crc = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t uncompressed_size = 0;
    std::uint32_t local_header_offset = 0;
    std::vector<unsigned char> compressed;
    std::vector<unsigned char> uncompressed;
};

struct zip {
    std::string path;
    bool write_mode = false;
    std::map<std::string, zip_entry> entries;
};

struct zip_file {
    std::vector<unsigned char> data;
    std::size_t offset = 0;
};

struct zip_source {
    std::vector<unsigned char> data;
};

static std::uint16_t
read_le16(const unsigned char *p)
{
    return (std::uint16_t)p[0] | ((std::uint16_t)p[1] << 8);
}

static std::uint32_t
read_le32(const unsigned char *p)
{
    return (std::uint32_t)p[0] |
           ((std::uint32_t)p[1] << 8) |
           ((std::uint32_t)p[2] << 16) |
           ((std::uint32_t)p[3] << 24);
}

static void
append_le16(std::vector<unsigned char> &out, std::uint16_t v)
{
    out.push_back((unsigned char)(v & 0xff));
    out.push_back((unsigned char)((v >> 8) & 0xff));
}

static void
append_le32(std::vector<unsigned char> &out, std::uint32_t v)
{
    out.push_back((unsigned char)(v & 0xff));
    out.push_back((unsigned char)((v >> 8) & 0xff));
    out.push_back((unsigned char)((v >> 16) & 0xff));
    out.push_back((unsigned char)((v >> 24) & 0xff));
}

static bool
read_file(const std::string &path, std::vector<unsigned char> &data)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
	return false;
    in.seekg(0, std::ios::end);
    std::streamoff sz = in.tellg();
    if (sz < 0)
	return false;
    in.seekg(0, std::ios::beg);
    data.resize((std::size_t)sz);
    if (!data.empty())
	in.read((char *)data.data(), (std::streamsize)data.size());
    return in.good() || in.eof();
}

static bool
inflate_raw(const std::vector<unsigned char> &input, std::uint32_t expected_size, std::vector<unsigned char> &output)
{
    output.assign(expected_size, 0);
    unsigned char empty_output = 0;

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    zs.next_in = const_cast<Bytef *>(input.data());
    zs.avail_in = (uInt)input.size();
    zs.next_out = expected_size ? output.data() : &empty_output;
    zs.avail_out = expected_size ? (uInt)output.size() : 1;

    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
	return false;
    int ret = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return ret == Z_STREAM_END && zs.total_out == expected_size;
}

static bool
deflate_raw(const std::vector<unsigned char> &input, std::vector<unsigned char> &output)
{
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
	return false;

    uLong bound = deflateBound(&zs, (uLong)input.size());
    output.assign((std::size_t)bound, 0);
    zs.next_in = const_cast<Bytef *>(input.data());
    zs.avail_in = (uInt)input.size();
    zs.next_out = output.data();
    zs.avail_out = (uInt)output.size();

    int ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
	deflateEnd(&zs);
	return false;
    }
    output.resize((std::size_t)zs.total_out);
    deflateEnd(&zs);
    return true;
}

static bool
parse_zip(zip_t *archive)
{
    std::vector<unsigned char> file_data;
    if (!read_file(archive->path, file_data) || file_data.size() < 22)
	return false;

    std::size_t scan_start = file_data.size() > 66000 ? file_data.size() - 66000 : 0;
    std::size_t eocd = std::string::npos;
    for (std::size_t i = file_data.size() - 22; i + 1 > scan_start; --i) {
	if (read_le32(&file_data[i]) == 0x06054b50u) {
	    eocd = i;
	    break;
	}
	if (i == 0)
	    break;
    }
    if (eocd == std::string::npos)
	return false;

    std::uint16_t total_entries = read_le16(&file_data[eocd + 10]);
    std::uint32_t cd_size = read_le32(&file_data[eocd + 12]);
    std::uint32_t cd_offset = read_le32(&file_data[eocd + 16]);
    if ((std::uint64_t)cd_offset + cd_size > file_data.size())
	return false;

    std::size_t pos = cd_offset;
    for (std::uint16_t i = 0; i < total_entries; ++i) {
	if (pos + 46 > file_data.size() || read_le32(&file_data[pos]) != 0x02014b50u)
	    return false;

	std::uint16_t flags = read_le16(&file_data[pos + 8]);
	std::uint16_t method = read_le16(&file_data[pos + 10]);
	std::uint32_t crc = read_le32(&file_data[pos + 16]);
	std::uint32_t csize = read_le32(&file_data[pos + 20]);
	std::uint32_t usize = read_le32(&file_data[pos + 24]);
	std::uint16_t name_len = read_le16(&file_data[pos + 28]);
	std::uint16_t extra_len = read_le16(&file_data[pos + 30]);
	std::uint16_t comment_len = read_le16(&file_data[pos + 32]);
	std::uint32_t local_offset = read_le32(&file_data[pos + 42]);

	if ((flags & 1) || (method != 0 && method != 8))
	    return false;
	if (pos + 46u + name_len + extra_len + comment_len > file_data.size())
	    return false;

	std::string name((const char *)&file_data[pos + 46], name_len);
	if ((std::uint64_t)local_offset + 30 > file_data.size() ||
	    read_le32(&file_data[local_offset]) != 0x04034b50u)
	    return false;

	std::uint16_t lname_len = read_le16(&file_data[local_offset + 26]);
	std::uint16_t lextra_len = read_le16(&file_data[local_offset + 28]);
	std::uint64_t data_offset = (std::uint64_t)local_offset + 30 + lname_len + lextra_len;
	if (data_offset + csize > file_data.size())
	    return false;

	zip_entry e;
	e.name = name;
	e.method = method;
	e.crc = crc;
	e.compressed_size = csize;
	e.uncompressed_size = usize;
	e.local_header_offset = local_offset;
	e.compressed.assign(file_data.begin() + (std::ptrdiff_t)data_offset,
	    file_data.begin() + (std::ptrdiff_t)(data_offset + csize));

	if (method == 0) {
	    e.uncompressed = e.compressed;
	    if (e.uncompressed.size() != usize)
		return false;
	} else if (!inflate_raw(e.compressed, usize, e.uncompressed)) {
	    return false;
	}

	archive->entries[name] = std::move(e);
	pos += 46u + name_len + extra_len + comment_len;
    }

    return true;
}

extern "C" zip_t *
zip_open(const char *path, int flags, int *errorp)
{
    if (errorp)
	*errorp = 0;
    if (!path)
	return nullptr;

    zip_t *archive = new zip;
    archive->path = path;
    archive->write_mode = (flags & ZIP_CREATE) || (flags & ZIP_TRUNCATE);

    if (!archive->write_mode) {
	if (!parse_zip(archive)) {
	    delete archive;
	    if (errorp)
		*errorp = 1;
	    return nullptr;
	}
    }

    return archive;
}

extern "C" void
zip_discard(zip_t *archive)
{
    delete archive;
}

extern "C" zip_file_t *
zip_fopen(zip_t *archive, const char *fname, int)
{
    if (!archive || !fname)
	return nullptr;
    auto it = archive->entries.find(fname);
    if (it == archive->entries.end())
	return nullptr;

    zip_file_t *f = new zip_file;
    f->data = it->second.uncompressed;
    return f;
}

extern "C" zip_int64_t
zip_fread(zip_file_t *file, void *buf, zip_uint64_t nbytes)
{
    if (!file || !buf)
	return -1;
    std::size_t remaining = file->data.size() - file->offset;
    std::size_t n = std::min<std::size_t>((std::size_t)nbytes, remaining);
    if (n)
	std::memcpy(buf, file->data.data() + file->offset, n);
    file->offset += n;
    return (zip_int64_t)n;
}

extern "C" int
zip_fclose(zip_file_t *file)
{
    delete file;
    return 0;
}

extern "C" void
zip_stat_init(zip_stat_t *st)
{
    if (st)
	std::memset(st, 0, sizeof(*st));
}

extern "C" int
zip_stat(zip_t *archive, const char *fname, int, zip_stat_t *st)
{
    if (!archive || !fname || !st)
	return -1;
    auto it = archive->entries.find(fname);
    if (it == archive->entries.end())
	return -1;
    zip_stat_init(st);
    st->valid = ZIP_STAT_SIZE;
    st->size = it->second.uncompressed.size();
    return 0;
}

extern "C" zip_source_t *
zip_source_buffer(zip_t *, const void *data, zip_uint64_t len, int)
{
    if (!data && len)
	return nullptr;
    zip_source_t *src = new zip_source;
    const unsigned char *bytes = (const unsigned char *)data;
    src->data.assign(bytes, bytes + len);
    return src;
}

extern "C" void
zip_source_free(zip_source_t *source)
{
    delete source;
}

extern "C" zip_int64_t
zip_file_add(zip_t *archive, const char *name, zip_source_t *source, int)
{
    if (!archive || !archive->write_mode || !name || !source)
	return -1;

    zip_entry e;
    e.name = name;
    e.uncompressed = source->data;
    e.uncompressed_size = (std::uint32_t)e.uncompressed.size();
    e.crc = crc32(0L, Z_NULL, 0);
    if (!e.uncompressed.empty())
	e.crc = crc32(e.crc, e.uncompressed.data(), (uInt)e.uncompressed.size());

    std::vector<unsigned char> compressed;
    if (deflate_raw(e.uncompressed, compressed) && compressed.size() < e.uncompressed.size()) {
	e.method = 8;
	e.compressed = std::move(compressed);
    } else {
	e.method = 0;
	e.compressed = e.uncompressed;
    }
    e.compressed_size = (std::uint32_t)e.compressed.size();

    archive->entries[e.name] = std::move(e);
    delete source;
    return (zip_int64_t)archive->entries.size() - 1;
}

static bool
write_all(std::ofstream &out, const std::vector<unsigned char> &data)
{
    if (!data.empty())
	out.write((const char *)data.data(), (std::streamsize)data.size());
    return out.good();
}

extern "C" int
zip_close(zip_t *archive)
{
    if (!archive)
	return -1;
    if (!archive->write_mode) {
	delete archive;
	return 0;
    }

    std::vector<unsigned char> file_data;
    std::vector<unsigned char> central_dir;

    for (auto &kv : archive->entries) {
	zip_entry &e = kv.second;
	e.local_header_offset = (std::uint32_t)file_data.size();

	append_le32(file_data, 0x04034b50u);
	append_le16(file_data, 20);
	append_le16(file_data, 0x0800u);
	append_le16(file_data, e.method);
	append_le16(file_data, 0);
	append_le16(file_data, 0);
	append_le32(file_data, e.crc);
	append_le32(file_data, e.compressed_size);
	append_le32(file_data, e.uncompressed_size);
	append_le16(file_data, (std::uint16_t)e.name.size());
	append_le16(file_data, 0);
	file_data.insert(file_data.end(), e.name.begin(), e.name.end());
	file_data.insert(file_data.end(), e.compressed.begin(), e.compressed.end());

	append_le32(central_dir, 0x02014b50u);
	append_le16(central_dir, 20);
	append_le16(central_dir, 20);
	append_le16(central_dir, 0x0800u);
	append_le16(central_dir, e.method);
	append_le16(central_dir, 0);
	append_le16(central_dir, 0);
	append_le32(central_dir, e.crc);
	append_le32(central_dir, e.compressed_size);
	append_le32(central_dir, e.uncompressed_size);
	append_le16(central_dir, (std::uint16_t)e.name.size());
	append_le16(central_dir, 0);
	append_le16(central_dir, 0);
	append_le16(central_dir, 0);
	append_le16(central_dir, 0);
	append_le32(central_dir, 0);
	append_le32(central_dir, e.local_header_offset);
	central_dir.insert(central_dir.end(), e.name.begin(), e.name.end());
    }

    std::uint32_t cd_offset = (std::uint32_t)file_data.size();
    std::uint32_t cd_size = (std::uint32_t)central_dir.size();
    file_data.insert(file_data.end(), central_dir.begin(), central_dir.end());

    append_le32(file_data, 0x06054b50u);
    append_le16(file_data, 0);
    append_le16(file_data, 0);
    append_le16(file_data, (std::uint16_t)archive->entries.size());
    append_le16(file_data, (std::uint16_t)archive->entries.size());
    append_le32(file_data, cd_size);
    append_le32(file_data, cd_offset);
    append_le16(file_data, 0);

    std::ofstream out(archive->path, std::ios::binary | std::ios::trunc);
    bool ok = out && write_all(out, file_data);
    out.close();
    delete archive;
    return ok ? 0 : -1;
}
