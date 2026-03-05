#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "driver/gpio.h"
#include "u8g2_esp32_hal.h"

#define WIFI_NETWORK_1_SSID_FOR_WORKSHOP_VENUE "cfb\0"
#define WIFI_NETWORK_1_PASSWORD_FOR_WORKSHOP_VENUE "cfb_1958!\0"
#define WIFI_NETWORK_2_SSID_FOR_TESTING_AND_HOME "(your_wifi_ssid_here)\0"
#define WIFI_NETWORK_2_PASSWORD_FOR_TESTING_AND_HOME "(your_wifi_password_here)\0"
#define WIFI_NETWORK_3_SSID_PLACEHOLDER_FOR_USER_CUSTOMIZATION "NO_SUCH_WIFI_PLACEHOLDER_ABCDEFGHIJKLMNOPQRSTUVWXYZ\0"
#define WIFI_NETWORK_3_PASSWORD_PLACEHOLDER_FOR_USER_CUSTOMIZATION "NO_SUCH_WIFI_PASSWORD_ABCDEFGHIJKLMNOPQRSTUVWXYZ\0"

#define GPIO_PIN_NUMBER_FOR_BLUE_LED_INDICATOR 8
#define WIFI_CONNECTION_ESTABLISHED_BIT BIT0
#define WIFI_CONNECTION_FAILED_PERMANENTLY_BIT BIT1
#define MAXIMUM_WIFI_CONNECTION_RETRY_ATTEMPTS 10

#define OLED_DISPLAY_WIDTH_IN_PIXELS 72
#define OLED_DISPLAY_HEIGHT_IN_PIXELS 40

static const char *TAG_FOR_MAIN_APPLICATION_LOGGING = "WORKSHOP";
static EventGroupHandle_t wifi_connection_status_event_group_handle;
static httpd_handle_t http_server_handle_for_ota_and_info = NULL;
static char device_hostname_derived_from_mac_address[32];
static char device_ip_address_as_string[16];
static char current_wifi_ssid_connected_to[33];
static int8_t current_wifi_signal_strength_in_dbm = 0;
static int wifi_signal_strength_as_percentage_0_to_100 = 0;

static void configure_led_gpio_pin_as_output_for_status_indication(void) {
  gpio_config_t led_gpio_configuration_structure = {
    .pin_bit_mask = (1ULL << GPIO_PIN_NUMBER_FOR_BLUE_LED_INDICATOR),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&led_gpio_configuration_structure);
  gpio_set_level(GPIO_PIN_NUMBER_FOR_BLUE_LED_INDICATOR, 1);
}

static void set_led_state_on_or_off(bool turn_led_on_when_true) {
  gpio_set_level(GPIO_PIN_NUMBER_FOR_BLUE_LED_INDICATOR, turn_led_on_when_true ? 0 : 1);
}

static void wifi_event_handler_callback_for_connection_management(void* handler_argument, esp_event_base_t event_base_type, int32_t event_id_number, void* event_data_pointer) {
  static int retry_attempt_counter_for_current_network = 0;
  
  if (event_base_type == WIFI_EVENT && event_id_number == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi station started, attempting connection...");
  } else if (event_base_type == WIFI_EVENT && event_id_number == WIFI_EVENT_STA_DISCONNECTED) {
    if (retry_attempt_counter_for_current_network < MAXIMUM_WIFI_CONNECTION_RETRY_ATTEMPTS) {
      esp_wifi_connect();
      retry_attempt_counter_for_current_network++;
      ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Retrying WiFi connection, attempt %d/%d", retry_attempt_counter_for_current_network, MAXIMUM_WIFI_CONNECTION_RETRY_ATTEMPTS);
    } else {
      xEventGroupSetBits(wifi_connection_status_event_group_handle, WIFI_CONNECTION_FAILED_PERMANENTLY_BIT);
      ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi connection failed after %d attempts", MAXIMUM_WIFI_CONNECTION_RETRY_ATTEMPTS);
    }
  } else if (event_base_type == IP_EVENT && event_id_number == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* ip_event_data_structure = (ip_event_got_ip_t*) event_data_pointer;
    snprintf(device_ip_address_as_string, sizeof(device_ip_address_as_string), IPSTR, IP2STR(&ip_event_data_structure->ip_info.ip));
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi connected! IP address: %s", device_ip_address_as_string);
    retry_attempt_counter_for_current_network = 0;
    xEventGroupSetBits(wifi_connection_status_event_group_handle, WIFI_CONNECTION_ESTABLISHED_BIT);
  }
}

