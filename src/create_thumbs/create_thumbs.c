#include "ceos.h"
#include "asf_meta.h"
#include "asf_raster.h"
#include "asf_license.h"
#include "create_thumbs_help.h"

#ifdef linux
#include <unistd.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "asf.h"
#include "get_ceos_names.h"
#include "asf_endian.h"
#include "asf_import.h"
#include "float_image.h"
#include "asf_nan.h"

#define MIN_ARGS (1)
#define MAX_ARGS (30)

typedef enum {
    not_L0=0,
    stf,
    ceos
} level_0_flag;

typedef enum output_format_types {
    unsupported_format=0,
    jpeg,
    tiff
} output_format_types;

int checkForOption(char* key, int argc, char* argv[]); // in help.c
char *spaces(int n);
int strmatches(const char *key, ...);
int has_prepension(const char * data_file_name);
void process(const char *what, int top, int recursive, int size, int verbose,
             level_0_flag L0Flag, float scale_factor, int browseFlag,
             char *out_dir);
void process_dir(const char *dir, int top, int recursive, int size, int verbose,
                 level_0_flag L0Flag, float scale_factor, int browseFlag,
                 char *out_dir);
void process_file(const char *file, int level, int size, int verbose, char *out_dir);
char *meta_file_name(const char * data_file_name);
meta_parameters * silent_meta_create(const char *filename);
int generate_ceos_thumbnail(const char *input_data, int size, char *out_dir);

int main(int argc, char *argv[])
{
  output_format_types output_format=jpeg;
  level_0_flag L0Flag=not_L0;
  int verbose = 0;
  const int default_thumbnail_size = 256;
  char *out_dir = NULL;
  int scaleFlag=0;
  float scale_factor=-1.0;
  int browseFlag=0;

  quietflag = (checkForOption("-quiet", argc, argv)  ||
               checkForOption("--quiet", argc, argv) ||
               checkForOption("-q", argc, argv));
  if (argc > 1 && !quietflag) {
      check_for_help(argc, argv);
      handle_license_and_version_args(argc, argv, TOOL_NAME);
  }
  if (argc <= MIN_ARGS || argc > MAX_ARGS) {
      if (!quietflag) usage();
      exit(1);
  }

  int recursive = FALSE;
  int size = default_thumbnail_size;

  do {
    char *key = argv[currArg++];
    if (strmatches(key,"-log","--log",NULL)) {
        CHECK_ARG(1);
        strcpy(logFile,GET_ARG(1));
        fLog = FOPEN(logFile, "a");
        logflag = TRUE;
    }
    else if (strmatches(key,"-quiet","--quiet","-q",NULL)) {
        quietflag = TRUE;
    }
    else if (strmatches(key,"-verbose","--verbose","-v",NULL)) {
        verbose = TRUE;
    }
    else if (strmatches(key,"-recursive","--recursive","-r","-R",NULL)) {
        recursive = TRUE;
    }
    else if (strmatches(key,"-out-dir","--out-dir","--output-dir",
                            "-output-dir","-o",NULL)) {
        CHECK_ARG(1);
        char tmp[1024];
        strcpy(tmp,GET_ARG(1));
        out_dir = MALLOC(sizeof(char)*(strlen(tmp)+1));
        strcpy(out_dir, tmp);
    }
    else if (strmatches(key,"--size","-size","-s",NULL)) {
        CHECK_ARG(1);
        size = atoi(GET_ARG(1));
    }
    else if (strmatches(key,"-L0","-LO","-Lo","-l0","-lO","-lo",NULL)) {
        CHECK_ARG(1);
        char tmp[1024];
        strcpy(tmp,GET_ARG(1));
        if (strncmp(uc(tmp),"STF",3) == 0) {
            L0Flag=stf;
        }
        else if (strncmp(uc(tmp),"CEOS",4) == 0) {
            L0Flag=ceos;
        }
        else {
            L0Flag=not_L0;
        }
    }
    else if (strmatches(key,"-output-format","--output-format",
             "-out-format", "--out-format", NULL)) {
        CHECK_ARG(1);
        char tmp[1024];
        strcpy(tmp,GET_ARG(1));
        output_format=jpeg; // Default
        if (strncmp(uc(tmp),"TIF",3) == 0) {
            output_format=tiff;
        }
        else if (strncmp(uc(tmp),"JPG",3) == 0 ||
                 strncmp(uc(tmp),"JPEG",4) == 0) {
            output_format=jpeg;
        }
        else {
            fprintf(stderr,"\n**Invalid output format type \"%s\".  Expected tiff or jpeg.\n", tmp);
        }
    }
    else if (strmatches(key,"--scale","-scale",NULL)) {
        CHECK_ARG(1);
        scaleFlag=TRUE;
        scale_factor = atof(GET_ARG(1));
        if (scale_factor < 1.0) {
            fprintf(stderr,"\n**Invalid scale factor for -scale option."
                    "  Scale factor must be 1.0 or greater.\n");
        }
    }
    else if (strmatches(key,"--browse","-browse","-b",NULL)) {
        browseFlag=TRUE;
    }
    else if (strmatches(key,"--",NULL)) {
        break;
    }
    else if (key[0] == '-') {
      fprintf(stderr,"\n**Invalid option:  %s\n",argv[currArg-1]);
      if (!quietflag) usage();
      exit(1);
    }
    else {
        // this was a file/dir to process -- back up
        --currArg;
        break;
    }
  } while (currArg < argc);

  if (!quietflag) {
      asfSplashScreen(argc, argv);
  }

  if (out_dir && !quietflag) asfPrintStatus("Output directory is: %s\n", out_dir);

  int i;
  for (i=currArg; i<argc; ++i) {
      process(argv[i], 0, recursive, size, verbose,
              L0Flag, scale_factor, browseFlag,
              out_dir);
  }

  if (fLog) fclose(fLog);
  if (out_dir) free(out_dir);

  exit(EXIT_SUCCESS);
}

