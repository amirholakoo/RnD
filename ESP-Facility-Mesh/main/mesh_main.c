/*
TODO:

3 fix device disconnect issue ( make device MAC invalid incase of disconnection )

2 convert function_table_set to be a FreeRTOS task and use RTOS Queue

5 design payload for task_cmd
6 write handle_task_cmd_message
7 write cmd_msg_sub_tasks
(add queue for task_cmd)

4 write tx_task with queue management ( either static array or FreeRTOS queue ) 

✓ 1 fix rx_task to use mesh internal queue ( not static array ) - COMPLETED
    - automatically uses internal queue. added a flag to check if there are any pending messages.

*/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "mesh_light.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "defines.h"


#define RX_SIZE  (1500)
#define TX_SIZE  (1460)


// Define the 5 GPIO pins for function selection
#define FUNC_PIN_0  GPIO_NUM_35
#define FUNC_PIN_1  GPIO_NUM_36
#define FUNC_PIN_2  GPIO_NUM_37
#define FUNC_PIN_3  GPIO_NUM_38
#define FUNC_PIN_4  GPIO_NUM_39


// versioning & multiplexing
typedef enum {
    MSG_NO_TYPE = 0,
    MSG_TYPE_FUNC_REPORT = 1,   // child → root, “here’s my function ID”
    MSG_TYPE_TASK_CMD    = 2,   // root  → child, “do this task”
    
    
    MSG_TYPE_MAX

} msg_type_t;

// every packet starts with this:
typedef struct __attribute__((packed)) {
    uint8_t  version;    // protocol version, start at 1
    uint8_t  type;       // one of msg_type_t
    uint16_t len;        // length of payload (big‑endian or little‑endian)
} msg_hdr_t;

/*============================================*/

// Paylaods


// payload for MSG_TYPE_FUNC_REPORT:
typedef struct __attribute__((packed)) {
    uint8_t          func_id;   // one of device_function_t
    // you could add a checksum or timestamp here if desired
} func_report_t;

// payload for func_cmd_t


// function
typedef struct {
    mesh_addr_t mac;     // 6‑byte MAC from esp_mesh
    bool valid;   // has this slot been filled yet?
} function_entry_t;

// the root’s master table
static function_entry_t function_table[NUM_FUNCTIONS];


/*============================================*/

static const char *MAC_TABLE_TAG = "mac_table_init";
static const char *MESH_TAG = "mesh_main";
static const char *MESH_RX_TAG = "mesh_rx_main";

static const char *FUNC_INIT_TAG = "func_init_main";
static const uint8_t MESH_ID[6] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
static uint8_t tx_buf[TX_SIZE] = { 0, };
static uint8_t rx_buf[RX_SIZE] = { 0, };
static bool is_running = true;
static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static esp_netif_t *netif_sta = NULL;

mesh_light_ctl_t light_on = {
    .cmd = MESH_CONTROL_CMD,
    .on = 1,
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
};

mesh_light_ctl_t light_off = {
    .cmd = MESH_CONTROL_CMD,
    .on = 0,
    .token_id = MESH_TOKEN_ID,
    .token_value = MESH_TOKEN_VALUE,
};

functions FUNCTION = NO_ROLE;



/*===================================================*/

// Function declarations
void send_function_report_task(void *arg);
void display_function_table_task(void *arg);

// Test
void east_wb_task(){

    while(true)
        vTaskDelay(pdMS_TO_TICKS(100));
    return;
}

void west_wb_task(){

    while(true)
        vTaskDelay(pdMS_TO_TICKS(100));
    return;
}

void east_gate_task(){

    while(true)
        vTaskDelay(pdMS_TO_TICKS(100));
    return;
}

void west_gate_task(){

    while(true)
        vTaskDelay(pdMS_TO_TICKS(100));
    return;
}