static bool attempt_wifi_connection_to_specific_network(const char *ssid_to_connect_to, const char *password_for_network) {
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Attempting to connect to WiFi network: %s", ssid_to_connect_to);
  
  wifi_config_t wifi_station_configuration_structure = {
    .sta = {
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
    },
  };
  
  strncpy((char *)wifi_station_configuration_structure.sta.ssid, ssid_to_connect_to, sizeof(wifi_station_configuration_structure.sta.ssid) - 1);
  strncpy((char *)wifi_station_configuration_structure.sta.password, password_for_network, sizeof(wifi_station_configuration_structure.sta.password) - 1);
  strncpy(current_wifi_ssid_connected_to, ssid_to_connect_to, sizeof(current_wifi_ssid_connected_to) - 1);
  
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_station_configuration_structure));
  ESP_ERROR_CHECK(esp_wifi_start());
  
  EventBits_t connection_result_bits = xEventGroupWaitBits(wifi_connection_status_event_group_handle, WIFI_CONNECTION_ESTABLISHED_BIT | WIFI_CONNECTION_FAILED_PERMANENTLY_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
  
  if (connection_result_bits & WIFI_CONNECTION_ESTABLISHED_BIT) {
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Successfully connected to %s", ssid_to_connect_to);
    return true;
  } else {
    ESP_LOGW(TAG_FOR_MAIN_APPLICATION_LOGGING, "Failed to connect to %s", ssid_to_connect_to);
    esp_wifi_stop();
    return false;
  }
}

