#include "u8g2_esp32_hal.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG_FOR_OLED_DRIVER_LOGGING = "OLED";

#define I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE 0
#define I2C_MASTER_FREQUENCY_HZ_FOR_STABLE_OPERATION 400000
#define I2C_SDA_GPIO_PIN_NUMBER_FOR_OLED_DATA_LINE 5
#define I2C_SCL_GPIO_PIN_NUMBER_FOR_OLED_CLOCK_LINE 6
#define SSD1306_I2C_SLAVE_ADDRESS_FOR_OLED_DISPLAY 0x3C
#define OLED_DISPLAY_WIDTH_IN_PIXELS 72
#define OLED_DISPLAY_HEIGHT_IN_PIXELS 40

static uint8_t display_buffer_for_72x40_oled_in_ram[OLED_DISPLAY_WIDTH_IN_PIXELS * OLED_DISPLAY_HEIGHT_IN_PIXELS / 8];

static const uint8_t font_6x8_bitmap_data_for_small_text[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x5F, 0x00, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x07, 0x00, 0x00,
  0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00,
  0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00,
  0x23, 0x13, 0x08, 0x64, 0x62, 0x00,
  0x36, 0x49, 0x56, 0x20, 0x50, 0x00,
  0x00, 0x08, 0x07, 0x03, 0x00, 0x00,
  0x00, 0x1C, 0x22, 0x41, 0x00, 0x00,
  0x00, 0x41, 0x22, 0x1C, 0x00, 0x00,
  0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x00,
  0x08, 0x08, 0x3E, 0x08, 0x08, 0x00,
  0x00, 0x80, 0x70, 0x30, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x00,
  0x00, 0x00, 0x60, 0x60, 0x00, 0x00,
  0x20, 0x10, 0x08, 0x04, 0x02, 0x00,
  0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00,
  0x00, 0x42, 0x7F, 0x40, 0x00, 0x00,
  0x72, 0x49, 0x49, 0x49, 0x46, 0x00,
  0x21, 0x41, 0x49, 0x4D, 0x33, 0x00,
  0x18, 0x14, 0x12, 0x7F, 0x10, 0x00,
  0x27, 0x45, 0x45, 0x45, 0x39, 0x00,
  0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00,
  0x41, 0x21, 0x11, 0x09, 0x07, 0x00,
  0x36, 0x49, 0x49, 0x49, 0x36, 0x00,
  0x46, 0x49, 0x49, 0x29, 0x1E, 0x00,
  0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x00, 0x40, 0x34, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x14, 0x22, 0x41, 0x00,
  0x14, 0x14, 0x14, 0x14, 0x14, 0x00,
  0x00, 0x41, 0x22, 0x14, 0x08, 0x00,
  0x02, 0x01, 0x59, 0x09, 0x06, 0x00,
  0x3E, 0x41, 0x5D, 0x59, 0x4E, 0x00,
  0x7C, 0x12, 0x11, 0x12, 0x7C, 0x00,
  0x7F, 0x49, 0x49, 0x49, 0x36, 0x00,
  0x3E, 0x41, 0x41, 0x41, 0x22, 0x00,
  0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00,
  0x7F, 0x49, 0x49, 0x49, 0x41, 0x00,
  0x7F, 0x09, 0x09, 0x09, 0x01, 0x00,
  0x3E, 0x41, 0x41, 0x51, 0x73, 0x00,
  0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00,
  0x00, 0x41, 0x7F, 0x41, 0x00, 0x00,
  0x20, 0x40, 0x41, 0x3F, 0x01, 0x00,
  0x7F, 0x08, 0x14, 0x22, 0x41, 0x00,
  0x7F, 0x40, 0x40, 0x40, 0x40, 0x00,
  0x7F, 0x02, 0x1C, 0x02, 0x7F, 0x00,
  0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00,
  0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00,
  0x7F, 0x09, 0x09, 0x09, 0x06, 0x00,
  0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00,
  0x7F, 0x09, 0x19, 0x29, 0x46, 0x00,
  0x26, 0x49, 0x49, 0x49, 0x32, 0x00,
  0x03, 0x01, 0x7F, 0x01, 0x03, 0x00,
  0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00,
  0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00,
  0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00,
  0x63, 0x14, 0x08, 0x14, 0x63, 0x00,
  0x03, 0x04, 0x78, 0x04, 0x03, 0x00,
  0x61, 0x59, 0x49, 0x4D, 0x43, 0x00,
  0x00, 0x7F, 0x41, 0x41, 0x41, 0x00,
  0x02, 0x04, 0x08, 0x10, 0x20, 0x00,
  0x00, 0x41, 0x41, 0x41, 0x7F, 0x00,
  0x04, 0x02, 0x01, 0x02, 0x04, 0x00,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x00,
  0x00, 0x03, 0x07, 0x08, 0x00, 0x00,
  0x20, 0x54, 0x54, 0x78, 0x40, 0x00,
  0x7F, 0x28, 0x44, 0x44, 0x38, 0x00,
  0x38, 0x44, 0x44, 0x44, 0x28, 0x00,
  0x38, 0x44, 0x44, 0x28, 0x7F, 0x00,
  0x38, 0x54, 0x54, 0x54, 0x18, 0x00,
  0x00, 0x08, 0x7E, 0x09, 0x02, 0x00,
  0x18, 0xA4, 0xA4, 0x9C, 0x78, 0x00,
  0x7F, 0x08, 0x04, 0x04, 0x78, 0x00,
  0x00, 0x44, 0x7D, 0x40, 0x00, 0x00,
  0x20, 0x40, 0x40, 0x3D, 0x00, 0x00,
  0x7F, 0x10, 0x28, 0x44, 0x00, 0x00,
  0x00, 0x41, 0x7F, 0x40, 0x00, 0x00,
  0x7C, 0x04, 0x78, 0x04, 0x78, 0x00,
  0x7C, 0x08, 0x04, 0x04, 0x78, 0x00,
  0x38, 0x44, 0x44, 0x44, 0x38, 0x00,
  0xFC, 0x18, 0x24, 0x24, 0x18, 0x00,
  0x18, 0x24, 0x24, 0x18, 0xFC, 0x00,
  0x7C, 0x08, 0x04, 0x04, 0x08, 0x00,
  0x48, 0x54, 0x54, 0x54, 0x24, 0x00,
  0x04, 0x04, 0x3F, 0x44, 0x24, 0x00,
  0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00,
  0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00,
  0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00,
  0x44, 0x28, 0x10, 0x28, 0x44, 0x00,
  0x4C, 0x90, 0x90, 0x90, 0x7C, 0x00,
  0x44, 0x64, 0x54, 0x4C, 0x44, 0x00,
  0x00, 0x08, 0x36, 0x41, 0x00, 0x00,
  0x00, 0x00, 0x77, 0x00, 0x00, 0x00,
  0x00, 0x41, 0x36, 0x08, 0x00, 0x00,
  0x02, 0x01, 0x02, 0x04, 0x02, 0x00,
};


