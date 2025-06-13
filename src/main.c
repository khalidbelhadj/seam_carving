#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct matrix_t {
    unsigned char *data;
    int width;
    int height;
};

struct image_t {
    unsigned char *data;
    int width;
    int height;
    int channels;
};

struct image_t image_create(int width, int height, int channels) {
    struct image_t img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data = calloc(1, sizeof(char) * width * height * channels);
    return img;
}

struct matrix_t matrix_create(int width, int height) {
    struct matrix_t m;
    m.width = width;
    m.height = height;
    m.data = calloc(1, sizeof(char) * width * height);
    return m;
}

struct matrix_t grayscale(struct image_t m) {
    int new_size = m.width * m.height;
    struct matrix_t result = matrix_create(m.width, m.height);
    for (int i = 0; i < new_size; ++i) {
        unsigned char r = m.data[i * 3 + 0];
        unsigned char g = m.data[i * 3 + 1];
        unsigned char b = m.data[i * 3 + 2];
        unsigned char gray = (r + g + b) / 3;
        result.data[i] = gray;
    }
    return result;
}

void sobel_filter(struct matrix_t *m) {
    static const char gradient_kernel_x[3][3] = {
        {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    static const char gradient_kernel_y[3][3] = {
        {-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    struct matrix_t result = matrix_create(m->width, m->height);

    for (int i = 0; i < m->height; ++i) {
        for (int j = 0; j < m->width; ++j) {
            if (i == 0 || i == m->height - 1 || j == 0 || j == m->width - 1) {
                result.data[i * m->width + j] = 0;
                continue;
            }

            int gx = 0;
            int gy = 0;

            for (int k = -1; k <= 1; ++k) {
                for (int l = -1; l <= 1; ++l) {
                    int ni = i + k;
                    int nj = j + l;

                    if (ni < 0 || ni >= m->height || nj < 0 || nj >= m->width) {
                        continue;
                    }

                    unsigned char pixel = m->data[ni * m->width + nj];
                    gx += gradient_kernel_x[k + 1][l + 1] * pixel;
                    gy += gradient_kernel_y[k + 1][l + 1] * pixel;
                }
            }

            gx = pow(gx, 2);
            gy = pow(gy, 2);

            // Clamp gx and gy to avoid overflow
            if (gx < -255) gx = -255;
            if (gx > 255) gx = 255;
            if (gy < -255) gy = -255;
            if (gy > 255) gy = 255;

            // Compute energy
            // result.data[i * result.width + j] = abs(gx) + abs(gy);
            result.data[i * result.width + j] = sqrt(gx + gy);
        }
    }

    memcpy(m->data, result.data, m->width * m->height);
    free(result.data);
}

int *find_seam(struct matrix_t m) {
    // cost so far
    int *cost = calloc(1, sizeof(int) * m.width * m.height);
    // the previous row from this cell which has the minimum cost
    int *back = calloc(1, sizeof(int) * m.width * m.height);

    // Initialize first row
    for (int i = 0; i < m.width; ++i) {
        cost[i] = m.data[i];
    }

    // Fill the DP table
    for (int row = 1; row < m.height; ++row) {
        for (int col = 0; col < m.width; ++col) {
            int min_cost = cost[(row - 1) * m.width + col];
            int best_prev_col = col;

            // Check left diagonal
            if (col > 0) {
                int left_cost = cost[(row - 1) * m.width + (col - 1)];
                if (left_cost < min_cost) {
                    min_cost = left_cost;
                    best_prev_col = col - 1;
                }
            }

            // Check right diagonal
            if (col < m.width - 1) {
                int right_cost = cost[(row - 1) * m.width + (col + 1)];
                if (right_cost < min_cost) {
                    min_cost = right_cost;
                    best_prev_col = col + 1;
                }
            }

            cost[row * m.width + col] = m.data[row * m.width + col] + min_cost;
            back[row * m.width + col] = best_prev_col;
        }
    }

    // Find minimum in last row
    int min_cost = cost[(m.height - 1) * m.width];
    int min_col = 0;
    for (int i = 1; i < m.width; ++i) {
        int curr_cost = cost[(m.height - 1) * m.width + i];
        if (curr_cost < min_cost) {
            min_cost = curr_cost;
            min_col = i;
        }
    }

    // Backtrack to find the seam
    int *seam = malloc(m.height * sizeof(int));
    for (int row = m.height - 1; row >= 0; --row) {
        seam[row] = min_col;
        if (row > 0) {
            min_col = back[row * m.width + min_col];
        }
    }

    free(cost);
    free(back);

    return seam;
}

void remove_seam(struct image_t *image, struct matrix_t *m, int *seam) {
    int new_width = m->width - 1;
    unsigned char *new_m_data = calloc(1, sizeof(char) * new_width * m->height);
    unsigned char *new_image_data =
        calloc(1, sizeof(char) * new_width * image->height * image->channels);

    for (int row = 0; row < m->height; ++row) {
        int seam_col = seam[row];
        for (int col = 0; col < m->width; ++col) {
            if (col < seam_col) {
                new_m_data[row * new_width + col] =
                    m->data[row * m->width + col];

                for (int c = 0; c < image->channels; ++c) {
                    new_image_data[row * new_width * image->channels +
                                   col * image->channels + c] =
                        image->data[row * m->width * image->channels +
                                    col * image->channels + c];
                }

            } else if (col > seam_col) {
                new_m_data[row * new_width + col - 1] =
                    m->data[row * m->width + col];

                for (int c = 0; c < image->channels; ++c) {
                    new_image_data[row * new_width * image->channels +
                                   (col - 1) * image->channels + c] =
                        image->data[row * m->width * image->channels +
                                    col * image->channels + c];
                }
            }
        }
    }

    free(m->data);
    m->data = new_m_data;
    m->width = new_width;

    free(image->data);
    image->data = new_image_data;
    image->width = new_width;
}

void usage() {
    fprintf(stderr, "Usage: seam_carving <image_file>\n");
    fprintf(stderr, "Example: seam_carving image.jpg\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage();
        return 1;
    }

    char *image_file = argv[1];
    char *output_file = argv[2];
    float ratio = 0.1f;

    int width, height, channels;
    unsigned char *data = stbi_load(image_file, &width, &height, &channels, 3);
    if (data == NULL) {
        fprintf(stderr, "Error loading image: %s\n", stbi_failure_reason());
        return 1;
    }
    printf("Image loaded: %dx%d, %d channels\n", width, height, channels);
    assert(channels == 3 && "Image must have 3 channels (RGB)");

    // Create image matrix
    struct image_t image = image_create(width, height, channels);
    memcpy(image.data, data, width * height * channels);
    stbi_image_free(data);

    // Grayscale the image
    struct matrix_t m = grayscale(image);
    sobel_filter(&m);

    for (int i = 0; i < (1 - ratio) * image.width; ++i) {
        int *seam = find_seam(m);
        remove_seam(&image, &m, seam);
        free(seam);
    }

    // Export
    stbi_write_jpg(output_file, image.width, image.height, image.channels,
                   image.data, 100);

    return 0;
}