static void initialize_wifi_with_multi_network_fallback_support(void) {
  wifi_connection_status_event_group_handle = xEventGroupCreate();
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Initializing WiFi subsystem...");
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  
  wifi_init_config_t wifi_initialization_configuration = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_initialization_configuration));
  
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_callback_for_connection_management, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_callback_for_connection_management, NULL));
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Starting WiFi for scanning (not connecting yet)...");
  
  wifi_config_t empty_wifi_config = {0};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &empty_wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  
  vTaskDelay(pdMS_TO_TICKS(500));
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Scanning for available WiFi networks...");
  wifi_scan_config_t scan_configuration = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = false
  };
  
  esp_err_t scan_start_result = esp_wifi_scan_start(&scan_configuration, true);
  if (scan_start_result != ESP_OK) {
    ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi scan failed with error: %s", esp_err_to_name(scan_start_result));
    strcpy(device_ip_address_as_string, "NO WIFI");
    strcpy(current_wifi_ssid_connected_to, "NONE");
    return;
  }
  
  uint16_t number_of_access_points_found = 0;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&number_of_access_points_found));
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi scan complete - found %d networks", number_of_access_points_found);
  
  if (number_of_access_points_found > 0) {
    wifi_ap_record_t *list_of_scanned_access_points = malloc(sizeof(wifi_ap_record_t) * number_of_access_points_found);
    if (list_of_scanned_access_points == NULL) {
      ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "Failed to allocate memory for scan results!");
      strcpy(device_ip_address_as_string, "NO WIFI");
      strcpy(current_wifi_ssid_connected_to, "NONE");
      return;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number_of_access_points_found, list_of_scanned_access_points));
    
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Available networks:");
    for (int i = 0; i < number_of_access_points_found && i < 10; i++) {
      ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "  [%d] %s (RSSI: %d, Ch: %d)", i + 1, list_of_scanned_access_points[i].ssid, list_of_scanned_access_points[i].rssi, list_of_scanned_access_points[i].primary);
    }
    
    typedef struct {
      const char *ssid_to_match;
      const char *password_for_network;
    } known_network_credentials_structure;
    
    known_network_credentials_structure known_networks_array[] = {
      {WIFI_NETWORK_1_SSID_FOR_WORKSHOP_VENUE, WIFI_NETWORK_1_PASSWORD_FOR_WORKSHOP_VENUE},
      {WIFI_NETWORK_2_SSID_FOR_TESTING_AND_HOME, WIFI_NETWORK_2_PASSWORD_FOR_TESTING_AND_HOME},
      {WIFI_NETWORK_3_SSID_PLACEHOLDER_FOR_USER_CUSTOMIZATION, WIFI_NETWORK_3_PASSWORD_PLACEHOLDER_FOR_USER_CUSTOMIZATION}
    };
    
    bool connection_successful = false;
    
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Checking for known networks...");
    for (int known_network_index = 0; known_network_index < sizeof(known_networks_array) / sizeof(known_networks_array[0]); known_network_index++) {
      ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Looking for: %s", known_networks_array[known_network_index].ssid_to_match);
      
      for (int scanned_ap_index = 0; scanned_ap_index < number_of_access_points_found; scanned_ap_index++) {
        if (strcmp((char *)list_of_scanned_access_points[scanned_ap_index].ssid, known_networks_array[known_network_index].ssid_to_match) == 0) {
          ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "MATCH! Attempting to connect to: %s (RSSI: %d)", list_of_scanned_access_points[scanned_ap_index].ssid, list_of_scanned_access_points[scanned_ap_index].rssi);
          
          ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Stopping WiFi before connection attempt...");
          ESP_ERROR_CHECK(esp_wifi_stop());
          
          if (attempt_wifi_connection_to_specific_network(known_networks_array[known_network_index].ssid_to_match, known_networks_array[known_network_index].password_for_network)) {
            ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Successfully connected!");
            connection_successful = true;
            break;
          } else {
            ESP_LOGW(TAG_FOR_MAIN_APPLICATION_LOGGING, "Connection attempt failed, continuing search...");
          }
        }
      }
      if (connection_successful) { break; }
    }
    
    free(list_of_scanned_access_points);
    
    if (!connection_successful) {
      ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "None of the known WiFi networks are available or connection failed!");
      strcpy(device_ip_address_as_string, "NO WIFI");
      strcpy(current_wifi_ssid_connected_to, "NONE");
    }
  } else {
    ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "No WiFi networks found in scan!");
    strcpy(device_ip_address_as_string, "NO WIFI");
    strcpy(current_wifi_ssid_connected_to, "NONE");
  }
}

static void initialize_mdns_service_with_unique_hostname_from_mac(void) {
  uint8_t mac_address_bytes_array[6];
  esp_read_mac(mac_address_bytes_array, ESP_MAC_WIFI_STA);
  snprintf(device_hostname_derived_from_mac_address, sizeof(device_hostname_derived_from_mac_address), "esp32-%02x%02x%02x", mac_address_bytes_array[3], mac_address_bytes_array[4], mac_address_bytes_array[5]);
  
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(device_hostname_derived_from_mac_address));
  ESP_ERROR_CHECK(mdns_instance_name_set("ESP32-C3 Workshop Board"));
  
  ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
  ESP_ERROR_CHECK(mdns_service_add(NULL, "_arduino", "_tcp", 80, NULL, 0));
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "mDNS service started: %s.local", device_hostname_derived_from_mac_address);
}

