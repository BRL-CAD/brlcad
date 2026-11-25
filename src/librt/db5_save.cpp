/*                     D B 5 _ S A V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "bio.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "db_history_db5.h"
#include "librt_private.h"

#if defined(HAVE_FSYNC) && defined(HAVE_MKSTEMP) && defined(HAVE_SYS_FILE_H) && \
    defined(HAVE_SYS_STAT_H) && defined(HAVE_LSTAT) && defined(HAVE_UNISTD_H)
#  include <sys/file.h>
#  include <sys/stat.h>
#  if defined(LOCK_EX) && defined(LOCK_NB)
#    define DB5_ATOMIC_REPLACEMENT 1
#  endif
#endif

struct db5_save {
#ifdef DB5_ATOMIC_REPLACEMENT
    std::string path;
    struct stat original = {};
    std::filesystem::file_time_type modified;
    const struct db_i *base = nullptr;
    bool installed = false;
#endif
};

static void
db5_save_message(struct bu_vls *message, const char *reason)
{
    if (!message)
	return;
    bu_vls_trunc(message, 0);
    bu_vls_strcat(message, reason);
}

#ifdef DB5_ATOMIC_REPLACEMENT
namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int descriptor) : fd(descriptor) {}
    ~FileDescriptor()
    {
	if (fd >= 0)
	    close(fd);
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    int fd;
};

class TemporarySnapshot {
public:
    explicit TemporarySnapshot(const std::string &directory) :
	path(directory + "/.brlcad-db5-XXXXXX")
    {
	FileDescriptor descriptor(mkstemp(path.data()));
	created = descriptor.fd >= 0;
	if (created) {
	    file = fdopen(descriptor.fd, "wb");
	    if (file)
		descriptor.fd = -1;
	}
    }
    ~TemporarySnapshot()
    {
	if (file)
	    fclose(file);
	if (created)
	    bu_file_delete(path.c_str());
    }
    TemporarySnapshot(const TemporarySnapshot &) = delete;
    TemporarySnapshot &operator=(const TemporarySnapshot &) = delete;

    bool finish(const struct stat &original)
    {
	if (fflush(file) != 0)
	    return false;
	const int fd = fileno(file);
	struct stat staged;
	if (fstat(fd, &staged) != 0)
	    return false;
	if ((staged.st_uid != original.st_uid || staged.st_gid != original.st_gid) &&
	    fchown(fd, original.st_uid, original.st_gid) != 0)
	    return false;
	constexpr mode_t permission_bits = S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX;
	if (bu_fchmod(fd, original.st_mode & permission_bits) != 0 || fsync(fd) != 0)
	    return false;

	FILE *finished = file;
	file = nullptr;
	return fclose(finished) == 0;
    }

    std::string path;
    FILE *file = nullptr;
    bool created = false;
};

bool
same_state(const struct stat &original, const struct stat &current)
{
    return S_ISREG(current.st_mode) && current.st_nlink == 1 &&
	current.st_dev == original.st_dev && current.st_ino == original.st_ino &&
	current.st_size == original.st_size && current.st_mode == original.st_mode &&
	current.st_uid == original.st_uid && current.st_gid == original.st_gid &&
	current.st_ctime == original.st_ctime && current.st_mtime == original.st_mtime;
}

bool
unchanged(const struct db5_save *save, int descriptor)
{
    struct stat path_state;
    if (lstat(save->path.c_str(), &path_state) != 0 || !same_state(save->original, path_state))
	return false;
    struct stat fd_state;
    if (descriptor >= 0 && (fstat(descriptor, &fd_state) != 0 || !same_state(save->original, fd_state)))
	return false;
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(save->path, error);
    return !error && modified == save->modified;
}

bool
write_snapshot(FILE *file, const struct db_i *snapshot, const db_history::Pack *pack, struct bu_vls *message)
{
    db_history::Manifest expected;
    if (pack && !db_history::selected_manifest(*pack, expected)) {
	db5_save_message(message, "History does not contain a usable selected model");
	return false;
    }
    if (db5_fwrite_header(file) < 0)
	return false;

    db_history::Manifest actual;
    bool global = false;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, snapshot) {
	if (!(dp->d_flags & RT_DIR_INMEM) && dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	struct bu_external record = BU_EXTERNAL_INIT_ZERO;
	if (db_get_external(&record, dp, snapshot) < 0)
	    return false;

	struct db5_raw_internal raw;
	const auto *end = db5_get_raw_internal_bounded(&raw, record.ext_buf, record.ext_nbytes);
	bool valid = end && raw.object_length == record.ext_nbytes && raw.h_name_present &&
	    raw.h_dli == DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT &&
	    BU_STR_EQUAL(dp->d_namep, reinterpret_cast<const char *>(raw.name.ext_buf));
	if (valid && BU_STR_EQUAL(dp->d_namep, DB5_GLOBAL_OBJECT_NAME)) {
	    valid = !global && raw.major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY;
	    global = valid;
	}
	bool written = false;
	if (valid && pack && BU_STR_EQUAL(dp->d_namep, db_history::archive_name)) {
	    db_history::ByteView body;
	    written = db_history::archive_payload(raw, body);
	    if (!written)
		db5_save_message(message, "A user object occupies the embedded history name");
	} else if (valid && (!pack || db_history::add_model_record(actual, dp->d_namep,
		    {record.ext_buf, record.ext_nbytes}))) {
	    written = bu_fwrite_external(file, &record) == 0;
	}
	bu_free_external(&record);
	if (!written)
	    return false;
    } FOR_ALL_DIRECTORY_END;
    if (!global)
	return false;
    if (pack && expected != actual) {
	db5_save_message(message, "The selected archive model differs from the DB5 snapshot");
	return false;
    }
    return !pack || db_history::write_embedded_pack(file, *pack);
}

} // namespace
#endif

struct db5_save *
db5_save_begin(const struct db_i *base, struct bu_vls *message)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_REPLACEMENT
    if (!base || !base->dbi_filename || db_version(base) != 5 || !base->i->dbi_fp ||
	base->i->dbi_mf || !base->i->dbi_dir_built) {
	db5_save_message(message, "A built DB5 database opened for writing is required");
	return nullptr;
    }

    auto save = std::make_unique<struct db5_save>();
    save->base = base;
    char *path = bu_file_realpath(base->dbi_filename, nullptr);
    if (path) {
	save->path = path;
	bu_free(path, "DB5 save path");
    }
    const int fd = fileno(base->i->dbi_fp);
    std::error_code error;
    save->modified = std::filesystem::last_write_time(save->path, error);
    if (error || fstat(fd, &save->original) != 0 || !unchanged(save.get(), fd)) {
	db5_save_message(message, "Could not identify an unchanged single-link DB5 file");
	return nullptr;
    }
    return save.release();
#else
    (void)base;
    db5_save_message(message, "Atomic DB5 replacement is unavailable on this platform");
    return nullptr;
#endif
}

static enum db5_save_result
replace_snapshot(struct db5_save *save, const struct db_i *snapshot, const db_history::Pack *pack,
		 struct db_i **replacement, struct bu_vls *message)
{
    db5_save_message(message, "");
    if (replacement)
	*replacement = DBI_NULL;
#ifdef DB5_ATOMIC_REPLACEMENT
    if (!save || save->installed || !snapshot || db_version(snapshot) != 5 || !replacement) {
	db5_save_message(message, "Invalid DB5 save or snapshot");
	return DB5_SAVE_ERROR;
    }
    if (!unchanged(save, -1)) {
	db5_save_message(message, "DB5 file changed since staging began");
	return DB5_SAVE_STALE;
    }
    if (pack) {
	const auto existing = db_history::inspect_db5_history(save->base);
	if (existing == db_history::EmbeddedStatus::Collision) {
	    db5_save_message(message, "A user object occupies the embedded history name");
	    return DB5_SAVE_ERROR;
	}
	if (existing != db_history::EmbeddedStatus::Current && existing != db_history::EmbeddedStatus::Missing) {
	    db5_save_message(message, "Cannot replace unreadable or unsupported embedded history");
	    return DB5_SAVE_ERROR;
	}
    }

    char *parent = bu_path_dirname(save->path.c_str());
    const std::string directory(parent);
    bu_free(parent, "DB5 save directory");

    TemporarySnapshot temporary(directory);
    // The write must be complete and the new database usable before
    // replacing the only directory entry for the original file.
    if (!temporary.file || !write_snapshot(temporary.file, snapshot, pack, message) ||
	!temporary.finish(save->original)) {
	if (message && bu_vls_strlen(message) == 0)
	    db5_save_message(message, "Could not write and synchronize the DB5 snapshot");
	return DB5_SAVE_ERROR;
    }

    std::unique_ptr<struct db_i, decltype(&db_close)> staged(
	db_open(temporary.path.c_str(), DB_OPEN_READWRITE), db_close);
    if (!staged || staged->dbi_read_only) {
	db5_save_message(message, "Could not reopen the staged DB5 snapshot");
	return DB5_SAVE_ERROR;
    }
    staged->i->dbi_use_comb_instance_ids = db_uses_instance_specifiers(snapshot);
    if (db_dirbuild(staged.get()) != 0) {
	db5_save_message(message, "Could not reopen the staged DB5 snapshot");
	return DB5_SAVE_ERROR;
    }

    // Cooperating writers lock the inode then verify the directory entry
    // still names it. Each save opens its own descriptor for this lock.
    FileDescriptor existing(open(save->path.c_str(), O_RDONLY));
    if (existing.fd < 0 || flock(existing.fd, LOCK_EX | LOCK_NB) != 0) {
	db5_save_message(message, "Could not lock the destination DB5 file");
	return DB5_SAVE_ERROR;
    }
    if (!unchanged(save, existing.fd)) {
	db5_save_message(message, "DB5 file changed during save");
	return DB5_SAVE_STALE;
    }
    FileDescriptor parent_fd(open(directory.c_str(), O_RDONLY));
    if (parent_fd.fd < 0) {
	db5_save_message(message, "Could not open the database directory for synchronization");
	return DB5_SAVE_ERROR;
    }

    bu_free(staged->dbi_filename, "staged filename");
    staged->dbi_filename = bu_strdup(save->path.c_str());
    std::error_code error;
    std::filesystem::rename(temporary.path, save->path, error);
    if (error) {
	db5_save_message(message, "Could not replace the DB5 file");
	return DB5_SAVE_ERROR;
    }

    temporary.created = false;
    save->installed = true;
    *replacement = staged.release();
    if (fsync(parent_fd.fd) != 0) {
	db5_save_message(message, "DB5 file replaced but directory synchronization failed");
	return DB5_SAVE_SYNC_FAILED;
    }
    return DB5_SAVE_OK;
#else
    (void)save;
    (void)snapshot;
    (void)pack;
    db5_save_message(message, "Atomic DB5 replacement is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

enum db5_save_result
db5_save_replace(struct db5_save *save, const struct db_i *snapshot,
		 struct db_i **replacement, struct bu_vls *message)
{
    return replace_snapshot(save, snapshot, nullptr, replacement, message);
}

enum db5_save_result
db_history::save_db5_history(struct db5_save *save, const struct db_i *snapshot, const Pack &pack,
			    struct db_i **replacement, struct bu_vls *message)
{
    return replace_snapshot(save, snapshot, &pack, replacement, message);
}

void
db5_save_free(struct db5_save *save)
{
    delete save;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