// Test
esp_err_t start_task_based_on_role(){

    switch(FUNCTION){
        case NO_ROLE:
        {
            ESP_LOGE(FUNC_INIT_TAG, "No role was detected!");
            return ESP_FAIL;
        }
        case EAST_WB:
        {
            ESP_LOGI(FUNC_INIT_TAG, " FUNCTION SET: EAST_WB");
            xTaskCreate(east_wb_task, "EAST_WB_TASK", 4096, NULL, 5, NULL);
            return ESP_OK;
        }
        case WEST_WB:
        {
            ESP_LOGI(FUNC_INIT_TAG, " FUNCTION SET: WEST_WB");
            xTaskCreate(west_wb_task, "WEST_WB_TASK", 4096, NULL, 5, NULL);
            return ESP_OK;
        }
        case EAST_GATE:
        {
            ESP_LOGI(FUNC_INIT_TAG, " FUNCTION SET: EAST_GATE");
            xTaskCreate(east_gate_task, "EAST_GATE_TASK", 4096, NULL, 5, NULL);
            return ESP_OK;
        }
        case WEST_GATE:
        {
            ESP_LOGI(FUNC_INIT_TAG, " FUNCTION SET: WEST_GATE");
            xTaskCreate(west_gate_task, "WEST_GATE_TASK", 4096, NULL, 5, NULL);
            return ESP_OK;
        }

        default:
        {
            ESP_LOGE(FUNC_INIT_TAG, "NO FUNCTION IS RECOGNIZED!"); 
            return ESP_FAIL;
        }
            
    }
}



//fixed
void function_table_init() {
    for (int i = 0; i < NUM_FUNCTIONS; i++) {
        function_table[i].valid = false;
        memset(&function_table[i].mac, 0, MAC_ADDR_LEN);
    }
}

//fixed
esp_err_t function_table_set(functions func, const uint8_t MAC[MAC_ADDR_LEN]) {

    ESP_LOGI(MAC_TABLE_TAG, "Setting function table for function %d:%s with MAC " MACSTR, func, function_names[func],  MAC2STR(MAC));
    // Fail-safe
    if (func >= NUM_FUNCTIONS) return ESP_FAIL;

    memcpy(function_table[func].mac.addr, MAC, MAC_ADDR_LEN);

    // Verify the copy
    if (memcmp(function_table[func].mac.addr, MAC, MAC_ADDR_LEN) != 0) {
        ESP_LOGE(MAC_TABLE_TAG,
                 "MAC write/read mismatch for function %d! Stored: " MACSTR,
                 func,
                 MAC2STR(function_table[func].mac.addr));
        return ESP_ERR_INVALID_RESPONSE;
    }
    else{
        function_table[func].valid = true;
    }

    ESP_LOGI(MAC_TABLE_TAG, "Assigned MAC " MACSTR " to function ID %d", MAC2STR(MAC), func);
    return ESP_OK;
}