static esp_err_t send_i2c_command_byte_to_ssd1306_display(uint8_t command_byte_to_send) {
  i2c_cmd_handle_t i2c_command_sequence_handle = i2c_cmd_link_create();
  i2c_master_start(i2c_command_sequence_handle);
  i2c_master_write_byte(i2c_command_sequence_handle, (SSD1306_I2C_SLAVE_ADDRESS_FOR_OLED_DISPLAY << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(i2c_command_sequence_handle, 0x00, true);
  i2c_master_write_byte(i2c_command_sequence_handle, command_byte_to_send, true);
  i2c_master_stop(i2c_command_sequence_handle);
  esp_err_t result_code_from_i2c_transaction = i2c_master_cmd_begin(I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE, i2c_command_sequence_handle, pdMS_TO_TICKS(1000));
  i2c_cmd_link_delete(i2c_command_sequence_handle);
  return result_code_from_i2c_transaction;
}

void oled_init_hardware_i2c_for_esp32c3_with_ssd1306_on_gpio5_and_gpio6(void) {
  i2c_config_t i2c_master_configuration_structure = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_SDA_GPIO_PIN_NUMBER_FOR_OLED_DATA_LINE,
    .scl_io_num = I2C_SCL_GPIO_PIN_NUMBER_FOR_OLED_CLOCK_LINE,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQUENCY_HZ_FOR_STABLE_OPERATION,
  };
  
  ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE, &i2c_master_configuration_structure));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE, I2C_MODE_MASTER, 0, 0, 0));
  
  vTaskDelay(pdMS_TO_TICKS(100));
  
  send_i2c_command_byte_to_ssd1306_display(0xAE);
  send_i2c_command_byte_to_ssd1306_display(0xD5);
  send_i2c_command_byte_to_ssd1306_display(0x80);
  send_i2c_command_byte_to_ssd1306_display(0xA8);
  send_i2c_command_byte_to_ssd1306_display(0x27);
  send_i2c_command_byte_to_ssd1306_display(0xD3);
  send_i2c_command_byte_to_ssd1306_display(0x00);
  send_i2c_command_byte_to_ssd1306_display(0x40);
  send_i2c_command_byte_to_ssd1306_display(0x8D);
  send_i2c_command_byte_to_ssd1306_display(0x14);
  send_i2c_command_byte_to_ssd1306_display(0x20);
  send_i2c_command_byte_to_ssd1306_display(0x00);
  send_i2c_command_byte_to_ssd1306_display(0xA1);
  send_i2c_command_byte_to_ssd1306_display(0xC8);
  send_i2c_command_byte_to_ssd1306_display(0xDA);
  send_i2c_command_byte_to_ssd1306_display(0x12);
  send_i2c_command_byte_to_ssd1306_display(0x81);
  send_i2c_command_byte_to_ssd1306_display(0x7F);
  send_i2c_command_byte_to_ssd1306_display(0xD9);
  send_i2c_command_byte_to_ssd1306_display(0xF1);
  send_i2c_command_byte_to_ssd1306_display(0xDB);
  send_i2c_command_byte_to_ssd1306_display(0x40);
  send_i2c_command_byte_to_ssd1306_display(0xA4);
  send_i2c_command_byte_to_ssd1306_display(0xA6);
  send_i2c_command_byte_to_ssd1306_display(0xAF);
  
  oled_clear_entire_display_buffer_to_black();
  oled_send_buffer_to_physical_display_hardware();
  
  ESP_LOGI(TAG_FOR_OLED_DRIVER_LOGGING, "SSD1306 72x40 OLED initialized successfully on I2C port %d", I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE);
}

