/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_err.h"
#include "esp_mesh.h"
#include "mesh_light.h"
#include "led_strip.h"

/*******************************************************
 *                Constants
 *******************************************************/
#define LED_PIN 48
#define NUM_LEDS 1

/*******************************************************
 *                Variable Definitions
 *******************************************************/
static bool s_light_inited = false;
static led_strip_handle_t led_strip;

/*******************************************************
 *                Function Definitions
 *******************************************************/
esp_err_t mesh_light_init(void)
{
    if (s_light_inited == true) {
        return ESP_OK;
    }
    s_light_inited = true;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = NUM_LEDS,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz resolution
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // Set initial color to Cyan (MESH_LIGHT_INIT)
    led_strip_set_pixel(led_strip, 0, 0, 255, 255);
    led_strip_refresh(led_strip);

    return ESP_OK;
}

esp_err_t mesh_light_set(int color)
{
    uint8_t r, g, b;

    switch (color) {
    case MESH_LIGHT_RED:
        r = 255; g = 0; b = 0;    // Red
        break;
    case MESH_LIGHT_GREEN:
        r = 0; g = 255; b = 0;    // Green
        break;
    case MESH_LIGHT_BLUE:
        r = 0; g = 0; b = 255;    // Blue
        break;
    case MESH_LIGHT_YELLOW:
        r = 255; g = 255; b = 0;  // Yellow
        break;
    case MESH_LIGHT_PINK:
        r = 255; g = 0; b = 255;  // Pink (using magenta)
        break;
    case MESH_LIGHT_INIT:
        r = 0; g = 255; b = 255;  // Cyan
        break;
    case MESH_LIGHT_WARNING:
        r = 255; g = 255; b = 255; // White
        break;
    default:
        r = 0; g = 0; b = 0;      // Off
        break;
    }

    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);

    return ESP_OK;
}

void mesh_connected_indicator(int layer)
{
    switch (layer) {
    case 1:
        mesh_light_set(MESH_LIGHT_PINK);
        break;
    case 2:
        mesh_light_set(MESH_LIGHT_YELLOW);
        break;
    case 3:
        mesh_light_set(MESH_LIGHT_RED);
        break;
    case 4:
        mesh_light_set(MESH_LIGHT_BLUE);
        break;
    case 5:
        mesh_light_set(MESH_LIGHT_GREEN);
        break;
    case 6:
        mesh_light_set(MESH_LIGHT_WARNING);
        break;
    default:
        mesh_light_set(0);
    }
}

void mesh_disconnected_indicator(void)
{
    mesh_light_set(MESH_LIGHT_WARNING);
}

esp_err_t mesh_light_process(mesh_addr_t *from, uint8_t *buf, uint16_t len)
{
    mesh_light_ctl_t *in = (mesh_light_ctl_t *) buf;
    if (!from || !buf || len < sizeof(mesh_light_ctl_t)) {
        return ESP_FAIL;
    }
    if (in->token_id != MESH_TOKEN_ID || in->token_value != MESH_TOKEN_VALUE) {
        return ESP_FAIL;
    }
    if (in->cmd == MESH_CONTROL_CMD) {
        if (in->on) {
            mesh_connected_indicator(esp_mesh_get_layer());
        } else {
            mesh_light_set(0);
        }
    }
    return ESP_OK;
}