//fixed
void send_test()
{
    while (true)
    {
        ESP_LOGI(MAC_TABLE_TAG, "Sending test message to all valid function_table entries");

        const char *test_msg = "Hello from root!";
        mesh_data_t mesh_data = {
            .data = (uint8_t *)test_msg,
            .size = strlen(test_msg) + 1, // include null terminator
            .proto = MESH_PROTO_BIN,
            .tos = MESH_TOS_DEF,
        };

        bool any_sent = false;
        for (int i = 0; i < NUM_FUNCTIONS; ++i)
        {
            if (function_table[i].valid)
            {
                mesh_addr_t dest_addr;
                (void)memcpy(dest_addr.addr, function_table[i].mac.addr, MAC_ADDR_LEN);

                esp_err_t err = esp_mesh_send(&dest_addr, &mesh_data, MESH_DATA_P2P, NULL, 0);
                if (err == ESP_OK)
                {
                    ESP_LOGI(MAC_TABLE_TAG, "Sent test message to function %d (%s) [" MACSTR "]",
                             i, function_names[i], MAC2STR(dest_addr.addr));
                    any_sent = true;
                }
                else
                {
                    ESP_LOGE(MAC_TABLE_TAG, "Failed to send test message to function %d (%s): %s",
                             i, function_names[i], esp_err_to_name(err));
                }
            }
        }

        if (!any_sent)
        {
            ESP_LOGW(MAC_TABLE_TAG, "No valid MAC addresses set in function_table");
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}


esp_err_t send_root_self_function(){

    uint32_t delay_ms = esp_random() % 7500;    // 0–5 seconds
    vTaskDelay(pdMS_TO_TICKS(delay_ms));


    // 1) build a single TX buffer
    uint8_t buf[sizeof(msg_hdr_t) + sizeof(func_report_t)];
    msg_hdr_t     *hdr  = (msg_hdr_t *)buf;
    func_report_t *body = (func_report_t *)(buf + sizeof(msg_hdr_t));

    // 2) fill the header
    hdr->version = 1;
    hdr->type    = MSG_TYPE_FUNC_REPORT;
    hdr->len     = sizeof(func_report_t);

    // 3) fill the payload
    body->func_id = FUNCTION;

    // 4) wrap in mesh_data_t
    mesh_data_t mesh_data = {
        .data  = buf,
        .size  = sizeof(buf),
        .proto = MESH_PROTO_BIN,   // binary protocol
        .tos   = MESH_TOS_P2P      // point‑to‑point
    };

    // 5) send to ROOT (NULL addr means "to root")
    esp_err_t err = esp_mesh_send(NULL, &mesh_data, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(MAC_TABLE_TAG, "func report send failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(MAC_TABLE_TAG, "Func report sent: ID=%d", FUNCTION);
    }
    return err;
}

// Handle MSG_TYPE_FUNC_REPORT messages
void handle_func_report_message(const mesh_addr_t *from, const uint8_t *payload, uint16_t len) {
    if (len < sizeof(func_report_t)) {
        ESP_LOGE(MESH_RX_TAG, "Invalid func_report message length: %d", len);
        return;
    }
    
    const func_report_t *report = (func_report_t*)payload;
    
    ESP_LOGI(MESH_RX_TAG, "Received function report from " MACSTR ": function_id=%d (%s)", 
             MAC2STR(from->addr), report->func_id, 
             (report->func_id < NUM_FUNCTIONS) ? function_names[report->func_id] : "UNKNOWN");
    
    // Set the function in the function table
    esp_err_t ret = function_table_set(report->func_id, from->addr);
    if (ret != ESP_OK) {
        ESP_LOGE(MESH_RX_TAG, "Failed to set function table: %s", esp_err_to_name(ret));
    }
}

// Handle MSG_TYPE_TASK_CMD messages  
void handle_task_cmd_message(const mesh_addr_t *from, const uint8_t *payload, uint16_t len) {
    ESP_LOGI(MESH_RX_TAG, "Received task command from " MACSTR " (length: %d)", MAC2STR(from->addr), len);
    
    // TODO: Implement task command handling
    // This function will be expanded later to handle specific task commands
}

// Main mesh message receiving task
void mesh_rx_task(void *arg) {
    mesh_addr_t from;
    mesh_data_t data;
    int flag = 0;
    uint32_t pending_count = 0;
    
    ESP_LOGI(MESH_RX_TAG, "Mesh RX task started");
    
    while (true) {
        // Check for pending messages before attempting to receive
        esp_err_t pending_err = esp_mesh_get_rx_pending(&pending_count);
        if (pending_err != ESP_OK) {
            ESP_LOGE(MESH_RX_TAG, "Failed to get RX pending count: %s", esp_err_to_name(pending_err));
            vTaskDelay(pdMS_TO_TICKS(10)); // Short delay on error
            continue;
        }
        
        // If no pending messages, sleep briefly and check again
        if (pending_count == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Sleep for 50ms when no messages
            continue;
        }
        
        ESP_LOGD(MESH_RX_TAG, "Pending messages: %lu", pending_count);
        
        // Initialize data structure
        data.data = rx_buf;
        data.size = RX_SIZE;
        
        // Receive mesh data with short timeout since we know messages are pending
        esp_err_t err = esp_mesh_recv(&from, &data, pdMS_TO_TICKS(100), &flag, NULL, 0);
        
        if (err != ESP_OK) {
            if (err == ESP_ERR_MESH_TIMEOUT) {
                ESP_LOGD(MESH_RX_TAG, "Mesh receive timeout - continuing");
            } else {
                ESP_LOGE(MESH_RX_TAG, "Mesh receive failed: %s", esp_err_to_name(err));
            }
            continue;
        }
        
        // Validate minimum message size
        if (data.size < sizeof(msg_hdr_t)) {
            ESP_LOGW(MESH_RX_TAG, "Received message too small: %d bytes (expected >= %d)", 
                     data.size, sizeof(msg_hdr_t));
            continue;
        }
        
        // Validate maximum message size
        if (data.size > RX_SIZE) {
            ESP_LOGW(MESH_RX_TAG, "Received message too large: %d bytes (max %d)", 
                     data.size, RX_SIZE);
            continue;
        }
        
        // Parse message header
        const msg_hdr_t *hdr = (msg_hdr_t*)data.data;
        const uint8_t *payload = data.data + sizeof(msg_hdr_t);
        uint16_t payload_len = data.size - sizeof(msg_hdr_t);
        
        ESP_LOGI(MESH_RX_TAG, "Received message: version=%d, type=%d, len=%d from " MACSTR,
                 hdr->version, hdr->type, hdr->len, MAC2STR(from.addr));
        
        // Validate protocol version
        if (hdr->version != 1) {
            ESP_LOGW(MESH_RX_TAG, "Unsupported message version: %d (expected 1)", hdr->version);
            continue;
        }
        
        // Validate payload length consistency
        if (hdr->len != payload_len) {
            ESP_LOGW(MESH_RX_TAG, "Message length mismatch: header=%d, actual=%d", 
                     hdr->len, payload_len);
            continue;
        }
        
        // Validate message type is within expected range
        if (hdr->type == 0 || hdr->type >= MSG_TYPE_MAX) {
            ESP_LOGW(MESH_RX_TAG, "Invalid message type: %d", hdr->type);
            continue;
        }
        
        // Handle message based on type
        switch (hdr->type) {
            case MSG_TYPE_FUNC_REPORT:
                handle_func_report_message(&from, payload, payload_len);
                break;
                
            case MSG_TYPE_TASK_CMD:
                handle_task_cmd_message(&from, payload, payload_len);
                break;
                
            default:
                ESP_LOGW(MESH_RX_TAG, "Unknown message type: %d", hdr->type);
                break;
        }
    }
}


void init_function_addr_table(){


    ESP_LOGI(MAC_TABLE_TAG, "Initializing function address table");

    function_table_init();


    xTaskCreate(send_test, "SEND_TEST", 4096, NULL, 5, NULL);

    // Test function to periodically display function table status
    xTaskCreate(display_function_table_task, "DISPLAY_FUNC_TABLE", 4096, NULL, 5, NULL);


    return;
}

// Test function to periodically display function table status
void display_function_table_task(void *arg) {
    ESP_LOGI(MAC_TABLE_TAG, "Function table monitoring task started");
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds between checks
        
        ESP_LOGI(MAC_TABLE_TAG, "=== FUNCTION TABLE STATUS ===");
        bool any_functions_registered = false;
        
        for (int i = 0; i < NUM_FUNCTIONS; i++) {
            if (function_table[i].valid) {
                ESP_LOGI(MAC_TABLE_TAG, "Function %d (%s): MAC " MACSTR, 
                         i, function_names[i], MAC2STR(function_table[i].mac.addr));
                any_functions_registered = true;
            } else {
                ESP_LOGI(MAC_TABLE_TAG, "Function %d (%s): NOT REGISTERED", 
                         i, function_names[i]);
            }
        }
        
        if (!any_functions_registered) {
            ESP_LOGW(MAC_TABLE_TAG, "No functions registered yet!");
        }
        
        ESP_LOGI(MAC_TABLE_TAG, "=== END FUNCTION TABLE ===");
    }
}


// Task function for child nodes to send function report to root
void send_function_report_task(void *arg) {
    ESP_LOGI(MESH_TAG, "Function report task started for child node");
    
    // Wait for mesh to be fully connected before sending
    while (!is_mesh_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Send function report to root
    for(int i = 0; i < 5; i++){
        esp_err_t err = send_root_self_function();
        if (err == ESP_OK) {
            ESP_LOGI(MESH_TAG, "Function report sent successfully to root");
            break;
        } else {
            ESP_LOGE(MESH_TAG, "Failed to send function report to root: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Delete the task after completion
    vTaskDelete(NULL);
}


void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {0,};
    static uint16_t last_layer = 0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        esp_mesh_get_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:"MACSTR"%s, ID:"MACSTR", duty:%d",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr), connected->duty);
        last_layer = mesh_layer;
        mesh_connected_indicator(mesh_layer);
        is_mesh_connected = true;
        if (esp_mesh_is_root()) {
            esp_netif_dhcpc_stop(netif_sta);
            esp_netif_dhcpc_start(netif_sta);
        }
        //esp_mesh_comm_p2p_start();
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        is_mesh_connected = false;
        mesh_disconnected_indicator();
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
        mesh_connected_indicator(mesh_layer);
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:"MACSTR"",
                 MAC2STR(root_addr->addr));

        
        if(esp_mesh_is_root())
        {
            ESP_LOGI(MESH_TAG, "\r\n\r\n MESH_EVENT_ROOT_ADDRESS IN ROOT NODE  \r\n\r\n");
            init_function_addr_table();

            // Start mesh message receiving task for root node
            xTaskCreate(mesh_rx_task, "mesh_rx_task", 4096, NULL, 5, NULL);
            ESP_LOGI(MESH_TAG, "Mesh RX task created for root node");

        }   
        else
        {
            ESP_LOGI(MESH_TAG, "\r\n\r\n MESH_EVENT_ROOT_ADDRESS IN NON-ROOT NODE  \r\n\r\n");
            
            // Child nodes should also start an RX task to handle commands from root
            xTaskCreate(mesh_rx_task, "mesh_rx_task", 4096, NULL, 5, NULL);
            ESP_LOGI(MESH_TAG, "Mesh RX task created for child node");
            
            // Send function report to root after a random delay
            // Use a task to avoid blocking the event handler
            xTaskCreate(send_function_report_task, "send_func_report", 4096, NULL, 5, NULL);
            ESP_LOGI(MESH_TAG, "Function report task created for child node");
        }
            
        

    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);

    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;

    default:
        ESP_LOGI(MESH_TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}


void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));

}

//fixed
functions role_read_pins(){
    
    
    // GPIO configuration structure
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FUNC_PIN_0) | (1ULL << FUNC_PIN_1) | 
                        (1ULL << FUNC_PIN_2) | (1ULL << FUNC_PIN_3) | 
                        (1ULL << FUNC_PIN_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    // Configure GPIO pins
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(FUNC_INIT_TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return NO_ROLE;
    }
    
    // Small delay to ensure pins stabilize
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Read pin states
    uint8_t pin_state = 0;
    pin_state |= (gpio_get_level(FUNC_PIN_0) << 0);
    pin_state |= (gpio_get_level(FUNC_PIN_1) << 1);
    pin_state |= (gpio_get_level(FUNC_PIN_2) << 2);
    pin_state |= (gpio_get_level(FUNC_PIN_3) << 3);
    pin_state |= (gpio_get_level(FUNC_PIN_4) << 4);
    
    ESP_LOGI(FUNC_INIT_TAG, "Pin reading: 0x%02X (binary: %d%d%d%d%d)", 
             pin_state,
             (pin_state >> 4) & 1, (pin_state >> 3) & 1, (pin_state >> 2) & 1,
             (pin_state >> 1) & 1, (pin_state >> 0) & 1);
    
    // Map pin combinations to functions
    // Since pins have pull-up resistors, unconnected pins read as 1
    // Connected to ground pins read as 0
    functions assigned_function = pin_state;
    
    /*
    switch(pin_state) {
        case 0b11110:  // Pin 0 to ground, others floating (EAST_WB)
            assigned_function = EAST_WB;
            break;
        case 0b11101:  // Pin 1 to ground, others floating (WEST_WB)
            assigned_function = WEST_WB;
            break;
        case 0b11011:  // Pin 2 to ground, others floating (EAST_GATE)
            assigned_function = EAST_GATE;
            break;
        case 0b10111:  // Pin 3 to ground, others floating (WEST_GATE)
            assigned_function = WEST_GATE;
            break;
        case 0b11111:  // All pins floating (default - no function assigned)
        default:
            assigned_function = NO_ROLE;
            ESP_LOGW(FUNC_INIT_TAG, "Unknown pin combination: 0x%02X, defaulting to NO_ROLE", pin_state);
            break;
    }
    */
    
    ESP_LOGI(FUNC_INIT_TAG, "\r\n\r\n   Function assigned based on pins: %s \r\n\r\n", 
             (assigned_function < NUM_FUNCTIONS) ? function_names[assigned_function] : "UNKNOWN");
    
    return assigned_function;
}

esp_err_t get_device_mac(uint8_t *mac)
{
    if (mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_wifi_get_mac(WIFI_IF_STA, mac);
}


void app_main(void)
{
    FUNCTION = role_read_pins();


    ESP_ERROR_CHECK(mesh_light_init());
    ESP_ERROR_CHECK(nvs_flash_init());

    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());

    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /*  create network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    
    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(40));

    uint8_t mac_addr[6] = {0};
    if (get_device_mac(mac_addr) == ESP_OK) {
        ESP_LOGI(FUNC_INIT_TAG, "\r\n\r\n   Device MAC Address: " MACSTR "  \r\n\r\n", MAC2STR(mac_addr));
    } else {
        ESP_LOGE(FUNC_INIT_TAG, "Failed to get device MAC address");
    }

    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));

    /*  set mesh topology */
    ESP_ERROR_CHECK(esp_mesh_set_topology(CONFIG_MESH_TOPOLOGY));
    /*  set mesh max layer according to the topology */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(128));

    /* Disable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));

    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);

    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
    memcpy((uint8_t *) &cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, CONFIG_MESH_ROUTER_PASSWD,
           strlen(CONFIG_MESH_ROUTER_PASSWD));

    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *) &cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));

    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());


    ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s<%d>%s, ps:%d",  esp_get_minimum_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed",
             esp_mesh_get_topology(), esp_mesh_get_topology() ? "(chain)":"(tree)", esp_mesh_is_ps_enabled());

    
}