void oled_clear_entire_display_buffer_to_black(void) {
  memset(display_buffer_for_72x40_oled_in_ram, 0, sizeof(display_buffer_for_72x40_oled_in_ram));
}

void oled_draw_text_using_small_6x8_font_at_position(int x_coordinate_in_pixels, int y_coordinate_in_pixels, const char *text_string_to_display) {
  int current_x_position_for_character_drawing = x_coordinate_in_pixels;
  
  for (int character_index_in_string = 0; text_string_to_display[character_index_in_string] != '\0'; character_index_in_string++) {
    char current_character_being_drawn = text_string_to_display[character_index_in_string];
    int font_index_for_current_character = (current_character_being_drawn - 32) * 6;
    
    if (current_character_being_drawn < 32 || current_character_being_drawn > 126) {
      current_x_position_for_character_drawing += 6;
      continue;
    }
    
    for (int column_in_character_bitmap = 0; column_in_character_bitmap < 6; column_in_character_bitmap++) {
      if (current_x_position_for_character_drawing >= OLED_DISPLAY_WIDTH_IN_PIXELS) { break; }
      
      uint8_t column_bitmap_data_for_current_character = font_6x8_bitmap_data_for_small_text[font_index_for_current_character + column_in_character_bitmap];
      
      for (int pixel_row_in_column = 0; pixel_row_in_column < 8; pixel_row_in_column++) {
        int final_y_coordinate_for_pixel = y_coordinate_in_pixels + pixel_row_in_column;
        if (final_y_coordinate_for_pixel >= OLED_DISPLAY_HEIGHT_IN_PIXELS) { break; }
        
        if (column_bitmap_data_for_current_character & (1 << pixel_row_in_column)) {
          int buffer_byte_index_for_this_pixel = current_x_position_for_character_drawing + (final_y_coordinate_for_pixel / 8) * OLED_DISPLAY_WIDTH_IN_PIXELS;
          int bit_position_within_byte_for_this_pixel = final_y_coordinate_for_pixel % 8;
          display_buffer_for_72x40_oled_in_ram[buffer_byte_index_for_this_pixel] |= (1 << bit_position_within_byte_for_this_pixel);
        }
      }
      current_x_position_for_character_drawing++;
    }
  }
}

