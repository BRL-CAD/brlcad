/*                   T E S T _ P E R M . C P P
 * BRL-CAD
 *
 * Published in 2024 by the United States Government.
 * This work is in the public domain.
 *
 */

#include "common.h"

#include <cstring>
#include <iostream>

#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"


static bool
write_payload(FILE *fp, const char *payload)
{
    size_t payload_len = 0;

    if (!fp || !payload) {
	return false;
    }

    payload_len = std::strlen(payload);

    clearerr(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
	return false;
    }
    if (fwrite(payload, 1, payload_len, fp) != payload_len) {
	return false;
    }
    if (fflush(fp) != 0) {
	return false;
    }

    return !ferror(fp);
}


static bool
payload_matches(FILE *fp, const char *payload)
{
    char buffer[64] = {0};
    size_t payload_len = 0;

    if (!fp || !payload) {
	return false;
    }

    payload_len = std::strlen(payload);
    if (payload_len >= sizeof(buffer)) {
	return false;
    }

    clearerr(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
	return false;
    }
    if (fread(buffer, 1, payload_len, fp) != payload_len) {
	return false;
    }

    return !std::memcmp(buffer, payload, payload_len);
}


int
main(int argc, char **argv)
{
    int failures = 0;
    FILE *fp = NULL;
    int fd = -1;
    char filepath[MAXPATHLEN] = {0};
    const char *payload = "bu_fchmod validation";
    static const unsigned long modes[] = {0600, 0640, 0660};

    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    if (argc != 1) {
	std::cerr << "Usage: " << argv[0] << "\n";
	return 1;
    }

    fp = bu_temp_file(filepath, MAXPATHLEN);
    if (!fp) {
	std::cerr << "Unable to create a temporary file for bu_fchmod testing\n";
	return 1;
    }

    fd = fileno(fp);
    if (fd < 0) {
	std::cerr << "Unable to obtain a file descriptor for bu_fchmod testing\n";
	fclose(fp);
	return 1;
    }

    if (bu_fchmod(-1, 0600) == 0) {
	std::cerr << "bu_fchmod should fail for an invalid file descriptor\n";
	failures++;
    }

    if (!write_payload(fp, payload) || !payload_matches(fp, payload)) {
	std::cerr << "Unable to establish a readable/writable temp file baseline for bu_fchmod testing\n";
	failures++;
    }

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
	if (bu_fchmod(fileno(fp), modes[i]) != 0) {
	    std::cerr << "bu_fchmod failed for mode 0" << std::oct << modes[i] << std::dec << "\n";
	    failures++;
	    continue;
	}

	if (!bu_file_exists(filepath, NULL)) {
	    std::cerr << "Temp file disappeared after bu_fchmod for mode 0" << std::oct << modes[i] << std::dec << "\n";
	    failures++;
	    continue;
	}
	if (!payload_matches(fp, payload)) {
	    std::cerr << "Temp file contents were not preserved after bu_fchmod for mode 0"
		      << std::oct << modes[i] << std::dec << "\n";
	    failures++;
	}
    }

    fclose(fp);
    if (bu_fchmod(fd, 0600) == 0) {
	std::cerr << "bu_fchmod should fail for a closed file descriptor\n";
	failures++;
    }
    bu_file_delete(filepath);

    return failures ? 1 : 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