char *spaces(int n)
{
    int i;
    static char buf[256];
    for (i=0; i<256; ++i)
        buf[i] = i<n*3 ? ' ' : '\0';
    return buf;
}

int strmatches(const char *key, ...)
{
    va_list ap;
    char *arg = NULL;
    int found = FALSE;

    va_start(ap, key);
    do {
        arg = va_arg(ap, char *);
        if (arg) {
            if (strcmp(key, arg) == 0) {
                found = TRUE;
                break;
            }
        }
    } while (arg);

    return found;
}

int has_prepension(const char * data_file_name)
{
    /* at the moment, the only prepension we allow is IMG- (ALOS) */
    char *basename = get_basename(data_file_name);
    int ret = strncmp(basename, "LED-", 4) == 0;
    free(basename);
    return ret ? 4 : 0;
}

char *meta_file_name(const char * data_file_name)
{
    char *basename = get_basename(data_file_name);
    int is_alos = strncmp(basename, "IMG-", 4) == 0;
    free(basename);

    if (is_alos) {
        char *meta_name = MALLOC(sizeof(char) * (strlen(data_file_name) + 5));
        char *dir = get_dirname(data_file_name);
        strcpy(meta_name, dir);
        strcat(meta_name, "LED-");
        char *file = get_filename(data_file_name);
        char *p=strchr(file, '-');
        if (p) p = strchr(p+1, '-');
        if (p) strcat(meta_name, p+1);
        free(dir);
        free(file);
        return meta_name;
    }
    else {
        return appendExt(data_file_name, ".L");
    }
}