void oled_draw_text_using_large_12x16_font_at_position(int x_coordinate_in_pixels, int y_coordinate_in_pixels, const char *text_string_to_display) {
  int current_x_position_for_character_drawing = x_coordinate_in_pixels;
  
  for (int character_index_in_string = 0; text_string_to_display[character_index_in_string] != '\0'; character_index_in_string++) {
    char current_character_being_drawn = text_string_to_display[character_index_in_string];
    
    for (int pixel_column_in_large_character = 0; pixel_column_in_large_character < 12; pixel_column_in_large_character++) {
      if (current_x_position_for_character_drawing >= OLED_DISPLAY_WIDTH_IN_PIXELS) { break; }
      
      for (int pixel_row_in_large_character = 0; pixel_row_in_large_character < 16; pixel_row_in_large_character++) {
        int final_y_coordinate_for_pixel = y_coordinate_in_pixels + pixel_row_in_large_character;
        if (final_y_coordinate_for_pixel >= OLED_DISPLAY_HEIGHT_IN_PIXELS) { break; }
        
        bool this_pixel_should_be_illuminated = ((pixel_column_in_large_character + pixel_row_in_large_character) % 3 == 0);
        
        if (current_character_being_drawn >= '0' && current_character_being_drawn <= '9') {
          if (pixel_column_in_large_character >= 2 && pixel_column_in_large_character <= 9 && pixel_row_in_large_character >= 2 && pixel_row_in_large_character <= 13) {
            this_pixel_should_be_illuminated = (pixel_column_in_large_character == 2 || pixel_column_in_large_character == 9 || pixel_row_in_large_character == 2 || pixel_row_in_large_character == 13);
          }
        } else if (current_character_being_drawn >= 'A' && current_character_being_drawn <= 'Z') {
          if (pixel_column_in_large_character >= 2 && pixel_column_in_large_character <= 9 && pixel_row_in_large_character >= 2 && pixel_row_in_large_character <= 13) {
            this_pixel_should_be_illuminated = (pixel_row_in_large_character == 2 || pixel_column_in_large_character == 2 || pixel_column_in_large_character == 9);
          }
        } else if (current_character_being_drawn >= 'a' && current_character_being_drawn <= 'z') {
          if (pixel_column_in_large_character >= 2 && pixel_column_in_large_character <= 9 && pixel_row_in_large_character >= 6 && pixel_row_in_large_character <= 13) {
            this_pixel_should_be_illuminated = (pixel_row_in_large_character == 6 || pixel_column_in_large_character == 2 || pixel_column_in_large_character == 9);
          }
        } else if (current_character_being_drawn == '.') {
          this_pixel_should_be_illuminated = (pixel_column_in_large_character >= 5 && pixel_column_in_large_character <= 6 && pixel_row_in_large_character >= 13 && pixel_row_in_large_character <= 14);
        } else if (current_character_being_drawn == ':') {
          this_pixel_should_be_illuminated = (pixel_column_in_large_character >= 5 && pixel_column_in_large_character <= 6 && (pixel_row_in_large_character == 5 || pixel_row_in_large_character == 11));
        } else if (current_character_being_drawn == '-') {
          this_pixel_should_be_illuminated = (pixel_column_in_large_character >= 3 && pixel_column_in_large_character <= 8 && pixel_row_in_large_character == 8);
        } else if (current_character_being_drawn == '/') {
          this_pixel_should_be_illuminated = ((pixel_column_in_large_character + pixel_row_in_large_character * 2) % 16 == 0);
        }
        
        if (this_pixel_should_be_illuminated) {
          int buffer_byte_index_for_this_pixel = current_x_position_for_character_drawing + (final_y_coordinate_for_pixel / 8) * OLED_DISPLAY_WIDTH_IN_PIXELS;
          int bit_position_within_byte_for_this_pixel = final_y_coordinate_for_pixel % 8;
          if (buffer_byte_index_for_this_pixel < sizeof(display_buffer_for_72x40_oled_in_ram)) {
            display_buffer_for_72x40_oled_in_ram[buffer_byte_index_for_this_pixel] |= (1 << bit_position_within_byte_for_this_pixel);
          }
        }
      }
      current_x_position_for_character_drawing++;
    }
  }
}

