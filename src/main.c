#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

/* ─── Constants ────────────────────────────────────────────────────── */

#define MAX_FILES       32
#define MAX_SIZE_MB     (200LL * 1024 * 1024)
#define HEADER_NUM_LEN  10          /* Fixed-width 10-digit header size field */
#define MAX_FNAME       256
#define BUF_SIZE        4096

static const char *USAGE_MESSAGE =
    "Kullanim:\n"
    "\ttarsau -b [girdi_dosyasi...] -o [cikti_dosyasi]\n"
    "\ttarsau -a [arsiv_dosyasi] [cikti_dizini]\n";

/* ─── Helper: create directory tree (like mkdir -p) ────────────────── */

static int mkdirs(const char *path)
{
    char tmp[BUF_SIZE];
    char *p;

    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* ─── -b : Build archive ────────────────────────────────────────────── */

static int build_archive(int argc, char *argv[])
{
    /* --- Parse arguments --- */
    int    file_count      = 0;
    int    file_start      = 2;        /* index of first input file in argv */
    char  *output_file     = NULL;
    struct stat file_stats[MAX_FILES];
    long long total_size   = 0;
    int    o_flag_idx      = -1;       /* index of "-o" in argv */

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            o_flag_idx = i;
            break;
        }

        /* Count and stat input file */
        file_count++;
        if (file_count > MAX_FILES) {
            fprintf(stdout, "Hata: Cok fazla girdi dosyasi belirtildi (maksimum %d).\n", MAX_FILES);
            return EXIT_FAILURE;
        }

        struct stat *st = &file_stats[file_count - 1];
        errno = 0;
        if (stat(argv[i], st) == -1) {
            fprintf(stderr, "Hata: '%s' dosyasina erisim saglanamadi: %s.\n", argv[i], strerror(errno));
            return EXIT_FAILURE;
        }
        /* Reject non-regular files */
        if (!S_ISREG(st->st_mode)) {
            fprintf(stderr, "'%s' girdi dosyasinin formati uyumsuzdur!\n", argv[i]);
            return EXIT_FAILURE;
        }
        total_size += st->st_size;
    }

    if (file_count == 0) {
        fprintf(stdout, "Hata: Girdi dosyasi belirtilmedi.\n");
        return EXIT_FAILURE;
    }
    if (total_size > MAX_SIZE_MB) {
        fprintf(stdout, "Hata: Toplam girdi dosyasi boyutu 200MB'i asiyor.\n");
        return EXIT_FAILURE;
    }

    /* Determine output file name */
    if (o_flag_idx != -1) {
        int name_idx = o_flag_idx + 1;
        if (name_idx >= argc || strcmp(argv[name_idx], "") == 0) {
            output_file = "a.sau";
        } else {
            output_file = argv[name_idx];
        }
        /* Anything after the output name is unexpected */
        if (name_idx + 1 < argc) {
            fprintf(stdout, "Hata: Cok fazla arguman belirtildi.\n");
            return EXIT_FAILURE;
        }
    } else {
        output_file = "a.sau";
    }

    /* --- Open output file --- */
    errno = 0;
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Hata: '%s' cikti dosyasi acilirken hata olustu: %s\n",
                output_file, strerror(errno));
        return EXIT_FAILURE;
    }

    /* --- Write organization section --- */
    /* First 10 bytes: placeholder for header_size */
    uint32_t header_size = 0;
    int wb;

    errno = 0;
    if ((wb = fprintf(out, "%0*" PRIu32 "|", HEADER_NUM_LEN, header_size)) < 0) {
        fprintf(stderr, "Hata: Cikti dosyasina yazilirken hata olustu: %s\n", strerror(errno));
        fclose(out); remove(output_file);
        return EXIT_FAILURE;
    }
    /* The '|' after the 10-digit number belongs to header content */
    header_size += (uint32_t)(wb - HEADER_NUM_LEN);

    /* Write one record per file: filename\0,permissions,size| */
    for (int i = 0; i < file_count; i++) {
        char *path = argv[file_start + i];
        struct stat *st = &file_stats[i];

        /* Store only the base name */
        char *base = strrchr(path, '/');
        const char *name = (base != NULL) ? base + 1 : path;

        errno = 0;
        if ((wb = fprintf(out, "%s%c,%04o,%lld|",
                          name, '\0',
                          (unsigned)(st->st_mode & 07777),
                          (long long)st->st_size)) < 0) {
            fprintf(stderr, "Hata: Cikti dosyasina yazilirken hata olustu: %s\n", strerror(errno));
            fclose(out); remove(output_file);
            return EXIT_FAILURE;
        }
        header_size += (uint32_t)wb;
    }

    /* --- Write file contents (ASCII validation) --- */
    for (int i = 0; i < file_count; i++) {
        char *path = argv[file_start + i];

        errno = 0;
        FILE *in = fopen(path, "rb");
        if (!in) {
            fprintf(stderr, "Hata: '%s' girdi dosyasi okunamadi: %s\n", path, strerror(errno));
            fclose(out); remove(output_file);
            return EXIT_FAILURE;
        }

        int c;
        while ((c = fgetc(in)) != EOF) {
            if (c < 0 || c > 127) {
                /* Base name for the error message */
                char *base = strrchr(path, '/');
                const char *name = (base != NULL) ? base + 1 : path;
                fprintf(stderr, "'%s' girdi dosyasinin formati uyumsuzdur!\n", name);
                fclose(in); fclose(out); remove(output_file);
                return EXIT_FAILURE;
            }
            errno = 0;
            if (fputc(c, out) == EOF) {
                fprintf(stderr, "Hata: Cikti dosyasina yazilirken hata olustu: %s\n", strerror(errno));
                fclose(in); fclose(out); remove(output_file);
                return EXIT_FAILURE;
            }
        }

        if (ferror(in)) {
            fprintf(stderr, "Hata: '%s' okunurken bilinmeyen bir hata olustu.\n", path);
            fclose(in); fclose(out); remove(output_file);
            return EXIT_FAILURE;
        }
        fclose(in);
    }

    /* --- Go back and write the real header size --- */
    errno = 0;
    if (fseek(out, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Hata: Cikti dosyasina yazilirken hata olustu: %s\n", strerror(errno));
        fclose(out); remove(output_file);
        return EXIT_FAILURE;
    }
    errno = 0;
    if (fprintf(out, "%0*" PRIu32, HEADER_NUM_LEN, header_size) < 0) {
        fprintf(stderr, "Hata: Cikti dosyasina yazilirken hata olustu: %s\n", strerror(errno));
        fclose(out); remove(output_file);
        return EXIT_FAILURE;
    }

    fclose(out);
    fprintf(stdout, "'%s' arsivi %d dosyayla basariyla olusturuldu.\n",
            output_file, file_count);
    return EXIT_SUCCESS;
}

