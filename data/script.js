// ==================== WEBSOCKET ====================
var gateway = `${window.location.protocol === 'https:' ? 'wss' : 'ws'}://${window.location.host}/ws`;
var websocket;

window.addEventListener('load', function() {
    initWebSocket();
    initGauges();
    loadCoreIOTConfig();
    initLEDControls(); // ✅ THÊM
});

function onOpen(event) {
    console.log('✅ WebSocket connected');
}

function onClose(event) {
    console.log('❌ WebSocket closed');
    setTimeout(initWebSocket, 2000);
}

function initWebSocket() {
    console.log('🔄 Opening WebSocket...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Sent:", data);
    } else {
        console.warn("⚠️ WebSocket not ready!");
    }
}

function onMessage(event) {
    console.log("📩 Received:", event.data);
    try {
        var data = JSON.parse(event.data);
        if (data.temp !== undefined && window.gaugeTemp) {
            window.gaugeTemp.refresh(data.temp);
        }
        if (data.humi !== undefined && window.gaugeHumi) {
            window.gaugeHumi.refresh(data.humi);
        }
    } catch (e) {
        console.warn("Not JSON:", event.data);
    }
}

// ==================== UI NAVIGATION ====================
let relayList = [];
let deleteTarget = null;

function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    document.getElementById(id).style.display = id === 'settings' ? 'flex' : 'block';
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    event.currentTarget.classList.add('active');
    
    if (id === 'settings') {
        loadCoreIOTConfig();
    }
}

// ==================== HOME GAUGES ====================
function initGauges() {
    window.gaugeTemp = new JustGage({
        id: "gauge_temp",
        value: 26,
        min: -10,
        max: 50,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#00BCD4", "#4CAF50", "#FFC107", "#F44336"]
    });

    window.gaugeHumi = new JustGage({
        id: "gauge_humi",
        value: 60,
        min: 0,
        max: 100,
        donut: true,
        pointer: false,
        gaugeWidthScale: 0.25,
        gaugeColor: "transparent",
        levelColorsGradient: true,
        levelColors: ["#42A5F5", "#00BCD4", "#0288D1"]
    });
}

// ==================== LED BRIGHTNESS CONTROL ====================
function initLEDControls() {
    // LED 1
    const toggle1 = document.getElementById('led1Toggle');
    const slider1 = document.getElementById('led1Brightness');
    const value1 = document.getElementById('led1Value');
    
    if (toggle1 && slider1) {
        toggle1.addEventListener('change', function() {
            const isOn = this.checked;
            slider1.disabled = !isOn;
            
            if (isOn) {
                slider1.parentElement.classList.add('slider-enabled');
                controlLED(1, 'ON', slider1.value);
            } else {
                slider1.parentElement.classList.remove('slider-enabled');
                controlLED(1, 'OFF', 0);
            }
        });
        
        slider1.addEventListener('input', function() {
            value1.textContent = this.value + '%';
            if (toggle1.checked) {
                controlLED(1, 'ON', this.value);
            }
        });
    }
    
    // LED 2
    const toggle2 = document.getElementById('led2Toggle');
    const slider2 = document.getElementById('led2Brightness');
    const value2 = document.getElementById('led2Value');
    
    if (toggle2 && slider2) {
        toggle2.addEventListener('change', function() {
            const isOn = this.checked;
            slider2.disabled = !isOn;
            
            if (isOn) {
                slider2.parentElement.classList.add('slider-enabled');
                controlLED(2, 'ON', slider2.value);
            } else {
                slider2.parentElement.classList.remove('slider-enabled');
                controlLED(2, 'OFF', 0);
            }
        });
        
        slider2.addEventListener('input', function() {
            value2.textContent = this.value + '%';
            if (toggle2.checked) {
                controlLED(2, 'ON', this.value);
            }
        });
    }
}

function controlLED(device, state, brightness) {
    // Via HTTP GET
    fetch(`/control?device=${device}&state=${state}&brightness=${brightness}`)
        .then(response => response.text())
        .then(data => {
            console.log(`✅ LED${device}: ${state} @ ${brightness}%`);
        })
        .catch(err => {
            console.error('❌ Control error:', err);
        });
}

