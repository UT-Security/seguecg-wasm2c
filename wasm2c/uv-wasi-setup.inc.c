#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasm-rt.h"

#include "uvwasi.h"
#include <limits.h> /* PATH_MAX */
#include <dirent.h>
#include <sys/stat.h>


static void addPreopenRootDir(uvwasi_preopen_t* preopens, const char* folder, long* curr_preopens_index, long total_preopens)
{
    if (*curr_preopens_index < total_preopens) {

        char* fullPath = malloc(1 + strlen(folder) + 1);
        strcpy(fullPath, "/");
        strcat(fullPath, folder);

        DIR* openable = opendir(fullPath);
        if (!openable) {
            free(fullPath);
            return;
        }
        closedir(openable);

        preopens[*curr_preopens_index].mapped_path = fullPath;
        preopens[*curr_preopens_index].real_path = fullPath;
        *curr_preopens_index = (*curr_preopens_index) + 1;
    } else {
        printf("Warning: Out of preopen slots. Increase the allocation\n");
    }
}

void init_uvwasi_local(uvwasi_t * local_uvwasi_state, bool mapRootSubdirs, int argc, char const * argv[])
{
    uvwasi_options_t init_options;

    //pass in standard descriptors
    init_options.in = 0;
    init_options.out = 1;
    init_options.err = 2;
    init_options.fd_table_size = 3;

    //pass in args and environement
    extern const char ** environ;
    init_options.argc = argc;
    init_options.argv = argv;
    init_options.envp = (const char **) environ;

    // we want to just give access to / --- no sandboxing enforced, binary has access to everything user does
    // sadly mapping / doesn't really work because of the way wasi is designed
    // so map each sub folder of root

    struct dirent *dent;
    DIR *srcdir = opendir("/");
    if (srcdir == NULL){
        printf("Root directory cannot be opened for wasi mapping!\n");
        abort();
    }

    const long total_preopens = 200;
    init_options.preopens = calloc(total_preopens, sizeof(uvwasi_preopen_t));

    // Doesn't really work, but map / anyway
    init_options.preopens[0].mapped_path = "/";
    init_options.preopens[0].real_path = "/";
    // Map curr dir
    init_options.preopens[1].mapped_path = "./";
    init_options.preopens[1].real_path = ".";

    long curr_preopens_index = 2;

    if (mapRootSubdirs) {
        while ((dent = readdir(srcdir)) != NULL){
            struct stat st;

            if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
                continue;
            }

            if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) < 0) {
                printf("Warning: could not fstat on: %s\n", dent->d_name);
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                //printf("%s\n", dent->d_name);
                addPreopenRootDir(init_options.preopens, dent->d_name, &curr_preopens_index, total_preopens);
            }
        }
        closedir(srcdir);
    }

    init_options.preopenc = curr_preopens_index;
    init_options.allocator = NULL;

    uvwasi_errno_t ret = uvwasi_init(local_uvwasi_state, &init_options);

    if (ret != UVWASI_ESUCCESS) {
        printf("uvwasi_init failed with error %d\n", ret);
        exit(1);
    }
}