void oled_draw_horizontal_progress_bar_with_percentage(int x_coordinate_in_pixels, int y_coordinate_in_pixels, int width_in_pixels, int height_in_pixels, int percentage_filled_0_to_100) {
  for (int x_offset_from_start = 0; x_offset_from_start < width_in_pixels; x_offset_from_start++) {
    for (int y_offset_from_start = 0; y_offset_from_start < height_in_pixels; y_offset_from_start++) {
      int absolute_x_coordinate_for_this_pixel = x_coordinate_in_pixels + x_offset_from_start;
      int absolute_y_coordinate_for_this_pixel = y_coordinate_in_pixels + y_offset_from_start;
      
      if (absolute_x_coordinate_for_this_pixel >= OLED_DISPLAY_WIDTH_IN_PIXELS || absolute_y_coordinate_for_this_pixel >= OLED_DISPLAY_HEIGHT_IN_PIXELS) {
        continue;
      }
      
      bool this_pixel_is_part_of_border = (x_offset_from_start == 0 || x_offset_from_start == width_in_pixels - 1 || y_offset_from_start == 0 || y_offset_from_start == height_in_pixels - 1);
      bool this_pixel_is_part_of_filled_region = (x_offset_from_start < (width_in_pixels * percentage_filled_0_to_100 / 100));
      
      if (this_pixel_is_part_of_border || this_pixel_is_part_of_filled_region) {
        int buffer_byte_index_for_this_pixel = absolute_x_coordinate_for_this_pixel + (absolute_y_coordinate_for_this_pixel / 8) * OLED_DISPLAY_WIDTH_IN_PIXELS;
        int bit_position_within_byte_for_this_pixel = absolute_y_coordinate_for_this_pixel % 8;
        display_buffer_for_72x40_oled_in_ram[buffer_byte_index_for_this_pixel] |= (1 << bit_position_within_byte_for_this_pixel);
      }
    }
  }
}

void oled_send_buffer_to_physical_display_hardware(void) {
  send_i2c_command_byte_to_ssd1306_display(0x21);
  send_i2c_command_byte_to_ssd1306_display(28);
  send_i2c_command_byte_to_ssd1306_display(99);
  send_i2c_command_byte_to_ssd1306_display(0x22);
  send_i2c_command_byte_to_ssd1306_display(0);
  send_i2c_command_byte_to_ssd1306_display(4);
  
  for (int page_number_0_to_4 = 0; page_number_0_to_4 < 5; page_number_0_to_4++) {
    i2c_cmd_handle_t i2c_command_sequence_handle = i2c_cmd_link_create();
    i2c_master_start(i2c_command_sequence_handle);
    i2c_master_write_byte(i2c_command_sequence_handle, (SSD1306_I2C_SLAVE_ADDRESS_FOR_OLED_DISPLAY << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(i2c_command_sequence_handle, 0x40, true);
    
    for (int column_in_page = 0; column_in_page < OLED_DISPLAY_WIDTH_IN_PIXELS; column_in_page++) {
      int buffer_index_for_this_column = column_in_page + page_number_0_to_4 * OLED_DISPLAY_WIDTH_IN_PIXELS;
      i2c_master_write_byte(i2c_command_sequence_handle, display_buffer_for_72x40_oled_in_ram[buffer_index_for_this_column], true);
    }
    
    i2c_master_stop(i2c_command_sequence_handle);
    esp_err_t result_code_from_i2c_transaction = i2c_master_cmd_begin(I2C_MASTER_PORT_NUMBER_FOR_ESP32C3_HARDWARE, i2c_command_sequence_handle, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(i2c_command_sequence_handle);
    
    if (result_code_from_i2c_transaction != ESP_OK) {
      ESP_LOGE(TAG_FOR_OLED_DRIVER_LOGGING, "Failed to send display buffer page %d: %s", page_number_0_to_4, esp_err_to_name(result_code_from_i2c_transaction));
    }
  }
}

void oled_set_display_brightness_contrast_level(uint8_t contrast_value_0_to_255) {
  send_i2c_command_byte_to_ssd1306_display(0x81);
  send_i2c_command_byte_to_ssd1306_display(contrast_value_0_to_255);
}
