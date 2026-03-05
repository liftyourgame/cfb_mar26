#ifndef U8G2_ESP32_HAL_H
#define U8G2_ESP32_HAL_H

#include <stdint.h>
#include <stdbool.h>

void oled_init_hardware_i2c_for_esp32c3_with_ssd1306_on_gpio5_and_gpio6(void);
void oled_clear_entire_display_buffer_to_black(void);
void oled_draw_text_using_small_6x8_font_at_position(int x_coordinate_in_pixels, int y_coordinate_in_pixels, const char *text_string_to_display);
void oled_draw_text_using_large_12x16_font_at_position(int x_coordinate_in_pixels, int y_coordinate_in_pixels, const char *text_string_to_display);
void oled_draw_horizontal_progress_bar_with_percentage(int x_coordinate_in_pixels, int y_coordinate_in_pixels, int width_in_pixels, int height_in_pixels, int percentage_filled_0_to_100);
void oled_send_buffer_to_physical_display_hardware(void);
void oled_set_display_brightness_contrast_level(uint8_t contrast_value_0_to_255);

#endif