static void update_wifi_signal_strength_measurement(void) {
  wifi_ap_record_t access_point_information_structure;
  if (esp_wifi_sta_get_ap_info(&access_point_information_structure) == ESP_OK) {
    current_wifi_signal_strength_in_dbm = access_point_information_structure.rssi;
    
    if (current_wifi_signal_strength_in_dbm >= -50) {
      wifi_signal_strength_as_percentage_0_to_100 = 100;
    } else if (current_wifi_signal_strength_in_dbm <= -100) {
      wifi_signal_strength_as_percentage_0_to_100 = 0;
    } else {
      wifi_signal_strength_as_percentage_0_to_100 = 2 * (current_wifi_signal_strength_in_dbm + 100);
    }
  }
}

static esp_err_t http_get_handler_for_root_page_showing_device_info(httpd_req_t *http_request_structure) {
  char html_response_buffer_for_device_information[2048];
  
  snprintf(html_response_buffer_for_device_information, sizeof(html_response_buffer_for_device_information),
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32-C3 Workshop Board</title>"
    "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}h1{color:#333}.info{background:white;padding:15px;margin:10px 0;border-radius:5px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}.label{font-weight:bold;color:#666}.value{color:#000;font-size:1.2em}a{display:inline-block;margin:10px 0;padding:10px 20px;background:#007bff;color:white;text-decoration:none;border-radius:5px}a:hover{background:#0056b3}</style>"
    "<meta http-equiv='refresh' content='5'></head><body>"
    "<h1>ESP32-C3 Workshop Board</h1>"
    "<div class='info'><span class='label'>Hostname:</span> <span class='value'>%s.local</span></div>"
    "<div class='info'><span class='label'>IP Address:</span> <span class='value'>%s</span></div>"
    "<div class='info'><span class='label'>WiFi Network:</span> <span class='value'>%s</span></div>"
    "<div class='info'><span class='label'>Signal Strength:</span> <span class='value'>%d%% (%d dBm)</span></div>"
    "<div class='info'><span class='label'>Free Heap:</span> <span class='value'>%lu KB</span></div>"
    "<a href='/ota'>Upload New Firmware (OTA)</a>"
    "</body></html>",
    device_hostname_derived_from_mac_address,
    device_ip_address_as_string,
    current_wifi_ssid_connected_to,
    wifi_signal_strength_as_percentage_0_to_100,
    current_wifi_signal_strength_in_dbm,
    esp_get_free_heap_size() / 1024
  );
  
  httpd_resp_set_type(http_request_structure, "text/html");
  return httpd_resp_send(http_request_structure, html_response_buffer_for_device_information, strlen(html_response_buffer_for_device_information));
}

static esp_err_t http_get_handler_for_ota_upload_page(httpd_req_t *http_request_structure) {
  const char *html_form_for_ota_firmware_upload = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Firmware Update</title>"
    "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0}h1{color:#333}.container{background:white;padding:20px;border-radius:5px;box-shadow:0 2px 4px rgba(0,0,0,0.1);max-width:500px}input[type=file]{margin:20px 0;padding:10px}input[type=submit]{background:#28a745;color:white;padding:10px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px}input[type=submit]:hover{background:#218838}.warning{color:#856404;background:#fff3cd;padding:10px;border-radius:5px;margin:10px 0}</style>"
    "</head><body>"
    "<h1>OTA Firmware Update</h1>"
    "<div class='container'>"
    "<p>Upload new firmware (.bin file) to update this device.</p>"
    "<div class='warning'><strong>Warning:</strong> Device will reboot after upload. Wait 60 seconds before reconnecting.</div>"
    "<form method='POST' action='/ota' enctype='multipart/form-data'>"
    "<input type='file' name='firmware' accept='.bin' required>"
    "<input type='submit' value='Upload Firmware'>"
    "</form>"
    "</div>"
    "</body></html>";
  
  httpd_resp_set_type(http_request_structure, "text/html");
  return httpd_resp_send(http_request_structure, html_form_for_ota_firmware_upload, strlen(html_form_for_ota_firmware_upload));
}

