// NOTE: slightly modified from: https://www.flipcode.com/archives/HDR_Image_Reader.shtml

/***********************************************************************************
        Created:        17:9:2002
        FileName:       hdrloader.cpp
        Author:         Igor Kravtchenko

        Info:           Load HDR image and convert to a set of float32 RGB triplet.
************************************************************************************/

typedef u8 RGBE[4];
#define R       0
#define G       1
#define B       2
#define E       3
#define MINELEN 8      // minimum scanline length for encoding
#define MAXELEN 0x7fff // maximum scanline length for encoding

float convert_component(int expo, int val) {
    float v = val / 256.0f;
    float d = (float) pow(2, expo);
    return v * d;
}

static void work_on_rgbe(RGBE* scan, int len, f32* data) {
    while (len-- > 0) {
        int expo = scan[0][E] - 128;
        data[0] = convert_component(expo, scan[0][R]);
        data[1] = convert_component(expo, scan[0][G]);
        data[2] = convert_component(expo, scan[0][B]);
        data += 3;
        scan++;
    }
}

static bool
old_decrunch(RGBE* scanline, int len, FILE* file) {
    int i;
    int rshift = 0;

    while (len > 0) {
        scanline[0][R] = fgetc(file);
        scanline[0][G] = fgetc(file);
        scanline[0][B] = fgetc(file);
        scanline[0][E] = fgetc(file);
        if (feof(file))
            return false;

        if (scanline[0][R] == 1 &&
            scanline[0][G] == 1 &&
            scanline[0][B] == 1) {
            for (i = scanline[0][E] << rshift; i > 0; i--) {
                memcpy(&scanline[0][0], &scanline[-1][0], 4);
                scanline++;
                len--;
            }
            rshift += 8;
        }
        else {
            scanline++;
            len--;
            rshift = 0;
        }
    }
    return true;
}

static bool
decrunch(RGBE* scanline, int len, FILE* file) {
    int  i, j;

    if (len < MINELEN || len > MAXELEN)
        return old_decrunch(scanline, len, file);

    i = fgetc(file);
    if (i != 2) {
        fseek(file, -1, SEEK_CUR);
        return old_decrunch(scanline, len, file);
    }

    scanline[0][G] = fgetc(file);
    scanline[0][B] = fgetc(file);
    i = fgetc(file);

    if (scanline[0][G] != 2 || scanline[0][B] & 128) {
        scanline[0][R] = 2;
        scanline[0][E] = i;
        return old_decrunch(scanline + 1, len - 1, file);
    }

    // read each component
    for (i = 0; i < 4; i++) {
        for (j = 0; j < len; ) {
            unsigned char code = fgetc(file);
            if (code > 128) { // run
                code &= 127;
                unsigned char val = fgetc(file);
                while (code--)
                    scanline[j++][i] = val;
            }
            else  { // non-run
                while(code--)
                    scanline[j++][i] = fgetc(file);
            }
        }
    }

    return feof(file) ? false : true;
}

f32*
load_hdr_image(const char* filename, int* width, int* height) {
    int i;
    char str[200];
    FILE* file;

    errno_t error = fopen_s(&file, filename, "rb");
    assert(error == 0 && "failed to open file");
    if (!file)
        return NULL;

    fread(str, 10, 1, file);
    if (memcmp(str, "#?RADIANCE", 10)) {
        fclose(file);
        return NULL;
    }

    fseek(file, 1, SEEK_CUR);

    char cmd[200];
    i = 0;
    char c = 0, oldc;
    while(true) {
        oldc = c;
        c = fgetc(file);
        if (c == 0xa && oldc == 0xa) break;
        cmd[i++] = c;
    }

    char reso[200];
    i = 0;
    while(true) {
        c = fgetc(file);
        reso[i++] = c;
        if (c == 0xa) break;
    }

    int w, h;
    if (!sscanf_s(reso, "-Y %ld +X %ld", &h, &w)) {
        fclose(file);
        return NULL;
    }
    *width = w;
    *height = h;

    f32* output_data = new f32[w * h * 3];

    RGBE *scanline = new RGBE[w];
    if (!scanline) {
        fclose(file);
        return false;
    }

    // convert image
    f32* data = output_data;
    for (int y = h - 1; y >= 0; y--) {
        if (decrunch(scanline, w, file) == false) {
            break;
        }
        work_on_rgbe(scanline, w, data);
        data += w * 3;
    }

    delete [] scanline;
    fclose(file);

    return output_data;
}

#undef R
#undef G
#undef B
#undef E
#undef MINELEN
#undef MAXELEN