/* ─── -a : Extract archive ─────────────────────────────────────────── */

typedef struct {
    char      name[MAX_FNAME];
    mode_t    perm;
    long long size;
} FileRecord;

static int extract_archive(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stdout, "%s", USAGE_MESSAGE);
        return EXIT_FAILURE;
    }
    if (argc > 4) {
        fprintf(stdout, "Hata: '-a' secenegi icin cok fazla arguman belirtildi.\n");
        return EXIT_FAILURE;
    }

    const char *archive = argv[2];
    const char *outdir  = (argc == 4) ? argv[3] : ".";

    /* --- Open archive --- */
    errno = 0;
    FILE *arc = fopen(archive, "rb");
    if (!arc) {
        fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        return EXIT_FAILURE;
    }

    /* --- Read the 10-byte header size field --- */
    char num_buf[HEADER_NUM_LEN + 1];
    if (fread(num_buf, 1, HEADER_NUM_LEN, arc) != (size_t)HEADER_NUM_LEN) {
        fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(arc);
        return EXIT_FAILURE;
    }
    num_buf[HEADER_NUM_LEN] = '\0';

    /* Validate: must be all ASCII digits */
    for (int i = 0; i < HEADER_NUM_LEN; i++) {
        if (num_buf[i] < '0' || num_buf[i] > '9') {
            fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            fclose(arc);
            return EXIT_FAILURE;
        }
    }

    uint32_t header_size = (uint32_t)atol(num_buf);
    if (header_size == 0) {
        fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(arc);
        return EXIT_FAILURE;
    }

    /* --- Read organization section into a buffer --- */
    char *hbuf = malloc(header_size + 1);
    if (!hbuf) {
        fprintf(stderr, "Hata: Bellek tahsisi basarisiz.\n");
        fclose(arc);
        return EXIT_FAILURE;
    }
    if (fread(hbuf, 1, header_size, arc) != header_size) {
        fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(hbuf); fclose(arc);
        return EXIT_FAILURE;
    }
    hbuf[header_size] = '\0'; /* sentinel (may not align with real nulls) */

    /* --- Parse records ------------------------------------------------
     * Layout of org section (header_size bytes):
     *   |name\0,PPPP,SSSSS|name\0,PPPP,SSSSS| ...
     *  where PPPP = 4-digit octal permissions, SSSSS = decimal file size
     * ----------------------------------------------------------------- */
    FileRecord records[MAX_FILES];
    int file_count = 0;

    char *pos = hbuf;
    char *end = hbuf + header_size;

    /* Skip the leading '|' */
    if (pos < end && *pos == '|')
        pos++;

    while (pos < end && file_count < MAX_FILES) {
        /* --- Filename (terminated by '\0') --- */
        char *name_start = pos;
        while (pos < end && *pos != '\0')
            pos++;
        if (pos >= end) break;          /* truncated */

        size_t nlen = (size_t)(pos - name_start);
        if (nlen == 0 || nlen >= MAX_FNAME) {
            fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(hbuf); fclose(arc);
            return EXIT_FAILURE;
        }
        memcpy(records[file_count].name, name_start, nlen);
        records[file_count].name[nlen] = '\0';
        pos++;                          /* skip '\0' */

        /* --- ',' separator --- */
        if (pos >= end || *pos != ',') break;
        pos++;

        /* --- Permissions: 4 octal digits --- */
        if (pos + 4 > end) break;
        char perm_buf[5];
        memcpy(perm_buf, pos, 4);
        perm_buf[4] = '\0';
        records[file_count].perm = (mode_t)strtol(perm_buf, NULL, 8);
        pos += 4;

        /* --- ',' separator --- */
        if (pos >= end || *pos != ',') break;
        pos++;

        /* --- File size (decimal, until '|') --- */
        char *size_start = pos;
        while (pos < end && *pos != '|')
            pos++;
        if (pos >= end) break;

        char size_str[32];
        size_t slen = (size_t)(pos - size_start);
        if (slen == 0 || slen >= sizeof(size_str)) break;
        memcpy(size_str, size_start, slen);
        size_str[slen] = '\0';
        records[file_count].size = atoll(size_str);
        pos++;                          /* skip '|' */

        file_count++;
    }
    free(hbuf);

    if (file_count == 0) {
        fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(arc);
        return EXIT_FAILURE;
    }

    /* --- Create output directory if it doesn't exist --- */
    if (strcmp(outdir, ".") != 0) {
        errno = 0;
        if (mkdirs(outdir) != 0) {
            fprintf(stderr, "Hata: '%s' dizini olusturulamadi: %s\n",
                    outdir, strerror(errno));
            fclose(arc);
            return EXIT_FAILURE;
        }
    }

    /* --- Extract each file --- */
    char outpath[BUF_SIZE];
    char copybuf[BUF_SIZE];

    for (int i = 0; i < file_count; i++) {
        snprintf(outpath, sizeof(outpath), "%s/%s", outdir, records[i].name);

        errno = 0;
        FILE *out = fopen(outpath, "wb");
        if (!out) {
            fprintf(stderr, "Hata: '%s' dosyasi olusturulamadi: %s\n",
                    outpath, strerror(errno));
            fclose(arc);
            return EXIT_FAILURE;
        }

        long long remaining = records[i].size;
        while (remaining > 0) {
            size_t to_read = (remaining < (long long)BUF_SIZE)
                             ? (size_t)remaining
                             : (size_t)BUF_SIZE;
            size_t got = fread(copybuf, 1, to_read, arc);
            if (got == 0) {
                fprintf(stdout, "Arsiv dosyasi uygunsuz veya bozuk!\n");
                fclose(out); fclose(arc);
                return EXIT_FAILURE;
            }
            if (fwrite(copybuf, 1, got, out) != got) {
                fprintf(stderr, "Hata: '%s' dosyasina yazilirken hata olustu.\n", outpath);
                fclose(out); fclose(arc);
                return EXIT_FAILURE;
            }
            remaining -= (long long)got;
        }

        fclose(out);

        /* Restore original permissions */
        if (chmod(outpath, records[i].perm) != 0) {
            fprintf(stderr, "Uyari: '%s' izinleri ayarlanamadi: %s\n",
                    outpath, strerror(errno));
            /* Non-fatal: keep going */
        }

        fprintf(stdout, "  -> %s\n", outpath);
    }

    fclose(arc);
    fprintf(stdout, "'%s' arsivindeki %d dosya '%s' dizinine basariyla acildi.\n",
            archive, file_count, outdir);
    return EXIT_SUCCESS;
}

/* ─── main ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stdout, "%s", USAGE_MESSAGE);
        return EXIT_FAILURE;
    }

    const char *op = argv[1];

    if (strcmp(op, "-b") == 0)
        return build_archive(argc, argv);

    if (strcmp(op, "-a") == 0)
        return extract_archive(argc, argv);

    fprintf(stdout, "%s", USAGE_MESSAGE);
    return EXIT_FAILURE;
}