void process_dir(const char *dir, int top, int recursive, int size, int verbose,
                 level_0_flag L0Flag, float scale_factor, int browseFlag,
                 char *out_dir)
{
    char name[1024];
    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(dir)) == NULL) {
        asfPrintStatus("cannot open %s\n",dir);
        return; // error
    }
    while ((dp = readdir(dfd)) != NULL) {
        if (strcmp(dp->d_name, ".")==0 || strcmp(dp->d_name, "..")==0) {
            continue;
        }
        if (strlen(dir)+strlen(dp->d_name)+2 > sizeof(name)) {
            asfPrintWarning("dirwalk: name %s/%s exceeds buffersize.\n",
                            dir, dp->d_name);
            return; // error
        }
        else {
            sprintf(name, "%s%c%s", dir, DIR_SEPARATOR, dp->d_name);
            process(name, top, recursive, size, verbose,
                    L0Flag, scale_factor, browseFlag,
                    out_dir);
        }
    }
    closedir(dfd);
}

meta_parameters * silent_meta_create(const char *filename)
{
    report_level_t prev = g_report_level;

    g_report_level = NOREPORT;
    meta_parameters *ret = meta_create(filename);

    g_report_level = prev;
    return ret;
}

int generate_ceos_thumbnail(const char *input_data, int size, char *out_dir)
{
    char *input_metadata = meta_file_name(input_data);

    /* This can happen if we don't get around to drawing the thumbnail
    until the file has already been processes & cleaned up, don't want
    to crash in that case. */
    if (!fileExists(input_metadata)) {
        asfPrintStatus("Metadata file not found: %s\n", input_metadata);
        return FALSE;
    }

    // Input metadata
    meta_parameters *imd;
    char *data_name, *met;

    int pre = has_prepension(input_metadata);
    if (pre > 0)
    {
        int nBands;
        char **dataName, *baseName, filename[255], dirname[255];
        baseName = (char *) MALLOC(sizeof(char)*256);
        split_dir_and_file(input_metadata, dirname, filename);
        met = MALLOC(sizeof(char)*(strlen(input_metadata)+1));
        sprintf(met, "%s%s", dirname, filename + pre);

        get_ceos_data_name(met, baseName, &dataName, &nBands);

        data_name = STRDUP(dataName[0]);
        imd = silent_meta_create(met);

        free_ceos_names(dataName, NULL);
    }
    else
    {
        imd = silent_meta_create (input_metadata);
        data_name = STRDUP(input_data);
        met = STRDUP(input_metadata);
    }

    if (imd->general->data_type != BYTE &&
        imd->general->data_type != INTEGER16)
// Turning off support for these guys for now.
//        imd->general->data_type != INTEGER32 &&
//        imd->general->data_type != REAL32 &&
//        imd->general->data_type != REAL64)
    {
        /* don't know how to make a thumbnail for this type ... */
        asfPrintStatus("Unknown data type: %d\n", imd->general->data_type);
        return FALSE;
    }

    FILE *fpIn = fopen(data_name, "rb");
    if (!fpIn)
    {
        // failed for some reason, quit without thumbnailing
        meta_free(imd);
        asfPrintStatus("Failed to open: %s\n", data_name);
        return FALSE;
    }

    struct IOF_VFDR image_fdr;                /* CEOS File Descriptor Record */
    get_ifiledr(met, &image_fdr);
    int leftFill = image_fdr.lbrdrpxl;
    int rightFill = image_fdr.rbrdrpxl;
    int headerBytes = firstRecordLen(data_name) +
            (image_fdr.reclen - (imd->general->sample_count + leftFill + rightFill)
            * image_fdr.bytgroup);

    // use a larger dimension at first, for our crude scaling.  We will
    // use a better scaling method later, from GdbPixbuf
    int sf;
    if (size > 1024)
    {
        sf = 1; // read in the whole thing
    }
    else
    {
        int larger_dim = size*4;
        if (larger_dim < 1024) larger_dim = 1024;

        // Vertical and horizontal scale factors required to meet the
        // max_thumbnail_dimension part of the interface contract.
        int vsf = ceil (imd->general->line_count / larger_dim);
        int hsf = ceil (imd->general->sample_count / larger_dim);
        // Overall scale factor to use is the greater of vsf and hsf.
        sf = (hsf > vsf ? hsf : vsf);
    }

    // Thumbnail image sizes.
    size_t tsx = imd->general->sample_count / sf;
    size_t tsy = imd->general->line_count / sf;

    // Form the thumbnail image by grabbing individual pixels.  FIXME:
    // Might be better to do some averaging or interpolating.
    size_t ii, jj;
    unsigned short *line = MALLOC (sizeof(unsigned short) * imd->general->sample_count);
    unsigned char *bytes = MALLOC (sizeof(unsigned char) * imd->general->sample_count);

    // Here's where we're putting all this data
    FloatImage *img = float_image_new(tsx, tsy);

    // Read in data line-by-line
    for ( ii = 0 ; ii < tsy ; ii++ ) {
        long long offset = (long long)headerBytes+ii*sf*(long long)image_fdr.reclen;

        FSEEK64(fpIn, offset, SEEK_SET);
        if (imd->general->data_type == INTEGER16)
        {
            FREAD(line, sizeof(unsigned short), imd->general->sample_count, fpIn);

            for (jj = 0; jj < imd->general->sample_count; ++jj) {
                big16(line[jj]);
            }
        }
        else if (imd->general->data_type == BYTE)
        {
            FREAD(bytes, sizeof(unsigned char), imd->general->sample_count, fpIn);

            for (jj = 0; jj < imd->general->sample_count; ++jj) {
                line[jj] = (unsigned short)bytes[jj];
            }
        }

        for ( jj = 0 ; jj < tsx ; jj++ ) {
            // Current sampled value.
            double csv;

            if (sf == 1) {
                csv = line[jj];
            } else {
                // We will average a couple pixels together.
                if ( jj * sf < imd->general->line_count - 1 ) {
                    csv = (line[jj * sf] + line[jj * sf + 1]) / 2;
                }
                else {
                    csv = (line[jj * sf] + line[jj * sf - 1]) / 2;
                }
            }

            float_image_set_pixel(img, jj, ii, csv);
        }
    }
    FREE (line);
    FREE (bytes);
    fclose(fpIn);

    char *out_file;
    char *thumb_file = appendToBasename(input_data, "_thumb");

    if (out_dir && strlen(out_dir) > 0) {
        char *basename = get_basename(thumb_file);
        out_file = MALLOC((strlen(out_dir)+strlen(basename)+10)*sizeof(char));
        sprintf(out_file, "%s/%s.jpg", out_dir, basename);
    } else {
        out_file = appendExt(thumb_file, ".jpg");
    }

    // Create the jpeg
    float_image_export_as_jpeg(img, out_file, size, NAN);

    meta_free(imd);
    FREE(data_name);
    FREE(thumb_file);
    FREE(met);
    FREE(out_file);
    FREE(input_metadata);

    return TRUE;
}

