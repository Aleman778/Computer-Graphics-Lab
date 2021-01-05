
static f32
fade(f32 t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static f32
grad(int hash, f32 x, f32 y, f32 z) {
    switch (hash & 0xf) {
        case 0x0: return  x + y;
        case 0x1: return -x + y;
        case 0x2: return  x - y;
        case 0x3: return -x - y;
        case 0x4: return  x + z;
        case 0x5: return -x + z;
        case 0x6: return  x - z;
        case 0x7: return -x - z;
        case 0x8: return  y + z;
        case 0x9: return -y + z;
        case 0xa: return  y - z;
        case 0xb: return -y - z;
        case 0xc: return  y + x;
        case 0xd: return -y + z;
        case 0xe: return  y - x;
        case 0xf: return -y - z;
        default:  return 0; // never happens
    }
}

f32
lerp(f32 t, f32 a, f32 b) {
    return a + t * (b - a);
}

static int*
generate_perlin_permutations() {
    std::random_device rd;
    std::mt19937 rng = std::mt19937(rd());
    std::uniform_real_distribution<f32> random(0.0f, 1.0f);

    int* permutations = new int[512];
    for (int i = 0; i < 256; i++) {
        permutations[i] = i;
    }
    for (int i = 0; i < 256; i++) {
        int index = i + (int) (random(rng) * (256 - i));
        int temp = permutations[i];
        permutations[i] = permutations[index];
        permutations[index] = temp;
    }
    for (int i = 0; i < 256; i++) {
        permutations[256+i] = permutations[i];
    }
    
#if 0
    printf("generated_permutations = {\n");
    for (int i = 0; i < 256; i++) {
        printf("%d, ", permutations[i]);
        if (i % 20 == 19) {
            printf("\n");
        }
    }
    printf("\n}\n");
#endif
    
    return permutations;
}

f32 // NOTE: implementation based on https://adrianb.io/2014/08/09/perlinnoise.html
perlin_noise(f32 x, f32 y, f32 z) {
    static int* p = generate_perlin_permutations();

    int xi = (int) x & 0xff;
    int yi = (int) y & 0xff;
    int zi = (int) z & 0xff;
    x -= (int) x;
    y -= (int) y;
    z -= (int) z;
    f32 u = fade(x);
    f32 v = fade(y);
    f32 w = fade(z);

    int aaa = p[p[p[xi    ] + yi    ] + zi    ];
    int aba = p[p[p[xi    ] + yi + 1] + zi    ];
    int aab = p[p[p[xi    ] + yi    ] + zi + 1];
    int abb = p[p[p[xi    ] + yi + 1] + zi + 1];
    int baa = p[p[p[xi + 1] + yi    ] + zi    ];
    int bba = p[p[p[xi + 1] + yi + 1] + zi    ];
    int bab = p[p[p[xi + 1] + yi    ] + zi + 1];
    int bbb = p[p[p[xi + 1] + yi + 1] + zi + 1];

    f32 x1, x2, y1, y2;
    x1 = lerp(u, grad(aaa, x, y,     z), grad(baa, x - 1, y,     z));
    x2 = lerp(u, grad(aba, x, y - 1, z), grad(bba, x - 1, y - 1, z));
    y1 = lerp(v, x1, x2);

    x1 = lerp(u, grad(aab, x, y,     z - 1), grad(bab, x - 1, y,     z - 1));
    x2 = lerp(u, grad(abb, x, y - 1, z - 1), grad(bbb, x - 1, y - 1, z - 1));
    y2 = lerp(v, x1, x2);

    return (lerp(w, y1, y2) + 1)/2;
}

f32
octave_perlin_noise(f32 x, f32 y, f32 z, int octaves, f32 persistance) {
    f32 total = 0.0f;
    f32 frequency = 1.0f;
    f32 amplitude = 1.0f;
    f32 max_value = 0.0f;
    for (int i = 0; i < octaves; i++) {
        total += perlin_noise(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistance;
        frequency *= 2;
    }

    return total/max_value;
}

f32
sample_point_at(Height_Map* map, f32 x, f32 y) {
    int x0 = (int) (x*map->scale_x);
    int y0 = (int) (y*map->scale_y);

    if (x0 < 0) x0 = 0; if (x0 > map->width - 1)  x0 = map->width  - 1;
    if (y0 < 0) y0 = 0; if (y0 > map->height - 1) y0 = map->height - 1;
    
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    if (x1 > map->width - 1)  x1 = map->width  - 1;
    if (y1 > map->height - 1) y1 = map->height - 1;

    f32 h1 = map->data[x0+y0*map->width];
    f32 h2 = map->data[x1+y0*map->width];
    f32 h3 = map->data[x0+y1*map->width];
    f32 h4 = map->data[x1+y1*map->width];
    
    f32 u = x*map->scale_x - x0;
    f32 v = y*map->scale_y - y0;
    f32 hx0 = lerp(u, h1, h2);
    f32 hx1 = lerp(u, h3, h4);
    return lerp(v, hx0, hx1);
}
