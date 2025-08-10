esp_err_t send_root_self_function(){

    uint32_t delay_ms = esp_random() % 4000;    // 0–5 seconds
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

    // 5) send to ROOT (NULL addr means “to root”)
    esp_err_t err = esp_mesh_send(NULL, &mesh_data, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(MAC_TABLE_TAG, "func report send failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(MAC_TABLE_TAG, "Func report sent: ID=%d", FUNCTION);
    }
    return err;
}




void receive_function_roles_task(void *pvParameters) {
    mesh_data_t mesh_data = {
        .data = rx_buf,
        .size = RX_SIZE,
        .proto = MESH_PROTO_BIN,
        .tos = MESH_TOS_DEF,
    };
    
    while (is_running) {
        esp_err_t err = esp_mesh_recv(&mesh_data, portMAX_DELAY, &mesh_data, NULL, NULL, NULL);
        if (err == ESP_OK) {
            ESP_LOGI(MAC_TABLE_TAG, "Received message from " MACSTR ", size: %d", 
                     MAC2STR(mesh_data.addr.addr), mesh_data.size);
            
            // Check if message is large enough to contain our header
            if (mesh_data.size >= sizeof(msg_hdr_t)) {
                msg_hdr_t *hdr = (msg_hdr_t *)mesh_data.data;
                
                // Verify protocol version
                if (hdr->version != 1) {
                    ESP_LOGW(MAC_TABLE_TAG, "Unknown protocol version: %d", hdr->version);
                    continue;
                }
                
                // Handle function report messages
                if (hdr->type == MSG_TYPE_FUNC_REPORT) {
                    if (mesh_data.size >= sizeof(msg_hdr_t) + sizeof(func_report_t)) {
                        func_report_t *func_report = (func_report_t *)(mesh_data.data + sizeof(msg_hdr_t));
                        
                        ESP_LOGI(MAC_TABLE_TAG, "Received function report from " MACSTR ": function_id=%d", 
                                 MAC2STR(mesh_data.addr.addr), func_report->func_id);
                        
                        // Validate function ID
                        if (func_report->func_id < NUM_FUNCTIONS) {
                            // Add to function table
                            esp_err_t set_err = function_table_set(func_report->func_id, mesh_data.addr.addr);
                            if (set_err == ESP_OK) {
                                ESP_LOGI(MAC_TABLE_TAG, "Successfully added function %d (" MACSTR ") to function table", 
                                         func_report->func_id, MAC2STR(mesh_data.addr.addr));
                                
                                // Start send_test task after first function is registered
                                static bool test_task_started = false;
                                if (!test_task_started) {
                                    xTaskCreate(send_test, "SEND_TEST", 4096, NULL, 5, NULL);
                                    test_task_started = true;
                                    ESP_LOGI(MAC_TABLE_TAG, "Started send_test task");
                                }
                            } else {
                                ESP_LOGE(MAC_TABLE_TAG, "Failed to add function to table: %s", esp_err_to_name(set_err));
                            }
                        } else {
                            ESP_LOGW(MAC_TABLE_TAG, "Invalid function ID received: %d", func_report->func_id);
                        }
                    } else {
                        ESP_LOGW(MAC_TABLE_TAG, "Function report message too short");
                    }
                } else {
                    ESP_LOGI(MAC_TABLE_TAG, "Received message type: %d (not a function report)", hdr->type);
                }
            } else {
                ESP_LOGW(MAC_TABLE_TAG, "Received message too short to contain header");
            }
        } else {
            ESP_LOGE(MAC_TABLE_TAG, "Mesh receive failed: %s", esp_err_to_name(err));
        }
    }
    
    vTaskDelete(NULL);
}