static esp_err_t http_post_handler_for_ota_firmware_upload(httpd_req_t *http_request_structure) {
  esp_ota_handle_t ota_update_handle_for_flash_writing;
  const esp_partition_t *next_ota_partition_to_write_firmware_to = esp_ota_get_next_update_partition(NULL);
  
  if (next_ota_partition_to_write_firmware_to == NULL) {
    httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition available");
    return ESP_FAIL;
  }
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Starting OTA update to partition: %s", next_ota_partition_to_write_firmware_to->label);
  
  esp_err_t ota_begin_result = esp_ota_begin(next_ota_partition_to_write_firmware_to, OTA_SIZE_UNKNOWN, &ota_update_handle_for_flash_writing);
  if (ota_begin_result != ESP_OK) {
    httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
    return ESP_FAIL;
  }
  
  char receive_buffer_for_firmware_chunks[1024];
  int bytes_received_in_current_chunk;
  int total_bytes_received_so_far = 0;
  
  while ((bytes_received_in_current_chunk = httpd_req_recv(http_request_structure, receive_buffer_for_firmware_chunks, sizeof(receive_buffer_for_firmware_chunks))) > 0) {
    esp_err_t write_result = esp_ota_write(ota_update_handle_for_flash_writing, receive_buffer_for_firmware_chunks, bytes_received_in_current_chunk);
    if (write_result != ESP_OK) {
      esp_ota_abort(ota_update_handle_for_flash_writing);
      httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
      return ESP_FAIL;
    }
    total_bytes_received_so_far += bytes_received_in_current_chunk;
    
    if (total_bytes_received_so_far % 10240 == 0) {
      ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "OTA progress: %d bytes written", total_bytes_received_so_far);
    }
  }
  
  if (bytes_received_in_current_chunk < 0) {
    esp_ota_abort(ota_update_handle_for_flash_writing);
    httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
    return ESP_FAIL;
  }
  
  esp_err_t ota_end_result = esp_ota_end(ota_update_handle_for_flash_writing);
  if (ota_end_result != ESP_OK) {
    httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
    return ESP_FAIL;
  }
  
  esp_err_t set_boot_partition_result = esp_ota_set_boot_partition(next_ota_partition_to_write_firmware_to);
  if (set_boot_partition_result != ESP_OK) {
    httpd_resp_send_err(http_request_structure, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
    return ESP_FAIL;
  }
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "OTA update successful! Total: %d bytes. Rebooting in 3 seconds...", total_bytes_received_so_far);
  
  const char *success_html_response = 
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Update Success</title>"
    "<style>body{font-family:Arial,sans-serif;margin:20px;text-align:center;background:#f0f0f0}.success{background:#d4edda;color:#155724;padding:20px;border-radius:5px;margin:20px auto;max-width:500px}</style>"
    "</head><body><div class='success'><h1>Firmware Updated!</h1><p>Device will reboot in 3 seconds...</p><p>Wait 30 seconds, then reconnect.</p></div></body></html>";
  
  httpd_resp_send(http_request_structure, success_html_response, strlen(success_html_response));
  
  vTaskDelay(pdMS_TO_TICKS(3000));
  esp_restart();
  
  return ESP_OK;
}

static void start_http_server_for_ota_and_device_information(void) {
  httpd_config_t http_server_configuration_structure = HTTPD_DEFAULT_CONFIG();
  http_server_configuration_structure.max_uri_handlers = 8;
  http_server_configuration_structure.max_resp_headers = 8;
  
  if (httpd_start(&http_server_handle_for_ota_and_info, &http_server_configuration_structure) == ESP_OK) {
    httpd_uri_t root_page_uri_handler_configuration = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = http_get_handler_for_root_page_showing_device_info,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server_handle_for_ota_and_info, &root_page_uri_handler_configuration);
    
    httpd_uri_t ota_get_page_uri_handler_configuration = {
      .uri = "/ota",
      .method = HTTP_GET,
      .handler = http_get_handler_for_ota_upload_page,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server_handle_for_ota_and_info, &ota_get_page_uri_handler_configuration);
    
    httpd_uri_t ota_post_upload_uri_handler_configuration = {
      .uri = "/ota",
      .method = HTTP_POST,
      .handler = http_post_handler_for_ota_firmware_upload,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(http_server_handle_for_ota_and_info, &ota_post_upload_uri_handler_configuration);
    
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "HTTP server started on port 80");
  }
}

