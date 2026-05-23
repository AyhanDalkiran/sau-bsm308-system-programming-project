#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <inttypes.h>

static const char* USAGE_MESSAGE =
    "Usage:\n"
    "\ttarsau -b [input_file...] -o [output_file]\n"
    "\ttarsau -a [archive_file] [output_dir]\n";

int main(int argc, char* argv[]) {
    if (argc == 0) {
        fprintf(stdout, "%s", USAGE_MESSAGE);
        return EXIT_FAILURE;
    }

    char* operation = argv[1];
    if (operation == NULL) {
        fprintf(stdout, "%s", USAGE_MESSAGE);
        return EXIT_FAILURE;
    }

    if (strcmp(operation, "-b") == 0) { // Build archive
        int input_file_count = 0;
        int input_file_start = 2;
        int arg_i;
        char* output_file = NULL;
        struct stat file_stats[32];
        long long input_file_size = 0;

        for (arg_i = 2; arg_i < argc; arg_i++) {
            char* arg = argv[arg_i];

            // All input files are specified
            if (strcmp(arg, "-o") == 0) break;

            // Process input file
            input_file_count++;
            if (input_file_count > 32) {
                fprintf(stdout, "Error: Too many input files specified (max 32).\n");
                return EXIT_FAILURE;
            }

            // Get file stats for size and permissions
            struct stat* file_stat = &file_stats[input_file_count - 1];
            errno = 0;
            if (stat(arg, file_stat) == -1) {
                fprintf(stderr, "Error: Could not access input file '%s': %s.\n", arg, strerror(errno));
                return EXIT_FAILURE;
            }
            input_file_size += file_stat->st_size;
        }

        // Additional case checks
        if (input_file_count == 0) {
            fprintf(stdout, "Error: No input files specified.\n");
            return EXIT_FAILURE;
        }
        if (input_file_size > (200 * 1024 * 1024)) {
            fprintf(stdout, "Error: Total input file size exceeds 200MB.\n");
            return EXIT_FAILURE;
        }

        // If -o flag not specified
        if (argv[arg_i] == NULL) output_file = "a.sau";

        // If -o flag specified but no file name provided
        arg_i++;
        output_file = argv[arg_i];
        if (output_file == NULL) output_file = "a.sau";

        arg_i++; // Move past output file
        if (arg_i < argc) {
            fprintf(stdout, "Error: Too many arguments specified.\n");
            return EXIT_FAILURE;
        }

        // Open output file for writing
        errno = 0;
        FILE* out_fp = fopen(output_file, "wb");
        if (out_fp == NULL) {
            fprintf(stderr, "Error: Could not open output file '%s' for writing: %s\n", output_file, strerror(errno));
            return EXIT_FAILURE;
        }

        // Write the header
        uint32_t header_size = 0;
        int written_bytes = 0;

        errno = 0;
        if ((written_bytes = fprintf(out_fp, "%010" PRIu32 "|", header_size)) < 0) { // Placeholder for header size
            fprintf(stderr, "Error: An error occurred while writing the output file: %s\n", strerror(errno));
            fclose(out_fp);
            remove(output_file); // Clean up partial output file
            return EXIT_FAILURE;
        }
        header_size += written_bytes - 10; // Subtract the placeholder size
        
        for (int i = 0; i < input_file_count; i++) {
            char* file_path = argv[input_file_start + i];
            struct stat* file_stat = &file_stats[i];

            // Get only the file name from the path
            char* last_slash = strrchr(file_path, '/');
            if (last_slash != NULL) file_path = last_slash + 1; // Move past the last slash

            errno = 0;
            if ((written_bytes = fprintf(out_fp, "%s%c,%04o,%lld|", file_path, '\0', file_stat->st_mode, (long long)file_stat->st_size)) < 0) {
                fprintf(stderr, "Error: An error occurred while writing to stdout: %s\n", strerror(errno));
                fclose(out_fp);
                remove(output_file); // Clean up partial output file
                return EXIT_FAILURE;
            }
            header_size += written_bytes;
        }

        // Write the file data
        for (int i = 0; i < input_file_count; i++) {
            char* file_path = argv[input_file_start + i];

            // Open input file for reading
            errno = 0;
            FILE* in_fp = fopen(file_path, "rb");
            if (in_fp == NULL) {
                fprintf(stderr, "Error: Could not open input file '%s' for reading: %s\n", file_path, strerror(errno));
                fclose(out_fp);
                remove(output_file); // Clean up partial output file
                return EXIT_FAILURE;
            }

            int c;
            while ((c = fgetc(in_fp)) != EOF) {
                // ASCII check
                if (c < 0 || c > 127) {
                    fprintf(stderr, "Error: Input file '%s' contains non-ASCII characters.\n", file_path);
                    fclose(in_fp);
                    fclose(out_fp);
                    remove(output_file); // Clean up partial output file
                    return EXIT_FAILURE;
                }
                
                errno = 0;
                if (fputc(c, out_fp) == EOF) {
                    fprintf(stderr, "Error: An error occurred while writing the output file: %s\n", strerror(errno));
                    fclose(in_fp);
                    fclose(out_fp);
                    remove(output_file); // Clean up partial output file
                    return EXIT_FAILURE;
                }
            }

            if (ferror(in_fp)) {
                fprintf(stderr, "Error: An unknown error occurred while reading input file '%s'.\n", file_path);
                fclose(in_fp);
                fclose(out_fp);
                remove(output_file); // Clean up partial output file
                return EXIT_FAILURE;
            }
        }
        
        // Seek to beginnig of file to write the actual header size
        errno = 0;
        if (fseek(out_fp, 0, SEEK_SET) != 0) {
            fprintf(stderr, "Error: An error occurred while writing the output file: %s\n", strerror(errno));
            fclose(out_fp);
            remove(output_file); // Clean up partial output file
            return EXIT_FAILURE;
        }

        // Update header size at the beginning of the file
        errno = 0;
        if (fprintf(out_fp, "%010" PRIu32, header_size) < 0) {
            fprintf(stderr, "Error: An error occurred while writing the output file: %s\n", strerror(errno));
            fclose(out_fp);
            remove(output_file); // Clean up partial output file
            return EXIT_FAILURE;
        }

        fclose(out_fp);
        fprintf(stdout, "Archive '%s' created successfully with %d file(s).\n", output_file, input_file_count);

        return EXIT_SUCCESS;
    }
    else if (strcmp(operation, "-a") == 0) { // Extract archive
        //TODO: Implement archive extraction
    }
    else {
        fprintf(stdout, "%s", USAGE_MESSAGE);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}