void process_file(const char *file, int level, int size, int verbose, char *out_dir)
{
    char *base = get_filename(file);
    char *ext = findExt(base);
    if ((ext && strcmp_case(ext, ".D") == 0) ||
         (strncmp(base, "IMG-", 4) == 0))
    {
        asfPrintStatus("%s%s\n", spaces(level), base);
        generate_ceos_thumbnail(file, size, out_dir);
    }
    else {
        if (verbose) {
            asfPrintStatus("%s%s (ignored)\n", spaces(level), base);
        }
    }
    FREE(base);
}

void process(const char *what, int level, int recursive, int size, int verbose,
             level_0_flag L0Flag, float scale_factor, int browseFlag,
             char *out_dir)
{
    struct stat stbuf;

    if (stat(what, &stbuf) == -1) {
        asfPrintStatus("Cannot access: %s\n", what);
        return;
    }

    char *base = get_filename(what);

    if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
        if (level==0 || recursive) {
            asfPrintStatus("%s%s/\n", spaces(level), base);
            process_dir(what, level+1, recursive, size, verbose,
                        L0Flag, scale_factor, browseFlag,
                        out_dir);
        }
        else {
            if (verbose) {
                asfPrintStatus("%s%s (skipped)\n", spaces(level), base);
            }
        }
    }
    else {
        process_file(what, level, size, verbose, out_dir);
    }

    FREE(base);
}

