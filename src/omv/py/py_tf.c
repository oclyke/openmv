/* This file is part of the OpenMV project.
 * Copyright (c) 2013-2019 Ibrahim Abdelkader <iabdalkader@openmv.io> & Kwabena W. Agyeman <kwagyeman@openmv.io>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 */

#include <mp.h>
#include "py_helper.h"
#include "imlib.h"
#include "ff_wrapper.h"
#include "libtf.h"
#include "libtf-mobilenet.h"

#ifdef IMLIB_ENABLE_TF

// TF Model Object
typedef struct py_tf_model_obj {
    mp_obj_base_t base;
    unsigned char *model_data;
    unsigned int model_data_len, height, width, channels;
} py_tf_model_obj_t;

STATIC void py_tf_model_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    py_tf_model_obj_t *self = self_in;
    mp_printf(print,
              "{\"len\":%d, \"height\":%d, \"width\":%d, \"channels\":%d}",
              self->model_data_len,
              self->height,
              self->width,
              self->channels);
}

// TF Class Object
#define py_tf_class_obj_size 6
typedef struct py_tf_class_obj {
    mp_obj_base_t base;
    mp_obj_t x, y, w, h, index, value;
} py_tf_class_obj_t;

STATIC void py_tf_class_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
    py_tf_class_obj_t *self = self_in;
    mp_printf(print,
              "{\"x\":%d, \"y\":%d, \"w\":%d, \"h\":%d, \"index\":%d, \"value\":%f}",
              mp_obj_get_int(self->x),
              mp_obj_get_int(self->y),
              mp_obj_get_int(self->w),
              mp_obj_get_int(self->h),
              mp_obj_get_int(self->index),
              (double) mp_obj_get_float(self->value));
}

STATIC mp_obj_t py_tf_class_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value)
{
    if (value == MP_OBJ_SENTINEL) { // load
        py_tf_class_obj_t *self = self_in;
        if (MP_OBJ_IS_TYPE(index, &mp_type_slice)) {
            mp_bound_slice_t slice;
            if (!mp_seq_get_fast_slice_indexes(py_tf_class_obj_size, index, &slice)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "only slices with step=1 (aka None) are supported"));
            }
            mp_obj_tuple_t *result = mp_obj_new_tuple(slice.stop - slice.start, NULL);
            mp_seq_copy(result->items, &(self->x) + slice.start, result->len, mp_obj_t);
            return result;
        }
        switch (mp_get_index(self->base.type, py_tf_class_obj_size, index, false)) {
            case 0: return self->x;
            case 1: return self->y;
            case 2: return self->w;
            case 3: return self->h;
            case 4: return self->index;
            case 5: return self->value;
        }
    }
    return MP_OBJ_NULL; // op not supported
}

mp_obj_t py_tf_class_rect(mp_obj_t self_in)
{
    return mp_obj_new_tuple(4, (mp_obj_t []) {((py_tf_class_obj_t *) self_in)->x,
                                              ((py_tf_class_obj_t *) self_in)->y,
                                              ((py_tf_class_obj_t *) self_in)->w,
                                              ((py_tf_class_obj_t *) self_in)->h});
}

mp_obj_t py_tf_class_x(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->x; }
mp_obj_t py_tf_class_y(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->y; }
mp_obj_t py_tf_class_w(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->w; }
mp_obj_t py_tf_class_h(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->h; }
mp_obj_t py_tf_class_index(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->index; }
mp_obj_t py_tf_class_value(mp_obj_t self_in) { return ((py_tf_class_obj_t *) self_in)->value; }

STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_rect_obj, py_tf_class_rect);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_x_obj, py_tf_class_x);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_y_obj, py_tf_class_y);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_w_obj, py_tf_class_w);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_h_obj, py_tf_class_h);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_index_obj, py_tf_class_index);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_class_value_obj, py_tf_class_value);

STATIC const mp_rom_map_elem_t py_tf_class_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&py_tf_class_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_x), MP_ROM_PTR(&py_tf_class_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_y), MP_ROM_PTR(&py_tf_class_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_w), MP_ROM_PTR(&py_tf_class_w_obj) },
    { MP_ROM_QSTR(MP_QSTR_h), MP_ROM_PTR(&py_tf_class_h_obj) },
    { MP_ROM_QSTR(MP_QSTR_index), MP_ROM_PTR(&py_tf_class_index_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&py_tf_class_value_obj) }
};