// ==================== RELAY FUNCTIONS (GIỮ NGUYÊN) ====================
function openAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'flex';
}
function closeAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'none';
}
function saveRelay() {
    const name = document.getElementById('relayName').value.trim();
    const gpio = document.getElementById('relayGPIO').value.trim();
    if (!name || !gpio) return alert("⚠️ Fill all fields!");
    relayList.push({ id: Date.now(), name, gpio, state: false });
    renderRelays();
    closeAddRelayDialog();
}
function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = "";
    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';
        card.innerHTML = `
            <i class="fa-solid fa-bolt device-icon"></i>
            <h3>${r.name}</h3>
            <p>GPIO: ${r.gpio}</p>
            <button class="toggle-btn ${r.state ? 'on' : ''}" onclick="toggleRelay(${r.id})">
                ${r.state ? 'ON' : 'OFF'}
            </button>
            <i class="fa-solid fa-trash delete-icon" onclick="showDeleteDialog(${r.id})"></i>
        `;
        container.appendChild(card);
    });
}
function toggleRelay(id) {
    const relay = relayList.find(r => r.id === id);
    if (relay) {
        relay.state = !relay.state;
        Send_Data(JSON.stringify({
            page: "device",
            value: { name: relay.name, status: relay.state ? "ON" : "OFF", gpio: relay.gpio }
        }));
        renderRelays();
    }
}
function showDeleteDialog(id) {
    deleteTarget = id;
    document.getElementById('confirmDeleteDialog').style.display = 'flex';
}
function closeConfirmDelete() {
    document.getElementById('confirmDeleteDialog').style.display = 'none';
}
function confirmDelete() {
    relayList = relayList.filter(r => r.id !== deleteTarget);
    renderRelays();
    closeConfirmDelete();
}

// ==================== COREIOT CONFIG API (GIỮ NGUYÊN) ====================
async function loadCoreIOTConfig() {
    try {
        const response = await fetch('/api/coreiot/config');
        if (!response.ok) {
            console.warn("⚠️ Cannot load config");
            return;
        }
        
        const data = await response.json();
        
        if (data.server) document.getElementById('server').value = data.server;
        if (data.port) document.getElementById('port').value = data.port;
        if (data.client_id) document.getElementById('client_id').value = data.client_id;
        if (data.username) document.getElementById('mqtt_username').value = data.username;
        
        if (data.password_set) {
            document.getElementById('mqtt_password').placeholder = "Password đã lưu (để trống = giữ nguyên)";
        }
        
        console.log("✅ Config loaded");
    } catch (error) {
        console.error("❌ Load error:", error);
    }
}

document.getElementById("settingsForm").addEventListener("submit", async function (e) {
    e.preventDefault();

    const ssid = document.getElementById("ssid").value.trim();
    const password = document.getElementById("password").value.trim();
    const server = document.getElementById("server").value.trim();
    const portValue = document.getElementById("port").value.trim();
    const client_id = document.getElementById("client_id").value.trim();
    const mqtt_username = document.getElementById("mqtt_username").value.trim();
    const mqtt_password = document.getElementById("mqtt_password").value.trim();

    const port = parseInt(portValue);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
        alert("⚠️ Port không hợp lệ! (1-65535)");
        return;
    }

    if (!server || !client_id || !mqtt_username) {
        alert("⚠️ Vui lòng điền đầy đủ: Server, Client ID, Username!");
        return;
    }

    const config = {
        ssid: ssid,
        password: password,
        server: server,
        port: port,
        client_id: client_id,
        username: mqtt_username,
        password: mqtt_password || "***"
    };

    try {
        const response = await fetch('/api/coreiot/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });

        const result = await response.json();
        
        if (result.success) {
            alert("✅ Đã lưu! MQTT đang kết nối...");
            setTimeout(loadCoreIOTConfig, 500);
        } else {
            alert("❌ Lỗi: " + result.message);
        }
    } catch (error) {
        console.error("❌ Request error:", error);
        alert("❌ Không thể kết nối ESP32!");
    }
});