static void scrolling_display_task_showing_hostname_ip_and_wifi_status(void *task_parameters_unused) {
  int scroll_position_offset_in_pixels = 0;
  int current_screen_number_for_rotation = 0;
  char text_buffer_for_display_formatting[64];
  int pause_frames_remaining_at_end_of_scroll = 0;
  
  while (1) {
    update_wifi_signal_strength_measurement();
    oled_clear_entire_display_buffer_to_black();
    
    if (current_screen_number_for_rotation == 0) {
      oled_draw_text_using_small_6x8_font_at_position(0, 0, "HOSTNAME:");
      
      int hostname_text_width_in_pixels = strlen(device_hostname_derived_from_mac_address) * 6;
      int display_x_position_for_scrolling_text = scroll_position_offset_in_pixels;
      
      if (hostname_text_width_in_pixels > OLED_DISPLAY_WIDTH_IN_PIXELS) {
        if (scroll_position_offset_in_pixels < -hostname_text_width_in_pixels && pause_frames_remaining_at_end_of_scroll == 0) {
          pause_frames_remaining_at_end_of_scroll = 20;
        }
        if (pause_frames_remaining_at_end_of_scroll > 0) {
          pause_frames_remaining_at_end_of_scroll--;
          display_x_position_for_scrolling_text = -hostname_text_width_in_pixels;
        } else {
          scroll_position_offset_in_pixels -= 2;
        }
      } else {
        display_x_position_for_scrolling_text = (OLED_DISPLAY_WIDTH_IN_PIXELS - hostname_text_width_in_pixels) / 2;
      }
      
      oled_draw_text_using_small_6x8_font_at_position(display_x_position_for_scrolling_text, 16, device_hostname_derived_from_mac_address);
      oled_draw_text_using_small_6x8_font_at_position(0, 32, ".local");
      
    } else if (current_screen_number_for_rotation == 1) {
      oled_draw_text_using_small_6x8_font_at_position(0, 0, "IP ADDRESS:");
      
      int ip_text_width_in_pixels = strlen(device_ip_address_as_string) * 6;
      int display_x_position_for_centered_ip = (OLED_DISPLAY_WIDTH_IN_PIXELS - ip_text_width_in_pixels) / 2;
      if (display_x_position_for_centered_ip < 0) { display_x_position_for_centered_ip = 0; }
      
      oled_draw_text_using_small_6x8_font_at_position(display_x_position_for_centered_ip, 16, device_ip_address_as_string);
      
    } else if (current_screen_number_for_rotation == 2) {
      snprintf(text_buffer_for_display_formatting, sizeof(text_buffer_for_display_formatting), "WIFI: %s", current_wifi_ssid_connected_to);
      oled_draw_text_using_small_6x8_font_at_position(0, 0, text_buffer_for_display_formatting);
      
      snprintf(text_buffer_for_display_formatting, sizeof(text_buffer_for_display_formatting), "RSSI:%d%%", wifi_signal_strength_as_percentage_0_to_100);
      oled_draw_text_using_small_6x8_font_at_position(0, 16, text_buffer_for_display_formatting);
      
      oled_draw_horizontal_progress_bar_with_percentage(0, 32, OLED_DISPLAY_WIDTH_IN_PIXELS, 6, wifi_signal_strength_as_percentage_0_to_100);
    }
    
    oled_send_buffer_to_physical_display_hardware();
    
    vTaskDelay(pdMS_TO_TICKS(150));
    
    static int frame_counter_for_screen_rotation = 0;
    frame_counter_for_screen_rotation++;
    if (frame_counter_for_screen_rotation >= 40) {
      frame_counter_for_screen_rotation = 0;
      current_screen_number_for_rotation = (current_screen_number_for_rotation + 1) % 3;
      scroll_position_offset_in_pixels = OLED_DISPLAY_WIDTH_IN_PIXELS;
      pause_frames_remaining_at_end_of_scroll = 0;
    }
  }
}