STATIC MP_DEFINE_CONST_DICT(py_tf_class_locals_dict, py_tf_class_locals_dict_table);

static const mp_obj_type_t py_tf_class_type = {
    { &mp_type_type },
    .name  = MP_QSTR_tf_class,
    .print = py_tf_class_print,
    .subscr = py_tf_class_subscr,
    .locals_dict = (mp_obj_t) &py_tf_class_locals_dict
};

static const mp_obj_type_t py_tf_model_type;

typedef struct py_tf_class_obj_list_lnk_data {
    rectangle_t rect;
    int index;
    float value;
    int merge_number;
    float value_acc;
} py_tf_class_obj_list_lnk_data_t;

STATIC mp_obj_t py_tf_classify(uint n_args, const mp_obj_t *args, mp_map_t *kw_args)
{
    py_tf_model_obj_t *arg_model;
    image_t *arg_img = py_helper_arg_to_image_mutable(args[1]);

    rectangle_t roi;
    py_helper_keyword_rectangle_roi(arg_img, n_args, args, 2, kw_args, &roi);

    float arg_threshold = py_helper_keyword_float(n_args, args, 3, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_threshold), 0.6f);
    PY_ASSERT_TRUE_MSG((0.0f <= arg_threshold) && (arg_threshold <= 1.0f), "0 <= threshold <= 1");

    float arg_min_scale = py_helper_keyword_float(n_args, args, 4, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_min_scale), 1.0f);
    PY_ASSERT_TRUE_MSG((0.0f < arg_min_scale) && (arg_min_scale <= 1.0f), "0 < min_scale <= 1");

    float arg_scale_mul = py_helper_keyword_float(n_args, args, 5, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_scale_mul), 0.5f);
    PY_ASSERT_TRUE_MSG((0.0f <= arg_scale_mul) && (arg_scale_mul < 1.0f), "0 <= scale_mul < 1");

    float arg_x_overlap = py_helper_keyword_float(n_args, args, 6, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_x_overlap), 0.0f);
    PY_ASSERT_TRUE_MSG(((0.0f <= arg_x_overlap) && (arg_x_overlap < 1.0f)) || (arg_x_overlap == -1.0f), "0 <= x_overlap < 1");

    float arg_y_overlap = py_helper_keyword_float(n_args, args, 7, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_y_overlap), 0.0f);
    PY_ASSERT_TRUE_MSG(((0.0f <= arg_y_overlap) && (arg_y_overlap < 1.0f)) || (arg_y_overlap == -1.0f), "0 <= y_overlap < 1");

    float arg_contrast_threshold = py_helper_keyword_float(n_args, args, 8, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_contrast_threshold), -1.0f);
    PY_ASSERT_TRUE_MSG((0.0f <= arg_contrast_threshold) || (arg_contrast_threshold == -1.0f), "0 <= contrast_threshold");

    bool arg_normalize = py_helper_keyword_int(n_args, args, 9, kw_args, MP_OBJ_NEW_QSTR(MP_QSTR_normalize), false);

    fb_alloc_mark();

    if (MP_OBJ_IS_TYPE(args[0], &py_tf_model_type)) {
        arg_model = (py_tf_model_obj_t *) args[0];
    } else {
        const char *path = mp_obj_str_get_str(args[0]);
        arg_model = m_new_obj(py_tf_model_obj_t);
        arg_model->base.type = &py_tf_model_type;

        if (!strcmp(path, "mobilenet")) {
            arg_model->model_data = (unsigned char *) mobilenet_model_data;
            arg_model->model_data_len = mobilenet_model_data_len;
        } else {
            FIL fp;
            file_read_open(&fp, path);
            arg_model->model_data_len = f_size(&fp);
            arg_model->model_data = fb_alloc(arg_model->model_data_len);
            read_data(&fp, arg_model->model_data, arg_model->model_data_len);
            file_close(&fp);
        }

        uint32_t tensor_arena_size;
        uint8_t *tensor_arena = fb_alloc_all(&tensor_arena_size);

        PY_ASSERT_FALSE_MSG(libtf_get_input_data_hwc(arg_model->model_data,
                                                     tensor_arena,
                                                     tensor_arena_size,
                                                     &arg_model->height,
                                                     &arg_model->width,
                                                     &arg_model->channels),
                            "Unable to read model height, width, and channels!");

        fb_free(); // Free just the fb_alloc_all() - not fb_alloc() above.
    }

    uint8_t *input_data = fb_alloc(arg_model->height * arg_model->width * arg_model->channels);

    uint32_t tensor_arena_size;
    uint8_t *tensor_arena = fb_alloc_all(&tensor_arena_size);

    unsigned int class_scores_size;
    PY_ASSERT_FALSE_MSG(libtf_get_classification_class_scores_size(arg_model->model_data,
                                                                   tensor_arena,
                                                                   tensor_arena_size,
                                                                   &class_scores_size),
                        "Unable to get read model class scores size!");
    fb_free();
    float *class_scores = fb_alloc(class_scores_size * sizeof(float));
    tensor_arena = fb_alloc_all(&tensor_arena_size);

    list_t out;
    list_init(&out, sizeof(py_tf_class_obj_list_lnk_data_t));

    for (float scale = 1.0f; scale >= arg_min_scale; scale *= arg_scale_mul) {
        // Either provide a subtle offset to center multiple detection windows or center the only detection window.
        for (int y = roi.y + ((arg_y_overlap != -1.0f) ? (fmodf(roi.h, (roi.h * scale)) / 2.0f) : ((roi.h - (roi.h * scale)) / 2.0f));
            // Finish when the detection window is outside of the ROI.
            (y + (roi.h * scale)) <= (roi.y + roi.h);
            // Step by an overlap amount accounting for scale or just terminate after one iteration.
            y += ((arg_y_overlap != -1.0f) ? (roi.h * scale * (1.0f - arg_y_overlap)) : roi.h)) {
            // Either provide a subtle offset to center multiple detection windows or center the only detection window.
            for (int x = roi.x + ((arg_x_overlap != -1.0f) ? (fmodf(roi.w, (roi.w * scale)) / 2.0f) : ((roi.w - (roi.w * scale)) / 2.0f));
                // Finish when the detection window is outside of the ROI.
                (x + (roi.w * scale)) <= (roi.x + roi.w);
                // Step by an overlap amount accounting for scale or just terminate after one iteration.
                x += ((arg_x_overlap != -1.0f) ? (roi.w * scale * (1.0f - arg_x_overlap)) : roi.w)) {
                rectangle_t new_roi;
                rectangle_init(&new_roi, x, y, roi.w * scale, roi.h * scale);
                if (rectangle_overlap(&roi, &new_roi)) { // Check if new_roi is null...

                    if (arg_contrast_threshold >= 0.0f) {
                        long long sum = 0;
                        long long sum_2 = 0;
                        for (int b = new_roi.y, bb = new_roi.y + new_roi.h; b < bb; b++) {
                            for (int a = new_roi.x, aa = new_roi.x + new_roi.w; a < aa; a++) {
                                switch(arg_img->bpp) {
                                    case IMAGE_BPP_BINARY: {
                                        int pixel = COLOR_BINARY_TO_GRAYSCALE(IMAGE_GET_BINARY_PIXEL(arg_img, a, b));
                                        sum += pixel;
                                        sum_2 += pixel * pixel;
                                        break;
                                    }
                                    case IMAGE_BPP_GRAYSCALE: {
                                        int pixel = IMAGE_GET_GRAYSCALE_PIXEL(arg_img, a, b);
                                        sum += pixel;
                                        sum_2 += pixel * pixel;
                                        break;
                                    }
                                    case IMAGE_BPP_RGB565: {
                                        int pixel = COLOR_RGB565_TO_GRAYSCALE(IMAGE_GET_RGB565_PIXEL(arg_img, a, b));
                                        sum += pixel;
                                        sum_2 += pixel * pixel;
                                        break;
                                    }
                                    default: {
                                        break;
                                    }
                                }
                            }
                        }

                        int area = new_roi.w * new_roi.h;
                        int mean = sum / area;
                        int variance = (sum_2 / area) - (mean * mean);

                        if (fast_sqrtf(variance) < arg_contrast_threshold) { // Skip flat regions...
                            continue;
                        }
                    }

                    float over_xscale = IM_DIV(new_roi.w, arg_model->width);
                    float over_yscale = IM_DIV(new_roi.h, arg_model->height);

                    switch(arg_img->bpp) {
                        case IMAGE_BPP_BINARY: {
                            for (int y = 0, yy = arg_model->height; y < yy; y++) {
                                uint32_t *row_ptr = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(arg_img, fast_floorf(y * over_yscale) + new_roi.y);
                                int row = arg_model->width * y;
                                for (int x = 0, xx = arg_model->width; x < xx; x++) {
                                    int pixel = IMAGE_GET_BINARY_PIXEL_FAST(row_ptr, fast_floorf(x * over_xscale) + new_roi.x);
                                    int index = row + x;
                                    switch (arg_model->channels) {
                                        case 1: {
                                            input_data[index] = COLOR_BINARY_TO_GRAYSCALE(pixel);
                                            break;
                                        }
                                        case 3: {
                                            int index_3 = index * 3;
                                            pixel = COLOR_BINARY_TO_RGB565(pixel);
                                            input_data[index_3 + 0] = COLOR_RGB565_TO_R8(pixel);
                                            input_data[index_3 + 1] = COLOR_RGB565_TO_G8(pixel);
                                            input_data[index_3 + 2] = COLOR_RGB565_TO_B8(pixel);
                                            break;
                                        }
                                        default: {
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case IMAGE_BPP_GRAYSCALE: {
                            for (int y = 0, yy = arg_model->height; y < yy; y++) {
                                uint8_t *row_ptr = IMAGE_COMPUTE_GRAYSCALE_PIXEL_ROW_PTR(arg_img, fast_floorf(y * over_yscale) + new_roi.y);
                                int row = arg_model->width * y;
                                for (int x = 0, xx = arg_model->width; x < xx; x++) {
                                    int pixel = IMAGE_GET_GRAYSCALE_PIXEL_FAST(row_ptr, fast_floorf(x * over_xscale) + new_roi.x);
                                    int index = row + x;
                                    switch (arg_model->channels) {
                                        case 1: {
                                            input_data[index] = pixel;
                                            break;
                                        }
                                        case 3: {
                                            int index_3 = index * 3;
                                            pixel = COLOR_GRAYSCALE_TO_RGB565(pixel);
                                            input_data[index_3 + 0] = COLOR_RGB565_TO_R8(pixel);
                                            input_data[index_3 + 1] = COLOR_RGB565_TO_G8(pixel);
                                            input_data[index_3 + 2] = COLOR_RGB565_TO_B8(pixel);
                                            break;
                                        }
                                        default: {
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        case IMAGE_BPP_RGB565: {
                            for (int y = 0, yy = arg_model->height; y < yy; y++) {
                                uint16_t *row_ptr = IMAGE_COMPUTE_RGB565_PIXEL_ROW_PTR(arg_img, fast_floorf(y * over_yscale) + new_roi.y);
                                int row = arg_model->width * y;
                                for (int x = 0, xx = arg_model->width; x < xx; x++) {
                                    int pixel = IMAGE_GET_RGB565_PIXEL_FAST(row_ptr, fast_floorf(x * over_xscale) + new_roi.x);
                                    int index = row + x;
                                    switch (arg_model->channels) {
                                        case 1: {
                                            input_data[index] = COLOR_RGB565_TO_GRAYSCALE(pixel);
                                            break;
                                        }
                                        case 3: {
                                            int index_3 = index * 3;
                                            input_data[index_3 + 0] = COLOR_RGB565_TO_R8(pixel);
                                            input_data[index_3 + 1] = COLOR_RGB565_TO_G8(pixel);
                                            input_data[index_3 + 2] = COLOR_RGB565_TO_B8(pixel);
                                            break;
                                        }
                                        default: {
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        default: {
                            break;
                        }
                    }

                    if (arg_normalize) {
                        // https://github.com/tensorflow/tensorflow/blob/r1.11/tensorflow/python/ops/image_ops_impl.py#L1176
                        // https://arxiv.org/pdf/1803.08607.pdf
                        long long r_sum = 0, r_sum_2 = 0;
                        long long g_sum = 0, g_sum_2 = 0;
                        long long b_sum = 0, b_sum_2 = 0;
                        int r_max = 0, r_min = 255;
                        int g_max = 0, g_min = 255;
                        int b_max = 0, b_min = 255;

                        for (int y = 0, yy = arg_model->height; y < yy; y++) {
                            int row = arg_model->width * y;
                            for (int x = 0, xx = arg_model->width; x < xx; x++) {
                                int index = row + x;
                                switch (arg_model->channels) {
                                    case 1: {
                                        int g = input_data[index];
                                        g_sum += g;
                                        g_sum_2 += g * g;
                                        g_max = IM_MAX(g, g_max);
                                        g_min = IM_MIN(g, g_min);
                                        break;
                                    }
                                    case 3: {
                                        int index_3 = index * 3;
                                        int r = input_data[index_3 + 0];
                                        int g = input_data[index_3 + 1];
                                        int b = input_data[index_3 + 2];
                                        r_sum += r;
                                        r_sum_2 += r * r;
                                        r_max = IM_MAX(r, r_max);
                                        r_min = IM_MIN(r, r_min);
                                        g_sum += g;
                                        g_sum_2 += g * g;
                                        g_max = IM_MAX(g, g_max);
                                        g_min = IM_MIN(g, g_min);
                                        b_sum += b;
                                        b_sum_2 += b * b;
                                        b_max = IM_MAX(b, b_max);
                                        b_min = IM_MIN(b, b_min);
                                        break;
                                    }
                                    default: {
                                        break;
                                    }
                                }
                            }
                        }

                        int area = arg_model->width * arg_model->height;

                        int r_mean = IM_DIV(r_sum, area), r_variance = IM_DIV(r_sum_2, area) - (r_mean * r_mean);
                        int g_mean = IM_DIV(g_sum, area), g_variance = IM_DIV(g_sum_2, area) - (g_mean * g_mean);
                        int b_mean = IM_DIV(b_sum, area), b_variance = IM_DIV(b_sum_2, area) - (b_mean * b_mean);

                        float safe_r_stddev_inv = IM_DIV(1.0f, IM_MAX(fast_sqrtf(r_variance), IM_DIV(1.0f, fast_sqrtf(area))));
                        float safe_g_stddev_inv = IM_DIV(1.0f, IM_MAX(fast_sqrtf(g_variance), IM_DIV(1.0f, fast_sqrtf(area))));
                        float safe_b_stddev_inv = IM_DIV(1.0f, IM_MAX(fast_sqrtf(b_variance), IM_DIV(1.0f, fast_sqrtf(area))));

                        float delta_r = (r_max - r_min) / 255.0f, delta_2_r = IM_DIV(r_min, delta_r);
                        float delta_g = (g_max - g_min) / 255.0f, delta_2_g = IM_DIV(g_min, delta_g);
                        float delta_b = (b_max - b_min) / 255.0f, delta_2_b = IM_DIV(b_min, delta_b);

                        float safe_r_stddev_inv_times_delta_r_inv = safe_r_stddev_inv * IM_DIV(1.0f, delta_r); // delta_r_inv
                        float safe_g_stddev_inv_times_delta_g_inv = safe_g_stddev_inv * IM_DIV(1.0f, delta_g); // delta_g_inv
                        float safe_b_stddev_inv_times_delta_b_inv = safe_b_stddev_inv * IM_DIV(1.0f, delta_b); // delta_b_inv

                        for (int y = 0, yy = arg_model->height; y < yy; y++) {
                            int row = arg_model->width * y;
                            for (int x = 0, xx = arg_model->width; x < xx; x++) {
                                int index = row + x;
                                switch (arg_model->channels) {
                                    case 1: {
                                        input_data[index] = fast_roundf(((input_data[index] - g_mean) * safe_g_stddev_inv_times_delta_g_inv) - delta_2_g);
                                        break;
                                    }
                                    case 3: {
                                        int index_3 = index * 3;
                                        input_data[index_3 + 0] = fast_roundf(((input_data[index_3 + 0] - r_mean) * safe_r_stddev_inv_times_delta_r_inv) - delta_2_r);
                                        input_data[index_3 + 1] = fast_roundf(((input_data[index_3 + 1] - g_mean) * safe_g_stddev_inv_times_delta_g_inv) - delta_2_g);
                                        input_data[index_3 + 2] = fast_roundf(((input_data[index_3 + 2] - b_mean) * safe_b_stddev_inv_times_delta_b_inv) - delta_2_b);
                                        break;
                                    }
                                    default: {
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    PY_ASSERT_FALSE_MSG(libtf_run_classification(arg_model->model_data,
                                                                 tensor_arena,
                                                                 tensor_arena_size,
                                                                 input_data,
                                                                 arg_model->height,
                                                                 arg_model->width,
                                                                 arg_model->channels,
                                                                 class_scores,
                                                                 class_scores_size),
                                        "Model classification failed!");

                    int max_index = -1;
                    float max_value = -1.0f;
                    for (int i = 0; i < class_scores_size; i++) {
                        float value = class_scores[i];
                        if ((value >= arg_threshold) && (value > max_value)) {
                            max_index = i;
                            max_value = value;
                        }
                    }

                    if (max_index != -1) {
                        py_tf_class_obj_list_lnk_data_t lnk_data;
                        lnk_data.rect.x = new_roi.x;
                        lnk_data.rect.y = new_roi.y;
                        lnk_data.rect.w = new_roi.w;
                        lnk_data.rect.h = new_roi.h;
                        lnk_data.index = max_index;
                        lnk_data.value = max_value;
                        lnk_data.merge_number = 1;
                        lnk_data.value_acc = max_value;
                        list_push_back(&out, &lnk_data);
                    }
                }
            }
        }
    }

    fb_alloc_free_till_mark();

    // Merge all overlapping and same detections and average them.

    for (;;) {
        bool merge_occured = false;

        list_t out_temp;
        list_init(&out_temp, sizeof(py_tf_class_obj_list_lnk_data_t));

        while (list_size(&out)) {
            py_tf_class_obj_list_lnk_data_t lnk_data;
            list_pop_front(&out, &lnk_data);

            for (size_t k = 0, l = list_size(&out); k < l; k++) {
                py_tf_class_obj_list_lnk_data_t tmp_data;
                list_pop_front(&out, &tmp_data);

                if ((lnk_data.index == tmp_data.index)
                && rectangle_overlap(&(lnk_data.rect), &(tmp_data.rect))) {
                    // Merge rects...
                    rectangle_united(&(lnk_data.rect), &(tmp_data.rect));
                    // Merge counters...
                    lnk_data.merge_number += 1;
                    // Merge accumulators...
                    lnk_data.value_acc += tmp_data.value;
                    // Compute current values...
                    lnk_data.value = lnk_data.value_acc / lnk_data.merge_number;
                    merge_occured = true;
                } else {
                    list_push_back(&out, &tmp_data);
                }
            }

            list_push_back(&out_temp, &lnk_data);
        }

        list_copy(&out, &out_temp);

        if (!merge_occured) {
            break;
        }
    }

    // Determine the winner between overlapping different class detections.

    for (;;) {
        bool merge_occured = false;

        list_t out_temp;
        list_init(&out_temp, sizeof(py_tf_class_obj_list_lnk_data_t));

        while (list_size(&out)) {
            py_tf_class_obj_list_lnk_data_t lnk_data;
            list_pop_front(&out, &lnk_data);

            for (size_t k = 0, l = list_size(&out); k < l; k++) {
                py_tf_class_obj_list_lnk_data_t tmp_data;
                list_pop_front(&out, &tmp_data);

                if ((lnk_data.index != tmp_data.index)
                && rectangle_overlap(&(lnk_data.rect), &(tmp_data.rect))) {
                    if (tmp_data.value > lnk_data.value) {
                        memcpy(&lnk_data, &tmp_data, sizeof(py_tf_class_obj_list_lnk_data_t));
                    }

                    merge_occured = true;
                } else {
                    list_push_back(&out, &tmp_data);
                }
            }

            list_push_back(&out_temp, &lnk_data);
        }

        list_copy(&out, &out_temp);

        if (!merge_occured) {
            break;
        }
    }

    mp_obj_list_t *objects_list = mp_obj_new_list(list_size(&out), NULL);

    for (size_t i = 0; list_size(&out); i++) {
        py_tf_class_obj_list_lnk_data_t lnk_data;
        list_pop_front(&out, &lnk_data);

        py_tf_class_obj_t *o = m_new_obj(py_tf_class_obj_t);
        o->base.type = &py_tf_class_type;
        o->x = mp_obj_new_int(lnk_data.rect.x);
        o->y = mp_obj_new_int(lnk_data.rect.y);
        o->w = mp_obj_new_int(lnk_data.rect.w);
        o->h = mp_obj_new_int(lnk_data.rect.h);
        o->index = mp_obj_new_int(lnk_data.index);
        o->value = mp_obj_new_float(lnk_data.value);

        objects_list->items[i] = o;
    }

    return objects_list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(py_tf_classify_obj, 2, py_tf_classify);

mp_obj_t py_tf_len(mp_obj_t self_in) { return mp_obj_new_int(((py_tf_model_obj_t *) self_in)->model_data_len); }
mp_obj_t py_tf_height(mp_obj_t self_in) { return mp_obj_new_int(((py_tf_model_obj_t *) self_in)->height); }
mp_obj_t py_tf_width(mp_obj_t self_in) { return mp_obj_new_int(((py_tf_model_obj_t *) self_in)->width); }
mp_obj_t py_tf_channels(mp_obj_t self_in) { return mp_obj_new_int(((py_tf_model_obj_t *) self_in)->channels); }

STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_len_obj, py_tf_len);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_height_obj, py_tf_height);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_width_obj, py_tf_width);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_channels_obj, py_tf_channels);

STATIC const mp_rom_map_elem_t locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_len), MP_ROM_PTR(&py_tf_len_obj) },
    { MP_ROM_QSTR(MP_QSTR_height), MP_ROM_PTR(&py_tf_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_width), MP_ROM_PTR(&py_tf_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_channels), MP_ROM_PTR(&py_tf_channels_obj) },
    { MP_ROM_QSTR(MP_QSTR_classify), MP_ROM_PTR(&py_tf_classify_obj) }
};

STATIC MP_DEFINE_CONST_DICT(locals_dict, locals_dict_table);

STATIC const mp_obj_type_t py_tf_model_type = {
    { &mp_type_type },
    .name  = MP_QSTR_tf_model,
    .print = py_tf_model_print,
    .locals_dict = (mp_obj_t) &locals_dict
};

STATIC mp_obj_t py_tf_load(mp_obj_t path_obj)
{
    const char *path = mp_obj_str_get_str(path_obj);
    py_tf_model_obj_t *tf_model = m_new_obj(py_tf_model_obj_t);
    tf_model->base.type = &py_tf_model_type;

    if (!strcmp(path, "mobilenet")) {
        tf_model->model_data = (unsigned char *) mobilenet_model_data;
        tf_model->model_data_len = mobilenet_model_data_len;
    } else {
        FIL fp;
        file_read_open(&fp, path);
        tf_model->model_data_len = f_size(&fp);
        tf_model->model_data = xalloc(tf_model->model_data_len);
        read_data(&fp, tf_model->model_data, tf_model->model_data_len);
        file_close(&fp);
    }

    fb_alloc_mark();
    uint32_t tensor_arena_size;
    uint8_t *tensor_arena = fb_alloc_all(&tensor_arena_size);

    PY_ASSERT_FALSE_MSG(libtf_get_input_data_hwc(tf_model->model_data,
                                                 tensor_arena,
                                                 tensor_arena_size,
                                                 &tf_model->height,
                                                 &tf_model->width,
                                                 &tf_model->channels),
                        "Unable to read model height, width, and channels!");

    fb_alloc_free_till_mark();

    return tf_model;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(py_tf_load_obj, py_tf_load);

#endif // IMLIB_ENABLE_TF

STATIC const mp_rom_map_elem_t globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_tf) },
#ifdef IMLIB_ENABLE_TF
    { MP_ROM_QSTR(MP_QSTR_load),     MP_ROM_PTR(&py_tf_load_obj) },
    { MP_ROM_QSTR(MP_QSTR_classify), MP_ROM_PTR(&py_tf_classify_obj) },
#else
    { MP_ROM_QSTR(MP_QSTR_load),     MP_ROM_PTR(&py_func_unavailable_obj) },
    { MP_ROM_QSTR(MP_QSTR_classify), MP_ROM_PTR(&py_func_unavailable_obj) }
#endif // IMLIB_ENABLE_TF
};

STATIC MP_DEFINE_CONST_DICT(globals_dict, globals_dict_table);

const mp_obj_module_t tf_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_t) &globals_dict
};
