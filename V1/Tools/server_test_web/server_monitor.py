#!/usr/bin/env python3
"""
Server Health Monitor Web Application
A simple web server that monitors multiple servers and displays their status.
"""

import http.server
import socketserver
import json
import urllib.request
import urllib.error
import threading
import time
from datetime import datetime


# Server configuration
try:
    from config import PORT
except ImportError:
    PORT = 8080  # Default port if config.py doesn't exist


SERVER_NAMES = [
    "Lab Admin",
    "Lab React App", 
    "Loading/Unloading Frontend Service",
    "DHT22 Thermal Cam data logger Server/Dashboard",
    "TOTAL monitor Server/Dashboard",
    "Thermal Camera Frontend Server",
    "Thermal Camera Temperature"
]

SERVER_URLS = [
    "http://192.168.2.46:8000/admin/",
    "http://192.168.2.46:5173",
    "http://192.168.2.46:18888",
    "http://192.168.2.46:8001/dashboard/",
    "http://192.168.2.20:7500/dashboard/",
    "http://172.16.15.20:8001/view/",
    "http://172.16.15.20:5000/temperature"
]

# Global variable to store server status
server_status = []

def check_server_health():
    """Check the health of all servers and update global status."""
    global server_status
    status_list = []
    
    for i, (name, url) in enumerate(zip(SERVER_NAMES, SERVER_URLS)):
        try:
            # Create request with timeout
            request = urllib.request.Request(url)
            request.add_header('User-Agent', 'ServerMonitor/1.0')
            
            with urllib.request.urlopen(request, timeout=10) as response:
                status_code = response.getcode()
                is_healthy = status_code == 200
                
        except (urllib.error.URLError, urllib.error.HTTPError, Exception) as e:
            status_code = 0
            is_healthy = False
            
        status_list.append({
            'name': name,
            'url': url,
            'status_code': status_code,
            'healthy': is_healthy,
            'last_checked': datetime.now().strftime('%H:%M:%S')
        })
    
    server_status = status_list

def background_checker():
    """Background thread that checks server health every 10 seconds."""
    while True:
        check_server_health()
        time.sleep(10)

class ServerMonitorHandler(http.server.SimpleHTTPRequestHandler):
    """Custom handler for the server monitor web application."""
    
    def do_GET(self):
        """Handle GET requests."""
        if self.path == '/':
            self.serve_main_page()
        elif self.path == '/api/status':
            self.serve_status_api()
        else:
            super().do_GET()
    
    def serve_main_page(self):
        """Serve the main HTML page."""
        html_content = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Server Health Monitor</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            padding: 20px;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
        }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        .server-card {
            border: 1px solid #ddd;
            border-radius: 6px;
            padding: 15px;
            background: #fafafa;
        }
        .server-name {
            font-weight: bold;
            font-size: 16px;
            margin-bottom: 8px;
            color: #333;
        }
        .server-url {
            font-size: 12px;
            color: #666;
            margin-bottom: 10px;
            word-break: break-all;
        }
        .status {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
        }
        .status.healthy {
            background-color: #d4edda;
            color: #155724;
        }
        .status.unhealthy {
            background-color: #f8d7da;
            color: #721c24;
        }
        .last-checked {
            font-size: 11px;
            color: #888;
            margin-top: 5px;
        }
        .summary {
            text-align: center;
            padding: 15px;
            background: #e9ecef;
            border-radius: 6px;
            margin-bottom: 20px;
        }
        .summary h2 {
            margin: 0 0 10px 0;
            color: #495057;
        }
        .auto-refresh {
            text-align: center;
            color: #666;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Server Health Monitor</h1>
        
        <div class="summary">
            <h2 id="summary">Loading...</h2>
        </div>
        
        <div class="status-grid" id="statusGrid">
            <div>Loading server status...</div>
        </div>
        
        <div class="auto-refresh">
            Auto-refreshing every 10 seconds
        </div>
    </div>

    <script>
        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    updateSummary(data);
                    updateServerCards(data);
                })
                .catch(error => {
                    console.error('Error fetching status:', error);
                });
        }

        function updateSummary(data) {
            const healthy = data.filter(server => server.healthy).length;
            const total = data.length;
            const summaryElement = document.getElementById('summary');
            summaryElement.textContent = `${healthy}/${total} servers healthy`;
        }

        function updateServerCards(data) {
            const grid = document.getElementById('statusGrid');
            grid.innerHTML = '';
            
            data.forEach(server => {
                const card = document.createElement('div');
                card.className = 'server-card';
                
                const statusClass = server.healthy ? 'healthy' : 'unhealthy';
                const statusText = server.healthy ? 'HEALTHY' : 'DOWN';
                
                card.innerHTML = `
                    <div class="server-name">${server.name}</div>
                    <div class="server-url">${server.url}</div>
                    <span class="status ${statusClass}">${statusText}</span>
                    <div class="last-checked">Last checked: ${server.last_checked}</div>
                `;
                
                grid.appendChild(card);
            });
        }

        // Initial load
        updateStatus();
        
        // Auto-refresh every 10 seconds
        setInterval(updateStatus, 10000);
    </script>
</body>
</html>
        """
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(html_content.encode())
    
    def serve_status_api(self):
        """Serve the server status as JSON."""
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        
        response = json.dumps(server_status, indent=2)
        self.wfile.write(response.encode())

def main():
    """Main function to start the server monitor."""
    print(f"Starting Server Health Monitor on port {PORT}")
    print(f"Open your browser to: http://localhost:{PORT}")
    print("Press Ctrl+C to stop the server")
    
    checker_thread = threading.Thread(target=background_checker, daemon=True)
    checker_thread.start()
    
    check_server_health()
    
    with socketserver.TCPServer(("", PORT), ServerMonitorHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")
            httpd.shutdown()

if __name__ == "__main__":
    main()