static void led_blink_task_indicating_wifi_connection_status(void *task_parameters_unused) {
  while (1) {
    EventBits_t wifi_status_bits = xEventGroupGetBits(wifi_connection_status_event_group_handle);
    
    if (wifi_status_bits & WIFI_CONNECTION_ESTABLISHED_BIT) {
      set_led_state_on_or_off(true);
      vTaskDelay(pdMS_TO_TICKS(900));
      set_led_state_on_or_off(false);
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      set_led_state_on_or_off(true);
      vTaskDelay(pdMS_TO_TICKS(100));
      set_led_state_on_or_off(false);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void app_main(void) {
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "========================================");
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "ESP32-C3 Workshop Firmware Starting");
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "========================================");
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Initializing NVS flash storage...");
  esp_err_t nvs_initialization_result = nvs_flash_init();
  if (nvs_initialization_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_initialization_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG_FOR_MAIN_APPLICATION_LOGGING, "NVS partition needs erasing, erasing now...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvs_initialization_result = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvs_initialization_result);
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "NVS initialized successfully");
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Configuring LED GPIO...");
  configure_led_gpio_pin_as_output_for_status_indication();
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Initializing OLED display...");
  oled_init_hardware_i2c_for_esp32c3_with_ssd1306_on_gpio5_and_gpio6();
  oled_clear_entire_display_buffer_to_black();
  oled_draw_text_using_small_6x8_font_at_position(20, 16, "BOOT");
  oled_send_buffer_to_physical_display_hardware();
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "OLED display initialized - showing BOOT screen");
  
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Starting WiFi initialization and network scan...");
  initialize_wifi_with_multi_network_fallback_support();
  ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi initialization complete");
  
  EventBits_t wifi_status_bits = xEventGroupGetBits(wifi_connection_status_event_group_handle);
  if (wifi_status_bits & WIFI_CONNECTION_ESTABLISHED_BIT) {
    initialize_mdns_service_with_unique_hostname_from_mac();
    start_http_server_for_ota_and_device_information();
    
    uint8_t mac_address_bytes_array[6];
    esp_read_mac(mac_address_bytes_array, ESP_MAC_WIFI_STA);
    
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "========================================");
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "ESP32-C3 Workshop Board Ready");
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "========================================");
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", mac_address_bytes_array[0], mac_address_bytes_array[1], mac_address_bytes_array[2], mac_address_bytes_array[3], mac_address_bytes_array[4], mac_address_bytes_array[5]);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Hostname: %s.local", device_hostname_derived_from_mac_address);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "IP Address: %s", device_ip_address_as_string);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi Network: %s", current_wifi_ssid_connected_to);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "Web Interface: http://%s/", device_ip_address_as_string);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "OTA Upload: http://%s/ota", device_ip_address_as_string);
    ESP_LOGI(TAG_FOR_MAIN_APPLICATION_LOGGING, "========================================");
    
    xTaskCreate(scrolling_display_task_showing_hostname_ip_and_wifi_status, "display", 4096, NULL, 5, NULL);
    xTaskCreate(led_blink_task_indicating_wifi_connection_status, "led", 2048, NULL, 5, NULL);
  } else {
    ESP_LOGE(TAG_FOR_MAIN_APPLICATION_LOGGING, "WiFi connection failed - device not accessible via network");
    oled_clear_entire_display_buffer_to_black();
    oled_draw_text_using_small_6x8_font_at_position(10, 16, "NO WIFI");
    oled_send_buffer_to_physical_display_hardware();
    
    while (1) {
      set_led_state_on_or_off(true);
      vTaskDelay(pdMS_TO_TICKS(200));
      set_led_state_on_or_off(false);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}
